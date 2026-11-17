#ifndef CS2_RESOURCESYSTEM_KV3_PARSER_H
#define CS2_RESOURCESYSTEM_KV3_PARSER_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <span>
#include <optional>
#include <memory>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace cs2::resourcesystem {

// Forward declarations
class KV3Value;
using KV3Array = std::vector<KV3Value>;
using KV3Table = std::vector<std::pair<std::string, KV3Value>>;
using KV3Blob  = std::vector<std::uint8_t>;

/**
 * @brief 128-bit Universally Unique Identifier for KV3 format and encoding specifications.
 */
struct KV3Guid_t {
    std::array<std::uint8_t, 16> m_bytes{};

    constexpr KV3Guid_t() noexcept = default;
    constexpr explicit KV3Guid_t(const std::array<std::uint8_t, 16>& bytes) noexcept : m_bytes(bytes) {}

    static KV3Guid_t Parse(std::string_view str);
    [[nodiscard]] std::string ToString() const;

    constexpr bool operator==(const KV3Guid_t& other) const noexcept {
        return m_bytes == other.m_bytes;
    }
    constexpr bool operator!=(const KV3Guid_t& other) const noexcept {
        return m_bytes != other.m_bytes;
    }
};

// Canonical Valve KV3 GUID definitions
inline constexpr std::array<std::uint8_t, 16> g_GuidTextEncoding = {
    0xe2, 0x1c, 0x7f, 0x3c, 0x8a, 0x33, 0x41, 0xc5, 0x99, 0x77, 0xa7, 0x6d, 0x3a, 0x32, 0xaa, 0x0d
};
inline constexpr std::array<std::uint8_t, 16> g_GuidGenericFormat = {
    0x74, 0x47, 0x72, 0x21, 0x4c, 0x60, 0x44, 0xd8, 0xa1, 0x4a, 0x03, 0x83, 0x8b, 0x55, 0x8f, 0x68
};
inline constexpr std::array<std::uint8_t, 16> g_GuidBinaryKV3BlockLZ4 = {
    0x46, 0x01, 0x27, 0xb0, 0x8c, 0x20, 0x48, 0x23, 0x91, 0x56, 0x9e, 0x16, 0x6f, 0x9e, 0x66, 0xcb
};

/**
 * @brief Compression schemes applied to binary KeyValues3 streams.
 */
enum class KV3CompressionType : std::uint32_t {
    None = 0,
    LZ4  = 1,
    ZSTD = 2,
    Unknown = 0xFFFFFFFF
};

/**
 * @brief Runtime type identifier for dynamic KV3 values.
 */
enum class KV3Type : std::uint8_t {
    Null = 0,
    Boolean = 1,
    Integer = 2,
    Double = 3,
    String = 4,
    BinaryBlob = 5,
    Array = 6,
    Table = 7
};

/**
 * @brief Dynamic recursive value tree container representing an arbitrary KeyValues3 node.
 */
class KV3Value {
public:
    using Storage = std::variant<
        std::monostate, // Null
        bool,           // Boolean
        std::int64_t,   // Integer
        double,         // Double
        std::string,    // String
        KV3Blob,        // Binary Blob
        KV3Array,       // Array
        KV3Table        // Table / Object
    >;

private:
    Storage m_storage{std::monostate{}};

public:
    KV3Value() noexcept = default;
    KV3Value(std::nullptr_t) noexcept : m_storage(std::monostate{}) {}
    KV3Value(bool val) noexcept : m_storage(val) {}
    KV3Value(std::int64_t val) noexcept : m_storage(val) {}
    KV3Value(std::int32_t val) noexcept : m_storage(static_cast<std::int64_t>(val)) {}
    KV3Value(std::uint32_t val) noexcept : m_storage(static_cast<std::int64_t>(val)) {}
    KV3Value(std::uint64_t val) noexcept : m_storage(static_cast<std::int64_t>(val)) {}
    KV3Value(double val) noexcept : m_storage(val) {}
    KV3Value(float val) noexcept : m_storage(static_cast<double>(val)) {}
    KV3Value(std::string str) : m_storage(std::move(str)) {}
    KV3Value(std::string_view sv) : m_storage(std::string(sv)) {}
    KV3Value(const char* sz) : m_storage(sz ? std::string(sz) : std::string{}) {}
    KV3Value(KV3Blob blob) : m_storage(std::move(blob)) {}
    KV3Value(KV3Array arr) : m_storage(std::move(arr)) {}
    KV3Value(KV3Table tbl) : m_storage(std::move(tbl)) {}

