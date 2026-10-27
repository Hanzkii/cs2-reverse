#ifndef CS2_CLIENT_ENTITY_SYSTEM_H
#define CS2_CLIENT_ENTITY_SYSTEM_H

#include <cstdint>
#include <cstddef>
#include "../shared/entity_handle.h"

namespace cs2::client {

#define CS2_PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

constexpr std::uint32_t MAX_TOTAL_ENTITIES   = 32768; // 64 chunks * 512 slots
constexpr std::uint32_t ENTITIES_PER_CHUNK   = 512;   // 0x200 slots per chunk
constexpr std::uint32_t MAX_CHUNKS           = 64;    // 0x40 chunks in table

/**
 * @brief Authoritative Entity Identity Descriptor (CEntityIdentity)
 * Size: exactly 112 bytes (0x70), alignas(8).
 * Cacheline-aligned metadata container residing directly inside the 2D chunk array.
 */
struct alignas(8) CEntityIdentity {
    void*                 m_pEntity{nullptr};        // +0x00: Active C_BaseEntity pointer (null if slot free)
    void*                 m_pClassInfo{nullptr};     // +0x08: Runtime CSchemaClassInfo pointer
    shared::CEntityHandle m_entityHandle{};          // +0x10: 32-bit handle (15-bit index, 17-bit serial)
    std::uint32_t         m_flags{0};                // +0x14: Flags (0x02 = dormant, 0x04 = queued deletion)
    CS2_PAD(0x40, m_pad18);                          // +0x18: Internal entity identity state
    CEntityIdentity*      m_pPrev{nullptr};          // +0x58: Singly linked bucket pointer
    CEntityIdentity*      m_pNext{nullptr};          // +0x60: Singly linked bucket pointer
    CS2_PAD(0x08, m_pad68);                          // +0x68

    [[nodiscard]] constexpr bool IsDormant() const noexcept { return (m_flags & 0x02) != 0; }
    [[nodiscard]] constexpr bool IsQueuedForDeletion() const noexcept { return (m_flags & 0x04) != 0; }
    [[nodiscard]] constexpr bool IsActive() const noexcept { return m_pEntity != nullptr && !IsDormant() && !IsQueuedForDeletion(); }
};
static_assert(sizeof(CEntityIdentity) == 0x70, "CEntityIdentity must be exactly 0x70 (112 bytes)");

/**
 * @brief Source 2 Entity System Master Singleton (CGameEntitySystem)
 * Total Size: 8,576 bytes (0x2180).
 * Global Pointer in client.dll: client.dll + 0x271c8b8.
 */
class CGameEntitySystem {
public:
    virtual ~CGameEntitySystem() = default;

    // Slot 1: Release
    virtual void Release() = 0;

    // Slot 2: GetEntityIdentity (RVA: +0x007357c0, VA: 0x1807357c0)
    virtual CEntityIdentity* GetEntityIdentity(int nIndex) = 0;

    // Slot 3: GetEntityByIndex
    virtual void* GetEntityByIndex(int nIndex) = 0;

    CS2_PAD(0x08, m_pad0);
    // +0x10: 2D Chunk Page-Table (64 pointers to 512-slot CEntityIdentity pages)
    CEntityIdentity* m_EntityChunks[MAX_CHUNKS]{};
    CS2_PAD(0x2170 - (MAX_CHUNKS * sizeof(void*)), m_padChunks);

    /**
     * @brief Resolves a 32-bit entity handle into a CEntityIdentity pointer.
     * Matches client.dll @ 0x1807357c0 O(1) resolution algorithm.
     */
    [[nodiscard]] CEntityIdentity* ResolveIdentity(shared::CEntityHandle handle) const noexcept;

    /**
     * @brief Resolves an entity pointer by handle, verifying that serial numbers match.
     */
    [[nodiscard]] void* ResolveEntity(shared::CEntityHandle handle) const noexcept;
};

#undef CS2_PAD

} // namespace cs2::client

#endif // CS2_CLIENT_ENTITY_SYSTEM_H
