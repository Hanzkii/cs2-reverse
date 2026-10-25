#ifndef CS2_CLIENT_USERCMD_WIRE_H
#define CS2_CLIENT_USERCMD_WIRE_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <optional>
#include <algorithm>
#include "viewmatrix.h"

namespace cs2::client {

// ============================================================================
// Source 2 UserCmd Wire & Protobuf Definitions
// Reconstructed via Ghidra static decompilation of client.dll and networksystem.dll
// ============================================================================

/**
 * @brief Standard Source 2 button bitmasks (64-bit uint64).
 */
enum InButtonBits : uint64_t {
    IN_ATTACK       = (1ULL << 0),
    IN_JUMP         = (1ULL << 1),
    IN_DUCK         = (1ULL << 2),
    IN_FORWARD      = (1ULL << 3),
    IN_BACK         = (1ULL << 4),
    IN_USE          = (1ULL << 5),
    IN_CANCEL       = (1ULL << 6),
    IN_LEFT         = (1ULL << 7),
    IN_RIGHT        = (1ULL << 8),
    IN_MOVELEFT     = (1ULL << 9),
    IN_MOVERIGHT    = (1ULL << 10),
    IN_ATTACK2      = (1ULL << 11),
    IN_RUN          = (1ULL << 12),
    IN_RELOAD       = (1ULL << 13),
    IN_ALT1         = (1ULL << 14),
    IN_ALT2         = (1ULL << 15),
    IN_SPEED        = (1ULL << 16),
    IN_WALK         = (1ULL << 17),
    IN_ZOOM         = (1ULL << 19),
    IN_FIRST_MOD    = (1ULL << 20),
    IN_SECOND_MOD   = (1ULL << 21),
    IN_BULLRUSH     = (1ULL << 22),
    IN_GRENADE1     = (1ULL << 23),
    IN_GRENADE2     = (1ULL << 24),
    IN_ATTACK3      = (1ULL << 25),
};

/**
 * @brief Protobuf Wire Types (Google Protobuf specification).
 */
enum class WireType : uint32_t {
    VarInt          = 0,
    Fixed64         = 1,
    LengthDelimited = 2,
    StartGroup      = 3, // deprecated
    EndGroup        = 4, // deprecated
    Fixed32         = 5,
};

/**
 * @brief Lightweight low-level binary buffer reader for protobuf parsing.
 */
class CByteStreamReader {
public:
    CByteStreamReader(const uint8_t* data, size_t size)
        : m_pData(data), m_nSize(size), m_nOffset(0) {}

    [[nodiscard]] bool HasMore() const noexcept { return m_nOffset < m_nSize; }
    [[nodiscard]] size_t GetOffset() const noexcept { return m_nOffset; }
    [[nodiscard]] size_t GetRemaining() const noexcept { return m_nOffset < m_nSize ? m_nSize - m_nOffset : 0; }
    [[nodiscard]] const uint8_t* CurrentPtr() const noexcept { return m_pData + m_nOffset; }

    bool ReadByte(uint8_t& outByte);
    bool ReadVarInt32(uint32_t& outVal);
    bool ReadVarInt64(uint64_t& outVal);
    bool ReadFixed32(uint32_t& outVal);
    bool ReadFloat(float& outVal);
    bool ReadFixed64(uint64_t& outVal);
    bool ReadDouble(double& outVal);
    bool ReadBytes(uint8_t* outDest, size_t len);
    bool ReadString(std::string& outStr, size_t len);
    bool Skip(size_t len);
    bool SkipField(WireType wireType);

private:
    const uint8_t* m_pData;
    size_t m_nSize;
    size_t m_nOffset;
};

/**
 * @brief Lightweight low-level binary buffer writer for protobuf serialization.
 */
class CByteStreamWriter {
public:
    CByteStreamWriter() = default;

    void WriteByte(uint8_t b) { m_Buffer.push_back(b); }
    void WriteVarInt32(uint32_t val);
    void WriteVarInt64(uint64_t val);
    void WriteFixed32(uint32_t val);
    void WriteFloat(float val);
    void WriteFixed64(uint64_t val);
    void WriteDouble(double val);
    void WriteTag(uint32_t fieldNumber, WireType wireType);
    void WriteLengthDelimited(const uint8_t* data, size_t len);
    void WriteString(const std::string& str);
    void WriteBytes(const uint8_t* data, size_t len);

