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
// 2. Entity Update Types & Stream Framing
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
// 3. Entity Baseline Snapshot Storage
// ============================================================================

/**
 * @brief Thread-safe baseline snapshot storage cache.
 * Source 2 maintains baseline slots (0 = default, 1 = alternate, etc.) for entity templates.
 */
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
    // baselineIndex -> (entityIndex -> property map)
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::unordered_map<std::string, PropertyValue>>> m_Baselines;
};

// ============================================================================
// 4. Field Change Callback Dispatcher
// ============================================================================

using FieldChangeCallbackFn = std::function<void(
    uint32_t entityIndex,
    const std::string& fieldName,
    const FieldPath& path,
    const PropertyValue& oldValue,
    const PropertyValue& newValue)>;

/**
 * @brief Dispatches property change notifications to registered client subsystems.
 */
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
// 5. Active Entity State Snapshot
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
// 6. Source 2 Entity Delta Codec (Clean-Room Engine)
// ============================================================================

/**
 * @brief Complete reconstructed Source 2 entity delta decompression and serialization engine.
 */
class CEntityDeltaCodec {
public:
    CEntityDeltaCodec() = default;

    // Serializer registration
    void RegisterSerializer(uint32_t classId, CFlattenedSerializer serializer);
    [[nodiscard]] const CFlattenedSerializer* GetSerializer(uint32_t classId) const;

    // Baseline storage access
    [[nodiscard]] CEntityBaselineStorage& GetBaselineStorage() noexcept { return m_BaselineStorage; }
    [[nodiscard]] const CEntityBaselineStorage& GetBaselineStorage() const noexcept { return m_BaselineStorage; }

    // Dispatcher access
    [[nodiscard]] CFieldChangeDispatcher& GetDispatcher() noexcept { return m_Dispatcher; }
    [[nodiscard]] const CFieldChangeDispatcher& GetDispatcher() const noexcept { return m_Dispatcher; }

    // Active entity snapshot lookup
    [[nodiscard]] const CEntitySnapshot* GetEntity(uint32_t entityIndex) const;
    [[nodiscard]] CEntitySnapshot* GetEntityMutable(uint32_t entityIndex);
    [[nodiscard]] const std::unordered_map<uint32_t, CEntitySnapshot>& GetAllEntities() const noexcept { return m_ActiveEntities; }

    /**
     * @brief Decodes a full CSVCMsg_PacketEntities message with bitstream payload.
     */
    bool DecodePacketEntities(const sdk::CSVCMsg_PacketEntities& msg,
                             const uint8_t* pPayloadData,
                             size_t nPayloadBytes);

    /**
     * @brief Encodes an entity delta snapshot into a CSVCMsg_PacketEntities message and bitstream.
     */
    bool EncodePacketEntities(const std::vector<uint32_t>& updatedEntityIndices,
                             bool isDelta,
                             int32_t deltaFrom,
                             int32_t baselineIndex,
                             sdk::CSVCMsg_PacketEntities& outMsg,
                             CBitWriteStream& outStream);

    // Bitstream low-level wire serialization helpers
    static PropertyValue DecodeFieldValue(CBitReadStream& stream, const SerializedFieldInfo& fieldInfo);
    static void EncodeFieldValue(CBitWriteStream& stream, const SerializedFieldInfo& fieldInfo, const PropertyValue& val);

    // Entity index delta bitstream reading
    static uint32_t ReadEntityIndexDelta(CBitReadStream& stream, uint32_t prevIndex);
    static void WriteEntityIndexDelta(CBitWriteStream& stream, uint32_t currentIndex, uint32_t prevIndex);

    // Entity delta header reading
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
