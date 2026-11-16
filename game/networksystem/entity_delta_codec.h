#ifndef CS2_ENTITY_DELTA_CODEC_H
#define CS2_ENTITY_DELTA_CODEC_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <memory>
#include <variant>
#include <cmath>
#include <algorithm>
#include <cassert>

#include "sdk/netmessages.hpp"
#include "game/networksystem/netmessage_framing.h"
#include "game/networksystem/flattened_serializers.h"

namespace cs2::networksystem {

// ============================================================================
// 1. Property Values & Variant Types
// ============================================================================

struct Vector3_t {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr bool operator==(const Vector3_t& o) const noexcept {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct QAngle_t {
    float pitch{0.0f};
    float yaw{0.0f};
    float roll{0.0f};

    constexpr bool operator==(const QAngle_t& o) const noexcept {
        return pitch == o.pitch && yaw == o.yaw && roll == o.roll;
    }
};

/**
 * @brief Variant value holding decoded entity schema properties.
 */
struct PropertyValue {
    enum class Type : uint8_t {
        None = 0,
        Bool,
        Int32,
        UInt32,
        Int64,
        UInt64,
        Float,
        Vector,
        QAngle,
        String,
        Handle
    };

    Type type{Type::None};

    union {
        bool        bVal;
        int32_t     i32Val;
        uint32_t    u32Val;
        int64_t     i64Val;
        uint64_t    u64Val;
        float       fVal;
        Vector3_t   vecVal;
        QAngle_t    angVal;
        uint32_t    handleVal;
    };
    std::string strVal{};

    PropertyValue() noexcept : type(Type::None), u64Val(0) {}
    explicit PropertyValue(bool v) noexcept : type(Type::Bool), bVal(v) {}
    explicit PropertyValue(int32_t v) noexcept : type(Type::Int32), i32Val(v) {}
    explicit PropertyValue(uint32_t v) noexcept : type(Type::UInt32), u32Val(v) {}
    explicit PropertyValue(int64_t v) noexcept : type(Type::Int64), i64Val(v) {}
    explicit PropertyValue(uint64_t v) noexcept : type(Type::UInt64), u64Val(v) {}
    explicit PropertyValue(float v) noexcept : type(Type::Float), fVal(v) {}
    explicit PropertyValue(Vector3_t v) noexcept : type(Type::Vector), vecVal(v) {}
    explicit PropertyValue(QAngle_t v) noexcept : type(Type::QAngle), angVal(v) {}
    explicit PropertyValue(std::string v) : type(Type::String), u64Val(0), strVal(std::move(v)) {}

    static PropertyValue FromHandle(uint32_t h) noexcept {
        PropertyValue pv;
        pv.type = Type::Handle;
        pv.handleVal = h;
        return pv;
    }

    PropertyValue(const PropertyValue& o) : type(o.type), u64Val(o.u64Val), strVal(o.strVal) {
        if (type == Type::Vector) vecVal = o.vecVal;
        else if (type == Type::QAngle) angVal = o.angVal;
    }

    PropertyValue& operator=(const PropertyValue& o) {
        if (this != &o) {
            type = o.type;
            u64Val = o.u64Val;
            strVal = o.strVal;
            if (type == Type::Vector) vecVal = o.vecVal;
            else if (type == Type::QAngle) angVal = o.angVal;
        }
        return *this;
    }

    PropertyValue(PropertyValue&& o) noexcept
        : type(o.type), u64Val(o.u64Val), strVal(std::move(o.strVal)) {
        if (type == Type::Vector) vecVal = o.vecVal;
        else if (type == Type::QAngle) angVal = o.angVal;
    }

    PropertyValue& operator=(PropertyValue&& o) noexcept {
        if (this != &o) {
            type = o.type;
            u64Val = o.u64Val;
            strVal = std::move(o.strVal);
            if (type == Type::Vector) vecVal = o.vecVal;
            else if (type == Type::QAngle) angVal = o.angVal;
        }
        return *this;
    }

    bool operator==(const PropertyValue& o) const noexcept;
    bool operator!=(const PropertyValue& o) const noexcept { return !(*this == o); }

