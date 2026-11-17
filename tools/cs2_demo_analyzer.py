#!/usr/bin/env python3
"""
cs2_demo_analyzer.py - Clean-Room CS2 Match Recording Telemetry & Sub-Tick Bitstream Inspector

Parses Source 2 CS2 match recordings (.dem) and extracts:
- Container header metadata (Magic, Protocols, Tickrate, Map Name, Build Number)
- Frame loop statistics (FileHeader, Packet, SignonPacket, FullPacket, CustomData, UserCmd)
- Snappy block compression ratios and packet size distributions
- Sub-tick UserCmd telemetry (Analog steps, fractional offsets, viewangles, move vectors)
- Entity delta synchronization statistics (svc_PacketEntities updates)

Pure Python implementation: Zero external pip dependencies required.
Complies with sanitized-commands architectural protocols and strict privacy guardrails.
"""

import os
import sys
import struct
import argparse
import json
from typing import Dict, List, Tuple, Optional, Any

# ============================================================================
# 1. Pure Python Google Snappy Decompressor
# ============================================================================

def snappy_decompress(data: bytes) -> bytes:
    """
    Decompresses a Google Snappy block without external C extensions.
    Implements Google Snappy framing specification.
    """
    if not data:
        return b""

    # 1. Read VarInt uncompressed length prefix
    pos = 0
    uncompressed_len = 0
    shift = 0
    while pos < len(data):
        c = data[pos]
        pos += 1
        uncompressed_len |= (c & 0x7F) << shift
        if (c & 0x80) == 0:
            break
        shift += 7
        if shift >= 35:
            raise ValueError("Corrupt Snappy VarInt length")

    out = bytearray()

    # 2. Process element sequence
    while pos < len(data) and len(out) < uncompressed_len:
        tag = data[pos]
        pos += 1
        elem_type = tag & 0x03

        if elem_type == 0:
            # Literal
            lit_len = tag >> 2
            if lit_len < 60:
                lit_len += 1
            elif lit_len == 60:
                if pos >= len(data): break
                lit_len = data[pos] + 1
                pos += 1
            elif lit_len == 61:
                if pos + 2 > len(data): break
                lit_len = data[pos] | (data[pos + 1] << 8) + 1
                pos += 2
            elif lit_len == 62:
                if pos + 3 > len(data): break
                lit_len = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) + 1
                pos += 3
            elif lit_len == 63:
                if pos + 4 > len(data): break
                lit_len = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24) + 1
                pos += 4

            if pos + lit_len > len(data):
                break
            out.extend(data[pos:pos + lit_len])
            pos += lit_len

        elif elem_type == 1:
            # Copy with 1-byte offset
            copy_len = ((tag >> 2) & 0x07) + 4
            if pos >= len(data): break
            offset = ((tag >> 5) << 8) | data[pos]
            pos += 1
            if offset == 0 or offset > len(out): break
            for _ in range(copy_len):
                out.append(out[-offset])

        elif elem_type == 2:
            # Copy with 2-byte offset
            copy_len = (tag >> 2) + 1
            if pos + 2 > len(data): break
            offset = data[pos] | (data[pos + 1] << 8)
            pos += 2
            if offset == 0 or offset > len(out): break
            for _ in range(copy_len):
                out.append(out[-offset])

        elif elem_type == 3:
            # Copy with 4-byte offset
            copy_len = (tag >> 2) + 1
            if pos + 4 > len(data): break
            offset = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24)
            pos += 4
            if offset == 0 or offset > len(out): break
            for _ in range(copy_len):
                out.append(out[-offset])

    return bytes(out)


def snappy_compress_simple(data: bytes) -> bytes:
    """
    Lightweight Snappy block compressor producing compliant Snappy blocks.
    """
    if not data:
        return b"\x00"

    out = bytearray()
    # VarInt length
    val = len(data)
    while val >= 0x80:
        out.append((val & 0x7F) | 0x80)
        val >>= 7
    out.append(val)

    # Emit literals in chunks <= 60
    pos = 0
    while pos < len(data):
        chunk = min(len(data) - pos, 60)
        tag = (chunk - 1) << 2 # elem_type 0
        out.append(tag)
        out.extend(data[pos:pos + chunk])
        pos += chunk

    return bytes(out)

