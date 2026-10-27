#include "entity_delta_codec.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace cs2::networksystem {

// ============================================================================
// 1. PropertyValue Implementation
// ============================================================================

bool PropertyValue::operator==(const PropertyValue& o) const noexcept {
    if (type != o.type) return false;
    switch (type) {
    case Type::None: return true;
    case Type::Bool: return bVal == o.bVal;
    case Type::Int32: return i32Val == o.i32Val;
    case Type::UInt32: return u32Val == o.u32Val;
    case Type::Int64: return i64Val == o.i64Val;
    case Type::UInt64: return u64Val == o.u64Val;
    case Type::Float: return std::abs(fVal - o.fVal) < 1e-5f;
    case Type::Vector: return vecVal == o.vecVal;
    case Type::QAngle: return angVal == o.angVal;
    case Type::String: return strVal == o.strVal;
    case Type::Handle: return handleVal == o.handleVal;
    default: return false;
    }
}

std::string PropertyValue::ToString() const {
    std::ostringstream ss;
    switch (type) {
    case Type::None: return "None";
    case Type::Bool: return bVal ? "true" : "false";
    case Type::Int32: ss << i32Val; return ss.str();
    case Type::UInt32: ss << u32Val; return ss.str();
    case Type::Int64: ss << i64Val; return ss.str();
    case Type::UInt64: ss << u64Val; return ss.str();
    case Type::Float:
        ss << std::fixed << std::setprecision(3) << fVal;
        return ss.str();
    case Type::Vector:
        ss << std::fixed << std::setprecision(2)
           << "(" << vecVal.x << ", " << vecVal.y << ", " << vecVal.z << ")";
        return ss.str();
    case Type::QAngle:
        ss << std::fixed << std::setprecision(2)
           << "{" << angVal.pitch << ", " << angVal.yaw << ", " << angVal.roll << "}";
        return ss.str();
    case Type::String: return "\"" + strVal + "\"";
    case Type::Handle:
        ss << "0x" << std::hex << handleVal;
        return ss.str();
    default: return "Unknown";
    }
}

// ============================================================================
// 2. CEntityBaselineStorage Implementation
// ============================================================================

void CEntityBaselineStorage::SetBaseline(uint32_t baselineIndex, uint32_t entityIndex,
                                         std::unordered_map<std::string, PropertyValue> properties) {
    m_Baselines[baselineIndex][entityIndex] = std::move(properties);
}

const std::unordered_map<std::string, PropertyValue>* CEntityBaselineStorage::GetBaseline(
    uint32_t baselineIndex, uint32_t entityIndex) const {
    auto bIt = m_Baselines.find(baselineIndex);
    if (bIt == m_Baselines.end()) return nullptr;

    auto eIt = bIt->second.find(entityIndex);
    if (eIt == bIt->second.end()) return nullptr;

    return &eIt->second;
}

void CEntityBaselineStorage::CopyBaseline(uint32_t srcBaseline, uint32_t dstBaseline) {
    auto it = m_Baselines.find(srcBaseline);
    if (it != m_Baselines.end()) {
        m_Baselines[dstBaseline] = it->second;
    } else {
        m_Baselines.erase(dstBaseline);
    }
}

void CEntityBaselineStorage::ClearBaseline(uint32_t baselineIndex) {
    m_Baselines.erase(baselineIndex);
}

void CEntityBaselineStorage::ClearAll() {
    m_Baselines.clear();
}

size_t CEntityBaselineStorage::GetEntityCount(uint32_t baselineIndex) const {
    auto it = m_Baselines.find(baselineIndex);
    return (it != m_Baselines.end()) ? it->second.size() : 0;
}

// ============================================================================
// 3. CFieldChangeDispatcher Implementation
// ============================================================================

uint32_t CFieldChangeDispatcher::RegisterCallback(const std::string& fieldName, FieldChangeCallbackFn callback) {
    const uint32_t id = m_NextId++;
    m_Callbacks.push_back(CallbackEntry{id, fieldName, std::move(callback)});
    return id;
}

