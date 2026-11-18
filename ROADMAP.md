# CS2 Reverse Engineering & Architecture: Development Roadmap

> **Main Project Objective**: Authoritative ground-truth reverse engineering, decompilation, VTable mapping, SchemaSystem reconstruction, and standalone C++20/interactive web verification models for Valve's **Source 2** engine in **Counter-Strike 2**.

---

## 🚀 Active Focus: Simulators Suite 2.0 (Full Web Visualizer Rewrite)

**Target**: Complete overhaul of the interactive web visualizers at [`https://hanzkii.github.io/cs2-reverse/docs/#simulators/`](https://hanzkii.github.io/cs2-reverse/docs/#simulators/).

Transform the existing 2D canvas representations into a high-fidelity **Interactive CS2 Physics & Telemetry Lab**:

### 1. 🎨 Visual Window & Rendering Upgrade (Three.js / WebGL / Canvas)
- **3D Interactive Viewport**:
  - Full 3D orbit/pan/zoom viewport with perspective and orthographic camera modes.
  - Hardware-accelerated rendering for 3D player skeleton hierarchies, rotated bounding capsules (`CHitBox`), Rubikon BVH trees, and volumetric voxel grids.
  - "Theater / Lab Mode" with full-screen toggle, dark theme aesthetic matching the Source 2 debug renderer (`r_drawothermodels`, `vcollide_wireframe`).
- **High-Fidelity Animations & Temporal Smoothing**:
  - Continuous sub-tick timeline playback with smooth interpolation ($\Delta t$ slicing).
  - Step-by-step playback controls: Play/Pause, Step $\pm 1$ Tick, Scrub timeline, Slow-motion ($0.1\times, 0.25\times, 0.5\times, 1.0\times$).
  - Dynamic visual particle tracers, shockwave ripples, and heatmaps (e.g. bullet penetration energy loss, Inferno fire propagation wavefronts).

### 2. 🔊 Web Audio & Acoustic Simulation
- **Web Audio API Engine**:
  - Procedural sound synthesis and real-time audio feedback matching Source 2's **Sound Operator System (SOS)**.
  - **Acoustic Spatialization**: 3D spatial audio panning and distance attenuation curves matching `soundsystem.dll`.
  - **Sub-Tick Audio Sync**: Audible tick impulses and micro-slice clicks demonstrating audio-thread vs simulation-thread synchronization.
  - **Ballistic & Impact Sound Effects**: Procedural ricochets, surface penetration thuds (wood vs metal vs concrete), headshot dings, and smoke bloom decompression hiss.

### 3. 🔬 Simulator-Specific Enhancements
- **Sim 03 (Sub-Tick Prediction)**:
  - 3D jump trajectory with tick boundary division slices and ground friction deceleration vectors.
  - Audible click on UserCmd button press timing within the $15.625\text{ ms}$ tick window.
- **Sim 11 (Volumetric Smoke)**:
  - 3D voxel density raymarching with dynamic bullet cavity carving and HE grenade blast dispersal animations.
- **Sim 14 (Bullet Penetration)**:
  - Multi-material 3D cross-section showing entry/exit normals, kinetic energy decay curve, and ricochet angle gating.
- **Sim 12 (Lag Compensation & Unlag)**:
  - Interactive ghost backtrack visualization showing client view vs server rewind buffer tick history.
- **Sim 09 (Skeleton & Two-Bone IK)**:
  - Interactive 3D limb manipulator demonstrating analytical Law of Cosines inverse kinematics and quaternion SLERP blending.

### 4. Implementation Status
- [x] **Milestone 1 — Core Lab Architecture**:
  - `Viewport3D`: Three.js r128 renderer, dual perspective/orthographic cameras, Hammer unit grid, Source 2 debug styling, Theater/Fullscreen mode.
  - `AudioEngine`: 100% procedural Web Audio API synthesis (metronome tick pulse, sub-tick micro-slice clicks, gunfire impulses, surface impacts, ricochets, smoke detonation, continuous hiss loop, HE explosion, helmet headshot dink, and flesh impacts).
  - `PlaybackController`: Continuous sub-tick timeline scrubber, $\pm 1$ tick stepping, slow-motion presets ($0.1\times, 0.25\times, 0.5\times, 1.0\times$), real-time telemetry HUD overlay.
- [x] **Milestone 2 — Sim 03 (Sub-Tick Prediction) & Sim 09 (Skeleton / Two-Bone IK)**:
  - `Sim03SubtickProvider`: Piecewise continuous integration, ground friction deceleration, UserCmd sub-tick click scheduling, Protobuf wire inspector.
  - `Sim09SkeletonIKProvider`: 19-bone topological FK propagation, 13 rotated bounding capsules (`CHitBox`), analytical two-bone Law of Cosines IK solver, SLERP pose blending, 3D target manipulator gizmo.
- [x] **Milestone 3 — Sim 11 (Smoke), Sim 14 (Penetration), Sim 12 (Lag Comp)**:
  - `Sim11VolumetricSmokeProvider`: $32\times 32\times 32$ voxel grid, Morton Z-order curve bit interleaving, 3D DDA optical line-of-sight raymarching ($\rho \ge 0.20$ occlusion threshold), bullet cavity carving ($R=32\text{ u}$), HE grenade blast core and shockwave displacement ($R=180\text{ u} \to 260\text{ u}$), 10-byte sparse delta wire compression.
  - `Sim14BulletPenetrationProvider`: Multi-material 3D slabs (Wood, Concrete, Metal, Cardboard, Glass), kinetic dissipation ODE, ricochet gating ($\theta > 65^\circ$), player pawn armor mitigation jump table.
  - `Sim12LagCompensationProvider`: 64-tick circular history ring buffer ($1.0\text{ s}$ retention), authoritative server unlag rollback (`sv_maxunlag = 0.200s`), live server pawn (Cyan) vs rewound lag-compensated ghost (Amber), authoritative hit registration raycast.
- [x] **Milestone 4 — E2E Verification & Guardrails**:
  - Unified E2E test runner (`tests/run_e2e_tests.py` & `tests/e2e/runner.js`) with 437/437 passing tests across all 4 tiers (Feature Coverage, Boundary/Corner, Cross-Feature, Workloads).
  - Zero local path leaks, zero binaries in remote repository.

---

## 🗺️ Long-Term Core Roadmap

### Phase 1: Reconstructed C++20 SDK Completeness
- [x] Master SDK headers (`client_classes.hpp`, `entity_system.hpp`, `interfaces_registry.hpp`).
- [x] Zero-cost `game_bridge.hpp` integration connecting SDK types with modular subsystems.
- [ ] Automated SchemaSystem class generator expansion for full network entity coverage.
- [ ] VTable completeness verification across all 34 Source 2 modular PE binaries.

### Phase 2: Binary Update & Differential Automation
- [x] Automated binary diffing engine (`cs2_patch_diff.py`).
- [x] 100% unique IDA/Ghidra byte pattern scanner (`scan_signatures.py`).
- [ ] Automated Ghidra headless progress porting between Steam patch versions.

### Phase 3: Interactive Web Portal & Static API
- [x] Interactive web documentation with KaTeX math and Mermaid diagrams.
- [x] Static JSON API endpoints (`docs/api/v1/sdk.json`) for automated AI analysis.
- [ ] **Simulators Suite 2.0**: Full audio-visual interactive lab rewrite.
