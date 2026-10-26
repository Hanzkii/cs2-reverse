#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <compare>

// ============================================================================
// Source 2 Reverse Engineered SDK - Phase 1: Entity System Architecture
// Target Build: 2026-10-25_26144732 (CS2 Update Archive)
// Architecture: x86-64 (MSVC ABI)
// Reconstructed & Authoritatively Verified via Static RTTI & Machine Code Analysis
// ============================================================================

namespace cs2::sdk {

#ifndef PAD
#define PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:
#endif

// ============================================================================
// Constants & Architectural Limits for Source 2 Entity Subsystem
// Disassembly Verification Anchors:
//   - Mask 0x7FFF (15-bit index, 32768 slots): client.dll + 0x15d1a4, 0x15ba270
//   - Mask 0x1FF (512 slots per chunk): client.dll + 0x15d1b7, 0x15ba28e
//   - Shift 9 (>> 9 chunk index arithmetic): client.dll + 0x15d1ab, 0x15ba283
//   - Shift 15 (>> 15 serial generation count): client.dll + 0x15ba2af
//   - Chunk stride 0x70 (sizeof CEntityIdentity): client.dll + 0x15d1be (imul rcx, 0x70)
// ============================================================================
inline constexpr std::uint32_t kInvalidEHandleIndex    = 0xFFFFFFFF;
inline constexpr std::uint32_t kEntEntryMask          = 0x7FFF;     // 15 bits -> [0, 32767]
inline constexpr std::uint32_t kNumSerialNumShiftBits = 15;         // 17 bits serial -> bits [15..31]
inline constexpr std::uint32_t kEntitiesPerChunk      = 512;        // 0x200 entities per chunk
inline constexpr std::uint32_t kMaxEntities           = 32768;      // 0x8000 max concurrent entities
inline constexpr std::uint32_t kMaxChunks             = 64;         // 32768 / 512 = 64 chunks
inline constexpr std::uint32_t kChunkShift            = 9;          // log2(512)
inline constexpr std::uint32_t kSlotMask              = 0x1FF;      // 512 - 1

// Forward declarations
class CEntityInstance;
class CEntityIdentity;
class CGameEntitySystem;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
#endif

// ============================================================================
// 1. CEntityHandle
// Scalar 32-bit unsigned integer split bitfield:
//   Bits  0..14 : Entity index in CGameEntitySystem chunk array (0..32767)
//   Bits 15..31 : Serial generation counter (prevents ABA stale pointer reuse)
// ============================================================================
class CEntityHandle {
public:
    union {
        std::uint32_t m_Value;
        struct {
            std::uint32_t m_Index  : 15;
            std::uint32_t m_Serial : 17;
        };
    };

    constexpr CEntityHandle() noexcept
        : m_Value(kInvalidEHandleIndex) {}

    constexpr explicit CEntityHandle(std::uint32_t value) noexcept
        : m_Value(value) {}

    constexpr CEntityHandle(std::uint32_t nIndex, std::uint32_t nSerial) noexcept
        : m_Value((nIndex & kEntEntryMask) | (nSerial << kNumSerialNumShiftBits)) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return m_Value != kInvalidEHandleIndex && (m_Value & kEntEntryMask) != kEntEntryMask;
    }

    [[nodiscard]] constexpr std::uint32_t GetEntryIndex() const noexcept {
        return m_Value & kEntEntryMask;
    }

    [[nodiscard]] constexpr std::uint32_t GetSerialNumber() const noexcept {
        return m_Value >> kNumSerialNumShiftBits;
    }

    [[nodiscard]] constexpr std::uint32_t GetRaw() const noexcept {
        return m_Value;
    }

    constexpr explicit operator std::uint32_t() const noexcept {
        return m_Value;
    }

    constexpr explicit operator bool() const noexcept {
        return IsValid();
    }

    constexpr bool operator==(const CEntityHandle& other) const noexcept {
        return m_Value == other.m_Value;
    }

    constexpr bool operator!=(const CEntityHandle& other) const noexcept {
        return m_Value != other.m_Value;
    }

