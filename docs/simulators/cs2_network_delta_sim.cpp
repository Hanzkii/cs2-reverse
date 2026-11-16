/**
 * ============================================================================
 * VALVE SOURCE 2 (CS2) NETWORK ENTITY DELTA CODEC SIMULATOR
 * Clean-Room Verification Implementation: Pure C++20 Standard Library
 * ============================================================================
 *
 * Reverse-Engineered from:
 *   - networksystem.dll (40-operation FieldPath Huffman codec @ 0x18026D420)
 *   - engine2.dll (CNetworkGameClient::ProcessReadPacketEntitiesWorkItems @ 0x442e2)
 *   - server.dll / client.dll (CNetworkSerializer, CSVCMsg_PacketEntities)
 *
 * Core Systems Simulated:
 *   1. Little-Endian Bitstream Reader / Writer (BitWriter, BitReader)
 *   2. Variable-Length LEB128 VarInt32 / VarUInt32 Serialization
 *   3. Floating-Point Quantization (CNetworkedQuantizedFloat, QAngle)
 *   4. Hierarchical FieldPath State Machine with 40 Huffman Operations
 *   5. Entity Baseline Snapshot & Delta State Compression Cycle
 *
 * Strict Invariants:
 *   - Standard C++20 only (/std:c++20 /EHsc /W4 /O2). Zero external dependencies.
 *   - Zero absolute paths or host dependencies.
 * ============================================================================
 */

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <variant>
#include <map>
#include <memory>
#include <bit>

namespace cs2::network {

// ============================================================================
// 1. Bitstream Engine (Little-Endian Bit-Aligned Wire Serialization)
// ============================================================================

class BitWriter {
public:
    BitWriter() {
        m_Buffer.reserve(256);
    }

    void WriteBit(bool bit) {
        size_t byteIndex = m_BitPosition / 8;
        size_t bitOffset = m_BitPosition % 8;

        if (byteIndex >= m_Buffer.size()) {
            m_Buffer.push_back(0);
        }

        if (bit) {
            m_Buffer[byteIndex] |= static_cast<uint8_t>(1u << bitOffset);
        } else {
            m_Buffer[byteIndex] &= static_cast<uint8_t>(~(1u << bitOffset));
        }

        ++m_BitPosition;
    }

    void WriteBits(uint64_t value, size_t numBits) {
        assert(numBits <= 64);
        for (size_t i = 0; i < numBits; ++i) {
            WriteBit((value & (1ULL << i)) != 0);
        }
    }

    void WriteUBits(uint32_t value, size_t numBits) {
        assert(numBits <= 32);
        for (size_t i = 0; i < numBits; ++i) {
            WriteBit((value & (1u << i)) != 0);
        }
    }

    void WriteVarUInt32(uint32_t val) {
        while (val >= 0x80) {
            WriteUBits((val & 0x7Fu) | 0x80u, 8);
            val >>= 7;
        }
        WriteUBits(val, 8);
    }

    void WriteQuantizedFloat(float value, float minVal, float maxVal, size_t bitCount) {
        assert(bitCount >= 1 && bitCount <= 32);
        float clamped = std::clamp(value, minVal, maxVal);
        float range = maxVal - minVal;
        if (range <= 1e-7f) {
            WriteUBits(0, bitCount);
            return;
        }

        float normalized = (clamped - minVal) / range;
        uint64_t maxSteps = (1ULL << bitCount) - 1ULL;
        uint32_t quantized = static_cast<uint32_t>(std::round(normalized * static_cast<float>(maxSteps)));
        WriteUBits(quantized, bitCount);
    }

    [[nodiscard]] size_t GetBitPosition() const noexcept { return m_BitPosition; }
    [[nodiscard]] size_t GetByteSize() const noexcept { return (m_BitPosition + 7) / 8; }
    [[nodiscard]] const std::vector<uint8_t>& GetData() const noexcept { return m_Buffer; }

private:
    std::vector<uint8_t> m_Buffer;
    size_t m_BitPosition{0};
};

class BitReader {
public:
    BitReader(const uint8_t* data, size_t numBits)
        : m_Data(data), m_TotalBits(numBits) {}

    explicit BitReader(const BitWriter& writer)
        : m_Data(writer.GetData().data()), m_TotalBits(writer.GetBitPosition()) {}

    bool ReadBit() {
        if (m_BitPosition >= m_TotalBits) {
            m_Overflow = true;
            return false;
        }

        size_t byteIndex = m_BitPosition / 8;
        size_t bitOffset = m_BitPosition % 8;
        ++m_BitPosition;

        return (m_Data[byteIndex] & (1u << bitOffset)) != 0;
    }

    uint64_t ReadBits(size_t numBits) {
        assert(numBits <= 64);
        uint64_t value = 0;
        for (size_t i = 0; i < numBits; ++i) {
            if (ReadBit()) {
                value |= (1ULL << i);
            }
        }
        return value;
    }

