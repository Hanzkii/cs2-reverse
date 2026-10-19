#ifndef CS2_NETMESSAGE_FRAMING_H
#define CS2_NETMESSAGE_FRAMING_H

#include <cstdint>
#include <cstddef>
#include <optional>
#include <vector>

namespace cs2::networksystem {

/**
 * @brief Source 2 NetMessage Header parsed from wire bitstream.
 * Reconstructed from networksystem.dll (Wire parser FUN_1800201a0).
 */
struct NetMessageHeader {
    uint32_t messageId{0};
    uint32_t byteSize{0};
    size_t headerBitSize{0};
};

class CBitReadStream {
public:
    explicit CBitReadStream(const uint8_t* data, size_t bitCount);
    CBitReadStream(const uint8_t* data, size_t byteCount, bool isByteCount);

    [[nodiscard]] uint32_t ReadUBits(size_t numBits);
    [[nodiscard]] uint32_t ReadVarInt32();
    [[nodiscard]] bool ReadBytes(uint8_t* out, size_t byteCount);

    [[nodiscard]] size_t GetBitsRead() const noexcept { return m_BitOffset; }
    [[nodiscard]] size_t GetTotalBits() const noexcept { return m_TotalBits; }
    [[nodiscard]] bool HasOverflowed() const noexcept { return m_Overflow; }

private:
    const uint8_t* m_Data;
    size_t m_TotalBits;
    size_t m_BitOffset{0};
    bool m_Overflow{false};
};

class CBitWriteStream {
public:
    CBitWriteStream();

    void WriteUBits(uint32_t value, size_t numBits);
    void WriteVarInt32(uint32_t value);
    void WriteBytes(const uint8_t* data, size_t byteCount);

    [[nodiscard]] const std::vector<uint8_t>& GetData() const noexcept { return m_Buffer; }
    [[nodiscard]] size_t GetBitOffset() const noexcept { return m_BitOffset; }

private:
    std::vector<uint8_t> m_Buffer;
    size_t m_BitOffset{0};
};

/**
 * @brief NetMessage framing codec reconstructing networksystem.dll protocol.
 */
class CNetMessageFraming {
public:
    /**
     * @brief Decodes a variable-bit header (6/10/14/34 bits) + VarInt32 payload size.
     */
    static std::optional<NetMessageHeader> ReadHeader(CBitReadStream& stream);

    /**
     * @brief Encodes message ID with optimal variable-bit prefix and VarInt32 size.
     */
    static void WriteHeader(CBitWriteStream& stream, uint32_t messageId, uint32_t byteSize);
};

} // namespace cs2::networksystem

#endif // CS2_NETMESSAGE_FRAMING_H
