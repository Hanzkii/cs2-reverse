# Valve Source 2 Skeleton Bone Hierarchy & Animation Graph Architecture

## Executive Summary

Through static binary reverse engineering of `client.dll` and `animationsystem.dll`, we reconstructed the architecture of Valve's **Source 2 Skeletal Animation and Hitbox Pipeline**.

In Source 2, characters, weapons, and animated props are driven by a unified skeletal transform and animation graph system:
1. **Packed 32-Byte `CTransform` Representation**: Every bone transform in memory is represented as a SIMD-friendly 32-byte struct containing a 16-byte unit quaternion rotation $(q_x, q_y, q_z, q_w)$, a 12-byte position vector $(p_x, p_y, p_z)$, and a 4-byte uniform scale factor $s$.
2. **Forward Kinematics (FK) with Lazy Bitmask Caching**: `CSkeletonInstance::CalculateWorldBones` (`FUN_180aa0f80`) evaluates bones in topological parent-first order, caching calculated states into a compact 32-bit word bitmask ($(\text{boneCount} + 31) / 32$ words) guarded by thread-safe atomic mutex locks (`CAtomicMutex`).
3. **Procedural Hitbox Binding**: `CHitBox` instances (`animationsystem.dll`) define local bounding volumes (Axis-Aligned Boxes or Capsule Cylinders with radius $r$) parented to specific bones via 32-bit name hashes (`m_nBoneNameHash`). These local primitives are dynamically transformed into world-space Oriented Bounding Boxes (OBB) and Capsules for sub-tick lag compensation and bullet ray-tracing.
4. **Analytical Two-Bone Inverse Kinematics (IK)**: `CTwoBoneIKPoseOperation::Execute` (`FUN_1802f9750`) implements the Law of Cosines to solve limb bending (arms and legs) against dynamic terrain or weapon grip points, projecting joints along a specified hinge axis and pole vector.

---

## 1. System Topology & Data Flow

```mermaid
flowchart TD
    subgraph Entity ["client.dll Entity Model"]
        Pawn["C_CSPlayerPawn"] --> SceneNode["m_pGameSceneNode (+0x0330)"]
        SceneNode --> SInst["CSkeletonInstance (Size: 1168 bytes)"]
        SInst --> MState["m_modelState (+0x0140, Size: 688 bytes)"]
        MState --> BonePtr["m_pBoneArray (+0x01c0 / m_modelState + 0x80)"]
        MState --> BoneCount["m_nBoneCount (+0x01d4 / m_modelState + 0x94)"]
    end

    subgraph AnimSystem ["animationsystem.dll Evaluation"]
        AnimGraph["CAnimGraph2Instance / AnimGraphController"] --> FK["Forward Kinematics Traversal"]
        FK --> Cat["CTransform Concatenation (FUN_181748480)"]
        Cat --> IK["Two-Bone IK Solver (FUN_1802f9750)"]
    end

    subgraph Collision ["Hit Registration & Hitboxes"]
        Hitboxes["CHitBox Array (animationsystem.dll)"] --> TransOBB["World-Space OBB / Capsule Generation"]
        TransOBB --> LagComp["Sub-Tick Lag Compensation Raytracing"]
    end

    IK --> BonePtr
    BonePtr --> Hitboxes
```

---

## 2. Binary Signatures & VTable Offsets

### 2.1 `client.dll` (Base: `0x180000000`)
- **`CSkeletonInstance` VTable**: `0x181c41ce0` (35 virtual methods).
- **`CalculateWorldBones` (`FUN_180aa0f80`)**: Core bone hierarchy evaluation engine.
- **`CTransform::Concatenate` (`FUN_181748480`)**: SIMD quaternion/vector transform multiplication.
- **`CSkeletonInstance` Bone Field Offsets**:
  - `m_nodeToWorld`: `+0x0010` (Root `CTransform`, 32 bytes)
  - `m_pOwner`: `+0x0030`
  - `m_pParent`: `+0x0038`
  - `m_modelState`: `+0x0140` (`CModelState`, 688 bytes)
  - `m_pBoneArray`: `+0x01c0` (`m_modelState + 0x80`, pointer to `CTransform[N]`)
  - `m_nBoneCount`: `+0x01d4` (`m_modelState + 0x94`, `uint16_t`)
  - `m_atomicMutex`: `+0x0114` (`CAtomicMutex` for thread-safe calculation)
  - `m_nCalculatedBoneFlags`: `+0x0458` (Evaluation dirty mask)

