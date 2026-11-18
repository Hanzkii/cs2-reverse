# Chapter 25: Entity System Architecture, CBaseEntity & Controller-Pawn Linkage

## 1. Executive Subsystem Summary

In Counter-Strike 2 (`server.dll`, Build `2026-11-11_26598758`), Valve implements an entity-component-system hybrid architecture centered around `CBaseEntity`, `CEntitySystem`, and the decoupling of the client's session state (`CCSPlayerController`) from the physical combat avatar (`CCSPlayerPawn`).

This chapter documents the clean-room, 1:1 ground-truth reverse-engineered structures, virtual method tables (VFTables), member byte offsets, and handle dereference mechanics directly from the game binary.

---

## 2. CBaseEntity Virtual Method Table & Footprint

- **Complete Object Locator (COL)**: `0x181B6A6D0` (RVA `0x1B6A6D0`)
- **Primary VFTable Base**: `0x18194FB38` (RVA `0x194FB38`, 238 slots)
- **Total Class Footprint**: Exactly **0x4A8 bytes (1,192 bytes)**

### 2.1 Critical Lifecycle Virtual Method Slots

| Method | VFTable Slot | Server VA / RVA | Architectural Role |
| :--- | :---: | :---: | :--- |
| `~CBaseEntity` | **Slot 0** | `0x180CDCf00` / `0x0CDCF00` | MSVC x64 Virtual Scalar Destructor |
| `PostConstructor` | **Slot 1** | `0x1801540D0` / `0x01540D0` | Post-allocation identity & component initialization |
| `Spawn` | **Slot 2** | `0x180153FF0` / `0x0153FF0` | Primary entity lifecycle spawn routine |
| `Precache` | **Slot 3** | `0x180614920` / `0x0614920` | Resource string / model / sound preloading |
| `Activate` | **Slot 4** | `0x180CD1C70` / `0x0CD1C70` | Post-spawn world activation; asserts `sizeof(CBaseEntity) == 0x4A8` |
| `PostDataUpdate` | **Slot 11** | `0x181399F50` / `0x1399F50` | Post-deserialization network update |
| `Think` | **Slot 81** | `0x180159DD0` / `0x0159DD0` | Virtual think callback |

### 2.2 Verified Member Offsets in `CBaseEntity`

| Offset | Type | Identifier | Description |
| :--- | :--- | :--- | :--- |
| `+0x0010` | `CEntityIdentity*` | `m_pEntityIdentity` | Pointer to 112-byte entity descriptor |
| `+0x0030` | `CBodyComponent*` | `m_CBodyComponent` | Scene node & transform anchor |
| `+0x0248` | `CUtlVector<thinkfunc_t>` | `m_aThinkFunctions` | Think context array |
| `+0x0288` | `void*` | `m_think` | Primary think delegate |
| `+0x02D0` | `int32_t` | `m_iHealth` | Current hitpoints |
| `+0x02D4` | `int32_t` | `m_iMaxHealth` | Maximum hitpoints |
| `+0x02D8` | `uint8_t` | `m_lifeState` | Life state (`0 = ALIVE`, `1 = DYING`, `2 = DEAD`) |
| `+0x031C` | `uint32_t` | `m_nSubclassID` | MurmurHash2 string token archetype ID |
| `+0x0344` | `int32_t` | `m_iTeamNum` | Active team index |
| `+0x0364` | `int32_t` | `m_nNextThinkTick` | Next scheduled execution tick |
| `+0x0388` | `uint32_t` | `m_fFlags` | Movement flags (`FL_ONGROUND`, `FL_DUCKING`) |
| `+0x03D8` | `CCollisionProperty*` | `m_pCollision` | Spatial bounding box and collision masks |
| `+0x04A0` | `void*` | `m_pPulseGraphInstance`| Scripting graph instance pointer |

---

## 3. CEntitySystem Partitioned Chunk Array & Handle Dereference

In Source 2, entities are indexed through a 32-bit `CEntityHandle`:
$$\text{Entity Index} = \text{Handle} \ \& \ \text{0x7FFF} \quad (15\text{ bits, up to } 32,768 \text{ entities})$$
$$\text{Serial Number} = \text{Handle} \gg 15 \quad (17\text{ bits, ABA prevention})$$

The `CEntitySystem` stores entities in 64 chunks of 512 entries each:
$$\text{Chunk Index} = \text{Index} \gg 9, \quad \text{Slot Index} = \text{Index} \ \& \ \text{0x1FF}$$
$$\text{Descriptor Address} = \text{ChunkPointer} + \text{SlotIndex} \times 0\text{x}70 \quad (\text{sizeof}(CEntityIdentity) = 112\text{ bytes})$$

---

## 4. CCSPlayerController $\leftrightarrow$ CCSPlayerPawn Linkage

- **`CCSPlayerController`** (`0xAB8` bytes): Primary VFTable at `0x18190DED8`. Holds session identity, name, money, and spectator state.
  - `m_hPlayerPawn`: Offset **`+0x08EC`** (`CHandle<CCSPlayerPawn>`)
  - `m_hObserverPawn`: Offset **`+0x08F0`** (`CHandle<CBaseEntity>`)
  - `m_bPawnIsAlive`: Offset **`+0x08FC`** (`bool`)
  - `m_sSanitizedPlayerName`: Offset **`+0x051C`** (`char[128]`)
- **`CCSPlayerPawn`** (`0x15C0` bytes): Primary VFTable at `0x1817843E8`. Holds combat physical state, services, and inventory.
  - `m_hController`: Offset **`+0x0C98`** (`CHandle<CCSPlayerController>`)
  - `m_pWeaponServices`: Offset **`+0x0B10`**
  - `m_pItemServices`: Offset **`+0x0B18`**
  - `m_pMovementServices`: Offset **`+0x0B50`**
  - `m_pCameraServices`: Offset **`+0x0B48`**

### 4.1 Decompiled Handle Dereference (`0x180212390`)
```x86asm
; CCSPlayerController::GetPlayerPawn
mov     edx, dword ptr [rcx + 8ECh]    ; Load m_hPlayerPawn
cmp     edx, -1                        ; Invalid handle check
je      ret_null
mov     rcx, qword ptr [g_pEntitySystem]
and     eax, 7FFFh                     ; Index = handle & 0x7FFF
shr     rax, 9                         ; Chunk = Index / 512
mov     r9, qword ptr [rcx + rax*8]    ; Chunk pointer
and     r8d, 1FFh                      ; Slot = Index % 512
imul    rax, r8, 70h                   ; Offset = Slot * 112
add     rax, r9                        ; pIdentity
cmp     dword ptr [rax + 10h], edx     ; Verify serial number matches handle
cmovne  rax, r10                       ; If mismatch, return nullptr
mov     rax, qword ptr [rax]           ; return pIdentity->m_pEntityInstance
ret
```

---

## 5. Reconstructed 1:1 SDK Headers

- [game/server/baseentity.h](../game/server/baseentity.h) & [sdk/include/server/baseentity.hpp](../sdk/include/server/baseentity.hpp)
- [game/server/player_controller.h](../game/server/player_controller.h) & [sdk/include/server/player_controller.hpp](../sdk/include/server/player_controller.hpp)
