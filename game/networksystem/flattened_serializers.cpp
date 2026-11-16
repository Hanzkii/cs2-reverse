#include "flattened_serializers.h"
#include "entity_delta_codec.h"
#include <sstream>

namespace cs2::networksystem {

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
    FieldPathHuffmanCodec codec;
    FieldPathStreamEngine engine(codec);
    BitReader reader(stream.GetData(), stream.GetTotalBits());
    reader.Seek(stream.GetBitsRead());

    std::vector<FieldPath> result = engine.ReadFieldPaths(reader);
    stream.SetBitsRead(reader.GetBitPosition());
    return result;
}

} // namespace cs2::networksystem