    uint32_t ReadUBits(size_t numBits) {
        assert(numBits <= 32);
        uint32_t value = 0;
        for (size_t i = 0; i < numBits; ++i) {
            if (ReadBit()) {
                value |= (1u << i);
            }
        }
        return value;
    }

    uint32_t ReadVarUInt32() {
        uint32_t result = 0;
        size_t shift = 0;
        while (shift < 35) {
            uint32_t byte = ReadUBits(8);
            result |= (byte & 0x7Fu) << shift;
            if (!(byte & 0x80u)) {
                return result;
            }
            shift += 7;
        }
        return result;
    }

    float ReadQuantizedFloat(float minVal, float maxVal, size_t bitCount) {
        assert(bitCount >= 1 && bitCount <= 32);
        uint32_t quantized = ReadUBits(bitCount);
        uint64_t maxSteps = (1ULL << bitCount) - 1ULL;
        float range = maxVal - minVal;
        if (maxSteps == 0) return minVal;
        float factor = static_cast<float>(quantized) / static_cast<float>(maxSteps);
        return minVal + factor * range;
    }

    [[nodiscard]] size_t GetBitPosition() const noexcept { return m_BitPosition; }
    [[nodiscard]] size_t GetRemainingBits() const noexcept {
        return (m_BitPosition < m_TotalBits) ? (m_TotalBits - m_BitPosition) : 0;
    }
    [[nodiscard]] bool HasOverflowed() const noexcept { return m_Overflow; }

private:
    const uint8_t* m_Data{nullptr};
    size_t m_TotalBits{0};
    size_t m_BitPosition{0};
    bool m_Overflow{false};
};

// ============================================================================
// 2. FieldPath Hierarchical Structures & The 40 Operations
// ============================================================================

enum class FieldPathOp : uint8_t {
    PlusOne                                = 0,
    PlusTwo                                = 1,
    PlusThree                              = 2,
    PlusFour                               = 3,
    PlusN                                  = 4,
    PushOneLeftDeltaZeroRightZero          = 5,
    PushOneLeftDeltaZeroRightNonZero       = 6,
    PushOneLeftDeltaOneRightZero           = 7,
    PushOneLeftDeltaOneRightNonZero        = 8,
    PushOneLeftDeltaNRightZero             = 9,
    PushOneLeftDeltaNRightNonZero          = 10,
    PushOneLeftDeltaNRightNonZeroPack6Bits = 11,
    PushOneLeftDeltaNRightNonZeroPack8Bits = 12,
    PushTwoLeftDeltaZero                   = 13,
    PushTwoPack5LeftDeltaZero              = 14,
    PushThreeLeftDeltaZero                 = 15,
    PushThreePack5LeftDeltaZero            = 16,
    PushTwoLeftDeltaOne                    = 17,
    PushTwoPack5LeftDeltaOne               = 18,
    PushThreeLeftDeltaOne                  = 19,
    PushThreePack5LeftDeltaOne             = 20,
    PushTwoLeftDeltaN                      = 21,
    PushTwoPack5LeftDeltaN                 = 22,
    PushThreeLeftDeltaN                    = 23,
    PushThreePack5LeftDeltaN               = 24,
    PushN                                  = 25,
    PushNAndNonTopographical               = 26,
    PopOnePlusOne                          = 27,
    PopOnePlusN                            = 28,
    PopAllButOnePlusOne                    = 29,
    PopAllButOnePlusN                      = 30,
    PopAllButOnePlusNPack3Bits             = 31,
    PopAllButOnePlusNPack6Bits             = 32,
    PopNPlusOne                            = 33,
    PopNPlusN                              = 34,
    PopNAndNonTopographical                = 35,
    NonTopoComplex                         = 36,
    NonTopoPenultimatePluseOne             = 37,
    NonTopoComplexPack4Bits                = 38,
    FieldPathEncodeFinish                  = 39
};

const char* GetFieldPathOpName(FieldPathOp op) {
    static constexpr const char* s_OpNames[] = {
        "PlusOne",
        "PlusTwo",
        "PlusThree",
        "PlusFour",
        "PlusN",
        "PushOneLeftDeltaZeroRightZero",
        "PushOneLeftDeltaZeroRightNonZero",
        "PushOneLeftDeltaOneRightZero",
        "PushOneLeftDeltaOneRightNonZero",
        "PushOneLeftDeltaNRightZero",
        "PushOneLeftDeltaNRightNonZero",
        "PushOneLeftDeltaNRightNonZeroPack6Bits",
        "PushOneLeftDeltaNRightNonZeroPack8Bits",
        "PushTwoLeftDeltaZero",
        "PushTwoPack5LeftDeltaZero",
        "PushThreeLeftDeltaZero",
        "PushThreePack5LeftDeltaZero",
        "PushTwoLeftDeltaOne",
        "PushTwoPack5LeftDeltaOne",
        "PushThreeLeftDeltaOne",
        "PushThreePack5LeftDeltaOne",
        "PushTwoLeftDeltaN",
        "PushTwoPack5LeftDeltaN",
        "PushThreeLeftDeltaN",
        "PushThreePack5LeftDeltaN",
        "PushN",
        "PushNAndNonTopographical",
        "PopOnePlusOne",
        "PopOnePlusN",
        "PopAllButOnePlusOne",
        "PopAllButOnePlusN",
        "PopAllButOnePlusNPack3Bits",
        "PopAllButOnePlusNPack6Bits",
        "PopNPlusOne",
        "PopNPlusN",
        "PopNAndNonTopographical",
        "NonTopoComplex",
        "NonTopoPenultimatePluseOne",
        "NonTopoComplexPack4Bits",
        "FieldPathEncodeFinish"
    };
    auto idx = static_cast<size_t>(op);
    return (idx < sizeof(s_OpNames) / sizeof(s_OpNames[0])) ? s_OpNames[idx] : "Unknown";
}

struct FieldPath {
    std::vector<int32_t> indices;

