#include "game/networksystem/demo_reader.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace cs2::networksystem {

// ============================================================================
// 1. Google Snappy Block Codec Implementation (Pure ISO C++20)
// ============================================================================

bool CSnappyCodec::GetUncompressedLength(
    const uint8_t* src,
    size_t srcLen,
    size_t& outLen,
    size_t& varIntBytes
) {
    if (src == nullptr || srcLen == 0) return false;

    uint32_t val = 0;
    int shift = 0;
    size_t pos = 0;

    while (pos < srcLen) {
        uint8_t c = src[pos++];
        val |= static_cast<uint32_t>(c & 0x7F) << shift;
        if ((c & 0x80) == 0) {
            outLen = val;
            varIntBytes = pos;
            return true;
        }
        shift += 7;
        if (shift >= 35) return false;
    }

    return false;
}

bool CSnappyCodec::Decompress(
    const uint8_t* src,
    size_t srcLen,
    uint8_t* dst,
    size_t dstCapacity,
    size_t& outWritten
) {
    outWritten = 0;
    if (src == nullptr || dst == nullptr || srcLen == 0) return false;

    size_t uncompressedLen = 0;
    size_t varIntBytes = 0;
    if (!GetUncompressedLength(src, srcLen, uncompressedLen, varIntBytes)) {
        return false;
    }

    if (uncompressedLen > dstCapacity) return false;

    size_t srcPos = varIntBytes;
    size_t dstPos = 0;

    while (srcPos < srcLen && dstPos < uncompressedLen) {
        uint8_t tag = src[srcPos++];
        uint8_t elemType = tag & 0x03;

        if (elemType == 0) {
            // Literal
            size_t litLen = (tag >> 2);
            if (litLen < 60) {
                litLen += 1;
            } else if (litLen == 60) {
                if (srcPos >= srcLen) return false;
                litLen = static_cast<size_t>(src[srcPos++]) + 1;
            } else if (litLen == 61) {
                if (srcPos + 2 > srcLen) return false;
                litLen = static_cast<size_t>(src[srcPos]) |
                         (static_cast<size_t>(src[srcPos + 1]) << 8);
                litLen += 1;
                srcPos += 2;
            } else if (litLen == 62) {
                if (srcPos + 3 > srcLen) return false;
                litLen = static_cast<size_t>(src[srcPos]) |
                         (static_cast<size_t>(src[srcPos + 1]) << 8) |
                         (static_cast<size_t>(src[srcPos + 2]) << 16);
                litLen += 1;
                srcPos += 3;
            } else if (litLen == 63) {
                if (srcPos + 4 > srcLen) return false;
                litLen = static_cast<size_t>(src[srcPos]) |
                         (static_cast<size_t>(src[srcPos + 1]) << 8) |
                         (static_cast<size_t>(src[srcPos + 2]) << 16) |
                         (static_cast<size_t>(src[srcPos + 3]) << 24);
                litLen += 1;
                srcPos += 4;
            }

            if (srcPos + litLen > srcLen || dstPos + litLen > uncompressedLen) return false;
            std::memcpy(dst + dstPos, src + srcPos, litLen);
            srcPos += litLen;
            dstPos += litLen;
        } else if (elemType == 1) {
            // Copy 1-byte offset
            size_t copyLen = ((tag >> 2) & 0x07) + 4;
            if (srcPos >= srcLen) return false;
            size_t offset = (static_cast<size_t>(tag >> 5) << 8) | static_cast<size_t>(src[srcPos++]);
            if (offset == 0 || offset > dstPos || dstPos + copyLen > uncompressedLen) return false;

            for (size_t i = 0; i < copyLen; ++i) {
                dst[dstPos + i] = dst[dstPos - offset + i];
            }
            dstPos += copyLen;
        } else if (elemType == 2) {
            // Copy 2-byte offset
            size_t copyLen = (tag >> 2) + 1;
            if (srcPos + 2 > srcLen) return false;
            size_t offset = static_cast<size_t>(src[srcPos]) |
                            (static_cast<size_t>(src[srcPos + 1]) << 8);
            srcPos += 2;
            if (offset == 0 || offset > dstPos || dstPos + copyLen > uncompressedLen) return false;

            for (size_t i = 0; i < copyLen; ++i) {
                dst[dstPos + i] = dst[dstPos - offset + i];
            }
            dstPos += copyLen;
        } else if (elemType == 3) {
            // Copy 4-byte offset
            size_t copyLen = (tag >> 2) + 1;
            if (srcPos + 4 > srcLen) return false;
            size_t offset = static_cast<size_t>(src[srcPos]) |
                            (static_cast<size_t>(src[srcPos + 1]) << 8) |
                            (static_cast<size_t>(src[srcPos + 2]) << 16) |
                            (static_cast<size_t>(src[srcPos + 3]) << 24);
            srcPos += 4;
            if (offset == 0 || offset > dstPos || dstPos + copyLen > uncompressedLen) return false;

            for (size_t i = 0; i < copyLen; ++i) {
                dst[dstPos + i] = dst[dstPos - offset + i];
            }
            dstPos += copyLen;
        }
    }

    outWritten = dstPos;
    return dstPos == uncompressedLen;
}

