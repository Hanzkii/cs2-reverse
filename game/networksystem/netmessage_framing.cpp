#include "netmessage_framing.h"
#include <cstring>
#include <algorithm>

namespace cs2::networksystem {

CBitReadStream::CBitReadStream(const uint8_t* data, size_t bitCount)
    : m_Data(data), m_TotalBits(bitCount) {}

CBitReadStream::CBitReadStream(const uint8_t* data, size_t byteCount, bool /*isByteCount*/)
    : m_Data(data), m_TotalBits(byteCount * 8) {}

uint32_t CBitReadStream::ReadUBits(size_t numBits) {
    if (m_BitOffset + numBits > m_TotalBits || numBits > 32) {
        m_Overflow = true;
        return 0;
    }

    uint32_t result = 0;
    for (size_t i = 0; i < numBits; ++i) {
        const size_t currentBit = m_BitOffset + i;
        const size_t byteIdx = currentBit / 8;
        const size_t bitIdx = currentBit % 8;
        if ((m_Data[byteIdx] >> bitIdx) & 1) {
            result |= (1u << i);
        }
    }

    m_BitOffset += numBits;
    return result;
}

uint32_t CBitReadStream::ReadVarInt32() {
    uint32_t result = 0;
    int shift = 0;

    for (int i = 0; i < 5; ++i) {
        if (m_BitOffset + 8 > m_TotalBits) {
            m_Overflow = true;
            return 0;
        }

        uint32_t byteVal = ReadUBits(8);
        result |= (byteVal & 0x7F) << shift;
        if ((byteVal & 0x80) == 0) {
            return result;
        }
        shift += 7;
    }

    m_Overflow = true;
    return 0;
}

bool CBitReadStream::ReadBytes(uint8_t* out, size_t byteCount) {
    if (m_BitOffset + (byteCount * 8) > m_TotalBits) {
        m_Overflow = true;
        return false;
    }

    for (size_t b = 0; b < byteCount; ++b) {
        out[b] = static_cast<uint8_t>(ReadUBits(8));
    }
    return true;
}

CBitWriteStream::CBitWriteStream() {
    m_Buffer.reserve(128);
}

void CBitWriteStream::WriteUBits(uint32_t value, size_t numBits) {
    for (size_t i = 0; i < numBits; ++i) {
        const size_t byteIdx = (m_BitOffset + i) / 8;
        const size_t bitIdx = (m_BitOffset + i) % 8;

        if (byteIdx >= m_Buffer.size()) {
            m_Buffer.resize(byteIdx + 1, 0);
        }

        if ((value >> i) & 1) {
            m_Buffer[byteIdx] |= static_cast<uint8_t>(1u << bitIdx);
        }
    }
    m_BitOffset += numBits;
}

void CBitWriteStream::WriteVarInt32(uint32_t value) {
    while (value >= 0x80) {
        WriteUBits((value & 0x7F) | 0x80, 8);
        value >>= 7;
    }
    WriteUBits(value & 0x7F, 8);
}

void CBitWriteStream::WriteBytes(const uint8_t* data, size_t byteCount) {
    for (size_t i = 0; i < byteCount; ++i) {
        WriteUBits(data[i], 8);
    }
}

std::optional<NetMessageHeader> CNetMessageFraming::ReadHeader(CBitReadStream& stream) {
    const size_t startBits = stream.GetBitsRead();

    // 2-bit prefix selector
    const uint32_t prefix = stream.ReadUBits(2);
    if (stream.HasOverflowed()) return std::nullopt;

    uint32_t messageId = 0;
    switch (prefix) {
    case 0: // 00b: 4 additional bits (6 total) -> IDs 0..15
        messageId = stream.ReadUBits(4);
        break;
    case 1: // 01b: 8 additional bits (10 total) -> IDs 16..271
        messageId = stream.ReadUBits(8) + 16;
        break;
    case 2: // 10b: 12 additional bits (14 total) -> IDs 272..4367
        messageId = stream.ReadUBits(12) + 272;
        break;
    case 3: // 11b: 32 additional bits (34 total) -> Extended IDs
        messageId = stream.ReadUBits(32);
        break;
    }

    if (stream.HasOverflowed()) return std::nullopt;

    const uint32_t byteSize = stream.ReadVarInt32();
    if (stream.HasOverflowed()) return std::nullopt;

    const size_t headerBitSize = stream.GetBitsRead() - startBits;
    return NetMessageHeader{messageId, byteSize, headerBitSize};
}

void CNetMessageFraming::WriteHeader(CBitWriteStream& stream, uint32_t messageId, uint32_t byteSize) {
    if (messageId < 16) {
        stream.WriteUBits(0, 2);
        stream.WriteUBits(messageId, 4);
    } else if (messageId < 272) {
        stream.WriteUBits(1, 2);
        stream.WriteUBits(messageId - 16, 8);
    } else if (messageId < 4368) {
        stream.WriteUBits(2, 2);
        stream.WriteUBits(messageId - 272, 12);
    } else {
        stream.WriteUBits(3, 2);
        stream.WriteUBits(messageId, 32);
    }

    stream.WriteVarInt32(byteSize);
}

} // namespace cs2::networksystem
