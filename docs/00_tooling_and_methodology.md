# Tooling & Methodology Guide

## 1. Overview & Philosophy

The **cs2-reverse** project is built on a strict, reproducible engineering methodology:
1. **Static Binary Analysis Only**: Zero live game process hooking, zero DLL injection, zero dynamic memory tampering, and zero anti-cheat circumvention. All research is performed against static PE binaries on disk (`client.dll`, `server.dll`, `engine2.dll`, `networksystem.dll`, `schemasystem.dll`, `tier0.dll`, etc.).
2. **Autonomous Agentic Analysis (Antigravity + Ghidra MCP)**: Powered by Google DeepMind's Antigravity agent paired directly with NSA Ghidra via a Model Context Protocol (MCP) server. Disassembly, decompilation, symbol tables, and cross-references across all 34 binaries in `CS2.gpr` are queried in real-time to reverse engineer and verify low-level mechanics.
3. **Ground-Truth Clean-Room Reconstruction**: Reversed algorithms are implemented in pure C++17 under `game/` grouped strictly by binary module, with zero external runtime dependencies.
4. **Modular Automation**: Custom Python utilities automate PE inspection, RTTI extraction, schema parsing, and signature diffing across updates.

---

## 2. Directory Architecture

```
cs2-reverse/
├── game/                      # Clean-room reconstructed C++ SDK grouped by module (header + implementation)
│   ├── client/                # client.dll: ViewMatrix, AngleVectors, Sub-Tick prediction
│   ├── server/                # server.dll: Lag compensation history, Controller <-> Pawn linkage
│   ├── engine2/               # engine2.dll: ConVar engine storage, GameEventSystem dispatcher
│   ├── schemasystem/          # schemasystem.dll: Runtime reflection & MurmurHash2 type scope
│   ├── networksystem/         # networksystem.dll: Bitstream NetMessage framing & Flattened Serializers
│   ├── vphysics2/             # vphysics2.dll: Rubikon collision solver & BVH raycaster
│   ├── animationsystem/       # animationsystem.dll: Two-Bone IK analytical Law of Cosines solver
│   ├── particles/             # particles.dll: 32x32x32 smoke voxel grid, 3D DDA LOS & bullet carve
│   └── pulse_system/          # pulse_system.dll: Pulse graph interpreter & fiber coroutines
├── docs/                      # Technical architecture documentation, Web Portal & Visualizers
│   ├── visualizers/           # Interactive HTML visual simulation engines
│   └── simulators/            # Standalone C++ verification simulation runners
├── tools/                     # Custom Python PE extraction, differential analyzer & signature scanners
├── data/                      # Centralized function signatures registry & baseline RVAs
├── versions/                  # Version manifests, static reflection profiles, diff reports
└── README.md                  # Project overview and reference guide
```

---

## 3. Analysis Tools (`tools/`)

The utilities in `tools/` require only standard Python 3.x (no external pip dependencies needed; all PE headers and section RVAs are parsed directly via standard `struct`):

### 3.1 Schema & Type Reflection Tools
- **`tools/extract_schema_classes.py`**:
  - **Purpose**: Parses Source 2's compiled `SchemaClassInfoData_t` table directly from `.rdata` and `.data`.
  - **Mechanism**: Reads PE Section Headers to map file offsets to Virtual Addresses (VA). Locates the registration arrays passed to `ResolveDeclaredClasses`, walks each 32-byte class descriptor, unpacks the class size, field count, base class pointer, and dumps each 32-byte `SchemaClassFieldData_t` (field name string, resolved type pointer, and byte offset).
- **`tools/find_all_schema_chunks.py`**:
  - **Purpose**: Discovers all independent schema registration callbacks across monolithic modules like `client.dll` (which split thousands of classes across multiple registration callbacks).
  - **Mechanism**: Scans `.text` for references to exported `InstallSchemaBindings` registration tables and dumps classes by name.
- **`tools/dump_schemasystem_vtable.py`**:
  - **Purpose**: Dumps the complete virtual method jump table for `CSchemaSystem` (40 methods).
  - **Mechanism**: Uses MSVC Complete Object Locator (COL) RTTI references to find the exact vtable address in `.rdata` and prints all 64-bit function pointers.
