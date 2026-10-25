# Counter-Strike 2 (Source 2) - Weapon Inaccuracy, Spread Cone & Recoil Punch Decay Pipeline

> **Binary Targets**: `client.dll` & `server.dll` (Base Address: `0x180000000`)  
> **Key Routines**: `C_CSWeaponBase::GetInaccuracy` (`0x18080c1a0`), `C_CSWeaponBase::UpdateAccuracyPenalty` (`0x180827be0`), `GetRecoveryTime` (`0x18080cef0`), `CalculateStrafingInaccuracy` (`0x18080d4c0`), `UpdateTurningInaccuracy` (`0x180828d90`), `CPlayer_AimPunchServices::SimulateAimPunchDecay` (`0x18085dc60`)  
> **Reconstructed Source**: [`weapon_accuracy.h`](../game/client/weapon_accuracy.h), [`weapon_accuracy.cpp`](../game/client/weapon_accuracy.cpp)  
> **Signature Verification**: 100% Unique Verified Matches across version archives (`0x00827be0` & `0x0085dc60` baseline, `+0xe30` patch shift to `0x00828a10` & `0x0085ea90` on build 26144732)

---

## 1. Architectural Overview: Weapon Inaccuracy & Recoil Pipeline

In Counter-Strike 2, shooting accuracy is determined by a layered composite model composed of **intrinsic weapon spread** (`m_flSpread`), **posture and movement inaccuracy** (`m_fAccuracyPenalty`), **strafing inaccuracy** (`sv_strafing_inaccuracy`), **view turning inaccuracy** (`sv_turning_inaccuracy`), and **continuous rapid firing recoil index** (`m_flRecoilIndex`). Simultaneously, camera kick and crosshair displacement are governed by the **aim punch spring-damper ODE system** inside `CPlayer_AimPunchServices`.

```mermaid
flowchart TD
    subgraph AccuracyPipeline ["CS2 Weapon Accuracy & Inaccuracy Pipeline (client.dll)"]
        POSTURE["Player Physical Posture (Stand / Crouch / Ladder / Air)"]
        VEL["Player Physical Velocity (2D Speed & Vertical Vz)"]
        STRAFE["Strafing Inaccuracy: Dot(EyeDir, VelDir)"]
        TURN["Turning Inaccuracy: Delta Angle Decay"]
        RECOIL_IDX["Rapid-Fire Recoil Index Decay: exp(-2*ln10*dt)"]
        PENALTY["m_fAccuracyPenalty Decay: exp(-(ln10 / recoveryTime) * dt)"]
        COMPOSITE["C_CSWeaponBase::GetInaccuracy (0x18080c1a0)"]
        CONE["Perturbed Spread Cone Ray Projection"]
    end

    subgraph RecoilPunchPipeline ["Recoil Aim Punch Dynamics (CPlayer_AimPunchServices)"]
        FIRE_IMP["OnWeaponFired Kick Impulse"]
        SPRING["Second-Order Damped Spring Integration"]
        VEL_DAMP["Velocity Damping: exp(-4.5 * dt)"]
        DEADZONE["Deadzone Threshold Filter: 0.140625 (0.375^2)"]
        CLAMP["View Pitch Clamping [-89 deg, +89 deg]"]
    end

    POSTURE --> PENALTY
    VEL --> COMPOSITE
    STRAFE --> COMPOSITE
    TURN --> COMPOSITE
    PENALTY --> COMPOSITE
    RECOIL_IDX --> PENALTY
    COMPOSITE --> CONE

    FIRE_IMP --> SPRING
    SPRING --> VEL_DAMP
    VEL_DAMP --> DEADZONE
    DEADZONE --> CLAMP
```

---

## 2. authoritatively Reversed Routines

### 2.1 Weapon Inaccuracy Evaluator (`C_CSWeaponBase::GetInaccuracy` @ `0x18080c1a0`)

`GetInaccuracy` synthesizes all active inaccuracy sources into a composite scalar in the range $[0.0, 1.0]$:

```cpp
float C_CSWeaponBase::GetInaccuracy(float* outSpread, float* outJumpInacc);
```

1. **ConVar Overrides**:
   - If `weapon_accuracy_forcespread > 0.0f`, overrides and returns the forced spread value directly.
   - If `weapon_accuracy_nospread != 0`, returns `0.0f` (perfect laser precision).
2. **Base Penalty**:
   - Starts with `m_fAccuracyPenalty` (`+0x1a18`).
