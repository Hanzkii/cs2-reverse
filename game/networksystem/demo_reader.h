#ifndef CS2_NETWORKSYSTEM_DEMO_READER_H
#define CS2_NETWORKSYSTEM_DEMO_READER_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include <span>
#include <memory>

#include "game/client/usercmd_wire.h"
#include "game/networksystem/netmessage_framing.h"

namespace cs2::networksystem {

// ============================================================================
// Source 2 Match Recording (.dem) Container Definitions
// Reconstructed via Ghidra static analysis of networksystem.dll and engine2.dll
// ============================================================================

inline constexpr std::string_view DEMO_MAGIC_SOURCE2 = "PBDEMS2"; // 8 bytes with null terminator
inline constexpr std::string_view DEMO_MAGIC_LEGACY  = "HL2DEMO"; // 8 bytes with null terminator

/**
 * @brief Source 2 Demo Command Identifiers (EDemoCommands in demo.proto).
 */
enum class EDemoCommand : uint32_t {
    DEM_Error               = 0xFFFFFFFF,
    DEM_Stop                = 0,
    DEM_FileHeader          = 1,
    DEM_FileInfo            = 2,
    DEM_SyncTick            = 3,
    DEM_SendTables          = 4,
    DEM_ClassInfo           = 5,
    DEM_StringTables        = 6,
    DEM_Packet              = 7,
    DEM_SignonPacket        = 8,
    DEM_ConsoleCmd          = 9,
    DEM_CustomData          = 10,
    DEM_CustomDataCallbacks = 11,
    DEM_UserCmd             = 12,
    DEM_FullPacket          = 13,
    DEM_SaveGame            = 14,
    DEM_SpawnGroups         = 15,
    DEM_AnimationData       = 16,
    DEM_AnimationHeader     = 17,
    DEM_Recovery            = 18,
    DEM_Max                 = 19,
    DEM_IsCompressed        = 64, // 0x40: Valve Snappy compression bitmask

    // Alternate numeric mappings for command loop dispatch:
    DEM_Packet_Alt          = 4,
    DEM_SignonPacket_Alt    = 5,
    DEM_FullPacket_Alt      = 7,
    DEM_CustomData_Alt      = 8,
};

/**
 * @brief High-level metadata parsed from container header & CDemoFileHeader.
 */
struct DemoHeader {
    std::string magic{"PBDEMS2"};
    int32_t fileProtocol{4};
    int32_t networkProtocol{13980};
    float serverTickrate{64.0f};
    std::string mapName{"de_dust2"};
    std::string serverName{"Counter-Strike 2 Dedicated Server"};
    std::string clientName{"Source2Client"};
    std::string gameDirectory{"csgo"};
    int32_t buildNum{25966348};
    int32_t serverStartTick{0};
    int32_t summaryOffset{0};
};

/**
 * @brief Sub-tick input step reconstructed from CSubtickMoveStep.
 */
struct SubTickAnalogStep {
    uint64_t button{0};
    bool pressed{false};
    float when{0.0f}; // Fractional offset in interval [0.0, 1.0]
    float analogForwardDelta{0.0f};
    float analogLeftDelta{0.0f};
    float analogPitchDelta{0.0f};
    float analogYawDelta{0.0f};
};

/**
 * @brief Sub-tick discrete spatial observation entry from CSGOInputHistoryEntryPB.
 */
struct SubTickInputHistory {
    float viewAngles[3]{0.0f, 0.0f, 0.0f};
    float observationPosition[3]{0.0f, 0.0f, 0.0f};
    int32_t targetEntityIndex{-1};
    int32_t playerTickCount{0};
    float playerTickFraction{0.0f}; // Fractional sub-tick offset
};

/**
 * @brief High-level telemetry representation of unpacked sub-tick user commands.
 */
struct ExtractedUserCmd {
    int32_t commandNumber{0};
    int32_t clientTick{0};
    uint64_t buttons{0};
    float forwardMove{0.0f};
    float leftMove{0.0f};
    float upMove{0.0f};
    float viewAngles[3]{0.0f, 0.0f, 0.0f};
    std::vector<SubTickAnalogStep> subtickSteps;
    std::vector<SubTickInputHistory> inputHistory;
};

/**
 * @brief Unpacked command frame from the match recording bitstream.
 */
struct DemoFrame {
    uint32_t command{0};
    int32_t tick{0};
    uint32_t flags{0};
    bool isCompressed{false};
    std::vector<uint8_t> rawData;
    std::vector<uint8_t> decompressedData;