    [[nodiscard]] std::string ToString() const;
};

// ============================================================================
// 2. Little-Endian Bitstream Reader & Writer (BitWriter / BitReader)
// ============================================================================

/**
 * @brief Little-endian bitstream writer conforming to Source 2 wire serialization.
 * Extracted from docs/simulators/cs2_network_delta_sim.cpp and networksystem.dll.
 */
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

    void WriteVarInt32(int32_t val) {
        // ZigZag encoding for signed LEB128
        auto encoded = static_cast<uint32_t>((val << 1) ^ (val >> 31));
        WriteVarUInt32(encoded);
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

    void WriteBytes(const uint8_t* data, size_t byteCount) {
        for (size_t i = 0; i < byteCount; ++i) {
            WriteUBits(data[i], 8);
        }
    }

    [[nodiscard]] size_t GetBitPosition() const noexcept { return m_BitPosition; }
    [[nodiscard]] size_t GetByteSize() const noexcept { return (m_BitPosition + 7) / 8; }
    [[nodiscard]] const std::vector<uint8_t>& GetData() const noexcept { return m_Buffer; }

private:
    std::vector<uint8_t> m_Buffer;
    size_t m_BitPosition{0};
};

/**
 * @brief Little-endian bitstream reader conforming to Source 2 wire serialization.
 * Extracted from docs/simulators/cs2_network_delta_sim.cpp and networksystem.dll.
 */
class BitReader {
public:
    BitReader(const uint8_t* data, size_t numBits)
        : m_Data(data), m_TotalBits(numBits), m_BitPosition(0), m_Overflow(false) {}

    explicit BitReader(const BitWriter& writer)
        : m_Data(writer.GetData().data()), m_TotalBits(writer.GetBitPosition()), m_BitPosition(0), m_Overflow(false) {}

    explicit BitReader(CBitReadStream& stream)
        : m_Data(stream.GetData()), m_TotalBits(stream.GetTotalBits()), m_BitPosition(stream.GetBitsRead()), m_Overflow(stream.HasOverflowed()) {}

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
            uint32_t byteVal = ReadUBits(8);
            result |= (byteVal & 0x7Fu) << shift;
            if (!(byteVal & 0x80u)) {
                return result;
            }
            shift += 7;
        }
        return result;
    }

    int32_t ReadVarInt32() {
        uint32_t raw = ReadVarUInt32();
        return static_cast<int32_t>((raw >> 1) ^ (-(static_cast<int32_t>(raw & 1))));
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

    bool ReadBytes(uint8_t* out, size_t byteCount) {
        if (m_BitPosition + (byteCount * 8) > m_TotalBits) {
            m_Overflow = true;
            return false;
        }
        for (size_t i = 0; i < byteCount; ++i) {
            out[i] = static_cast<uint8_t>(ReadUBits(8));
        }
        return true;
    }

    [[nodiscard]] size_t GetBitPosition() const noexcept { return m_BitPosition; }
    [[nodiscard]] size_t GetTotalBits() const noexcept { return m_TotalBits; }
    [[nodiscard]] size_t GetRemainingBits() const noexcept {
        return (m_BitPosition < m_TotalBits) ? (m_TotalBits - m_BitPosition) : 0;
    }
    [[nodiscard]] bool HasOverflowed() const noexcept { return m_Overflow; }

    void Seek(size_t bitPos) noexcept {
        m_BitPosition = std::min(bitPos, m_TotalBits);
    }

private:
    const uint8_t* m_Data{nullptr};
    size_t m_TotalBits{0};
    size_t m_BitPosition{0};
    bool m_Overflow{false};
};

// ============================================================================
// 3. Floating-Point Quantization Functions
// ============================================================================

[[nodiscard]] constexpr inline uint32_t QuantizeFloat(float value, float minVal, float maxVal, size_t bitCount) noexcept {
    float clamped = std::clamp(value, minVal, maxVal);
    float range = maxVal - minVal;
    if (range <= 1e-7f || bitCount == 0 || bitCount > 32) return 0;
    float normalized = (clamped - minVal) / range;
    uint64_t maxSteps = (1ULL << bitCount) - 1ULL;
    return static_cast<uint32_t>(normalized * static_cast<float>(maxSteps) + 0.5f);
}

[[nodiscard]] constexpr inline float DequantizeFloat(uint32_t rawBits, float minVal, float maxVal, size_t bitCount) noexcept {
    if (bitCount == 0 || bitCount > 32) return minVal;
    uint64_t maxSteps = (1ULL << bitCount) - 1ULL;
    if (maxSteps == 0) return minVal;
    float factor = static_cast<float>(rawBits) / static_cast<float>(maxSteps);
    return minVal + factor * (maxVal - minVal);
}

// ============================================================================
// 4. Complete 40-Operation FieldPath Huffman Codec & Virtual Machine
// ============================================================================

struct HuffmanCode {
    uint32_t code{0};
    uint8_t bitLength{0};
};

class FieldPathHuffmanCodec {
public:
    FieldPathHuffmanCodec();