    [[nodiscard]] KV3Type GetType() const noexcept {
        return static_cast<KV3Type>(m_storage.index());
    }

    [[nodiscard]] bool IsNull() const noexcept { return std::holds_alternative<std::monostate>(m_storage); }
    [[nodiscard]] bool IsBool() const noexcept { return std::holds_alternative<bool>(m_storage); }
    [[nodiscard]] bool IsInt() const noexcept { return std::holds_alternative<std::int64_t>(m_storage); }
    [[nodiscard]] bool IsDouble() const noexcept { return std::holds_alternative<double>(m_storage); }
    [[nodiscard]] bool IsString() const noexcept { return std::holds_alternative<std::string>(m_storage); }
    [[nodiscard]] bool IsBlob() const noexcept { return std::holds_alternative<KV3Blob>(m_storage); }
    [[nodiscard]] bool IsArray() const noexcept { return std::holds_alternative<KV3Array>(m_storage); }
    [[nodiscard]] bool IsTable() const noexcept { return std::holds_alternative<KV3Table>(m_storage); }

    [[nodiscard]] bool AsBool(bool defaultVal = false) const noexcept {
        if (const auto* p = std::get_if<bool>(&m_storage)) return *p;
        if (const auto* p = std::get_if<std::int64_t>(&m_storage)) return *p != 0;
        return defaultVal;
    }

    [[nodiscard]] std::int64_t AsInt(std::int64_t defaultVal = 0) const noexcept {
        if (const auto* p = std::get_if<std::int64_t>(&m_storage)) return *p;
        if (const auto* p = std::get_if<double>(&m_storage)) return static_cast<std::int64_t>(*p);
        if (const auto* p = std::get_if<bool>(&m_storage)) return *p ? 1 : 0;
        return defaultVal;
    }

    [[nodiscard]] double AsDouble(double defaultVal = 0.0) const noexcept {
        if (const auto* p = std::get_if<double>(&m_storage)) return *p;
        if (const auto* p = std::get_if<std::int64_t>(&m_storage)) return static_cast<double>(*p);
        if (const auto* p = std::get_if<bool>(&m_storage)) return *p ? 1.0 : 0.0;
        return defaultVal;
    }

    [[nodiscard]] const std::string& AsString(const std::string& defaultVal = "") const noexcept {
        if (const auto* p = std::get_if<std::string>(&m_storage)) return *p;
        return defaultVal;
    }

    [[nodiscard]] const KV3Blob& AsBlob() const {
        if (const auto* p = std::get_if<KV3Blob>(&m_storage)) return *p;
        static const KV3Blob s_emptyBlob;
        return s_emptyBlob;
    }

    [[nodiscard]] KV3Blob& AsBlob() {
        if (!IsBlob()) m_storage = KV3Blob{};
        return std::get<KV3Blob>(m_storage);
    }

    [[nodiscard]] const KV3Array& AsArray() const {
        if (const auto* p = std::get_if<KV3Array>(&m_storage)) return *p;
        static const KV3Array s_emptyArr;
        return s_emptyArr;
    }

    [[nodiscard]] KV3Array& AsArray() {
        if (!IsArray()) m_storage = KV3Array{};
        return std::get<KV3Array>(m_storage);
    }

    [[nodiscard]] const KV3Table& AsTable() const {
        if (const auto* p = std::get_if<KV3Table>(&m_storage)) return *p;
        static const KV3Table s_emptyTbl;
        return s_emptyTbl;
    }

    [[nodiscard]] KV3Table& AsTable() {
        if (!IsTable()) m_storage = KV3Table{};
        return std::get<KV3Table>(m_storage);
    }

    [[nodiscard]] std::size_t Size() const noexcept {
        if (const auto* p = std::get_if<KV3Array>(&m_storage)) return p->size();
        if (const auto* p = std::get_if<KV3Table>(&m_storage)) return p->size();
        if (const auto* p = std::get_if<KV3Blob>(&m_storage)) return p->size();
        if (const auto* p = std::get_if<std::string>(&m_storage)) return p->size();
        return 0;
    }