void CFieldChangeDispatcher::UnregisterCallback(uint32_t callbackId) {
    std::erase_if(m_Callbacks, [callbackId](const CallbackEntry& entry) {
        return entry.id == callbackId;
    });
}

void CFieldChangeDispatcher::Dispatch(uint32_t entityIndex,
                                      const std::string& fieldName,
                                      const FieldPath& path,
                                      const PropertyValue& oldValue,
                                      const PropertyValue& newValue) const {
    for (const auto& entry : m_Callbacks) {
        if (entry.fieldName.empty() || entry.fieldName == "*" || entry.fieldName == fieldName) {
            entry.callback(entityIndex, fieldName, path, oldValue, newValue);
        }
    }
}

// ============================================================================
// 4. CEntitySnapshot Implementation
// ============================================================================

const PropertyValue* CEntitySnapshot::GetProperty(const std::string& name) const {
    auto it = m_Properties.find(name);
    return (it != m_Properties.end()) ? &it->second : nullptr;
}

void CEntitySnapshot::SetProperty(const std::string& name, PropertyValue value) {
    m_Properties[name] = std::move(value);
}

bool CEntitySnapshot::HasProperty(const std::string& name) const {
    return m_Properties.contains(name);
}

// ============================================================================
// 5. CEntityDeltaCodec Implementation
// ============================================================================

void CEntityDeltaCodec::RegisterSerializer(uint32_t classId, CFlattenedSerializer serializer) {
    m_Serializers.insert_or_assign(classId, std::move(serializer));
}

const CFlattenedSerializer* CEntityDeltaCodec::GetSerializer(uint32_t classId) const {
    auto it = m_Serializers.find(classId);
    return (it != m_Serializers.end()) ? &it->second : nullptr;
}

const CEntitySnapshot* CEntityDeltaCodec::GetEntity(uint32_t entityIndex) const {
    auto it = m_ActiveEntities.find(entityIndex);
    return (it != m_ActiveEntities.end()) ? &it->second : nullptr;
}

CEntitySnapshot* CEntityDeltaCodec::GetEntityMutable(uint32_t entityIndex) {
    auto it = m_ActiveEntities.find(entityIndex);
    return (it != m_ActiveEntities.end()) ? &it->second : nullptr;
}

// Entity index delta reading:
// Bit 0 == 0: Consecutive (+1)
// Bit 0 == 1: Tiered jump
uint32_t CEntityDeltaCodec::ReadEntityIndexDelta(CBitReadStream& stream, uint32_t prevIndex) {
    if (stream.ReadUBits(1) == 0) {
        return prevIndex + 1;
    }
    const uint32_t tier = stream.ReadUBits(2);
    switch (tier) {
    case 0: return prevIndex + 2 + stream.ReadUBits(3);
    case 1: return prevIndex + 10 + stream.ReadUBits(6);
    case 2: return prevIndex + 74 + stream.ReadUBits(9);
    case 3: return stream.ReadUBits(14); // Absolute index
    default: return prevIndex + 1;
    }
}

void CEntityDeltaCodec::WriteEntityIndexDelta(CBitWriteStream& stream, uint32_t currentIndex, uint32_t prevIndex) {
    if (currentIndex == prevIndex + 1) {
        stream.WriteUBits(0, 1);
        return;
    }

    stream.WriteUBits(1, 1);
    const uint32_t delta = currentIndex - prevIndex;
    if (delta >= 2 && delta < 10) {
        stream.WriteUBits(0, 2);
        stream.WriteUBits(delta - 2, 3);
    } else if (delta >= 10 && delta < 74) {
        stream.WriteUBits(1, 2);
        stream.WriteUBits(delta - 10, 6);
    } else if (delta >= 74 && delta < 586) {
        stream.WriteUBits(2, 2);
        stream.WriteUBits(delta - 74, 9);
    } else {
        stream.WriteUBits(3, 2);
        stream.WriteUBits(currentIndex & 0x3FFF, 14);
    }
}