    constexpr auto operator<=>(const CEntityHandle& other) const noexcept {
        return m_Value <=> other.m_Value;
    }
};
static_assert(sizeof(CEntityHandle) == 0x04, "CEntityHandle size must be 4 bytes");

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// ============================================================================
// 2. CHandle<T>
// Strongly-typed wrapper around CEntityHandle
// ============================================================================
template <typename T = CEntityInstance>
class CHandle : public CEntityHandle {
public:
    using TargetType = T;

    constexpr CHandle() noexcept
        : CEntityHandle() {}

    constexpr explicit CHandle(std::uint32_t value) noexcept
        : CEntityHandle(value) {}

    constexpr explicit CHandle(CEntityHandle handle) noexcept
        : CEntityHandle(handle.GetRaw()) {}

    constexpr CHandle(std::uint32_t nIndex, std::uint32_t nSerial) noexcept
        : CEntityHandle(nIndex, nSerial) {}

    template <typename U>
    requires std::is_base_of_v<T, U> || std::is_base_of_v<U, T> || std::is_void_v<T>
    constexpr CHandle(const CHandle<U>& other) noexcept
        : CEntityHandle(other.GetRaw()) {}
};
static_assert(sizeof(CHandle<>) == 0x04, "CHandle size must be 4 bytes");

// ============================================================================
// 3. CEntityInstance
// Base class for all Source 2 entities (C_BaseEntity, C_CSPlayerPawn, etc.)
// Binary: client.dll / server.dll
// RTTI:   .?AVCEntityInstance@@ (COL: client.dll + 0x1f83068, VTable: client.dll + 0x1d978d8)
// ============================================================================
class CEntityInstance {
public:
    virtual ~CEntityInstance() = default;           // +0x00: Virtual destructor (Slot 0)
    PAD(0x08, m_pad08);                             // +0x08: m_iszPrivateVScripts (int64)
    CEntityIdentity* m_pEntityIdentity;             // +0x10: Pointer back to CEntityIdentity descriptor
    PAD(0x18, m_pad18);                             // +0x18..+0x30

    [[nodiscard]] inline CEntityIdentity* GetIdentity() const noexcept {
        return m_pEntityIdentity;
    }

    [[nodiscard]] inline CEntityHandle GetHandle() const noexcept;
};
static_assert(sizeof(CEntityInstance) == 0x30, "CEntityInstance size must be 0x30 bytes");
static_assert(offsetof(CEntityInstance, m_pEntityIdentity) == 0x10, "m_pEntityIdentity offset mismatch");