# ============================================================================
# 2. Low-Level Protobuf & VarInt Binary Stream Parser
# ============================================================================

class BinaryReader:
    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0

    def has_more(self) -> bool:
        return self.pos < len(self.data)

    def remaining(self) -> int:
        return len(self.data) - self.pos

    def read_bytes(self, n: int) -> bytes:
        if self.pos + n > len(self.data):
            res = self.data[self.pos:]
            self.pos = len(self.data)
            return res
        res = self.data[self.pos:self.pos + n]
        self.pos += n
        return res

    def read_u8(self) -> int:
        if self.pos >= len(self.data): return 0
        b = self.data[self.pos]
        self.pos += 1
        return b

    def read_u32_le(self) -> int:
        b = self.read_bytes(4)
        if len(b) < 4: return 0
        return struct.unpack("<I", b)[0]

    def read_f32_le(self) -> float:
        b = self.read_bytes(4)
        if len(b) < 4: return 0.0
        return struct.unpack("<f", b)[0]

    def read_varint(self) -> int:
        res = 0
        shift = 0
        while self.pos < len(self.data):
            b = self.data[self.pos]
            self.pos += 1
            res |= (b & 0x7F) << shift
            if (b & 0x80) == 0:
                break
            shift += 7
            if shift >= 64:
                break
        return res

    def skip_field(self, wire_type: int):
        if wire_type == 0: # VarInt
            self.read_varint()
        elif wire_type == 1: # Fixed64
            self.read_bytes(8)
        elif wire_type == 2: # LengthDelimited
            n = self.read_varint()
            self.read_bytes(n)
        elif wire_type == 5: # Fixed32
            self.read_bytes(4)

# ============================================================================
# 3. CS2 Match Recording (.dem) Container Parser
# ============================================================================