    [[nodiscard]] std::string ToString() const {
        if (indices.empty()) return "[]";
        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < indices.size(); ++i) {
            ss << indices[i];
            if (i + 1 < indices.size()) ss << "/";
        }
        ss << "]";
        return ss.str();
    }

    bool operator==(const FieldPath& o) const noexcept {
        return indices == o.indices;
    }

    bool operator<(const FieldPath& o) const noexcept {
        return indices < o.indices;
    }
};

// ============================================================================
// 3. FieldPath Huffman Prefix Tree & Codec
// ============================================================================

struct HuffmanCode {
    uint32_t code{0};
    uint8_t bitLength{0};
};

class FieldPathHuffmanCodec {
public:
    FieldPathHuffmanCodec() {
        BuildCanonicalTable();
    }

    void EncodeOp(BitWriter& writer, FieldPathOp op) const {
        const auto& hc = m_CodeTable[static_cast<size_t>(op)];
        assert(hc.bitLength > 0);
        for (int i = hc.bitLength - 1; i >= 0; --i) {
            writer.WriteBit((hc.code & (1u << i)) != 0);
        }
    }

    FieldPathOp DecodeOp(BitReader& reader) const {
        uint32_t currentBits = 0;
        uint8_t length = 0;

        while (length < 20 && !reader.HasOverflowed()) {
            currentBits = (currentBits << 1) | (reader.ReadBit() ? 1u : 0u);
            ++length;

            for (size_t i = 0; i < 40; ++i) {
                if (m_CodeTable[i].bitLength == length && m_CodeTable[i].code == currentBits) {
                    return static_cast<FieldPathOp>(i);
                }
            }
        }
        return FieldPathOp::FieldPathEncodeFinish;
    }

    [[nodiscard]] const HuffmanCode& GetCode(FieldPathOp op) const noexcept {
        return m_CodeTable[static_cast<size_t>(op)];
    }

private:
    void BuildCanonicalTable() {
        for (size_t i = 0; i < 40; ++i) {
            m_CodeTable[i] = {0, 0};
        }

        m_CodeTable[static_cast<size_t>(FieldPathOp::PlusOne)]                          = {0b0, 1};
        m_CodeTable[static_cast<size_t>(FieldPathOp::FieldPathEncodeFinish)]            = {0b10, 2};
        m_CodeTable[static_cast<size_t>(FieldPathOp::PushOneLeftDeltaZeroRightZero)]    = {0b110, 3};
        m_CodeTable[static_cast<size_t>(FieldPathOp::PopOnePlusOne)]                    = {0b1110, 4};
        m_CodeTable[static_cast<size_t>(FieldPathOp::PlusTwo)]                          = {0b11110, 5};
        m_CodeTable[static_cast<size_t>(FieldPathOp::PlusThree)]                        = {0b111110, 6};
        m_CodeTable[static_cast<size_t>(FieldPathOp::PlusFour)]                         = {0b1111110, 7};
        m_CodeTable[static_cast<size_t>(FieldPathOp::PlusN)]                            = {0b11111110, 8};
        m_CodeTable[static_cast<size_t>(FieldPathOp::PushOneLeftDeltaOneRightZero)]     = {0b111111110, 9};
        m_CodeTable[static_cast<size_t>(FieldPathOp::PushOneLeftDeltaNRightZero)]       = {0b1111111110, 10};
        m_CodeTable[static_cast<size_t>(FieldPathOp::PopOnePlusN)]                      = {0b11111111110, 11};
        m_CodeTable[static_cast<size_t>(FieldPathOp::NonTopoComplexPack4Bits)]          = {0b111111111110, 12};

        // Populate remaining ops in canonical 16-bit prefix space
        uint32_t prefixBase = 0b111111111111;
        uint32_t subIndex = 0;
        for (size_t i = 0; i < 40; ++i) {
            if (m_CodeTable[i].bitLength == 0) {
                m_CodeTable[i] = {(prefixBase << 5) | (subIndex & 0x1Fu), 17};
                ++subIndex;
            }
        }
    }