bool CSnappyCodec::Decompress(std::span<const uint8_t> src, std::vector<uint8_t>& outDst) {
    if (src.empty()) return false;
    size_t uncompressedLen = 0;
    size_t varIntBytes = 0;
    if (!GetUncompressedLength(src.data(), src.size(), uncompressedLen, varIntBytes)) {
        return false;
    }

    outDst.resize(uncompressedLen);
    size_t written = 0;
    if (!Decompress(src.data(), src.size(), outDst.data(), outDst.size(), written)) {
        outDst.clear();
        return false;
    }
    return true;
}

std::vector<uint8_t> CSnappyCodec::Compress(std::span<const uint8_t> src) {
    std::vector<uint8_t> out;
    if (src.empty()) {
        out.push_back(0); // 0 uncompressed length
        return out;
    }

    // 1. Write uncompressed length VarInt
    uint32_t val = static_cast<uint32_t>(src.size());
    while (val >= 0x80) {
        out.push_back(static_cast<uint8_t>((val & 0x7F) | 0x80));
        val >>= 7;
    }
    out.push_back(static_cast<uint8_t>(val));

    // 2. Hash table for 4-byte sub-sequences
    constexpr size_t HASH_TABLE_SIZE = 4096;
    int32_t hashTable[HASH_TABLE_SIZE];
    for (size_t i = 0; i < HASH_TABLE_SIZE; ++i) hashTable[i] = -1;

    auto emitLiterals = [&](size_t litStart, size_t litLen) {
        while (litLen > 0) {
            size_t chunk = std::min<size_t>(litLen, 60);
            uint8_t tag = static_cast<uint8_t>((chunk - 1) << 2); // Type 0
            out.push_back(tag);
            out.insert(out.end(), src.data() + litStart, src.data() + litStart + chunk);
            litStart += chunk;
            litLen -= chunk;
        }
    };

    size_t pos = 0;
    size_t litStart = 0;
    const size_t limit = src.size() >= 4 ? src.size() - 4 : 0;

    while (pos < limit) {
        uint32_t word = static_cast<uint32_t>(src[pos]) |
                        (static_cast<uint32_t>(src[pos + 1]) << 8) |
                        (static_cast<uint32_t>(src[pos + 2]) << 16) |
                        (static_cast<uint32_t>(src[pos + 3]) << 24);

        size_t h = (static_cast<uint64_t>(word) * 0x1e35a7bdULL) >> (32 - 12);
        h %= HASH_TABLE_SIZE;

        int32_t matchPos = hashTable[h];
        hashTable[h] = static_cast<int32_t>(pos);

        if (matchPos >= 0 && (pos - static_cast<size_t>(matchPos)) < 65535 &&
            std::memcmp(src.data() + pos, src.data() + matchPos, 4) == 0) {
            // Match found! Emit any pending literals
            if (pos > litStart) {
                emitLiterals(litStart, pos - litStart);
            }

            size_t offset = pos - static_cast<size_t>(matchPos);
            size_t matchLen = 4;
            while (pos + matchLen < src.size() &&
                   src[pos + matchLen] == src[matchPos + matchLen] &&
                   matchLen < 64) {
                matchLen++;
            }

            if (offset < 2048 && matchLen >= 4 && matchLen <= 11) {
                // Type 1: 1-byte offset
                uint8_t tag = static_cast<uint8_t>(0x01 | ((matchLen - 4) << 2) | ((offset >> 8) << 5));
                out.push_back(tag);
                out.push_back(static_cast<uint8_t>(offset & 0xFF));
            } else {
                // Type 2: 2-byte offset
                uint8_t tag = static_cast<uint8_t>(0x02 | ((matchLen - 1) << 2));
                out.push_back(tag);
                out.push_back(static_cast<uint8_t>(offset & 0xFF));
                out.push_back(static_cast<uint8_t>((offset >> 8) & 0xFF));
            }

            pos += matchLen;
            litStart = pos;
        } else {
            pos++;
        }
    }

    if (litStart < src.size()) {
        emitLiterals(litStart, src.size() - litStart);
    }

    return out;
}

// ============================================================================
// 2. Sub-Tick UserCmd Extractor Implementation
// ============================================================================

bool CSubTickUserCmdExtractor::ExtractFromBasePB(
    const client::CBaseUserCmdPB& baseCmd,
    ExtractedUserCmd& outCmd
) {
    outCmd.commandNumber = baseCmd.m_nCommandNumber;
    outCmd.clientTick = baseCmd.m_nClientTick;
    outCmd.buttons = baseCmd.m_ButtonsPB.buttonstate1;
    outCmd.forwardMove = baseCmd.m_flForwardMove;
    outCmd.leftMove = baseCmd.m_flLeftMove;
    outCmd.upMove = baseCmd.m_flUpMove;
    outCmd.viewAngles[0] = baseCmd.m_ViewAngles.pitch;
    outCmd.viewAngles[1] = baseCmd.m_ViewAngles.yaw;
    outCmd.viewAngles[2] = baseCmd.m_ViewAngles.roll;

    outCmd.subtickSteps.reserve(baseCmd.m_SubtickMoves.size());
    for (const auto& step : baseCmd.m_SubtickMoves) {
        SubTickAnalogStep s;
        s.button = step.m_nButton;
        s.pressed = step.m_bPressed;
        s.when = step.m_flWhen;
        s.analogForwardDelta = step.m_flAnalogForwardDelta;
        s.analogLeftDelta = step.m_flAnalogLeftDelta;
        s.analogPitchDelta = step.m_flAnalogPitchDelta;
        s.analogYawDelta = step.m_flAnalogYawDelta;
        outCmd.subtickSteps.push_back(s);
    }

    return true;
}

