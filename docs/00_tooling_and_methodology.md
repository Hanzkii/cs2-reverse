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

The utilities in `tools/` require only standard Python 3.x (no external pip dependencies like `pefile` needed; all PE headers and section RVAs are parsed directly via standard `struct`):

### 3.1 Schema & Type Reflection Tools
- **`tools/extract_schema_classes.py`**:
  - **Purpose**: Parses Source 2's compiled `SchemaClassInfoData_t` table directly from `.rdata` and `.data`.
  - **Mechanism**: Reads PE Section Headers to map file offsets to Virtual Addresses (VA). Locates the registration arrays passed to `ResolveDeclaredClasses`, walks each 32-byte class descriptor, unpacks the class size, field count, base class pointer, and dumps each 32-byte `SchemaClassFieldData_t` (field name string, resolved type pointer, and byte offset).
- **`tools/find_all_schema_chunks.py`**:
  - **Purpose**: Discovers all independent schema registration callbacks across monolithic modules like `client.dll` (which split thousands of classes across multiple registration callbacks).
  - **Mechanism**: Scans `.text` for references to exported `InstallSchemaBindings` registration tables and dumps classes by name.
- **`tools/dump_schemasystem_vtable.py` & `tools/dump_typescope_vtable.py`**:
  - **Purpose**: Dumps the complete virtual method jump tables for `CSchemaSystem` (40 methods) and `CSchemaSystemTypeScope` (35 methods).
  - **Mechanism**: Uses MSVC Complete Object Locator (COL) RTTI references to find the exact vtable address in `.rdata` and prints all 64-bit function pointers.
- **`tools/inspect_controller_pawn.py` & `tools/inspect_server_pawn.py`**:
  - **Purpose**: Queries and compares entity field layouts between client and server (e.g. `CCSPlayerController` vs. `CCSPlayerPawn`).

### 3.2 VTable & RTTI Discovery Tools
- **`tools/find_vtable_rtti.py`**:
  - **Purpose**: Universal MSVC x64 RTTI crawler.
  - **Mechanism**: Locates the mangled `TypeDescriptor` string (e.g. `.?AVCCvar@@` or `.?AVCViewRender@@`), calculates its RVA, searches `.rdata` for the `_s__RTTICompleteObjectLocator` referencing that RVA, and then searches for the vtable that points to that COL at offset `-8`.

### 3.3 ConVar & Network Inspection Tools
- **`tools/dump_cvar_vtable.py`**:
  - **Purpose**: Inspects `tier0.dll`'s `CCvar` (`VEngineCvar007`) 46-method vtable.
- **`tools/dump_net_messages.py`**:
  - **Purpose**: Extracts protobuf descriptor strings and message ID registrations across `NET_Messages`, `CLC_Messages`, and `SVC_Messages`.
- **`tools/dump_fp_ops.py` & `tools/dump_type_table.py`**:
  - **Purpose**: Extracts the 40-operation Huffman bit-tree table and field type serializers from `networksystem.dll`.

---

## 4. Reconstructed SDK (`game/`) & Reference Simulators (`docs/simulators/`)

All reconstructed SDK interfaces and algorithms reside in `game/` (`game/interfaces/` and `game/shared/`). Reference standalone test runners reside in `docs/simulators/`:

### How to Compile & Run Any Simulator
Open a Visual Studio x64 command prompt (or run `vcvars64.bat`):
```cmd
cd docs/simulators
build_smoke_sim.bat
build_rubikon_sim.bat
build_skeleton_anim_sim.bat
build_game_events_sim.bat
build_viewmatrix_sim.bat
build_convar_sim.bat
build_controller_sim.bat
build_schema_sim.bat
build_subtick_sim.bat
build_serializer_sim.bat
build_sim.bat
```
Each batch script compiles the corresponding `.cpp` with `/O2 /std:c++17 /EHsc` and runs the resulting executable, confirming that all assertion tests pass.

---

## 5. Source 2 Binary Ecosystem & Architecture Inventory

Source 2 employs a strictly decoupled, shared-library architecture where engine responsibilities are divided across modular dynamic link libraries (`.dll`). All working copies are synchronized in the local disassembly workspace and indexed in the `CS2.gpr` Ghidra project:

### 5.1 Gameplay & Entity Topology
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`client.dll`** | `game\csgo\bin\win64\` | 39.2 MB | Client-side game logic, local prediction, HUD view rendering, `CSkeletonInstance`, camera setup, aim punch recoil injection, user command sampling. |
| **`server.dll`** | `game\csgo\bin\win64\` | 34.6 MB | Authoritative gameplay simulation, bot execution, lag compensation history buffers, damage calculations, `CCSPlayerController` session management. |
| **`host.dll`** | `game\csgo\bin\win64\` | 1.4 MB | Host tick scheduling, engine initialization loop, game-to-engine handoff bridge. |
| **`matchmaking.dll`** | `game\csgo\bin\win64\` | 1.9 MB | Match state machine, lobby management, game server reservation handshake. |

### 5.2 Core Engine & Reflection Subsystems
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`engine2.dll`** | `game\bin\win64\` | 6.8 MB | Main engine executive: `CGameEventSystem`, frame loop dispatch, console command routing, map change orchestrator, network channel supervisor. |
| **`tier0.dll`** | `game\bin\win64\` | 4.0 MB | Foundational utilities: `CCvar` / `VEngineCvar007` ConVar registry, memory allocators (`g_pMemAlloc`), thread synchronization primitives, logging (`Msg`, `Warning`). |
| **`networksystem.dll`** | `game\bin\win64\` | 2.8 MB | Wire packet framing, Protobuf message serialization/deserialization, 40-op Huffman flattened fieldpath serializer. |
| **`schemasystem.dll`** | `game\bin\win64\` | 0.5 MB | Runtime type reflection (`SchemaSystem_001`), MurmurHash2 type scopes, dynamic class field offset resolution. |

### 5.3 Animation, Physics & Spatial Audio
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`animationsystem.dll`** | `game\bin\win64\` | 9.3 MB | AnimGraph 2 runtime (`CNm` Navier Motion task graph), `CTwoBoneIKPoseOperation` analytical IK solver, `CHitBox` definition tables, blend trees. |
| **`vphysics2.dll`** | `game\bin\win64\` | 4.8 MB | Valve Rubikon physics engine: rigid body simulation, collision mesh acceleration structures (BVH/QBVH), raycast intersection pipelines. |
| **`soundsystem.dll`** | `game\bin\win64\` | 5.8 MB | Sound operator engine (`CSoundOpSystem`), sound event graphs, dynamic audio ducking, voice compression/decompression pipeline. |
| **`steamaudio.dll`** | `game\bin\win64\` | 3.5 MB | Steam Audio spatialization backend: binaural HRTF filtering, sound occlusion rays, physics-based environmental reverb. |

### 5.4 Rendering & Visual Simulation
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`rendersystemvulkan.dll`** | `game\bin\win64\` | 6.2 MB | Vulkan rendering backend: SPIR-V pipeline state objects, memory barrier sync, command buffer submission. |
| **`rendersystemdx11.dll`** | `game\bin\win64\` | 4.5 MB | DirectX 11 rendering backend: HLSL shader binding, constant buffer management, draw call batching. |
| **`scenesystem.dll`** | `game\bin\win64\` | 7.1 MB | High-level scene graph management, render pass orchestration, lighting clusters, shadow map generation. |
| **`worldrenderer.dll`** | `game\bin\win64\` | 2.0 MB | Map geometry clustering, occlusion culling, Potentially Visible Set (PVS) calculations, mesh batching. |
| **`particles.dll`** | `game\bin\win64\` | 6.5 MB | Source 2 particle system: volumetric smoke grenade expansion, bullet tracers, fire, blood splatters. |

### 5.5 Input, Navigation & Low-Level Networking
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`inputsystem.dll`** | `game\bin\win64\` | 1.3 MB | Raw Windows input capture, mouse delta accumulation, sub-tick input timestamping. |
| **`navsystem.dll`** | `game\bin\win64\` | 1.3 MB | CS2 navigation mesh (`.nav`) parsing, bot pathfinding, tactical cover evaluations. |
| **`resourcesystem.dll`** | `game\bin\win64\` | 0.5 MB | KeyValues3 (KV3) resource compiler cache, asset manifest loader, texture/model streaming. |
| **`steamnetworkingsockets.dll`**| `game\bin\win64\` | 4.3 MB | Valve low-level UDP transport layer, MTU fragmentation, Steam Datagram Relay (SDR) encryption. |

### 5.6 Panorama UI Architecture
| Module | Location | Size | Engine Subsystem & Architectural Responsibility |
|:---|:---|:---|:---|
| **`panorama.dll`** | `game\bin\win64\` | 6.1 MB | Valve's GUI runtime: XML/CSS/JS DOM tree parser, JavaScript engine bindings, UI event bus. |
| **`panoramauiclient.dll`** | `game\bin\win64\` | 2.7 MB | Game-specific Panorama glue code: HUD scoreboard, radar, killfeed, inventory, buy menu. |
| **`panorama_text_pango.dll`** | `game\bin\win64\` | 3.0 MB | Pango/FreeType font rasterization, multilingual unicode text shaping, glyph rendering. |

---

## 6. Disassembly Workspace & Module Management Workflow

To maintain clean and reproducible static analysis across reverse engineering tools:
1. **Source Location**: The authoritative game installation is located in the user's Steam library directory (`<STEAM_LIBRARY>\steamapps\common\Counter-Strike Global Offensive\`).
2. **Analysis Directory**: Working copies are synchronized in an isolated disassembly directory (`<DISASSEMBLY_ROOT>\CS2\`).
3. **Reproducibility Rule**: Before analyzing or importing any new module, verify whether it exists in the disassembly workspace. If not, copy the pristine binary from the Steam library folder, verify PE header signatures, and import it into the Ghidra project (`CS2.gpr`) with `auto_analyze: false` for on-demand analysis.

---

## 7. External Reference Frameworks & SDK Integration

To accelerate reverse engineering and disambiguate stripped binary symbols, this project integrates authoritative public reference sources:

### 7.1 AlliedModders Source 2 SDK (`s2sdk`)
- **Repository**: [alliedmodders/s2sdk](https://github.com/alliedmodders/s2sdk)
- **Role in Reversing**: Provides exact C++ interface definitions, virtual method orders, struct alignments, and memory layouts for Valve engine subsystems.
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
The knowledge from both repositories is formalized into an Antigravity custom skill at `.agents/skills/source2-sdk/SKILL.md` (and globally in `~/.gemini/config/skills/source2-sdk/`), equipping automated agents with immediate VTable index lookups, memory layouts, and architectural verification procedures.

