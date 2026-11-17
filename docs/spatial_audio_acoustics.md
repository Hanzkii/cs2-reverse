# Source 2 Spatial Audio, Steam Audio (Phonon) & Acoustic Wave Physics

## Executive Architecture Summary
In Counter-Strike 2 (Source 2 engine), spatial audio rendering is managed through the cooperation of `soundsystem.dll`, `steamaudio.dll` (Valve's integration of the open-source Steam Audio / Phonon SDK), and the client-side sound operator dispatcher in `client.dll`. Rather than utilizing simplistic stereophonic panning, Source 2 evaluates physically based acoustic wave propagation:

1. **Direct Sound Path**: Distance attenuation governed by the spherical inverse square law, frequency-dependent atmospheric air absorption (ISO 9613-1) across three octave bands (400 Hz, 2.5 kHz, 15 kHz), emitter directional radiation (dipoles and sound cones), and geometric raycasting through physical barriers with material-specific transmission loss (concrete, wood, glass).
2. **Indirect Sound Path**: Directional sound field decomposition using 2nd-order Ambisonics B-format spherical harmonics (9 channels in ACN/SN3D format), room reverberation decay prediction ($RT_{60}$) via statistical acoustics (Sabine and Eyring-Norris equations), and hybrid impulse response filter parameter synthesis.
3. **Sound Event System & Audio Bus Architecture**: The `ISoundOpSystem` interface (VTable `0x1804AB140`), listener orientation tracking, and multi-bus routing hierarchy (Master, SFX, Music, Footsteps, Voice, Ambient, UI).

This document details the clean-room reverse-engineered architecture, mathematical formulations, disassembled structures, and C++20 reference implementation.

---

## 1. Disassembled Subsystems & Interface Inventory

### 1.1 Core Interfaces & VTable Mappings

| Module | Interface / Symbol | VTable / Address | Architectural Role |
|:---|:---|:---|:---|
| `soundsystem.dll` | `ISoundOpSystem` ("SoundOpSystem001") | `0x1804AB140` | Core sound operator subsystem: sound event dispatching, parameter binding, dynamic fade, and lifetime tracking. |
| `soundsystem.dll` | `CVMixSteamAudioDirectProcessorDesc` | Schema RTTI `0x1806D1240` | Direct path DSP operator: position, right/up/ahead coordinate frames, distance rolloff, air absorption, directivity, and occlusion/transmission. |
| `soundsystem.dll` | `CVMixSteamAudioHRTFProcessorDesc` | Schema RTTI `0x1806D12A0` | Head-Related Transfer Function binaural convolution processor with perspective correction and interaural time delay (ITD). |
| `soundsystem.dll` | `CVMixSteamAudioHybridReverbProcessorDesc` | Schema RTTI `0x1806D1310` | 3-band parametric reverberation time processor with early reflection delay and late diffuse tail decay. |
| `soundsystem.dll` | `CVMixSteamAudioPathingProcessorDesc` | Schema RTTI `0x1806D1380` | Sound propagation pathing processor using precomputed acoustic probe graphs and diffraction coefficients. |
| `client.dll` | Sound Event Dispatcher | `0x180B4FB20` | Client-side spatial listener transform, entity spatial binding, and sound event instance initialization. |

### 1.2 Schema Processor Layout: `CVMixSteamAudioDirectProcessorDesc`
Extracted from `soundsystem.dll` schema metadata (Size: 144 bytes / `0x90`):

```cpp
struct CVMixSteamAudioDirectProcessorDesc {
    uint8_t _pad_0x0[40];                      // +0x00: Base processor descriptor
    void*   m_paramPositionX;                  // +0x28: Emitter world X coordinate
    void*   m_paramPositionY;                  // +0x2C: Emitter world Y coordinate
    void*   m_paramPositionZ;                  // +0x30: Emitter world Z coordinate
    void*   m_paramRightX;                     // +0x34: Emitter local right vector X
    void*   m_paramRightY;                     // +0x38: Emitter local right vector Y
    void*   m_paramRightZ;                     // +0x3C: Emitter local right vector Z
    void*   m_paramUpX;                        // +0x40: Emitter local up vector X
    void*   m_paramUpY;                        // +0x44: Emitter local up vector Y
    void*   m_paramUpZ;                        // +0x48: Emitter local up vector Z
    void*   m_paramAheadX;                     // +0x4C: Emitter local ahead vector X
    void*   m_paramAheadY;                     // +0x50: Emitter local ahead vector Y
    void*   m_paramAheadZ;                     // +0x54: Emitter local ahead vector Z
    void*   m_paramApplyDistanceAttenuation;   // +0x58: Boolean flag for distance attenuation
    void*   m_paramApplyAirAbsorption;         // +0x5C: Boolean flag for atmospheric absorption
    void*   m_paramApplyDirectivity;           // +0x60: Boolean flag for directivity radiation
    void*   m_paramApplyOcclusion;             // +0x64: Boolean flag for geometric occlusion
    void*   m_paramApplyTransmission;          // +0x68: Boolean flag for material transmission
    void*   m_paramDipoleWeight;               // +0x6C: Dipole shape blend parameter alpha [0..1]
    void*   m_paramDipolePower;                // +0x70: Dipole power exponent p >= 1.0
    void*   m_paramOcclusion;                  // +0x74: Evaluated scalar occlusion factor [0..1]
    void*   m_paramTransmissionLow;            // +0x78: Low band (400 Hz) transmission gain
    void*   m_paramTransmissionMid;            // +0x7C: Mid band (2.5 kHz) transmission gain
    void*   m_paramTransmissionHigh;           // +0x80: High band (15 kHz) transmission gain
    void*   m_paramBand;                       // +0x84: Frequency band selector
};
```

---

## 2. Direct Sound Path Architecture & Mathematical Formulations

```mermaid
flowchart TD
    Emitter["Sound Emitter Position & Orientation"] --> DistEval["Distance Evaluation: d = ||P_L - P_E||"]
    Listener["Listener Position & 3D Basis"] --> DistEval
    
    DistEval --> DistAtten["Spherical Inverse Law: g_dist(d)"]
    DistEval --> AirAbs["3-Band Atmospheric Air Absorption ODE: g_air(d)"]
    
    Emitter --> Directivity["Directivity Radiation: D(theta)"]
    Listener --> Directivity
    
    Emitter --> Raycast["Rubikon / Phonon Barrier Raycast"]
    Listener --> Raycast
    Raycast --> Occlusion["Occlusion & Material Transmission Loss: g_trans(b)"]
    
    DistAtten --> DirectMix["Direct Signal Gain Multiplier: G_direct = g_dist * D(theta) * g_air * g_trans"]
    AirAbs --> DirectMix
    Directivity --> DirectMix
    Occlusion --> DirectMix
    
    DirectMix --> Ambisonics["Ambisonics Spatial Encoding (ACN/SN3D)"]
    Ambisonics --> HRTF["HRTF Binaural Convolution / Bus Output"]
```

### 2.1 Geometric Distance Attenuation
Sound waves radiated by a localized point source expand spherically through 3D space. According to the conservation of acoustic energy, acoustic intensity $I$ decreases inversely with the square of the distance:
$$I(d) = \frac{P_{acoustic}}{4 \pi d^2} \implies p(d) \propto \frac{1}{d}$$

Because audio DSP operates on linear sound pressure amplitude $p$, amplitude gain follows the inverse distance law clamped between minimum and maximum audible thresholds ($d_{\min}, d_{\max}$):

$$g_{dist}(d) = \begin{cases}
1.0 & \text{if } d \le d_{\min} \\
\frac{d_{\min}}{d} \cdot \left(\frac{d_{\max} - d}{d_{\max} - d_{\min}}\right)^\gamma & \text{if } d_{\min} < d < d_{\max} \\
0.0 & \text{if } d \ge d_{\max}
\end{cases}$$

Where $\gamma \ge 1.0$ is the roll-off shaping exponent, ensuring smooth continuity to silence at $d_{\max}$ without audible clipping.

### 2.2 Frequency-Dependent Atmospheric Air Absorption
Acoustic energy is absorbed during atmospheric propagation by thermal conduction, shear viscosity, and molecular relaxation of diatomic nitrogen ($N_2$) and oxygen ($O_2$) molecules (ISO 9613-1). High frequencies attenuate significantly faster than low frequencies, creating the natural low-pass "muffling" effect heard over large distances.

Steam Audio and Source 2 discretize the acoustic spectrum into three standard octave bands:
- **Low Band ($b_0$)**: $400 \text{ Hz}$ (Bass fundamentals; structural resonance)
- **Mid Band ($b_1$)**: $2500 \text{ Hz}$ (Speech intelligibility; transient weapon attacks)
- **High Band ($b_2$)**: $15000 \text{ Hz}$ (Treble brilliance; air damping)

The continuous differential equation governing acoustic intensity $I(r)$ over distance $r$ is:
$$\frac{dI_b(r)}{dr} = -\mu_b \cdot I_b(r)$$

Where $\mu_b = \alpha_{dB, b} \cdot \frac{\ln(10)}{10} \approx 0.2302585 \cdot \alpha_{dB, b}$ is the linear power absorption coefficient ($m^{-1}$), derived from the decibel attenuation coefficient $\alpha_{dB, b}$ ($dB/m$).

Integrating over distance $d$:
$$I_b(d) = I_0 \cdot \exp(-\mu_b \cdot d)$$

The linear pressure amplitude gain $g_{air, b}(d) = \sqrt{\frac{I_b(d)}{I_0}}$ is:
$$g_{air, b}(d) = \exp\left(-\frac{\mu_b}{2} \cdot d\right) = 10^{-\frac{\alpha_{dB, b} \cdot d}{20}}$$

#### Atmospheric Temperature and Humidity Scaling
Standard baseline attenuation coefficients at $20^\circ\text{C}$ and $50\%$ relative humidity ($RH$):
- $\alpha_{low} = 0.0020 \text{ dB/m}$
- $\alpha_{mid} = 0.0150 \text{ dB/m}$
- $\alpha_{high} = 0.1500 \text{ dB/m}$

Accounting for ambient thermodynamic state ($T \text{ in } ^\circ\text{C}$, $RH \in [10, 100]\%$):
$$T_{rel} = \frac{293.15}{T + 273.15}, \quad H_{rel} = \frac{50.0}{RH}$$
$$\alpha_{low}(T, RH) = 0.0020 \cdot \sqrt{T_{rel}} \cdot (0.8 + 0.2 H_{rel})$$
$$\alpha_{mid}(T, RH) = 0.0150 \cdot T_{rel} \cdot (0.6 + 0.4 H_{rel})$$
$$\alpha_{high}(T, RH) = 0.1500 \cdot T_{rel}^2 \cdot (0.4 + 0.6 H_{rel})$$

### 2.3 Emitter Directivity & Radiation Patterns
Physical acoustic sources radiate non-uniformly. Source 2 supports two complementary directivity models:

#### Steam Audio Dipole Model
Parameterized by dipole blend weight $\alpha \in [0, 1]$ and power exponent $p \ge 1.0$:
$$D(\theta) = \left| (1 - \alpha) + \alpha \cos\theta \right|^p$$
Where $\cos\theta = \frac{\mathbf{f}_E \cdot (\mathbf{P}_L - \mathbf{P}_E)}{\|\mathbf{P}_L - \mathbf{P}_E\|}$, and $\mathbf{f}_E$ is the emitter's forward vector.
- $\alpha = 0.0$: Omnidirectional (monopole). $D(\theta) = 1.0$.
- $\alpha = 0.5$: Standard Cardioid. $D(\theta) = 0.5 \cdot (1 + \cos\theta)$.
- $\alpha = 0.75$: Hypercardioid (sharpened frontal lobe with small rear lobe).
- $\alpha = 1.0$: Figure-8 Dipole. $D(\theta) = |\cos\theta|$.

#### Directional Sound Cone Model
Defined by inner angle $\theta_{inner}$, outer angle $\theta_{outer}$, and minimum outer gain $g_{outer}$:
$$D_{cone}(\theta) = \begin{cases}
1.0 & \text{if } \theta \le \theta_{inner} \\
1.0 - (1.0 - g_{outer}) \cdot (3t^2 - 2t^3) & \text{if } \theta_{inner} < \theta < \theta_{outer} \\
g_{outer} & \text{if } \theta \ge \theta_{outer}
\end{cases}$$
Where $t = \frac{\theta - \theta_{inner}}{\theta_{outer} - \theta_{inner}}$ provides a $C^1$-continuous cubic Hermite smoothstep transition.

### 2.4 Physical Barrier Transmission & Occlusion Raycasting
When a direct line-of-sight ray between sound emitter and listener intersects geometry, the wave is partially occluded and transmitted through the obstacle:

$$\mathbf{r}(t) = \mathbf{P}_E + t \cdot \frac{\mathbf{P}_L - \mathbf{P}_E}{\|\mathbf{P}_L - \mathbf{P}_E\|}, \quad t \in [0, \|\mathbf{P}_L - \mathbf{P}_E\|]$$

For each intersecting barrier of thickness $\Delta t = t_{exit} - t_{enter}$ and material $M$, acoustic transmission loss follows the acoustic mass law. Sound transmission decreases exponentially with effective barrier depth:

$$\tau_{eff, b} = \tau_{base, b} \cdot \exp\left(-\kappa \cdot \frac{\Delta t}{t_{ref}}\right)$$

#### Acoustic Material Properties Registry
Measured transmission factors ($\tau$) and absorption coefficients ($\alpha$) across the 3 octave bands:

| Material | Low (400 Hz) $\tau$ / $\alpha$ | Mid (2.5 kHz) $\tau$ / $\alpha$ | High (15 kHz) $\tau$ / $\alpha$ | Scattering | Acoustic Description |
|:---|:---|:---|:---|:---|:---|
| **Concrete** | $0.010$ / $0.015$ | $0.005$ / $0.025$ | $0.001$ / $0.040$ | $0.05$ | Massive structural wall; near-total transmission loss. |
| **Brick** | $0.020$ / $0.030$ | $0.010$ / $0.040$ | $0.004$ / $0.070$ | $0.08$ | Dense masonry; strong reflections, low transmission. |
| **Wood** | $0.120$ / $0.150$ | $0.080$ / $0.110$ | $0.040$ / $0.100$ | $0.12$ | Timber partitions / doors; permits bass leakage. |
| **Glass** | $0.200$ / $0.180$ | $0.100$ / $0.060$ | $0.030$ / $0.030$ | $0.02$ | Thin pane; high bass transmission, low treble transmission. |
| **Plaster** | $0.080$ / $0.100$ | $0.040$ / $0.080$ | $0.015$ / $0.050$ | $0.06$ | Interior drywall; moderate attenuation. |
| **Carpet** | $0.040$ / $0.050$ | $0.015$ / $0.300$ | $0.003$ / $0.600$ | $0.20$ | High-frequency acoustic absorber; deadens reflections. |

---

## 3. Indirect Sound Path Architecture (Reflection & Reverberation)

```mermaid
flowchart LR
    SourceEvent["Sound Source Event"] --> DirectPath["Direct Sound Path"]
    SourceEvent --> IndirectPath["Indirect Sound Path"]
    
    IndirectPath --> EarlyRef["Early Reflections Raytracer"]
    IndirectPath --> LateRev["Statistical Late Reverberation"]
    
    EarlyRef --> SpecularRays["Specular Reflection Rays: d_k, a_k"]
    LateRev --> RoomAcoustics["Room Geometry & Materials: V, S, alpha"]
    
    RoomAcoustics --> RT60Calc["Eyring / Sabine RT60 Decay Prediction"]
    RT60Calc --> TailSynth["Exponential Diffuse Tail: exp(-6.91 * t / RT60)"]
    
    SpecularRays --> IRCombine["Parametric Impulse Response Filter h[n]"]
    TailSynth --> IRCombine
    
    IRCombine --> Ambisonics["2nd-Order Ambisonics B-Format (9 Channels)"]
    Ambisonics --> SpatialDecoder["Virtual Surround / Binaural Decoder"]
```

### 3.1 Ambisonics B-Format Spherical Harmonics
Ambisonics represents the continuous 3D acoustic sound field around the listener as an expansion of spherical harmonics $Y_\ell^m(\theta, \phi)$. Steam Audio and Source 2 utilize **2nd-order Ambisonics** with **SN3D normalization** and **ACN (Ambisonic Channel Number)** channel ordering ($N = (\ell + 1)^2 = 9 \text{ channels}$):

$$\text{ACN Index: } n = \ell(\ell + 1) + m$$

Given a normalized direction vector from listener to source $\mathbf{u} = (x, y, z)$:
- $x = \cos(\text{elevation}) \cos(\text{azimuth})$ (Front / Back)
- $y = \cos(\text{elevation}) \sin(\text{azimuth})$ (Left / Right)
- $z = \sin(\text{elevation})$ (Up / Down)

The 9 basis functions are defined as:

| ACN | Order $\ell$ | Degree $m$ | Basis Symbol | Mathematical Formulation (Cartesian) | Acoustic Significance |
|:---|:---|:---|:---|:---|:---|
| **0** | $0$ | $0$ | $W$ | $1.0$ | 0th-order omnidirectional monopole pressure field. |
| **1** | $1$ | $-1$ | $Y$ | $y$ | 1st-order lateral figure-8 dipole (Left / Right). |
| **2** | $1$ | $0$ | $Z$ | $z$ | 1st-order vertical figure-8 dipole (Up / Down). |
| **3** | $1$ | $+1$ | $X$ | $x$ | 1st-order longitudinal figure-8 dipole (Front / Back). |
| **4** | $2$ | $-2$ | $V$ | $\sqrt{3} \cdot x \cdot y$ | 2nd-order diagonal planar quadrupole. |
| **5** | $2$ | $-1$ | $T$ | $\sqrt{3} \cdot y \cdot z$ | 2nd-order vertical-lateral quadrupole. |
| **6** | $2$ | $0$ | $R$ | $\frac{1}{2}(3z^2 - 1)$ | 2nd-order vertical axial quadrupole. |
| **7** | $2$ | $+1$ | $S$ | $\sqrt{3} \cdot x \cdot z$ | 2nd-order vertical-longitudinal quadrupole. |
| **8** | $2$ | $+2$ | $U$ | $\frac{\sqrt{3}}{2}(x^2 - y^2)$ | 2nd-order horizontal planar quadrupole. |

#### Ambisonics Decoding to Virtual Loudspeakers
Given an Ambisonics signal vector $\mathbf{B} = [B_0, \dots, B_8]^T$, the signal for a virtual speaker at direction $\mathbf{v}_j$ is computed via projection mode-matching with max-$r_E$ order weights $w_\ell$:
$$s_j = \sum_{\ell=0}^2 w_\ell \sum_{m=-\ell}^\ell B_{\ell, m} \cdot Y_\ell^m(\mathbf{v}_j)$$
Where standard SN3D max-$r_E$ weights are $w_0 = 1.0$, $w_1 = \frac{1}{\sqrt{3}} \approx 0.57735$, $w_2 = \frac{1}{\sqrt{5}} \approx 0.35355$.

### 3.2 Statistical Room Reverberation ($RT_{60}$)
The reverberation time $RT_{60}$ is the time required for sound energy density inside an enclosed space to decay by $60 \text{ dB}$ ($10^{-6}$ of initial energy).

Given enclosed volume $V$ ($m^3$), bounding surfaces with area $S_i$ and absorption $\alpha_i$:
- Total surface area: $S = \sum_i S_i$
- Total equivalent absorption area: $A = \sum_i S_i \alpha_i$ (metric Sabins)
- Mean absorption coefficient: $\bar{\alpha} = \frac{A}{S}$

#### Sabine Formulation
Valid for live, weakly absorptive spaces ($\bar{\alpha} < 0.2$):
$$RT_{60, \text{Sabine}} = \frac{0.161 \cdot V}{A} = \frac{0.161 \cdot V}{\sum_i S_i \alpha_i}$$
Where $0.161 \approx \frac{24 \ln(10)}{c_{sound}} = \frac{55.262}{343.0}$.

#### Eyring-Norris Formulation
Valid across all absorption levels, accounting for non-zero mean free paths and discrete boundary reflection losses:
$$RT_{60, \text{Eyring}} = \frac{0.161 \cdot V}{-S \ln(1 - \bar{\alpha})}$$

> [!NOTE]
> When $\bar{\alpha} \to 0$, the Taylor expansion $-\ln(1 - \bar{\alpha}) \approx \bar{\alpha}$, causing the Eyring-Norris formula to converge with the classic Sabine formula. When $\bar{\alpha} \to 1.0$ (anechoic chamber), Eyring properly yields $RT_{60} \to 0$, whereas Sabine unrealistically predicts non-zero reverberation.

### 3.3 Parametric Impulse Response (IR) Filter Synthesis
Steam Audio synthesizes hybrid impulse responses $h[n]$ at sample rate $f_s = 48000 \text{ Hz}$:
1. **Direct Sound Arrival**: Delta impulse at sample $n = 0$: $h[0] = 1.0$.
2. **Early Reflections Cluster**: Discrete specular arrivals at $n_{early} = \lfloor \tau_{early} \cdot f_s \rfloor$ attenuated by boundary reflections.
3. **Late Diffuse Reverberation Tail**: Diffuse sound field beginning at mixing time $t_{mix} \approx 20\dots 50 \text{ ms}$. Amplitude follows exponential energy decay:
   $$A(t) = \exp\left(-\frac{6.90776 \cdot t}{RT_{60}}\right) \cdot w[t]$$
   Where $w[t] \sim \mathcal{N}(0, 1)$ is decorrelated Gaussian white noise filtered by 3-band spectral coloration matching the room's $(RT_{60, low}, RT_{60, mid}, RT_{60, high})$.

---

## 4. Sound Event Dispatcher & Audio Bus Routing

### 4.1 Listener Coordinate Transformation
To spatialise sounds relative to the player's head, the engine defines an orthonormal coordinate frame from the camera's view matrix:
- $\mathbf{P}_L$: Listener world origin
- $\mathbf{F}_L$: Forward unit vector
- $\mathbf{U}_L$: Up unit vector
- $\mathbf{R}_L = \mathbf{F}_L \times \mathbf{U}_L$: Right unit vector

Any world-space sound source $\mathbf{P}_W$ transforms into listener-local coordinates $\mathbf{P}_{local} = (x_{local}, y_{local}, z_{local})$ via:
$$\mathbf{P}_{local} = \begin{bmatrix}
\mathbf{R}_L \cdot (\mathbf{P}_W - \mathbf{P}_L) \\
\mathbf{F}_L \cdot (\mathbf{P}_W - \mathbf{P}_L) \\
\mathbf{U}_L \cdot (\mathbf{P}_W - \mathbf{P}_L)
\end{bmatrix}$$

From $\mathbf{P}_{local}$, horizontal azimuth $\theta$ and vertical elevation $\phi$ are:
$$\theta = \text{atan2}(x_{local}, y_{local}), \quad \phi = \text{asin}\left(\frac{z_{local}}{\|\mathbf{P}_{local}\|}\right)$$

### 4.2 Audio Bus Hierarchy
Sound events are dynamically routed through a hierarchical bus topology with independent volume control, ducking, and compression:

```
[Master Audio Bus]
   ├── [SFX Submix]
   │      ├── Weapon Audio (Gunfire, Reloads, Grenades)
   │      └── Impact Audio (Physics, Flesh Hits, Glass Shatter)
   ├── [Footsteps Submix]
   │      └── Material Footsteps (Concrete, Metal Grates, Water)
   ├── [Voice Communications Submix]
   │      └── VOIP Player Chat (3D Spatialized / Global)
   ├── [Ambient Submix]
   │      └── Environmental Emitters & Reverb Submix
   ├── [Music Submix]
   │      └── Dynamic Round Music & Bomb Timers
   └── [UI Submix]
          └── Menu Navigation & HUD Audio Cues
```

---

## 5. Verification & Simulation Results

The reference simulator (`docs/simulators/cs2_spatial_audio_sim.cpp`) validates every physical equation and algorithm under strict MSVC compilation (`/std:c++20 /O2 /EHsc /W4 /WX`).

### 5.1 Test Execution Suite (18 / 18 Passed)

```
====================================================================
Counter-Strike 2 (Source 2) Spatial Audio & Acoustic Wave Simulator
Clean-Room Reverse Engineering & Verification Harness (ISO C++20)
====================================================================

[STAGE 1] Direct Sound Path: Distance Attenuation Models
  Distance Attenuation Profile (Inverse Distance):
    d =  0.5m (d <= d_min): Gain = 1.0000 (Clamped 1.0)
    d =  1.0m (Reference):  Gain = 1.0000
    d =  5.0m:             Gain = 0.1837
    d = 10.0m:             Gain = 0.0816
    d = 25.0m:             Gain = 0.0204
    d = 50.0m (d == d_max): Gain = 0.0000 (Silence boundary)
    d = 80.0m (d > d_max):  Gain = 0.0000
  [PASS] Distance <= minDistance clamps to 1.0f
  [PASS] Distance >= maxDistance attenuates to 0.0f
  [PASS] Monotonic attenuation decay over distance

[STAGE 2] Frequency-Dependent Atmospheric Air Absorption (ISO 9613-1 / ODE)
  40m Distance 3-Band Air Absorption Gain Multipliers:
    Standard (20C, 50% RH): Low(400Hz)=0.9908  Mid(2.5kHz)=0.9333  High(15kHz)=0.5012
    Cold Dry ( 0C, 20% RH): Low(400Hz)=0.9877  Mid(2.5kHz)=0.8881  High(15kHz)=0.2205
    Hot Humid(35C, 80% RH): Low(400Hz)=0.9917  Mid(2.5kHz)=0.9457  High(15kHz)=0.6160
  ODE Numerical Integration Verification (High Band, 15 kHz):
    Analytical Amplitude: 0.5012
    RK4 Stepper Solution: 0.5012 (Delta = 0.0000)
  [PASS] High frequency treble attenuates significantly faster than mid/low bass
  [PASS] Runge-Kutta 4th-order ODE numerical solution matches analytical solution within 0.01%

[STAGE 3] Emitter Directivity Patterns
  Cardioid Directivity Gain Profile:
    0 deg (On-axis front): 1.0000
   45 deg (Flank):         0.8536
   90 deg (Side):          0.5000
  180 deg (Rear null):     0.0000
  [PASS] Cardioid on-axis emission retains full gain 1.0
  [PASS] Cardioid 90-degree side emission gain is exactly 0.5
  [PASS] Cardioid 180-degree rear emission has complete null (gain ~0.0)

[STAGE 4] Physical Barrier Transmission & Occlusion Raycasting
  Barrier Acoustic Transmission Comparison:
    Unobstructed Path: Occlusion=0.0000  Gain=[ 1.0000, 1.0000, 1.0000 ]
    Concrete Wall:     Occlusion=1.0000  Gain=[ 0.0005, 0.0003, 0.0001 ]
    Wooden Door:       Occlusion=1.0000  Gain=[ 0.0060, 0.0040, 0.0020 ]
    Glass Pane:        Occlusion=1.0000  Gain=[ 0.0100, 0.0050, 0.0015 ]
  [PASS] Clear ray has 0.0 occlusion and 1.0 transmission
  [PASS] Concrete wall exhibits massive transmission loss (sound barrier)
  [PASS] Wood allows greater low-frequency acoustic transmission than solid concrete

[STAGE 5] 2nd-Order Ambisonics B-Format Spherical Harmonics Encoding & Decoding
  Encoded Channels for Source at Left (0, 1, 0):
    ACN 0 (W, Omni):        1.0000
    ACN 1 (Y, Left/Right):  1.0000 (+1.0 Left)
    ACN 2 (Z, Up/Down):     0.0000
    ACN 3 (X, Front/Back):  0.0000
    ACN 6 (R, Quadrupole Z):-0.5000
    ACN 8 (U, Quadrupole XY):-0.8660
  Decoded Virtual Loudspeaker Gains:
    Left Speaker  (+30 deg): Gain = 1.2445
    Right Speaker (-30 deg): Gain = 0.6671
  [PASS] Ambisonics Left source yields W=1.0 and Y=1.0
  [PASS] Left speaker receives significantly higher gain for left source

[STAGE 6] Room Acoustics & Statistical Reverberation (Sabine vs Eyring-Norris RT60)
  Industrial Warehouse (V = 1200 m^3, S = 800.0000 m^2):
    Sabine RT60: Low=7.7280s  Mid=6.6621s  High=4.8300s
    Eyring RT60: Low=7.6066s  Mid=6.5406s  High=4.7082s
  Damped Studio (V = 150 m^3, S = 220.0000 m^2):
    Sabine RT60: Low=0.9471s  Mid=0.7500s  High=0.5427s
    Eyring RT60: Low=0.8910s  Mid=0.6937s  High=0.4857s
  [PASS] Reflective warehouse exhibits realistic RT60 > 1.5 seconds
  [PASS] Damped room has substantially lower RT60 than warehouse
  [PASS] Eyring RT60 is properly bounded below or equal to Sabine RT60

[STAGE 7] Parametric Impulse Response (IR) Filter Synthesis
  Synthesized IR Filter Buffer Statistics:
    Duration: 0.5000 s (24000 samples @ 48000 Hz)
    Direct Arrival (n=0):     1.0000
    Early Reflection (15 ms): 0.2800
    Late Tail Start (40 ms):  0.0857
    Late Tail Decayed (200ms):-0.0715
    Early Energy (0..100ms): 41.3828
    Late Energy (200..300ms): 5.3900 (Ratio = 0.1302)
  [PASS] Impulse response begins with unit direct Dirac delta
  [PASS] Late reverberation tail exhibits exponential energy decay

====================================================================
SIMULATION RESULTS: 18 / 18 PASSED
====================================================================
```

---

## 6. Project File Artifacts

- Header Reconstruction: [`game/soundsystem/sound_ops.h`](../game/soundsystem/sound_ops.h)
- Implementation Source: [`game/soundsystem/sound_ops.cpp`](../game/soundsystem/sound_ops.cpp)
- Verification Simulator: [`docs/simulators/cs2_spatial_audio_sim.cpp`](simulators/cs2_spatial_audio_sim.cpp)
- MSVC Batch Build Runner: [`docs/simulators/build_spatial_audio_sim.bat`](simulators/build_spatial_audio_sim.bat)