bool CSubTickUserCmdExtractor::ExtractFromCSGOPB(
    const client::CSGOUserCmdPB& csgoCmd,
    ExtractedUserCmd& outCmd
) {
    ExtractFromBasePB(csgoCmd.m_Base, outCmd);

    outCmd.inputHistory.reserve(csgoCmd.m_InputHistory.size());
    for (const auto& entry : csgoCmd.m_InputHistory) {
        SubTickInputHistory h;
        h.viewAngles[0] = entry.view_angles.pitch;
        h.viewAngles[1] = entry.view_angles.yaw;
        h.viewAngles[2] = entry.view_angles.roll;
        h.observationPosition[0] = entry.shoot_position.x;
        h.observationPosition[1] = entry.shoot_position.y;
        h.observationPosition[2] = entry.shoot_position.z;
        h.targetEntityIndex = entry.target_ent_index;
        h.playerTickCount = entry.player_tick_count;
        h.playerTickFraction = entry.player_tick_fraction;
        outCmd.inputHistory.push_back(h);
    }

    return true;
}

bool CSubTickUserCmdExtractor::ExtractFromBuffer(
    const uint8_t* data,
    size_t size,
    std::vector<ExtractedUserCmd>& outCmds
) {
    if (data == nullptr || size == 0) return false;

    // 1. First attempt to unpack as clc_Move bitstream
    std::vector<client::CSGOUserCmdPB> unpackedCmds;
    uint32_t numBackup = 0;
    uint32_t numNew = 0;
    if (client::CSGOInputWireCodec::UnpackUserCmdFromBitbuf(data, size, unpackedCmds, numBackup, numNew)) {
        for (const auto& cmd : unpackedCmds) {
            ExtractedUserCmd extracted;
            ExtractFromCSGOPB(cmd, extracted);
            outCmds.push_back(extracted);
        }
        return true;
    }

    // 2. Fallback: Parse directly as CSGOUserCmdPB protobuf stream
    client::CByteStreamReader directReader(data, size);
    bool foundAny = false;
    while (directReader.HasMore()) {
        client::CSGOUserCmdPB csgoCmd;
        if (csgoCmd._InternalParse(directReader)) {
            ExtractedUserCmd extracted;
            ExtractFromCSGOPB(csgoCmd, extracted);
            outCmds.push_back(extracted);
            foundAny = true;
        } else {
            break;
        }
    }

    if (foundAny) return true;

    // 3. Fallback: Parse directly as CBaseUserCmdPB
    client::CByteStreamReader baseReader(data, size);
    while (baseReader.HasMore()) {
        client::CBaseUserCmdPB baseCmd;
        if (baseCmd._InternalParse(baseReader)) {
            ExtractedUserCmd extracted;
            ExtractFromBasePB(baseCmd, extracted);
            outCmds.push_back(extracted);
            foundAny = true;
        } else {
            break;
        }
    }

    return foundAny;
}

// ============================================================================
// 3. CDemoReader Implementation
// ============================================================================

bool CDemoReader::Open(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    const std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) return false;

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        return false;
    }

    return ParseMemory(buffer.data(), buffer.size());
}

bool CDemoReader::ParseMemory(const uint8_t* data, size_t size) {
    m_Frames.clear();
    m_ExtractedCmds.clear();
    m_Stats = DemoTelemetryStats{};

    if (data == nullptr || size < 8) return false;

    client::CByteStreamReader reader(data, size);

    if (!ParseHeaderBlock(reader)) {
        return false;
    }

    if (!ParseFrameLoop(reader)) {
        return false;
    }

    // Finalize statistics
    if (m_Stats.packetFrames + m_Stats.signonFrames + m_Stats.fullPacketFrames > 0) {
        m_Stats.averagePacketSize = static_cast<double>(m_Stats.totalRawBytes) /
            static_cast<double>(m_Stats.packetFrames + m_Stats.signonFrames + m_Stats.fullPacketFrames);
    }
    if (m_Stats.totalDecompressedBytes > 0) {
        m_Stats.compressionRatio = static_cast<double>(m_Stats.totalRawBytes) /
            static_cast<double>(m_Stats.totalDecompressedBytes);
    }

    return true;
}

