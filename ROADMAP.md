# CS2 Reverse Engineering & Architecture: Development Roadmap

> **Main Project Objective**: Authoritative ground-truth reverse engineering, decompilation, VTable mapping, SchemaSystem reconstruction, and standalone C++20/interactive web verification models for Valve's **Source 2** engine in **Counter-Strike 2**.

---

## 🏆 Project Milestone Status: 100% Complete Across All Phases

All planned architecture milestones across Phase 1 (Reconstructed C++20 SDK Completeness), Phase 2 (Binary Update & Differential Automation), and Phase 3 (Interactive Web Portal & Static API / Simulators Suite 2.0) have been successfully implemented, audited, and verified.

---

## 🚀 Simulators Suite 2.0 (High-Fidelity Audio-Visual Interactive Lab)

**Portal URL**: [`https://hanzkii.github.io/cs2-reverse/docs/#simulators/`](https://hanzkii.github.io/cs2-reverse/docs/#simulators/)

Transformed the static 2D canvas representations into a high-fidelity **Interactive CS2 Physics & Telemetry Lab**:

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

### 3. 🔬 Comprehensive Simulator Implementations (18 Simulators)
- **Sim 03 (Sub-Tick Prediction)**: 3D jump trajectory with tick boundary division slices, ground friction deceleration vectors, and sub-tick UserCmd click scheduling.
- **Sim 05 (CGameTraceManager)**: Spatial ray tracing, surface penetration normals, optical occlusion, and fixed reactive slider controls.
- **Sim 09 (Skeleton & Two-Bone IK)**: 19-bone articulated humanoid mannequin rig, 13 rotated bounding capsules (`CHitBox`), analytical Law of Cosines IK solver, and quaternion SLERP blending.
- **Sim 11 (Volumetric Smoke)**: 3,500 organic overlapping soft volumetric smoke puffs, $32\times 32\times 32$ Morton Z-order voxel grid, dynamic supersonic bullet cavity carving ($R=32\text{ u}$), HE grenade blast core and shockwave rings ($R=180\text{ u} \to 260\text{ u}$), and optical line-of-sight raymarching.
- **Sim 12 (Lag Compensation & Unlag)**: 64-tick circular history ring buffer ($1.0\text{ s}$ retention), authoritative server unlag rollback (`sv_maxunlag = 0.200s`), 3D Source 2 Tactical CS2 Agent models for live server pawn (Cyan) vs rewound lag-compensated ghost (Amber), and authoritative hit registration raycast.
- **Sim 14 (Bullet Penetration)**: Multi-material 3D slabs (Wood, Concrete, Metal, Cardboard, Glass), physically staggered audio timing (gunfire, wall impact crack, ricochet, flesh/headshot), kinetic dissipation ODE, ricochet gating ($\theta > 65^\circ$), and player armor mitigation jump table.
- **Sim 15 (Econ Item Wear ODE)**: Full 3D Karambit Knife model with procedural skins (Case Hardened, Fade, Asiimov), Runge-Kutta 4th Order wear degradation ODE ($0.00 \to 1.00$), dynamic paint strip shader exposing oxidized steel, and `m_nFallbackSeed` UV pattern randomization.
- **Sim 17 (Steam Audio Acoustics)**: 3D Acoustic Room in `CS2Lab` with procedural Web Audio API binaural HRTF panner, ISO 9613-1 atmospheric air absorption, wall occlusion transmission filter (380 Hz lowpass muffle + -14 dB attenuation), and Sabine/Eyring RT60 reverberation.
- **Sim 21 (Packet Entities Pre-Decode Merger)**: Embedded multi-threaded worker swimlanes canvas directly into the main page.
- **Sim 22 (Particle Compute Engine)**: 3D WebGL GPU particle system running 4,096 active particles with Structure-of-Arrays (SoA) SIMD memory and Valve's 4 compute operators (`COpMovementBasic`, `COpTurbulenceNoise`, `COpWorldCollisionPlane`, `COpFadeAndKill`).

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

## 🗺️ Long-Term Core Roadmap: All Completed Milestones

### Phase 1: Reconstructed C++20 SDK Completeness
- [x] **Master SDK headers** (`client_classes.hpp`, `entity_system.hpp`, `interfaces_registry.hpp`):
  - Clean-room C++20 headers under `game/` mapped to Source 2 modular architecture (`game/client/`, `game/server/`, `game/engine2/`, `game/schemasystem/`, `game/networksystem/`, `game/vphysics2/`, `game/animationsystem/`, `game/particles/`, `game/pulse_system/`).
- [x] **Zero-cost `game_bridge.hpp` integration**:
  - Unified interface adapter connecting SDK types with modular subsystems.
- [x] **Automated SchemaSystem class generator expansion for full network entity coverage**:
  - Enhanced `tools/generate_schema_classes.py` with `--entities` flag discovering and synthesizing all 354+ networked entity classes derived from `CEntityInstance`, `C_BaseEntity`, `CBaseEntity`, `C_BaseModelEntity`, `CBasePlayerPawn`, controllers, weapons, and projectiles.
  - Topological inheritance ordering, C++20 zero-overhead lvalue reference accessors (`SCHEMA_FIELD`), memory padding to exact engine sizeof, and cross-platform nested class symbol sanitization (`::` to `__`).
- [x] **VTable completeness verification across all 34 Source 2 modular PE binaries**:
  - Developed `tools/verify_vtable_completeness.py` validating 100% of the 34 Source 2 modular PE binaries against `manifest.json`.
  - Verified 4,107 exported interface factory tokens across 33 dynamic DLLs.
  - Audited 13,365 VTables and 416,473 virtual method slots in `data/vtables.json` for continuous slot indexing and MSVC x64 ABI slot alignment.
  - Audited C++20 abstract class headers in `game/interfaces/` with pure virtual method parity.

### Phase 2: Binary Update & Differential Automation
- [x] **Automated binary diffing engine (`cs2_patch_diff.py`)**:
  - Full automated PE section comparison, ConVar delta extraction, schema byte-offset shifts, and interface version tracking across game updates.
- [x] **100% unique IDA/Ghidra byte pattern scanner (`scan_signatures.py`)**:
  - High-speed AOB scanner validating that every signature in `data/function_signatures.json` produces exactly 1 match (zero ambiguous, zero broken).
  - `--update-db` mode automatically updates RVAs, VAs, and net delta bytes across patch versions.
- [x] **Automated Ghidra headless progress porting between Steam patch versions**:
  - Headless batch orchestrator in `tools/ghidra/port_signatures_to_ghidra.py` and `tools/ghidra/Source2_PortSignatures.java`.
  - Automatically loads updated binaries into Ghidra under versioned project folders (`/<DATE>_<BUILD_ID>/`), ports function labels, bookmarks, and prototypes via headless analyzer (`analyzeHeadless`).
  - Master reverse-engineering toolkit in `tools/ghidra/cs2_ghidra_toolkit.py` for type injection (`inject-types`), schema injection (`inject-schemas`), prototype synchronization (`sync-prototypes`), RTTI scanning (`scan-rtti`), and cross-reference tracing (`trace-xrefs`).

### Phase 3: Interactive Web Portal & Static API
- [x] **Interactive web documentation with KaTeX math and Mermaid diagrams**:
  - Complete 13-chapter architectural documentation suite in `docs/` with live search, symbol index, and ConVar explorer.
- [x] **Static JSON API endpoints (`docs/api/v1/sdk.json`) for automated AI analysis**:
  - Static JSON data endpoints (`docs/api/v1/`) providing machine-readable schema dictionaries, signature tables, and ConVar listings.
- [x] **Simulators Suite 2.0**:
  - Full audio-visual interactive lab rewrite with Three.js WebGL 3D rendering, Web Audio API procedural sound engine, sub-tick timeline playback, and 18 high-fidelity physical simulators.

---

## 🔄 Continuous Maintenance & Ongoing Simulation Evolution Protocol

As Counter-Strike 2 receives engine updates and internal reversing uncovers further mechanics:
1. **Iterative Simulator Refinement**:
   - As decompilation of internal game functions progresses (e.g. precise floating-point damping curves, sub-tick command aggregation order, or Rubikon narrowphase collision heuristics), update the corresponding C++ simulators in `docs/simulators/` and web providers in `docs/js/cs2lab/` immediately.
2. **Deterministic Parity Checks**:
   - Run the E2E verification suite (`python tests/run_e2e_tests.py`) whenever simulation math or visualizers are updated to maintain 100% test passing rate and zero regression.
3. **Portal Synchronization**:
   - Keep `docs/portal-data.js` and static API endpoints in sync via `python tools/generate_docs_bundle.py; python tools/generate_portal_data.py`.