EntityDeltaHeader CEntityDeltaCodec::ReadEntityHeader(CBitReadStream& stream, uint32_t prevIndex, bool isDelta) {
    EntityDeltaHeader header;
    header.entityIndex = ReadEntityIndexDelta(stream, prevIndex);

    if (!isDelta) {
        // Full update / EnterPVS
        header.updateType = EntityUpdateType::EnterPVS;
        header.classId = stream.ReadVarInt32();
        header.serialNumber = stream.ReadUBits(17);
        return header;
    }

    // Delta update mode: 2-bit PVS status flag
    const uint32_t pvsFlag = stream.ReadUBits(2);
    switch (pvsFlag) {
    case 0:
        header.updateType = EntityUpdateType::Preserve;
        break;
    case 1:
        // Entity leaves PVS
        header.updateType = EntityUpdateType::LeavePVS;
        if (stream.ReadUBits(1) != 0) {
            header.updateType = EntityUpdateType::Delete;
        }
        break;
    case 2:
        // Entity enters PVS
        header.updateType = EntityUpdateType::EnterPVS;
        header.classId = stream.ReadVarInt32();
        header.serialNumber = stream.ReadUBits(17);
        break;
    case 3:
    default:
        // Existing entity delta property update
        header.updateType = EntityUpdateType::DeltaUpdate;
        break;
    }

    return header;
}

void CEntityDeltaCodec::WriteEntityHeader(CBitWriteStream& stream, const EntityDeltaHeader& header,
                                         uint32_t prevIndex, bool isDelta) {
    WriteEntityIndexDelta(stream, header.entityIndex, prevIndex);

    if (!isDelta) {
        stream.WriteVarInt32(header.classId);
        stream.WriteUBits(header.serialNumber & 0x1FFFF, 17);
        return;
    }

    switch (header.updateType) {
    case EntityUpdateType::Preserve:
        stream.WriteUBits(0, 2);
        break;
    case EntityUpdateType::LeavePVS:
        stream.WriteUBits(1, 2);
        stream.WriteUBits(0, 1);
        break;
    case EntityUpdateType::Delete:
        stream.WriteUBits(1, 2);
        stream.WriteUBits(1, 1);
        break;
    case EntityUpdateType::EnterPVS:
        stream.WriteUBits(2, 2);
        stream.WriteVarInt32(header.classId);
        stream.WriteUBits(header.serialNumber & 0x1FFFF, 17);
        break;
    case EntityUpdateType::DeltaUpdate:
        stream.WriteUBits(3, 2);
        break;
    }
}

