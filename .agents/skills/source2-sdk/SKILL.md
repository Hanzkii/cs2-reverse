---
name: source2-sdk
description: >-
  Authoritative reverse-engineering reference guide for Source 2 and Counter-Strike 2 based on
  AlliedModders s2sdk and Valve Developer Community (VDC) architecture. Use when analyzing
  Source 2 binaries (client.dll, server.dll, engine2.dll, schemasystem.dll, vphysics2.dll,
  networksystem.dll), resolving virtual method tables (VTables), decoding entity handles,
  inspecting SchemaSystem classes, or tracing network wire formats.
---

# Source 2 SDK & Engine Reference (s2sdk + VDC)

This skill provides fast lookup tables, virtual table (VTable) indices, memory structures, and architectural principles derived from **AlliedModders `s2sdk`** and the **Valve Developer Community (VDC) Source 2 Documentation**.

Use this knowledge to accelerate Ghidra decompilation, symbol reconstruction, and simulator development for CS2 / Source 2 binaries.

---

## Quick Reference Links
- Detailed Header & VTable Reference: [s2sdk_headers.md](./references/s2sdk_headers.md)
- Engine Architecture & Subsystems: [vdc_architecture.md](./references/vdc_architecture.md)

---

## 1. Fast Decompilation Disambiguation Rules

When analyzing x86-64 assembly / decompiler output in Ghidra for Source 2 modules:

### A. Entity Handle Extraction (`CEntityHandle` / `CHandle<T>`)
In Source 2, entity handles are 32-bit unsigned integers with a split bitfield:
- **Index Mask (`0x7FFF`)**: Bits 0..14 (up to 32,768 active entities).
- **Serial Mask (`0x1FFFF`)**: Bits 15..31 (generation count to prevent dangling references).
- **Invalid Handle**: `0xFFFFFFFF` (`INVALID_EHANDLE_INDEX`).

```cpp
// Common Ghidra pattern:
uint32_t handle = *(uint32_t*)(pEntity + 0x10);
uint32_t entityIndex = handle & 0x7FFF;
uint32_t serialNumber = handle >> 15;
```

### B. String Tokens (`CUtlStringToken`)
Source 2 uses 32-bit hashed tokens extensively for schema field lookups, network message dispatch, and material parameters instead of raw strings:
- **Algorithm**: MurmurHash2 (32-bit, seed `0x31415926` or zero-seed depending on subsystem) or normalized FNV-1a.
- **Lowercase Pre-pass**: Strings are lowercased and path separators normalized (`/` vs `\`) prior to hashing.

```cpp
struct CUtlStringToken {
    uint32_t m_nHashCode; // 0x0
};
```

### C. Interface Discovery (`CreateInterface`)
Every Source 2 DLL exports `CreateInterface(const char* pName, int* pReturnCode)`:
- Internal structure is a linked list of `InterfaceReg` nodes:
```cpp
struct InterfaceReg {
    void* (*m_CreateFn)();
    const char* m_pName;
    InterfaceReg* m_pNext;
};
```
- Core interfaces:
  - `SchemaSystem_001` in `schemasystem.dll`
  - `Source2EngineToServer001` / `Source2EngineToClient001` in `engine2.dll`
  - `Source2ServerConfig001` in `server.dll`
  - `Source2Client002` in `client.dll`
  - `VPhysics2_Interface_001` in `vphysics2.dll`
  - `NetworkSystemVersion001` in `networksystem.dll`

---

## 2. SchemaSystem Memory Introspection

Source 2 replaces legacy Source 1 `RecvTable` / `SendTable` data tables with the universal **SchemaSystem**.

### Memory Layout of Schema Metadata:
```cpp
// Pointer obtained from: ISchemaSystem::FindTypeScopeForModule("client.dll")
class CSchemaSystemTypeScope {
    void* vftable;
    char m_szScopeName[256];
    // Hash map / red-black tree of declared classes
};

struct SchemaClassInfoData_t {
    SchemaClassInfoData_t* m_pSelf;     // 0x00
    const char* m_pszName;              // 0x08 (e.g. "C_CSPlayerPawn")
    const char* m_pszModule;            // 0x10 (e.g. "client.dll")
    int32_t m_nSize;                    // 0x18 (Instance byte size)
    int16_t m_nFieldCount;              // 0x1C (Number of fields)
    int16_t m_nStaticCount;             // 0x1E
    int16_t m_nMetadataCount;           // 0x20
    char _pad0[0x6];                    // 0x22
    SchemaClassFieldData_t* m_pFields;  // 0x28 (Array of fields)
    SchemaBaseClassInfoData_t* m_pBaseClasses; // 0x30
};

struct SchemaClassFieldData_t {
    const char* m_pszName;              // 0x00 (e.g. "m_iHealth")
    SchemaType_t* m_pType;              // 0x08 (Field type descriptor)
    int32_t m_nOffset;                  // 0x10 (Relative offset within class)
    int32_t m_nMetadataCount;           // 0x14
    SchemaMetadataEntryData_t* m_pMetadata; // 0x18
};
```

---

## 3. Sub-Tick & UserCmd Prediction

Valve's Source 2 engine replaces discrete 64-tick input sampling with sub-tick input accumulation:
- **`CSGOUserCmdPB`**: Protobuf wrapper holding base commands, tick sequence numbers, and sub-tick movement vectors.
- **`CSubtickMoveStep`**: Contains `float subtick_fraction` (range `[0.0, 1.0]`) specifying the exact microsecond within the frame interval when button presses, view angle adjustments, or jumps occurred.
- **Physics Execution**: `vphysics2.dll` (Rubikon) integrates continuous collision resolution using sub-tick fractional slices rather than snapping forces to discrete tick boundaries.

---

## 4. Reverse Engineering Workflow

When tackling a new Source 2 DLL in Ghidra:

1. **Locate `CreateInterface`**:
   - Trace the export `CreateInterface`.
   - Inspect the global registration pointer (`s_pInterfaceRegs`) to discover all exported classes and their version strings.
2. **Resolve VTables against `s2sdk`**:
   - Compare virtual method order with AlliedModders headers in [references/s2sdk_headers.md](./references/s2sdk_headers.md).
   - Label missing symbols (`sub_180...`) with their canonical SDK names.
3. **Inspect Entity Classes**:
   - Find references to `SchemaSystem_001` -> `FindTypeScopeForModule`.
   - Traverse `SchemaClassInfoData_t` to locate dynamic field offsets (`m_iHealth`, `m_pClippingWeapon`, `m_vOldOrigin`).
4. **Inspect Network Packets**:
   - Locate wire framing handlers: `CNetMessagePB<T>` decoding routines using Protobuf-C++ reflection.
