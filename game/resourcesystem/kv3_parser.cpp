#include "kv3_parser.h"

#include <cstring>
#include <charconv>
#include <cctype>

namespace cs2::resourcesystem {

// ============================================================================
// KV3Guid_t Implementation
// ============================================================================

KV3Guid_t KV3Guid_t::Parse(std::string_view str) {
    KV3Guid_t guid;
    // Strip optional surrounding '{' and '}'
    if (!str.empty() && str.front() == '{') str.remove_prefix(1);
    if (!str.empty() && str.back() == '}') str.remove_suffix(1);

    std::size_t byteIdx = 0;
    std::size_t i = 0;
    while (i < str.size() && byteIdx < 16) {
        if (str[i] == '-') {
            ++i;
            continue;
        }
        if (i + 1 >= str.size()) break;

        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        int hi = hexVal(str[i]);
        int lo = hexVal(str[i + 1]);
        if (hi >= 0 && lo >= 0) {
            guid.m_bytes[byteIdx++] = static_cast<std::uint8_t>((hi << 4) | lo);
        }
        i += 2;
    }
    return guid;
}

std::string KV3Guid_t::ToString() const {
    std::ostringstream ss;
    ss << '{';
    for (std::size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) ss << '-';
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(m_bytes[i]);
    }
    ss << '}';
    return ss.str();
}

// ============================================================================
// KV3Value Equality Operator
// ============================================================================

bool KV3Value::operator==(const KV3Value& other) const {
    if (m_storage.index() != other.m_storage.index()) return false;

    return std::visit([&other](const auto& val) -> bool {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return true;
        } else if constexpr (std::is_same_v<T, KV3Array>) {
            const auto& otherArr = std::get<KV3Array>(other.m_storage);
            if (val.size() != otherArr.size()) return false;
            for (std::size_t i = 0; i < val.size(); ++i) {
                if (val[i] != otherArr[i]) return false;
            }
            return true;
        } else if constexpr (std::is_same_v<T, KV3Table>) {
            const auto& otherTbl = std::get<KV3Table>(other.m_storage);
            if (val.size() != otherTbl.size()) return false;
            for (std::size_t i = 0; i < val.size(); ++i) {
                if (val[i].first != otherTbl[i].first || val[i].second != otherTbl[i].second) return false;
            }
            return true;
        } else {
            return val == std::get<T>(other.m_storage);
        }
    }, m_storage);
}

// ============================================================================
// CLZ4Codec: Pure C++20 Standard LZ4 Block Codec
// ============================================================================

bool CLZ4Codec::Decompress(std::span<const std::uint8_t> src, std::span<std::uint8_t> dst) {
    std::size_t srcPos = 0;
    std::size_t dstPos = 0;

    while (srcPos < src.size()) {
        const std::uint8_t token = src[srcPos++];
        std::size_t literalLen = static_cast<std::size_t>(token >> 4);

        if (literalLen == 15) {
            while (srcPos < src.size()) {
                const std::uint8_t extra = src[srcPos++];
                literalLen += extra;
                if (extra != 255) break;
            }
        }

        if (srcPos + literalLen > src.size() || dstPos + literalLen > dst.size()) {
            return false;
        }

        for (std::size_t i = 0; i < literalLen; ++i) {
            dst[dstPos++] = src[srcPos++];
        }

        if (srcPos >= src.size()) {
            break; // Finished at end of stream
        }

        if (srcPos + 2 > src.size()) {
            return false;
        }

        const std::uint16_t offset = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(src[srcPos]) |
            (static_cast<std::uint16_t>(src[srcPos + 1]) << 8)
        );
        srcPos += 2;

        if (offset == 0 || offset > dstPos) {
            return false;
        }

        std::size_t matchLen = static_cast<std::size_t>((token & 0x0F) + 4);
        if ((token & 0x0F) == 15) {
            while (srcPos < src.size()) {
                const std::uint8_t extra = src[srcPos++];
                matchLen += extra;
                if (extra != 255) break;
            }
        }

        if (dstPos + matchLen > dst.size()) {
            return false;
        }

        for (std::size_t i = 0; i < matchLen; ++i) {
            dst[dstPos] = dst[dstPos - offset];
            dstPos++;
        }
    }

    return dstPos == dst.size();
}