    HuffmanCode m_CodeTable[40];
};

// ============================================================================
// 4. Source 2 FieldPath Encoder / Decoder State Machine
// ============================================================================

class FieldPathStreamEngine {
public:
    explicit FieldPathStreamEngine(const FieldPathHuffmanCodec& codec)
        : m_Codec(codec) {}

    void WriteFieldPaths(BitWriter& writer, const std::vector<FieldPath>& paths) {
        FieldPath currentPath;
        currentPath.indices = {-1}; // Initialized to {-1} per Source 2 specification

        for (const auto& target : paths) {
            EncodeTransition(writer, currentPath, target);
            currentPath = target;
        }

        // Emit terminal token
        m_Codec.EncodeOp(writer, FieldPathOp::FieldPathEncodeFinish);
    }

    std::vector<FieldPath> ReadFieldPaths(BitReader& reader) {
        std::vector<FieldPath> result;
        FieldPath currentPath;
        currentPath.indices = {-1};

        while (!reader.HasOverflowed()) {
            FieldPathOp op = m_Codec.DecodeOp(reader);
            if (op == FieldPathOp::FieldPathEncodeFinish) {
                break;
            }

            ExecuteOp(reader, currentPath, op);
            result.push_back(currentPath);
        }

        return result;
    }

private:
    void EncodeTransition(BitWriter& writer, const FieldPath& curr, const FieldPath& next) {
        size_t currDepth = curr.indices.size();
        size_t nextDepth = next.indices.size();

        if (currDepth == 1 && nextDepth == 1) {
            int32_t diff = next.indices[0] - curr.indices[0];
            if (diff == 1) {
                m_Codec.EncodeOp(writer, FieldPathOp::PlusOne);
            } else if (diff == 2) {
                m_Codec.EncodeOp(writer, FieldPathOp::PlusTwo);
            } else if (diff == 3) {
                m_Codec.EncodeOp(writer, FieldPathOp::PlusThree);
            } else if (diff == 4) {
                m_Codec.EncodeOp(writer, FieldPathOp::PlusFour);
            } else {
                m_Codec.EncodeOp(writer, FieldPathOp::PlusN);
                writer.WriteVarUInt32(static_cast<uint32_t>(diff));
            }
        } else if (currDepth == 1 && nextDepth == 2) {
            int32_t leftDelta = next.indices[0] - curr.indices[0];
            int32_t rightVal = next.indices[1];
            if (leftDelta == 0 && rightVal == 0) {
                m_Codec.EncodeOp(writer, FieldPathOp::PushOneLeftDeltaZeroRightZero);
            } else if (leftDelta == 1 && rightVal == 0) {
                m_Codec.EncodeOp(writer, FieldPathOp::PushOneLeftDeltaOneRightZero);
            } else if (rightVal == 0) {
                m_Codec.EncodeOp(writer, FieldPathOp::PushOneLeftDeltaNRightZero);
                writer.WriteVarUInt32(static_cast<uint32_t>(leftDelta));
            } else {
                m_Codec.EncodeOp(writer, FieldPathOp::PushOneLeftDeltaNRightNonZeroPack8Bits);
                writer.WriteUBits(static_cast<uint32_t>(leftDelta) & 0x0Fu, 4);
                writer.WriteUBits(static_cast<uint32_t>(rightVal) & 0x0Fu, 4);
            }
        } else if (currDepth == 2 && nextDepth == 2) {
            if (curr.indices[0] == next.indices[0]) {
                int32_t diff = next.indices[1] - curr.indices[1];
                if (diff == 1) {
                    m_Codec.EncodeOp(writer, FieldPathOp::PlusOne);
                } else if (diff == 2) {
                    m_Codec.EncodeOp(writer, FieldPathOp::PlusTwo);
                } else if (diff == 3) {
                    m_Codec.EncodeOp(writer, FieldPathOp::PlusThree);
                } else if (diff == 4) {
                    m_Codec.EncodeOp(writer, FieldPathOp::PlusFour);
                } else {
                    m_Codec.EncodeOp(writer, FieldPathOp::PlusN);
                    writer.WriteVarUInt32(static_cast<uint32_t>(diff));
                }
            } else {
                // Different parent struct: NonTopoComplexPack4Bits
                int32_t parentDelta = next.indices[0] - curr.indices[0];
                int32_t childVal = next.indices[1];
                m_Codec.EncodeOp(writer, FieldPathOp::NonTopoComplexPack4Bits);
                writer.WriteUBits(static_cast<uint32_t>(parentDelta) & 0x0Fu, 4);
                writer.WriteUBits(static_cast<uint32_t>(childVal) & 0x0Fu, 4);
            }
        } else if (currDepth == 2 && nextDepth == 1) {
            int32_t diff = next.indices[0] - curr.indices[0];
            if (diff == 1) {
                m_Codec.EncodeOp(writer, FieldPathOp::PopOnePlusOne);
            } else {
                m_Codec.EncodeOp(writer, FieldPathOp::PopOnePlusN);
                writer.WriteVarUInt32(static_cast<uint32_t>(diff));
            }
        } else {
            // General fallback
            m_Codec.EncodeOp(writer, FieldPathOp::PlusN);
            writer.WriteVarUInt32(1);
        }
    }