### 2.2 `animationsystem.dll` (Base: `0x180000000`)
- **`CTwoBoneIKPoseOperation` VTable**: `0x180689268`
- **`SolveTwoBoneIK` (`FUN_1802f9750`)**: Law of Cosines analytical limb solver.
- **`CHitBox` Class Info**: `0x18081d840` (Size: 112 bytes)
- **`CHitBoxSet` Class Info**: `0x18081d940` (Size: 48 bytes)

---

## 3. Data Structure: The 32-Byte `CTransform`

In Source 2, all bone matrices in memory are packed as `CTransform`:

```cpp
struct alignas(16) CTransform {
    // Quaternion Rotation: (qx, qy, qz, qw)
    // Unit quaternion representing 3D spatial orientation
    float qx, qy, qz, qw;  // +0x00 (16 bytes)

    // Vector3D Translation: (px, py, pz)
    // 3D positional offset in parent space or world space
    float px, py, pz;      // +0x10 (12 bytes)

    // Uniform Scale Factor: s
    float scale;           // +0x1C (4 bytes)
};
static_assert(sizeof(CTransform) == 32, "CTransform must be 32 bytes");
```

### Transform Concatenation (`FUN_181748480`)
When combining a parent transform $\mathbf{T}_{parent} = (\mathbf{q}_p, \mathbf{p}_p, s_p)$ and a child local transform $\mathbf{T}_{local} = (\mathbf{q}_l, \mathbf{p}_l, s_l)$, the resulting world transform is:

1. **Position Concatenation**:
   $$\mathbf{p}_{world} = \mathbf{p}_p + s_p \cdot \mathbf{R}(\mathbf{q}_p, \mathbf{p}_l)$$
   where $\mathbf{R}(\mathbf{q}, \mathbf{v})$ evaluates spatial vector rotation via Rodrigues' formula (SIMD implementation in `FUN_181748480`):
   $$\mathbf{t} = 2 (\mathbf{q}_{xyz} \times \mathbf{v})$$
   $$\mathbf{R}(\mathbf{q}, \mathbf{v}) = \mathbf{v} + q_w \mathbf{t} + \mathbf{q}_{xyz} \times \mathbf{t}$$
   *(Note: This requires **zero trigonometric calls** $\sin/\cos$, operating in pure multiply-adds).*

2. **Rotation Concatenation**:
   $$\mathbf{q}_{world} = \mathbf{q}_p \otimes \mathbf{q}_l$$
   The resulting quaternion is normalized using SIMD dot-product (`dpps`) and square root (`sqrtps`):
   $$\mathbf{q}_{world} = \frac{\mathbf{q}_{world}}{\|\mathbf{q}_{world}\|}$$

3. **Scale Concatenation**:
   $$s_{world} = s_p \times s_l$$

---

## 4. Forward Kinematics Hierarchy (`FUN_180aa0f80`)

`CalculateWorldBones` traverses the skeleton topological array:

```mermaid
sequenceDiagram
    participant Engine as Animation Loop / Renderer
    participant Mutex as CAtomicMutex (+0x0114)
    participant SInst as CSkeletonInstance
    participant Bitmask as Lazy Mask (+0x0458)
    participant BoneArray as Bone Array (+0x01c0)

    Engine->>SInst: CalculateWorldBones(flags)
    SInst->>Bitmask: Check if already calculated for tick
    alt Already evaluated
        Bitmask-->>Engine: Early Return (0 cost)
    else Needs Evaluation
        SInst->>Mutex: AcquireLock()
        loop For each bone i from 0 to m_nBoneCount-1
            alt Root Bone (ParentIndex is negative)
                SInst->>BoneArray: Concatenate(m_nodeToWorld, Local[i])
            else Child Bone (ParentIndex is non-negative)
                SInst->>BoneArray: Concatenate(BoneArray[parent], Local[i])
            end
            SInst->>Bitmask: Set Bit (i mod 32) in Word (i / 32)
        end
        SInst->>Mutex: ReleaseLock()
    end
```

### Lazy Evaluation Bitmask
To prevent redundant recalculations during a single render frame (where HUD, shadow maps, bullet hit checks, and render passes all query bones), Source 2 maintains:
- `m_nCalculatedBoneFlags` at `+0x0458`.
- If `(m_nCalculatedBoneFlags & requestedFlags) == requestedFlags`, the function exits immediately at line 147.
- A dynamic array of bitmask words:
  $$\text{numWords} = \frac{N + 31}{32}$$
  Each bone $i$ flips bit `1 << (i % 32)` in word `i / 32` when calculated.

---

## 5. Procedural Hitbox System (`animationsystem.dll`)

Hitboxes are defined per model in `CHitBox` (`0x18081d840`):

