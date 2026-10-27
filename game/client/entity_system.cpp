#include "entity_system.h"

namespace cs2::client {

CEntityIdentity* CGameEntitySystem::ResolveIdentity(shared::CEntityHandle handle) const noexcept {
    if (!handle.IsValid()) return nullptr;

    const std::uint32_t index = handle.GetEntryIndex();
    if (index >= MAX_TOTAL_ENTITIES) return nullptr;

    const std::uint32_t chunkIdx = index >> 9; // index / 512
    const std::uint32_t slotIdx  = index & 0x1FF; // index % 512

    if (chunkIdx >= MAX_CHUNKS) return nullptr;

    CEntityIdentity* pChunk = m_EntityChunks[chunkIdx];
    if (!pChunk) return nullptr;

    return &pChunk[slotIdx];
}

void* CGameEntitySystem::ResolveEntity(shared::CEntityHandle handle) const noexcept {
    CEntityIdentity* pIdentity = ResolveIdentity(handle);
    if (!pIdentity) return nullptr;

    // Serial verification: ensure entity slot hasn't been recycled
    if (pIdentity->m_entityHandle.GetSerialNumber() != handle.GetSerialNumber()) {
        return nullptr; // Stale handle
    }

    if (!pIdentity->IsActive()) {
        return nullptr; // Dormant or queued for deletion
    }

    return pIdentity->m_pEntity;
}

} // namespace cs2::client