PropertyValue CEntityDeltaCodec::DecodeFieldValue(CBitReadStream& stream, const SerializedFieldInfo& fieldInfo) {
    const std::string& type = fieldInfo.varType;

    if (type == "bool") {
        return PropertyValue(stream.ReadUBits(1) != 0);
    }
    if (type == "uint8" || type == "int8") {
        return PropertyValue(stream.ReadUBits(8));
    }
    if (type == "uint16" || type == "int16") {
        return PropertyValue(stream.ReadUBits(16));
    }
    if (type == "uint32" || type == "int32") {
        return PropertyValue(stream.ReadUBits(32));
    }
    if (type == "uint64" || type == "int64") {
        const uint64_t low = stream.ReadUBits(32);
        const uint64_t high = stream.ReadUBits(32);
        return PropertyValue(static_cast<uint64_t>(low | (high << 32)));
    }
    if (type == "float32" || type == "float") {
        if (fieldInfo.bitCount > 0 && fieldInfo.bitCount <= 32) {
            // Source 2 Quantized Float
            const uint32_t rawBits = stream.ReadUBits(static_cast<size_t>(fieldInfo.bitCount));
            const auto maxVal = static_cast<float>((1ULL << fieldInfo.bitCount) - 1);
            const float norm = (maxVal > 0.0f) ? static_cast<float>(rawBits) / maxVal : 0.0f;
            const float val = fieldInfo.lowValue + norm * (fieldInfo.highValue - fieldInfo.lowValue);
            return PropertyValue(val);
        }
        // Standard IEEE 754 32-bit float
        const uint32_t raw = stream.ReadUBits(32);
        float fVal = 0.0f;
        std::memcpy(&fVal, &raw, sizeof(fVal));
        return PropertyValue(fVal);
    }
    if (type == "Vector" || type == "Vector3") {
        Vector3_t v{};
        if (fieldInfo.bitCount > 0) {
            const auto maxVal = static_cast<float>((1ULL << fieldInfo.bitCount) - 1);
            const float range = fieldInfo.highValue - fieldInfo.lowValue;
            v.x = fieldInfo.lowValue + (static_cast<float>(stream.ReadUBits(static_cast<size_t>(fieldInfo.bitCount))) / maxVal) * range;
            v.y = fieldInfo.lowValue + (static_cast<float>(stream.ReadUBits(static_cast<size_t>(fieldInfo.bitCount))) / maxVal) * range;
            v.z = fieldInfo.lowValue + (static_cast<float>(stream.ReadUBits(static_cast<size_t>(fieldInfo.bitCount))) / maxVal) * range;
        } else {
            uint32_t rx = stream.ReadUBits(32);
            uint32_t ry = stream.ReadUBits(32);
            uint32_t rz = stream.ReadUBits(32);
            std::memcpy(&v.x, &rx, sizeof(float));
            std::memcpy(&v.y, &ry, sizeof(float));
            std::memcpy(&v.z, &rz, sizeof(float));
        }
        return PropertyValue(v);
    }
    if (type == "QAngle") {
        QAngle_t a{};
        if (fieldInfo.bitCount > 0) {
            const auto maxVal = static_cast<float>((1ULL << fieldInfo.bitCount) - 1);
            const float range = fieldInfo.highValue - fieldInfo.lowValue;
            a.pitch = fieldInfo.lowValue + (static_cast<float>(stream.ReadUBits(static_cast<size_t>(fieldInfo.bitCount))) / maxVal) * range;
            a.yaw   = fieldInfo.lowValue + (static_cast<float>(stream.ReadUBits(static_cast<size_t>(fieldInfo.bitCount))) / maxVal) * range;
            a.roll  = fieldInfo.lowValue + (static_cast<float>(stream.ReadUBits(static_cast<size_t>(fieldInfo.bitCount))) / maxVal) * range;
        } else {
            uint32_t rp = stream.ReadUBits(32);
            uint32_t ry = stream.ReadUBits(32);
            uint32_t rr = stream.ReadUBits(32);
            std::memcpy(&a.pitch, &rp, sizeof(float));
            std::memcpy(&a.yaw,   &ry, sizeof(float));
            std::memcpy(&a.roll,  &rr, sizeof(float));
        }
        return PropertyValue(a);
    }
    if (type == "CEntityHandle" || type == "CHandle") {
        return PropertyValue::FromHandle(stream.ReadUBits(32));
    }
    if (type == "CUtlString" || type == "string") {
        const uint32_t len = stream.ReadVarInt32();
        std::vector<uint8_t> strBytes(len);
        if (stream.ReadBytes(strBytes.data(), len)) {
            return PropertyValue(std::string(reinterpret_cast<char*>(strBytes.data()), len));
        }
        return PropertyValue(std::string{});
    }

    // Default fallback: 32-bit unsigned integer
    return PropertyValue(stream.ReadUBits(32));
}