- **`tools/inspect_controller_pawn.py`**:
  - **Purpose**: Queries and compares entity field layouts between client and server (e.g. `CCSPlayerController` vs. `CCSPlayerPawn`).

### 3.2 VTable, Interface & RTTI Discovery Tools
- **`tools/find_vtable_rtti.py`**:
  - **Purpose**: Universal MSVC x64 RTTI crawler.
  - **Mechanism**: Locates the mangled `TypeDescriptor` string (e.g. `.?AVCCvar@@` or `.?AVCViewRender@@`), calculates its RVA, searches `.rdata` for the `_s__RTTICompleteObjectLocator` referencing that RVA, and then searches for the vtable that points to that COL at offset `-8`.
- **`tools/dump_interfaces.py`**:
  - **Purpose**: Scans `CreateInterface` export tables across all core binaries, dumping interface versions and constructor pointers.
- **`tools/dump_cvar_vtable.py` & `tools/find_cvar_flags.py`**:
  - **Purpose**: Inspects `tier0.dll`'s `CCvar` (`VEngineCvar007`) 46-method vtable and extracts 64-bit ConVar flag bitmasks and security gates.

### 3.3 Protobuf, Wire & Network Inspection Tools
- **`tools/dump_net_messages.py`**:
  - **Purpose**: Extracts protobuf descriptor strings and message ID registrations across `NET_Messages`, `CLC_Messages`, and `SVC_Messages`.
- **`tools/dump_game_events_proto.py`**:
  - **Purpose**: Extracts `CSVCMsg_GameEventList` and `CSVCMsgList_GameEvents` protobuf wire descriptor tables.
- **`tools/dump_usercmd_proto.py`**:
  - **Purpose**: Extracts `CSGOUserCmdPB` and `CSubtickMoveStep` protobuf wire schemas and field tags.
- **`tools/dump_setconvar_proto.py`**:
  - **Purpose**: Decodes `CNETMsg_SetConVar` and `CMsg_CVars` network replication protobuf wire schemas.
- **`tools/dump_fp_ops.py`**:
  - **Purpose**: Extracts the 40-operation Huffman bit-tree table and field type serializers from `networksystem.dll`.

### 3.4 Physics, Animation & Simulation Analysis Tools
- **`tools/inspect_vphysics2_rubikon.py`**:
  - **Purpose**: Resolves 80-slot VTables, convex polyhedron half-spaces, and GJK support function addresses for the Rubikon physics engine.
- **`tools/explore_camera_pipeline.py`**:
  - **Purpose**: Disassembles camera setup, FOV interpolation, `AngleVectors`, and 2.0x recoil punch multipliers in `client.dll`.
- **`tools/find_animation_classes.py`**:
  - **Purpose**: Extracts `CSkeletonInstance`, `CTransform` SIMD routines, and Two-Bone IK solvers from `animationsystem.dll`.
- **`tools/find_smoke_classes.py`**:
  - **Purpose**: Analyzes volumetric smoke grenade classes, 32x32x32 voxel grid structures, and 3D Morton-order bit-interleaving.
- **`tools/find_lag_comp.py`**:
  - **Purpose**: Analyzes server-side player lag compensation history buffers, hitbox track records, and sub-tick time rewinds in `server.dll`.

### 3.5 Binary Scanning, Differential Analysis & Automation
- **`tools/scan_signatures.py`**:
  - **Purpose**: High-speed AOB pattern scanner verifying 100% uniqueness (1 match) and tracking RVA shifts across CS2 updates.
- **`tools/cs2_patch_diff.py`**:
  - **Purpose**: Multi-dimensional binary patch diffing engine tracking schema offset shifts, new ConVars, and interface bumps across updates.
- **`tools/fast_scan.cpp` & `tools/run_fast_scan.bat`**:
  - **Purpose**: High-performance C++ multithreaded byte pattern and string scanner using Windows memory-mapped files.
- **`tools/enrich_signatures.py`**:
  - **Purpose**: Enriches canonical function signature records with Ghidra decompiled C++ snippets and explanations.
- **`tools/generate_portal_data.py` & `tools/generate_docs_bundle.py`**:
  - **Purpose**: Bundles architectural documentation, binary profiles, C++ simulators, and the tools catalog into web portal data.

---