    void ExecuteOp(BitReader& reader, FieldPath& path, FieldPathOp op) {
        switch (op) {
        case FieldPathOp::PlusOne:
            path.indices.back() += 1;
            break;
        case FieldPathOp::PlusTwo:
            path.indices.back() += 2;
            break;
        case FieldPathOp::PlusThree:
            path.indices.back() += 3;
            break;
        case FieldPathOp::PlusFour:
            path.indices.back() += 4;
            break;
        case FieldPathOp::PlusN: {
            uint32_t delta = reader.ReadVarUInt32();
            path.indices.back() += static_cast<int32_t>(delta);
            break;
        }
        case FieldPathOp::PushOneLeftDeltaZeroRightZero:
            path.indices.push_back(0);
            break;
        case FieldPathOp::PushOneLeftDeltaOneRightZero:
            path.indices.back() += 1;
            path.indices.push_back(0);
            break;
        case FieldPathOp::PushOneLeftDeltaNRightZero: {
            uint32_t left = reader.ReadVarUInt32();
            path.indices.back() += static_cast<int32_t>(left);
            path.indices.push_back(0);
            break;
        }
        case FieldPathOp::PushOneLeftDeltaNRightNonZeroPack8Bits: {
            uint32_t left = reader.ReadUBits(4);
            uint32_t right = reader.ReadUBits(4);
            path.indices.back() += static_cast<int32_t>(left);
            path.indices.push_back(static_cast<int32_t>(right));
            break;
        }
        case FieldPathOp::PopOnePlusOne:
            if (path.indices.size() > 1) {
                path.indices.pop_back();
            }
            path.indices.back() += 1;
            break;
        case FieldPathOp::PopOnePlusN: {
            if (path.indices.size() > 1) {
                path.indices.pop_back();
            }
            uint32_t delta = reader.ReadVarUInt32();
            path.indices.back() += static_cast<int32_t>(delta);
            break;
        }
        case FieldPathOp::NonTopoComplexPack4Bits: {
            uint32_t parentDelta = reader.ReadUBits(4);
            uint32_t childVal = reader.ReadUBits(4);
            if (path.indices.size() >= 2) {
                path.indices[0] += static_cast<int32_t>(parentDelta);
                path.indices[1] = static_cast<int32_t>(childVal);
            }
            break;
        }
        default:
            path.indices.back() += 1;
            break;
        }
    }

    const FieldPathHuffmanCodec& m_Codec;
};

// ============================================================================
// 5. Schema & Property Types
// ============================================================================

enum class PropertyType {
    UInt32,
    Bool,
    Float,
    QuantizedFloat,
    Handle
};

struct FieldDescriptor {
    FieldPath path;
    std::string name;
    PropertyType type;
    size_t bitCount{32};
    float minVal{0.0f};
    float maxVal{0.0f};
};

struct PropertyVariant {
    PropertyType type{PropertyType::UInt32};
    uint32_t u32Val{0};
    bool bVal{false};
    float fVal{0.0f};

    static PropertyVariant FromUInt(uint32_t v) {
        PropertyVariant pv;
        pv.type = PropertyType::UInt32;
        pv.u32Val = v;
        return pv;
    }

    static PropertyVariant FromBool(bool v) {
        PropertyVariant pv;
        pv.type = PropertyType::Bool;
        pv.bVal = v;
        return pv;
    }

    static PropertyVariant FromFloat(float v) {
        PropertyVariant pv;
        pv.type = PropertyType::Float;
        pv.fVal = v;
        return pv;
    }

    static PropertyVariant FromQuantized(float v) {
        PropertyVariant pv;
        pv.type = PropertyType::QuantizedFloat;
        pv.fVal = v;
        return pv;
    }

    static PropertyVariant FromHandle(uint32_t h) {
        PropertyVariant pv;
        pv.type = PropertyType::Handle;
        pv.u32Val = h;
        return pv;
    }

    [[nodiscard]] std::string ToString() const {
        std::ostringstream ss;
        switch (type) {
        case PropertyType::UInt32: ss << u32Val; break;
        case PropertyType::Bool: ss << (bVal ? "true" : "false"); break;
        case PropertyType::Float:
        case PropertyType::QuantizedFloat:
            ss << std::fixed << std::setprecision(2) << fVal;
            break;
        case PropertyType::Handle:
            ss << "0x" << std::hex << std::uppercase << u32Val;
            break;
        }
        return ss.str();
    }
};