std::vector<std::uint8_t> CLZ4Codec::Compress(std::span<const std::uint8_t> src) {
    std::vector<std::uint8_t> compressed;
    if (src.empty()) return compressed;

    // Hash table for 4-byte matching (4096 entries)
    constexpr std::size_t HASH_SIZE = 4096;
    std::array<std::int32_t, HASH_SIZE> hashTable;
    hashTable.fill(-1);

    auto hash4 = [](const std::uint8_t* p) -> std::size_t {
        std::uint32_t v = static_cast<std::uint32_t>(p[0]) |
                         (static_cast<std::uint32_t>(p[1]) << 8) |
                         (static_cast<std::uint32_t>(p[2]) << 16) |
                         (static_cast<std::uint32_t>(p[3]) << 24);
        return (v * 2654435761u) % HASH_SIZE;
    };

    std::size_t srcPos = 0;
    std::size_t anchor = 0;

    auto emitSequence = [&](std::size_t litLen, std::uint16_t offset, std::size_t matchLen) {
        std::uint8_t tokenLit = static_cast<std::uint8_t>(std::min<std::size_t>(litLen, 15));
        std::uint8_t tokenMatch = 0;
        if (offset > 0) {
            std::size_t mExtra = (matchLen >= 4) ? (matchLen - 4) : 0;
            tokenMatch = static_cast<std::uint8_t>(std::min<std::size_t>(mExtra, 15));
        }

        compressed.push_back(static_cast<std::uint8_t>((tokenLit << 4) | tokenMatch));

        if (litLen >= 15) {
            std::size_t rem = litLen - 15;
            while (rem >= 255) {
                compressed.push_back(255);
                rem -= 255;
            }
            compressed.push_back(static_cast<std::uint8_t>(rem));
        }

        for (std::size_t i = 0; i < litLen; ++i) {
            compressed.push_back(src[anchor + i]);
        }

        if (offset > 0) {
            compressed.push_back(static_cast<std::uint8_t>(offset & 0xFF));
            compressed.push_back(static_cast<std::uint8_t>((offset >> 8) & 0xFF));

            std::size_t mExtra = (matchLen >= 4) ? (matchLen - 4) : 0;
            if (mExtra >= 15) {
                std::size_t rem = mExtra - 15;
                while (rem >= 255) {
                    compressed.push_back(255);
                    rem -= 255;
                }
                compressed.push_back(static_cast<std::uint8_t>(rem));
            }
        }
    };

    while (srcPos + 4 <= src.size()) {
        const std::size_t h = hash4(&src[srcPos]);
        const std::int32_t matchPos = hashTable[h];
        hashTable[h] = static_cast<std::int32_t>(srcPos);

        if (matchPos >= 0 && (srcPos - static_cast<std::size_t>(matchPos)) <= 65535) {
            const std::size_t offset = srcPos - static_cast<std::size_t>(matchPos);
            // Check 4-byte match
            if (src[srcPos] == src[matchPos] &&
                src[srcPos + 1] == src[matchPos + 1] &&
                src[srcPos + 2] == src[matchPos + 2] &&
                src[srcPos + 3] == src[matchPos + 3]) {

                std::size_t matchLen = 4;
                while (srcPos + matchLen < src.size() &&
                       src[srcPos + matchLen] == src[matchPos + matchLen]) {
                    ++matchLen;
                }

                const std::size_t litLen = srcPos - anchor;
                emitSequence(litLen, static_cast<std::uint16_t>(offset), matchLen);

                srcPos += matchLen;
                anchor = srcPos;
                continue;
            }
        }

        ++srcPos;
    }

    // Emit remaining literals
    const std::size_t lastLitLen = src.size() - anchor;
    if (lastLitLen > 0 || compressed.empty()) {
        emitSequence(lastLitLen, 0, 0);
    }

    return compressed;
}

// ============================================================================
// CZSTDCodec: Frame Inspector & Raw Block Decompressor
// ============================================================================

CZSTDCodec::FrameInfo_t CZSTDCodec::InspectFrame(std::span<const std::uint8_t> src) {
    FrameInfo_t info{};
    if (src.size() < 4) return info;

    const std::uint32_t magic = static_cast<std::uint32_t>(src[0]) |
                               (static_cast<std::uint32_t>(src[1]) << 8) |
                               (static_cast<std::uint32_t>(src[2]) << 16) |
                               (static_cast<std::uint32_t>(src[3]) << 24);
    if (magic != ZSTD_MAGIC) return info;

    info.m_bValid = true;
    if (src.size() < 5) return info;

    const std::uint8_t fhd = src[4];
    info.m_bSingleSegment = ((fhd >> 5) & 1) != 0;
    const std::uint8_t fcsField = (fhd >> 6);

    std::size_t headerSize = 5;
    if (!info.m_bSingleSegment) {
        headerSize += 1; // Window descriptor
    }

    if (fcsField == 0) {
        if (info.m_bSingleSegment && src.size() >= headerSize + 1) {
            info.m_nContentSize = src[headerSize];
            headerSize += 1;
        } else {
            info.m_nContentSize = 0;
        }
    } else if (fcsField == 1) {
        if (src.size() >= headerSize + 2) {
            std::uint64_t val = static_cast<std::uint64_t>(src[headerSize]) |
                               (static_cast<std::uint64_t>(src[headerSize + 1]) << 8);
            info.m_nContentSize = val + 256;
            headerSize += 2;
        }
    } else if (fcsField == 2) {
        if (src.size() >= headerSize + 4) {
            info.m_nContentSize = static_cast<std::uint64_t>(src[headerSize]) |
                                 (static_cast<std::uint64_t>(src[headerSize + 1]) << 8) |
                                 (static_cast<std::uint64_t>(src[headerSize + 2]) << 16) |
                                 (static_cast<std::uint64_t>(src[headerSize + 3]) << 24);
            headerSize += 4;
        }
    } else if (fcsField == 3) {
        if (src.size() >= headerSize + 8) {
            std::uint64_t sz = 0;
            for (std::size_t i = 0; i < 8; ++i) {
                sz |= (static_cast<std::uint64_t>(src[headerSize + i]) << (i * 8));
            }
            info.m_nContentSize = sz;
            headerSize += 8;
        }
    }

    info.m_nHeaderSize = static_cast<std::uint32_t>(headerSize);
    return info;
}

bool CZSTDCodec::DecompressRawFrame(std::span<const std::uint8_t> src, std::span<std::uint8_t> dst) {
    const FrameInfo_t info = InspectFrame(src);
    if (!info.m_bValid || info.m_nHeaderSize >= src.size()) return false;

    std::size_t srcPos = info.m_nHeaderSize;
    std::size_t dstPos = 0;

    while (srcPos + 3 <= src.size() && dstPos < dst.size()) {
        const std::uint32_t blockHeader = static_cast<std::uint32_t>(src[srcPos]) |
                                         (static_cast<std::uint32_t>(src[srcPos + 1]) << 8) |
                                         (static_cast<std::uint32_t>(src[srcPos + 2]) << 16);
        srcPos += 3;

        const bool lastBlock = (blockHeader & 1) != 0;
        const std::uint32_t blockType = (blockHeader >> 1) & 3;
        const std::uint32_t blockSize = (blockHeader >> 3);

        if (blockType == 0) { // Raw block
            if (srcPos + blockSize > src.size() || dstPos + blockSize > dst.size()) return false;
            for (std::size_t i = 0; i < blockSize; ++i) {
                dst[dstPos++] = src[srcPos++];
            }
        } else if (blockType == 1) { // RLE block
            if (srcPos >= src.size() || dstPos + blockSize > dst.size()) return false;
            const std::uint8_t byteVal = src[srcPos++];
            for (std::size_t i = 0; i < blockSize; ++i) {
                dst[dstPos++] = byteVal;
            }
        } else {
            // Compressed block requires full entropy decoder
            return false;
        }

        if (lastBlock) break;
    }

    return dstPos == dst.size();
}