    [[nodiscard]] const std::vector<uint8_t>& GetPayload() const noexcept {
        return isCompressed && !decompressedData.empty() ? decompressedData : rawData;
    }
};

/**
 * @brief Aggregate telemetry metrics for demo container inspection.
 */
struct DemoTelemetryStats {
    uint32_t totalFrames{0};
    uint32_t headerFrames{0};
    uint32_t packetFrames{0};
    uint32_t signonFrames{0};
    uint32_t fullPacketFrames{0};
    uint32_t customDataFrames{0};
    uint32_t userCmdFrames{0};
    uint32_t compressedFrames{0};
    uint32_t uncompressedFrames{0};
    size_t totalRawBytes{0};
    size_t totalDecompressedBytes{0};
    double averagePacketSize{0.0};
    double compressionRatio{0.0};

    // Sub-tick user command distribution
    uint32_t totalUserCmds{0};
    uint32_t totalSubtickSteps{0};
    uint32_t totalInputHistoryEntries{0};
    std::unordered_map<uint64_t, uint32_t> buttonEventDistribution;

    // Entity delta synchronization metrics
    uint32_t totalPacketEntitiesMessages{0};
    uint32_t totalEntityDeltas{0};
    uint32_t totalEntityCreations{0};
    uint32_t totalEntityDeletions{0};
};

/**
 * @brief Standalone pure ISO C++20 Google Snappy block compressor and decompressor.
 * Complies with the official Snappy framing specification used by Valve Source 2.
 */
class CSnappyCodec {
public:
    /**
     * @brief Reads the VarInt uncompressed payload length prefix.
     */
    static bool GetUncompressedLength(const uint8_t* src, size_t srcLen, size_t& outLen, size_t& varIntBytes);

    /**
     * @brief Decompresses a raw Snappy byte block into dst buffer.
     */
    static bool Decompress(const uint8_t* src, size_t srcLen, uint8_t* dst, size_t dstCapacity, size_t& outWritten);

    /**
     * @brief Decompresses a raw Snappy byte block into a newly allocated vector.
     */
    static bool Decompress(std::span<const uint8_t> src, std::vector<uint8_t>& outDst);

    /**
     * @brief Compresses a raw byte block using Snappy format.
     */
    static std::vector<uint8_t> Compress(std::span<const uint8_t> src);
};

/**
 * @brief Utility for unpacking sub-tick user command bitstreams.
 */
class CSubTickUserCmdExtractor {
public:
    static bool ExtractFromBuffer(const uint8_t* data, size_t size, std::vector<ExtractedUserCmd>& outCmds);
    static bool ExtractFromBasePB(const client::CBaseUserCmdPB& baseCmd, ExtractedUserCmd& outCmd);
    static bool ExtractFromCSGOPB(const client::CSGOUserCmdPB& csgoCmd, ExtractedUserCmd& outCmd);
};

/**
 * @brief Clean-room C++20 CS2 match recording container parser.
 */
class CDemoReader {
public:
    CDemoReader() = default;

    /**
     * @brief Reads and parses a .dem file from disk.
     */
    bool Open(const std::string& filepath);

    /**
     * @brief Parses an in-memory demo stream buffer.
     */
    bool ParseMemory(const uint8_t* data, size_t size);

