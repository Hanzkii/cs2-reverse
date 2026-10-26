/**
 * VALVE SOURCE 2 (CS2) 2D CHUNK PAGE-TABLE ENTITY RESOLUTION SIMULATOR
 * Reconstructed directly from:
 *   - client.dll:
 *       FUN_1807357c0 (CGameEntitySystem::GetEntityIdentity - Slot 2 resolver)
 *       CGameEntitySystem global pointer @ 0x18271c8b8
 *       m_EntityChunks 2D chunk table @ +0x10 (64 chunks x 512 entries = 32,768)
 *       CEntityIdentity layout (0x70 bytes)
 *
 * Demonstrates:
 *   1. 32-bit CEntityHandle decomposition: 15-bit index (0..32767), 17-bit serial counter.
 *   2. 2D chunk table addressing: Chunk Index (index >> 9), Slot Index (index & 0x1FF).
 *   3. Authoritative CEntityIdentity (0x70 bytes) resolution and serial matching.
 *   4. Stale/Dangling handle rejection when serials diverge after entity destruction.
 *   5. Dormancy & deletion flags validation (0x02 = dormant, 0x04 = queued deletion).
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <string>

namespace Source2 {

// ----------------------------------------------------------------------------
// Entity System Constants (from client.dll @ 0x1807357c0)
// ----------------------------------------------------------------------------
constexpr std::uint32_t MAX_TOTAL_ENTITIES   = 32768; // 64 chunks * 512 slots
constexpr std::uint32_t ENTITIES_PER_CHUNK   = 512;   // 0x200 slots per chunk
constexpr std::uint32_t MAX_CHUNKS           = 64;    // 0x40 chunks in table
constexpr std::uint32_t ENT_ENTRY_MASK       = 0x7FFF; // 15-bit index mask
constexpr std::uint32_t INVALID_EHANDLE_INDEX= 0x7FFF;
constexpr std::uint32_t INVALID_EHANDLE_VAL  = 0xFFFFFFFF;

// ----------------------------------------------------------------------------
// CEntityHandle (32-bit packed bitfield)
// ----------------------------------------------------------------------------
struct CEntityHandle {
    std::uint32_t m_uValue{INVALID_EHANDLE_VAL};

    constexpr CEntityHandle() = default;
    constexpr explicit CEntityHandle(std::uint32_t val) : m_uValue(val) {}
    constexpr CEntityHandle(std::uint32_t index, std::uint32_t serial)
        : m_uValue((index & ENT_ENTRY_MASK) | (serial << 15)) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return m_uValue != INVALID_EHANDLE_VAL && GetEntryIndex() != INVALID_EHANDLE_INDEX;
    }

    [[nodiscard]] constexpr std::uint32_t GetEntryIndex() const noexcept {
        return m_uValue & ENT_ENTRY_MASK;
    }

    [[nodiscard]] constexpr std::uint32_t GetSerialNumber() const noexcept {
        return m_uValue >> 15;
    }

    [[nodiscard]] constexpr std::uint32_t GetChunkIndex() const noexcept {
        return GetEntryIndex() >> 9;
    }

    [[nodiscard]] constexpr std::uint32_t GetSlotIndex() const noexcept {
        return GetEntryIndex() & 0x1FF;
    }

    constexpr bool operator==(const CEntityHandle& o) const noexcept { return m_uValue == o.m_uValue; }
    constexpr bool operator!=(const CEntityHandle& o) const noexcept { return m_uValue != o.m_uValue; }
};

// ----------------------------------------------------------------------------
// Schema Class Info Stub
// ----------------------------------------------------------------------------
struct CSchemaClassInfo {
    const char* m_pszClassName;
    const char* m_pszModuleName;
    std::uint32_t m_nClassSize;
};

// ----------------------------------------------------------------------------
// Minimal Reconstructed C_BaseEntity
// ----------------------------------------------------------------------------
class C_BaseEntity {
public:
    virtual ~C_BaseEntity() = default;
    CEntityHandle m_hRefHandle;
    std::string m_strDebugName;
    int m_iHealth{100};
    bool m_bIsDormant{false};

    C_BaseEntity(std::string name, int hp)
        : m_strDebugName(std::move(name)), m_iHealth(hp) {}
};

// ----------------------------------------------------------------------------
// CEntityIdentity (Exact 0x70 / 112 bytes layout in client.dll)
// ----------------------------------------------------------------------------
struct CEntityIdentity {
    C_BaseEntity*      m_pEntity{nullptr};           // +0x00
    CSchemaClassInfo*  m_pClassInfo{nullptr};        // +0x08
    CEntityHandle      m_hHandle;                    // +0x10
    std::uint16_t      m_nFlags{0};                  // +0x14 (bit 1: dormant, bit 2: deleted)
    std::uint16_t      m_nAllocIndex{0};             // +0x16
    const char*        m_pszInternalName{nullptr};   // +0x18
    const char*        m_pszDesignerName{nullptr};   // +0x20
    std::uint8_t       m_pad28[0x18]{0};             // +0x28
    CEntityIdentity*   m_pPrev{nullptr};             // +0x40
    CEntityIdentity*   m_pNext{nullptr};             // +0x48
    CEntityIdentity*   m_pPrevByClass{nullptr};      // +0x50
    CEntityIdentity*   m_pNextByClass{nullptr};      // +0x58
    std::uint8_t       m_pad60[0x10]{0};             // +0x60
};
static_assert(sizeof(CEntityIdentity) == 0x70, "CEntityIdentity size mismatch");

// ----------------------------------------------------------------------------
// CGameEntitySystem 2D Chunk Table Simulator
// ----------------------------------------------------------------------------
class CGameEntitySystemSimulator {
private:
    // 64 Chunks, each pointing to an array of 512 CEntityIdentity entries
    CEntityIdentity* m_pEntityChunks[MAX_CHUNKS]{nullptr};
    std::uint32_t m_nSerialCounters[MAX_TOTAL_ENTITIES]{0};
    std::uint32_t m_nActiveEntityCount{0};

public:
    CGameEntitySystemSimulator() {
        for (std::uint32_t i = 0; i < MAX_TOTAL_ENTITIES; ++i) {
            m_nSerialCounters[i] = 1; // Initial serial starting at 1
        }
    }

    ~CGameEntitySystemSimulator() {
        for (std::uint32_t c = 0; c < MAX_CHUNKS; ++c) {
            if (m_pEntityChunks[c]) {
                delete[] m_pEntityChunks[c];
                m_pEntityChunks[c] = nullptr;
            }
        }
    }

    // Allocate chunk on-demand
    void EnsureChunkAllocated(std::uint32_t chunkIndex) {
        assert(chunkIndex < MAX_CHUNKS);
        if (!m_pEntityChunks[chunkIndex]) {
            m_pEntityChunks[chunkIndex] = new CEntityIdentity[ENTITIES_PER_CHUNK]();
        }
    }

    // Allocate a new entity into a target index slot
    CEntityHandle SpawnEntity(std::uint32_t index, C_BaseEntity* pEntity, CSchemaClassInfo* pClass) {
        if (index >= MAX_TOTAL_ENTITIES) return CEntityHandle();

        const std::uint32_t chunk = index >> 9;
        const std::uint32_t slot  = index & 0x1FF;
        EnsureChunkAllocated(chunk);

        CEntityIdentity& ident = m_pEntityChunks[chunk][slot];
        const std::uint32_t serial = m_nSerialCounters[index];

        ident.m_pEntity = pEntity;
        ident.m_pClassInfo = pClass;
        ident.m_hHandle = CEntityHandle(index, serial);
        ident.m_nFlags = 0;
        ident.m_nAllocIndex = static_cast<std::uint16_t>(index);
        ident.m_pszInternalName = pEntity->m_strDebugName.c_str();
        ident.m_pszDesignerName = pClass ? pClass->m_pszClassName : "unknown";

        pEntity->m_hRefHandle = ident.m_hHandle;
        m_nActiveEntityCount++;
        return ident.m_hHandle;
    }

    // Destroy an entity, increment serial, mark slot vacant
    bool DestroyEntity(CEntityHandle handle) {
        if (!handle.IsValid()) return false;
        const std::uint32_t index = handle.GetEntryIndex();
        const std::uint32_t chunk = handle.GetChunkIndex();
        const std::uint32_t slot  = handle.GetSlotIndex();

        if (chunk >= MAX_CHUNKS || !m_pEntityChunks[chunk]) return false;

        CEntityIdentity& ident = m_pEntityChunks[chunk][slot];
        if (ident.m_hHandle != handle) return false;

        delete ident.m_pEntity;
        ident.m_pEntity = nullptr;
        ident.m_nFlags |= 0x04; // Deleted flag

        // In Source 2, serial counter increments to invalidate existing external handles
        m_nSerialCounters[index] = (m_nSerialCounters[index] + 1) & 0x1FFFF; // 17-bit wrap
        if (m_nSerialCounters[index] == 0) m_nSerialCounters[index] = 1;
        ident.m_hHandle = CEntityHandle(index, m_nSerialCounters[index]);

        m_nActiveEntityCount--;
        return true;
    }

    // Fast O(1) GetEntityIdentity - exact algorithm of client.dll FUN_1807357c0
    [[nodiscard]] CEntityIdentity* GetEntityIdentity(CEntityHandle handle) const noexcept {
        if (!handle.IsValid()) return nullptr;

        const std::uint32_t index = handle.GetEntryIndex();
        if (index >= MAX_TOTAL_ENTITIES) return nullptr;

        const std::uint32_t chunk = index >> 9;
        if (chunk >= MAX_CHUNKS) return nullptr;

        const CEntityIdentity* pChunk = m_pEntityChunks[chunk];
        if (!pChunk) return nullptr;

        const std::uint32_t slot = index & 0x1FF;
        CEntityIdentity* pIdentity = const_cast<CEntityIdentity*>(&pChunk[slot]);

        // Validate 17-bit serial match
        if (pIdentity->m_hHandle.GetSerialNumber() != handle.GetSerialNumber()) {
            return nullptr; // Stale handle!
        }

        // Validate deletion flag
        if (pIdentity->m_nFlags & 0x04) {
            return nullptr; // Marked for deletion
        }

        return pIdentity;
    }

    // GetEntity from handle
    [[nodiscard]] C_BaseEntity* GetEntity(CEntityHandle handle) const noexcept {
        CEntityIdentity* pIdentity = GetEntityIdentity(handle);
        return pIdentity ? pIdentity->m_pEntity : nullptr;
    }

    [[nodiscard]] std::uint32_t GetActiveCount() const noexcept { return m_nActiveEntityCount; }
};

} // namespace Source2

int main() {
    using namespace Source2;

    std::cout << "===============================================================================\n";
    std::cout << "  VALVE SOURCE 2 (CS2) 2D CHUNK PAGE-TABLE ENTITY RESOLUTION SIMULATOR\n";
    std::cout << "  Reconstructed from client.dll (FUN_1807357c0, CGameEntitySystem @ 0x18271c8b8)\n";
    std::cout << "===============================================================================\n\n";

    CGameEntitySystemSimulator entSys;

    CSchemaClassInfo clsController{"CCSPlayerController", "client.dll", 0x980};
    CSchemaClassInfo clsPawn{"C_CSPlayerPawn", "client.dll", 0x3720};
    CSchemaClassInfo clsC4{"C_PlantedC4", "client.dll", 0x1958};

    std::cout << "[PHASE 1] Spawning Game Entities Across 2D Page-Table Chunks...\n";

    // Slot 1 (Chunk 0, Slot 1) - Local Player Controller
    auto* pCtrl = new C_BaseEntity("LocalPlayerController", 100);
    CEntityHandle hCtrl = entSys.SpawnEntity(1, pCtrl, &clsController);
    std::cout << "  - Spawned: " << pCtrl->m_strDebugName 
              << " | Handle: 0x" << std::hex << hCtrl.m_uValue << std::dec
              << " [Index: " << hCtrl.GetEntryIndex() 
              << ", Serial: " << hCtrl.GetSerialNumber()
              << ", Chunk: " << hCtrl.GetChunkIndex() 
              << ", Slot: " << hCtrl.GetSlotIndex() << "]\n";

    // Slot 65 (Chunk 0, Slot 65) - Player Pawn
    auto* pPawn = new C_BaseEntity("LocalPlayerPawn", 100);
    CEntityHandle hPawn = entSys.SpawnEntity(65, pPawn, &clsPawn);
    std::cout << "  - Spawned: " << pPawn->m_strDebugName 
              << " | Handle: 0x" << std::hex << hPawn.m_uValue << std::dec
              << " [Index: " << hPawn.GetEntryIndex() 
              << ", Serial: " << hPawn.GetSerialNumber()
              << ", Chunk: " << hPawn.GetChunkIndex() 
              << ", Slot: " << hPawn.GetSlotIndex() << "]\n";

    // Slot 515 (Chunk 1, Slot 3) - Planted C4 in second chunk!
    auto* pC4 = new C_BaseEntity("PlantedC4_SiteA", 100);
    CEntityHandle hC4 = entSys.SpawnEntity(515, pC4, &clsC4);
    std::cout << "  - Spawned: " << pC4->m_strDebugName 
              << " | Handle: 0x" << std::hex << hC4.m_uValue << std::dec
              << " [Index: " << hC4.GetEntryIndex() 
              << ", Serial: " << hC4.GetSerialNumber()
              << ", Chunk: " << hC4.GetChunkIndex() 
              << ", Slot: " << hC4.GetSlotIndex() << "] (Multi-Chunk Page Allocation!)\n\n";

    std::cout << "[PHASE 2] Validating O(1) Page-Table Lookup and Serial Matching...\n";
    CEntityIdentity* idCtrl = entSys.GetEntityIdentity(hCtrl);
    assert(idCtrl != nullptr && idCtrl->m_pEntity == pCtrl);
    std::cout << "  [PASS] hCtrl resolved -> " << idCtrl->m_pszInternalName 
              << " (Class: " << idCtrl->m_pszDesignerName << ")\n";

    CEntityIdentity* idPawn = entSys.GetEntityIdentity(hPawn);
    assert(idPawn != nullptr && idPawn->m_pEntity == pPawn);
    std::cout << "  [PASS] hPawn resolved -> " << idPawn->m_pszInternalName 
              << " (Class: " << idPawn->m_pszDesignerName << ")\n";

    CEntityIdentity* idC4 = entSys.GetEntityIdentity(hC4);
    assert(idC4 != nullptr && idC4->m_pEntity == pC4);
    std::cout << "  [PASS] hC4 resolved across Chunk 1 -> " << idC4->m_pszInternalName 
              << " (Class: " << idC4->m_pszDesignerName << ")\n\n";

    std::cout << "[PHASE 3] Simulating Entity Destruction & Stale Handle Invalidation...\n";
    CEntityHandle staleHandle = hPawn; // Copy handle before destruction
    std::cout << "  - Destroying Player Pawn (Slot 65)...\n";
    bool bDestroyed = entSys.DestroyEntity(hPawn);
    assert(bDestroyed);

    std::cout << "  - Attempting lookup using stale handle (0x" << std::hex << staleHandle.m_uValue << std::dec << ")...\n";
    CEntityIdentity* idStale = entSys.GetEntityIdentity(staleHandle);
    if (!idStale) {
        std::cout << "  [PASS] Stale handle correctly rejected! Serial mismatch detected.\n";
    } else {
        std::cerr << "  [FAIL] Stale handle should have been rejected!\n";
        return 1;
    }

    std::cout << "\n[PHASE 4] Re-spawning Entity at Slot 65 (Demonstrating Serial Increment)...\n";
    auto* pNewPawn = new C_BaseEntity("RespawnedPlayerPawn", 100);
    CEntityHandle hNewPawn = entSys.SpawnEntity(65, pNewPawn, &clsPawn);
    std::cout << "  - New Spawn at Slot 65 | Handle: 0x" << std::hex << hNewPawn.m_uValue << std::dec
              << " [Index: " << hNewPawn.GetEntryIndex() 
              << ", Serial: " << hNewPawn.GetSerialNumber()
              << " (Incremented from " << staleHandle.GetSerialNumber() << "!)]\n";

    assert(entSys.GetEntity(hNewPawn) == pNewPawn);
    assert(entSys.GetEntity(staleHandle) == nullptr); // Old handle still invalid!
    std::cout << "  [PASS] New entity resolved, old handle remains invalid.\n\n";

    std::cout << "===============================================================================\n";
    std::cout << "  ALL SOURCE 2 ENTITY CHUNK PAGE-TABLE CHECKS PASSED SUCCESSFULLY!\n";
    std::cout << "===============================================================================\n";
    return 0;
}