// ============================================================================
// Text KV3 Lexer & Parser Implementation
// ============================================================================

namespace {

enum class TokenType {
    EndOfFile,
    OpenBrace,
    CloseBrace,
    OpenBracket,
    CloseBracket,
    Equals,
    Comma,
    String,
    NumberInt,
    NumberDouble,
    Boolean,
    Null,
    Blob,
    Identifier
};

struct Token {
    TokenType m_type{TokenType::EndOfFile};
    std::string m_text;
    std::int64_t m_intVal{0};
    double m_doubleVal{0.0};
    bool m_boolVal{false};
    KV3Blob m_blobVal;
};

class TextLexer {
private:
    std::string_view m_source;
    std::size_t m_pos{0};

    void SkipWhitespaceAndComments() {
        while (m_pos < m_source.size()) {
            char c = m_source[m_pos];
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++m_pos;
                continue;
            }

            // Single line comment //
            if (c == '/' && m_pos + 1 < m_source.size() && m_source[m_pos + 1] == '/') {
                m_pos += 2;
                while (m_pos < m_source.size() && m_source[m_pos] != '\n') ++m_pos;
                continue;
            }

            // Block comment /* ... */
            if (c == '/' && m_pos + 1 < m_source.size() && m_source[m_pos + 1] == '*') {
                m_pos += 2;
                while (m_pos + 1 < m_source.size()) {
                    if (m_source[m_pos] == '*' && m_source[m_pos + 1] == '/') {
                        m_pos += 2;
                        break;
                    }
                    ++m_pos;
                }
                continue;
            }

            // XML style comment <!-- ... -->
            if (c == '<' && m_pos + 3 < m_source.size() &&
                m_source[m_pos + 1] == '!' && m_source[m_pos + 2] == '-' && m_source[m_pos + 3] == '-') {
                m_pos += 4;
                while (m_pos + 2 < m_source.size()) {
                    if (m_source[m_pos] == '-' && m_source[m_pos + 1] == '-' && m_source[m_pos + 2] == '>') {
                        m_pos += 3;
                        break;
                    }
                    ++m_pos;
                }
                continue;
            }

            break;
        }
    }

public:
    explicit TextLexer(std::string_view src) : m_source(src) {}

    Token NextToken() {
        SkipWhitespaceAndComments();

        if (m_pos >= m_source.size()) {
            return Token{TokenType::EndOfFile, {}, 0, 0.0, false, {}};
        }

        char c = m_source[m_pos];

        if (c == '{') { ++m_pos; return Token{TokenType::OpenBrace, "{", 0, 0.0, false, {}}; }
        if (c == '}') { ++m_pos; return Token{TokenType::CloseBrace, "}", 0, 0.0, false, {}}; }
        if (c == '[') { ++m_pos; return Token{TokenType::OpenBracket, "[", 0, 0.0, false, {}}; }
        if (c == ']') { ++m_pos; return Token{TokenType::CloseBracket, "]", 0, 0.0, false, {}}; }
        if (c == '=') { ++m_pos; return Token{TokenType::Equals, "=", 0, 0.0, false, {}}; }
        if (c == ',') { ++m_pos; return Token{TokenType::Comma, ",", 0, 0.0, false, {}}; }

        // Binary Blob #[ 00 AA FF ]
        if (c == '#' && m_pos + 1 < m_source.size() && m_source[m_pos + 1] == '[') {
            m_pos += 2;
            Token tok{TokenType::Blob, "", 0, 0.0, false, {}};
            while (m_pos < m_source.size()) {
                SkipWhitespaceAndComments();
                if (m_pos < m_source.size() && m_source[m_pos] == ']') {
                    ++m_pos;
                    break;
                }
                if (m_pos + 1 < m_source.size()) {
                    auto hexVal = [](char h) -> int {
                        if (h >= '0' && h <= '9') return h - '0';
                        if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                        if (h >= 'A' && h <= 'F') return h - 'A' + 10;
                        return -1;
                    };
                    int hi = hexVal(m_source[m_pos]);
                    int lo = hexVal(m_source[m_pos + 1]);
                    if (hi >= 0 && lo >= 0) {
                        tok.m_blobVal.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
                        m_pos += 2;
                    } else {
                        ++m_pos;
                    }
                } else {
                    ++m_pos;
                }
            }
            return tok;
        }

        // Quoted string "..." or multiline """..."""
        if (c == '"') {
            bool multiLine = false;
            if (m_pos + 2 < m_source.size() && m_source[m_pos + 1] == '"' && m_source[m_pos + 2] == '"') {
                multiLine = true;
                m_pos += 3;
            } else {
                ++m_pos;
            }

            std::string content;
            while (m_pos < m_source.size()) {
                if (multiLine) {
                    if (m_pos + 2 < m_source.size() && m_source[m_pos] == '"' && m_source[m_pos + 1] == '"' && m_source[m_pos + 2] == '"') {
                        m_pos += 3;
                        break;
                    }
                    content.push_back(m_source[m_pos++]);
                } else {
                    if (m_source[m_pos] == '"') {
                        ++m_pos;
                        break;
                    }
                    if (m_source[m_pos] == '\\' && m_pos + 1 < m_source.size()) {
                        char esc = m_source[++m_pos];
                        if (esc == 'n') content.push_back('\n');
                        else if (esc == 't') content.push_back('\t');
                        else if (esc == 'r') content.push_back('\r');
                        else if (esc == '\\') content.push_back('\\');
                        else if (esc == '"') content.push_back('"');
                        else content.push_back(esc);
                        ++m_pos;
                    } else {
                        content.push_back(m_source[m_pos++]);
                    }
                }
            }
            return Token{TokenType::String, std::move(content), 0, 0.0, false, {}};
        }

        // Numbers or Identifier
        std::size_t start = m_pos;
        bool isNumber = (std::isdigit(static_cast<unsigned char>(c)) != 0 ||
                        ((c == '-' || c == '+') && m_pos + 1 < m_source.size() &&
                         std::isdigit(static_cast<unsigned char>(m_source[m_pos + 1])) != 0));

        while (m_pos < m_source.size()) {
            char ch = m_source[m_pos];
            if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '.' || ch == '-' || ch == '+') {
                ++m_pos;
            } else {
                break;
            }
        }