bool CDemoReader::ParseHeaderBlock(client::CByteStreamReader& reader) {
    if (reader.GetRemaining() < 8) return false;

    char magicBytes[8]{0};
    if (!reader.ReadBytes(reinterpret_cast<uint8_t*>(magicBytes), 8)) {
        return false;
    }

    if (std::memcmp(magicBytes, "PBDEMS2", 7) == 0) {
        m_Header.magic = "PBDEMS2";

        // Optional 4-byte or 8-byte file summary offset
        if (reader.GetRemaining() >= 4) {
            const uint8_t* p = reader.CurrentPtr();
            // If the next byte is 1 (DEM_FileHeader) or 0x41 (DEM_FileHeader | compressed),
            // the command frames begin immediately at offset 8.
            if (*p != 1 && *p != 0x41) {
                uint32_t summaryOff = 0;
                reader.ReadFixed32(summaryOff);
                m_Header.summaryOffset = static_cast<int32_t>(summaryOff);
            }
        }
        return true;
    } else if (std::memcmp(magicBytes, "HL2DEMO", 7) == 0) {
        m_Header.magic = "HL2DEMO";

        // Read legacy header fields
        uint32_t fileProto = 0, netProto = 0;
        if (!reader.ReadFixed32(fileProto)) return false;
        if (!reader.ReadFixed32(netProto)) return false;

        m_Header.fileProtocol = static_cast<int32_t>(fileProto);
        m_Header.networkProtocol = static_cast<int32_t>(netProto);

        char srvName[260]{0}, cliName[260]{0}, mapName[260]{0}, gameDir[260]{0};
        if (!reader.ReadBytes(reinterpret_cast<uint8_t*>(srvName), 260)) return false;
        if (!reader.ReadBytes(reinterpret_cast<uint8_t*>(cliName), 260)) return false;
        if (!reader.ReadBytes(reinterpret_cast<uint8_t*>(mapName), 260)) return false;
        if (!reader.ReadBytes(reinterpret_cast<uint8_t*>(gameDir), 260)) return false;

        m_Header.serverName = srvName;
        m_Header.clientName = cliName;
        m_Header.mapName = mapName;
        m_Header.gameDirectory = gameDir;

        float playbackTime = 0.0f;
        uint32_t ticks = 0, frames = 0, signonLen = 0;
        reader.ReadFloat(playbackTime);
        reader.ReadFixed32(ticks);
        reader.ReadFixed32(frames);
        reader.ReadFixed32(signonLen);

        if (playbackTime > 0.0f && ticks > 0) {
            m_Header.serverTickrate = static_cast<float>(ticks) / playbackTime;
        }

        return true;
    }

    return false;
}

bool CDemoReader::ParseFrameLoop(client::CByteStreamReader& reader) {
    while (reader.HasMore()) {
        uint32_t cmdRaw = 0;
        if (!reader.ReadVarInt32(cmdRaw)) break;

        // Check for stop command
        if (cmdRaw == static_cast<uint32_t>(EDemoCommand::DEM_Stop)) {
            break;
        }

        // Determine Snappy compression flag
        bool isCompressed = (cmdRaw & static_cast<uint32_t>(EDemoCommand::DEM_IsCompressed)) != 0;
        uint32_t cmd = cmdRaw & ~static_cast<uint32_t>(EDemoCommand::DEM_IsCompressed);

        // Also handle alternate bit 0 flag
        if (!isCompressed && (cmdRaw & 0x01) != 0 && (cmdRaw > 19)) {
            isCompressed = true;
            cmd = cmdRaw & ~0x01;
        }

        uint32_t tickVal = 0;
        if (!reader.ReadVarInt32(tickVal)) break;
        int32_t tick = static_cast<int32_t>(tickVal);

        uint32_t size = 0;
        if (!reader.ReadVarInt32(size)) break;

        if (reader.GetRemaining() < size) break;

        DemoFrame frame;
        frame.command = cmd;
        frame.tick = tick;
        frame.isCompressed = isCompressed;
        frame.rawData.resize(size);
        reader.ReadBytes(frame.rawData.data(), size);

        // Decompress Snappy payload if flagged
        if (isCompressed && !frame.rawData.empty()) {
            if (CSnappyCodec::Decompress(frame.rawData, frame.decompressedData)) {
                m_Stats.compressedFrames++;
                m_Stats.totalDecompressedBytes += frame.decompressedData.size();
            } else {
                // If Snappy failed, keep raw payload
                m_Stats.uncompressedFrames++;
                m_Stats.totalDecompressedBytes += frame.rawData.size();
            }
        } else {
            m_Stats.uncompressedFrames++;
            m_Stats.totalDecompressedBytes += frame.rawData.size();
        }

        m_Stats.totalFrames++;
        m_Stats.totalRawBytes += frame.rawData.size();

        ProcessFramePayload(frame);
        m_Frames.push_back(std::move(frame));
    }

    return true;
}