    void EncodeOp(BitWriter& writer, FieldPathOp op) const;
    FieldPathOp DecodeOp(BitReader& reader) const;

    [[nodiscard]] const HuffmanCode& GetCode(FieldPathOp op) const noexcept {
        return m_CodeTable[static_cast<size_t>(op)];
    }

private:
    void BuildCanonicalTable();
    HuffmanCode m_CodeTable[40];
};

/**
 * @brief Source 2 FieldPath Encoder / Decoder State Machine.
 * Supports all 40 operations from networksystem.dll.
 */
class FieldPathStreamEngine {
public:
    explicit FieldPathStreamEngine(const FieldPathHuffmanCodec& codec)
        : m_Codec(codec) {}

    void WriteFieldPaths(BitWriter& writer, const std::vector<FieldPath>& paths) const;
    std::vector<FieldPath> ReadFieldPaths(BitReader& reader) const;

    void EncodeTransition(BitWriter& writer, const FieldPath& curr, const FieldPath& next) const;
    void ExecuteOp(BitReader& reader, FieldPath& path, FieldPathOp op) const;

private:
    const FieldPathHuffmanCodec& m_Codec;
};

// ============================================================================
// 5. Entity Update Types & Stream Framing
// ============================================================================

enum class EntityUpdateType : uint8_t {
    Preserve    = 0, // Entity state unchanged
    LeavePVS    = 1, // Entity leaves visibility (enters dormancy)
    EnterPVS    = 2, // Entity enters visibility (created / baseline instantiated)
    DeltaUpdate = 3, // Entity properties delta compressed
    Delete      = 4  // Entity destroyed / purged
};

struct EntityDeltaHeader {
    uint32_t entityIndex{0};
    EntityUpdateType updateType{EntityUpdateType::Preserve};
    uint32_t classId{0};
    uint32_t serialNumber{0};
};

// ============================================================================
// 6. Entity Baseline Snapshot Storage
// ============================================================================

class CEntityBaselineStorage {
public:
    static constexpr uint32_t MAX_BASELINES = 64;

    CEntityBaselineStorage() = default;

    void SetBaseline(uint32_t baselineIndex, uint32_t entityIndex,
                     std::unordered_map<std::string, PropertyValue> properties);

    [[nodiscard]] const std::unordered_map<std::string, PropertyValue>* GetBaseline(
        uint32_t baselineIndex, uint32_t entityIndex) const;

    void CopyBaseline(uint32_t srcBaseline, uint32_t dstBaseline);
    void ClearBaseline(uint32_t baselineIndex);
    void ClearAll();

    [[nodiscard]] size_t GetEntityCount(uint32_t baselineIndex) const;

private:
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::unordered_map<std::string, PropertyValue>>> m_Baselines;
};

// ============================================================================
// 7. Field Change Callback Dispatcher
// ============================================================================

using FieldChangeCallbackFn = std::function<void(
    uint32_t entityIndex,
    const std::string& fieldName,
    const FieldPath& path,
    const PropertyValue& oldValue,
    const PropertyValue& newValue)>;

class CFieldChangeDispatcher {
public:
    CFieldChangeDispatcher() = default;

    uint32_t RegisterCallback(const std::string& fieldName, FieldChangeCallbackFn callback);
    void UnregisterCallback(uint32_t callbackId);

    void Dispatch(uint32_t entityIndex,
                  const std::string& fieldName,
                  const FieldPath& path,
                  const PropertyValue& oldValue,
                  const PropertyValue& newValue) const;

    [[nodiscard]] size_t GetCallbackCount() const noexcept { return m_Callbacks.size(); }

private:
    struct CallbackEntry {
        uint32_t id{0};
        std::string fieldName;
        FieldChangeCallbackFn callback;
    };