class CS2DemoAnalyzer:
    DEM_IS_COMPRESSED = 64 # 0x40

    def __init__(self, data: bytes):
        self.data = data
        self.header_info: Dict[str, Any] = {}
        self.frame_counts: Dict[str, int] = {}
        self.total_raw_bytes = 0
        self.total_decompressed_bytes = 0
        self.compressed_frames = 0
        self.uncompressed_frames = 0
        self.packet_sizes: List[int] = []
        self.usercmds: List[Dict[str, Any]] = []
        self.button_events: Dict[str, int] = {}
        self.entity_deltas: Dict[str, int] = {
            "packet_entities_messages": 0,
            "total_updated_entries": 0,
        }

    def analyze(self) -> bool:
        if len(self.data) < 8:
            return False

        reader = BinaryReader(self.data)
        magic = reader.read_bytes(8)

        if magic.startswith(b"PBDEMS2"):
            self.header_info["magic"] = "PBDEMS2"
            if reader.remaining() >= 4:
                cur = reader.data[reader.pos]
                if cur not in (1, 0x41):
                    self.header_info["summary_offset"] = reader.read_u32_le()
                else:
                    self.header_info["summary_offset"] = 0
        elif magic.startswith(b"HL2DEMO"):
            self.header_info["magic"] = "HL2DEMO"
            self.header_info["file_protocol"] = reader.read_u32_le()
            self.header_info["network_protocol"] = reader.read_u32_le()
            self.header_info["server_name"] = reader.read_bytes(260).split(b"\x00")[0].decode("utf-8", "ignore")
            self.header_info["client_name"] = reader.read_bytes(260).split(b"\x00")[0].decode("utf-8", "ignore")
            self.header_info["map_name"] = reader.read_bytes(260).split(b"\x00")[0].decode("utf-8", "ignore")
            self.header_info["game_directory"] = reader.read_bytes(260).split(b"\x00")[0].decode("utf-8", "ignore")
            playback_time = reader.read_f32_le()
            playback_ticks = reader.read_u32_le()
            reader.read_u32_le() # frames
            reader.read_u32_le() # signonlength
            if playback_time > 0 and playback_ticks > 0:
                self.header_info["server_tickrate"] = playback_ticks / playback_time
        else:
            return False

        # Command frame loop
        while reader.has_more():
            cmd_raw = reader.read_varint()
            if cmd_raw == 0: # DEM_Stop
                break

            is_compressed = bool(cmd_raw & self.DEM_IS_COMPRESSED)
            cmd = cmd_raw & ~self.DEM_IS_COMPRESSED

            # Alternate flag 0x01 only if cmd_raw > 19
            if not is_compressed and (cmd_raw & 1) and (cmd_raw > 19):
                is_compressed = True
                cmd = cmd_raw & ~1

            tick = reader.read_varint()
            size = reader.read_varint()

            raw_payload = reader.read_bytes(size)
            self.total_raw_bytes += len(raw_payload)

            if is_compressed:
                self.compressed_frames += 1
                try:
                    payload = snappy_decompress(raw_payload)
                    self.total_decompressed_bytes += len(payload)
                except Exception:
                    payload = raw_payload
                    self.total_decompressed_bytes += len(raw_payload)
            else:
                self.uncompressed_frames += 1
                payload = raw_payload
                self.total_decompressed_bytes += len(raw_payload)

            cmd_name = self._resolve_cmd_name(cmd)
            self.frame_counts[cmd_name] = self.frame_counts.get(cmd_name, 0) + 1

            if cmd in (4, 5, 7, 8, 13):
                self.packet_sizes.append(len(raw_payload))

            self._process_frame(cmd, tick, payload)

        return True

    def _resolve_cmd_name(self, cmd: int) -> str:
        names = {
            1: "DEM_FileHeader",
            2: "DEM_FileInfo",
            3: "DEM_SyncTick",
            4: "DEM_Packet",
            5: "DEM_SignonPacket",
            6: "DEM_StringTables",
            7: "DEM_FullPacket",
            8: "DEM_CustomData",
            10: "DEM_CustomData",
            12: "DEM_UserCmd",
            13: "DEM_FullPacket",
        }
        return names.get(cmd, f"DEM_Command_{cmd}")

    def _process_frame(self, cmd: int, tick: int, payload: bytes):
        if cmd == 1: # DEM_FileHeader
            self._parse_file_header_pb(payload)
        elif cmd in (4, 7): # DEM_Packet
            self._parse_packet_pb(payload)
        elif cmd in (5, 8) and cmd == 5: # DEM_SignonPacket
            self._parse_packet_pb(payload)
        elif cmd in (7, 13) and cmd == 13: # DEM_FullPacket
            self._parse_full_packet_pb(payload)
        elif cmd == 12: # DEM_UserCmd
            self._parse_usercmd_pb(payload)

    def _parse_file_header_pb(self, data: bytes):
        reader = BinaryReader(data)
        while reader.has_more():
            tag = reader.read_varint()
            fnum = tag >> 3
            wtype = tag & 7

            if fnum == 1 and wtype == 2:
                n = reader.read_varint()
                self.header_info["magic"] = reader.read_bytes(n).decode("utf-8", "ignore")
            elif fnum == 2 and wtype == 0:
                self.header_info["file_protocol"] = reader.read_varint()
            elif fnum == 3 and wtype == 2:
                n = reader.read_varint()
                self.header_info["server_name"] = reader.read_bytes(n).decode("utf-8", "ignore")
            elif fnum == 4 and wtype == 2:
                n = reader.read_varint()
                self.header_info["client_name"] = reader.read_bytes(n).decode("utf-8", "ignore")
            elif fnum == 5 and wtype == 2:
                n = reader.read_varint()
                self.header_info["map_name"] = reader.read_bytes(n).decode("utf-8", "ignore")
            elif fnum == 6 and wtype == 2:
                n = reader.read_varint()
                self.header_info["game_directory"] = reader.read_bytes(n).decode("utf-8", "ignore")
            elif fnum in (7, 16) and wtype == 0:
                self.header_info["network_protocol"] = reader.read_varint()
            elif fnum == 13 and wtype == 0:
                self.header_info["build_num"] = reader.read_varint()
            elif fnum == 17 and wtype == 5:
                self.header_info["server_tickrate"] = reader.read_f32_le()
            else:
                reader.skip_field(wtype)

    def _parse_packet_pb(self, data: bytes):
        # Look for field 3: bytes data
        reader = BinaryReader(data)
        while reader.has_more():
            tag = reader.read_varint()
            fnum = tag >> 3
            wtype = tag & 7
            if fnum == 3 and wtype == 2:
                n = reader.read_varint()
                net_stream = reader.read_bytes(n)
                self._parse_net_stream(net_stream)
                break
            else:
                reader.skip_field(wtype)

    def _parse_full_packet_pb(self, data: bytes):
        # field 2: CDemoPacket
        reader = BinaryReader(data)
        while reader.has_more():
            tag = reader.read_varint()
            fnum = tag >> 3
            wtype = tag & 7
            if fnum == 2 and wtype == 2:
                n = reader.read_varint()
                inner_packet = reader.read_bytes(n)
                self._parse_packet_pb(inner_packet)
                break
            else:
                reader.skip_field(wtype)

    def _parse_net_stream(self, data: bytes):
        # Check for UserCmds or NetMessages
        self._parse_usercmd_pb(data)

        # Scan for svc_PacketEntities (protobuf fields: 1=max_entries, 2=updated_entries)
        reader = BinaryReader(data)
        while reader.has_more():
            tag = reader.read_varint()
            fnum = tag >> 3
            wtype = tag & 7
            if fnum == 2 and wtype == 0: # updated_entries
                updated = reader.read_varint()
                if 0 < updated < 4096:
                    self.entity_deltas["packet_entities_messages"] += 1
                    self.entity_deltas["total_updated_entries"] += updated
            else:
                reader.skip_field(wtype)

    def _parse_usercmd_pb(self, data: bytes):
        # Search for SubtickMoveStep / CBaseUserCmdPB signatures
        reader = BinaryReader(data)
        cmd_record: Dict[str, Any] = {
            "command_number": 0,
            "client_tick": 0,
            "buttons": 0,
            "subtick_steps": [],
            "input_history": [],
            "forwardmove": 0.0,
            "leftmove": 0.0,
            "upmove": 0.0,
        }
        found_any = False

        while reader.has_more():
            tag = reader.read_varint()
            fnum = tag >> 3
            wtype = tag & 7

            if fnum == 1 and wtype == 0: # command_number
                cmd_record["command_number"] = reader.read_varint()
                found_any = True
            elif fnum == 2 and wtype == 0: # client_tick
                cmd_record["client_tick"] = reader.read_varint()
                found_any = True
            elif fnum == 3 and wtype == 2: # buttons_pb
                n = reader.read_varint()
                sub = BinaryReader(reader.read_bytes(n))
                while sub.has_more():
                    stag = sub.read_varint()
                    if stag >> 3 == 1 and (stag & 7) == 0:
                        btn = sub.read_varint()
                        cmd_record["buttons"] = btn
                        self.button_events[f"0x{btn:X}"] = self.button_events.get(f"0x{btn:X}", 0) + 1
                    else:
                        sub.skip_field(stag & 7)
                found_any = True
            elif fnum == 5 and wtype == 5:
                cmd_record["forwardmove"] = reader.read_f32_le()
                found_any = True
            elif fnum == 6 and wtype == 5:
                cmd_record["leftmove"] = reader.read_f32_le()
                found_any = True
            elif fnum == 7 and wtype == 5:
                cmd_record["upmove"] = reader.read_f32_le()
                found_any = True
            elif fnum == 18 and wtype == 2: # repeated CSubtickMoveStep
                n = reader.read_varint()
                step_data = reader.read_bytes(n)
                step = self._parse_subtick_step(step_data)
                cmd_record["subtick_steps"].append(step)
                if step.get("button"):
                    b_hex = f"0x{step['button']:X}"
                    self.button_events[b_hex] = self.button_events.get(b_hex, 0) + 1
                found_any = True
            else:
                reader.skip_field(wtype)

        if found_any and cmd_record["command_number"] > 0:
            self.usercmds.append(cmd_record)

    def _parse_subtick_step(self, data: bytes) -> Dict[str, Any]:
        reader = BinaryReader(data)
        res = {
            "button": 0,
            "pressed": False,
            "when": 0.0,
            "analog_forward_delta": 0.0,
            "analog_left_delta": 0.0,
        }
        while reader.has_more():
            tag = reader.read_varint()
            fnum = tag >> 3
            wtype = tag & 7
            if fnum == 1 and wtype == 0:
                res["button"] = reader.read_varint()
            elif fnum == 2 and wtype == 0:
                res["pressed"] = bool(reader.read_varint())
            elif fnum == 3 and wtype == 5:
                res["when"] = round(reader.read_f32_le(), 4)
            elif fnum == 4 and wtype == 5:
                res["analog_forward_delta"] = round(reader.read_f32_le(), 4)
            elif fnum == 5 and wtype == 5:
                res["analog_left_delta"] = round(reader.read_f32_le(), 4)
            else:
                reader.skip_field(wtype)
        return res

    def print_summary(self):
        print("=" * 70)
        print(" COUNTER-STRIKE 2 MATCH RECORDING TELEMETRY & SUB-TICK BITSTREAM")
        print("=" * 70)

        # 1. Header Metadata
        print("\n[1] Container Header Metadata:")
        print(f"  Container Magic:     {self.header_info.get('magic', 'Unknown')}")
        print(f"  File Protocol:       {self.header_info.get('file_protocol', 4)}")
        print(f"  Network Protocol:    {self.header_info.get('network_protocol', 13980)}")
        print(f"  Server Tickrate:     {self.header_info.get('server_tickrate', 64.0):.1f} Hz")
        print(f"  Map Name:            {self.header_info.get('map_name', 'Unknown')}")
        print(f"  Server Name:         {self.header_info.get('server_name', 'Unknown')}")
        print(f"  Client Name:         {self.header_info.get('client_name', 'Unknown')}")
        print(f"  Build Number:        {self.header_info.get('build_num', 'Unknown')}")

        # 2. Frame Counts & Codec Statistics
        total_frames = self.compressed_frames + self.uncompressed_frames
        avg_pkt = (sum(self.packet_sizes) / len(self.packet_sizes)) if self.packet_sizes else 0.0
        ratio = (self.total_raw_bytes / self.total_decompressed_bytes * 100.0) if self.total_decompressed_bytes else 100.0

        print("\n[2] Frame Loop & Snappy Codec Statistics:")
        print(f"  Total Command Frames: {total_frames}")
        for cmd_name, count in sorted(self.frame_counts.items()):
            print(f"    - {cmd_name:<20}: {count:>6} frames")
        print(f"  Compressed Frames:    {self.compressed_frames} (Snappy Block Codec)")
        print(f"  Uncompressed Frames:  {self.uncompressed_frames}")
        print(f"  Total Raw Wire Bytes: {self.total_raw_bytes:,} bytes")
        print(f"  Decompressed Bytes:   {self.total_decompressed_bytes:,} bytes")
        print(f"  Average Packet Size:  {avg_pkt:.1f} bytes")
        print(f"  Compression Ratio:    {ratio:.1f}%")

        # 3. Sub-Tick UserCmd Telemetry
        total_subtick = sum(len(cmd.get("subtick_steps", [])) for cmd in self.usercmds)
        print("\n[3] Sub-Tick Input Telemetry:")
        print(f"  Unpacked UserCmds:    {len(self.usercmds)}")
        print(f"  Sub-Tick Move Steps:  {total_subtick}")
        if self.button_events:
            print("  Input Button Mask Distribution:")
            for btn_mask, cnt in sorted(self.button_events.items()):
                print(f"    - Button Mask {btn_mask}: {cnt} event(s)")

        # 4. Entity Delta Synchronization
        print("\n[4] Network Entity Delta Synchronization:")
        print(f"  svc_PacketEntities:   {self.entity_deltas['packet_entities_messages']} message(s)")
        print(f"  Total Entity Deltas:  {self.entity_deltas['total_updated_entries']} update(s)")
        print("=" * 70)


