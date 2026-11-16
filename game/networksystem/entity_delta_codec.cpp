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
    case Type::Float: return std::abs(fVal - o.fVal) < 1e-4f;
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
// 2. 40-Operation FieldPath Huffman Codec Implementation
// ============================================================================

FieldPathHuffmanCodec::FieldPathHuffmanCodec() {
    BuildCanonicalTable();
}

void FieldPathHuffmanCodec::BuildCanonicalTable() {
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

    // Populate remaining ops in canonical prefix space
    uint32_t prefixBase = 0b111111111111;
    uint32_t subIndex = 0;
    for (size_t i = 0; i < 40; ++i) {
        if (m_CodeTable[i].bitLength == 0) {
            m_CodeTable[i] = {(prefixBase << 5) | (subIndex & 0x1Fu), 17};
            ++subIndex;
        }
    }
}

void FieldPathHuffmanCodec::EncodeOp(BitWriter& writer, FieldPathOp op) const {
    const auto& hc = m_CodeTable[static_cast<size_t>(op)];
    assert(hc.bitLength > 0);
    for (int i = static_cast<int>(hc.bitLength) - 1; i >= 0; --i) {
        writer.WriteBit((hc.code & (1u << i)) != 0);
    }
}

FieldPathOp FieldPathHuffmanCodec::DecodeOp(BitReader& reader) const {
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

// ============================================================================
// 3. FieldPathStreamEngine Virtual Machine
// ============================================================================

void FieldPathStreamEngine::WriteFieldPaths(BitWriter& writer, const std::vector<FieldPath>& paths) const {
    FieldPath currentPath({-1});

    for (const auto& target : paths) {
        EncodeTransition(writer, currentPath, target);
        currentPath = target;
    }

    m_Codec.EncodeOp(writer, FieldPathOp::FieldPathEncodeFinish);
}

std::vector<FieldPath> FieldPathStreamEngine::ReadFieldPaths(BitReader& reader) const {
    std::vector<FieldPath> result;
    FieldPath currentPath({-1});

    while (!reader.HasOverflowed() && reader.GetRemainingBits() > 0) {
        FieldPathOp op = m_Codec.DecodeOp(reader);
        if (op == FieldPathOp::FieldPathEncodeFinish) {
            break;
        }

        ExecuteOp(reader, currentPath, op);
        result.push_back(currentPath);
    }

    return result;
}

void FieldPathStreamEngine::EncodeTransition(BitWriter& writer, const FieldPath& curr, const FieldPath& next) const {
    const size_t currDepth = curr.path.size();
    const size_t nextDepth = next.path.size();

    if (currDepth == 1 && nextDepth == 1) {
        int32_t diff = next.path[0] - curr.path[0];
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
        int32_t leftDelta = next.path[0] - curr.path[0];
        int32_t rightVal = next.path[1];
        if (leftDelta == 0 && rightVal == 0) {
            m_Codec.EncodeOp(writer, FieldPathOp::PushOneLeftDeltaZeroRightZero);
        } else if (leftDelta == 1 && rightVal == 0) {
            m_Codec.EncodeOp(writer, FieldPathOp::PushOneLeftDeltaOneRightZero);
        } else if (rightVal == 0) {
            m_Codec.EncodeOp(writer, FieldPathOp::PushOneLeftDeltaNRightZero);
            writer.WriteVarUInt32(static_cast<uint32_t>(leftDelta));
        } else if (leftDelta >= 0 && leftDelta < 16 && rightVal >= 0 && rightVal < 16) {
            m_Codec.EncodeOp(writer, FieldPathOp::PushOneLeftDeltaNRightNonZeroPack8Bits);
            writer.WriteUBits(static_cast<uint32_t>(leftDelta) & 0x0Fu, 4);
            writer.WriteUBits(static_cast<uint32_t>(rightVal) & 0x0Fu, 4);
        } else {
            m_Codec.EncodeOp(writer, FieldPathOp::PushOneLeftDeltaNRightNonZero);
            writer.WriteVarUInt32(static_cast<uint32_t>(leftDelta));
            writer.WriteVarUInt32(static_cast<uint32_t>(rightVal));
        }
    } else if (currDepth == 2 && nextDepth == 2) {
        if (curr.path[0] == next.path[0]) {
            int32_t diff = next.path[1] - curr.path[1];
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
            int32_t parentDelta = next.path[0] - curr.path[0];
            int32_t childVal = next.path[1];
            if (parentDelta >= 0 && parentDelta < 16 && childVal >= 0 && childVal < 16) {
                m_Codec.EncodeOp(writer, FieldPathOp::NonTopoComplexPack4Bits);
                writer.WriteUBits(static_cast<uint32_t>(parentDelta) & 0x0Fu, 4);
                writer.WriteUBits(static_cast<uint32_t>(childVal) & 0x0Fu, 4);
            } else {
                m_Codec.EncodeOp(writer, FieldPathOp::PopAllButOnePlusN);
                writer.WriteVarUInt32(static_cast<uint32_t>(parentDelta));
                m_Codec.EncodeOp(writer, FieldPathOp::PushOneLeftDeltaZeroRightNonZero);
                writer.WriteVarUInt32(static_cast<uint32_t>(childVal));
            }
        }
    } else if (currDepth == 2 && nextDepth == 1) {
        int32_t diff = next.path[0] - curr.path[0];
        if (diff == 1) {
            m_Codec.EncodeOp(writer, FieldPathOp::PopOnePlusOne);
        } else {
            m_Codec.EncodeOp(writer, FieldPathOp::PopOnePlusN);
            writer.WriteVarUInt32(static_cast<uint32_t>(diff));
        }
    } else {
        size_t commonPrefix = 0;
        while (commonPrefix < currDepth && commonPrefix < nextDepth &&
               curr.path[commonPrefix] == next.path[commonPrefix]) {
            ++commonPrefix;
        }
        size_t pops = currDepth - commonPrefix;
        if (pops == 1) {
            if (commonPrefix < nextDepth) {
                int32_t diff = next.path[commonPrefix] - curr.path[commonPrefix];
                if (diff == 1) {
                    m_Codec.EncodeOp(writer, FieldPathOp::PopOnePlusOne);
                } else {
                    m_Codec.EncodeOp(writer, FieldPathOp::PopOnePlusN);
                    writer.WriteVarUInt32(static_cast<uint32_t>(diff));
                }
                ++commonPrefix;
            } else {
                m_Codec.EncodeOp(writer, FieldPathOp::PopOnePlusOne);
            }
        } else if (pops > 1 && commonPrefix == 1) {
            int32_t diff = (nextDepth > 0) ? (next.path[0] - curr.path[0]) : 0;
            if (diff == 1) {
                m_Codec.EncodeOp(writer, FieldPathOp::PopAllButOnePlusOne);
            } else {
                m_Codec.EncodeOp(writer, FieldPathOp::PopAllButOnePlusN);
                writer.WriteVarUInt32(static_cast<uint32_t>(diff));
            }
            commonPrefix = 1;
        } else if (pops > 0) {
            m_Codec.EncodeOp(writer, FieldPathOp::PopNPlusOne);
            writer.WriteUBits(static_cast<uint32_t>(pops) & 0x07u, 3);
        }

        while (commonPrefix < nextDepth) {
            int32_t val = next.path[commonPrefix];
            m_Codec.EncodeOp(writer, FieldPathOp::PushOneLeftDeltaZeroRightNonZero);
            writer.WriteVarUInt32(static_cast<uint32_t>(val));
            ++commonPrefix;
        }
    }
}

void FieldPathStreamEngine::ExecuteOp(BitReader& reader, FieldPath& path, FieldPathOp op) const {
    switch (op) {
    case FieldPathOp::PlusOne:
        path.path.back() += 1;
        break;
    case FieldPathOp::PlusTwo:
        path.path.back() += 2;
        break;
    case FieldPathOp::PlusThree:
        path.path.back() += 3;
        break;
    case FieldPathOp::PlusFour:
        path.path.back() += 4;
        break;
    case FieldPathOp::PlusN: {
        uint32_t delta = reader.ReadVarUInt32();
        path.path.back() += static_cast<int32_t>(delta);
        break;
    }
    case FieldPathOp::PushOneLeftDeltaZeroRightZero:
        path.path.push_back(0);
        break;
    case FieldPathOp::PushOneLeftDeltaZeroRightNonZero: {
        uint32_t right = reader.ReadVarUInt32();
        path.path.push_back(static_cast<int32_t>(right));
        break;
    }
    case FieldPathOp::PushOneLeftDeltaOneRightZero:
        path.path.back() += 1;
        path.path.push_back(0);
        break;
    case FieldPathOp::PushOneLeftDeltaOneRightNonZero: {
        uint32_t right = reader.ReadVarUInt32();
        path.path.back() += 1;
        path.path.push_back(static_cast<int32_t>(right));
        break;
    }
    case FieldPathOp::PushOneLeftDeltaNRightZero: {
        uint32_t left = reader.ReadVarUInt32();
        path.path.back() += static_cast<int32_t>(left);
        path.path.push_back(0);
        break;
    }
    case FieldPathOp::PushOneLeftDeltaNRightNonZero: {
        uint32_t left = reader.ReadVarUInt32();
        uint32_t right = reader.ReadVarUInt32();
        path.path.back() += static_cast<int32_t>(left);
        path.path.push_back(static_cast<int32_t>(right));
        break;
    }
    case FieldPathOp::PushOneLeftDeltaNRightNonZeroPack6Bits: {
        uint32_t left = reader.ReadUBits(3);
        uint32_t right = reader.ReadUBits(3);
        path.path.back() += static_cast<int32_t>(left);
        path.path.push_back(static_cast<int32_t>(right));
        break;
    }
    case FieldPathOp::PushOneLeftDeltaNRightNonZeroPack8Bits: {
        uint32_t left = reader.ReadUBits(4);
        uint32_t right = reader.ReadUBits(4);
        path.path.back() += static_cast<int32_t>(left);
        path.path.push_back(static_cast<int32_t>(right));
        break;
    }
    case FieldPathOp::PushTwoLeftDeltaZero: {
        uint32_t v1 = reader.ReadVarUInt32();
        uint32_t v2 = reader.ReadVarUInt32();
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        break;
    }
    case FieldPathOp::PushTwoPack5LeftDeltaZero: {
        uint32_t v1 = reader.ReadUBits(5);
        uint32_t v2 = reader.ReadUBits(5);
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        break;
    }
    case FieldPathOp::PushThreeLeftDeltaZero: {
        uint32_t v1 = reader.ReadVarUInt32();
        uint32_t v2 = reader.ReadVarUInt32();
        uint32_t v3 = reader.ReadVarUInt32();
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        path.path.push_back(static_cast<int32_t>(v3));
        break;
    }
    case FieldPathOp::PushThreePack5LeftDeltaZero: {
        uint32_t v1 = reader.ReadUBits(5);
        uint32_t v2 = reader.ReadUBits(5);
        uint32_t v3 = reader.ReadUBits(5);
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        path.path.push_back(static_cast<int32_t>(v3));
        break;
    }
    case FieldPathOp::PushTwoLeftDeltaOne: {
        uint32_t v1 = reader.ReadVarUInt32();
        uint32_t v2 = reader.ReadVarUInt32();
        path.path.back() += 1;
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        break;
    }
    case FieldPathOp::PushTwoPack5LeftDeltaOne: {
        uint32_t v1 = reader.ReadUBits(5);
        uint32_t v2 = reader.ReadUBits(5);
        path.path.back() += 1;
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        break;
    }
    case FieldPathOp::PushThreeLeftDeltaOne: {
        uint32_t v1 = reader.ReadVarUInt32();
        uint32_t v2 = reader.ReadVarUInt32();
        uint32_t v3 = reader.ReadVarUInt32();
        path.path.back() += 1;
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        path.path.push_back(static_cast<int32_t>(v3));
        break;
    }
    case FieldPathOp::PushThreePack5LeftDeltaOne: {
        uint32_t v1 = reader.ReadUBits(5);
        uint32_t v2 = reader.ReadUBits(5);
        uint32_t v3 = reader.ReadUBits(5);
        path.path.back() += 1;
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        path.path.push_back(static_cast<int32_t>(v3));
        break;
    }
    case FieldPathOp::PushTwoLeftDeltaN: {
        uint32_t delta = reader.ReadVarUInt32();
        uint32_t v1 = reader.ReadVarUInt32();
        uint32_t v2 = reader.ReadVarUInt32();
        path.path.back() += static_cast<int32_t>(delta);
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        break;
    }
    case FieldPathOp::PushTwoPack5LeftDeltaN: {
        uint32_t delta = reader.ReadUBits(5);
        uint32_t v1 = reader.ReadUBits(5);
        uint32_t v2 = reader.ReadUBits(5);
        path.path.back() += static_cast<int32_t>(delta);
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        break;
    }
    case FieldPathOp::PushThreeLeftDeltaN: {
        uint32_t delta = reader.ReadVarUInt32();
        uint32_t v1 = reader.ReadVarUInt32();
        uint32_t v2 = reader.ReadVarUInt32();
        uint32_t v3 = reader.ReadVarUInt32();
        path.path.back() += static_cast<int32_t>(delta);
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        path.path.push_back(static_cast<int32_t>(v3));
        break;
    }
    case FieldPathOp::PushThreePack5LeftDeltaN: {
        uint32_t delta = reader.ReadUBits(5);
        uint32_t v1 = reader.ReadUBits(5);
        uint32_t v2 = reader.ReadUBits(5);
        uint32_t v3 = reader.ReadUBits(5);
        path.path.back() += static_cast<int32_t>(delta);
        path.path.push_back(static_cast<int32_t>(v1));
        path.path.push_back(static_cast<int32_t>(v2));
        path.path.push_back(static_cast<int32_t>(v3));
        break;
    }
    case FieldPathOp::PushN: {
        uint32_t count = reader.ReadVarUInt32();
        uint32_t delta = reader.ReadVarUInt32();
        path.path.back() += static_cast<int32_t>(delta);
        for (uint32_t i = 0; i < count; ++i) {
            path.path.push_back(static_cast<int32_t>(reader.ReadVarUInt32()));
        }
        break;
    }
    case FieldPathOp::PushNAndNonTopographical: {
        uint32_t count = reader.ReadUBits(3);
        for (uint32_t i = 0; i < count; ++i) {
            path.path.push_back(static_cast<int32_t>(reader.ReadVarUInt32()));
        }
        break;
    }
    case FieldPathOp::PopOnePlusOne:
        if (path.path.size() > 1) {
            path.path.pop_back();
        }
        path.path.back() += 1;
        break;
    case FieldPathOp::PopOnePlusN: {
        if (path.path.size() > 1) {
            path.path.pop_back();
        }
        uint32_t delta = reader.ReadVarUInt32();
        path.path.back() += static_cast<int32_t>(delta);
        break;
    }
    case FieldPathOp::PopAllButOnePlusOne:
        while (path.path.size() > 1) {
            path.path.pop_back();
        }
        path.path.back() += 1;
        break;
    case FieldPathOp::PopAllButOnePlusN: {
        while (path.path.size() > 1) {
            path.path.pop_back();
        }
        uint32_t delta = reader.ReadVarUInt32();
        path.path.back() += static_cast<int32_t>(delta);
        break;
    }
    case FieldPathOp::PopAllButOnePlusNPack3Bits: {
        while (path.path.size() > 1) {
            path.path.pop_back();
        }
        uint32_t delta = reader.ReadUBits(3);
        path.path.back() += static_cast<int32_t>(delta);
        break;
    }
    case FieldPathOp::PopAllButOnePlusNPack6Bits: {
        while (path.path.size() > 1) {
            path.path.pop_back();
        }
        uint32_t delta = reader.ReadUBits(6);
        path.path.back() += static_cast<int32_t>(delta);
        break;
    }
    case FieldPathOp::PopNPlusOne: {
        uint32_t count = reader.ReadUBits(3);
        while (count-- > 0 && path.path.size() > 1) {
            path.path.pop_back();
        }
        path.path.back() += 1;
        break;
    }
    case FieldPathOp::PopNPlusN: {
        uint32_t count = reader.ReadUBits(3);
        while (count-- > 0 && path.path.size() > 1) {
            path.path.pop_back();
        }
        uint32_t delta = reader.ReadVarUInt32();
        path.path.back() += static_cast<int32_t>(delta);
        break;
    }
    case FieldPathOp::PopNAndNonTopographical: {
        uint32_t count = reader.ReadUBits(3);
        while (count-- > 0 && path.path.size() > 1) {
            path.path.pop_back();
        }
        for (size_t i = 0; i < path.path.size(); ++i) {
            if (reader.ReadBit()) {
                path.path[i] += static_cast<int32_t>(reader.ReadVarUInt32());
            }
        }
        break;
    }
    case FieldPathOp::NonTopoComplex: {
        for (size_t i = 0; i < path.path.size(); ++i) {
            if (reader.ReadBit()) {
                path.path[i] += static_cast<int32_t>(reader.ReadVarUInt32());
            }
        }
        break;
    }
    case FieldPathOp::NonTopoPenultimatePluseOne:
        if (path.path.size() >= 2) {
            path.path[path.path.size() - 2] += 1;
        }
        break;
    case FieldPathOp::NonTopoComplexPack4Bits: {
        uint32_t parentDelta = reader.ReadUBits(4);
        uint32_t childVal = reader.ReadUBits(4);
        if (path.path.size() >= 2) {
            path.path[0] += static_cast<int32_t>(parentDelta);
            path.path[1] = static_cast<int32_t>(childVal);
        }
        break;
    }
    case FieldPathOp::FieldPathEncodeFinish:
        break;
    default:
        path.path.back() += 1;
        break;
    }
}

// ============================================================================
// 4. CEntityBaselineStorage Implementation
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
// 5. CFieldChangeDispatcher Implementation
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
// 6. CEntitySnapshot Implementation
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
// 7. CEntityDeltaCodec Implementation
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
        header.updateType = EntityUpdateType::EnterPVS;
        header.classId = stream.ReadVarInt32();
        header.serialNumber = stream.ReadUBits(17);
        return header;
    }

    const uint32_t pvsFlag = stream.ReadUBits(2);
    switch (pvsFlag) {
    case 0:
        header.updateType = EntityUpdateType::Preserve;
        break;
    case 1:
        header.updateType = EntityUpdateType::LeavePVS;
        if (stream.ReadUBits(1) != 0) {
            header.updateType = EntityUpdateType::Delete;
        }
        break;
    case 2:
        header.updateType = EntityUpdateType::EnterPVS;
        header.classId = stream.ReadVarInt32();
        header.serialNumber = stream.ReadUBits(17);
        break;
    case 3:
    default:
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
            const uint32_t rawBits = stream.ReadUBits(static_cast<size_t>(fieldInfo.bitCount));
            const float val = DequantizeFloat(rawBits, fieldInfo.lowValue, fieldInfo.highValue, static_cast<size_t>(fieldInfo.bitCount));
            return PropertyValue(val);
        }
        const uint32_t raw = stream.ReadUBits(32);
        float fVal = 0.0f;
        std::memcpy(&fVal, &raw, sizeof(fVal));
        return PropertyValue(fVal);
    }
    if (type == "Vector" || type == "Vector3") {
        Vector3_t v{};
        if (fieldInfo.bitCount > 0) {
            const auto bits = static_cast<size_t>(fieldInfo.bitCount);
            v.x = DequantizeFloat(stream.ReadUBits(bits), fieldInfo.lowValue, fieldInfo.highValue, bits);
            v.y = DequantizeFloat(stream.ReadUBits(bits), fieldInfo.lowValue, fieldInfo.highValue, bits);
            v.z = DequantizeFloat(stream.ReadUBits(bits), fieldInfo.lowValue, fieldInfo.highValue, bits);
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
            const auto bits = static_cast<size_t>(fieldInfo.bitCount);
            a.pitch = DequantizeFloat(stream.ReadUBits(bits), fieldInfo.lowValue, fieldInfo.highValue, bits);
            a.yaw   = DequantizeFloat(stream.ReadUBits(bits), fieldInfo.lowValue, fieldInfo.highValue, bits);
            a.roll  = DequantizeFloat(stream.ReadUBits(bits), fieldInfo.lowValue, fieldInfo.highValue, bits);
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
            const auto bits = static_cast<size_t>(fieldInfo.bitCount);
            const uint32_t raw = QuantizeFloat(val.fVal, fieldInfo.lowValue, fieldInfo.highValue, bits);
            stream.WriteUBits(raw, bits);
            return;
        }
        uint32_t raw = 0;
        std::memcpy(&raw, &val.fVal, sizeof(raw));
        stream.WriteUBits(raw, 32);
        return;
    }
    if (type == "Vector" || type == "Vector3") {
        if (fieldInfo.bitCount > 0) {
            const auto bits = static_cast<size_t>(fieldInfo.bitCount);
            stream.WriteUBits(QuantizeFloat(val.vecVal.x, fieldInfo.lowValue, fieldInfo.highValue, bits), bits);
            stream.WriteUBits(QuantizeFloat(val.vecVal.y, fieldInfo.lowValue, fieldInfo.highValue, bits), bits);
            stream.WriteUBits(QuantizeFloat(val.vecVal.z, fieldInfo.lowValue, fieldInfo.highValue, bits), bits);
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
            const auto bits = static_cast<size_t>(fieldInfo.bitCount);
            stream.WriteUBits(QuantizeFloat(val.angVal.pitch, fieldInfo.lowValue, fieldInfo.highValue, bits), bits);
            stream.WriteUBits(QuantizeFloat(val.angVal.yaw,   fieldInfo.lowValue, fieldInfo.highValue, bits), bits);
            stream.WriteUBits(QuantizeFloat(val.angVal.roll,  fieldInfo.lowValue, fieldInfo.highValue, bits), bits);
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

    CEntitySnapshot* pEntity = nullptr;

    if (header.updateType == EntityUpdateType::EnterPVS) {
        auto& ent = m_ActiveEntities[header.entityIndex];
        ent.m_nEntityIndex = header.entityIndex;
        ent.m_nClassId = header.classId;
        ent.m_nSerialNumber = header.serialNumber;
        ent.m_bActive = true;
        ent.m_bDormant = false;

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
            return false;
        }
        pEntity = &it->second;
        pEntity->m_bDormant = false;
    }

    const auto* pSerializer = GetSerializer(pEntity->m_nClassId);
    if (!pSerializer) {
        return false;
    }

    // Decode changed field paths from bitstream using 40-op Huffman VM
    const std::vector<FieldPath> fieldPaths = CFlattenedSerializer::DecodeFieldPaths(stream);
    const auto& serializerFields = pSerializer->GetFields();

    for (const auto& fp : fieldPaths) {
        if (fp.path.empty()) continue;

        const auto fieldIdx = static_cast<size_t>(fp.path[0]);
        if (fieldIdx >= serializerFields.size()) {
            continue;
        }

        const auto& fieldInfo = serializerFields[fieldIdx];
        const PropertyValue newVal = DecodeFieldValue(stream, fieldInfo);

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

    FieldPathHuffmanCodec huffCodec;
    FieldPathStreamEngine fpEngine(huffCodec);

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

        // Collect changed field paths
        std::vector<size_t> changedIndices;
        std::vector<FieldPath> pathsToEmit;

        for (size_t i = 0; i < fields.size(); ++i) {
            if (!ent.m_DirtyBitmask.IsSet(i) && isDelta) continue;

            const auto& fInfo = fields[i];
            if (!ent.m_Properties.contains(fInfo.varName)) continue;

            changedIndices.push_back(i);
            pathsToEmit.push_back(FieldPath{static_cast<int32_t>(i)});
        }

        // Phase 1: Emit all Huffman FieldPaths using the 40-operation VM
        BitWriter bw;
        fpEngine.WriteFieldPaths(bw, pathsToEmit);

        for (size_t b = 0; b < bw.GetBitPosition(); ++b) {
            bool bit = (bw.GetData()[b / 8] >> (b % 8)) & 1;
            outStream.WriteUBits(bit ? 1 : 0, 1);
        }

        // Phase 2: Encode property values in order of changed field paths
        for (size_t idx : changedIndices) {
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