    uint32_t m_NextId{1};
    std::vector<CallbackEntry> m_Callbacks;
};

// ============================================================================
// 8. Active Entity State Snapshot
// ============================================================================

class CEntitySnapshot {
public:
    uint32_t m_nEntityIndex{0};
    uint32_t m_nClassId{0};
    uint32_t m_nSerialNumber{0};
    bool m_bActive{false};
    bool m_bDormant{false};

    std::unordered_map<std::string, PropertyValue> m_Properties;
    sdk::CNetworkPropertyDirtyBitmask<1024> m_DirtyBitmask;

    [[nodiscard]] const PropertyValue* GetProperty(const std::string& name) const;
    void SetProperty(const std::string& name, PropertyValue value);
    [[nodiscard]] bool HasProperty(const std::string& name) const;
};

// ============================================================================
// 9. Source 2 Entity Delta Codec (Clean-Room Engine)
// ============================================================================

class CEntityDeltaCodec {
public:
    CEntityDeltaCodec() = default;

    void RegisterSerializer(uint32_t classId, CFlattenedSerializer serializer);
    [[nodiscard]] const CFlattenedSerializer* GetSerializer(uint32_t classId) const;

    [[nodiscard]] CEntityBaselineStorage& GetBaselineStorage() noexcept { return m_BaselineStorage; }
    [[nodiscard]] const CEntityBaselineStorage& GetBaselineStorage() const noexcept { return m_BaselineStorage; }

    [[nodiscard]] CFieldChangeDispatcher& GetDispatcher() noexcept { return m_Dispatcher; }
    [[nodiscard]] const CFieldChangeDispatcher& GetDispatcher() const noexcept { return m_Dispatcher; }

    [[nodiscard]] const CEntitySnapshot* GetEntity(uint32_t entityIndex) const;
    [[nodiscard]] CEntitySnapshot* GetEntityMutable(uint32_t entityIndex);
    CEntitySnapshot& CreateEntity(uint32_t entityIndex, uint32_t classId = 0) {
        auto& ent = m_ActiveEntities[entityIndex];
        ent.m_nEntityIndex = entityIndex;
        ent.m_nClassId = classId;
        ent.m_bActive = true;
        return ent;
    }
    [[nodiscard]] const std::unordered_map<uint32_t, CEntitySnapshot>& GetAllEntities() const noexcept { return m_ActiveEntities; }

    bool DecodePacketEntities(const sdk::CSVCMsg_PacketEntities& msg,
                             const uint8_t* pPayloadData,
                             size_t nPayloadBytes);

    bool EncodePacketEntities(const std::vector<uint32_t>& updatedEntityIndices,
                             bool isDelta,
                             int32_t deltaFrom,
                             int32_t baselineIndex,
                             sdk::CSVCMsg_PacketEntities& outMsg,
                             CBitWriteStream& outStream);

    static PropertyValue DecodeFieldValue(CBitReadStream& stream, const SerializedFieldInfo& fieldInfo);
    static void EncodeFieldValue(CBitWriteStream& stream, const SerializedFieldInfo& fieldInfo, const PropertyValue& val);

    static uint32_t ReadEntityIndexDelta(CBitReadStream& stream, uint32_t prevIndex);
    static void WriteEntityIndexDelta(CBitWriteStream& stream, uint32_t currentIndex, uint32_t prevIndex);

    static EntityDeltaHeader ReadEntityHeader(CBitReadStream& stream, uint32_t prevIndex, bool isDelta);
    static void WriteEntityHeader(CBitWriteStream& stream, const EntityDeltaHeader& header, uint32_t prevIndex, bool isDelta);

    [[nodiscard]] uint32_t GetServerTick() const noexcept { return m_nServerTick; }

private:
    bool DecodeSingleEntity(CBitReadStream& stream, const EntityDeltaHeader& header, int32_t baselineIndex);

    uint32_t m_nServerTick{0};
    CEntityBaselineStorage m_BaselineStorage;
    CFieldChangeDispatcher m_Dispatcher;
    std::unordered_map<uint32_t, CFlattenedSerializer> m_Serializers;
    std::unordered_map<uint32_t, CEntitySnapshot> m_ActiveEntities;
};

} // namespace cs2::networksystem

#endif // CS2_ENTITY_DELTA_CODEC_H