        std::string_view word = m_source.substr(start, m_pos - start);

        if (word == "true") {
            return Token{TokenType::Boolean, "true", 1, 1.0, true, {}};
        }
        if (word == "false") {
            return Token{TokenType::Boolean, "false", 0, 0.0, false, {}};
        }
        if (word == "null") {
            return Token{TokenType::Null, "null", 0, 0.0, false, {}};
        }

        if (isNumber) {
            bool hasDotOrExp = (word.find('.') != std::string_view::npos ||
                                word.find('e') != std::string_view::npos ||
                                word.find('E') != std::string_view::npos);
            if (hasDotOrExp) {
                double val = 0.0;
                auto [ptr, ec] = std::from_chars(word.data(), word.data() + word.size(), val);
                if (ec == std::errc()) {
                    return Token{TokenType::NumberDouble, std::string(word), static_cast<std::int64_t>(val), val, false, {}};
                }
            } else {
                std::int64_t val = 0;
                int base = 10;
                std::string_view numPart = word;
                if (word.starts_with("0x") || word.starts_with("0X")) {
                    base = 16;
                    numPart.remove_prefix(2);
                }
                auto [ptr, ec] = std::from_chars(numPart.data(), numPart.data() + numPart.size(), val, base);
                if (ec == std::errc()) {
                    return Token{TokenType::NumberInt, std::string(word), val, static_cast<double>(val), false, {}};
                }
            }
        }

        return Token{TokenType::Identifier, std::string(word), 0, 0.0, false, {}};
    }
};

class TextParser {
private:
    TextLexer m_lexer;
    Token m_currentToken;

    void Advance() {
        m_currentToken = m_lexer.NextToken();
    }

    KV3Value ParseValue() {
        switch (m_currentToken.m_type) {
            case TokenType::Null:
                Advance();
                return KV3Value{nullptr};
            case TokenType::Boolean: {
                bool b = m_currentToken.m_boolVal;
                Advance();
                return KV3Value{b};
            }
            case TokenType::NumberInt: {
                std::int64_t v = m_currentToken.m_intVal;
                Advance();
                return KV3Value{v};
            }
            case TokenType::NumberDouble: {
                double v = m_currentToken.m_doubleVal;
                Advance();
                return KV3Value{v};
            }
            case TokenType::String: {
                std::string s = std::move(m_currentToken.m_text);
                Advance();
                return KV3Value{std::move(s)};
            }
            case TokenType::Blob: {
                KV3Blob b = std::move(m_currentToken.m_blobVal);
                Advance();
                return KV3Value{std::move(b)};
            }
            case TokenType::OpenBracket:
                return ParseArray();
            case TokenType::OpenBrace:
                return ParseTable();
            case TokenType::Identifier: {
                std::string s = std::move(m_currentToken.m_text);
                Advance();
                return KV3Value{std::move(s)};
            }
            default:
                Advance();
                return KV3Value{nullptr};
        }
    }

    KV3Value ParseArray() {
        Advance(); // skip '['
        KV3Array arr;
        while (m_currentToken.m_type != TokenType::CloseBracket && m_currentToken.m_type != TokenType::EndOfFile) {
            arr.push_back(ParseValue());
            if (m_currentToken.m_type == TokenType::Comma) {
                Advance();
            }
        }
        if (m_currentToken.m_type == TokenType::CloseBracket) {
            Advance();
        }
        return KV3Value{std::move(arr)};
    }

    KV3Value ParseTable() {
        Advance(); // skip '{'
        KV3Table tbl;
        while (m_currentToken.m_type != TokenType::CloseBrace && m_currentToken.m_type != TokenType::EndOfFile) {
            if (m_currentToken.m_type == TokenType::Identifier || m_currentToken.m_type == TokenType::String) {
                std::string key = std::move(m_currentToken.m_text);
                Advance();

                if (m_currentToken.m_type == TokenType::Equals) {
                    Advance();
                }

                KV3Value val = ParseValue();
                tbl.emplace_back(std::move(key), std::move(val));

                if (m_currentToken.m_type == TokenType::Comma) {
                    Advance();
                }
            } else {
                Advance();
            }
        }
        if (m_currentToken.m_type == TokenType::CloseBrace) {
            Advance();
        }
        return KV3Value{std::move(tbl)};
    }

public:
    explicit TextParser(std::string_view src) : m_lexer(src) {
        Advance();
    }

    KV3Value ParseRoot() {
        if (m_currentToken.m_type == TokenType::OpenBrace) {
            return ParseTable();
        }
        return ParseValue();
    }
};

} // anonymous namespace

// ============================================================================
// CKV3Document Text Formatting & Parsing
// ============================================================================

bool CKV3Document::LoadFromText(std::string_view text) {
    // Parse KV3 Header <!-- kv3 encoding:text:version{...} format:generic:version{...} -->
    std::size_t headerStart = text.find("<!-- kv3");
    if (headerStart != std::string_view::npos) {
        std::size_t headerEnd = text.find("-->", headerStart);
        if (headerEnd != std::string_view::npos) {
            std::string_view headerStr = text.substr(headerStart, headerEnd + 3 - headerStart);

            // Extract encoding GUID
            std::size_t encPos = headerStr.find("encoding:");
            if (encPos != std::string_view::npos) {
                std::size_t vPos = headerStr.find("version{", encPos);
                if (vPos != std::string_view::npos) {
                    std::size_t closeBrace = headerStr.find('}', vPos);
                    if (closeBrace != std::string_view::npos) {
                        m_header.m_encodingGuid = KV3Guid_t::Parse(headerStr.substr(vPos + 8, closeBrace - (vPos + 8)));
                    }
                }
            }

            // Extract format GUID
            std::size_t fmtPos = headerStr.find("format:");
            if (fmtPos != std::string_view::npos) {
                std::size_t vPos = headerStr.find("version{", fmtPos);
                if (vPos != std::string_view::npos) {
                    std::size_t closeBrace = headerStr.find('}', vPos);
                    if (closeBrace != std::string_view::npos) {
                        m_header.m_formatGuid = KV3Guid_t::Parse(headerStr.substr(vPos + 8, closeBrace - (vPos + 8)));
                    }
                }
            }

            text.remove_prefix(headerEnd + 3);
        }
    }

    TextParser parser(text);
    m_root = parser.ParseRoot();
    return true;
}