    // Table operations
    [[nodiscard]] bool HasKey(std::string_view key) const noexcept {
        if (const auto* p = std::get_if<KV3Table>(&m_storage)) {
            return std::any_of(p->begin(), p->end(), [key](const auto& pair) {
                return pair.first == key;
            });
        }
        return false;
    }

    [[nodiscard]] const KV3Value* Find(std::string_view key) const noexcept {
        if (const auto* p = std::get_if<KV3Table>(&m_storage)) {
            for (const auto& pair : *p) {
                if (pair.first == key) return &pair.second;
            }
        }
        return nullptr;
    }

    [[nodiscard]] KV3Value* Find(std::string_view key) noexcept {
        if (auto* p = std::get_if<KV3Table>(&m_storage)) {
            for (auto& pair : *p) {
                if (pair.first == key) return &pair.second;
            }
        }
        return nullptr;
    }

    KV3Value& operator[](std::string_view key) {
        if (!IsTable()) m_storage = KV3Table{};
        auto& tbl = std::get<KV3Table>(m_storage);
        for (auto& pair : tbl) {
            if (pair.first == key) return pair.second;
        }
        tbl.emplace_back(std::string(key), KV3Value{});
        return tbl.back().second;
    }

    const KV3Value& operator[](std::string_view key) const {
        const auto* ptr = Find(key);
        if (ptr) return *ptr;
        static const KV3Value s_nullValue;
        return s_nullValue;
    }

    // Array operations
    void PushBack(KV3Value value) {
        if (!IsArray()) m_storage = KV3Array{};
        std::get<KV3Array>(m_storage).push_back(std::move(value));
    }

    KV3Value& operator[](std::size_t index) {
        if (!IsArray()) m_storage = KV3Array{};
        auto& arr = std::get<KV3Array>(m_storage);
        if (index >= arr.size()) arr.resize(index + 1);
        return arr[index];
    }

    const KV3Value& operator[](std::size_t index) const {
        if (const auto* p = std::get_if<KV3Array>(&m_storage)) {
            if (index < p->size()) return (*p)[index];
        }
        static const KV3Value s_nullValue;
        return s_nullValue;
    }

    bool operator==(const KV3Value& other) const;
    bool operator!=(const KV3Value& other) const { return !(*this == other); }
};

/**
 * @brief Metadata parsed from text or binary KV3 headers.
 */
struct KV3Header_t {
    std::string m_szEncoding{"text"};
    KV3Guid_t m_encodingGuid{g_GuidTextEncoding};
    std::string m_szFormat{"generic"};
    KV3Guid_t m_formatGuid{g_GuidGenericFormat};
    KV3CompressionType m_eCompression{KV3CompressionType::None};
};

/**
 * @brief Standalone pure ISO C++20 LZ4 block codec for Source 2 streams.
 */
class CLZ4Codec {
public:
    static bool Decompress(std::span<const std::uint8_t> src, std::span<std::uint8_t> dst);
    static std::vector<std::uint8_t> Compress(std::span<const std::uint8_t> src);
};

/**
 * @brief ZSTD block format specification and frame metadata inspector.
 */
class CZSTDCodec {
public:
    static constexpr std::uint32_t ZSTD_MAGIC = 0xFD2FB528;

    struct FrameInfo_t {
        bool m_bValid{false};
        bool m_bSingleSegment{false};
        std::uint64_t m_nWindowSize{0};
        std::uint64_t m_nContentSize{0};
        std::uint32_t m_nHeaderSize{0};
    };

    static FrameInfo_t InspectFrame(std::span<const std::uint8_t> src);
    static bool DecompressRawFrame(std::span<const std::uint8_t> src, std::span<std::uint8_t> dst);
};

/**
 * @brief In-memory KeyValues3 Document parser, generator, and tree manager.
 */
class CKV3Document {
public:
    KV3Header_t m_header;
    KV3Value m_root{KV3Table{}};

    CKV3Document() = default;

    // Text format operations
    bool LoadFromText(std::string_view text);
    [[nodiscard]] std::string SaveToText() const;

