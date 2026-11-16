#ifndef CS2_FLATTENED_SERIALIZERS_H
#define CS2_FLATTENED_SERIALIZERS_H

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <initializer_list>
#include "netmessage_framing.h"

namespace cs2::networksystem {

/**
 * @brief Complete 40-operation FieldPath instruction set in Source 2 entity state serialization.
 * Reconstructed from networksystem.dll (40-operation Huffman decoder @ 0x180272420 / 0x18026D420).
 */
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

const char* GetFieldPathOpName(FieldPathOp op);

struct FieldPath {
    std::vector<int32_t> path;

    FieldPath() = default;
    FieldPath(std::vector<int32_t> p) : path(std::move(p)) {}
    FieldPath(std::initializer_list<int32_t> init) : path(init) {}

    [[nodiscard]] std::string ToString() const;

    bool operator==(const FieldPath& o) const noexcept { return path == o.path; }
    bool operator<(const FieldPath& o) const noexcept { return path < o.path; }
};

struct SerializedFieldInfo {
    uint32_t fieldIndex;
    std::string varName;
    std::string varType;
    int32_t bitCount;
    float lowValue;
    float highValue;
};

/**
 * @brief Reconstructed Flattened Serializer walker from networksystem.dll.
 */
class CFlattenedSerializer {
public:
    CFlattenedSerializer(std::string typeName, uint32_t version);

    void AddField(SerializedFieldInfo field);
    [[nodiscard]] const std::vector<SerializedFieldInfo>& GetFields() const noexcept { return m_Fields; }
    [[nodiscard]] const std::string& GetTypeName() const noexcept { return m_TypeName; }
    [[nodiscard]] uint32_t GetVersion() const noexcept { return m_Version; }

    /**
     * @brief Decodes field paths from the wire bitstream using 40-op Huffman VM.
     */
    static std::vector<FieldPath> DecodeFieldPaths(CBitReadStream& stream);

private:
    std::string m_TypeName;
    uint32_t m_Version;
    std::vector<SerializedFieldInfo> m_Fields;
};

} // namespace cs2::networksystem

#endif // CS2_FLATTENED_SERIALIZERS_H
