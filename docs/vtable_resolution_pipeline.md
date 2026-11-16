# Source 2 VTable & RTTI Auto-Resolution Pipeline

This document details the automated reverse-engineering methodology, memory structures, and orchestration toolchain used to reconstruct stripped virtual method tables (VTables) across Counter-Strike 2 compiled PE binaries (`client.dll`, `server.dll`, `engine2.dll`, `schemasystem.dll`, `tier0.dll`, etc.).

---

## 1. Architectural Overview & The Stripped Binary Challenge

In production release builds of Counter-Strike 2, Valve strips symbol tables, function names, and debug information (PDBs). Furthermore, functions are aggressively optimized by the Microsoft Visual C++ (MSVC) compiler under Link-Time Code Generation (`/LTCG`) and Whole Program Optimization (`/GL`).

However, dynamic polymorphism in modern C++ requires runtime dispatcher tables:
1. **Virtual Method Tables (VTables)**: Contiguous arrays of 64-bit function pointers residing in the read-only initialized data section (`.rdata`).
2. **MSVC Run-Time Type Information (RTTI)**: Metadata generated for polymorphic types to support `dynamic_cast`, `typeid`, and C++ exception handling (`/GR`).
3. **Engine Interface Registrations (`CreateInterface`)**: Singly-linked list of `InterfaceReg` nodes exposing public engine subsystems.
4. **Runtime Schema Reflection (`SchemaSystem`)**: Type descriptors detailing class sizes, field offsets, and inheritance hierarchies.

The **Source 2 VTable Resolver Toolchain** automates the end-to-end extraction, demangling, canonical SDK correlation, and Ghidra program annotation across all core game modules.

```
+-----------------------------------------------------------------------------------------+
|                                Source 2 PE Module (.rdata)                              |
|                                                                                         |
|   +---------------------------------------+      +----------------------------------+   |
|   |   _RTTICompleteObjectLocator (COL)    |      |         TypeDescriptor           |   |
|   |  - signature: 0x00000001 (x64)        |      |  - pVFTable: &type_info::vftable |   |
|   |  - offset: byte offset in class       | ---> |  - spare: nullptr                |   |
|   |  - cdOffset: constructor disp (0)     |      |  - name: ".?AVCEngineClient@@\0" |   |
|   |  - typeDescriptorRVA: &TypeDescriptor |      +----------------------------------+   |
|   +---------------------------------------+                                             |
|                      ^                                                                  |
|                      | (vtable - 0x08)                                                  |
|   +---------------------------------------------------------------------------------+   |
|   |                           Virtual Method Table (vftable)                        |   |
|   |  Slot 0  (+0x00): Function Ptr -> .text (e.g. CEngineClient::~CEngineClient)    |   |
|   |  Slot 1  (+0x08): Function Ptr -> .text (e.g. CEngineClient::Connect)           |   |
|   |  Slot 27 (+0xD8): Function Ptr -> .text (e.g. CEngineClient::IsInGame)          |   |
|   |  Slot 32 (+0x100): Function Ptr -> .text (e.g. CEngineClient::GetScreenSize)    |   |
|   +---------------------------------------------------------------------------------+   |
+-----------------------------------------------------------------------------------------+
```

---

## 2. MSVC x64 C++ Virtual Dispatch Memory Layout

On 64-bit Windows under MSVC ABI, every polymorphic class instance contains a virtual method table pointer (`vptr`) at offset `+0x00` (or offset `+N` for multiple inheritance bases).

### 2.1 Complete Object Locator (`_RTTICompleteObjectLocator`)

Immediately preceding every valid polymorphic vtable array in `.rdata` (at `vtable_address - 0x08`), the compiler places a 64-bit absolute virtual address (VA) pointing to the class's `_RTTICompleteObjectLocator`:

```cpp
struct _RTTICompleteObjectLocator {
    uint32_t signature;          // +0x00: 0 for 32-bit x86, 1 for 64-bit x86-64
    uint32_t offset;             // +0x04: Byte offset of this vtable within complete class
    uint32_t cdOffset;           // +0x08: Constructor displacement offset (0 for normal single/multiple inheritance)
    int32_t  typeDescriptorRVA;  // +0x0C: 32-bit RVA pointing to the TypeDescriptor
    int32_t  classHierarchyRVA;  // +0x10: 32-bit RVA pointing to _RTTIClassHierarchyDescriptor
    int32_t  objectBaseRVA;      // +0x14: 32-bit RVA pointing to module ImageBase
};
static_assert(sizeof(_RTTICompleteObjectLocator) == 0x18, "COL size must be 24 bytes in x64");
```

### 2.2 Type Descriptor (`_RTTITypeDescriptor`)

The `TypeDescriptor` contains the mangled class name:

```cpp
struct _RTTITypeDescriptor {
    void* pVFTable;              // +0x00: Pointer to type_info virtual table in .rdata
    void* spare;                 // +0x08: Reserved runtime pointer (nullptr in static PE)
    char  name[];                // +0x10: Null-terminated mangled ASCII string
};
```

In MSVC ABI:
- Classes start with `.?AV` (e.g. `.?AVCEngineClient@@`, `.?AVCSchemaSystem@@`).
- Structs start with `.?AU` (e.g. `.?AUInterfaceReg@@`).
- Namespaces are encoded in reverse order separated by `@` (e.g. `.?AVexception@std@@` $\rightarrow$ `std::exception`, `.?AVMapFieldAccessor@internal@protobuf@google@@` $\rightarrow$ `google::protobuf::internal::MapFieldAccessor`).
- Template classes start with `?$TemplateName@Arg1@Arg2@@`.
- Function-local classes embed `@?1??FunctionName@EnclosingClass@@...`.

---

## 3. Multiple Inheritance & `this` Pointer Adjustments

When a Source 2 class inherits from multiple base classes, MSVC creates multiple vtables within the object layout.

### 3.1 Memory Layout of Multiple Inheritance Objects

Consider a derived class inheriting from two polymorphic interfaces:

```cpp
class CCSPlayerPawn : public C_BasePlayerPawn, public IEntityListener {
    // Primary vtable (offset +0x0) covers C_BasePlayerPawn and all its ancestors
    // Secondary vtable (offset +0xE28) covers IEntityListener methods
};
```

In memory:
- **Primary VTable**: Offset `0x0`. In the COL, `offset == 0`. Calls to primary virtual methods receive the instance pointer directly in `rcx`.
- **Secondary VTable**: Offset `+0xE28` (or base offset). In the COL, `offset == 0x0E28`. When calling through an `IEntityListener*` interface pointer, `rcx` points to `pInstance + 0xE28`.

```
Instance Memory [CCSPlayerPawn]
+0x0000: vptr_primary (Points to CCSPlayerPawn primary vtable)
+0x0008: C_BaseEntity / C_BasePlayerPawn member fields...
...
+0x0E28: vptr_secondary (Points to CCSPlayerPawn[IEntityListener] vtable)
+0x0E30: IEntityListener member fields / remaining derived fields...
```

### 3.2 Adjustor Thunks (Fixup Stubs)

When the derived class overrides a virtual method declared in a secondary base interface, calling that method requires fixing `this` back from `pInstance + offset` to `pInstance`. The compiler generates an **Adjustor Thunk** in `.text`:

```nasm
; Adjustor thunk in .text:
sub   rcx, 0xE28         ; Adjust 'this' pointer from secondary base to complete class
jmp   CCSPlayerPawn::OnEntityEvent ; Tail-call the actual implementation
```

The VTable Resolver identifies secondary vtables via `col.offset > 0` and names them accordingly:
`<ClassName>+0x<HexOffset>::vtable[<Slot>]`.

---

## 4. Correlation with SchemaSystem & AlliedModders s2sdk

The pipeline correlates discovered stripped VTables against two authoritative sources:

### 4.1 Canonical AlliedModders `s2sdk` Virtual Slot Mapping

For core engine services and singleton interfaces, AlliedModders `s2sdk` defines stable virtual function orders:

| Interface / Class | Binary | Critical Slot Numbers & Canonical Method Names |
| :--- | :--- | :--- |
| **`ISchemaSystem`** | `schemasystem.dll` | Slot 0: `~ISchemaSystem`<br>Slot 13: `FindTypeScopeForModule`<br>Slot 14: `FindTypeScopeForModuleByToken`<br>Slot 25: `GlobalTypeScope` |
| **`CSchemaSystemTypeScope`** | `schemasystem.dll` | Slot 0: `~CSchemaSystemTypeScope`<br>Slot 1: `GetScopeName`<br>Slot 2: `FindDeclaredClass`<br>Slot 3: `FindDeclaredEnum`<br>Slot 24: `FindDeclaredClassByToken`<br>Slot 25: `FindDeclaredClassByName` |
| **`ISource2EngineToClient`** | `engine2.dll` | Slot 0: `~ISource2EngineToClient`<br>Slot 27: `IsInGame`<br>Slot 28: `IsConnected`<br>Slot 30: `GetMaxClients`<br>Slot 32: `GetScreenSize`<br>Slot 37: `GetLocalPlayer`<br>Slot 41: `GetNetChannelInfo`<br>Slot 51: `ExecuteClientCmd` |
| **`INetworkServerService`** | `engine2.dll` | Slot 0: `~INetworkServerService`<br>Slot 23: `GetIGameServer`<br>Slot 24: `GetNetworkChannel` |
| **`INetworkClientService`** | `engine2.dll` | Slot 0: `~INetworkClientService`<br>Slot 21: `GetNetworkChannel`<br>Slot 22: `GetSignonState`<br>Slot 23: `GetNetworkGameClient` |
| **`IEngineCVar`** | `tier0.dll` | Slot 0: `~IEngineCVar`<br>Slot 11: `FindVar`<br>Slot 12: `FindVarByIndex`<br>Slot 41: `GetFirstCVarArrayValue`<br>Slot 42: `GetNextCVarArrayValue` |
| **`IMemAlloc`** | `tier0.dll` | Slot 0: `~IMemAlloc`<br>Slot 1: `Alloc`<br>Slot 2: `Realloc`<br>Slot 3: `Free`<br>Slot 21: `GetSize` |
| **`CEntityInstance`** | `client.dll` / `server.dll` | Slot 0: `~CEntityInstance`<br>Slot 1: `GetRefEHandle`<br>Slot 2: `GetClassname`<br>Slot 3: `GetDebugName`<br>Slot 4: `GetEntityIdentity` |
| **`CBaseEntity`** | `server.dll` / `client.dll` | Slot 0: `~CBaseEntity`<br>Slot 1: `PostConstructor`<br>Slot 2: `Spawn`<br>Slot 3: `Precache`<br>Slot 4: `Activate` |

### 4.2 SchemaSystem Runtime Reflection Cross-Referencing

Classes extracted from `versions/profiles/<BUILD_ID>/schemas.json` contain the exact inheritance chain (`base`) and field layout. By matching demangled RTTI class names (`C_CSPlayerPawn`, `CCSPlayerController`, `CBaseAnimGraph`) against SchemaSystem metadata, the resolver correlates virtual method counts with the class depth in the inheritance tree.

---

## 5. Toolchain Components & Usage

The toolchain is implemented in pure standard Python 3 and Java for Ghidra, with zero external pip dependencies.

### 5.1 Component Architecture

```
tools/
├── ghidra/
│   ├── Source2_VTableResolver.java  # Ghidra post-script (Headless & GUI)
│   ├── resolve_vtables.py           # CLI orchestrator & offline fast scanner
│   ├── port_signatures_to_ghidra.py # Canonical function signature porter
│   └── Source2_SchemaImporter.java  # Schema structures importer
data/
└── vtables.json                     # Canonical structured VTable metadata database
```

### 5.2 Standalone Offline Scan Mode

Scans compiled PE binaries directly using pure-Python standard library `struct` unpacking at over 2,000 vtables per second:

```bash
# Scan all core CS2 binaries in the latest archive
python tools/ghidra/resolve_vtables.py

# Scan a specific binary
python tools/ghidra/resolve_vtables.py --binary engine2.dll

# Scan binaries from an explicit archive folder
python tools/ghidra/resolve_vtables.py --archive-dir versions/archive/2026-11-11_26598758/
```

