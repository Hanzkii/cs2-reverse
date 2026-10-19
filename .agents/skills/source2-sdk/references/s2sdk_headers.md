# AlliedModders s2sdk Header & VTable Reference

This reference documents the key C++ interfaces, virtual method table (VTable) slot layouts, and core data structures extracted from AlliedModders' `s2sdk` (formerly `hl2sdk-cs2`). Use these signatures to cross-reference stripped binary VTables in Ghidra.

---

## 1. Core Platform & Utility Structures (`tier0`, `tier1`)

### `IMemAlloc` (`tier0/memalloc.h`)
Source 2 routes all dynamic allocations through `g_pMemAlloc` exported by `tier0.dll`.
VTable Slots:
- Slot 1: `Alloc(size_t nSize)`
- Slot 2: `Realloc(void* pMem, size_t nSize)`
- Slot 3: `Free(void* pMem)`
- Slot 21: `GetSize(void* pMem)`

### `CUtlVector<T>` (`tier0/utlvector.h`)
Standard dynamic array used throughout all Source 2 modules:
```cpp
template <class T>
class CUtlVector {
public:
    int32_t m_Size;      // 0x00: Current element count
    char _pad0[0x4];     // 0x04: Alignment padding
    T* m_pElements;      // 0x08: Buffer pointer
    int32_t m_Capacity;  // 0x10: Allocated capacity
    char _pad1[0x4];     // 0x14: Alignment padding
};
// Size = 0x18 bytes (64-bit)
```

### `CUtlStringToken` (`tier1/utlstringtoken.h`)
Fast hashed string representation for runtime dictionary lookups:
```cpp
class CUtlStringToken {
public:
    uint32_t m_nHashCode; // 0x00: 32-bit MurmurHash2 of lowercase string
};
```

---

## 2. SchemaSystem Interface (`schemasystem/schemasystem.h`)

Interface Version: `"SchemaSystem_001"` (exported by `schemasystem.dll`)

### `ISchemaSystem` VTable:
- Slot 13: `FindTypeScopeForModule(const char* pszModule)` -> returns `CSchemaSystemTypeScope*`
- Slot 14: `GetTypeScopes()` -> returns array of active module scopes
- Slot 25: `GlobalTypeScope()` -> global types scope

### `CSchemaSystemTypeScope` VTable & Access:
- Slot 2: `FindDeclaredClass(const char* pszClassName)` -> returns `SchemaClassInfoData_t*`
- Slot 3: `FindDeclaredEnum(const char* pszEnumName)` -> returns `SchemaEnumInfoData_t*`

### Field Metadata Bitflags:
- `MNetworkEnable`: Field is registered for networked entity serialization.
- `MNetworkChangeAccessorField`: Field triggers a notification callback on change.
- `MNetworkSerializer`: Custom serializer function bound to this field.

---

## 3. Entity2 System (`entity2/entityidentity.h`, `entity2/entityinstance.h`)

### `CEntityIdentity`:
Every entity in Source 2 is identified by an identity block:
```cpp
class CEntityIdentity {
public:
    CEntityInstance* m_pInstance;      // 0x00: Pointer back to entity instance
    SchemaClassInfoData_t* m_pClass;    // 0x08: Schema class descriptor
    uint32_t m_flags;                  // 0x10: Entity status flags
    uint32_t m_nEntityHandle;          // 0x14: Split 15-bit index, 17-bit serial
    const char* m_name;                // 0x18: Entity target name
    const char* m_designerName;        // 0x20: Classname (e.g. "cs_player_controller")
    uint32_t m_worldGroupId;           // 0x28
    char _pad0[0x14];                  // 0x2C
    CEntityIdentity* m_pPrev;          // 0x40: Doubly linked list links
    CEntityIdentity* m_pNext;          // 0x48
    CEntityIdentity* m_pPrevByClass;   // 0x50
    CEntityIdentity* m_pNextByClass;   // 0x58
};
```

### Entity Handle Deconstruction:
```cpp
#define ENT_ENTRY_MASK 0x7FFF
#define NUM_SERIAL_NUM_SHIFT_BITS 15
#define ENT_HANDLE_INVALID 0xFFFFFFFF

inline uint32_t GetEntityIndex(uint32_t hHandle) {
    return hHandle & ENT_ENTRY_MASK;
}

inline uint32_t GetEntitySerial(uint32_t hHandle) {
    return hHandle >> NUM_SERIAL_NUM_SHIFT_BITS;
}
```

---

## 4. Engine2 Service Architecture (`engine2/`)

In Source 2, engine loops are organized into service managers (`IEngineServiceMgr`):

### Core Engine Services:
1. `INetworkServerService` (`"NetworkServerService_001"`):
   - Manages dedicated and listen server network loops.
   - Slot 23: `GetIGameServer()`
   - Slot 24: `GetNetworkChannel(int slot)`
2. `INetworkClientService` (`"NetworkClientService_001"`):
   - Manages client connection to server, packet pacing, and connection state.
   - Slot 21: `GetNetworkChannel()`
   - Slot 22: `GetSignonState()`
3. `IGameEventSystem` (`"GameEventSystem_001"`):
   - Dispatches engine and game events (e.g., `player_death`, `round_start`).

---

## 5. VPhysics2 & Rubikon Collision (`vphysics2/`)

Source 2 completely removed Havok in favor of Valve's in-house **Rubikon** physics engine:
- Interface: `IVPhysics2` (`"VPhysics2_Interface_001"`)
- Coordinate system: Source units ($1 \text{ unit} = 0.75 \text{ inches} = 0.01905 \text{ meters}$).
- Ray Trace Structure (`RnRay_t`):
```cpp
struct RnRay_t {
    Vector m_vStart;        // 0x00
    Vector m_vEnd;          // 0x0C
    Vector m_vOffset;       // 0x18
    Vector m_vExtent;       // 0x24 (Bounding box half-extents)
    uint32_t m_nFlags;      // 0x30
    uint64_t m_nMask;       // 0x38 (Collision trace masks: CONTENTS_SOLID, etc.)
};
```