| Offset | Field | Type | Description |
|:---|:---|:---|:---|
| `+0x0000` | `m_name` | `CUtlString` | Identifier (e.g. `"head"`, `"chest"`, `"left_arm"`) |
| `+0x0010` | `m_sBoneName` | `CUtlString` | Attached bone name |
| `+0x0018` | `m_vMinBounds` | `Vector3D` | Local minimum bound (or Capsule Start) |
| `+0x0024` | `m_vMaxBounds` | `Vector3D` | Local maximum bound (or Capsule End) |
| `+0x0030` | `m_flShapeRadius`| `float` | Capsule radius ($r > 0$ indicates capsule) |
| `+0x0034` | `m_nBoneNameHash`| `uint32_t` | MurmurHash2 bone hash |
| `+0x0038` | `m_nGroupId` | `int32_t` | Hitgroup: Head (1), Chest (2), Stomach (3), Arms (4/5), Legs (6/7), Neck (8) |
| `+0x003c` | `m_nShapeType` | `uint8_t` | `0` = Oriented Box, `1` = Capsule |

### World-Space Projection
To evaluate a hitbox against incoming bullets or lag compensation records:
1. Lookup the bone's world transform $\mathbf{T}_{world, bone} = \mathbf{M}_{bone}$.
2. Transform the capsule endpoints into world space:
   $$\mathbf{P}_{A, world} = \mathbf{T}_{world, bone} \cdot \mathbf{v}_{min}$$
   $$\mathbf{P}_{B, world} = \mathbf{T}_{world, bone} \cdot \mathbf{v}_{max}$$
3. Hitbox center:
   $$\mathbf{C}_{world} = \frac{\mathbf{P}_{A, world} + \mathbf{P}_{B, world}}{2}$$

---

## 6. Analytical Two-Bone Inverse Kinematics (`FUN_1802f9750`)

Used by `CTwoBoneIKPoseOperation` to bend character knees and elbows towards target footsteps and weapon grips.

```
       Root (Hip / Shoulder)
        o
       / \
  L1  /   \  L2
     /     \
    o-------o  Target (Foot / Hand)
  Joint        Distance D
  (Knee/Elbow)
```

### Mathematical Formulation
Given bone lengths $L_1, L_2$, root position $\mathbf{R}$, target $\mathbf{T}$, and a pole vector $\mathbf{P}$ (hint for bending direction):

1. **Distance to Target**:
   $$\mathbf{D} = \mathbf{T} - \mathbf{R}, \quad d = \|\mathbf{D}\|$$
   Clamped to $[|L_1 - L_2| \cdot 1.0001, (L_1 + L_2) \cdot 0.9999]$ to prevent trigonometric domain errors.

2. **Law of Cosines (Joint Angles)**:
   $$\cos(\alpha) = \frac{L_1^2 + d^2 - L_2^2}{2 L_1 d}$$
   $$\alpha = \arccos(\cos(\alpha)) \quad \text{(Evaluated via } \texttt{V\_acosf} \text{ at line 237)}$$

3. **Hinge Plane Construction**:
   $$\mathbf{n}_{bend} = \frac{\mathbf{D} \times (\mathbf{P} - \mathbf{R})}{\|\mathbf{D} \times (\mathbf{P} - \mathbf{R})\|}$$

4. **Joint Spatial Placement**:
   $$\mathbf{q}_{bend} = \text{AngleAxis}(\mathbf{n}_{bend}, \alpha)$$
   $$\mathbf{J} = \mathbf{R} + \mathbf{q}_{bend} \cdot \left(\frac{\mathbf{D}}{d}\right) \cdot L_1$$

The end effector $\mathbf{E} = \mathbf{J} + \text{Normalize}(\mathbf{T} - \mathbf{J}) \cdot L_2$ reaches the exact target point with zero iteration error.

---

## 7. Verification Simulator

The standalone C++17 verification simulator is available in:
- Code: [`simulators/cs2_skeleton_animation_sim.cpp`](../simulators/cs2_skeleton_animation_sim.cpp)
- Build Script: [`simulators/build_skeleton_anim_sim.bat`](../simulators/build_skeleton_anim_sim.bat)

```cmd
cd simulators
build_skeleton_anim_sim.bat
```

Output:
```
[Test 1] Verifying CTransform Concatenation (FUN_181748480)... PASSED! Pos=(100, 210, 79)
[Test 2] Verifying Skeletal Hierarchy (CSkeletonInstance) & Hitbox Projection... PASSED! Head Bone Z=204, Hitbox Center=(503, 1000, 205)
[Test 3] Verifying Two-Bone IK Analytical Law of Cosines (FUN_1802f9750)... PASSED! Knee Pos=(0, 11.8809, 19.9114), Target Err=0

>> All 3 verification test suites successfully passed!
```