## 4. Reconstructed SDK (`game/`) & Reference Simulators (`docs/simulators/`)

All clean-room reconstructed SDK headers and C++17 implementations reside in `game/`, strictly grouped by their corresponding Source 2 binary module (`game/client/`, `game/server/`, `game/engine2/`, `game/schemasystem/`, `game/networksystem/`, `game/vphysics2/`, `game/animationsystem/`, `game/particles/`, and `game/pulse_system/`).

Standalone mathematical and physics verification runners that back the interactive visualizers reside in `docs/simulators/`:

### How to Compile & Run Any Simulator
Open a Visual Studio x64 Native Tools Command Prompt (or run `vcvars64.bat`):
```cmd
cd docs/simulators
build_subtick_sim.bat         :: 03 - Sub-Tick UserCmds & Multi-Slice Prediction (cs2_subtick_usercmd_prediction_sim.cpp)
build_viewmatrix_sim.bat      :: 07 - Camera, View Matrix & Recoil Punch Pipeline (cs2_view_matrix_sim.cpp)
build_skeleton_anim_sim.bat   :: 09 - Skeleton Bone Hierarchy & Two-Bone IK (cs2_skeleton_animation_sim.cpp)
build_rubikon_sim.bat         :: 10 - Rubikon Physics Engine & BVH Raycasting (cs2_rubikon_physics_sim.cpp)
build_smoke_sim.bat           :: 11 - Volumetric Smoke & 32x32x32 Morton Voxel Grid (cs2_volumetric_smoke_sim.cpp)
```
Each batch script compiles the corresponding `.cpp` simulator with `/O2 /std:c++17 /EHsc` and executes the resulting binary, verifying all mathematical invariants and deterministic test assertions.

---

## 5. Source 2 Binary Ecosystem & Architecture Inventory (34 Core Binaries)

Source 2 employs a strictly decoupled, shared-library architecture where engine responsibilities are divided across 34 modular binaries (`.dll` and `.exe`). All 34 modules are archived per build in `versions/archive/<DATE>_<BUILD_ID>/`, profiled in `versions/profiles/`, and indexed in the `CS2.gpr` Ghidra project:

### 5.1 Gameplay & Entity Topology
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`client.dll`** | `game\csgo\bin\win64\` | 39.2 MB | Client-side game logic, local prediction, HUD view rendering, `CSkeletonInstance`, camera setup, aim punch recoil injection, user command sampling. |
| **`server.dll`** | `game\csgo\bin\win64\` | 34.6 MB | Authoritative gameplay simulation, bot execution, lag compensation history buffers, damage calculations, `CCSPlayerController` session management. |
| **`host.dll`** | `game\csgo\bin\win64\` | 1.4 MB | Host tick scheduling, engine initialization loop, game-to-engine handoff bridge. |
| **`matchmaking.dll`** | `game\csgo\bin\win64\` | 1.9 MB | Match state machine, lobby management, game server reservation handshake. |

### 5.2 Core Engine, OS & Reflection Subsystems
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`cs2.exe`** | `game\bin\win64\` | 2.8 MB | Main PE launcher executable: process bootstrap, `g_pMemAlloc` initialization, command-line dispatch, `Source2Main` handoff. |
| **`engine2.dll`** | `game\bin\win64\` | 6.8 MB | Main engine executive: `CGameEventSystem`, frame loop dispatch, console command routing, map change orchestrator, network channel supervisor. |
| **`tier0.dll`** | `game\bin\win64\` | 4.0 MB | Foundational utilities: `CCvar` / `VEngineCvar007` ConVar registry, memory allocators (`g_pMemAlloc`), thread synchronization primitives, logging (`Msg`, `Warning`). |
| **`networksystem.dll`** | `game\bin\win64\` | 2.8 MB | Wire packet framing, Protobuf message serialization/deserialization, 40-op Huffman flattened fieldpath serializer. |
| **`schemasystem.dll`** | `game\bin\win64\` | 0.5 MB | Runtime type reflection (`SchemaSystem_001`), MurmurHash2 type scopes, dynamic class field offset resolution. |
| **`filesystem_stdio.dll`** | `game\bin\win64\` | 2.2 MB | VPK file system (`VFileSystem017`), LZ4 pak decompression, asset search paths, `sv_pure` integrity verification. |
| **`vconcomm.dll`** | `game\bin\win64\` | 0.3 MB | VConsole2 developer diagnostic communication socket (`VConComm001`) and remote logging channel. |

### 5.3 Scripting & Graph Execution Subsystems
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`pulse_system.dll`** | `game\bin\win64\` | 1.9 MB | Source 2 Pulse visual node graph interpreter (`IPulseSystem_001`), 56-byte `PGDInstruction_t` bytecode VM, coroutine execution cursors. |
| **`vscript.dll`** | `game\bin\win64\` | 1.3 MB | Scripting language bridge (`VScript_001` / `IVScriptManager`) for workshop maps and custom gamemode logic. |

### 5.4 Animation, Physics & Spatial Audio
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`animationsystem.dll`** | `game\bin\win64\` | 9.3 MB | AnimGraph 2 runtime (`CNm` Navier Motion task graph), `CTwoBoneIKPoseOperation` analytical IK solver, `CHitBox` definition tables, blend trees. |
| **`vphysics2.dll`** | `game\bin\win64\` | 4.8 MB | Valve Rubikon physics engine: rigid body simulation, collision mesh acceleration structures (BVH/QBVH), raycast intersection pipelines. |
| **`soundsystem.dll`** | `game\bin\win64\` | 5.8 MB | Sound operator engine (`CSoundOpSystem`), sound event graphs, dynamic audio ducking, voice compression/decompression pipeline. |
| **`steamaudio.dll`** | `game\bin\win64\` | 3.5 MB | Steam Audio spatialization backend: binaural HRTF filtering, sound occlusion rays, physics-based environmental reverb. |
| **`scenefilecache.dll`** | `game\bin\win64\` | 1.2 MB | Choreography scene cache (`SceneFileCache002`), audio cue timing, and synchronized facial gesture data (`.vcd_c`). |

### 5.5 Rendering, Materials & Visual Simulation
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`rendersystemvulkan.dll`** | `game\bin\win64\` | 6.2 MB | Vulkan rendering backend: SPIR-V pipeline state objects, memory barrier sync, command buffer submission. |
| **`rendersystemdx11.dll`** | `game\bin\win64\` | 4.5 MB | DirectX 11 rendering backend: HLSL shader binding, constant buffer management, draw call batching. |
| **`scenesystem.dll`** | `game\bin\win64\` | 7.1 MB | High-level scene graph management, render pass orchestration, lighting clusters, shadow map generation. |
| **`worldrenderer.dll`** | `game\bin\win64\` | 2.0 MB | Map geometry clustering, occlusion culling, Potentially Visible Set (PVS) calculations, mesh batching. |
| **`particles.dll`** | `game\bin\win64\` | 6.5 MB | Source 2 particle system: volumetric smoke grenade expansion, bullet tracers, fire, blood splatters. |
| **`meshsystem.dll`** | `game\bin\win64\` | 1.6 MB | 3D geometry streaming (`MeshSystem001`), vertex skinning buffers, morph targets (`CMorphSetData`), flex controllers. |
| **`materialsystem2.dll`** | `game\bin\win64\` | 1.4 MB | Material pipeline (`VMaterialSystem2_001`), compiled `.vmat_c` shaders, HDR tonemapping & post-processing parameters. |

### 5.6 Input, Navigation & Low-Level Networking
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`inputsystem.dll`** | `game\bin\win64\` | 1.3 MB | Raw Windows input capture, mouse delta accumulation, sub-tick input timestamping. |
| **`navsystem.dll`** | `game\bin\win64\` | 1.3 MB | CS2 navigation mesh (`.nav`) parsing, bot pathfinding, tactical cover evaluations. |
| **`resourcesystem.dll`** | `game\bin\win64\` | 0.5 MB | KeyValues3 (KV3) resource compiler cache, asset manifest loader, texture/model streaming. |
| **`steamnetworkingsockets.dll`**| `game\bin\win64\` | 4.3 MB | Valve low-level UDP transport layer, MTU fragmentation, Steam Datagram Relay (SDR) encryption. |

### 5.7 Panorama UI & Localization Architecture
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`panorama.dll`** | `game\bin\win64\` | 6.1 MB | Valve's GUI runtime: XML/CSS/JS DOM tree parser, JavaScript engine bindings, UI event bus. |
| **`panoramauiclient.dll`** | `game\bin\win64\` | 2.7 MB | Game-specific Panorama glue code: HUD scoreboard, radar, killfeed, inventory, buy menu. |
| **`panorama_text_pango.dll`** | `game\bin\win64\` | 3.0 MB | Pango/FreeType font rasterization, multilingual unicode text shaping, glyph rendering. |
| **`localize.dll`** | `game\bin\win64\` | 0.4 MB | String table localization (`Localize_001`), translation token lookup (`#SFUI_...`, `#CSGO_...`), Unicode formatting. |
| **`v8system.dll`** | `game\bin\win64\` | 0.2 MB | Google V8 JavaScript engine binding (`Source2V8System001`) powering Panorama UI panel scripts and event callbacks. |

---

## 6. Version Archiving, Signature Tracking & Disassembly Workflow

To maintain reproducible static analysis across Counter-Strike 2 updates without committing proprietary binaries to Git:
1. **Local Binary Archiving (`versions/archive/<DATE>_<BUILD_ID>/`)**: Pristine game binaries from `<STEAM_LIBRARY>\steamapps\common\Counter-Strike Global Offensive\` are snapshotted into local-only archive directories (strictly excluded via `.gitignore`) and imported into the local `CS2.gpr` Ghidra project.
2. **Version Telemetry Profiles (`versions/profiles/<DATE>_<BUILD_ID>/`)**: `tools/cs2_patch_diff.py` extracts lightweight JSON metadata (`manifest.json`, `schemas.json`, `convars.json`, `interfaces.json`) for Git tracking and automated patch diffing (`versions/diffs/`).
3. **Canonical Function Signature Registry (`data/function_signatures.json`)**: Every reversed internal routine, VTable, and dispatcher is registered with a 100% unique IDA/Ghidra wildcard byte pattern and verified against archived builds via `tools/scan_signatures.py`.

---

## 7. External Reference Frameworks & SDK Integration

To accelerate reverse engineering and disambiguate stripped binary symbols, this project cross-references community and developer documentation:

### 7.1 AlliedModders Source 2 SDK (`s2sdk`)
- **Repository**: [alliedmodders/s2sdk](https://github.com/alliedmodders/s2sdk) (Unofficial, community-maintained Source 2 SDK)
- **Role in Reversing**: Provides community-reconstructed C++ interface definitions, virtual method table orders, struct alignments, and memory layouts for Valve engine subsystems.
- **Key Headers Leveraged**:
  - `public/tier0/`: Platform allocators (`IMemAlloc`), `CUtlVector`, `CUtlStringToken` (MurmurHash2 / FNV-1a).
  - `public/engine2/`: `IEngineServiceMgr`, `INetworkServerService`, `INetworkClientService`, `IGameEventSystem`.
  - `public/entity2/`: `CEntityIdentity`, `CEntityInstance`, `CEntityHandle` bitfield masks (`0x7FFF` index, 17-bit serial).
  - `public/schemasystem/`: `CSchemaSystem`, `CSchemaSystemTypeScope`, `SchemaClassInfoData_t`, `SchemaClassFieldData_t`.
  - `public/vphysics2/`: `IVPhysics2`, `IVPhysics2World`, `CRnShape`, `RnRay_t` Rubikon collision queries.
  - `public/networksystem/`: Wire framing formats and Protobuf NetMessage dispatchers.

### 7.2 Valve Developer Community (VDC) Documentation
- **Reference**: [Valve Developer Community: Source 2](https://developer.valvesoftware.com/wiki/Source_2)
- **Role in Reversing**: Clarifies engine architectural boundaries, sub-tick input accumulation, Rubikon physics solver differences versus legacy Havok, Panorama UI JavaScript DOM abstractions, Sound Operator System (SOS) graphs, and compiled asset structures (`.vpk` and `*_c` formats).

### 7.3 Custom Antigravity Skill (`source2-sdk`)
The architectural patterns and verification procedures are formalized into the repository's custom skill at `.agents/skills/source2-sdk/SKILL.md`, equipping automated agents with immediate VTable index lookups, handle bitmask decoding, and SchemaSystem reflection rules.