namespace {

void FormatValue(std::ostringstream& ss, const KV3Value& val, int indentLevel) {
    std::string indent(static_cast<std::size_t>(indentLevel * 4), ' ');

    switch (val.GetType()) {
        case KV3Type::Null:
            ss << "null";
            break;
        case KV3Type::Boolean:
            ss << (val.AsBool() ? "true" : "false");
            break;
        case KV3Type::Integer:
            ss << val.AsInt();
            break;
        case KV3Type::Double: {
            std::ostringstream numSs;
            numSs << std::fixed << std::setprecision(6) << val.AsDouble();
            ss << numSs.str();
            break;
        }
        case KV3Type::String: {
            ss << '"';
            for (char c : val.AsString()) {
                if (c == '"') ss << "\\\"";
                else if (c == '\\') ss << "\\\\";
                else if (c == '\n') ss << "\\n";
                else if (c == '\t') ss << "\\t";
                else ss << c;
            }
            ss << '"';
            break;
        }
        case KV3Type::BinaryBlob: {
            ss << "#[ ";
            for (std::uint8_t b : val.AsBlob()) {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << ' ';
            }
            ss << "]";
            break;
        }
        case KV3Type::Array: {
            const auto& arr = val.AsArray();
            if (arr.empty()) {
                ss << "[]";
            } else {
                ss << "[\n";
                for (std::size_t i = 0; i < arr.size(); ++i) {
                    ss << indent << "    ";
                    FormatValue(ss, arr[i], indentLevel + 1);
                    if (i + 1 < arr.size()) ss << ",";
                    ss << "\n";
                }
                ss << indent << "]";
            }
            break;
        }
        case KV3Type::Table: {
            const auto& tbl = val.AsTable();
            ss << "{\n";
            for (const auto& [k, v] : tbl) {
                ss << indent << "    " << k << " = ";
                FormatValue(ss, v, indentLevel + 1);
                ss << "\n";
            }
            ss << indent << "}";
            break;
        }
    }
}

} // anonymous namespace

std::string CKV3Document::SaveToText() const {
    std::ostringstream ss;
    ss << "<!-- kv3 encoding:text:version{"
       << m_header.m_encodingGuid.ToString().substr(1, 36)
       << "} format:generic:version{"
       << m_header.m_formatGuid.ToString().substr(1, 36)
       << "} -->\n";

    FormatValue(ss, m_root, 0);
    ss << "\n";
    return ss.str();
}

// ============================================================================
// Binary KV3 (VKV3) Serialization & Deserialization
// ============================================================================

namespace {

inline constexpr std::uint32_t VKV3_MAGIC_ASCII = 0x33564B56; // "VKV3"
inline constexpr std::uint32_t VKV3_MAGIC_RAW   = 0x03564B56; // "VKV\x03"

class BinaryWriter {
public:
    std::vector<std::uint8_t> m_bytes;
    std::vector<std::string> m_stringTable;

    std::uint32_t AddString(const std::string& str) {
        for (std::uint32_t i = 0; i < m_stringTable.size(); ++i) {
            if (m_stringTable[i] == str) return i;
        }
        m_stringTable.push_back(str);
        return static_cast<std::uint32_t>(m_stringTable.size() - 1);
    }

    void WriteU8(std::uint8_t val) { m_bytes.push_back(val); }

    void WriteU32(std::uint32_t val) {
        m_bytes.push_back(static_cast<std::uint8_t>(val & 0xFF));
        m_bytes.push_back(static_cast<std::uint8_t>((val >> 8) & 0xFF));
        m_bytes.push_back(static_cast<std::uint8_t>((val >> 16) & 0xFF));
        m_bytes.push_back(static_cast<std::uint8_t>((val >> 24) & 0xFF));
    }

    void WriteI64(std::int64_t val) {
        auto uval = static_cast<std::uint64_t>(val);
        for (int i = 0; i < 8; ++i) {
            m_bytes.push_back(static_cast<std::uint8_t>((uval >> (i * 8)) & 0xFF));
        }
    }

    void WriteDouble(double val) {
        std::uint64_t uval = 0;
        std::memcpy(&uval, &val, sizeof(double));
        for (int i = 0; i < 8; ++i) {
            m_bytes.push_back(static_cast<std::uint8_t>((uval >> (i * 8)) & 0xFF));
        }
    }

    void WriteBytes(std::span<const std::uint8_t> span) {
        m_bytes.insert(m_bytes.end(), span.begin(), span.end());
    }

    void SerializeNode(const KV3Value& node) {
        const auto type = node.GetType();
        WriteU8(static_cast<std::uint8_t>(type));

        switch (type) {
            case KV3Type::Null:
                break;
            case KV3Type::Boolean:
                WriteU8(node.AsBool() ? 1 : 0);
                break;
            case KV3Type::Integer:
                WriteI64(node.AsInt());
                break;
            case KV3Type::Double:
                WriteDouble(node.AsDouble());
                break;
            case KV3Type::String: {
                std::uint32_t strId = AddString(node.AsString());
                WriteU32(strId);
                break;
            }
            case KV3Type::BinaryBlob: {
                const auto& blob = node.AsBlob();
                WriteU32(static_cast<std::uint32_t>(blob.size()));
                WriteBytes(blob);
                break;
            }
            case KV3Type::Array: {
                const auto& arr = node.AsArray();
                WriteU32(static_cast<std::uint32_t>(arr.size()));
                for (const auto& item : arr) {
                    SerializeNode(item);
                }
                break;
            }
            case KV3Type::Table: {
                const auto& tbl = node.AsTable();
                WriteU32(static_cast<std::uint32_t>(tbl.size()));
                for (const auto& [k, v] : tbl) {
                    std::uint32_t keyId = AddString(k);
                    WriteU32(keyId);
                    SerializeNode(v);
                }
                break;
            }
        }
    }
};

class BinaryReader {
private:
    std::span<const std::uint8_t> m_span;
    std::size_t m_pos{0};
    std::vector<std::string> m_stringTable;

public:
    explicit BinaryReader(std::span<const std::uint8_t> span) : m_span(span) {}