void CDemoReader::ProcessFramePayload(const DemoFrame& frame) {
    const auto& payload = frame.GetPayload();
    if (payload.empty()) return;

    if (IsFileHeaderCommand(frame.command)) {
        m_Stats.headerFrames++;
        // Parse CDemoFileHeader protobuf
        client::CByteStreamReader headerReader(payload.data(), payload.size());
        while (headerReader.HasMore()) {
            uint32_t tag = 0;
            if (!headerReader.ReadVarInt32(tag)) break;
            uint32_t fieldNumber = tag >> 3;
            auto wireType = static_cast<client::WireType>(tag & 7);

            switch (fieldNumber) {
                case 1: { // demo_file_stamp
                    if (wireType == client::WireType::LengthDelimited) {
                        uint32_t len = 0;
                        headerReader.ReadVarInt32(len);
                        headerReader.ReadString(m_Header.magic, len);
                    } else headerReader.SkipField(wireType);
                    break;
                }
                case 2: { // patch_version / file_protocol
                    if (wireType == client::WireType::VarInt) {
                        uint32_t val = 0;
                        headerReader.ReadVarInt32(val);
                        m_Header.fileProtocol = static_cast<int32_t>(val);
                    } else headerReader.SkipField(wireType);
                    break;
                }
                case 3: { // server_name
                    if (wireType == client::WireType::LengthDelimited) {
                        uint32_t len = 0;
                        headerReader.ReadVarInt32(len);
                        headerReader.ReadString(m_Header.serverName, len);
                    } else headerReader.SkipField(wireType);
                    break;
                }
                case 4: { // client_name
                    if (wireType == client::WireType::LengthDelimited) {
                        uint32_t len = 0;
                        headerReader.ReadVarInt32(len);
                        headerReader.ReadString(m_Header.clientName, len);
                    } else headerReader.SkipField(wireType);
                    break;
                }
                case 5: { // map_name
                    if (wireType == client::WireType::LengthDelimited) {
                        uint32_t len = 0;
                        headerReader.ReadVarInt32(len);
                        headerReader.ReadString(m_Header.mapName, len);
                    } else headerReader.SkipField(wireType);
                    break;
                }
                case 6: { // game_directory
                    if (wireType == client::WireType::LengthDelimited) {
                        uint32_t len = 0;
                        headerReader.ReadVarInt32(len);
                        headerReader.ReadString(m_Header.gameDirectory, len);
                    } else headerReader.SkipField(wireType);
                    break;
                }
                case 7: { // fullpackets_version / network protocol
                    if (wireType == client::WireType::VarInt) {
                        uint32_t val = 0;
                        headerReader.ReadVarInt32(val);
                        m_Header.networkProtocol = static_cast<int32_t>(val);
                    } else headerReader.SkipField(wireType);
                    break;
                }
                case 13: { // build_num
                    if (wireType == client::WireType::VarInt) {
                        uint32_t val = 0;
                        headerReader.ReadVarInt32(val);
                        m_Header.buildNum = static_cast<int32_t>(val);
                    } else headerReader.SkipField(wireType);
                    break;
                }
                case 15: { // server_start_tick
                    if (wireType == client::WireType::VarInt) {
                        uint32_t val = 0;
                        headerReader.ReadVarInt32(val);
                        m_Header.serverStartTick = static_cast<int32_t>(val);
                    } else headerReader.SkipField(wireType);
                    break;
                }
                case 16: { // network_protocol explicit field
                    if (wireType == client::WireType::VarInt) {
                        uint32_t val = 0;
                        headerReader.ReadVarInt32(val);
                        m_Header.networkProtocol = static_cast<int32_t>(val);
                    } else headerReader.SkipField(wireType);
                    break;
                }
                case 17: { // server_tickrate (float)
                    if (wireType == client::WireType::Fixed32) {
                        headerReader.ReadFloat(m_Header.serverTickrate);
                    } else headerReader.SkipField(wireType);
                    break;
                }
                default:
                    headerReader.SkipField(wireType);
                    break;
            }
        }
    } else if (IsPacketCommand(frame.command)) {
        m_Stats.packetFrames++;
        ProcessNetMessages(payload.data(), payload.size());
    } else if (IsSignonPacketCommand(frame.command)) {
        m_Stats.signonFrames++;
        ProcessNetMessages(payload.data(), payload.size());
    } else if (IsFullPacketCommand(frame.command)) {
        m_Stats.fullPacketFrames++;
        // CDemoFullPacket has field 1 (string_table) and field 2 (packet)
        client::CByteStreamReader fullPacketReader(payload.data(), payload.size());
        while (fullPacketReader.HasMore()) {
            uint32_t tag = 0;
            if (!fullPacketReader.ReadVarInt32(tag)) break;
            uint32_t fieldNumber = tag >> 3;
            auto wireType = static_cast<client::WireType>(tag & 7);

            if (fieldNumber == 2 && wireType == client::WireType::LengthDelimited) {
                // Inner CDemoPacket
                uint32_t packetLen = 0;
                fullPacketReader.ReadVarInt32(packetLen);
                if (packetLen > 0 && packetLen <= fullPacketReader.GetRemaining()) {
                    ProcessNetMessages(fullPacketReader.CurrentPtr(), packetLen);
                    fullPacketReader.Skip(packetLen);
                }
            } else {
                fullPacketReader.SkipField(wireType);
            }
        }
    } else if (IsCustomDataCommand(frame.command)) {
        m_Stats.customDataFrames++;
    } else if (IsUserCmdCommand(frame.command)) {
        m_Stats.userCmdFrames++;
        // Direct DEM_UserCmd: field 1: cmd_number, field 2: data (CBaseUserCmdPB / CSGOUserCmdPB)
        client::CByteStreamReader userCmdReader(payload.data(), payload.size());
        while (userCmdReader.HasMore()) {
            uint32_t tag = 0;
            if (!userCmdReader.ReadVarInt32(tag)) break;
            uint32_t fieldNumber = tag >> 3;
            auto wireType = static_cast<client::WireType>(tag & 7);

            if (fieldNumber == 2 && wireType == client::WireType::LengthDelimited) {
                uint32_t cmdLen = 0;
                userCmdReader.ReadVarInt32(cmdLen);
                if (cmdLen > 0 && cmdLen <= userCmdReader.GetRemaining()) {
                    std::vector<ExtractedUserCmd> cmds;
                    if (CSubTickUserCmdExtractor::ExtractFromBuffer(userCmdReader.CurrentPtr(), cmdLen, cmds)) {
                        for (auto& cmd : cmds) {
                            m_Stats.totalUserCmds++;
                            m_Stats.totalSubtickSteps += static_cast<uint32_t>(cmd.subtickSteps.size());
                            m_Stats.totalInputHistoryEntries += static_cast<uint32_t>(cmd.inputHistory.size());
                            m_Stats.buttonEventDistribution[cmd.buttons]++;
                            for (const auto& step : cmd.subtickSteps) {
                                m_Stats.buttonEventDistribution[step.button]++;
                            }
                            m_ExtractedCmds.push_back(std::move(cmd));
                        }
                    }
                    userCmdReader.Skip(cmdLen);
                }
            } else {
                userCmdReader.SkipField(wireType);
            }
        }
    }
}