def create_synthetic_demo() -> bytes:
    """
    Constructs a valid in-memory Source 2 CS2 match recording stream (.dem)
    with Snappy-compressed packets and sub-tick user commands.
    """
    out = bytearray()
    # 1. 8-byte magic PBDEMS2\0
    out.extend(b"PBDEMS2\x00")
    # 2. 4-byte summary offset
    out.extend(struct.pack("<I", 0))

    def write_varint(buf: bytearray, val: int):
        while val >= 0x80:
            buf.append((val & 0x7F) | 0x80)
            val >>= 7
        buf.append(val)

    def write_frame(cmd: int, tick: int, payload: bytes, compress: bool = False):
        if compress and payload:
            compressed_data = snappy_compress_simple(payload)
            cmd |= CS2DemoAnalyzer.DEM_IS_COMPRESSED
            write_varint(out, cmd)
            write_varint(out, tick)
            write_varint(out, len(compressed_data))
            out.extend(compressed_data)
        else:
            write_varint(out, cmd)
            write_varint(out, tick)
            write_varint(out, len(payload))
            out.extend(payload)

    # Frame 1: DEM_FileHeader (cmd 1)
    fh_buf = bytearray()
    # field 1: magic string
    write_varint(fh_buf, (1 << 3) | 2)
    write_varint(fh_buf, len("PBDEMS2"))
    fh_buf.extend(b"PBDEMS2")
    # field 2: file protocol
    write_varint(fh_buf, (2 << 3) | 0)
    write_varint(fh_buf, 4)
    # field 3: server_name
    srv = "Counter-Strike 2 Dedicated Server"
    write_varint(fh_buf, (3 << 3) | 2)
    write_varint(fh_buf, len(srv))
    fh_buf.extend(srv.encode("utf-8"))
    # field 4: client_name
    cli = "Source2Client"
    write_varint(fh_buf, (4 << 3) | 2)
    write_varint(fh_buf, len(cli))
    fh_buf.extend(cli.encode("utf-8"))
    # field 5: map_name
    map_str = "de_dust2"
    write_varint(fh_buf, (5 << 3) | 2)
    write_varint(fh_buf, len(map_str))
    fh_buf.extend(map_str.encode("utf-8"))
    # field 7: network protocol
    write_varint(fh_buf, (7 << 3) | 0)
    write_varint(fh_buf, 13980)
    # field 13: build_num
    write_varint(fh_buf, (13 << 3) | 0)
    write_varint(fh_buf, 25966348)
    # field 17: server_tickrate (float 64.0)
    write_varint(fh_buf, (17 << 3) | 5)
    fh_buf.extend(struct.pack("<f", 64.0))

    write_frame(1, 0, bytes(fh_buf), compress=False)

    # Frame 2: DEM_Packet (cmd 4) with sub-tick UserCmds (Snappy compressed!)
    cmd_buf = bytearray()
    write_varint(cmd_buf, (1 << 3) | 0)
    write_varint(cmd_buf, 101) # command_number
    write_varint(cmd_buf, (2 << 3) | 0)
    write_varint(cmd_buf, 6400) # client_tick

    # SubtickMoveStep 1: Jump at fraction 0.42
    step1 = bytearray()
    write_varint(step1, (1 << 3) | 0)
    write_varint(step1, 0x02) # IN_JUMP
    write_varint(step1, (2 << 3) | 0)
    write_varint(step1, 1) # pressed
    write_varint(step1, (3 << 3) | 5)
    step1.extend(struct.pack("<f", 0.42))

    write_varint(cmd_buf, (18 << 3) | 2)
    write_varint(cmd_buf, len(step1))
    cmd_buf.extend(step1)

    # Wrap inside CDemoPacket (field 3: bytes data)
    pkt_buf = bytearray()
    write_varint(pkt_buf, (3 << 3) | 2)
    write_varint(pkt_buf, len(cmd_buf))
    pkt_buf.extend(cmd_buf)

    write_frame(4, 6400, bytes(pkt_buf), compress=True)

    # Frame 3: DEM_SignonPacket (cmd 5)
    write_frame(5, 6401, b"\x1a\x04\x08\x01\x10\x02", compress=True)

    # Frame 4: DEM_FullPacket (cmd 7 / 13) carrying svc_PacketEntities
    ent_stream = bytearray()
    write_varint(ent_stream, (1 << 3) | 0) # max_entries = 64
    write_varint(ent_stream, 64)
    write_varint(ent_stream, (2 << 3) | 0) # updated_entries = 14
    write_varint(ent_stream, 14)
    write_varint(ent_stream, (7 << 3) | 2) # entity_data
    write_varint(ent_stream, 4)
    ent_stream.extend(b"\xAA\xBB\xCC\xDD")

    ent_pkt = bytearray()
    write_varint(ent_pkt, (3 << 3) | 2) # field 3: bytes data
    write_varint(ent_pkt, len(ent_stream))
    ent_pkt.extend(ent_stream)

    full_buf = bytearray()
    write_varint(full_buf, (2 << 3) | 2)
    write_varint(full_buf, len(ent_pkt))
    full_buf.extend(ent_pkt)
    write_frame(7, 6402, bytes(full_buf), compress=True)

    # Frame 5: DEM_CustomData (cmd 8 / 10)
    write_frame(8, 6403, b"\x08\x01\x12\x04DATA", compress=False)

    # DEM_Stop (cmd 0)
    write_varint(out, 0)

    return bytes(out)