    [[nodiscard]] bool HasBytes(std::size_t count) const noexcept {
        return m_pos + count <= m_span.size();
    }

    std::uint8_t ReadU8() {
        if (!HasBytes(1)) return 0;
        return m_span[m_pos++];
    }

    std::uint32_t ReadU32() {
        if (!HasBytes(4)) return 0;
        std::uint32_t v = static_cast<std::uint32_t>(m_span[m_pos]) |
                         (static_cast<std::uint32_t>(m_span[m_pos + 1]) << 8) |
                         (static_cast<std::uint32_t>(m_span[m_pos + 2]) << 16) |
                         (static_cast<std::uint32_t>(m_span[m_pos + 3]) << 24);
        m_pos += 4;
        return v;
    }

    std::int64_t ReadI64() {
        if (!HasBytes(8)) return 0;
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= (static_cast<std::uint64_t>(m_span[m_pos + static_cast<std::size_t>(i)]) << (i * 8));
        }
        m_pos += 8;
        return static_cast<std::int64_t>(v);
    }

    double ReadDouble() {
        if (!HasBytes(8)) return 0.0;
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= (static_cast<std::uint64_t>(m_span[m_pos + static_cast<std::size_t>(i)]) << (i * 8));
        }
        m_pos += 8;
        double d = 0.0;
        std::memcpy(&d, &v, sizeof(double));
        return d;
    }

    void ReadStringTable() {
        std::uint32_t count = ReadU32();
        m_stringTable.clear();
        m_stringTable.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint32_t len = ReadU32();
            if (HasBytes(len)) {
                std::string s(reinterpret_cast<const char*>(&m_span[m_pos]), len);
                m_pos += len;
                m_stringTable.push_back(std::move(s));
            } else {
                m_stringTable.push_back({});
            }
        }
    }

    std::string GetString(std::uint32_t id) const {
        if (id < m_stringTable.size()) return m_stringTable[id];
        return {};
    }

    KV3Value DeserializeNode() {
        const std::uint8_t typeByte = ReadU8();
        const auto type = static_cast<KV3Type>(typeByte);

        switch (type) {
            case KV3Type::Null:
                return KV3Value{nullptr};
            case KV3Type::Boolean:
                return KV3Value{ReadU8() != 0};
            case KV3Type::Integer:
                return KV3Value{ReadI64()};
            case KV3Type::Double:
                return KV3Value{ReadDouble()};
            case KV3Type::String: {
                std::uint32_t id = ReadU32();
                return KV3Value{GetString(id)};
            }
            case KV3Type::BinaryBlob: {
                std::uint32_t len = ReadU32();
                KV3Blob b;
                if (HasBytes(len)) {
                    b.assign(&m_span[m_pos], &m_span[m_pos + len]);
                    m_pos += len;
                }
                return KV3Value{std::move(b)};
            }
            case KV3Type::Array: {
                std::uint32_t count = ReadU32();
                KV3Array arr;
                arr.reserve(count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    arr.push_back(DeserializeNode());
                }
                return KV3Value{std::move(arr)};
            }
            case KV3Type::Table: {
                std::uint32_t count = ReadU32();
                KV3Table tbl;
                tbl.reserve(count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    std::uint32_t keyId = ReadU32();
                    std::string key = GetString(keyId);
                    KV3Value val = DeserializeNode();
                    tbl.emplace_back(std::move(key), std::move(val));
                }
                return KV3Value{std::move(tbl)};
            }
            default:
                return KV3Value{nullptr};
        }
    }
};

} // anonymous namespace