void CDemoReader::ProcessNetMessages(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) return;

    // Check if this is an outer CDemoPacket protobuf carrying bytes data = 3
    client::CByteStreamReader packetPbReader(data, size);
    while (packetPbReader.HasMore()) {
        uint32_t tag = 0;
        if (!packetPbReader.ReadVarInt32(tag)) break;
        uint32_t fieldNumber = tag >> 3;
        auto wireType = static_cast<client::WireType>(tag & 7);

        if (fieldNumber == 3 && wireType == client::WireType::LengthDelimited) {
            uint32_t dataLen = 0;
            packetPbReader.ReadVarInt32(dataLen);
            if (dataLen > 0 && dataLen <= packetPbReader.GetRemaining()) {
                // Recurse into inner netmessages stream
                data = packetPbReader.CurrentPtr();
                size = dataLen;
            }
            break;
        } else {
            packetPbReader.SkipField(wireType);
        }
    }

    // Try extracting UserCmds via clc_Move bitstream
    std::vector<ExtractedUserCmd> cmds;
    if (CSubTickUserCmdExtractor::ExtractFromBuffer(data, size, cmds)) {
        for (auto& cmd : cmds) {
            m_Stats.totalUserCmds++;
            m_Stats.totalSubtickSteps += static_cast<uint32_t>(cmd.subtickSteps.size());
            m_Stats.totalInputHistoryEntries += static_cast<uint32_t>(cmd.inputHistory.size());
            m_Stats.buttonEventDistribution[cmd.buttons]++;
            for (const auto& step : cmd.subtickSteps) {
                m_Stats.buttonEventDistribution[step.button]++;
            }
            m_ExtractedCmds.push_back(std::move(cmd));
        }
    }

    // Also scan variable-bit NetMessage frames
    bool foundNetMsgEnts = false;
    CBitReadStream bitReader(data, size, true);
    while (bitReader.GetBitsRead() + 6 <= bitReader.GetTotalBits()) {
        auto headerOpt = CNetMessageFraming::ReadHeader(bitReader);
        if (!headerOpt) break;

        const auto& hdr = *headerOpt;
        if (hdr.byteSize == 0) continue;

        if (hdr.messageId == 21) { // clc_Move (NetMessage 0x15)
            std::vector<uint8_t> movePayload(hdr.byteSize);
            if (bitReader.ReadBytes(movePayload.data(), hdr.byteSize)) {
                client::CByteStreamReader movePbReader(movePayload.data(), movePayload.size());
                client::CCLCMsg_Move moveMsg;
                if (moveMsg._InternalParse(movePbReader)) {
                    std::vector<ExtractedUserCmd> moveCmds;
                    if (CSubTickUserCmdExtractor::ExtractFromBuffer(moveMsg.m_vecData.data(), moveMsg.m_vecData.size(), moveCmds)) {
                        for (auto& cmd : moveCmds) {
                            m_Stats.totalUserCmds++;
                            m_Stats.totalSubtickSteps += static_cast<uint32_t>(cmd.subtickSteps.size());
                            m_Stats.totalInputHistoryEntries += static_cast<uint32_t>(cmd.inputHistory.size());
                            m_Stats.buttonEventDistribution[cmd.buttons]++;
                            for (const auto& step : cmd.subtickSteps) {
                                m_Stats.buttonEventDistribution[step.button]++;
                            }
                            m_ExtractedCmds.push_back(std::move(cmd));
                        }
                    }
                }
            }
        } else if (hdr.messageId == 55 || hdr.messageId == 26 || hdr.messageId == 40) { // svc_PacketEntities
            m_Stats.totalPacketEntitiesMessages++;
            foundNetMsgEnts = true;
            std::vector<uint8_t> entPayload(hdr.byteSize);
            if (bitReader.ReadBytes(entPayload.data(), hdr.byteSize)) {
                // Parse svc_PacketEntities protobuf fields
                client::CByteStreamReader entReader(entPayload.data(), entPayload.size());
                while (entReader.HasMore()) {
                    uint32_t tag = 0;
                    if (!entReader.ReadVarInt32(tag)) break;
                    uint32_t fnum = tag >> 3;
                    auto wtype = static_cast<client::WireType>(tag & 7);

                    if (fnum == 2 && wtype == client::WireType::VarInt) { // updated_entries
                        uint32_t updated = 0;
                        entReader.ReadVarInt32(updated);
                        m_Stats.totalEntityDeltas += updated;
                    } else {
                        entReader.SkipField(wtype);
                    }
                }
            }
        } else {
            // Skip other NetMessage payloads
            std::vector<uint8_t> dummy(hdr.byteSize);
            if (!bitReader.ReadBytes(dummy.data(), hdr.byteSize)) break;
        }
    }
}