void CEntityDeltaCodec::EncodeFieldValue(CBitWriteStream& stream, const SerializedFieldInfo& fieldInfo, const PropertyValue& val) {
    const std::string& type = fieldInfo.varType;

    if (type == "bool") {
        stream.WriteUBits(val.bVal ? 1 : 0, 1);
        return;
    }
    if (type == "uint8" || type == "int8") {
        stream.WriteUBits(val.u32Val & 0xFF, 8);
        return;
    }
    if (type == "uint16" || type == "int16") {
        stream.WriteUBits(val.u32Val & 0xFFFF, 16);
        return;
    }
    if (type == "uint32" || type == "int32") {
        stream.WriteUBits(val.u32Val, 32);
        return;
    }
    if (type == "uint64" || type == "int64") {
        stream.WriteUBits(static_cast<uint32_t>(val.u64Val & 0xFFFFFFFF), 32);
        stream.WriteUBits(static_cast<uint32_t>(val.u64Val >> 32), 32);
        return;
    }
    if (type == "float32" || type == "float") {
        if (fieldInfo.bitCount > 0 && fieldInfo.bitCount <= 32) {
            const float clamped = std::clamp(val.fVal, fieldInfo.lowValue, fieldInfo.highValue);
            const float range = fieldInfo.highValue - fieldInfo.lowValue;
            const float norm = (range > 0.0f) ? (clamped - fieldInfo.lowValue) / range : 0.0f;
            const auto maxVal = static_cast<float>((1ULL << fieldInfo.bitCount) - 1);
            const auto raw = static_cast<uint32_t>(norm * maxVal + 0.5f);
            stream.WriteUBits(raw, static_cast<size_t>(fieldInfo.bitCount));
            return;
        }
        uint32_t raw = 0;
        std::memcpy(&raw, &val.fVal, sizeof(raw));
        stream.WriteUBits(raw, 32);
        return;
    }
    if (type == "Vector" || type == "Vector3") {
        if (fieldInfo.bitCount > 0) {
            const auto maxVal = static_cast<float>((1ULL << fieldInfo.bitCount) - 1);
            const float range = fieldInfo.highValue - fieldInfo.lowValue;
            auto encodeComp = [&](float comp) {
                const float c = std::clamp(comp, fieldInfo.lowValue, fieldInfo.highValue);
                const float n = (range > 0.0f) ? (c - fieldInfo.lowValue) / range : 0.0f;
                return static_cast<uint32_t>(n * maxVal + 0.5f);
            };
            stream.WriteUBits(encodeComp(val.vecVal.x), static_cast<size_t>(fieldInfo.bitCount));
            stream.WriteUBits(encodeComp(val.vecVal.y), static_cast<size_t>(fieldInfo.bitCount));
            stream.WriteUBits(encodeComp(val.vecVal.z), static_cast<size_t>(fieldInfo.bitCount));
        } else {
            uint32_t rx = 0, ry = 0, rz = 0;
            std::memcpy(&rx, &val.vecVal.x, sizeof(float));
            std::memcpy(&ry, &val.vecVal.y, sizeof(float));
            std::memcpy(&rz, &val.vecVal.z, sizeof(float));
            stream.WriteUBits(rx, 32);
            stream.WriteUBits(ry, 32);
            stream.WriteUBits(rz, 32);
        }
        return;
    }
    if (type == "QAngle") {
        if (fieldInfo.bitCount > 0) {
            const auto maxVal = static_cast<float>((1ULL << fieldInfo.bitCount) - 1);
            const float range = fieldInfo.highValue - fieldInfo.lowValue;
            auto encodeComp = [&](float comp) {
                const float c = std::clamp(comp, fieldInfo.lowValue, fieldInfo.highValue);
                const float n = (range > 0.0f) ? (c - fieldInfo.lowValue) / range : 0.0f;
                return static_cast<uint32_t>(n * maxVal + 0.5f);
            };
            stream.WriteUBits(encodeComp(val.angVal.pitch), static_cast<size_t>(fieldInfo.bitCount));
            stream.WriteUBits(encodeComp(val.angVal.yaw),   static_cast<size_t>(fieldInfo.bitCount));
            stream.WriteUBits(encodeComp(val.angVal.roll),  static_cast<size_t>(fieldInfo.bitCount));
        } else {
            uint32_t rp = 0, ry = 0, rr = 0;
            std::memcpy(&rp, &val.angVal.pitch, sizeof(float));
            std::memcpy(&ry, &val.angVal.yaw,   sizeof(float));
            std::memcpy(&rr, &val.angVal.roll,  sizeof(float));
            stream.WriteUBits(rp, 32);
            stream.WriteUBits(ry, 32);
            stream.WriteUBits(rr, 32);
        }
        return;
    }
    if (type == "CEntityHandle" || type == "CHandle") {
        stream.WriteUBits(val.handleVal, 32);
        return;
    }
    if (type == "CUtlString" || type == "string") {
        stream.WriteVarInt32(static_cast<uint32_t>(val.strVal.size()));
        stream.WriteBytes(reinterpret_cast<const uint8_t*>(val.strVal.data()), val.strVal.size());
        return;
    }

    stream.WriteUBits(val.u32Val, 32);
}