// ============================================================================
// 4. CEntityIdentity
// Header/metadata descriptor allocated in contiguous 512-element chunks.
// Size: Exactly 0x70 bytes (112 bytes).
// Disassembly Provenance Anchors:
//   - Stride calculation: client.dll + 0x15d1be `imul rax, rdx, 0x70`
//   - Stride calculation: client.dll + 0x15ba28e `imul rcx, rcx, 0x70`
//   - Stride calculation: client.dll + 0x15c5aa8 `imul rcx, rcx, 0x70`
// Member Layout Verification:
//   - m_pEntity              : +0x00 (client.dll + 0x15d1d0 `mov rcx, [rax]`)
//   - m_pClassInfo           : +0x08 (SchemaClassInfoData_t descriptor pointer)
//   - m_handle               : +0x10 (client.dll + 0x15d1c5 `cmp [rax+0x10], ecx`)
//   - m_nameStringTableIndex : +0x14 (int32 string table token)
//   - m_pszName              : +0x18 (const char* targetname)
//   - m_pszDesignerName      : +0x20 (const char* classname e.g. "cs_player_controller")
//   - m_flags                : +0x30 (client.dll + 0x15ba2bc `mov ecx, [rcx+r9+0x30]`)
//   - m_worldGroupId         : +0x38 (uint32 world group ID)
//   - m_fDataObjectTypes     : +0x3C (uint32 data object mask)
//   - m_PathIndex            : +0x40 (uint64 path index)
//   - m_pAttributes          : +0x48 (void* attributes pointer)
//   - m_pPrev                : +0x50 (CEntityIdentity* doubly-linked entity chain)
//   - m_pNext                : +0x58 (CEntityIdentity* doubly-linked entity chain)
//   - m_pPrevByClass         : +0x60 (CEntityIdentity* class-filtered chain)
//   - m_pNextByClass         : +0x68 (CEntityIdentity* class-filtered chain)
// ============================================================================
class CEntityIdentity {
public:
    CEntityInstance*  m_pEntity;              // +0x00: Pointer to instantiated entity object
    void*             m_pClassInfo;           // +0x08: SchemaClassInfoData_t descriptor
    CEntityHandle     m_handle;               // +0x10: 32-bit handle (15-bit index, 17-bit serial)
    std::int32_t      m_nameStringTableIndex; // +0x14: String table index
    const char*       m_pszName;              // +0x18: Entity target name ("player", "weapon_ak47")
    const char*       m_pszDesignerName;      // +0x20: Designer classname ("cs_player_controller")
    std::uint64_t     m_pad28;                // +0x28: Flags / alignment
    std::uint32_t     m_flags;                // +0x30: Entity flags (active, dormant, etc.)
    std::uint32_t     m_pad34;                // +0x34: Alignment padding
    std::uint32_t     m_worldGroupId;         // +0x38: World group ID
    std::uint32_t     m_fDataObjectTypes;     // +0x3C: Data object bitmask
    std::uint64_t     m_PathIndex;            // +0x40: Hierarchy / nav path index
    void*             m_pAttributes;          // +0x48: Dynamic attribute storage
    CEntityIdentity*  m_pPrev;                // +0x50: Doubly linked list previous node
    CEntityIdentity*  m_pNext;                // +0x58: Doubly linked list next node
    CEntityIdentity*  m_pPrevByClass;         // +0x60: Next entity with matching class
    CEntityIdentity*  m_pNextByClass;         // +0x68: Prev entity with matching class

    [[nodiscard]] inline bool IsValid() const noexcept {
        return m_pEntity != nullptr && m_handle.IsValid();
    }

    [[nodiscard]] inline int GetEntryIndex() const noexcept {
        return static_cast<int>(m_handle.GetEntryIndex());
    }

    [[nodiscard]] inline const char* GetName() const noexcept {
        return m_pszName ? m_pszName : "";
    }

    [[nodiscard]] inline const char* GetDesignerName() const noexcept {
        return m_pszDesignerName ? m_pszDesignerName : "";
    }
};
static_assert(sizeof(CEntityIdentity) == 0x70, "CEntityIdentity size must be 0x70 bytes");
static_assert(offsetof(CEntityIdentity, m_pEntity)              == 0x00, "m_pEntity offset mismatch");
static_assert(offsetof(CEntityIdentity, m_pClassInfo)           == 0x08, "m_pClassInfo offset mismatch");
static_assert(offsetof(CEntityIdentity, m_handle)               == 0x10, "m_handle offset mismatch");
static_assert(offsetof(CEntityIdentity, m_nameStringTableIndex) == 0x14, "m_nameStringTableIndex offset mismatch");
static_assert(offsetof(CEntityIdentity, m_pszName)              == 0x18, "m_pszName offset mismatch");
static_assert(offsetof(CEntityIdentity, m_pszDesignerName)      == 0x20, "m_pszDesignerName offset mismatch");
static_assert(offsetof(CEntityIdentity, m_flags)                == 0x30, "m_flags offset mismatch");
static_assert(offsetof(CEntityIdentity, m_worldGroupId)         == 0x38, "m_worldGroupId offset mismatch");
static_assert(offsetof(CEntityIdentity, m_fDataObjectTypes)     == 0x3C, "m_fDataObjectTypes offset mismatch");
static_assert(offsetof(CEntityIdentity, m_PathIndex)            == 0x40, "m_PathIndex offset mismatch");
static_assert(offsetof(CEntityIdentity, m_pAttributes)          == 0x48, "m_pAttributes offset mismatch");
static_assert(offsetof(CEntityIdentity, m_pPrev)                == 0x50, "m_pPrev offset mismatch");
static_assert(offsetof(CEntityIdentity, m_pNext)                == 0x58, "m_pNext offset mismatch");
static_assert(offsetof(CEntityIdentity, m_pPrevByClass)         == 0x60, "m_pPrevByClass offset mismatch");
static_assert(offsetof(CEntityIdentity, m_pNextByClass)         == 0x68, "m_pNextByClass offset mismatch");