    // Binary format operations (Valve VKV3)
    bool LoadFromBinary(std::span<const std::uint8_t> bytes);
    [[nodiscard]] std::vector<std::uint8_t> SaveToBinary(KV3CompressionType compression = KV3CompressionType::None) const;

    [[nodiscard]] const KV3Value& Root() const noexcept { return m_root; }
    [[nodiscard]] KV3Value& Root() noexcept { return m_root; }
};

// ============================================================================
// Source 2 Compiled Resource Containers (resourcesystem.dll)
// ============================================================================

#pragma pack(push, 1)

/**
 * @brief Canonical Source 2 Compiled Resource Header ("VASU").
 * Binary signature: 0x55534156 ("VASU" in little-endian representation).
 */
struct ResourceHeader_t {
    std::uint32_t m_nMagic;         // 0x00: 0x55534156 ("VASU")
    std::uint16_t m_nHeaderVersion; // 0x04: 12 (0x000C)
    std::uint16_t m_nVersion;       // 0x06: Resource compilation version
    std::uint32_t m_nFlags;         // 0x08: Resource compiler flags
    std::uint32_t m_nBlockOffset;   // 0x0C: Offset to block descriptor table
    std::uint32_t m_nBlockCount;    // 0x10: Number of embedded resource blocks
};

/**
 * @brief Descriptor entry for individual resource data blocks.
 */
struct ResourceBlockEntry_t {
    std::uint32_t m_nBlockId;       // 0x00: FourCC ('DATA', 'RED2', 'RERL', etc.)
    std::uint32_t m_nOffset;        // 0x04: Absolute offset from start of file
    std::uint32_t m_nSize;          // 0x08: Uncompressed byte length
};

#pragma pack(pop)

/**
 * @brief FourCC identifier helper.
 */
constexpr std::uint32_t MakeFourCC(char a, char b, char c, char d) noexcept {
    return static_cast<std::uint32_t>(static_cast<std::uint8_t>(a)) |
          (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b)) << 8) |
          (static_cast<std::uint32_t>(static_cast<std::uint8_t>(c)) << 16) |
          (static_cast<std::uint32_t>(static_cast<std::uint8_t>(d)) << 24);
}

// Well-known block identifiers
inline constexpr std::uint32_t RESOURCE_MAGIC_VASU = 0x55534156;
inline constexpr std::uint32_t BLOCK_FOURCC_DATA   = MakeFourCC('D', 'A', 'T', 'A');
inline constexpr std::uint32_t BLOCK_FOURCC_RED2   = MakeFourCC('R', 'E', 'D', '2');
inline constexpr std::uint32_t BLOCK_FOURCC_RERL   = MakeFourCC('R', 'E', 'R', 'L');
inline constexpr std::uint32_t BLOCK_FOURCC_REDI   = MakeFourCC('R', 'E', 'D', 'I');
inline constexpr std::uint32_t BLOCK_FOURCC_VBIB   = MakeFourCC('V', 'B', 'I', 'B');

/**
 * @brief Compiled resource external reference entry.
 */
struct ResourceExternalRef_t {
    std::uint64_t m_nHash{0};
    std::string m_szAssetPath;
};

/**
 * @brief In-memory representation of a Source 2 compiled resource container.
 */
class CResourceFile {
public:
    ResourceHeader_t m_header{};
    std::vector<ResourceBlockEntry_t> m_blockEntries;
    std::vector<ResourceExternalRef_t> m_externalReferences;
    std::vector<std::uint8_t> m_rawBuffer;

    CResourceFile() = default;

    bool LoadFromMemory(std::span<const std::uint8_t> buffer);
    [[nodiscard]] std::span<const std::uint8_t> GetBlockData(std::uint32_t blockFourCC) const;
    [[nodiscard]] bool HasBlock(std::uint32_t blockFourCC) const noexcept;
    [[nodiscard]] bool UnpackDataBlockAsKV3(CKV3Document& outDoc) const;

    [[nodiscard]] static std::vector<std::uint8_t> BuildContainer(
        std::span<const std::uint8_t> dataBlockPayload,
        const std::vector<std::string>& externalRefs = {}
    );
};

} // namespace cs2::resourcesystem

#endif // CS2_RESOURCESYSTEM_KV3_PARSER_H