bool CKV3Document::LoadFromBinary(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 24) return false;

    // Check Magic
    const std::uint32_t magic = static_cast<std::uint32_t>(bytes[0]) |
                               (static_cast<std::uint32_t>(bytes[1]) << 8) |
                               (static_cast<std::uint32_t>(bytes[2]) << 16) |
                               (static_cast<std::uint32_t>(bytes[3]) << 24);

    if (magic != VKV3_MAGIC_ASCII && magic != VKV3_MAGIC_RAW) {
        return false;
    }

    // Format GUID (16 bytes)
    std::memcpy(m_header.m_formatGuid.m_bytes.data(), &bytes[4], 16);

    // Compression Type (4 bytes)
    const std::uint32_t compVal = static_cast<std::uint32_t>(bytes[20]) |
                                 (static_cast<std::uint32_t>(bytes[21]) << 8) |
                                 (static_cast<std::uint32_t>(bytes[22]) << 16) |
                                 (static_cast<std::uint32_t>(bytes[23]) << 24);
    m_header.m_eCompression = static_cast<KV3CompressionType>(compVal);

    std::size_t offset = 24;
    std::vector<std::uint8_t> decompressedBuffer;

    if (m_header.m_eCompression == KV3CompressionType::None) {
        BinaryReader reader(bytes.subspan(offset));
        reader.ReadStringTable();
        m_root = reader.DeserializeNode();
        return true;
    } else if (m_header.m_eCompression == KV3CompressionType::LZ4) {
        if (bytes.size() < offset + 8) return false;
        const std::uint32_t uncompressedSize = static_cast<std::uint32_t>(bytes[offset]) |
                                              (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                                              (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                                              (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
        const std::uint32_t compressedSize = static_cast<std::uint32_t>(bytes[offset + 4]) |
                                            (static_cast<std::uint32_t>(bytes[offset + 5]) << 8) |
                                            (static_cast<std::uint32_t>(bytes[offset + 6]) << 16) |
                                            (static_cast<std::uint32_t>(bytes[offset + 7]) << 24);
        offset += 8;

        if (bytes.size() < offset + compressedSize) return false;

        decompressedBuffer.resize(uncompressedSize);
        if (!CLZ4Codec::Decompress(bytes.subspan(offset, compressedSize), decompressedBuffer)) {
            return false;
        }

        BinaryReader reader(decompressedBuffer);
        reader.ReadStringTable();
        m_root = reader.DeserializeNode();
        return true;
    } else if (m_header.m_eCompression == KV3CompressionType::ZSTD) {
        if (bytes.size() < offset + 8) return false;
        const std::uint32_t uncompressedSize = static_cast<std::uint32_t>(bytes[offset]) |
                                              (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                                              (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                                              (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
        const std::uint32_t compressedSize = static_cast<std::uint32_t>(bytes[offset + 4]) |
                                            (static_cast<std::uint32_t>(bytes[offset + 5]) << 8) |
                                            (static_cast<std::uint32_t>(bytes[offset + 6]) << 16) |
                                            (static_cast<std::uint32_t>(bytes[offset + 7]) << 24);
        offset += 8;

        decompressedBuffer.resize(uncompressedSize);
        if (!CZSTDCodec::DecompressRawFrame(bytes.subspan(offset, compressedSize), decompressedBuffer)) {
            return false;
        }

        BinaryReader reader(decompressedBuffer);
        reader.ReadStringTable();
        m_root = reader.DeserializeNode();
        return true;
    }

    return false;
}

std::vector<std::uint8_t> CKV3Document::SaveToBinary(KV3CompressionType compression) const {
    BinaryWriter nodeWriter;
    nodeWriter.SerializeNode(m_root);

    // Build uncompressed stream: [String Table] + [Node Stream]
    BinaryWriter streamWriter;
    streamWriter.WriteU32(static_cast<std::uint32_t>(nodeWriter.m_stringTable.size()));
    for (const auto& s : nodeWriter.m_stringTable) {
        streamWriter.WriteU32(static_cast<std::uint32_t>(s.size()));
        streamWriter.WriteBytes(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()));
    }
    streamWriter.WriteBytes(nodeWriter.m_bytes);

    std::vector<std::uint8_t> result;
    // Magic "VKV3"
    result.push_back('V'); result.push_back('K'); result.push_back('V'); result.push_back('3');

    // Format GUID (16 bytes)
    result.insert(result.end(), m_header.m_formatGuid.m_bytes.begin(), m_header.m_formatGuid.m_bytes.end());

    // Compression Type (4 bytes)
    auto compVal = static_cast<std::uint32_t>(compression);
    result.push_back(static_cast<std::uint8_t>(compVal & 0xFF));
    result.push_back(static_cast<std::uint8_t>((compVal >> 8) & 0xFF));
    result.push_back(static_cast<std::uint8_t>((compVal >> 16) & 0xFF));
    result.push_back(static_cast<std::uint8_t>((compVal >> 24) & 0xFF));

    if (compression == KV3CompressionType::None) {
        result.insert(result.end(), streamWriter.m_bytes.begin(), streamWriter.m_bytes.end());
    } else if (compression == KV3CompressionType::LZ4) {
        std::vector<std::uint8_t> compPayload = CLZ4Codec::Compress(streamWriter.m_bytes);
        const auto uncompressedSize = static_cast<std::uint32_t>(streamWriter.m_bytes.size());
        const auto compressedSize = static_cast<std::uint32_t>(compPayload.size());

        // Uncompressed size
        result.push_back(static_cast<std::uint8_t>(uncompressedSize & 0xFF));
        result.push_back(static_cast<std::uint8_t>((uncompressedSize >> 8) & 0xFF));
        result.push_back(static_cast<std::uint8_t>((uncompressedSize >> 16) & 0xFF));
        result.push_back(static_cast<std::uint8_t>((uncompressedSize >> 24) & 0xFF));

        // Compressed size
        result.push_back(static_cast<std::uint8_t>(compressedSize & 0xFF));
        result.push_back(static_cast<std::uint8_t>((compressedSize >> 8) & 0xFF));
        result.push_back(static_cast<std::uint8_t>((compressedSize >> 16) & 0xFF));
        result.push_back(static_cast<std::uint8_t>((compressedSize >> 24) & 0xFF));

        result.insert(result.end(), compPayload.begin(), compPayload.end());
    }

    return result;
}

// ============================================================================
// Source 2 Compiled Resource Containers (resourcesystem.dll)
// ============================================================================

bool CResourceFile::LoadFromMemory(std::span<const std::uint8_t> buffer) {
    if (buffer.size() < sizeof(ResourceHeader_t)) return false;

    m_rawBuffer.assign(buffer.begin(), buffer.end());
    std::memcpy(&m_header, m_rawBuffer.data(), sizeof(ResourceHeader_t));

    if (m_header.m_nMagic != RESOURCE_MAGIC_VASU) {
        return false;
    }

    const std::size_t blockTableSize = static_cast<std::size_t>(m_header.m_nBlockCount) * sizeof(ResourceBlockEntry_t);
    if (m_header.m_nBlockOffset + blockTableSize > buffer.size()) {
        return false;
    }

    m_blockEntries.resize(m_header.m_nBlockCount);
    std::memcpy(m_blockEntries.data(), &m_rawBuffer[m_header.m_nBlockOffset], blockTableSize);

    // Parse RED2 or RERL block if present
    m_externalReferences.clear();
    auto redSpan = GetBlockData(BLOCK_FOURCC_RED2);
    if (redSpan.empty()) {
        redSpan = GetBlockData(BLOCK_FOURCC_RERL);
    }

    if (!redSpan.empty() && redSpan.size() >= 4) {
        std::uint32_t refCount = static_cast<std::uint32_t>(redSpan[0]) |
                                (static_cast<std::uint32_t>(redSpan[1]) << 8) |
                                (static_cast<std::uint32_t>(redSpan[2]) << 16) |
                                (static_cast<std::uint32_t>(redSpan[3]) << 24);

        std::size_t pos = 4;
        for (std::uint32_t i = 0; i < refCount && pos + 8 <= redSpan.size(); ++i) {
            std::uint64_t hash = 0;
            for (int b = 0; b < 8; ++b) {
                hash |= (static_cast<std::uint64_t>(redSpan[pos + static_cast<std::size_t>(b)]) << (b * 8));
            }
            pos += 8;

            std::string path;
            while (pos < redSpan.size() && redSpan[pos] != '\0') {
                path.push_back(static_cast<char>(redSpan[pos++]));
            }
            if (pos < redSpan.size()) ++pos; // skip null terminator

            m_externalReferences.push_back(ResourceExternalRef_t{hash, std::move(path)});
        }
    }

    return true;
}

std::span<const std::uint8_t> CResourceFile::GetBlockData(std::uint32_t blockFourCC) const {
    for (const auto& entry : m_blockEntries) {
        if (entry.m_nBlockId == blockFourCC) {
            if (entry.m_nOffset + entry.m_nSize <= m_rawBuffer.size()) {
                return std::span<const std::uint8_t>(&m_rawBuffer[entry.m_nOffset], entry.m_nSize);
            }
        }
    }
    return {};
}

bool CResourceFile::HasBlock(std::uint32_t blockFourCC) const noexcept {
    return std::any_of(m_blockEntries.begin(), m_blockEntries.end(), [blockFourCC](const auto& entry) {
        return entry.m_nBlockId == blockFourCC;
    });
}

bool CResourceFile::UnpackDataBlockAsKV3(CKV3Document& outDoc) const {
    auto dataSpan = GetBlockData(BLOCK_FOURCC_DATA);
    if (dataSpan.empty()) return false;

    // Check if binary KV3
    if (dataSpan.size() >= 4) {
        const std::uint32_t magic = static_cast<std::uint32_t>(dataSpan[0]) |
                                   (static_cast<std::uint32_t>(dataSpan[1]) << 8) |
                                   (static_cast<std::uint32_t>(dataSpan[2]) << 16) |
                                   (static_cast<std::uint32_t>(dataSpan[3]) << 24);
        if (magic == VKV3_MAGIC_ASCII || magic == VKV3_MAGIC_RAW) {
            return outDoc.LoadFromBinary(dataSpan);
        }
    }

    // Check if text KV3
    std::string_view sv(reinterpret_cast<const char*>(dataSpan.data()), dataSpan.size());
    if (sv.find("<!-- kv3") != std::string_view::npos || sv.find('{') != std::string_view::npos) {
        return outDoc.LoadFromText(sv);
    }

    return false;
}

std::vector<std::uint8_t> CResourceFile::BuildContainer(
    std::span<const std::uint8_t> dataBlockPayload,
    const std::vector<std::string>& externalRefs)
{
    std::vector<std::uint8_t> container;

    // Header setup
    ResourceHeader_t header{};
    header.m_nMagic = RESOURCE_MAGIC_VASU;
    header.m_nHeaderVersion = 12;
    header.m_nVersion = 1;
    header.m_nFlags = 0;
    header.m_nBlockOffset = sizeof(ResourceHeader_t);

    const bool hasRefs = !externalRefs.empty();
    header.m_nBlockCount = hasRefs ? 2 : 1;

    // Prepare RED2 block payload if refs present
    std::vector<std::uint8_t> redPayload;
    if (hasRefs) {
        const auto refCount = static_cast<std::uint32_t>(externalRefs.size());
        redPayload.push_back(static_cast<std::uint8_t>(refCount & 0xFF));
        redPayload.push_back(static_cast<std::uint8_t>((refCount >> 8) & 0xFF));
        redPayload.push_back(static_cast<std::uint8_t>((refCount >> 16) & 0xFF));
        redPayload.push_back(static_cast<std::uint8_t>((refCount >> 24) & 0xFF));

        for (const auto& path : externalRefs) {
            // 8-byte hash
            std::uint64_t dummyHash = 0x1122334455667788ULL;
            for (int b = 0; b < 8; ++b) {
                redPayload.push_back(static_cast<std::uint8_t>((dummyHash >> (b * 8)) & 0xFF));
            }
            // Null-terminated string
            for (char c : path) redPayload.push_back(static_cast<std::uint8_t>(c));
            redPayload.push_back(0);
        }
    }

    // Prepare block descriptor entries
    std::vector<ResourceBlockEntry_t> blocks;
    blocks.resize(header.m_nBlockCount);

    const std::uint32_t dataStartOffset = static_cast<std::uint32_t>(
        sizeof(ResourceHeader_t) + header.m_nBlockCount * sizeof(ResourceBlockEntry_t)
    );

    std::size_t curOffset = dataStartOffset;

    if (hasRefs) {
        blocks[0].m_nBlockId = BLOCK_FOURCC_RED2;
        blocks[0].m_nOffset = static_cast<std::uint32_t>(curOffset);
        blocks[0].m_nSize = static_cast<std::uint32_t>(redPayload.size());
        curOffset += redPayload.size();

        blocks[1].m_nBlockId = BLOCK_FOURCC_DATA;
        blocks[1].m_nOffset = static_cast<std::uint32_t>(curOffset);
        blocks[1].m_nSize = static_cast<std::uint32_t>(dataBlockPayload.size());
    } else {
        blocks[0].m_nBlockId = BLOCK_FOURCC_DATA;
        blocks[0].m_nOffset = static_cast<std::uint32_t>(curOffset);
        blocks[0].m_nSize = static_cast<std::uint32_t>(dataBlockPayload.size());
    }

    // Write header
    container.resize(sizeof(ResourceHeader_t));
    std::memcpy(container.data(), &header, sizeof(ResourceHeader_t));

    // Write block table
    const std::size_t tableBytes = blocks.size() * sizeof(ResourceBlockEntry_t);
    std::size_t oldSize = container.size();
    container.resize(oldSize + tableBytes);
    std::memcpy(&container[oldSize], blocks.data(), tableBytes);

    // Write RED2 payload
    if (hasRefs) {
        container.insert(container.end(), redPayload.begin(), redPayload.end());
    }

    // Write DATA payload
    container.insert(container.end(), dataBlockPayload.begin(), dataBlockPayload.end());

    return container;
}

} // namespace cs2::resourcesystem