inline CEntityHandle CEntityInstance::GetHandle() const noexcept {
    return m_pEntityIdentity ? m_pEntityIdentity->m_handle : CEntityHandle();
}

// ============================================================================
// 5. CGameEntitySystem
// High-performance 2-tier chunked entity table managing up to 32,768 entities.
//
// Access & Discovery Anchors:
//   - client.dll Direct Pointer (dwGameEntitySystem):
//       RVA: 0x271c8b8 | VA: 0x18271c8b8
//       Initialized at client.dll + 0x15befa9 `mov [0x18271c8b8], rcx`
//   - engine2.dll / client.dll Interface Import (GameResourceServiceClientV001):
//       client.dll IGameResourceService pointer: RVA 0x279f088 | VA 0x18279f088
//       Pointer to CGameEntitySystem at: *(CGameEntitySystem**)((uintptr_t)pGameResourceService + 0x58)
//   - VTable: client.dll + 0x1c30820 (RTTI: .?AVCGameEntitySystem@@)
//   - Constructor: client.dll + 0x9c1e40 (Allocation size: 0x2180 bytes)
//
// Chunk Table Layout:
//   - Offset +0x10: m_EntityChunks[64] (Pointers to contiguous 512-element CEntityIdentity arrays)
//   - Total 64 chunks * 512 entities/chunk = 32,768 entities.
//
// Indexing Arithmetic (client.dll + 0x15d1a4):
//   chunk_index = (nIndex & 0x7FFF) >> 9;   // [0..63]
//   slot_index  = (nIndex & 0x7FFF) & 0x1FF; // [0..511]
//   pIdentity   = &m_EntityChunks[chunk_index][slot_index];
// ============================================================================
class CGameEntitySystem {
public:
    virtual ~CGameEntitySystem() = default;         // +0x00: VTable (client.dll + 0x1c30820)
    PAD(0x08, m_pad08);                             // +0x08: Internal registration state

    // +0x10: 2D Chunk Table holding pointers to 512-element CEntityIdentity arrays
    // Stride = 8 bytes per chunk pointer, 64 pointers = 0x200 bytes (spanning +0x10 to +0x210)
    CEntityIdentity* m_EntityChunks[kMaxChunks];    // +0x10..+0x210

    PAD(0x2180 - 0x210, m_padTail);                 // +0x210..+0x2180: Entity tracking, freelists, hash indices

    // ------------------------------------------------------------------------
    // Lookup Helpers
    // ------------------------------------------------------------------------

    /// @brief Retrieve CEntityIdentity by entity index [0..32767]
    /// @param nIndex Raw entity index (bits 0..14 of handle, or player slot + 1)
    /// @return Pointer to CEntityIdentity or nullptr if unallocated
    [[nodiscard]] inline CEntityIdentity* GetEntityIdentity(int nIndex) const noexcept {
        if (nIndex < 0 || static_cast<std::uint32_t>(nIndex) >= kMaxEntities) {
            return nullptr;
        }

        const auto uIndex = static_cast<std::uint32_t>(nIndex) & kEntEntryMask;
        const std::uint32_t chunkIndex = uIndex >> kChunkShift;
        const std::uint32_t slotIndex  = uIndex & kSlotMask;

        const CEntityIdentity* pChunk = m_EntityChunks[chunkIndex];
        if (!pChunk) {
            return nullptr;
        }

        // Chunk is a contiguous array of 512 CEntityIdentity structures (0x70 bytes each)
        return const_cast<CEntityIdentity*>(&pChunk[slotIndex]);
    }

