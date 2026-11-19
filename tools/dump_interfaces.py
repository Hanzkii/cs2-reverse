#!/usr/bin/env python3
"""
tools/dump_interfaces.py - Source 2 CreateInterface & InterfaceReg Universal Extractor

Discovers and extracts all exported engine interface tokens, factory functions,
and version strings across all Source 2 modular binaries (client.dll, server.dll,
engine2.dll, tier0.dll, schemasystem.dll, etc.).

Mechanism:
  Every modular dynamic library in Source 2 exports CreateInterface to provide
  decoupled abstraction. During static initialization (.CRT$XCU), modules register
  their subsystems into a singly-linked list of InterfaceReg nodes:
    struct InterfaceReg {
        InstantiateInterfaceFn m_CreateFn; // +0x00 (Function pointer)
        const char*            m_pName;    // +0x08 (ASCII version token)
        InterfaceReg*          m_pNext;    // +0x10 (Next node in list)
    };

This tool operates 100% offline via standalone PE header and section parsing,
with zero external pip or runtime dependencies.
"""

import os
import sys
import json
import struct
import argparse
from pathlib import Path
from typing import Dict, List, Any, Optional

ROOT_DIR = Path(__file__).resolve().parent.parent

class Source2PEParser:
    """Minimal standalone 64-bit PE parser for InterfaceReg extraction."""
    def __init__(self, filepath: Path):
        self.filepath = filepath
        self.filename = filepath.name
        with open(filepath, "rb") as f:
            self.data = f.read()
        self.image_base = 0x180000000
        self.sections = []
        self._parse_headers()

    def _parse_headers(self):
        if len(self.data) < 0x40 or self.data[:2] != b"MZ":
            return
        e_lfanew = struct.unpack_from("<I", self.data, 0x3C)[0]
        if e_lfanew + 0x108 > len(self.data) or self.data[e_lfanew:e_lfanew+4] != b"PE\x00\x00":
            return
        num_sections = struct.unpack_from("<H", self.data, e_lfanew + 6)[0]
        opt_hdr_size = struct.unpack_from("<H", self.data, e_lfanew + 20)[0]
        opt_hdr_offset = e_lfanew + 24
        self.image_base = struct.unpack_from("<Q", self.data, opt_hdr_offset + 24)[0]

        sec_offset = opt_hdr_offset + opt_hdr_size
        for _ in range(num_sections):
            if sec_offset + 40 > len(self.data):
                break
            name = self.data[sec_offset:sec_offset+8].rstrip(b"\x00").decode("latin1", errors="ignore")
            vsize, vrva, rsize, rraw = struct.unpack_from("<IIII", self.data, sec_offset + 8)
            self.sections.append({
                "name": name,
                "rva": vrva,
                "vsize": vsize,
                "raw_offset": rraw,
                "raw_size": rsize
            })
            sec_offset += 40

    def rva_to_offset(self, rva: int) -> Optional[int]:
        for sec in self.sections:
            if sec["rva"] <= rva < sec["rva"] + max(sec["vsize"], sec["raw_size"]):
                return sec["raw_offset"] + (rva - sec["rva"])
        return None

    def offset_to_va(self, off: int) -> Optional[int]:
        for sec in self.sections:
            if sec["raw_offset"] <= off < sec["raw_offset"] + sec["raw_size"]:
                return self.image_base + sec["rva"] + (off - sec["raw_offset"])
        return None

    def va_to_offset(self, va: int) -> Optional[int]:
        if va < self.image_base:
            return None
        return self.rva_to_offset(va - self.image_base)

    def extract_interfaces(self) -> List[Dict[str, Any]]:
        """Walks InterfaceReg structures and scans for versioned interface tokens."""
        import re
        interfaces = []
        seen_tokens = set()

        # Token pattern: e.g. Source2EngineToClient001, VEngineCvar007, SchemaSystem_001
        pattern = re.compile(rb"([A-Za-z0-9_]{3,48}\d{3})\x00")
        for match in pattern.finditer(self.data):
            str_off = match.start(1)
            str_va = self.offset_to_va(str_off)
            if not str_va:
                continue
            token = match.group(1).decode("latin1", errors="ignore")
            if token in seen_tokens:
                continue

            # Look for 64-bit pointers to this string in .data / .rdata
            str_va_bytes = struct.pack("<Q", str_va)
            pos = 0
            while True:
                ptr_pos = self.data.find(str_va_bytes, pos)
                if ptr_pos == -1:
                    break
                reg_off = ptr_pos - 8
                if 0 <= reg_off and reg_off + 24 <= len(self.data):
                    fn_ptr, name_ptr, next_ptr = struct.unpack_from("<QQQ", self.data, reg_off)
                    if name_ptr == str_va and (fn_ptr >= self.image_base or fn_ptr == 0):
                        interfaces.append({
                            "token": token,
                            "fn_va": f"0x{fn_ptr:x}",
                            "binary": self.filename
                        })
                        seen_tokens.add(token)
                        break
                pos = ptr_pos + 8

        return sorted(interfaces, key=lambda x: x["token"])


def get_latest_archive_dir() -> Path:
    archive_root = ROOT_DIR / "versions" / "archive"
    if archive_root.exists():
        versions = sorted([p for p in archive_root.iterdir() if p.is_dir()])
        if versions:
            return versions[-1]
    fallback_game = ROOT_DIR / "game" / "bin" / "win64"
    if fallback_game.exists():
        return fallback_game
    return ROOT_DIR


def main():
    parser = argparse.ArgumentParser(description="Universal Source 2 CreateInterface & InterfaceReg Extractor")
    parser.add_argument("--archive-dir", type=Path, default=None,
                        help="Path to directory containing CS2 binaries (defaults to latest version archive)")
    parser.add_argument("--dll", type=Path, default=None,
                        help="Path to single PE binary to inspect")
    parser.add_argument("--output", "-o", type=Path, default=ROOT_DIR / "data" / "interfaces.json",
                        help="Path to output JSON file (default: data/interfaces.json)")
    args = parser.parse_args()

    target_dir = args.archive_dir or get_latest_archive_dir()

    print("=================================================================")
    print(" SOURCE 2 UNIVERSAL INTERFACE REGISTRY EXTRACTOR")
    print("=================================================================")

    if args.dll:
        files = [args.dll]
    else:
        print(f"[*] Scanning binaries in: {target_dir.name}")
        files = sorted([p for p in target_dir.iterdir() if p.suffix.lower() in (".dll", ".exe")])

    all_interfaces = []
    bin_counts = {}

    for fp in files:
        pe = Source2PEParser(fp)
        ifs = pe.extract_interfaces()
        if ifs:
            all_interfaces.extend(ifs)
            bin_counts[fp.name] = len(ifs)
            print(f"  [+] {fp.name:<28} -> {len(ifs):>4} interfaces")

    all_interfaces.sort(key=lambda x: (x["binary"], x["token"]))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(all_interfaces, f, indent=2)

    print("-----------------------------------------------------------------")
    print(f"[SUCCESS] Extracted {len(all_interfaces):,} total interfaces across {len(bin_counts)} binaries.")
    print(f"[+] Output written to: {args.output.name} ({os.path.getsize(args.output):,} bytes)")
    print("=================================================================")


if __name__ == "__main__":
    main()
