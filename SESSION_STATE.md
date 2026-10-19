# Counter-Strike 2 (Source 2) Reverse Engineering - Session State

> **Last Updated**: 2026-10-19 20:10 UTC  
> **Repository**: [https://github.com/Hanzkii/cs2-reverse.git](https://github.com/Hanzkii/cs2-reverse.git)  
> **GitHub Pages**: [https://hanzkii.github.io/cs2-reverse/](https://hanzkii.github.io/cs2-reverse/)  
> **Current Focus**: Fixed Mermaid v10 diagram rendering errors across docs, launched dedicated "Analysis Tools" tab profiling 13 custom reverse-engineering utilities with copyable CLI syntax and terminal output breakdowns!

---

## 1. Executive Summary & Active Status

This repository contains reverse-engineered technical documentation, binary memory layouts, disassembly analysis, and verified standalone C++ reference simulators for Valve's Source 2 engine (Counter-Strike 2).

All analyses are conducted via static binary reverse engineering in Ghidra and custom PE binary extractors. Standalone simulators are verified with MSVC x64 (`vcvars64.bat`) without live process hooks, DLL injection, or dynamic memory modification.

---

## 2. Completed Milestones

| # | Subsystem | Target Binaries | Documentation | Verification Simulator | Status |
|---|---|---|---|---|---|
| **00** | **Tooling & Methodology Guide** | Repository-wide | [`docs/00_tooling_and_methodology.md`](docs/00_tooling_and_methodology.md) | Standard Python 3 & MSVC C++ | ✅ Documented |
| **01** | **Network Framing & Protobuf Serialization** | `networksystem.dll` | [`docs/01_network_framing.md`](docs/01_network_framing.md) | [`simulators/cs2_netmessage_framing_sim.cpp`](simulators/cs2_netmessage_framing_sim.cpp) | ✅ Verified |
| **02** | **Flattened Serializers & Huffman FieldPaths** | `networksystem.dll` | [`docs/02_flattened_serializers.md`](docs/02_flattened_serializers.md) | [`simulators/cs2_flattened_serializer_sim.cpp`](simulators/cs2_flattened_serializer_sim.cpp) | ✅ Verified |
| **03** | **Sub-Tick UserCmd & Multi-Slice Prediction** | `client.dll` | [`docs/03_subtick_usercmd_and_prediction.md`](docs/03_subtick_usercmd_and_prediction.md) | [`simulators/cs2_subtick_usercmd_prediction_sim.cpp`](simulators/cs2_subtick_usercmd_prediction_sim.cpp) | ✅ Verified |
| **04** | **Schema System & Dynamic Runtime Reflection** | `schemasystem.dll`, `client.dll` | [`docs/04_schemasystem_runtime_reflection.md`](docs/04_schemasystem_runtime_reflection.md) | [`simulators/cs2_schemasystem_reflection_sim.cpp`](simulators/cs2_schemasystem_reflection_sim.cpp) | ✅ Verified |
| **05** | **Player Controller vs. Pawn Architecture** | `client.dll`, `server.dll` | [`docs/05_controller_vs_pawn_architecture.md`](docs/05_controller_vs_pawn_architecture.md) | [`simulators/cs2_controller_pawn_lifecycle_sim.cpp`](simulators/cs2_controller_pawn_lifecycle_sim.cpp) | ✅ Verified |
| **06** | **ConVar Storage, Flags & Network Replication** | `tier0.dll`, `engine2.dll` | [`docs/06_convar_storage_and_replication.md`](docs/06_convar_storage_and_replication.md) | [`simulators/cs2_convar_replication_sim.cpp`](simulators/cs2_convar_replication_sim.cpp) | ✅ Verified |
| **07** | **Camera, View Matrix & Recoil Punch Pipeline** | `client.dll` | [`docs/07_camera_and_view_matrix_pipeline.md`](docs/07_camera_and_view_matrix_pipeline.md) | [`simulators/cs2_view_matrix_sim.cpp`](simulators/cs2_view_matrix_sim.cpp) | ✅ Verified |
| **08** | **Source 2 Game Event System & Protobuf Dispatch** | `engine2.dll`, `server.dll`, `networksystem.dll` | [`docs/08_game_events_pipeline.md`](docs/08_game_events_pipeline.md) | [`simulators/cs2_game_events_sim.cpp`](simulators/cs2_game_events_sim.cpp) | ✅ Verified |
| **09** | **Skeleton Bone Hierarchy & AnimGraph IK Pipeline** | `client.dll`, `animationsystem.dll` | [`docs/09_skeleton_bone_hierarchy_and_animgraph.md`](docs/09_skeleton_bone_hierarchy_and_animgraph.md) | [`simulators/cs2_skeleton_animation_sim.cpp`](simulators/cs2_skeleton_animation_sim.cpp) | ✅ Verified |
| **10** | **Rubikon Physics Engine & Collision Detection** | `vphysics2.dll` | [`docs/10_vphysics2_rubikon_collision_engine.md`](docs/10_vphysics2_rubikon_collision_engine.md) | [`simulators/cs2_rubikon_physics_sim.cpp`](simulators/cs2_rubikon_physics_sim.cpp) | ✅ Verified |
| **11** | **Volumetric Smoke & 3D Voxel Grid Simulation** | `server.dll`, `client.dll`, `particles.dll` | [`docs/11_volumetric_smoke_and_3d_voxel_grids.md`](docs/11_volumetric_smoke_and_3d_voxel_grids.md) | [`simulators/cs2_volumetric_smoke_sim.cpp`](simulators/cs2_volumetric_smoke_sim.cpp) | ✅ Verified |
| **12** | **Automated Binary Patch Diffing & Archiving** | 34 Engine Binaries | [`docs/12_binary_patch_diffing_and_version_archiving.md`](docs/12_binary_patch_diffing_and_version_archiving.md) | [`tools/cs2_patch_diff.py`](tools/cs2_patch_diff.py) | ✅ Verified |
| **13** | **Pulse Node Engine & Bytecode Virtual Machine** | `pulse_system.dll` | [`docs/13_pulse_system_visual_scripting_engine.md`](docs/13_pulse_system_visual_scripting_engine.md) | [`simulators/cs2_pulse_vm_sim.cpp`](simulators/cs2_pulse_vm_sim.cpp) | ✅ Verified |

---

## 3. Function Signatures & RVA Shift Tracking (100% Unique)

- **Centralized Registry**: [`data/function_signatures.json`](data/function_signatures.json) (21 canonical IDA/Ghidra byte patterns).
- **Scanner & Diffing CLI**: [`tools/scan_signatures.py`](tools/scan_signatures.py) (`--dir`, `--dll`, `--diff`).
- **Telemetry Parity**: 21 of 21 signatures verified unique (1 match, 0 ambiguous, 0 broken) across the 34-binary baseline archive.
- **Automated Update Diffing**: Fully integrated into `tools/cs2_patch_diff.py` to flag shifted RVAs (+/- bytes) and broken signatures.

---

## 4. Key Binary Addresses & Signatures

### `client.dll` (Base: `0x180000000`)
- **`CSkeletonInstance` VTable**: `0x181c41ce0` (35 virtual methods)
- **`CalculateWorldBones`**: `0x180aa0f80` (`FUN_180aa0f80`)
- **`CTransform::Concatenate`**: `0x181748480` (`FUN_181748480`)
- **Skeleton Offsets**:
  - `m_nodeToWorld`: `+0x0010` (Root `CTransform`, 32 bytes)
  - `m_modelState`: `+0x0140` (`CModelState`, 688 bytes)
  - `m_pBoneArray`: `+0x01c0` (`m_modelState + 0x80`, pointer to `CTransform[N]`)
  - `m_nBoneCount`: `+0x01d4` (`m_modelState + 0x94`, `uint16_t`)
  - `m_atomicMutex`: `+0x0114` (`CAtomicMutex`)
  - `m_nCalculatedBoneFlags`: `+0x0458` (32-bit dirty/evaluation bitmask)
- **`CViewRender` VTable**: `0x181c7d1f8`
- **`CViewRender::SetUpView`**: `0x180c147b0` (VTable Index 4)
- **`AngleVectors` / `BuildViewMatrix`**: `0x1817402a0` ($\text{rad} = \text{deg} \times 0.017453292$)
- **Global `VMatrix` Buffer**: `0x18256ad00` (16 consecutive floats, $4 \times 4$ projection)

### `server.dll` (Base: `0x180000000`)
- **`CSmokeGrenadeProjectile` VTable**: `0x1818e0e08` (COL: `0x181b615c8`, RTTI TypeDescriptor: `0x181ed9178`)
- **`CSmokeGrenadeProjectile::Spawn`**: `0x1809cb810` (VTable Slot 9, initializes 3,072-byte buffer and grenade model)
- **`CSmokeGrenadeProjectile::TickUpdate`**: `0x1809cea90` (Checks position delta, generates voxel frame updates)
- **`LineOfSightThroughSmoke`**: `0x1809b7f50` (Authoritative vision test checking active smokes against `0.20f` density gate)
- **`VoxelGrid_Raymarch`**: `0x1809cd500` (3D Digital Differential Analyzer DDA raymarcher over $640 \times 640 \times 640$ volume)
- **`VoxelGrid_ExpandAndCarve`**: `0x1809c8e50` (3D Morton Order bit-interleaving, bullet tunnel clearing, and $+30.0$ unit blast displacement)
- **`SerializeSparseDelta`**: `0x1809b1bd0` (10-byte sparse word + bitmask network serializer)
- **Global Active Smoke Head**: `0x181fa12b8` (`DAT_181fa12b8`, linked via `+0x0b60`)

### `particles.dll` (Base: `0x180000000`)
- **`C_OP_RenderVolumetricEmitter` Class Info**: `0x1805c4b60` (Size: 8,848 bytes, 15 fields)
- **Volumetric Emitter Radius**: `m_flRadius` (`+0x1848`)
- **Volumetric Density Multiplier**: `m_flDensity` (`+0x19c0`)
- **Volumetric Kill Radius**: `m_flKillRadius` (`+0x1e28`)

### `animationsystem.dll` (Base: `0x180000000`)
- **`CTwoBoneIKPoseOperation` VTable**: `0x180689268`
- **`SolveTwoBoneIK`**: `0x1802f9750` (`FUN_1802f9750`, Law of Cosines $\arccos$ analytical solver)
- **`CHitBox` Class Info**: `0x18081d840` (Size: 112 bytes)
- **`CHitBoxSet` Class Info**: `0x18081d940` (Size: 48 bytes)

### `engine2.dll` (Base: `0x180000000`)
- **`CGameEventSystem` VTable**: `0x18058aea8`
- **`IGameEventManager2` String Identifier**: `"GAMEEVENTSMANAGER002"`
- **`CGameEvent` VTable**: `0x18058a970`
- **`net_SetConVar` Receiver**: `0x1800825e0` (`FUN_1800825e0`)
- **Replication Validation**: Checks `(pCvar->m_nFlags >> 13) & 1 == 1`
- **Camera & Recoil Schema Offsets**:
  - `CGameSceneNode::m_vecAbsOrigin`: `+0x00c8`
  - `C_BasePlayerPawn::m_vecViewOffset`: `+0x0f60` (Eye height: $Z = 64.0$)
  - `C_CSPlayerPawn::m_pAimPunchServices`: `+0x1598`
  - `CCSPlayer_AimPunchServices::m_predictableBaseAngle`: `+0x0050`
  - `CCSPlayer_AimPunchServices::m_predictableBaseAngleVel`: `+0x005c`
  - `C_CSPlayerPawn::m_angEyeAngles`: `+0x3600`
  - `C_CSPlayerPawn::m_flViewmodelFOV`: `+0x1dc8`

### `tier0.dll` (Base: `0x180000000`)
- **`VEngineCvar007` / `CCvar` VTable**: `0x180313208`
- **`CCvar::FindVar`**: `0x18006c6b0` (Hash seed: `0x31415926`)
- **Flags**: `FCVAR_REPLICATED` = Bit 13 (`0x2000`), `FCVAR_CHEAT` = Bit 14 (`0x4000`)

### `networksystem.dll` (Base: `0x180000000`)
- **`CSVCMsg_GameEventList` (ID `0x1E` / 30)**: Phase 1 signon dictionary syncing descriptors & typed key keys.
- **`CSVCMsgList_GameEvents` (ID `0x22` / 34)**: Phase 2 in-match per-tick event array.
- **`CSVCMsg_GameEvent`**: Compact key-value payload carrying numerical descriptor indexes (zero property name strings on wire).

### `schemasystem.dll` (Base: `0x180000000`)
- **`CreateInterface`**: `0x180037560` (`"SchemaSystem_001"`)
- **MurmurHash2 Algorithm**: `0x180010380` (`seed = len ^ 0xbaadfeed`, `m = 0x5bd1e995`)

### `vphysics2.dll` (Base: `0x180000000`)
- **`CreateInterface`**: `0x180350120` (`"VPhysics2_Interface_001"`, Singleton: `0x180460e60`)
- **`CVPhysics2Interface` VTable**: `0x1803fbf50` (80 virtual methods)
- **`CVPhys2World` VTable**: `0x1803f9b20` (80 virtual methods)
- **`CFnPhysicsWorldImpl` VTable**: `0x1803fa798` (38 virtual methods)
- **`CFnPhysicsBodyImpl` VTable**: `0x1803fa610` (48 virtual methods)
- **`CRnShape` Base VTable**: `0x1803e1268` (33 virtual methods)
- **`CRnHullShape` VTable**: `0x1803e1140` (33 virtual methods)
- **`CRnCapsuleShape` VTable**: `0x1803e1030` (33 virtual methods)
- **Convex Polyhedron Raycast**: `0x1802544a0` (Slab-clipping half-space test with $1/32$" skin tolerance)
- **Capsule Raycast / Sweep**: `0x18016caf0` (Analytical finite cylinder and hemispherical endcap solver)
- **GJK Support Functions**:
  - `CTraceAABB::GetSupportPoint`: `0x1802ea210` (Sign-bit axis argmax)
  - `CTraceCapsule::GetSupportPoint`: `0x18006d070` (Max endpoint projection + radius)
  - `CTraceHull::GetSupportPoint`: `0x1802ea5d0` (SIMD 4-wide unrolled vertex projection argmax)
- **`RnCompoundTree_t` BVH Traversal**: `0x18000f4e0` (48-byte SIMD node hierarchical query)
- **`CWorldStepJob` VTable**: `0x1803fa418` (7 virtual methods, ThreadPool step task)

---

## 4. Upcoming Subsystems for Exploration

1. **Spatial Audio & Voice Packet Pipeline** (`soundsystem.dll`, `steamaudio.dll`, `engine2.dll`):
   - HRTF spatialization, Steam Voice / Opus codec audio streaming, sound operator stacks (`CSoundOpSystem`).
2. **Smoke Grenade Volumetric Grids & Particle Simulation** (`particles.dll`):
   - Source 2 volumetric smoke voxel grids, expansion simulation, bullet carve-out mechanics, and particle emitter operators.
3. **Vulkan / DX11 Render Submission Pipeline** (`rendersystemvulkan.dll`, `rendersystemdx11.dll`):
   - Draw call batching, compute shader dispatch, descriptor pool allocations, constant buffer binding.
4. **Panorama UI & JavaScript V8 Binding Engine** (`panorama.dll`, `panoramauiclient.dll`):
   - Panel hierarchy, style cascading, event dispatching, and C++ to V8 JS native bridges.

---

## 5. Development & Environment Setup

- **Platform**: Windows 11 x64
- **Original Game Binaries**: `<CS2_ROOT>/game/bin/win64/` and `<CS2_ROOT>/game/csgo/bin/win64/`
- **Analysis Working Copies**: Disassembly workspace (Ghidra Project: `CS2.gpr`)
- **Ghidra Project Binary Inventory** (All synchronized and imported):
  - `client.dll`, `server.dll`, `host.dll`, `matchmaking.dll`
  - `engine2.dll`, `tier0.dll`, `networksystem.dll`, `schemasystem.dll`
  - `animationsystem.dll`, `soundsystem.dll`, `steamaudio.dll`
  - `vphysics2.dll`, `rendersystemvulkan.dll`, `rendersystemdx11.dll`
  - `particles.dll`, `inputsystem.dll`, `navsystem.dll`, `worldrenderer.dll`
  - `resourcesystem.dll`, `scenesystem.dll`, `steamnetworkingsockets.dll`
  - `panorama.dll`, `panoramauiclient.dll`, `panorama_text_pango.dll`
- **Workflow Rule**: Always copy new binaries from the Steam library folder to the local working directory before analyzing or importing into Ghidra.
- **Disassembly Engine**: Ghidra MCP Server (Project: `CS2`)
- **C++ Compiler**: Microsoft Visual C++ 2026 (MSVC 19.50 x64) via `vcvars64.bat`
- **Git Remote**: `origin` -> `https://github.com/Hanzkii/cs2-reverse.git` (branch `main`)

---

## 6. Antigravity Custom Skill: `source2-sdk`

To dramatically speed up reverse engineering and disambiguation of stripped Source 2 binaries, this project maintains a formal Antigravity Custom Skill:
- **Skill Path (Project)**: `.agents/skills/source2-sdk/SKILL.md`
- **Skill Path (Global)**: `~/.gemini/config/skills/source2-sdk/SKILL.md`
- **Reference Files**:
  - `.agents/skills/source2-sdk/references/s2sdk_headers.md`: VTable slots, `tier0`, `engine2`, `entity2`, `schemasystem`, `vphysics2` memory layouts.
  - `.agents/skills/source2-sdk/references/vdc_architecture.md`: Source 2 engine subsystems, sub-tick input sampling, Rubikon vs. Havok, SOS sound operators, Panorama UI architecture.
- **Capabilities**:
  - Instant resolution of stripped VTable function pointers in Ghidra.
  - Direct translation of `CEntityHandle` bitfield masks (`0x7FFF` index, 17-bit serial).
  - Runtime `SchemaSystem` structure navigation and field offset lookups.
  - Identification of MurmurHash2 / FNV-1a string token hashing (`CUtlStringToken`).