    [[nodiscard]] const DemoHeader& GetHeader() const noexcept { return m_Header; }
    [[nodiscard]] const std::vector<DemoFrame>& GetFrames() const noexcept { return m_Frames; }
    [[nodiscard]] const std::vector<ExtractedUserCmd>& GetUserCmds() const noexcept { return m_ExtractedCmds; }
    [[nodiscard]] const DemoTelemetryStats& GetStats() const noexcept { return m_Stats; }

    static bool IsPacketCommand(uint32_t cmd) noexcept {
        return cmd == static_cast<uint32_t>(EDemoCommand::DEM_Packet) ||
               cmd == static_cast<uint32_t>(EDemoCommand::DEM_Packet_Alt);
    }

    static bool IsSignonPacketCommand(uint32_t cmd) noexcept {
        return cmd == static_cast<uint32_t>(EDemoCommand::DEM_SignonPacket) ||
               cmd == static_cast<uint32_t>(EDemoCommand::DEM_SignonPacket_Alt);
    }

    static bool IsFullPacketCommand(uint32_t cmd) noexcept {
        return cmd == static_cast<uint32_t>(EDemoCommand::DEM_FullPacket) ||
               cmd == static_cast<uint32_t>(EDemoCommand::DEM_FullPacket_Alt);
    }

    static bool IsCustomDataCommand(uint32_t cmd) noexcept {
        return cmd == static_cast<uint32_t>(EDemoCommand::DEM_CustomData) ||
               cmd == static_cast<uint32_t>(EDemoCommand::DEM_CustomData_Alt);
    }

    static bool IsFileHeaderCommand(uint32_t cmd) noexcept {
        return cmd == static_cast<uint32_t>(EDemoCommand::DEM_FileHeader);
    }

    static bool IsUserCmdCommand(uint32_t cmd) noexcept {
        return cmd == static_cast<uint32_t>(EDemoCommand::DEM_UserCmd);
    }

private:
    bool ParseHeaderBlock(client::CByteStreamReader& reader);
    bool ParseFrameLoop(client::CByteStreamReader& reader);
    void ProcessFramePayload(const DemoFrame& frame);
    void ProcessNetMessages(const uint8_t* data, size_t size);

    DemoHeader m_Header;
    std::vector<DemoFrame> m_Frames;
    std::vector<ExtractedUserCmd> m_ExtractedCmds;
    DemoTelemetryStats m_Stats;
};

/**
 * @brief Clean-room synthetic demo stream generator for verification and testing.
 */
class CDemoWriter {
public:
    CDemoWriter();

    void SetHeader(const DemoHeader& header) { m_Header = header; }

    void AddFileHeaderFrame(const DemoHeader& header, int32_t tick = 0);

    void AddPacketFrame(
        int32_t tick,
        const uint8_t* packetData,
        size_t size,
        bool compress = true,
        uint32_t cmdOverride = 0
    );

    void AddSignonPacketFrame(
        int32_t tick,
        const uint8_t* packetData,
        size_t size,
        bool compress = true,
        uint32_t cmdOverride = 0
    );

    void AddFullPacketFrame(
        int32_t tick,
        const uint8_t* packetData,
        size_t size,
        bool compress = true,
        uint32_t cmdOverride = 0
    );

    void AddCustomDataFrame(
        int32_t tick,
        int32_t callbackIndex,
        const uint8_t* customData,
        size_t size,
        bool compress = false,
        uint32_t cmdOverride = 0
    );

    void AddUserCmdFrame(
        int32_t tick,
        int32_t cmdNumber,
        const client::CSGOUserCmdPB& userCmd,
        bool compress = false
    );

    [[nodiscard]] std::vector<uint8_t> Finalize();
    bool WriteToFile(const std::string& filepath);

private:
    void AppendFrame(uint32_t cmd, int32_t tick, const uint8_t* payload, size_t size, bool compress);

    DemoHeader m_Header;
    std::vector<uint8_t> m_Buffer;
};

} // namespace cs2::networksystem

#endif // CS2_NETWORKSYSTEM_DEMO_READER_H