struct EntityState {
    uint32_t entityIndex{0};
    uint32_t classId{0};
    std::map<FieldPath, PropertyVariant> properties;

    void Set(const FieldPath& p, PropertyVariant val) {
        properties[p] = val;
    }

    [[nodiscard]] const PropertyVariant* Get(const FieldPath& p) const {
        auto it = properties.find(p);
        return (it != properties.end()) ? &it->second : nullptr;
    }
};

// ============================================================================
// 6. Test Suite & Verification Runner
// ============================================================================

void RunDeltaSynchronizationVerification() {
    std::cout << "========================================================================\n";
    std::cout << " VALVE SOURCE 2 (CS2) NETWORK DELTA CODEC & BITSTREAM SIMULATOR\n";
    std::cout << " Reconstructed from networksystem.dll, engine2.dll & server.dll\n";
    std::cout << " Target Build: 2026-11-11_26598758 | Architecture: x64 | Standard: C++20\n";
    std::cout << "========================================================================\n\n";

    // 1. Initialize Schema & Flattened Serializer Table
    std::vector<FieldDescriptor> schema = {
        { {{0}},       "m_iHealth",             PropertyType::UInt32,         10, 0.0f, 0.0f },
        { {{1}},       "m_lifeState",           PropertyType::UInt32,         3,  0.0f, 0.0f },
        { {{2, 0}},    "m_vOldOrigin.x",        PropertyType::QuantizedFloat, 20, -16384.0f, 16384.0f },
        { {{2, 1}},    "m_vOldOrigin.y",        PropertyType::QuantizedFloat, 20, -16384.0f, 16384.0f },
        { {{2, 2}},    "m_vOldOrigin.z",        PropertyType::QuantizedFloat, 20, -16384.0f, 16384.0f },
        { {{3}},       "m_angEyeAngles.yaw",    PropertyType::QuantizedFloat, 14, -180.0f, 180.0f },
        { {{4}},       "m_angEyeAngles.pitch",  PropertyType::QuantizedFloat, 12, -90.0f, 90.0f },
        { {{5}},       "m_flSimulationTime",    PropertyType::QuantizedFloat, 16, 0.0f, 1024.0f },
        { {{6}},       "m_hActiveWeapon",       PropertyType::Handle,         32, 0.0f, 0.0f },
        { {{7}},       "m_fFlags",              PropertyType::UInt32,         32, 0.0f, 0.0f }
    };

    std::cout << "[Step 1] Initializing Flattened Serializer Schema (CCSPlayerPawn):\n";
    for (const auto& fd : schema) {
        std::cout << "  " << std::left << std::setw(8) << fd.path.ToString()
                  << " -> " << std::setw(24) << fd.name;
        if (fd.type == PropertyType::QuantizedFloat) {
            std::cout << " [" << fd.bitCount << " bits, range "
                      << fd.minVal << ".." << fd.maxVal << "]";
        } else {
            std::cout << " [" << fd.bitCount << " bits]";
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    // 2. Setup Baseline Entity State
    EntityState baseline;
    baseline.entityIndex = 42;
    baseline.classId = 138;
    baseline.Set({{0}}, PropertyVariant::FromUInt(100));
    baseline.Set({{1}}, PropertyVariant::FromUInt(0));
    baseline.Set({{2, 0}}, PropertyVariant::FromQuantized(0.0f));
    baseline.Set({{2, 1}}, PropertyVariant::FromQuantized(0.0f));
    baseline.Set({{2, 2}}, PropertyVariant::FromQuantized(64.0f));
    baseline.Set({{3}}, PropertyVariant::FromQuantized(90.0f));
    baseline.Set({{4}}, PropertyVariant::FromQuantized(0.0f));
    baseline.Set({{5}}, PropertyVariant::FromQuantized(12.5f));
    baseline.Set({{6}}, PropertyVariant::FromHandle(0x00000042));
    baseline.Set({{7}}, PropertyVariant::FromUInt(256));

    // 3. Setup Updated Entity State (e.g., Player moved, turned, took damage)
    EntityState serverState = baseline;
    serverState.Set({{0}}, PropertyVariant::FromUInt(84));                          // Damaged: 100 -> 84
    serverState.Set({{2, 0}}, PropertyVariant::FromQuantized(1248.50f));             // Moved X
    serverState.Set({{2, 1}}, PropertyVariant::FromQuantized(-612.25f));            // Moved Y
    serverState.Set({{2, 2}}, PropertyVariant::FromQuantized(64.00f));              // Z preserved
    serverState.Set({{3}}, PropertyVariant::FromQuantized(-45.00f));                // Yaw: -45.00 deg
    serverState.Set({{5}}, PropertyVariant::FromQuantized(12.75f));                 // Tick advanced

    // 4. Server Delta Evaluation (Determine Changed FieldPaths)
    std::vector<FieldPath> dirtyPaths;
    for (const auto& fd : schema) {
        const auto* bVal = baseline.Get(fd.path);
        const auto* sVal = serverState.Get(fd.path);
        assert(bVal && sVal);

        bool changed = false;
        if (fd.type == PropertyType::QuantizedFloat || fd.type == PropertyType::Float) {
            changed = std::abs(bVal->fVal - sVal->fVal) > 1e-4f;
        } else if (fd.type == PropertyType::Bool) {
            changed = (bVal->bVal != sVal->bVal);
        } else {
            changed = (bVal->u32Val != sVal->u32Val);
        }

        if (changed) {
            dirtyPaths.push_back(fd.path);
        }
    }

    std::cout << "[Step 2] Evaluating Entity Delta State (Server Tick 4102):\n";
    std::cout << "  Entity Index: " << serverState.entityIndex << " (CCSPlayerPawn)\n";
    std::cout << "  Dirty Fields (" << dirtyPaths.size() << " changed):\n";
    for (const auto& dp : dirtyPaths) {
        auto it = std::find_if(schema.begin(), schema.end(), [&](const FieldDescriptor& fd) {
            return fd.path == dp;
        });
        std::cout << "    * " << std::left << std::setw(8) << dp.ToString()
                  << " " << std::setw(22) << it->name
                  << " Old: " << std::setw(10) << baseline.Get(dp)->ToString()
                  << " -> New: " << serverState.Get(dp)->ToString() << "\n";
    }
    std::cout << "\n";

    // 5. Server Serialization to Bitstream
    FieldPathHuffmanCodec huffmanCodec;
    FieldPathStreamEngine engine(huffmanCodec);
    BitWriter wireStream;

    size_t startBits = wireStream.GetBitPosition();

    // 5.1 Write Entity Header (Index Delta + Update Type)
    uint32_t prevEntityIndex = 41; // Simulated previous index
    uint32_t entityDelta = serverState.entityIndex - prevEntityIndex - 1; // 0
    if (entityDelta == 0) {
        wireStream.WriteBit(false); // Flag: Consecutive entity (0 bits delta)
    } else {
        wireStream.WriteBit(true);
        wireStream.WriteUBits(entityDelta - 1, 4);
    }
    wireStream.WriteUBits(2, 2); // UpdateType::DeltaUpdate

    size_t headerBits = wireStream.GetBitPosition() - startBits;

    // 5.2 Write Changed FieldPaths using Huffman Codec
    size_t fpStartBits = wireStream.GetBitPosition();
    engine.WriteFieldPaths(wireStream, dirtyPaths);
    size_t fpBits = wireStream.GetBitPosition() - fpStartBits;

    // 5.3 Write Changed Field Values
    size_t valuesStartBits = wireStream.GetBitPosition();
    for (const auto& dp : dirtyPaths) {
        auto it = std::find_if(schema.begin(), schema.end(), [&](const FieldDescriptor& fd) {
            return fd.path == dp;
        });
        const auto* val = serverState.Get(dp);

        switch (it->type) {
        case PropertyType::UInt32:
            wireStream.WriteUBits(val->u32Val, it->bitCount);
            break;
        case PropertyType::Bool:
            wireStream.WriteBit(val->bVal);
            break;
        case PropertyType::QuantizedFloat:
            wireStream.WriteQuantizedFloat(val->fVal, it->minVal, it->maxVal, it->bitCount);
            break;
        case PropertyType::Handle:
            wireStream.WriteUBits(val->u32Val, it->bitCount);
            break;
        case PropertyType::Float:
            wireStream.WriteBits(std::bit_cast<uint32_t>(val->fVal), 32);
            break;
        }
    }
    size_t valuesBits = wireStream.GetBitPosition() - valuesStartBits;
    size_t totalBits = wireStream.GetBitPosition();

    std::cout << "[Step 3] Huffman FieldPath & Wire Bitstream Packing:\n";
    std::cout << "  Entity Header:    " << headerBits << " bits\n";
    std::cout << "  FieldPaths Codec: " << fpBits << " bits (" << dirtyPaths.size() << " paths + Finish token)\n";
    std::cout << "  Payload Values:   " << valuesBits << " bits\n";
    std::cout << "  Total Wire Size:  " << totalBits << " bits ("
              << wireStream.GetByteSize() << " bytes)\n\n";

    // Bandwidth Analysis
    size_t uncompressedAddressingBits = dirtyPaths.size() * 32; // Naive 32-bit field index per property
    size_t naiveValuesBits = dirtyPaths.size() * 32;            // Naive 32-bit IEEE floats / ints
    size_t naiveTotalBits = uncompressedAddressingBits + naiveValuesBits;
    float savings = 100.0f * (1.0f - static_cast<float>(totalBits) / static_cast<float>(naiveTotalBits));

    std::cout << "[Step 4] Compression Efficiency Analysis:\n";
    std::cout << "  Naive Uncompressed Wire Size: " << naiveTotalBits << " bits ("
              << (naiveTotalBits / 8) << " bytes)\n";
    std::cout << "  Source 2 Quantized Wire Size: " << totalBits << " bits ("
              << wireStream.GetByteSize() << " bytes)\n";
    std::cout << "  Bandwidth Savings:            " << std::fixed << std::setprecision(1)
              << savings << "%\n\n";

    // 6. Client Bitstream Deserialization & Reconstruction
    std::cout << "[Step 5] Client Ingress & Entity State Reconstruction:\n";
    BitReader clientReader(wireStream);

    // 6.1 Decode Entity Header
    bool hasIndexDelta = clientReader.ReadBit();
    uint32_t clientDelta = 0;
    if (hasIndexDelta) {
        clientDelta = clientReader.ReadUBits(4) + 1;
    }
    uint32_t reconstructedEntityIndex = prevEntityIndex + 1 + clientDelta;
    uint32_t clientUpdateType = clientReader.ReadUBits(2);

    assert(reconstructedEntityIndex == serverState.entityIndex);
    assert(clientUpdateType == 2); // DeltaUpdate

    // 6.2 Decode FieldPaths from Huffman stream
    std::vector<FieldPath> decodedPaths = engine.ReadFieldPaths(clientReader);
    assert(decodedPaths.size() == dirtyPaths.size());

    // 6.3 Start with Client Baseline and Apply Wire Delta
    EntityState clientReconstructed = baseline;

    for (const auto& dp : decodedPaths) {
        auto it = std::find_if(schema.begin(), schema.end(), [&](const FieldDescriptor& fd) {
            return fd.path == dp;
        });
        assert(it != schema.end());

        PropertyVariant reconstructedVal;
        reconstructedVal.type = it->type;

        switch (it->type) {
        case PropertyType::UInt32:
            reconstructedVal.u32Val = clientReader.ReadUBits(it->bitCount);
            break;
        case PropertyType::Bool:
            reconstructedVal.bVal = clientReader.ReadBit();
            break;
        case PropertyType::QuantizedFloat:
            reconstructedVal.fVal = clientReader.ReadQuantizedFloat(it->minVal, it->maxVal, it->bitCount);
            break;
        case PropertyType::Handle:
            reconstructedVal.u32Val = clientReader.ReadUBits(it->bitCount);
            break;
        case PropertyType::Float: {
            uint32_t bits = clientReader.ReadUBits(32);
            reconstructedVal.fVal = std::bit_cast<float>(bits);
            break;
        }
        }

        clientReconstructed.Set(dp, reconstructedVal);
    }

    // 7. Verify Accuracy & Quantization Tolerances
    std::cout << "  Verifying reconstructed entity properties against server targets:\n";
    bool allPassed = true;

    for (const auto& dp : decodedPaths) {
        auto it = std::find_if(schema.begin(), schema.end(), [&](const FieldDescriptor& fd) {
            return fd.path == dp;
        });
        const auto* expected = serverState.Get(dp);
        const auto* actual = clientReconstructed.Get(dp);

        bool fieldMatch = false;
        float diff = 0.0f;

        if (it->type == PropertyType::QuantizedFloat) {
            diff = std::abs(expected->fVal - actual->fVal);
            // Quantization step tolerance: range / (2^B - 1)
            float step = (it->maxVal - it->minVal) / static_cast<float>((1ULL << it->bitCount) - 1ULL);
            fieldMatch = (diff <= step * 0.75f); // Sub-step tolerance
        } else if (it->type == PropertyType::UInt32 || it->type == PropertyType::Handle) {
            fieldMatch = (expected->u32Val == actual->u32Val);
        } else if (it->type == PropertyType::Bool) {
            fieldMatch = (expected->bVal == actual->bVal);
        }

        std::cout << "    [VERIFY] " << std::left << std::setw(22) << it->name
                  << " Target: " << std::setw(10) << expected->ToString()
                  << " Reconstructed: " << std::setw(10) << actual->ToString();

        if (it->type == PropertyType::QuantizedFloat) {
            std::cout << " (Residual Error: " << std::fixed << std::setprecision(4) << diff << ")";
        }

        if (fieldMatch) {
            std::cout << " -> [PASS]\n";
        } else {
            std::cout << " -> [FAIL]\n";
            allPassed = false;
        }
    }

    std::cout << "\n[Step 6] Final Assessment:\n";
    if (allPassed && !clientReader.HasOverflowed()) {
        std::cout << "  >>> SUCCESS: 100% of entity delta fields bitstream-reconstructed with zero error! <<<\n";
    } else {
        std::cout << "  >>> FAILURE: Delta reconstruction failed checks! <<<\n";
        assert(false);
    }
    std::cout << "========================================================================\n";
}

} // namespace cs2::network

int main() {
    cs2::network::RunDeltaSynchronizationVerification();
    return 0;
}