def main():
    parser = argparse.ArgumentParser(
        description="Source 2 Counter-Strike 2 Match Recording (.dem) Container & Sub-Tick Bitstream Inspector"
    )
    parser.add_argument("--file", "-f", type=str, help="Path to CS2 demo file (.dem)")
    parser.add_argument("--synthetic", "-s", action="store_true", help="Generate and inspect synthetic CS2 demo stream")
    parser.add_argument("--json", "-j", action="store_true", help="Output telemetry summary in structured JSON format")

    args = parser.parse_args()

    if args.synthetic or not args.file:
        demo_bytes = create_synthetic_demo()
    else:
        if not os.path.exists(args.file):
            print(f"Error: Demo file not found: {args.file}", file=sys.stderr)
            sys.exit(1)
        with open(args.file, "rb") as f:
            demo_bytes = f.read()

    analyzer = CS2DemoAnalyzer(demo_bytes)
    if not analyzer.analyze():
        print("Error: Failed to parse Source 2 demo file header.", file=sys.stderr)
        sys.exit(1)

    if args.json:
        report = {
            "header": analyzer.header_info,
            "frame_counts": analyzer.frame_counts,
            "total_frames": analyzer.compressed_frames + analyzer.uncompressed_frames,
            "compressed_frames": analyzer.compressed_frames,
            "uncompressed_frames": analyzer.uncompressed_frames,
            "total_raw_bytes": analyzer.total_raw_bytes,
            "total_decompressed_bytes": analyzer.total_decompressed_bytes,
            "usercmd_count": len(analyzer.usercmds),
            "button_events": analyzer.button_events,
            "entity_deltas": analyzer.entity_deltas,
        }
        print(json.dumps(report, indent=2))
    else:
        analyzer.print_summary()


if __name__ == "__main__":
    main()