    [[nodiscard]] const std::vector<uint8_t>& GetData() const noexcept { return m_Buffer; }
    [[nodiscard]] size_t GetSize() const noexcept { return m_Buffer.size(); }
    void Clear() noexcept { m_Buffer.clear(); }

private:
    std::vector<uint8_t> m_Buffer;
};

/**
 * @brief CSubtickMoveStep exact memory layout and wire codec.
 * Reconstructed from client.dll:
 *   - VTable: 0x181b2d4d8 (baseline) / 0x181b2f488 (current)
 *   - Size: 0x38 (56 bytes)
 *   - Parse: FUN_1804f4950 (baseline) / FUN_1804f5640 (current)
 *   - Serialize: FUN_1804f4c40 (baseline) / FUN_1804f5930 (current)
 */
class CSubtickMoveStep {
public:
    void* m_pVTable{nullptr};            // +0x00: vtable pointer
    void* m_pInternalArena{nullptr};     // +0x08: Google protobuf Arena pointer
    uint32_t m_nHasBits{0};              // +0x10: _has_bits_ mask
    mutable int32_t m_nCachedSize{0};    // +0x14: _cached_size_
    uint64_t m_nButton{0};               // +0x18: field 1: button mask
    bool m_bPressed{false};              // +0x20: field 2: keydown edge
    uint8_t m_nPad[3]{0, 0, 0};          // +0x21..0x23: alignment padding
    float m_flWhen{0.0f};                // +0x24: field 3: normalized fraction [0.0, 1.0]
    float m_flAnalogForwardDelta{0.0f};  // +0x28: field 4: analog forward delta
    float m_flAnalogLeftDelta{0.0f};     // +0x2c: field 5: analog left delta
    float m_flAnalogPitchDelta{0.0f};    // +0x30: field 8: analog pitch delta
    float m_flAnalogYawDelta{0.0f};      // +0x34: field 9: analog yaw delta

    // Has-bits bitmasks (from FUN_1804f4c40)
    enum HasBits : uint32_t {
        HAS_BUTTON               = (1 << 0), // 0x01
        HAS_PRESSED              = (1 << 1), // 0x02
        HAS_WHEN                 = (1 << 2), // 0x04
        HAS_ANALOG_FORWARD_DELTA = (1 << 3), // 0x08
        HAS_ANALOG_LEFT_DELTA    = (1 << 4), // 0x10
        HAS_ANALOG_PITCH_DELTA   = (1 << 5), // 0x20
        HAS_ANALOG_YAW_DELTA     = (1 << 6), // 0x40
    };

    CSubtickMoveStep() = default;

    // Field setters with automatic has-bits tracking
    void SetButton(uint64_t btn) noexcept { m_nButton = btn; m_nHasBits |= HAS_BUTTON; }
    void SetPressed(bool pressed) noexcept { m_bPressed = pressed; m_nHasBits |= HAS_PRESSED; }
    void SetWhen(float when) noexcept { m_flWhen = std::clamp(when, 0.0f, 1.0f); m_nHasBits |= HAS_WHEN; }
    void SetAnalogForwardDelta(float delta) noexcept { m_flAnalogForwardDelta = delta; m_nHasBits |= HAS_ANALOG_FORWARD_DELTA; }
    void SetAnalogLeftDelta(float delta) noexcept { m_flAnalogLeftDelta = delta; m_nHasBits |= HAS_ANALOG_LEFT_DELTA; }
    void SetAnalogPitchDelta(float delta) noexcept { m_flAnalogPitchDelta = delta; m_nHasBits |= HAS_ANALOG_PITCH_DELTA; }
    void SetAnalogYawDelta(float delta) noexcept { m_flAnalogYawDelta = delta; m_nHasBits |= HAS_ANALOG_YAW_DELTA; }

    [[nodiscard]] size_t ByteSizeLong() const;
    void _InternalSerialize(CByteStreamWriter& writer) const;
    bool _InternalParse(CByteStreamReader& reader);
};
static_assert(sizeof(CSubtickMoveStep) == 0x38, "CSubtickMoveStep memory layout mismatch (must be 56 bytes)");

/**
 * @brief CSGOInputHistoryEntryPB analog shot recording message.
 * Reconstructed from client.dll:
 *   - VTable: 0x181bccd20 (baseline) / 0x181bcecb0 (current)
 */
struct CSGOInputHistoryEntryPB {
    QAngle view_angles{0.0f, 0.0f, 0.0f};
    Vector3 shoot_position{0.0f, 0.0f, 0.0f};
    int32_t target_ent_index{-1};
    int32_t player_tick_count{0};
    float player_tick_fraction{0.0f};
    Vector3 target_head_pos_check{0.0f, 0.0f, 0.0f};
    Vector3 target_abs_pos_check{0.0f, 0.0f, 0.0f};

