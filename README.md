# Counter-Strike 2 (Source 2) Reverse Engineering & Architecture Reference

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey.svg)](#)
[![Source 2](https://img.shields.io/badge/engine-Valve%20Source%202-orange.svg)](#)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-brightgreen.svg)](https://hanzkii.github.io/cs2-reverse/)

> 🌐 **Interactive Documentation & Visualizers Portal**: [**https://hanzkii.github.io/cs2-reverse/**](https://hanzkii.github.io/cs2-reverse/)  
> Features live interactive Sub-Tick lag compensation visualizers, Two-Bone Inverse Kinematics solvers, 34-binary module catalog with reversal statuses, canonical function signature registry, and full architecture reports with KaTeX math and Mermaid diagrams.

A comprehensive, ground-truth technical reference on Valve's **Source 2** engine as implemented in **Counter-Strike 2**.

This project documents low-level engine architecture, binary memory layouts, wire serialization formats, and mathematical simulation models reconstructed through static binary reverse engineering of `client.dll`, `server.dll`, `networksystem.dll`, `schemasystem.dll`, `tier0.dll`, `engine2.dll`, `vphysics2.dll`, `animationsystem.dll`, `particles.dll`, and `pulse_system.dll`.

---

## 🤖 Engineering Environment: Antigravity + Ghidra MCP

This repository is developed using an autonomous pair-programming pipeline combining **Google DeepMind's Antigravity** with **NSA Ghidra** through the **Model Context Protocol (MCP)**:

- **Google Antigravity Agentic Workflow**:
  - Autonomous reasoning, static decompilation tracing, mathematical modeling, and documentation bundling.
  - Custom Source 2 skill ([`.agents/skills/source2-sdk/`](.agents/skills/source2-sdk/)) providing ground-truth memory structures, AlliedModders `s2sdk` virtual method tables, and Valve Developer Community (VDC) architecture references.
- **NSA Ghidra + Ghidra MCP Server Bridge**:
  - Centralized Ghidra project database (`CS2.gpr`) synchronizing disassembly, symbol tables, and data types across all 34 Counter-Strike 2 binaries.
  - High-throughput bi-directional MCP server allowing the AI agent to:
    - Query symbols, decompile functions, trace cross-references (`xrefs`), and inspect control-flow graphs directly on demand.
    - Analyze virtual method tables (VTables) and reconstruct clean-room C++ implementations without manual copy-paste bottlenecks.
    - Extract, pad, and verify canonical IDA/Ghidra byte signatures (AOB) against baseline binaries.

---

## 🔍 Reverse Engineering Methodology

Our reverse-engineering approach is strictly **static binary analysis** and **clean-room reconstruction**:

1. **Static Analysis Only / Zero Injection**:
   - Zero dynamic DLL injection, live game process hooking, or runtime memory manipulation.
   - All findings are derived through static decompilation in Ghidra, PE structure inspection, and static reflection extraction.

2. **Interface & VTable Disambiguation**:
   - Stripped virtual function tables (VTables) are cross-referenced against AlliedModders `s2sdk` virtual method table orders (`tier0`, `engine2`, `entity2`, `schemasystem`, `vphysics2`).
   - Interface factories exported via `CreateInterface` are located and traversed in `.rdata` to map master interfaces (`Source2Client002`, `Source2ServerConfig001`, `SchemaSystem_001`, `VPhysics2_Interface_001`, etc.).

3. **Dynamic Reflection via SchemaSystem**:
   - Source 2 replaces legacy compile-time RTTI with runtime reflection (`schemasystem.dll`).
   - Field names, byte offsets, and inheritance hierarchies are resolved by analyzing `CSchemaSystem` type scopes and 32-bit MurmurHash2 token lookup tables.

4. **Canonical Pattern Signatures & Automated RVA Shift Diffing**:
   - Every reversed internal routine is cataloged in `data/function_signatures.json` with an unambiguous, discriminator-padded IDA/Ghidra byte pattern (AOB).
   - Our automated scanner verifies that every pattern produces exactly 1 match across the baseline binaries and alerts to RVA shifts whenever CS2 updates.

5. **Clean-Room C++ Reconstruction**:
   - Reconstructed engine algorithms are implemented in pure standard C++17 under `game/` grouped strictly by binary module (`game/client/`, `game/server/`, `game/engine2/`, etc.) with zero third-party runtime dependencies.

---

## 📚 External Reference Frameworks & Skills

To accelerate reverse-engineering precision and cross-reference stripped binary VTables:
- **[AlliedModders s2sdk](https://github.com/alliedmodders/s2sdk)**: Unofficial Source 2 SDK providing community-maintained C++ header structures, entity handle bitmasks (`0x7FFF` index, 17-bit serial), SchemaSystem classes, and virtual method orders across `tier0`, `engine2`, `entity2`, `schemasystem`, and `vphysics2`.
- **[Valve Developer Community: Source 2](https://developer.valvesoftware.com/wiki/Source_2)**: Architectural documentation covering sub-tick input sampling, Rubikon physics, Sound Operator System (SOS), Panorama UI, and compiled asset structures.
- **Antigravity Custom Skill (`source2-sdk`)**: Formalized into [`.agents/skills/source2-sdk/SKILL.md`](.agents/skills/source2-sdk/SKILL.md) for automated lookup of VTable indices, struct offsets, and memory signatures.