// ============================================================================
// 4. CDemoWriter Implementation
// ============================================================================

CDemoWriter::CDemoWriter() {
    m_Header.magic = "PBDEMS2";
    m_Header.fileProtocol = 4;
    m_Header.networkProtocol = 13980;
    m_Header.serverTickrate = 64.0f;
    m_Header.mapName = "de_dust2";
    m_Header.serverName = "Counter-Strike 2 Dedicated Server";
    m_Header.clientName = "Source2Client";
    m_Header.gameDirectory = "csgo";
    m_Header.buildNum = 25966348;
}

void CDemoWriter::AppendFrame(
    uint32_t cmd,
    int32_t tick,
    const uint8_t* payload,
    size_t size,
    bool compress
) {
    std::vector<uint8_t> compressedPayload;
    const uint8_t* writePayload = payload;
    size_t writeSize = size;

    if (compress && size > 0) {
        compressedPayload = CSnappyCodec::Compress(std::span<const uint8_t>(payload, size));
        writePayload = compressedPayload.data();
        writeSize = compressedPayload.size();
    }

    uint32_t cmdRaw = cmd;
    if (compress) {
        cmdRaw |= static_cast<uint32_t>(EDemoCommand::DEM_IsCompressed);
    }

    client::CByteStreamWriter writer;
    writer.WriteVarInt32(cmdRaw);
    writer.WriteVarInt32(static_cast<uint32_t>(tick));
    writer.WriteVarInt32(static_cast<uint32_t>(writeSize));
    for (size_t i = 0; i < writeSize; ++i) {
        writer.WriteByte(writePayload[i]);
    }

    const auto& frameBytes = writer.GetData();
    m_Buffer.insert(m_Buffer.end(), frameBytes.begin(), frameBytes.end());
}

void CDemoWriter::AddFileHeaderFrame(const DemoHeader& header, int32_t tick) {
    client::CByteStreamWriter pbWriter;

    // field 1: demo_file_stamp
    pbWriter.WriteTag(1, client::WireType::LengthDelimited);
    pbWriter.WriteString(header.magic);

    // field 2: patch_version
    pbWriter.WriteTag(2, client::WireType::VarInt);
    pbWriter.WriteVarInt32(static_cast<uint32_t>(header.fileProtocol));

    // field 3: server_name
    pbWriter.WriteTag(3, client::WireType::LengthDelimited);
    pbWriter.WriteString(header.serverName);

    // field 4: client_name
    pbWriter.WriteTag(4, client::WireType::LengthDelimited);
    pbWriter.WriteString(header.clientName);

    // field 5: map_name
    pbWriter.WriteTag(5, client::WireType::LengthDelimited);
    pbWriter.WriteString(header.mapName);

    // field 6: game_directory
    pbWriter.WriteTag(6, client::WireType::LengthDelimited);
    pbWriter.WriteString(header.gameDirectory);

    // field 7: fullpackets_version / network_protocol
    pbWriter.WriteTag(7, client::WireType::VarInt);
    pbWriter.WriteVarInt32(static_cast<uint32_t>(header.networkProtocol));

    // field 13: build_num
    pbWriter.WriteTag(13, client::WireType::VarInt);
    pbWriter.WriteVarInt32(static_cast<uint32_t>(header.buildNum));

    // field 16: network_protocol
    pbWriter.WriteTag(16, client::WireType::VarInt);
    pbWriter.WriteVarInt32(static_cast<uint32_t>(header.networkProtocol));

    // field 17: server_tickrate
    pbWriter.WriteTag(17, client::WireType::Fixed32);
    pbWriter.WriteFloat(header.serverTickrate);

    const auto& bytes = pbWriter.GetData();
    AppendFrame(static_cast<uint32_t>(EDemoCommand::DEM_FileHeader), tick, bytes.data(), bytes.size(), false);
}