    [[nodiscard]] size_t ByteSizeLong() const;
    void _InternalSerialize(CByteStreamWriter& writer) const;
    bool _InternalParse(CByteStreamReader& reader);
};

/**
 * @brief Button state protobuf representation (CInButtonStatePB).
 */
struct CInButtonStatePB {
    uint64_t buttonstate1{0};
    uint64_t buttonstate2{0};
    uint64_t buttonstate3{0};

    [[nodiscard]] size_t ByteSizeLong() const;
    void _InternalSerialize(CByteStreamWriter& writer) const;
    bool _InternalParse(CByteStreamReader& reader);
};

/**
 * @brief CBaseUserCmdPB exact memory layout and wire codec.
 * Reconstructed from client.dll:
 *   - VTable: 0x181b2d618 (baseline) / 0x181b2f5c8 (current)
 *   - Parse: FUN_1804f5c00 (baseline) / FUN_1804f68f0 (current)
 *   - Serialize: FUN_1804f63f0 (baseline) / FUN_1804f70e0 (current)
 *
 * Wire Tags:
 *   - field 1: command_number (tag 0x08, wire 0)
 *   - field 2: client_tick (tag 0x10, wire 0)
 *   - field 3: buttons_pb (tag 0x1A, wire 2)
 *   - field 5: forwardmove (tag 0x2D, wire 5)
 *   - field 6: leftmove (tag 0x35, wire 5)
 *   - field 7: upmove (tag 0x3D, wire 5)
 *   - field 18: repeated subtick_moves (tag 0x92 0x01, wire 2)
 *   - field 19: move_crc (tag 0x9A 0x01, wire 2)
 *   - field 20: consumed_server_angle_changes (tag 0xA0 0x01, wire 0)
 *   - field 21: cmd_flags (tag 0xA8 0x01, wire 0)
 */
class CBaseUserCmdPB {
public:
    uint32_t m_nHasBits{0};
    int32_t m_nCommandNumber{0};
    int32_t m_nClientTick{0};
    CInButtonStatePB m_ButtonsPB;
    float m_flForwardMove{0.0f};
    float m_flLeftMove{0.0f};
    float m_flUpMove{0.0f};
    QAngle m_ViewAngles{0.0f, 0.0f, 0.0f};
    std::vector<CSubtickMoveStep> m_SubtickMoves; // Field 18 (tag 0x92 0x01)
    std::string m_strMoveCRC;
    int32_t m_nConsumedServerAngleChanges{0};
    int32_t m_nCmdFlags{0};

    enum HasBits : uint32_t {
        HAS_COMMAND_NUMBER                 = (1 << 4),  // 0x10
        HAS_CLIENT_TICK                    = (1 << 5),  // 0x20
        HAS_BUTTONS_PB                     = (1 << 1),  // 0x02
        HAS_FORWARDMOVE                    = (1 << 6),  // 0x40
        HAS_LEFTMOVE                       = (1 << 7),  // 0x80
        HAS_UPMOVE                         = (1 << 8),  // 0x100
        HAS_VIEWANGLES                     = (1 << 9),  // 0x200
        HAS_MOVE_CRC                       = (1 << 0),  // 0x01
        HAS_CONSUMED_SERVER_ANGLE_CHANGES  = (1 << 15), // 0x8000
        HAS_CMD_FLAGS                      = (1 << 16), // 0x10000
    };

    CBaseUserCmdPB() = default;