bool CEntityDeltaCodec::DecodeSingleEntity(CBitReadStream& stream, const EntityDeltaHeader& header, int32_t baselineIndex) {
    if (header.updateType == EntityUpdateType::Preserve) {
        return true;
    }

    if (header.updateType == EntityUpdateType::LeavePVS) {
        auto it = m_ActiveEntities.find(header.entityIndex);
        if (it != m_ActiveEntities.end()) {
            it->second.m_bDormant = true;
        }
        return true;
    }

    if (header.updateType == EntityUpdateType::Delete) {
        m_ActiveEntities.erase(header.entityIndex);
        return true;
    }

    // For EnterPVS or DeltaUpdate, resolve serializer
    CEntitySnapshot* pEntity = nullptr;

    if (header.updateType == EntityUpdateType::EnterPVS) {
        auto& ent = m_ActiveEntities[header.entityIndex];
        ent.m_nEntityIndex = header.entityIndex;
        ent.m_nClassId = header.classId;
        ent.m_nSerialNumber = header.serialNumber;
        ent.m_bActive = true;
        ent.m_bDormant = false;

        // Copy baseline properties if available
        if (baselineIndex >= 0) {
            const auto* pBaseline = m_BaselineStorage.GetBaseline(static_cast<uint32_t>(baselineIndex), header.entityIndex);
            if (pBaseline) {
                ent.m_Properties = *pBaseline;
            }
        }
        pEntity = &ent;
    } else {
        auto it = m_ActiveEntities.find(header.entityIndex);
        if (it == m_ActiveEntities.end()) {
            return false; // Received delta for non-existent entity
        }
        pEntity = &it->second;
        pEntity->m_bDormant = false;
    }

    const auto* pSerializer = GetSerializer(pEntity->m_nClassId);
    if (!pSerializer) {
        return false; // Missing serializer for class
    }

    // Decode changed field paths from bitstream using 40-op Huffman tree
    const std::vector<FieldPath> fieldPaths = CFlattenedSerializer::DecodeFieldPaths(stream);
    const auto& serializerFields = pSerializer->GetFields();

    for (const auto& fp : fieldPaths) {
        if (fp.path.empty()) continue;

        // In flattened serializers, root path index addresses the flattened field array
        const auto fieldIdx = static_cast<size_t>(fp.path[0]);
        if (fieldIdx >= serializerFields.size()) {
            continue;
        }

        const auto& fieldInfo = serializerFields[fieldIdx];
        const PropertyValue newVal = DecodeFieldValue(stream, fieldInfo);

        // Track change & dispatch callbacks
        PropertyValue oldVal;
        auto propIt = pEntity->m_Properties.find(fieldInfo.varName);
        if (propIt != pEntity->m_Properties.end()) {
            oldVal = propIt->second;
        }

        pEntity->m_Properties[fieldInfo.varName] = newVal;
        pEntity->m_DirtyBitmask.Set(fieldIdx);

        if (oldVal != newVal) {
            m_Dispatcher.Dispatch(header.entityIndex, fieldInfo.varName, fp, oldVal, newVal);
        }
    }

    return true;
}

bool CEntityDeltaCodec::DecodePacketEntities(const sdk::CSVCMsg_PacketEntities& msg,
                                            const uint8_t* pPayloadData,
                                            size_t nPayloadBytes) {
    if (!pPayloadData || nPayloadBytes == 0) {
        return false;
    }

    m_nServerTick = msg.server_tick;
    CBitReadStream stream(pPayloadData, nPayloadBytes, true);

    const int32_t totalUpdated = msg.updated_entries;
    const bool isDelta = !msg.pending_full_frame && msg.legacy_is_delta;
    const int32_t baselineIdx = msg.baseline;

    uint32_t currentEntityIndex = 0;
    int32_t entriesRead = 0;

    while (entriesRead < totalUpdated && !stream.HasOverflowed() && stream.GetBitsRead() < stream.GetTotalBits()) {
        const EntityDeltaHeader header = ReadEntityHeader(stream, currentEntityIndex, isDelta);
        currentEntityIndex = header.entityIndex;

        if (!DecodeSingleEntity(stream, header, baselineIdx)) {
            return false;
        }
        entriesRead++;
    }

    // If update_baseline flag is set, synchronize active entities into baseline snapshot
    if (msg.update_baseline && baselineIdx >= 0) {
        for (const auto& [idx, ent] : m_ActiveEntities) {
            m_BaselineStorage.SetBaseline(static_cast<uint32_t>(baselineIdx), idx, ent.m_Properties);
        }
    }

    return true;
}

