#ifndef CS2_FLATTENED_SERIALIZERS_H
#define CS2_FLATTENED_SERIALIZERS_H

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include "netmessage_framing.h"

namespace cs2::networksystem {

/**
 * @brief Field path operations in Source 2 entity state serialization.
 * Reconstructed from networksystem.dll (40-operation Huffman decoder @ 0x180272420).
 */
enum class FieldPathOp : uint8_t {
    PlusOne = 0,
    PlusTwo = 1,
    PlusThree = 2,
    PlusFour = 3,
    PlusN = 4,
    PushOneLeftDeltaZero = 5,
    PushOneLeftDeltaOne = 6,
    PushOneLeftDeltaN = 7,
    PushThreePackets = 8,
    PopAllButOneWalk = 9,
    PopOnePlusOne = 10,
    PopNPlusOne = 11,
    PopNPlusN = 12,
    Finish = 13
};

struct FieldPath {
    std::vector<int32_t> path;

    [[nodiscard]] std::string ToString() const;
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
     * @brief Decodes field paths from the wire bitstream.
     */
    static std::vector<FieldPath> DecodeFieldPaths(CBitReadStream& stream);

private:
    std::string m_TypeName;
    uint32_t m_Version;
    std::vector<SerializedFieldInfo> m_Fields;
};

} // namespace cs2::networksystem

#endif // CS2_FLATTENED_SERIALIZERS_H
