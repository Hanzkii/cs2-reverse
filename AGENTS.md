# Repository Rules & Invariants: cs2-reverse

## 1. Strict Privacy & Path Sanitization Guardrail
- **Absolute Local Paths Prohibited**: NEVER output, commit, or serialize absolute local system paths (such as `C:\Users\...`, local drive letters `E:\...`, or username references) into git commits, markdown documentation (`docs/*.md`), generated web files (`portal-data.js`, `docs-data.js`), or `README.md`.
- **Allowed Path Formats**:
  - In documentation links: Clean relative paths (e.g. `../simulators/foo.cpp`).
  - In configuration/setup guides: Generic environment placeholders (`<STEAM_LIBRARY>`, `<DISASSEMBLY_ROOT>`).
- **Pre-Commit Verification**: Always verify that neither `Users` nor personal local drive paths exist in modified files before committing.

## 2. Strict Binary Exclusion Guardrail (Zero Binaries in Git)
- **Prohibited Extensions & Formats**: NEVER stage, commit, or push any compiled PE executables (`*.exe`), dynamic link libraries (`*.dll`), object files (`*.obj`), static libraries (`*.lib`, `*.a`), shared objects (`*.so`, `*.dylib`), debug symbols (`*.pdb`, `*.idb`), linker artifacts (`*.exp`, `*.ilk`), raw memory/binary dumps (`*.bin`, `*.dmp`), or Ghidra project databases (`*.rep/`, `*.gpr`).
- **Version Archiving Isolation**: Raw game binaries from Counter-Strike 2 updates MUST strictly reside in local-only directories (`versions/archive/<DATE>_<BUILD_ID>/`) and be explicitly excluded via `.gitignore`.
- **Track Metadata, Never Binaries**: Only commit extracted structured metadata (`versions/profiles/` JSON schemas, cvars, interfaces) and markdown diff reports (`versions/diffs/`).
- **Pre-Commit Binary Audit**: Before every `git commit`, inspect staged files via `git diff --cached --name-only` to ensure zero binary files or prohibited extensions are staged.
- **Repository Size Invariant**: Keep the Git remote repository lightweight (< 15 MB total) consisting purely of source code, markdown documentation, simulators, Python analysis tools, and JSON telemetry profiles.

## 3. Source 2 Reverse Engineering Workflow & Skill Usage
- **Active Reference**: Consult and apply the custom `source2-sdk` skill (`.agents/skills/source2-sdk/SKILL.md`) whenever analyzing Source 2 binaries in Ghidra or writing simulators.
- **Decompilation Disambiguation**:
  - **Entity Handles**: Always treat `CEntityHandle` / `CHandle<T>` as 32-bit bitfields (Index: `handle & 0x7FFF`, Serial: `handle >> 15`).
  - **Hashed Identifiers**: Check for `CUtlStringToken` 32-bit MurmurHash2 or FNV-1a hashes for schema keys and net messages.
  - **VTable Identification**: Match stripped virtual function pointers against AlliedModders `s2sdk` virtual method table orders (`public/tier0`, `public/engine2`, `public/entity2`, `public/schemasystem`, `public/vphysics2`).
  - **SchemaSystem Reflection**: Resolve dynamic fields via `SchemaSystem_001` -> `FindTypeScopeForModule` -> `FindDeclaredClass`.

## 4. Engineering & Verification Standards
- **Static Analysis Only**: No live game process hooks, DLL injection, or dynamic memory modification.
- **Reconstructed SDK & Simulator Standards**: Pure C++17 under `game/` (`game/interfaces/` and `game/shared/`) and reference simulation runners under `docs/simulators/`, compiled with MSVC x64 (`/O2 /std:c++17 /EHsc`), zero third-party pip/vcpkg runtime dependencies.
- **Portal Data Sync**: Whenever `docs/*.md` are modified or added, always regenerate portal data via `python tools/generate_docs_bundle.py; python tools/generate_portal_data.py`.

## 5. Canonical Function Signatures & RVA Tracking Protocol
- **Centralized Registry**: Every reversed internal function, math routine, or virtual execution dispatcher MUST be registered in `data/function_signatures.json` with its target binary, symbol name, baseline RVA/VA, and robust IDA/Ghidra pattern.
- **100% Uniqueness Invariant**: Every pattern MUST be verified against the baseline archive to produce exactly 1 match using `python tools/scan_signatures.py`. Ambiguous (>1) or broken (0) patterns must be extended with discriminator bytes.
- **Automated Update Diffing**: On each new CS2 patch, `tools/cs2_patch_diff.py` and `tools/scan_signatures.py --diff` automatically detect RVA shifts (`+N` bytes) and flag broken signatures requiring Ghidra re-analysis.

## 6. Temporary Tools & Scratchpad Guardrail
- **No Temporary or Auxiliary Scripts in `tools/`**: Only canonical, documented reverse-engineering utilities that directly analyze game binaries or build project datasets may reside in `tools/`.
- **Use `temp/` for Scratchpad Operations**: Any one-off migration, cleanup, temporary scraping, or intermediate inspection scripts must be placed in `temp/` (or `tools/temp/`), which is excluded via `.gitignore`.
- **Immediate Post-Task Cleanup**: Always delete temporary scripts immediately after their execution. Never stage or commit ad-hoc exploration scripts to Git.

