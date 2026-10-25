#include "usercmd_wire.h"
#include <cstring>
#include <cmath>
#include <stdexcept>

namespace cs2::client {

// ============================================================================
// CByteStreamReader Implementation
// ============================================================================

bool CByteStreamReader::ReadByte(uint8_t& outByte) {
    if (m_nOffset >= m_nSize) return false;
    outByte = m_pData[m_nOffset++];
    return true;
}

bool CByteStreamReader::ReadVarInt32(uint32_t& outVal) {
    uint64_t val64 = 0;
    if (!ReadVarInt64(val64)) return false;
    outVal = static_cast<uint32_t>(val64);
    return true;
}

bool CByteStreamReader::ReadVarInt64(uint64_t& outVal) {
    outVal = 0;
    int shift = 0;
    while (m_nOffset < m_nSize && shift < 64) {
        uint8_t b = m_pData[m_nOffset++];
        outVal |= static_cast<uint64_t>(b & 0x7F) << shift;
        if ((b & 0x80) == 0) {
            return true;
        }
        shift += 7;
    }
    return false;
}

bool CByteStreamReader::ReadFixed32(uint32_t& outVal) {
    if (m_nOffset + sizeof(uint32_t) > m_nSize) return false;
    std::memcpy(&outVal, m_pData + m_nOffset, sizeof(uint32_t));
    m_nOffset += sizeof(uint32_t);
    return true;
}

bool CByteStreamReader::ReadFloat(float& outVal) {
    static_assert(sizeof(float) == sizeof(uint32_t), "Float size mismatch");
    uint32_t bits = 0;
    if (!ReadFixed32(bits)) return false;
    std::memcpy(&outVal, &bits, sizeof(float));
    return true;
}

bool CByteStreamReader::ReadFixed64(uint64_t& outVal) {
    if (m_nOffset + sizeof(uint64_t) > m_nSize) return false;
    std::memcpy(&outVal, m_pData + m_nOffset, sizeof(uint64_t));
    m_nOffset += sizeof(uint64_t);
    return true;
}

bool CByteStreamReader::ReadDouble(double& outVal) {
    static_assert(sizeof(double) == sizeof(uint64_t), "Double size mismatch");
    uint64_t bits = 0;
    if (!ReadFixed64(bits)) return false;
    std::memcpy(&outVal, &bits, sizeof(double));
    return true;
}

bool CByteStreamReader::ReadBytes(uint8_t* outDest, size_t len) {
    if (m_nOffset + len > m_nSize) return false;
    std::memcpy(outDest, m_pData + m_nOffset, len);
    m_nOffset += len;
    return true;
}

bool CByteStreamReader::ReadString(std::string& outStr, size_t len) {
    if (m_nOffset + len > m_nSize) return false;
    outStr.assign(reinterpret_cast<const char*>(m_pData + m_nOffset), len);
    m_nOffset += len;
    return true;
}

bool CByteStreamReader::Skip(size_t len) {
    if (m_nOffset + len > m_nSize) return false;
    m_nOffset += len;
    return true;
}

bool CByteStreamReader::SkipField(WireType wireType) {
    switch (wireType) {
        case WireType::VarInt: {
            uint64_t dummy = 0;
            return ReadVarInt64(dummy);
        }
        case WireType::Fixed64:
            return Skip(8);
        case WireType::LengthDelimited: {
            uint32_t len = 0;
            if (!ReadVarInt32(len)) return false;
            return Skip(len);
        }
        case WireType::Fixed32:
            return Skip(4);
        default:
            return false;
    }
}

// ============================================================================
// CByteStreamWriter Implementation
// ============================================================================

void CByteStreamWriter::WriteVarInt32(uint32_t val) {
    while (val >= 0x80) {
        m_Buffer.push_back(static_cast<uint8_t>((val & 0x7F) | 0x80));
        val >>= 7;
    }
    m_Buffer.push_back(static_cast<uint8_t>(val & 0x7F));
}

void CByteStreamWriter::WriteVarInt64(uint64_t val) {
    while (val >= 0x80) {
        m_Buffer.push_back(static_cast<uint8_t>((val & 0x7F) | 0x80));
        val >>= 7;
    }
    m_Buffer.push_back(static_cast<uint8_t>(val & 0x7F));
}

void CByteStreamWriter::WriteFixed32(uint32_t val) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    m_Buffer.insert(m_Buffer.end(), p, p + 4);
}

void CByteStreamWriter::WriteFloat(float val) {
    uint32_t bits = 0;
    std::memcpy(&bits, &val, sizeof(float));
    WriteFixed32(bits);
}

void CByteStreamWriter::WriteFixed64(uint64_t val) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    m_Buffer.insert(m_Buffer.end(), p, p + 8);
}

void CByteStreamWriter::WriteDouble(double val) {
    uint64_t bits = 0;
    std::memcpy(&bits, &val, sizeof(double));
    WriteFixed64(bits);
}

void CByteStreamWriter::WriteTag(uint32_t fieldNumber, WireType wireType) {
    uint32_t key = (fieldNumber << 3) | static_cast<uint32_t>(wireType);
    WriteVarInt32(key);
}