void CDemoWriter::AddPacketFrame(
    int32_t tick,
    const uint8_t* packetData,
    size_t size,
    bool compress,
    uint32_t cmdOverride
) {
    // Wrap in CDemoPacket (field 3: bytes data)
    client::CByteStreamWriter pbWriter;
    pbWriter.WriteTag(3, client::WireType::LengthDelimited);
    pbWriter.WriteLengthDelimited(packetData, size);

    const auto& bytes = pbWriter.GetData();
    uint32_t cmd = cmdOverride != 0 ? cmdOverride : static_cast<uint32_t>(EDemoCommand::DEM_Packet);
    AppendFrame(cmd, tick, bytes.data(), bytes.size(), compress);
}

void CDemoWriter::AddSignonPacketFrame(
    int32_t tick,
    const uint8_t* packetData,
    size_t size,
    bool compress,
    uint32_t cmdOverride
) {
    client::CByteStreamWriter pbWriter;
    pbWriter.WriteTag(3, client::WireType::LengthDelimited);
    pbWriter.WriteLengthDelimited(packetData, size);

    const auto& bytes = pbWriter.GetData();
    uint32_t cmd = cmdOverride != 0 ? cmdOverride : static_cast<uint32_t>(EDemoCommand::DEM_SignonPacket);
    AppendFrame(cmd, tick, bytes.data(), bytes.size(), compress);
}

void CDemoWriter::AddFullPacketFrame(
    int32_t tick,
    const uint8_t* packetData,
    size_t size,
    bool compress,
    uint32_t cmdOverride
) {
    // Inner CDemoPacket
    client::CByteStreamWriter innerWriter;
    innerWriter.WriteTag(3, client::WireType::LengthDelimited);
    innerWriter.WriteLengthDelimited(packetData, size);

    // CDemoFullPacket (field 2: packet)
    client::CByteStreamWriter fullWriter;
    fullWriter.WriteTag(2, client::WireType::LengthDelimited);
    fullWriter.WriteLengthDelimited(innerWriter.GetData().data(), innerWriter.GetData().size());

    const auto& bytes = fullWriter.GetData();
    uint32_t cmd = cmdOverride != 0 ? cmdOverride : static_cast<uint32_t>(EDemoCommand::DEM_FullPacket);
    AppendFrame(cmd, tick, bytes.data(), bytes.size(), compress);
}

void CDemoWriter::AddCustomDataFrame(
    int32_t tick,
    int32_t callbackIndex,
    const uint8_t* customData,
    size_t size,
    bool compress,
    uint32_t cmdOverride
) {
    // CDemoCustomData: field 1: callback_index, field 2: data
    client::CByteStreamWriter customWriter;
    customWriter.WriteTag(1, client::WireType::VarInt);
    customWriter.WriteVarInt32(static_cast<uint32_t>(callbackIndex));
    customWriter.WriteTag(2, client::WireType::LengthDelimited);
    customWriter.WriteLengthDelimited(customData, size);

    const auto& bytes = customWriter.GetData();
    uint32_t cmd = cmdOverride != 0 ? cmdOverride : static_cast<uint32_t>(EDemoCommand::DEM_CustomData);
    AppendFrame(cmd, tick, bytes.data(), bytes.size(), compress);
}

void CDemoWriter::AddUserCmdFrame(
    int32_t tick,
    int32_t cmdNumber,
    const client::CSGOUserCmdPB& userCmd,
    bool compress
) {
    client::CByteStreamWriter cmdWriter;
    userCmd._InternalSerialize(cmdWriter);

    // CDemoUserCmd: field 1: cmd_number, field 2: data
    client::CByteStreamWriter frameWriter;
    frameWriter.WriteTag(1, client::WireType::VarInt);
    frameWriter.WriteVarInt32(static_cast<uint32_t>(cmdNumber));
    frameWriter.WriteTag(2, client::WireType::LengthDelimited);
    frameWriter.WriteLengthDelimited(cmdWriter.GetData().data(), cmdWriter.GetData().size());

    const auto& bytes = frameWriter.GetData();
    AppendFrame(static_cast<uint32_t>(EDemoCommand::DEM_UserCmd), tick, bytes.data(), bytes.size(), compress);
}

std::vector<uint8_t> CDemoWriter::Finalize() {
    std::vector<uint8_t> output;
    // Magic: 8 bytes
    output.resize(8, 0);
    std::memcpy(output.data(), m_Header.magic.c_str(), std::min<size_t>(m_Header.magic.size(), 7));

    // Summary offset: 4 bytes (offset 8..11) pointing to end or 0
    client::CByteStreamWriter summaryWriter;
    summaryWriter.WriteFixed32(static_cast<uint32_t>(m_Header.summaryOffset));
    const auto& summaryBytes = summaryWriter.GetData();
    output.insert(output.end(), summaryBytes.begin(), summaryBytes.end());

    // Append all frames
    output.insert(output.end(), m_Buffer.begin(), m_Buffer.end());

    // Append DEM_Stop
    client::CByteStreamWriter stopWriter;
    stopWriter.WriteVarInt32(static_cast<uint32_t>(EDemoCommand::DEM_Stop));
    const auto& stopBytes = stopWriter.GetData();
    output.insert(output.end(), stopBytes.begin(), stopBytes.end());

    return output;
}

bool CDemoWriter::WriteToFile(const std::string& filepath) {
    auto data = Finalize();
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    return out.good();
}

} // namespace cs2::networksystem