    [[nodiscard]] size_t ByteSizeLong() const;
    void _InternalSerialize(CByteStreamWriter& writer) const;
    bool _InternalParse(CByteStreamReader& reader);
};

/**
 * @brief CSGOUserCmdPB wrapper framing Base UserCmd and Input History shots.
 * Reconstructed from client.dll:
 *   - VTable: 0x181bccdc0 (baseline) / 0x181bced50 (current)
 *   - Parse: FUN_1807b0740 (baseline) / FUN_1807b1830 (current)
 *   - Serialize: FUN_1807b0be0 (baseline) / FUN_1807b1cd0 (current)
 */
class CSGOUserCmdPB {
public:
    uint32_t m_nHasBits{0};
    CBaseUserCmdPB m_Base;
    std::vector<CSGOInputHistoryEntryPB> m_InputHistory;
    int32_t m_nAttack1StartHistoryIndex{-1};
    int32_t m_nAttack2StartHistoryIndex{-1};
    int32_t m_nAttack3StartHistoryIndex{-1};
    bool m_bHasBeenPredicted{false};

    enum HasBits : uint32_t {
        HAS_BASE                           = (1 << 0), // 0x01
        HAS_ATTACK1_START_HISTORY_INDEX    = (1 << 5), // 0x20
        HAS_ATTACK2_START_HISTORY_INDEX    = (1 << 6), // 0x40
        HAS_HAS_BEEN_PREDICTED             = (1 << 1), // 0x02
        HAS_ATTACK3_START_HISTORY_INDEX    = (1 << 2), // 0x04
    };

    CSGOUserCmdPB() = default;

    [[nodiscard]] size_t ByteSizeLong() const;
    void _InternalSerialize(CByteStreamWriter& writer) const;
    bool _InternalParse(CByteStreamReader& reader);
};

/**
 * @brief CCLCMsg_Move network packet encapsulation (clc_Move, NetMessage 0x15).
 * Reconstructed from networksystem.dll and client.dll:
 *   - NetMessage ID: 0x15 (21)
 *   - Wire format:
 *       field 1: num_backup_commands (uint32, tag 0x08)
 *       field 2: num_new_commands (uint32, tag 0x10)
 *       field 3: data (bytes, tag 0x1A) - length-delimited stream of CSGOUserCmdPB
 */
class CCLCMsg_Move {
public:
    static constexpr uint32_t NET_MESSAGE_ID = 0x15; // 21

    uint32_t m_nNumBackupCommands{0};
    uint32_t m_nNumNewCommands{1};
    std::vector<uint8_t> m_vecData;

    [[nodiscard]] size_t ByteSizeLong() const;
    void _InternalSerialize(CByteStreamWriter& writer) const;
    bool _InternalParse(CByteStreamReader& reader);
};

/**
 * @brief Reconstructed CSGOInput client-side user command coordinator.
 * Reconstructed from client.dll (FUN_180cfa6b0 / CSGOInput::CreateMove).
 */
class CSGOInputWireCodec {
public:
    static constexpr float DEFAULT_TICK_INTERVAL = 1.0f / 64.0f; // 0.015625s

    /**
     * @brief Computes normalized sub-tick fraction w in [0.0, 1.0] and instantiates CSubtickMoveStep.
     */
    static CSubtickMoveStep CreateSubtickMoveStep(
        uint64_t button,
        bool pressed,
        double eventTimestampSec,
        double tickStartTimestampSec,
        float tickInterval = DEFAULT_TICK_INTERVAL,
        float analogForwardDelta = 0.0f,
        float analogLeftDelta = 0.0f
    );

    /**
     * @brief Encodes variable-bit NetMessage ID (6, 10, 14, or 34 bits) as used by networksystem.dll.
     */
    static void WriteVariableBitMessageId(std::vector<uint8_t>& bitstream, size_t& bitOffset, uint32_t messageId);

    /**
     * @brief Decodes variable-bit NetMessage ID header from bitstream.
     */
    static uint32_t ReadVariableBitMessageId(const uint8_t* bitstream, size_t& bitOffset, size_t maxBits);

    /**
     * @brief Serializes a CSGOUserCmdPB into a clc_Move wire payload.
     */
    static std::vector<uint8_t> SerializeUserCmdToBitbuf(
        const CSGOUserCmdPB& userCmd,
        uint32_t numBackupCommands = 0,
        uint32_t numNewCommands = 1
    );

    /**
     * @brief Unpacks a clc_Move netmessage payload back into CSGOUserCmdPB commands.
     */
    static bool UnpackUserCmdFromBitbuf(
        const uint8_t* payloadData,
        size_t payloadSize,
        std::vector<CSGOUserCmdPB>& outCmds,
        uint32_t& outNumBackup,
        uint32_t& outNumNew
    );
};

} // namespace cs2::client

#endif // CS2_CLIENT_USERCMD_WIRE_H