void CByteStreamWriter::WriteLengthDelimited(const uint8_t* data, size_t len) {
    WriteVarInt32(static_cast<uint32_t>(len));
    if (len > 0 && data != nullptr) {
        m_Buffer.insert(m_Buffer.end(), data, data + len);
    }
}

void CByteStreamWriter::WriteString(const std::string& str) {
    WriteLengthDelimited(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

void CByteStreamWriter::WriteBytes(const uint8_t* data, size_t len) {
    WriteLengthDelimited(data, len);
}

// ============================================================================
// CSubtickMoveStep Serialization & Parsing
// Reconstructed from FUN_1804f4c40 (_InternalSerialize) and FUN_1804f4950 (_InternalParse)
// ============================================================================

size_t CSubtickMoveStep::ByteSizeLong() const {
    size_t total = 0;

    // Field 1: button (tag 1, wire 0, 1 byte tag)
    if (m_nHasBits & HAS_BUTTON) {
        total += 1;
        uint64_t v = m_nButton;
        do {
            total++;
            v >>= 7;
        } while (v > 0);
    }

    // Field 2: pressed (tag 2, wire 0, tag 0x10 = 1 byte + 1 byte value)
    if (m_nHasBits & HAS_PRESSED) {
        total += 2;
    }

    // Field 3: when (tag 3, wire 5, tag 0x1D = 1 byte + 4 bytes float)
    if (m_nHasBits & HAS_WHEN) {
        total += 5;
    }

    // Field 4: analog_forward_delta (tag 4, wire 5, tag 0x25 = 1 byte + 4 bytes float)
    if (m_nHasBits & HAS_ANALOG_FORWARD_DELTA) {
        total += 5;
    }

    // Field 5: analog_left_delta (tag 5, wire 5, tag 0x2D = 1 byte + 4 bytes float)
    if (m_nHasBits & HAS_ANALOG_LEFT_DELTA) {
        total += 5;
    }

    // Field 8: analog_pitch_delta (tag 8, wire 5, tag 0x45 = 1 byte + 4 bytes float)
    if (m_nHasBits & HAS_ANALOG_PITCH_DELTA) {
        total += 5;
    }

    // Field 9: analog_yaw_delta (tag 9, wire 5, tag 0x4D = 1 byte + 4 bytes float)
    if (m_nHasBits & HAS_ANALOG_YAW_DELTA) {
        total += 5;
    }

    m_nCachedSize = static_cast<int32_t>(total);
    return total;
}

void CSubtickMoveStep::_InternalSerialize(CByteStreamWriter& writer) const {
    // 1. Field 1: uint64 button (Tag: (1 << 3) | 0 = 0x08)
    if (m_nHasBits & HAS_BUTTON) {
        writer.WriteTag(1, WireType::VarInt);
        writer.WriteVarInt64(m_nButton);
    }

    // 2. Field 2: bool pressed (Tag: (2 << 3) | 0 = 0x10)
    if (m_nHasBits & HAS_PRESSED) {
        writer.WriteTag(2, WireType::VarInt);
        writer.WriteByte(m_bPressed ? 1 : 0);
    }

    // 3. Field 3: float when (Tag: (3 << 3) | 5 = 0x1D)
    if (m_nHasBits & HAS_WHEN) {
        writer.WriteTag(3, WireType::Fixed32);
        writer.WriteFloat(m_flWhen);
    }

    // 4. Field 4: float analog_forward_delta (Tag: (4 << 3) | 5 = 0x25)
    if (m_nHasBits & HAS_ANALOG_FORWARD_DELTA) {
        writer.WriteTag(4, WireType::Fixed32);
        writer.WriteFloat(m_flAnalogForwardDelta);
    }

    // 5. Field 5: float analog_left_delta (Tag: (5 << 3) | 5 = 0x2D)
    if (m_nHasBits & HAS_ANALOG_LEFT_DELTA) {
        writer.WriteTag(5, WireType::Fixed32);
        writer.WriteFloat(m_flAnalogLeftDelta);
    }

    // 6. Field 8: float analog_pitch_delta (Tag: (8 << 3) | 5 = 0x45)
    if (m_nHasBits & HAS_ANALOG_PITCH_DELTA) {
        writer.WriteTag(8, WireType::Fixed32);
        writer.WriteFloat(m_flAnalogPitchDelta);
    }

    // 7. Field 9: float analog_yaw_delta (Tag: (9 << 3) | 5 = 0x4D)
    if (m_nHasBits & HAS_ANALOG_YAW_DELTA) {
        writer.WriteTag(9, WireType::Fixed32);
        writer.WriteFloat(m_flAnalogYawDelta);
    }
}

bool CSubtickMoveStep::_InternalParse(CByteStreamReader& reader) {
    while (reader.HasMore()) {
        uint32_t tag = 0;
        if (!reader.ReadVarInt32(tag)) return false;

        uint32_t fieldNumber = tag >> 3;
        WireType wireType = static_cast<WireType>(tag & 7);

        switch (fieldNumber) {
            case 1: { // button (uint64)
                if (wireType != WireType::VarInt) return false;
                if (!reader.ReadVarInt64(m_nButton)) return false;
                m_nHasBits |= HAS_BUTTON;
                break;
            }
            case 2: { // pressed (bool)
                if (wireType != WireType::VarInt) return false;
                uint32_t val = 0;
                if (!reader.ReadVarInt32(val)) return false;
                m_bPressed = (val != 0);
                m_nHasBits |= HAS_PRESSED;
                break;
            }
            case 3: { // when (float)
                if (wireType != WireType::Fixed32) return false;
                if (!reader.ReadFloat(m_flWhen)) return false;
                m_flWhen = std::clamp(m_flWhen, 0.0f, 1.0f);
                m_nHasBits |= HAS_WHEN;
                break;
            }
            case 4: { // analog_forward_delta (float)
                if (wireType != WireType::Fixed32) return false;
                if (!reader.ReadFloat(m_flAnalogForwardDelta)) return false;
                m_nHasBits |= HAS_ANALOG_FORWARD_DELTA;
                break;
            }
            case 5: { // analog_left_delta (float)
                if (wireType != WireType::Fixed32) return false;
                if (!reader.ReadFloat(m_flAnalogLeftDelta)) return false;
                m_nHasBits |= HAS_ANALOG_LEFT_DELTA;
                break;
            }
            case 8: { // analog_pitch_delta (float)
                if (wireType != WireType::Fixed32) return false;
                if (!reader.ReadFloat(m_flAnalogPitchDelta)) return false;
                m_nHasBits |= HAS_ANALOG_PITCH_DELTA;
                break;
            }
            case 9: { // analog_yaw_delta (float)
                if (wireType != WireType::Fixed32) return false;
                if (!reader.ReadFloat(m_flAnalogYawDelta)) return false;
                m_nHasBits |= HAS_ANALOG_YAW_DELTA;
                break;
            }
            default:
                if (!reader.SkipField(wireType)) return false;
                break;
        }
    }
    return true;
}

// ============================================================================
// CInButtonStatePB Implementation
// ============================================================================

size_t CInButtonStatePB::ByteSizeLong() const {
    size_t total = 0;
    if (buttonstate1 != 0) {
        total += 1;
        uint64_t v = buttonstate1;
        do { total++; v >>= 7; } while (v > 0);
    }
    if (buttonstate2 != 0) {
        total += 1;
        uint64_t v = buttonstate2;
        do { total++; v >>= 7; } while (v > 0);
    }
    if (buttonstate3 != 0) {
        total += 1;
        uint64_t v = buttonstate3;
        do { total++; v >>= 7; } while (v > 0);
    }
    return total;
}

void CInButtonStatePB::_InternalSerialize(CByteStreamWriter& writer) const {
    if (buttonstate1 != 0) {
        writer.WriteTag(1, WireType::VarInt);
        writer.WriteVarInt64(buttonstate1);
    }
    if (buttonstate2 != 0) {
        writer.WriteTag(2, WireType::VarInt);
        writer.WriteVarInt64(buttonstate2);
    }
    if (buttonstate3 != 0) {
        writer.WriteTag(3, WireType::VarInt);
        writer.WriteVarInt64(buttonstate3);
    }
}

bool CInButtonStatePB::_InternalParse(CByteStreamReader& reader) {
    while (reader.HasMore()) {
        uint32_t tag = 0;
        if (!reader.ReadVarInt32(tag)) return false;
        uint32_t fieldNumber = tag >> 3;
        WireType wireType = static_cast<WireType>(tag & 7);

        switch (fieldNumber) {
            case 1:
                if (wireType != WireType::VarInt || !reader.ReadVarInt64(buttonstate1)) return false;
                break;
            case 2:
                if (wireType != WireType::VarInt || !reader.ReadVarInt64(buttonstate2)) return false;
                break;
            case 3:
                if (wireType != WireType::VarInt || !reader.ReadVarInt64(buttonstate3)) return false;
                break;
            default:
                if (!reader.SkipField(wireType)) return false;
                break;
        }
    }
    return true;
}

// ============================================================================
// CSGOInputHistoryEntryPB Implementation
// ============================================================================

size_t CSGOInputHistoryEntryPB::ByteSizeLong() const {
    CByteStreamWriter temp;
    _InternalSerialize(temp);
    return temp.GetSize();
}

void CSGOInputHistoryEntryPB::_InternalSerialize(CByteStreamWriter& writer) const {
    // Field 1: view_angles (embedded QAngle)
    {
        CByteStreamWriter angleWriter;
        angleWriter.WriteTag(1, WireType::Fixed32);
        angleWriter.WriteFloat(view_angles.pitch);
        angleWriter.WriteTag(2, WireType::Fixed32);
        angleWriter.WriteFloat(view_angles.yaw);
        angleWriter.WriteTag(3, WireType::Fixed32);
        angleWriter.WriteFloat(view_angles.roll);

        writer.WriteTag(1, WireType::LengthDelimited);
        writer.WriteLengthDelimited(angleWriter.GetData().data(), angleWriter.GetSize());
    }

    // Field 2: shoot_position (Vector3)
    {
        CByteStreamWriter posWriter;
        posWriter.WriteTag(1, WireType::Fixed32);
        posWriter.WriteFloat(shoot_position.x);
        posWriter.WriteTag(2, WireType::Fixed32);
        posWriter.WriteFloat(shoot_position.y);
        posWriter.WriteTag(3, WireType::Fixed32);
        posWriter.WriteFloat(shoot_position.z);

        writer.WriteTag(2, WireType::LengthDelimited);
        writer.WriteLengthDelimited(posWriter.GetData().data(), posWriter.GetSize());
    }

    // Field 3: target_ent_index
    if (target_ent_index != -1) {
        writer.WriteTag(3, WireType::VarInt);
        writer.WriteVarInt32(static_cast<uint32_t>(target_ent_index));
    }

    // Field 4: player_tick_count
    writer.WriteTag(4, WireType::VarInt);
    writer.WriteVarInt32(static_cast<uint32_t>(player_tick_count));

    // Field 5: player_tick_fraction
    writer.WriteTag(5, WireType::Fixed32);
    writer.WriteFloat(player_tick_fraction);
}

bool CSGOInputHistoryEntryPB::_InternalParse(CByteStreamReader& reader) {
    while (reader.HasMore()) {
        uint32_t tag = 0;
        if (!reader.ReadVarInt32(tag)) return false;
        uint32_t fieldNumber = tag >> 3;
        WireType wireType = static_cast<WireType>(tag & 7);

        switch (fieldNumber) {
            case 1: { // view_angles
                if (wireType != WireType::LengthDelimited) return false;
                uint32_t len = 0;
                if (!reader.ReadVarInt32(len)) return false;
                CByteStreamReader subReader(reader.CurrentPtr(), len);
                reader.Skip(len);
                while (subReader.HasMore()) {
                    uint32_t subTag = 0;
                    if (!subReader.ReadVarInt32(subTag)) return false;
                    uint32_t sf = subTag >> 3;
                    if (sf == 1) subReader.ReadFloat(view_angles.pitch);
                    else if (sf == 2) subReader.ReadFloat(view_angles.yaw);
                    else if (sf == 3) subReader.ReadFloat(view_angles.roll);
                    else subReader.SkipField(static_cast<WireType>(subTag & 7));
                }
                break;
            }
            case 2: { // shoot_position
                if (wireType != WireType::LengthDelimited) return false;
                uint32_t len = 0;
                if (!reader.ReadVarInt32(len)) return false;
                CByteStreamReader subReader(reader.CurrentPtr(), len);
                reader.Skip(len);
                while (subReader.HasMore()) {
                    uint32_t subTag = 0;
                    if (!subReader.ReadVarInt32(subTag)) return false;
                    uint32_t sf = subTag >> 3;
                    if (sf == 1) subReader.ReadFloat(shoot_position.x);
                    else if (sf == 2) subReader.ReadFloat(shoot_position.y);
                    else if (sf == 3) subReader.ReadFloat(shoot_position.z);
                    else subReader.SkipField(static_cast<WireType>(subTag & 7));
                }
                break;
            }
            case 3: { // target_ent_index
                uint32_t val = 0;
                if (!reader.ReadVarInt32(val)) return false;
                target_ent_index = static_cast<int32_t>(val);
                break;
            }
            case 4: { // player_tick_count
                uint32_t val = 0;
                if (!reader.ReadVarInt32(val)) return false;
                player_tick_count = static_cast<int32_t>(val);
                break;
            }
            case 5: { // player_tick_fraction
                if (!reader.ReadFloat(player_tick_fraction)) return false;
                break;
            }
            default:
                if (!reader.SkipField(wireType)) return false;
                break;
        }
    }
    return true;
}

// ============================================================================
// CBaseUserCmdPB Serialization & Parsing
// Reconstructed from FUN_1804f63f0 (_InternalSerialize) and FUN_1804f5c00 (_InternalParse)
// ============================================================================

size_t CBaseUserCmdPB::ByteSizeLong() const {
    CByteStreamWriter temp;
    _InternalSerialize(temp);
    return temp.GetSize();
}

void CBaseUserCmdPB::_InternalSerialize(CByteStreamWriter& writer) const {
    // 1. Field 1: command_number (tag 0x08, wire 0)
    if (m_nHasBits & HAS_COMMAND_NUMBER) {
        writer.WriteTag(1, WireType::VarInt);
        writer.WriteVarInt32(static_cast<uint32_t>(m_nCommandNumber));
    }

    // 2. Field 2: client_tick (tag 0x10, wire 0)
    if (m_nHasBits & HAS_CLIENT_TICK) {
        writer.WriteTag(2, WireType::VarInt);
        writer.WriteVarInt32(static_cast<uint32_t>(m_nClientTick));
    }

    // 3. Field 3: buttons_pb (tag 0x1A, wire 2)
    if (m_nHasBits & HAS_BUTTONS_PB) {
        CByteStreamWriter btnWriter;
        m_ButtonsPB._InternalSerialize(btnWriter);
        writer.WriteTag(3, WireType::LengthDelimited);
        writer.WriteLengthDelimited(btnWriter.GetData().data(), btnWriter.GetSize());
    }

    // 4. Field 5: forwardmove (tag 0x2D, wire 5)
    if (m_nHasBits & HAS_FORWARDMOVE) {
        writer.WriteTag(5, WireType::Fixed32);
        writer.WriteFloat(m_flForwardMove);
    }

    // 5. Field 6: leftmove (tag 0x35, wire 5)
    if (m_nHasBits & HAS_LEFTMOVE) {
        writer.WriteTag(6, WireType::Fixed32);
        writer.WriteFloat(m_flLeftMove);
    }

    // 6. Field 7: upmove (tag 0x3D, wire 5)
    if (m_nHasBits & HAS_UPMOVE) {
        writer.WriteTag(7, WireType::Fixed32);
        writer.WriteFloat(m_flUpMove);
    }

    // 7. Field 18: repeated CSubtickMoveStep subtick_moves
    // Exact binary tag: (18 << 3) | 2 = 146 = 0x92 0x01
    for (const auto& step : m_SubtickMoves) {
        CByteStreamWriter stepWriter;
        step._InternalSerialize(stepWriter);
        // Write tag 18 length-delimited (0x92 0x01)
        writer.WriteTag(18, WireType::LengthDelimited);
        writer.WriteLengthDelimited(stepWriter.GetData().data(), stepWriter.GetSize());
    }

    // 8. Field 19: move_crc (tag 0x9A 0x01, wire 2)
    if ((m_nHasBits & HAS_MOVE_CRC) && !m_strMoveCRC.empty()) {
        writer.WriteTag(19, WireType::LengthDelimited);
        writer.WriteString(m_strMoveCRC);
    }

    // 9. Field 20: consumed_server_angle_changes (tag 0xA0 0x01, wire 0)
    if (m_nHasBits & HAS_CONSUMED_SERVER_ANGLE_CHANGES) {
        writer.WriteTag(20, WireType::VarInt);
        writer.WriteVarInt32(static_cast<uint32_t>(m_nConsumedServerAngleChanges));
    }

    // 10. Field 21: cmd_flags (tag 0xA8 0x01, wire 0)
    if (m_nHasBits & HAS_CMD_FLAGS) {
        writer.WriteTag(21, WireType::VarInt);
        writer.WriteVarInt32(static_cast<uint32_t>(m_nCmdFlags));
    }
}

bool CBaseUserCmdPB::_InternalParse(CByteStreamReader& reader) {
    while (reader.HasMore()) {
        uint32_t tag = 0;
        if (!reader.ReadVarInt32(tag)) return false;

        uint32_t fieldNumber = tag >> 3;
        WireType wireType = static_cast<WireType>(tag & 7);

        switch (fieldNumber) {
            case 1: { // command_number
                uint32_t val = 0;
                if (!reader.ReadVarInt32(val)) return false;
                m_nCommandNumber = static_cast<int32_t>(val);
                m_nHasBits |= HAS_COMMAND_NUMBER;
                break;
            }
            case 2: { // client_tick
                uint32_t val = 0;
                if (!reader.ReadVarInt32(val)) return false;
                m_nClientTick = static_cast<int32_t>(val);
                m_nHasBits |= HAS_CLIENT_TICK;
                break;
            }
            case 3: { // buttons_pb
                uint32_t len = 0;
                if (!reader.ReadVarInt32(len)) return false;
                CByteStreamReader subReader(reader.CurrentPtr(), len);
                reader.Skip(len);
                if (!m_ButtonsPB._InternalParse(subReader)) return false;
                m_nHasBits |= HAS_BUTTONS_PB;
                break;
            }
            case 5: { // forwardmove
                if (!reader.ReadFloat(m_flForwardMove)) return false;
                m_nHasBits |= HAS_FORWARDMOVE;
                break;
            }
            case 6: { // leftmove
                if (!reader.ReadFloat(m_flLeftMove)) return false;
                m_nHasBits |= HAS_LEFTMOVE;
                break;
            }
            case 7: { // upmove
                if (!reader.ReadFloat(m_flUpMove)) return false;
                m_nHasBits |= HAS_UPMOVE;
                break;
            }
            case 18: { // repeated subtick_moves (tag 0x92 0x01)
                uint32_t len = 0;
                if (!reader.ReadVarInt32(len)) return false;
                CByteStreamReader subReader(reader.CurrentPtr(), len);
                reader.Skip(len);

                CSubtickMoveStep step;
                if (!step._InternalParse(subReader)) return false;
                m_SubtickMoves.push_back(step);
                break;
            }
            case 19: { // move_crc
                uint32_t len = 0;
                if (!reader.ReadVarInt32(len)) return false;
                if (!reader.ReadString(m_strMoveCRC, len)) return false;
                m_nHasBits |= HAS_MOVE_CRC;
                break;
            }
            case 20: { // consumed_server_angle_changes
                uint32_t val = 0;
                if (!reader.ReadVarInt32(val)) return false;
                m_nConsumedServerAngleChanges = static_cast<int32_t>(val);
                m_nHasBits |= HAS_CONSUMED_SERVER_ANGLE_CHANGES;
                break;
            }
            case 21: { // cmd_flags
                uint32_t val = 0;
                if (!reader.ReadVarInt32(val)) return false;
                m_nCmdFlags = static_cast<int32_t>(val);
                m_nHasBits |= HAS_CMD_FLAGS;
                break;
            }
            default:
                if (!reader.SkipField(wireType)) return false;
                break;
        }
    }
    return true;
}

// ============================================================================
// CSGOUserCmdPB Serialization & Parsing
// Reconstructed from FUN_1807b0be0 (_InternalSerialize) and FUN_1807b0740 (_InternalParse)
// ============================================================================

size_t CSGOUserCmdPB::ByteSizeLong() const {
    CByteStreamWriter temp;
    _InternalSerialize(temp);
    return temp.GetSize();
}

void CSGOUserCmdPB::_InternalSerialize(CByteStreamWriter& writer) const {
    // 1. Field 1: CBaseUserCmdPB base (tag 0x0A, wire 2)
    {
        CByteStreamWriter baseWriter;
        m_Base._InternalSerialize(baseWriter);
        writer.WriteTag(1, WireType::LengthDelimited);
        writer.WriteLengthDelimited(baseWriter.GetData().data(), baseWriter.GetSize());
    }

    // 2. Field 2: repeated CSGOInputHistoryEntryPB input_history (tag 0x12, wire 2)
    for (const auto& entry : m_InputHistory) {
        CByteStreamWriter histWriter;
        entry._InternalSerialize(histWriter);
        writer.WriteTag(2, WireType::LengthDelimited);
        writer.WriteLengthDelimited(histWriter.GetData().data(), histWriter.GetSize());
    }

    // 3. Field 6: attack1_start_history_index (tag 0x30, wire 0)
    if (m_nAttack1StartHistoryIndex != -1) {
        writer.WriteTag(6, WireType::VarInt);
        writer.WriteVarInt32(static_cast<uint32_t>(m_nAttack1StartHistoryIndex));
    }

    // 4. Field 7: attack2_start_history_index (tag 0x38, wire 0)
    if (m_nAttack2StartHistoryIndex != -1) {
        writer.WriteTag(7, WireType::VarInt);
        writer.WriteVarInt32(static_cast<uint32_t>(m_nAttack2StartHistoryIndex));
    }

    // 5. Field 9: has_been_predicted (tag 0x48, wire 0)
    if (m_bHasBeenPredicted) {
        writer.WriteTag(9, WireType::VarInt);
        writer.WriteByte(1);
    }

    // 6. Field 11: attack3_start_history_index (tag 0x58, wire 0)
    if (m_nAttack3StartHistoryIndex != -1) {
        writer.WriteTag(11, WireType::VarInt);
        writer.WriteVarInt32(static_cast<uint32_t>(m_nAttack3StartHistoryIndex));
    }
}

bool CSGOUserCmdPB::_InternalParse(CByteStreamReader& reader) {
    while (reader.HasMore()) {
        uint32_t tag = 0;
        if (!reader.ReadVarInt32(tag)) return false;

        uint32_t fieldNumber = tag >> 3;
        WireType wireType = static_cast<WireType>(tag & 7);

        switch (fieldNumber) {
            case 1: { // base
                uint32_t len = 0;
                if (!reader.ReadVarInt32(len)) return false;
                CByteStreamReader subReader(reader.CurrentPtr(), len);
                reader.Skip(len);
                if (!m_Base._InternalParse(subReader)) return false;
                m_nHasBits |= HAS_BASE;
                break;
            }
            case 2: { // input_history
                uint32_t len = 0;
                if (!reader.ReadVarInt32(len)) return false;
                CByteStreamReader subReader(reader.CurrentPtr(), len);
                reader.Skip(len);
                CSGOInputHistoryEntryPB entry;
                if (!entry._InternalParse(subReader)) return false;
                m_InputHistory.push_back(entry);
                break;
            }
            case 6: { // attack1_start_history_index
                uint32_t val = 0;
                if (!reader.ReadVarInt32(val)) return false;
                m_nAttack1StartHistoryIndex = static_cast<int32_t>(val);
                m_nHasBits |= HAS_ATTACK1_START_HISTORY_INDEX;
                break;
            }
            case 7: { // attack2_start_history_index
                uint32_t val = 0;
                if (!reader.ReadVarInt32(val)) return false;
                m_nAttack2StartHistoryIndex = static_cast<int32_t>(val);
                m_nHasBits |= HAS_ATTACK2_START_HISTORY_INDEX;
                break;
            }
            case 9: { // has_been_predicted
                uint32_t val = 0;
                if (!reader.ReadVarInt32(val)) return false;
                m_bHasBeenPredicted = (val != 0);
                m_nHasBits |= HAS_HAS_BEEN_PREDICTED;
                break;
            }
            case 11: { // attack3_start_history_index
                uint32_t val = 0;
                if (!reader.ReadVarInt32(val)) return false;
                m_nAttack3StartHistoryIndex = static_cast<int32_t>(val);
                m_nHasBits |= HAS_ATTACK3_START_HISTORY_INDEX;
                break;
            }
            default:
                if (!reader.SkipField(wireType)) return false;
                break;
        }
    }
    return true;
}

// ============================================================================
// CCLCMsg_Move Serialization & Parsing
// ============================================================================

size_t CCLCMsg_Move::ByteSizeLong() const {
    CByteStreamWriter temp;
    _InternalSerialize(temp);
    return temp.GetSize();
}

void CCLCMsg_Move::_InternalSerialize(CByteStreamWriter& writer) const {
    // Field 1: num_backup_commands (tag 0x08, wire 0)
    if (m_nNumBackupCommands > 0) {
        writer.WriteTag(1, WireType::VarInt);
        writer.WriteVarInt32(m_nNumBackupCommands);
    }

    // Field 2: num_new_commands (tag 0x10, wire 0)
    writer.WriteTag(2, WireType::VarInt);
    writer.WriteVarInt32(m_nNumNewCommands);

    // Field 3: data (bytes, tag 0x1A, wire 2)
    if (!m_vecData.empty()) {
        writer.WriteTag(3, WireType::LengthDelimited);
        writer.WriteBytes(m_vecData.data(), m_vecData.size());
    }
}

bool CCLCMsg_Move::_InternalParse(CByteStreamReader& reader) {
    while (reader.HasMore()) {
        uint32_t tag = 0;
        if (!reader.ReadVarInt32(tag)) return false;

        uint32_t fieldNumber = tag >> 3;
        WireType wireType = static_cast<WireType>(tag & 7);

        switch (fieldNumber) {
            case 1:
                if (!reader.ReadVarInt32(m_nNumBackupCommands)) return false;
                break;
            case 2:
                if (!reader.ReadVarInt32(m_nNumNewCommands)) return false;
                break;
            case 3: {
                uint32_t len = 0;
                if (!reader.ReadVarInt32(len)) return false;
                m_vecData.resize(len);
                if (!reader.ReadBytes(m_vecData.data(), len)) return false;
                break;
            }
            default:
                if (!reader.SkipField(wireType)) return false;
                break;
        }
    }
    return true;
}

// ============================================================================
// CSGOInputWireCodec Implementation
// ============================================================================

CSubtickMoveStep CSGOInputWireCodec::CreateSubtickMoveStep(
    uint64_t button,
    bool pressed,
    double eventTimestampSec,
    double tickStartTimestampSec,
    float tickInterval,
    float analogForwardDelta,
    float analogLeftDelta
) {
    CSubtickMoveStep step;
    step.SetButton(button);
    step.SetPressed(pressed);

    // Continuous temporal slicing: when = (T_event - T_tick_start) / dt
    double elapsed = eventTimestampSec - tickStartTimestampSec;
    float whenFraction = static_cast<float>(elapsed / static_cast<double>(tickInterval));
    step.SetWhen(whenFraction);

    if (std::abs(analogForwardDelta) > 1e-5f) {
        step.SetAnalogForwardDelta(analogForwardDelta);
    }
    if (std::abs(analogLeftDelta) > 1e-5f) {
        step.SetAnalogLeftDelta(analogLeftDelta);
    }

    return step;
}

void CSGOInputWireCodec::WriteVariableBitMessageId(
    std::vector<uint8_t>& bitstream,
    size_t& bitOffset,
    uint32_t messageId
) {
    // Source 2 Variable-Bit Encoding:
    // Prefix 0b00 (6 bits):   0..15
    // Prefix 0b01 (10 bits):  16..255
    // Prefix 0b10 (14 bits):  256..4095
    // Prefix 0b11 (34 bits):  4096..2^32-1
    auto writeBits = [&](uint32_t val, size_t numBits) {
        for (size_t i = 0; i < numBits; ++i) {
            size_t byteIdx = bitOffset / 8;
            size_t bitInByte = bitOffset % 8;
            if (byteIdx >= bitstream.size()) {
                bitstream.resize(byteIdx + 1, 0);
            }
            if ((val >> i) & 1) {
                bitstream[byteIdx] |= static_cast<uint8_t>(1 << bitInByte);
            }
            bitOffset++;
        }
    };

    if (messageId < 16) {
        uint32_t header = (0 << 4) | (messageId & 0x0F);
        writeBits(header, 6);
    } else if (messageId < 256) {
        uint32_t header = (1 << 4) | (messageId & 0x0F);
        writeBits(header, 6);
        writeBits(messageId >> 4, 4);
    } else if (messageId < 4096) {
        uint32_t header = (2 << 4) | (messageId & 0x0F);
        writeBits(header, 6);
        writeBits(messageId >> 4, 8);
    } else {
        uint32_t header = (3 << 4) | (messageId & 0x0F);
        writeBits(header, 6);
        writeBits(messageId >> 4, 28);
    }
}

uint32_t CSGOInputWireCodec::ReadVariableBitMessageId(
    const uint8_t* bitstream,
    size_t& bitOffset,
    size_t maxBits
) {
    auto readBits = [&](size_t numBits) -> uint32_t {
        uint32_t res = 0;
        for (size_t i = 0; i < numBits; ++i) {
            if (bitOffset >= maxBits) return 0;
            size_t byteIdx = bitOffset / 8;
            size_t bitInByte = bitOffset % 8;
            if ((bitstream[byteIdx] >> bitInByte) & 1) {
                res |= (1 << i);
            }
            bitOffset++;
        }
        return res;
    };

    uint32_t initial6 = readBits(6);
    uint32_t prefix = (initial6 >> 4) & 3;
    uint32_t extraBits = ((static_cast<int32_t>(2 - prefix) >> 31 & 0x10) + prefix * 4);
    if (extraBits == 0) {
        return initial6 & 0x0F;
    }
    uint32_t upper = readBits(extraBits);
    return (upper << 4) | (initial6 & 0x0F);
}

std::vector<uint8_t> CSGOInputWireCodec::SerializeUserCmdToBitbuf(
    const CSGOUserCmdPB& userCmd,
    uint32_t numBackupCommands,
    uint32_t numNewCommands
) {
    // 1. Serialize CSGOUserCmdPB to protobuf bytes
    CByteStreamWriter cmdWriter;
    userCmd._InternalSerialize(cmdWriter);

    // 2. Wrap into CCLCMsg_Move
    CCLCMsg_Move moveMsg;
    moveMsg.m_nNumBackupCommands = numBackupCommands;
    moveMsg.m_nNumNewCommands = numNewCommands;
    moveMsg.m_vecData = cmdWriter.GetData();

    // 3. Serialize CCLCMsg_Move to protobuf payload
    CByteStreamWriter moveWriter;
    moveMsg._InternalSerialize(moveWriter);

    // 4. Frame into bitstream with Variable-Bit NetMessage ID header (0x15 = 21 -> 10 bits)
    std::vector<uint8_t> outputBitstream;
    size_t bitOffset = 0;
    WriteVariableBitMessageId(outputBitstream, bitOffset, CCLCMsg_Move::NET_MESSAGE_ID);

    // Byte-align for payload fast-path blit (per networksystem.dll)
    size_t alignedBytes = (bitOffset + 7) / 8;
    outputBitstream.resize(alignedBytes);

    // Append VarInt32 length prefix
    CByteStreamWriter lenWriter;
    lenWriter.WriteVarInt32(static_cast<uint32_t>(moveWriter.GetSize()));
    outputBitstream.insert(outputBitstream.end(), lenWriter.GetData().begin(), lenWriter.GetData().end());

    // Append raw protobuf payload
    outputBitstream.insert(outputBitstream.end(), moveWriter.GetData().begin(), moveWriter.GetData().end());

    return outputBitstream;
}

bool CSGOInputWireCodec::UnpackUserCmdFromBitbuf(
    const uint8_t* payloadData,
    size_t payloadSize,
    std::vector<CSGOUserCmdPB>& outCmds,
    uint32_t& outNumBackup,
    uint32_t& outNumNew
) {
    if (payloadData == nullptr || payloadSize == 0) return false;

    size_t bitOffset = 0;
    size_t maxBits = payloadSize * 8;
    uint32_t msgId = ReadVariableBitMessageId(payloadData, bitOffset, maxBits);
    if (msgId != CCLCMsg_Move::NET_MESSAGE_ID) {
        return false;
    }

    // Align to byte boundary
    size_t byteOffset = (bitOffset + 7) / 8;
    if (byteOffset >= payloadSize) return false;

    CByteStreamReader lenReader(payloadData + byteOffset, payloadSize - byteOffset);
    uint32_t movePayloadLen = 0;
    if (!lenReader.ReadVarInt32(movePayloadLen)) return false;

    size_t dataStart = byteOffset + lenReader.GetOffset();
    if (dataStart + movePayloadLen > payloadSize) return false;

    // Parse CCLCMsg_Move
    CByteStreamReader moveReader(payloadData + dataStart, movePayloadLen);
    CCLCMsg_Move moveMsg;
    if (!moveMsg._InternalParse(moveReader)) return false;

    outNumBackup = moveMsg.m_nNumBackupCommands;
    outNumNew = moveMsg.m_nNumNewCommands;

    // Parse concatenated CSGOUserCmdPB stream from moveMsg.m_vecData
    CByteStreamReader cmdStreamReader(moveMsg.m_vecData.data(), moveMsg.m_vecData.size());
    while (cmdStreamReader.HasMore()) {
        CSGOUserCmdPB cmd;
        if (!cmd._InternalParse(cmdStreamReader)) break;
        outCmds.push_back(cmd);
    }

    return !outCmds.empty();
}

} // namespace cs2::client
