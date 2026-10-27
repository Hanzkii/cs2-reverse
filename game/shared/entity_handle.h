#ifndef CS2_SHARED_ENTITY_HANDLE_H
#define CS2_SHARED_ENTITY_HANDLE_H

#include <cstdint>

namespace cs2::shared {

constexpr std::uint32_t ENT_ENTRY_MASK        = 0x7FFF;
constexpr std::uint32_t INVALID_EHANDLE_INDEX = 0x7FFF;
constexpr std::uint32_t INVALID_EHANDLE_VAL   = 0xFFFFFFFF;

/**
 * @brief Source 2 32-bit Packed Entity Handle (CEntityHandle)
 * Reconstructed from client.dll @ 0x1807357c0 (GetEntityIdentity).
 * Decomposition:
 *   - Bits 0..14:  15-bit entry index [0..32,767]
 *   - Bits 15..31: 17-bit serial counter (incremented upon entity recycling)
 */
struct CEntityHandle {
    std::uint32_t m_uValue{INVALID_EHANDLE_VAL};

    constexpr CEntityHandle() noexcept = default;
    constexpr explicit CEntityHandle(std::uint32_t val) noexcept : m_uValue(val) {}
    constexpr CEntityHandle(std::uint32_t index, std::uint32_t serial) noexcept
        : m_uValue((index & ENT_ENTRY_MASK) | (serial << 15)) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return m_uValue != INVALID_EHANDLE_VAL && GetEntryIndex() != INVALID_EHANDLE_INDEX;
    }

    [[nodiscard]] constexpr std::uint32_t GetRaw() const noexcept {
        return m_uValue;
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
    constexpr bool operator<(const CEntityHandle& o) const noexcept { return m_uValue < o.m_uValue; }
};
static_assert(sizeof(CEntityHandle) == 4, "CEntityHandle must be 4 bytes");

/**
 * @brief Strongly typed entity handle template (CHandle<T>)
 */
template <typename T>
class CHandle : public CEntityHandle {
public:
    using CEntityHandle::CEntityHandle;
};
static_assert(sizeof(CHandle<void>) == 4, "CHandle<T> must be 4 bytes");

} // namespace cs2::shared

#endif // CS2_SHARED_ENTITY_HANDLE_H
