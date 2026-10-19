#include "flattened_serializers.h"
#include <sstream>

namespace cs2::networksystem {

std::string FieldPath::ToString() const {
    if (path.empty()) return "[]";
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < path.size(); ++i) {
        ss << path[i];
        if (i + 1 < path.size()) ss << "/";
    }
    ss << "]";
    return ss.str();
}

CFlattenedSerializer::CFlattenedSerializer(std::string typeName, uint32_t version)
    : m_TypeName(std::move(typeName)), m_Version(version) {}

void CFlattenedSerializer::AddField(SerializedFieldInfo field) {
    m_Fields.push_back(std::move(field));
}

std::vector<FieldPath> CFlattenedSerializer::DecodeFieldPaths(CBitReadStream& stream) {
    std::vector<FieldPath> result;
    std::vector<int32_t> currentPath = {-1};

    while (!stream.HasOverflowed()) {
        // Read 4-bit operation code corresponding to the Huffman walk table entry
        const uint32_t opCode = stream.ReadUBits(4);
        if (stream.HasOverflowed()) break;

        const auto op = static_cast<FieldPathOp>(opCode);
        if (op == FieldPathOp::Finish) {
            break;
        }

        switch (op) {
        case FieldPathOp::PlusOne:
            currentPath.back() += 1;
            break;
        case FieldPathOp::PlusTwo:
            currentPath.back() += 2;
            break;
        case FieldPathOp::PlusThree:
            currentPath.back() += 3;
            break;
        case FieldPathOp::PlusFour:
            currentPath.back() += 4;
            break;
        case FieldPathOp::PlusN: {
            uint32_t delta = stream.ReadUBits(6);
            currentPath.back() += static_cast<int32_t>(delta);
            break;
        }
        case FieldPathOp::PushOneLeftDeltaZero:
            currentPath.push_back(0);
            break;
        case FieldPathOp::PushOneLeftDeltaOne:
            currentPath.back() += 1;
            currentPath.push_back(0);
            break;
        case FieldPathOp::PushOneLeftDeltaN: {
            uint32_t delta = stream.ReadUBits(4);
            currentPath.back() += static_cast<int32_t>(delta);
            currentPath.push_back(0);
            break;
        }
        case FieldPathOp::PopOnePlusOne:
            if (currentPath.size() > 1) {
                currentPath.pop_back();
            }
            currentPath.back() += 1;
            break;
        case FieldPathOp::PopNPlusOne: {
            uint32_t popCount = stream.ReadUBits(3);
            while (popCount-- > 0 && currentPath.size() > 1) {
                currentPath.pop_back();
            }
            currentPath.back() += 1;
            break;
        }
        case FieldPathOp::PopNPlusN: {
            uint32_t popCount = stream.ReadUBits(3);
            while (popCount-- > 0 && currentPath.size() > 1) {
                currentPath.pop_back();
            }
            uint32_t delta = stream.ReadUBits(4);
            currentPath.back() += static_cast<int32_t>(delta);
            break;
        }
        default:
            currentPath.back() += 1;
            break;
        }

        result.push_back(FieldPath{currentPath});
    }

    return result;
}

} // namespace cs2::networksystem
