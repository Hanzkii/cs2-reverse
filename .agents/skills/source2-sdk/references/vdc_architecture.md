# Valve Developer Community (VDC) Source 2 Engine Architecture

This document summarizes the official Valve Developer Community (VDC) documentation and architectural principles of the **Source 2** engine.

---

## 1. Engine Core Philosophy

Source 2 was engineered from the ground up to address the technical debt of the original Source 1 engine (which evolved from GoldSrc / Quake):
1. **64-bit Native Only**: Completely drops 32-bit pointers and memory limits. All game pointers are 64-bit ($8 \text{ bytes}$).
2. **Unified Data-Driven Schema System**: Replaces hardcoded `DT_` networking tables with runtime reflection (`SchemaSystem`), allowing hot reloading and seamless serialization.
3. **Multi-Threaded Job System**: Work is broken into asynchronous fiber/job tasks processed across all available CPU cores rather than a single dominant main thread.
4. **Vulkan & DirectX 11 PBR Pipeline**: Replaces the old fixed-function / DX9 shader system with physically-based rendering (PBR) and compute shaders.

---

## 2. Key Engine Subsystems

```
+-------------------------------------------------------------+
|                         client.dll                          |
|  (Panorama UI, Client Prediction, Animation Graph, Audio)  |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                         engine2.dll                         |
|   (Job System, Host State, Network Channels, Service Mgr)   |
+-------------------------------------------------------------+
       |                      |                      |
       v                      v                      v
+--------------+      +---------------+      +----------------+
| schemasystem |      |  vphysics2    |      | networksystem  |
| (Reflection) |      | (Rubikon Sim) |      |  (Wire I/O)    |
+--------------+      +---------------+      +----------------+
```

### A. Sub-Tick Input & Movement System
- **The Problem in Source 1**: Commands were processed in discrete tick batches (e.g. 64 or 128 ticks per second). If a player clicked between tick 100 and tick 101, the action was held until tick 101, causing up to 15.6 ms of input latency jitter.
- **Source 2 Solution**: Input commands (`CSGOUserCmdPB`) attach precise sub-tick timestamps (`subtick_fraction` between `0.0` and `1.0`). The server reconstructs the exact microsecond when a shot was fired or jump was initiated, performing precise sub-tick collision raycasts.

### B. Rubikon Physics (`vphysics2.dll`)
- Replaces Havok.
- In-house Valve physics solver optimized for stability at varying tick rates.
- Supports rigid body ragdolls, vehicle mechanics, destructible meshes, and sub-tick continuous collision detection (CCD).

### C. Panorama UI (`panorama.dll`)
- Modern declarative UI framework replacing legacy VGUI (`vgui2.dll`).
- Uses web standards: HTML-like XML for structure, CSS3 for styling and animations, and JavaScript for client-side UI logic.
- Interfaces directly with C++ game code through registered C++ Panorama abstractions and schema event listeners.

### D. Asset Compilation & Packaging (`resourcecompiler.exe`)
- Source assets (text files, FBX models, PNG/TGA textures) are compiled into runtime `*_c` files:
  - Textures: `.vtex_c` (compressed into BCn/ASTC texture formats with mipmaps).
  - Models: `.vmdl_c` (skeleton, mesh LODs, animation sequences).
  - Materials: `.vmat_c` (PBR shaders, roughness/metalness parameters).
  - Maps: `.vmap_c` (compiled tile meshes and baked visibility geometry).
- Packaging: Stored inside `.vpk` archive files (Valve PacKage).

### E. Sound Operator System (SOS - `soundsystem.dll`)
- Eliminates hardcoded sound scripts.
- Sound scripts define an operator graph: an audio graph of interconnected processing nodes (operators) controlling volume falloff, random pitch modulation, DSP environmental reverb, occlusion, and 3D spatialization.
