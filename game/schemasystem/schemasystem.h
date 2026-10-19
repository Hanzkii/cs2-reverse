#ifndef CS2_SCHEMASYSTEM_H
#define CS2_SCHEMASYSTEM_H

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace cs2::schemasystem {

struct SchemaField {
    std::string name;
    std::string type;
    int32_t offset;
};

struct SchemaClass {
    std::string name;
    std::string moduleName;
    int32_t size;
    std::unordered_map<std::string, SchemaField> fields;
};

/**
 * @brief Reconstructed CSchemaSystem from schemasystem.dll
 * Verified MurmurHash2 implementation matching schemasystem.dll @ 0x180010380.
 */
class CSchemaSystem {
public:
    static CSchemaSystem& Get();

    static constexpr uint32_t MurmurHash2(std::string_view str) noexcept {
        const uint32_t len = static_cast<uint32_t>(str.length());
        if (len == 0) return 0;

        constexpr uint32_t m = 0x5bd1e995;
        constexpr int r = 24;

        uint32_t h = len ^ 0xbaadfeed;
        const char* data = str.data();
        uint32_t remaining = len;

        while (remaining >= 4) {
            uint32_t k = static_cast<uint8_t>(data[0]) |
                        (static_cast<uint8_t>(data[1]) << 8) |
                        (static_cast<uint8_t>(data[2]) << 16) |
                        (static_cast<uint8_t>(data[3]) << 24);

            k *= m;
            k ^= k >> r;
            k *= m;

            h *= m;
            h ^= k;

            data += 4;
            remaining -= 4;
        }

        if (remaining == 3) h ^= static_cast<uint8_t>(data[2]) << 16;
        if (remaining >= 2) h ^= static_cast<uint8_t>(data[1]) << 8;
        if (remaining >= 1) {
            h ^= static_cast<uint8_t>(data[0]);
            h *= m;
        }

        h ^= h >> 13;
        h *= m;
        h ^= h >> 15;

        return h;
    }

    void RegisterClass(const std::string& moduleName, const std::string& className, int32_t classSize);
    void RegisterField(const std::string& className, const std::string& fieldName, const std::string& type, int32_t offset);

    [[nodiscard]] const SchemaClass* FindDeclaredClass(const std::string& className) const;
    [[nodiscard]] int32_t GetFieldOffset(const std::string& className, const std::string& fieldName) const;

private:
    CSchemaSystem();
    std::unordered_map<std::string, SchemaClass> m_Classes;
};

} // namespace cs2::schemasystem

#endif // CS2_SCHEMASYSTEM_H