#### Typical Scan Performance Output:
```
=================================================================
 SOURCE 2 OFFLINE VTABLE RESOLVER
=================================================================
 Binary Archive: versions/archive/2026-11-11_26598758
 Output Target:  data/vtables.json
 Target Count:   10
=================================================================
  [+] schemasystem.dll             :    31 vtables ( 17 canonical slots) in 0.01s
  [+] tier0.dll                    :   110 vtables ( 10 canonical slots) in 0.04s
  [+] engine2.dll                  :  1121 vtables ( 28 canonical slots) in 0.16s
  [+] server.dll                   :  4683 vtables ( 19 canonical slots) in 1.25s
  [+] client.dll                   :  6662 vtables ( 25 canonical slots) in 1.27s
  [+] vphysics2.dll                :   227 vtables (  0 canonical slots) in 0.04s
  [+] networksystem.dll            :   306 vtables (  0 canonical slots) in 0.04s
  [+] inputsystem.dll              :     9 vtables (  2 canonical slots) in 0.01s
  [+] filesystem_stdio.dll         :    47 vtables (  0 canonical slots) in 0.03s
  [+] pulse_system.dll             :   169 vtables (  0 canonical slots) in 0.08s
=================================================================
 TOTAL RESOLVED: 13,365 VTables across 10 binaries (101 canonical slots) in 5.59s
 Structured report written to: data/vtables.json
=================================================================
```

### 5.3 Ghidra Headless Analyzer Mode

Orchestrates Ghidra's `analyzeHeadless.bat` across the project database to create namespaces, label functions, and apply plate comments:

```bash
python tools/ghidra/resolve_vtables.py --headless ^
  --ghidra-home "%GHIDRA_HOME%" ^
  --project-dir "%CS2_DISASSEMBLY_DIR%" ^
  --project-name "CS2" ^
  --folder "2026-11-11_26598758"
```

### 5.4 Interactive Ghidra GUI Execution

In Ghidra's **CodeBrowser**:
1. Open **Script Manager** (`Window -> Script Manager`).
2. Search for `Source2_VTableResolver.java` under category `Source2`.
3. Click **Run**.
4. The script identifies `.rdata`, resolves all VTables, labels them under namespaces, adds bookmarks under category `Source2 VTable`, and updates `data/vtables.json`.

---

## 6. Structured Metadata Schema (`data/vtables.json`)

The output metadata file `data/vtables.json` provides a standardized schema for consumption by simulators, SDK generators, and diff engines:

```json
[
  {
    "class_name": "CEngineClient",
    "raw_mangled": ".?AVCEngineClient@@",
    "binary": "engine2.dll",
    "vtable_rva": "0x5d1308",
    "vtable_va": "0x1805d1308",
    "this_offset": 0,
    "method_count": 185,
    "type_descriptor_rva": "0x6c1df0",
    "col_rva": "0x5f9ca8",
    "methods": [
      {
        "slot": 0,
        "rva": "0x24a520",
        "va": "0x18024a520",
        "symbol": "CEngineClient::~ISource2EngineToClient",
        "canonical": true
      },
      {
        "slot": 27,
        "rva": "0x78ab0",
        "va": "0x180078ab0",
        "symbol": "CEngineClient::IsInGame",
        "canonical": true
      },
      {
        "slot": 32,
        "rva": "0x76150",
        "va": "0x180076150",
        "symbol": "CEngineClient::GetScreenSize",
        "canonical": true
      }
    ]
  }
]
```

---

## 7. Update Protocol & Differential Verification

With every CS2 update:
1. Updated binaries are ingested into `versions/archive/<DATE>_<BUILD_ID>/`.
2. `python tools/ghidra/resolve_vtables.py` re-scans the new binaries.
3. The diff engine identifies:
   - Added/removed virtual method slots (method count shifts).
   - RVA delta displacements across versions.
   - Newly introduced polymorphic classes in `.rdata`.
4. Documentation bundle and portal data are regenerated via:
   ```bash
   python tools/generate_docs_bundle.py
   python tools/generate_portal_data.py
   ```