bool CEntityDeltaCodec::EncodePacketEntities(const std::vector<uint32_t>& updatedEntityIndices,
                                            bool isDelta,
                                            int32_t deltaFrom,
                                            int32_t baselineIndex,
                                            sdk::CSVCMsg_PacketEntities& outMsg,
                                            CBitWriteStream& outStream) {
    outMsg.delta_from = deltaFrom;
    outMsg.baseline = baselineIndex;
    outMsg.legacy_is_delta = isDelta;
    outMsg.pending_full_frame = !isDelta;
    outMsg.updated_entries = static_cast<int32_t>(updatedEntityIndices.size());
    outMsg.server_tick = m_nServerTick;

    uint32_t prevEntityIndex = 0;

    for (uint32_t entIdx : updatedEntityIndices) {
        auto it = m_ActiveEntities.find(entIdx);
        if (it == m_ActiveEntities.end()) continue;

        const auto& ent = it->second;
        EntityDeltaHeader header;
        header.entityIndex = entIdx;
        header.classId = ent.m_nClassId;
        header.serialNumber = ent.m_nSerialNumber;
        header.updateType = isDelta ? EntityUpdateType::DeltaUpdate : EntityUpdateType::EnterPVS;

        WriteEntityHeader(outStream, header, prevEntityIndex, isDelta);
        prevEntityIndex = entIdx;

        const auto* pSerializer = GetSerializer(ent.m_nClassId);
        if (!pSerializer) continue;

        const auto& fields = pSerializer->GetFields();

        // Phase 1: Emit all Huffman FieldPaths
        std::vector<size_t> changedFieldIndices;
        int32_t lastFieldPath = -1;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (!ent.m_DirtyBitmask.IsSet(i) && isDelta) continue;

            const auto& fInfo = fields[i];
            auto pIt = ent.m_Properties.find(fInfo.varName);
            if (pIt == ent.m_Properties.end()) continue;

            changedFieldIndices.push_back(i);

            // Huffman delta: emit PlusN / PlusOne
            const auto targetPath = static_cast<int32_t>(i);
            const int32_t delta = targetPath - lastFieldPath;
            if (delta == 1) {
                outStream.WriteUBits(static_cast<uint32_t>(FieldPathOp::PlusOne), 4);
            } else if (delta == 2) {
                outStream.WriteUBits(static_cast<uint32_t>(FieldPathOp::PlusTwo), 4);
            } else if (delta == 3) {
                outStream.WriteUBits(static_cast<uint32_t>(FieldPathOp::PlusThree), 4);
            } else if (delta == 4) {
                outStream.WriteUBits(static_cast<uint32_t>(FieldPathOp::PlusFour), 4);
            } else {
                outStream.WriteUBits(static_cast<uint32_t>(FieldPathOp::PlusN), 4);
                outStream.WriteUBits(static_cast<uint32_t>(delta), 6);
            }
            lastFieldPath = targetPath;
        }

        // Terminal finish token for entity property update
        outStream.WriteUBits(static_cast<uint32_t>(FieldPathOp::Finish), 4);

        // Phase 2: Encode property values in order of changed field paths
        for (size_t idx : changedFieldIndices) {
            const auto& fInfo = fields[idx];
            auto pIt = ent.m_Properties.find(fInfo.varName);
            if (pIt != ent.m_Properties.end()) {
                EncodeFieldValue(outStream, fInfo, pIt->second);
            }
        }
    }

    return true;
}

} // namespace cs2::networksystem