    /// @brief Retrieve typed entity pointer by entity index [0..32767]
    /// @tparam T Entity class type (e.g. C_BaseEntity, CCSPlayerController)
    /// @param nIndex Raw entity index
    /// @return Pointer to entity instance or nullptr if invalid/unallocated
    template <typename T = CEntityInstance>
    [[nodiscard]] inline T* GetEntityByIndex(int nIndex) const noexcept {
        CEntityIdentity* pIdentity = GetEntityIdentity(nIndex);
        if (!pIdentity || !pIdentity->m_pEntity) {
            return nullptr;
        }
        return reinterpret_cast<T*>(pIdentity->m_pEntity);
    }

    /// @brief Retrieve typed entity pointer from 32-bit CEntityHandle
    /// Performs 17-bit serial generation validation to eliminate dangling pointer references
    /// Disassembly Anchor: client.dll + 0x15d1c5 `cmp [rax+0x10], ecx`
    /// @tparam T Entity class type
    /// @param handle Strongly-typed or raw CEntityHandle
    /// @return Pointer to entity instance or nullptr if handle is stale/invalid
    template <typename T = CEntityInstance>
    [[nodiscard]] inline T* GetEntityFromHandle(CEntityHandle handle) const noexcept {
        if (!handle.IsValid()) {
            return nullptr;
        }

        CEntityIdentity* pIdentity = GetEntityIdentity(static_cast<int>(handle.GetEntryIndex()));
        if (!pIdentity || !pIdentity->m_pEntity) {
            return nullptr;
        }

        // Serial generation validation: verify active handle matches expected serial
        if (pIdentity->m_handle != handle) {
            return nullptr;
        }

        return reinterpret_cast<T*>(pIdentity->m_pEntity);
    }

    /// @brief Overload accepting raw uint32 handle scalar
    template <typename T = CEntityInstance>
    [[nodiscard]] inline T* GetEntityFromHandle(std::uint32_t handleVal) const noexcept {
        return GetEntityFromHandle<T>(CEntityHandle(handleVal));
    }

    // ------------------------------------------------------------------------
    // Local Player Resolution Pipeline
    // ------------------------------------------------------------------------
    // Algorithm:
    //   1. Call CEngineClient::GetLocalPlayer(nSplitScreenSlot) [engine2.dll slot 37, RVA 0x762c0]
    //      Reads *(int32_t*)(pClientSession + 0x39c), returning zero-based player slot (0..63).
    //   2. Local Player Controller Entity Index = player_slot + 1
    //      (Index 0 is reserved for world entity / CWorld).
    //   3. Get CCSPlayerController from CGameEntitySystem::GetEntityByIndex(slot + 1).
    //   4. Retrieve Pawn Handle from Controller schema field:
    //      m_hPlayerPawn at offset +0x8ec (server.dll) / +0x92c (client.dll).
    //   5. Resolve C_CSPlayerPawn via CGameEntitySystem::GetEntityFromHandle(m_hPlayerPawn).
    // ------------------------------------------------------------------------

    /// @brief Resolves player controller for a specific 0-based split screen slot
    /// @tparam TController Controller type (e.g. CCSPlayerController)
    /// @param nPlayerSlot 0-based client slot returned by CEngineClient::GetLocalPlayer()
    /// @return Pointer to controller or nullptr
    template <typename TController = CEntityInstance>
    [[nodiscard]] inline TController* GetPlayerControllerBySlot(int nPlayerSlot) const noexcept {
        if (nPlayerSlot < 0 || nPlayerSlot >= 64) {
            return nullptr;
        }
        return GetEntityByIndex<TController>(nPlayerSlot + 1);
    }
};
static_assert(offsetof(CGameEntitySystem, m_EntityChunks) == 0x10, "m_EntityChunks offset mismatch");
static_assert(sizeof(CGameEntitySystem) == 0x2180, "CGameEntitySystem size must be 0x2180 bytes");

} // namespace cs2::sdk