3. **Movement Speed Modulation**:
   - Computes planar horizontal speed $v_{\text{2D}} = \sqrt{v_x^2 + v_y^2}$.
   - Remaps speed between a deadzone threshold of $34\%$ max weapon speed ($0.34 \cdot v_{\max}$) and full running penalty at $95\%$ max speed ($0.95 \cdot v_{\max}$):
     $$\text{speedFrac} = \text{clamp}\left(\frac{v_{\text{2D}} - 0.34 \cdot v_{\max}}{0.95 \cdot v_{\max} - 0.34 \cdot v_{\max}}, 0.0, 1.0\right)$$
   - **Crouch Power Curve**: If the pawn is ducking (`pPawn->m_flags & FL_DUCKING`), the movement factor is exponentiated by $0.25$:
     $$\text{speedFrac}_{\text{crouch}} = (\text{speedFrac})^{0.25}$$
   - Adds $\text{speedFrac} \cdot \text{m\_flInaccuracyMove}$ to total inaccuracy.
4. **Airborne / Jumping Inaccuracy**:
   - If the player pawn is not on the ground (`!(pPawn->m_flags & FL_ONGROUND)`):
     - Examines absolute vertical velocity $|v_z|$.
     - If near jump apex ($|v_z| \le 200 \text{ u/s}$), uses `m_flInaccuracyJumpApex` (`+0x7bc`).
     - Otherwise, linearly interpolates up to `m_flInaccuracyJumpInitial` (`+0x7b8`) towards terminal falling speed.
5. **Strafing & Turning Inaccuracies**:
   - Appends orthogonal strafing inaccuracy from `CalculateStrafingInaccuracy` (`FUN_18080d4c0`).
   - Appends view turning angular velocity penalty `m_flTurningInaccuracy` (`+0x1a14`).
6. **Clamping**:
   - Total composite inaccuracy is clamped to $[0.0, 1.0]$.

---

### 2.2 Accuracy Penalty Update & Exponential Decay (`UpdateAccuracyPenalty` @ `0x180827be0`)

Executes every engine tick to decay accumulated fire recoil penalties back to resting baseline:

1. **Target Posture Inaccuracy**:
   - **Ladder**: Target is `m_flInaccuracyLadder`.
   - **Airborne**: Target is `m_flInaccuracyStand + m_flInaccuracyJump`.
   - **Player Stacking (Boost)**: If standing atop another player pawn and stack depth $\ge \text{weapon\_accuracy\_stack\_boost\_limit}$, applies `m_flInaccuracyLadder * 2.0f`.
   - **Crouching**: Target is `m_flInaccuracyCrouch`.
   - **Standing**: Target is `m_flInaccuracyStand`.
   - **Reloading**: Adds `m_flInaccuracyReload` (`+0x7c0`) if `m_bInReload` is active.
2. **Exponential Recovery ODE Decay**:
   - If $m\_fAccuracyPenalty \ge \text{target}$:
     $$\text{decayFactor} = \exp\left(-\frac{\ln(10.0)}{\text{recoveryTime}} \cdot \Delta t\right)$$
     $$m\_fAccuracyPenalty \leftarrow \text{target} + (m\_fAccuracyPenalty - \text{target}) \cdot \text{decayFactor}$$
   - Where $\Delta t = 0.015625 \text{ s}$ ($64 \text{ Hz}$ tick interval).
   - $\ln(10) \approx 2.302585$ ensures the penalty decays by one full order of magnitude ($90\%$ reduction) across exactly $\text{recoveryTime}$ seconds.
3. **Rapid Continuous Firing Recoil Index Decay**:
   - When firing stops ($t - t_{\text{last\_shot}} > \text{m\_flCycleTime} + \Delta t$):
     $$\text{recoilDecay} = \exp(-2 \ln(10) \cdot \Delta t)$$
     $$m\_flRecoilIndex \leftarrow m\_flRecoilIndex \cdot \text{recoilDecay}$$
   - Cutoff deadzone: When $m\_flRecoilIndex \le 0.1$, it snaps immediately to zero.

---

### 2.3 Weapon Recovery Time Evaluator (`GetRecoveryTime` @ `0x18080cef0`)

Computes the characteristic time $\tau$ (in seconds) required for inaccuracy to reset:

- **Airborne State**: If the player is airborne, recovery time is quadrupled:
  $$\text{recoveryTime}_{\text{air}} = \text{m\_flRecoveryTimeCrouch} \cdot 4.0$$
- **Ladder State**: Uses `m_flRecoveryTimeStand`.
- **Standing vs Crouching**:
  - Crouched: base is `m_flRecoveryTimeCrouch`, final is `m_flRecoveryTimeCrouchFinal`.
  - Standing: base is `m_flRecoveryTimeStand`, final is `m_flRecoveryTimeStandFinal`.
- **Bullet Count Transition**:
  - If `m_flRecoveryTime...Final != -1.0f`, interpolates linearly between base and final recovery times based on recoil bullet index $i_{\text{bullet}}$ between `m_nRecoveryTransitionStartBullet` and `m_nRecoveryTransitionEndBullet`:
    $$\text{frac} = \text{clamp}\left(\frac{i_{\text{bullet}} - \text{start}}{\text{end} - \text{start}}, 0.0, 1.0\right)$$
    $$\tau = \tau_{\text{base}} + \text{frac} \cdot (\tau_{\text{final}} - \tau_{\text{base}})$$

