/**
 * VALVE SOURCE 2 (CS2) NETWORK FRAMING & SERIALIZATION SIMULATOR
 * Reconstructed directly from: networksystem.dll (64-bit)
 * Target Build: 2026-11-11_26598758 | Architecture: x64 | Standard: C++20
 *
 * Demonstrates:
 *   1. Variable-Bit NetMessage ID encoding (2-bit selector + 4/8/12/32 payload)
 *   2. VarInt32 ULEB128 payload length serialization
 *   3. Sub-byte bitstream packing without bit-drift
 *   4. Full packet framing, multiplexing & deserialization dispatch
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <cassert>
#include <string>
#include <cstring>

namespace Source2 {

class CBitStreamWriter {
public:
    std::vector<uint8_t> m_buffer;
    size_t m_bitPos{0};

    void WriteBits(uint32_t val, size_t numBits) {
        for (size_t i = 0; i < numBits; ++i) {
            WriteBit((val >> i) & 1);
        }
    }

    void WriteBit(uint8_t bit) {
        size_t byteIndex = m_bitPos / 8;
        size_t bitOffset = m_bitPos % 8;
        if (byteIndex >= m_buffer.size()) {
            m_buffer.push_back(0);
        }
        if (bit) {
            m_buffer[byteIndex] |= static_cast<uint8_t>(1 << bitOffset);
        }
        m_bitPos++;
    }

    void WriteVarInt32(uint32_t val) {
        while (val >= 0x80) {
            WriteBits((val & 0x7F) | 0x80, 8);
            val >>= 7;
        }
        WriteBits(val & 0x7F, 8);
    }

    void WriteVarBitMsgId(uint32_t msgId) {
        if (msgId < 16) {
            WriteBits(0, 2);
            WriteBits(msgId, 4);
        } else if (msgId < 256) {
            WriteBits(1, 2);
            WriteBits(msgId, 8);
        } else if (msgId < 4096) {
            WriteBits(2, 2);
            WriteBits(msgId, 12);
        } else {
            WriteBits(3, 2);
            WriteBits(msgId, 32);
        }
    }

    void WriteBytes(const uint8_t* pData, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            WriteBits(pData[i], 8);
        }
    }
};

class CBitStreamReader {
public:
    const uint8_t* m_pBuffer{nullptr};
    size_t m_totalBits{0};
    size_t m_bitPos{0};

    CBitStreamReader(const uint8_t* pBuf, size_t numBits)
        : m_pBuffer(pBuf), m_totalBits(numBits), m_bitPos(0) {}

    uint32_t ReadBits(size_t numBits) {
        uint32_t val = 0;
        for (size_t i = 0; i < numBits; ++i) {
            if (ReadBit()) {
                val |= (1 << i);
            }
        }
        return val;
    }

    uint8_t ReadBit() {
        if (m_bitPos >= m_totalBits) return 0;
        size_t byteIndex = m_bitPos / 8;
        size_t bitOffset = m_bitPos % 8;
        m_bitPos++;
        return (m_pBuffer[byteIndex] >> bitOffset) & 1;
    }

    uint32_t ReadVarInt32() {
        uint32_t result = 0;
        int shift = 0;
        while (true) {
            uint32_t b = ReadBits(8);
            result |= (b & 0x7F) << shift;
            if (!(b & 0x80)) break;
            shift += 7;
        }
        return result;
    }

    uint32_t ReadVarBitMsgId(size_t* outHeaderBits = nullptr) {
        uint32_t selector = ReadBits(2);
        uint32_t id = 0;
        if (selector == 0) {
            id = ReadBits(4);
            if (outHeaderBits) *outHeaderBits = 6;
        } else if (selector == 1) {
            id = ReadBits(8);
            if (outHeaderBits) *outHeaderBits = 10;
        } else if (selector == 2) {
            id = ReadBits(12);
            if (outHeaderBits) *outHeaderBits = 14;
        } else {
            id = ReadBits(32);
            if (outHeaderBits) *outHeaderBits = 34;
        }
        return id;
    }

    void ReadBytes(uint8_t* pOut, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            pOut[i] = static_cast<uint8_t>(ReadBits(8));
        }
    }
};

} // namespace Source2

int main() {
    using namespace Source2;

    std::cout << "========================================================================\n";
    std::cout << "  VALVE SOURCE 2 (CS2) NETWORK FRAMING & SERIALIZATION SIMULATOR\n";
    std::cout << "  Reconstructed from: networksystem.dll (64-bit)\n";
    std::cout << "========================================================================\n\n";

    // ------------------------------------------------------------------------
    // Step 1: Variable-Bit Message ID Bit-Allocation Test
    // ------------------------------------------------------------------------
    std::cout << "[Step 1] Variable-Bit Message ID Bit-Allocation Test:\n";

    struct TestCase {
        std::string name;
        uint32_t id;
        size_t expectedBits;
    };

    std::vector<TestCase> cases = {
        {"net_NOP (ID 0 < 16)                       ", 0x0, 6},
        {"net_Tick (ID 4 < 16)                      ", 0x4, 6},
        {"clc_Move (ID 21 < 256)                    ", 0x15, 10},
        {"svc_PacketEntities (ID 55 < 256)          ", 0x37, 10},
        {"Extended GameMsg (ID 420 < 4096)          ", 0x1a4, 14},
        {"CustomSubtickSyncMsg (Token Hash >= 4096) ", 0x9eaa, 34}
    };

    for (const auto& tc : cases) {
        CBitStreamWriter writer;
        writer.WriteVarBitMsgId(tc.id);
        assert(writer.m_bitPos == tc.expectedBits);

        CBitStreamReader reader(writer.m_buffer.data(), writer.m_bitPos);
        size_t headerBits = 0;
        uint32_t decoded = reader.ReadVarBitMsgId(&headerBits);
        assert(decoded == tc.id);
        assert(headerBits == tc.expectedBits);

        std::cout << "  * " << tc.name << " -> ID: 0x" << std::hex << tc.id 
                  << " | Wire: " << std::dec << headerBits << " bits | Decoded: 0x" << std::hex << decoded 
                  << std::dec << " | VERIFIED\n";
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // Step 2: Constructing Full Source 2 UDP Packet Stream
    // ------------------------------------------------------------------------
    std::cout << "[Step 2] Constructing Full Source 2 UDP Packet Stream:\n";

    CBitStreamWriter packetWriter;

    // Msg 0: net_Tick (ID 4), Payload: 6 bytes
    packetWriter.WriteVarBitMsgId(4); // 6 bits
    packetWriter.WriteVarInt32(6);    // 8 bits (1 byte)
    uint8_t payloadTick[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    packetWriter.WriteBytes(payloadTick, 6); // 48 bits

    // Msg 1: clc_Move (ID 21 = 0x15), Payload: 15 bytes
    packetWriter.WriteVarBitMsgId(0x15); // 10 bits
    packetWriter.WriteVarInt32(15);      // 8 bits
    uint8_t payloadMove[15] = {0};
    packetWriter.WriteBytes(payloadMove, 15); // 120 bits

    // Msg 2: svc_PacketEntities (ID 55 = 0x37), Payload: 17 bytes
    packetWriter.WriteVarBitMsgId(0x37); // 10 bits
    packetWriter.WriteVarInt32(17);      // 8 bits
    uint8_t payloadEnts[17] = {0};
    packetWriter.WriteBytes(payloadEnts, 17); // 136 bits

    // Msg 3: Dynamic Token Msg (ID 0xb6a5), Payload: 9 bytes
    packetWriter.WriteVarBitMsgId(0xb6a5); // 34 bits
    packetWriter.WriteVarInt32(9);         // 8 bits
    uint8_t payloadDynamic[9] = {0};
    packetWriter.WriteBytes(payloadDynamic, 9); // 72 bits

    // Total bits = (6 + 8 + 48) + (10 + 8 + 120) + (10 + 8 + 136) + (34 + 8 + 72) = 62 + 138 + 154 + 114 = 468 bits
    // 468 bits / 8 = 58.5 bytes -> 59 bytes
    size_t totalBits = packetWriter.m_bitPos;
    size_t totalBytes = (totalBits + 7) / 8;
    assert(totalBytes == 59);
    assert(totalBits == 468);

    std::cout << "  Total Wire Size: " << totalBytes << " bytes (" << totalBits << " bits)\n\n";

    // ------------------------------------------------------------------------
    // Step 3: Deserializing and Dispatching Packet via BitStream Reader
    // ------------------------------------------------------------------------
    std::cout << "[Step 3] Deserializing and Dispatching Packet via BitStream Reader:\n";

    CBitStreamReader packetReader(packetWriter.m_buffer.data(), totalBits);

    struct ExpectedMsg {
        size_t offset;
        uint32_t id;
        std::string name;
        size_t payloadLen;
    };

    std::vector<ExpectedMsg> expectedMsgs = {
        {0, 0x0004, "net_Tick                    ", 6},
        {62, 0x0015, "clc_Move                    ", 15},
        {200, 0x0037, "svc_PacketEntities          ", 17},
        {354, 0xb6a5, "CUtlStringToken_DynamicMsg_46757", 9}
    };

    for (size_t i = 0; i < expectedMsgs.size(); ++i) {
        size_t currentBitOffset = packetReader.m_bitPos;
        assert(currentBitOffset == expectedMsgs[i].offset);

        uint32_t msgId = packetReader.ReadVarBitMsgId();
        uint32_t length = packetReader.ReadVarInt32();
        std::vector<uint8_t> payload(length);
        packetReader.ReadBytes(payload.data(), length);

        assert(msgId == expectedMsgs[i].id);
        assert(length == expectedMsgs[i].payloadLen);

        std::cout << "  [" << i << "] Offset: " << std::setfill('0') << std::setw(3) << currentBitOffset 
                  << " bits | ID: 0x" << std::hex << std::setw(4) << msgId << std::dec << " (" 
                  << expectedMsgs[i].name << ") | Payload: " << length << "  bytes\n";
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // Step 4: Verification & Accuracy Assessment
    // ------------------------------------------------------------------------
    std::cout << "[Step 4] Verification & Accuracy Assessment:\n";
    std::cout << "  [SUCCESS] All 4 NetMessages parsed perfectly without bit-drift!\n";
    std::cout << "  [Bit Economy] Fixed 32-bit ID headers would take: 128 bits.\n";
    std::cout << "                Source 2 Variable-Bit headers took: 6 + 10 + 10 + 34 = 60 bits.\n";
    std::cout << "                Bitstream savings: 53.125% bandwidth reduction on headers.\n";
    std::cout << "========================================================================\n";

    return 0;
}
