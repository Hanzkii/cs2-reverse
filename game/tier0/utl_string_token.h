#ifndef CS2_TIER0_UTL_STRING_TOKEN_H
#define CS2_TIER0_UTL_STRING_TOKEN_H

#include <cstdint>
#include <string_view>

namespace cs2::tier0 {

/**
 * @brief 32-bit MurmurHash2 / FNV-1a Hashed String Identifier (CUtlStringToken)
 * Used extensively throughout Source 2 (schemasystem.dll, client.dll, server.dll)
 * to avoid runtime string comparisons.
 * Size: exactly 4 bytes.
 */
class CUtlStringToken {
public:
    std::uint32_t m_nHashCode{0};

    constexpr CUtlStringToken() noexcept = default;
    constexpr explicit CUtlStringToken(std::uint32_t nCode) noexcept : m_nHashCode(nCode) {}

    // Compile-time or runtime string hashing via MurmurHash2
    constexpr explicit CUtlStringToken(std::string_view str) noexcept
        : m_nHashCode(Hash(str)) {}

    constexpr explicit CUtlStringToken(const char* pszString) noexcept
        : m_nHashCode(pszString ? Hash(std::string_view(pszString)) : 0) {}

    [[nodiscard]] constexpr std::uint32_t Get() const noexcept { return m_nHashCode; }
    [[nodiscard]] constexpr bool IsValid() const noexcept { return m_nHashCode != 0; }

    constexpr bool operator==(const CUtlStringToken& o) const noexcept { return m_nHashCode == o.m_nHashCode; }
    constexpr bool operator!=(const CUtlStringToken& o) const noexcept { return m_nHashCode != o.m_nHashCode; }
    constexpr bool operator<(const CUtlStringToken& o) const noexcept { return m_nHashCode < o.m_nHashCode; }

    /**
     * @brief Source 2 Authoritative MurmurHash2 implementation
     * Matches schemasystem.dll @ 0x180010380
     */
    static constexpr std::uint32_t Hash(std::string_view str) noexcept {
        const std::uint32_t len = static_cast<std::uint32_t>(str.length());
        if (len == 0) return 0;

        constexpr std::uint32_t m = 0x5bd1e995;
        constexpr int r = 24;

        std::uint32_t h = len ^ 0xbaadfeed;
        const char* data = str.data();
        std::uint32_t remaining = len;

        while (remaining >= 4) {
            std::uint32_t k = static_cast<std::uint8_t>(data[0]) |
                             (static_cast<std::uint8_t>(data[1]) << 8) |
                             (static_cast<std::uint8_t>(data[2]) << 16) |
                             (static_cast<std::uint8_t>(data[3]) << 24);

            k *= m;
            k ^= k >> r;
            k *= m;

            h *= m;
            h ^= k;

            data += 4;
            remaining -= 4;
        }

        switch (remaining) {
            case 3: h ^= static_cast<std::uint8_t>(data[2]) << 16; [[fallthrough]];
            case 2: h ^= static_cast<std::uint8_t>(data[1]) << 8;  [[fallthrough]];
            case 1: h ^= static_cast<std::uint8_t>(data[0]);
                    h *= m;
                    break;
            default: break;
        }

        h ^= h >> 13;
        h *= m;
        h ^= h >> 15;

        return h;
    }
};

static_assert(sizeof(CUtlStringToken) == 4, "CUtlStringToken must be 4 bytes");

} // namespace cs2::tier0

#endif // CS2_TIER0_UTL_STRING_TOKEN_H