---

### 2.4 Counter-Strike 2 Strafing Inaccuracy (`CalculateStrafingInaccuracy` @ `0x18080d4c0`)

Counter-Strike 2 introduces explicit strafing inaccuracy penalty when counter-strafing or moving perpendicular to the viewing axis:

```cpp
float CalculateStrafingInaccuracy(const PlayerPawnAccuracyContext& ctx, const AccuracyConVars& cvars);
```

1. Computes normalized movement direction $\vec{v}_{\text{dir}} = \frac{\vec{v}}{\|\vec{v}\|}$.
2. Computes eye view direction unit vector $\vec{f}$ via `AngleVectors`.
3. Dot product $\cos\theta = \vec{f} \cdot \vec{v}_{\text{dir}}$.
4. Orthogonal fraction $\text{perpFrac} = 1.0 - |\cos\theta|$.
   - $\text{perpFrac} = 0.0$ when moving purely forward or backward.
   - $\text{perpFrac} = 1.0$ when moving purely sideways (left/right strafe).
5. Non-linear bias power transformation:
   $$\text{biasedPerp} = (\text{perpFrac})^{\text{sv\_strafing\_inaccuracy\_bias}}$$
6. Velocity scaled output:
   $$\text{inaccuracy}_{\text{strafe}} = \left(\frac{\|\vec{v}\|}{250.0}\right) \cdot \text{biasedPerp} \cdot \text{sv\_strafing\_inaccuracy\_scale}$$

---

### 2.5 Recoil Aim Punch Decay ODE (`CPlayer_AimPunchServices::SimulateAimPunchDecay` @ `0x18085dc60`)

Recoil kick on the camera view is governed by `CPlayer_AimPunchServices`:
- `m_predictableBaseAngle` (`+0x0050`): Current aim punch angular deflection $(\text{pitch}, \text{yaw}, \text{roll})$.
- `m_predictableBaseAngleVel` (`+0x005c`): Angular velocity of the punch.

```mermaid
flowchart LR
    A["Kick Impulse (dt=0)"] --> B["Angle Velocity Omega"]
    B --> C["Exponential Damping: exp(-4.5 * dt)"]
    C --> D["Angle Step: Theta += Omega * dt"]
    D --> E["Deadzone Filter: ||Theta||^2 <= 0.140625"]
    E --> F["Clamping [-89 deg, +89 deg]"]
```

#### Differential Equation of Damped Harmonic Motion:
$$\ddot{\vec{\theta}} + c \dot{\vec{\theta}} + k \vec{\theta} = \vec{0}$$

In Valve's discrete integration per tick:
1. **Velocity Damping**:
   $$\vec{\omega}(t + \Delta t) = \vec{\omega}(t) \cdot \exp(-4.5 \cdot \Delta t)$$
2. **Angular Integration**:
   $$\vec{\theta}(t + \Delta t) = \vec{\theta}(t) + \vec{\omega}(t + \Delta t) \cdot \Delta t$$
3. **Deadzone Cutoff**:
   - If $\|\vec{\theta}\|^2 \le 0.140625 \text{ deg}^2$ (corresponding to $0.375^\circ$ magnitude), angular deflection and velocity snap to exactly $(0, 0, 0)$.
   - Otherwise, scales by $(1.0 - \frac{0.140625}{\|\vec{\theta}\|^2})$.
4. **Pitch Clamping**:
   - View pitch is clamped to $[-89.0^\circ, +89.0^\circ]$.

---

## 3. Spread Cone 3D Ray Generation

Once composite inaccuracy and intrinsic weapon spread are calculated:
1. Generate two uniform random variates $u_1, u_2 \in [0, 1)$.
2. Compute radius $r = (\text{inaccuracy} + \text{spread}) \cdot \sqrt{u_1}$.
3. Compute angle $\phi = u_2 \cdot 2\pi$.
4. Project along the view basis vectors:
   $$\vec{d}_{\text{bullet}} = \text{Normalize}\left(\vec{f} + \vec{r} \cdot (r \cos\phi) + \vec{u} \cdot (r \sin\phi)\right)$$

---

## 4. Function Signatures & RVA Shift Registry

| Symbol Name | Binary | Baseline RVA (2026-10-15) | Current RVA (2026-10-25) | Delta | Matches |
|---|---|---|---|---|---|
| `C_CSWeaponBase::UpdateAccuracyPenalty` | `client.dll` | `0x00827be0` | `0x00828a10` | `+0xe30` | 1 |
| `CPlayer_AimPunchServices::SimulateAimPunchDecay` | `client.dll` | `0x0085dc60` | `0x0085ea90` | `+0xe30` | 1 |
| `C_CSWeaponBase::GetInaccuracy` | `client.dll` | `0x0080c1a0` | `0x0080cfd0` | `+0xe30` | 1 |

All signatures verified with 100% uniqueness using `python tools/scan_signatures.py`.
