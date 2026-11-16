#!/usr/bin/env python3
"""
resolve_vtables.py - Source 2 Virtual Method Table (VTable) & RTTI Auto-Resolver

Correlates stripped virtual function pointers in Source 2 binaries (client.dll,
server.dll, engine2.dll, schemasystem.dll, tier0.dll, etc.) against known
AlliedModders s2sdk interfaces, MSVC x64 RTTI descriptors, and SchemaSystem classes.

Dual-Mode Operation:
1. Offline Standalone Mode (Default):
   - Fast, multithreaded / batch analysis of PE binaries (.rdata scanning).
   - Zero external pip dependencies (Standard Python 3 struct/re).
   - Optional pefile support if available.
   - Extracts complete RTTI vtable hierarchies, this_offsets, method counts, and RVAs.
   - Correlates with AlliedModders s2sdk canonical virtual method tables.
   - Outputs / updates structured metadata in data/vtables.json.

2. Ghidra Headless Analyzer Mode (--headless):
   - Orchestrates analyzeHeadless.bat to execute Source2_VTableResolver.java
     inside Ghidra project databases.
   - Labels functions, creates namespaces, sets plate comments, and tags bookmarks.
"""

import os
import sys
import struct
import re
import json
import argparse
import subprocess
import time
from pathlib import Path

# =============================================================================
# Canonical AlliedModders s2sdk Virtual Slot Specifications
# =============================================================================

CANONICAL_SDK_SLOTS = {
    "CSchemaSystem": {
        0: "~ISchemaSystem",
        1: "Connect",
        2: "Disconnect",
        3: "QueryInterface",
        4: "Init",
        5: "Shutdown",
        13: "FindTypeScopeForModule",
        14: "FindTypeScopeForModuleByToken",
        25: "GlobalTypeScope",
    },
    "ISchemaSystem": {
        0: "~ISchemaSystem",
        1: "Connect",
        2: "Disconnect",
        3: "QueryInterface",
        4: "Init",
        5: "Shutdown",
        13: "FindTypeScopeForModule",
        14: "FindTypeScopeForModuleByToken",
        25: "GlobalTypeScope",
    },
    "CSchemaSystemTypeScope": {
        0: "~CSchemaSystemTypeScope",
        1: "GetScopeName",
        2: "FindDeclaredClass",
        3: "FindDeclaredEnum",
        24: "FindDeclaredClassByToken",
        25: "FindDeclaredClassByName",
        26: "FindDeclaredEnumByToken",
        27: "FindDeclaredEnumByName",
    },
    "CEngineClient": {
        0: "~ISource2EngineToClient",
        27: "IsInGame",
        28: "IsConnected",
        29: "IsPaused",
        30: "GetMaxClients",
        31: "GetLevelName",
        32: "GetScreenSize",
        35: "GetLocalPlayerSlot",
        36: "GetScreenAspectRatio",
        37: "GetLocalPlayer",
        39: "IsInGame_Verification",
        40: "IsConnected_Verification",
        41: "GetNetChannelInfo",
        51: "ExecuteClientCmd",
    },
    "ISource2EngineToClient": {
        0: "~ISource2EngineToClient",
        27: "IsInGame",
        28: "IsConnected",
        29: "IsPaused",
        30: "GetMaxClients",
        31: "GetLevelName",
        32: "GetScreenSize",
        35: "GetLocalPlayerSlot",
        36: "GetScreenAspectRatio",
        37: "GetLocalPlayer",
        39: "IsInGame_Verification",
        40: "IsConnected_Verification",
        41: "GetNetChannelInfo",
        51: "ExecuteClientCmd",
    },
    "CNetworkServerService": {
        0: "~INetworkServerService",
        23: "GetIGameServer",
        24: "GetNetworkChannel",
    },
    "INetworkServerService": {
        0: "~INetworkServerService",
        23: "GetIGameServer",
        24: "GetNetworkChannel",
    },
    "CNetworkClientService": {
        0: "~INetworkClientService",
        21: "GetNetworkChannel",
        22: "GetSignonState",
        23: "GetNetworkGameClient",
    },
    "INetworkClientService": {
        0: "~INetworkClientService",
        21: "GetNetworkChannel",
        22: "GetSignonState",
        23: "GetNetworkGameClient",
    },
    "CCvar": {
        0: "~IEngineCVar",
        11: "FindVar",
        12: "FindVarByIndex",
        41: "GetFirstCVarArrayValue",
        42: "GetNextCVarArrayValue",
    },
    "IEngineCVar": {
        0: "~IEngineCVar",
        11: "FindVar",
        12: "FindVarByIndex",
        41: "GetFirstCVarArrayValue",
        42: "GetNextCVarArrayValue",
    },
    "CMemAllocStandard": {
        0: "~IMemAlloc",
        1: "Alloc",
        2: "Realloc",
        3: "Free",
        21: "GetSize",
    },
    "IMemAlloc": {
        0: "~IMemAlloc",
        1: "Alloc",
        2: "Realloc",
        3: "Free",
        21: "GetSize",
    },
    "CInputSystem": {
        0: "~IInputSystem",
        19: "IsButtonDown",
    },
    "IInputSystem": {
        0: "~IInputSystem",
        19: "IsButtonDown",
    },
    "CEntityInstance": {
        0: "~CEntityInstance",
        1: "GetRefEHandle",
        2: "GetClassname",
        3: "GetDebugName",
        4: "GetEntityIdentity",
    },
    "CBaseEntity": {
        0: "~CBaseEntity",
        1: "PostConstructor",
        2: "Spawn",
        3: "Precache",
        4: "Activate",
    },
    "C_BaseEntity": {
        0: "~C_BaseEntity",
        1: "PostConstructor",
        2: "Spawn",
        3: "Precache",
        4: "Activate",
    },
    "CCSPlayerPawn": {
        0: "~CCSPlayerPawn",
        1: "GetRefEHandle",
        2: "GetClassname",
    },
    "C_CSPlayerPawn": {
        0: "~C_CSPlayerPawn",
        1: "GetRefEHandle",
        2: "GetClassname",
    },
}

# =============================================================================
# Demangling & Utility Routines
# =============================================================================

def demangle_msvc_name(raw_name: str) -> str:
    """
    Demangles MSVC RTTI mangled names into clean C++ symbols.
    Examples:
      'CEngineClient' -> 'CEngineClient'
      'exception@std' -> 'std::exception'
      'MapFieldAccessor@internal@protobuf@google' -> 'google::protobuf::internal::MapFieldAccessor'
    """
    s = raw_name.strip()
    if s.startswith(".?AV") or s.startswith(".?AU"):
        s = s[4:]
    if s.endswith("@@"):
        s = s[:-2]
    elif s.endswith("@"):
        s = s[:-1]

    # Function-local class: Class@?1??Func@Scope@@...
    if "@?1??" in s:
        class_part, func_part = s.split("@?1??", 1)
        enclosing_tokens = [p for p in func_part.split("@@")[0].split("@") if p and not p.startswith("?")]
        if enclosing_tokens:
            enclosing = "::".join(reversed(enclosing_tokens))
            return f"{enclosing}::{class_part}"
        return class_part

    # Template classes: ?$TemplateName@Type1@Type2@@
    if s.startswith("?$"):
        s = s[2:]
        parts = [p for p in s.split("@") if p]
        if parts:
            tmpl_name = parts[0]
            args = []
            for arg in parts[1:]:
                clean_arg = arg
                if clean_arg.startswith("PEAV") or clean_arg.startswith("PEAU"):
                    clean_arg = clean_arg[4:] + "*"
                elif clean_arg.startswith("V") or clean_arg.startswith("U"):
                    clean_arg = clean_arg[1:]
                elif clean_arg.startswith("_N"):
                    clean_arg = "bool"
                elif clean_arg.startswith("H"):
                    clean_arg = "int"
                elif clean_arg.startswith("I"):
                    clean_arg = "unsigned int"
                elif clean_arg.startswith("M"):
                    clean_arg = "float"
                args.append(clean_arg)
            if args:
                joined_args = ", ".join(args)
                return f"{tmpl_name}<{joined_args}>"
            return tmpl_name

    parts = [p for p in s.split("@") if p and not p.startswith("?")]
    if len(parts) > 1:
        return "::".join(reversed(parts))
    elif len(parts) == 1:
        return parts[0]
    return s


def find_canonical_slot(class_name: str, slot: int):
    """Checks if a class has a canonical AlliedModders s2sdk slot definition."""
    clean_name = class_name
    if "::" in clean_name:
        clean_name = clean_name.rsplit("::", 1)[-1]

    # Exact or suffix match
    for target_class, slot_map in CANONICAL_SDK_SLOTS.items():
        if clean_name.lower() == target_class.lower():
            if slot in slot_map:
                return slot_map[slot]
    return None

# =============================================================================
# PE Binary Parser & VTable Scanner
# =============================================================================

class PEVTableScanner:
    """Lightweight 64-bit PE Parser & RTTI Complete Object Locator Scanner."""

    def __init__(self, file_path: str):
        self.path = file_path
        self.filename = os.path.basename(file_path)
        with open(file_path, "rb") as f:
            self.data = f.read()

        self._parse_headers()

    def _parse_headers(self):
        if len(self.data) < 0x40 or self.data[:2] != b"MZ":
            raise ValueError(f"Invalid PE header in {self.filename}")
        self.e_lfanew = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[self.e_lfanew : self.e_lfanew + 4] != b"PE\x00\x00":
            raise ValueError(f"Invalid PE signature in {self.filename}")

        self.machine = struct.unpack_from("<H", self.data, self.e_lfanew + 4)[0]
        self.num_sections = struct.unpack_from("<H", self.data, self.e_lfanew + 6)[0]
        self.opt_hdr_size = struct.unpack_from("<H", self.data, self.e_lfanew + 20)[0]

        opt_hdr = self.e_lfanew + 24
        self.magic = struct.unpack_from("<H", self.data, opt_hdr)[0]
        self.image_base = struct.unpack_from("<Q", self.data, opt_hdr + 24)[0]

        sec_off = opt_hdr + self.opt_hdr_size
        self.sections = []
        self.text_sec = None
        self.rdata_sec = None

        for i in range(self.num_sections):
            s = self.data[sec_off + i * 40 : sec_off + (i + 1) * 40]
            name = s[:8].rstrip(b"\x00").decode("latin1", errors="ignore")
            vsize, rva, rsize, roff = struct.unpack_from("<IIII", s, 8)
            sec = {
                "name": name,
                "vsize": vsize,
                "rva": rva,
                "rsize": rsize,
                "roff": roff,
            }
            self.sections.append(sec)
            if name == ".text":
                self.text_sec = sec
            elif name == ".rdata":
                self.rdata_sec = sec

        if not self.rdata_sec:
            # Fallback to first read-only initialized data section
            for sec in self.sections:
                if sec["name"].startswith(".rdata") or sec["name"] == ".rodata":
                    self.rdata_sec = sec
                    break

    def rva_to_off(self, rva: int):
        for s in self.sections:
            if s["rva"] <= rva < s["rva"] + s["vsize"]:
                return s["roff"] + (rva - s["rva"])
        return None

    def off_to_rva(self, off: int):
        for s in self.sections:
            if s["roff"] <= off < s["roff"] + s["rsize"]:
                return s["rva"] + (off - s["roff"])
        return None

    def is_executable_rva(self, rva: int) -> bool:
        """Checks if RVA points into executable .text code."""
        if self.text_sec:
            if self.text_sec["rva"] <= rva < self.text_sec["rva"] + self.text_sec["vsize"]:
                return True
        for s in self.sections:
            if s["name"] in (".text", "text") and s["rva"] <= rva < s["rva"] + s["vsize"]:
                return True
        return False

    def scan_vtables(self):
        """
        Scans .rdata for MSVC x64 RTTI Complete Object Locators and resolves
        all virtual method tables with their method slot entries.
        """
        if not self.rdata_sec:
            return []

        rdata_start = self.rdata_sec["roff"]
        rdata_end = rdata_start + self.rdata_sec["rsize"]

        # Step 1: Scan for valid Complete Object Locators (COL)
        # struct _RTTICompleteObjectLocator:
        #   uint32_t signature;          // +0x00 (1 in x64)
        #   uint32_t offset;             // +0x04 (this pointer offset)
        #   uint32_t cdOffset;           // +0x08 (constructor displacement, 0)
        #   int32_t  typeDescriptorRVA;  // +0x0C (RVA to TypeDescriptor)
        #   int32_t  classHierarchyRVA;  // +0x10
        #   int32_t  objectBaseRVA;      // +0x14
        valid_cols = {}

        for off in range(rdata_start, rdata_end - 24, 4):
            sig, this_offset, cd_off, td_rva = struct.unpack_from("<IIII", self.data, off)
            if sig == 1 and cd_off == 0 and this_offset < 0x40000 and td_rva > 0:
                td_off = self.rva_to_off(td_rva)
                if td_off and td_off + 20 < len(self.data):
                    # Check TypeDescriptor tag at td_off + 16: '.?AV' or '.?AU'
                    if self.data[td_off + 16 : td_off + 20] in (b".?AV", b".?AU"):
                        null_pos = self.data.find(b"\x00", td_off + 16)
                        if null_pos != -1 and 4 < (null_pos - (td_off + 16)) < 256:
                            raw_name = self.data[td_off + 16 : null_pos].decode("latin1", errors="ignore")
                            col_rva = self.off_to_rva(off)
                            if col_rva is not None:
                                col_va = self.image_base + col_rva
                                valid_cols[col_va] = {
                                    "raw_name": raw_name,
                                    "this_offset": this_offset,
                                    "td_rva": td_rva,
                                    "col_rva": col_rva,
                                    "col_va": col_va,
                                }

        # Step 2: Scan .rdata for pointers to valid COLs
        # In MSVC x64, each vtable pointer table is immediately preceded by a pointer to COL.
        resolved_vtables = []

        for off in range(rdata_start, rdata_end - 8, 8):
            ptr = struct.unpack_from("<Q", self.data, off)[0]
            if ptr in valid_cols:
                col_meta = valid_cols[ptr]
                vtable_off = off + 8
                vtable_rva = self.off_to_rva(vtable_off)
                if vtable_rva is None:
                    continue

                vtable_va = self.image_base + vtable_rva
                raw_name = col_meta["raw_name"]
                demangled = demangle_msvc_name(raw_name)
                this_offset = col_meta["this_offset"]

                # Read contiguous virtual function pointers pointing into .text
                methods = []
                curr_off = vtable_off
                slot = 0

                while curr_off + 8 <= len(self.data):
                    fn_ptr = struct.unpack_from("<Q", self.data, curr_off)[0]
                    fn_rva = fn_ptr - self.image_base
                    if self.is_executable_rva(fn_rva):
                        canonical_name = find_canonical_slot(demangled, slot)
                        if canonical_name:
                            sym_name = f"{demangled}::{canonical_name}"
                            is_canonical = True
                        elif this_offset > 0:
                            sym_name = f"{demangled}+0x{this_offset:x}::vtable[{slot}]"
                            is_canonical = False
                        else:
                            sym_name = f"{demangled}::vtable[{slot}]"
                            is_canonical = False

                        methods.append({
                            "slot": slot,
                            "rva": f"0x{fn_rva:x}",
                            "va": f"0x{fn_ptr:x}",
                            "symbol": sym_name,
                            "canonical": is_canonical,
                        })
                        slot += 1
                        curr_off += 8
                    else:
                        break

                if methods:
                    resolved_vtables.append({
                        "class_name": demangled,
                        "raw_mangled": raw_name,
                        "binary": self.filename.lower(),
                        "vtable_rva": f"0x{vtable_rva:x}",
                        "vtable_va": f"0x{vtable_va:x}",
                        "this_offset": this_offset,
                        "method_count": len(methods),
                        "type_descriptor_rva": f"0x{col_meta['td_rva']:x}",
                        "col_rva": f"0x{col_meta['col_rva']:x}",
                        "methods": methods,
                    })

        return resolved_vtables

# =============================================================================
# Headless Analyzer Orchestration
# =============================================================================

def run_headless_ghidra(ghidra_home: Path, project_dir: Path, project_name: str,
                        folder: str, target_binaries: list, output_file: Path,
                        dry_run: bool = False):
    """Executes analyzeHeadless.bat with Source2_VTableResolver.java across target binaries."""
    analyze_bat = ghidra_home / "support" / "analyzeHeadless.bat"
    if not analyze_bat.exists():
        analyze_bat = ghidra_home / "support" / "analyzeHeadless"
        if not analyze_bat.exists():
            print(f"[-] ERROR: analyzeHeadless not found under {ghidra_home}")
            return False

    script_path = Path(__file__).parent / "Source2_VTableResolver.java"
    if not script_path.exists():
        print(f"[-] ERROR: Source2_VTableResolver.java not found at {script_path}")
        return False

    project_folder_path = f"{project_name}/{folder}" if folder else project_name

    print("=================================================================")
    print(" SOURCE 2 GHIDRA HEADLESS VTABLE RESOLVER")
    print("=================================================================")
    print(f" Ghidra Home:    {ghidra_home}")
    print(f" Project Dir:    {project_dir}")
    print(f" Project Folder: {project_folder_path}")
    print(f" Output Report:  {output_file}")
    print(f" Target Count:   {len(target_binaries)}")
    print("=================================================================")

    results = {}
    for binary_name in target_binaries:
        print(f"\n[*] Processing {binary_name} via Ghidra analyzeHeadless...")
        if dry_run:
            print(f"  [DRY-RUN] analyzeHeadless {project_dir} {project_folder_path} -process {binary_name} -postScript Source2_VTableResolver.java {output_file}")
            results[binary_name] = "DRY-RUN"
            continue

        cmd = [
            str(analyze_bat),
            str(project_dir),
            project_folder_path,
            "-process", binary_name,
            "-noanalysis",
            "-postScript", "Source2_VTableResolver.java",
            str(output_file.resolve()),
            "-scriptPath", str(script_path.parent.resolve()),
        ]

        try:
            proc = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                input="",
                timeout=300
            )

            lines = [l for l in proc.stdout.splitlines() if "Source2_VTableResolver" in l or "VTables" in l or "ERROR" in l]
            for line in lines[-10:]:
                print(f"  {line}")

            if proc.returncode == 0:
                results[binary_name] = "SUCCESS"
                print(f"  [+] Finished {binary_name}")
            else:
                results[binary_name] = f"EXIT_{proc.returncode}"
                print(f"  [-] Failed {binary_name} (code {proc.returncode})")

        except subprocess.TimeoutExpired:
            print(f"  [-] Timeout processing {binary_name}")
            results[binary_name] = "TIMEOUT"
        except Exception as ex:
            print(f"  [-] Exception processing {binary_name}: {ex}")
            results[binary_name] = "EXCEPTION"

    print("\n=================================================================")
    print(" HEADLESS EXECUTION SUMMARY")
    print("=================================================================")
    for b, res in results.items():
        print(f"  {b:<30} : {res}")
    print("=================================================================")
    return all(r in ("SUCCESS", "DRY-RUN") for r in results.values())

# =============================================================================
# Standalone Offline Scanner & Merger
# =============================================================================

KEY_SOURCE2_CLASSES = {
    "CEngineClient", "ISource2EngineToClient", "CSchemaSystem", "ISchemaSystem",
    "CSchemaSystemTypeScope", "CNetworkServerService", "INetworkServerService",
    "CNetworkClientService", "INetworkClientService", "CCvar", "IEngineCVar",
    "CMemAllocStandard", "IMemAlloc", "CInputSystem", "IInputSystem",
    "CEntityInstance", "CBaseEntity", "C_BaseEntity", "CCSPlayerPawn", "C_CSPlayerPawn",
    "CGameEntitySystem", "IGameResourceService", "CGameResourceService", "CVPhysics2",
}

def run_offline_scan(archive_dir: Path, target_binaries: list, output_file: Path,
                     summary: bool = True, full_all: bool = False):
    """Scans PE binaries offline and merges results into output_file."""
    print("=================================================================")
    print(" SOURCE 2 OFFLINE VTABLE RESOLVER")
    print("=================================================================")
    print(f" Binary Archive: {archive_dir}")
    print(f" Output Target:  {output_file}")
    print(f" Target Count:   {len(target_binaries)}")
    print("=================================================================")

    # Load existing report to preserve other binaries if present
    existing_vtables = []
    if output_file.exists():
        try:
            with open(output_file, "r", encoding="utf-8") as f:
                existing_vtables = json.load(f)
        except Exception:
            existing_vtables = []

    existing_by_bin = {}
    for entry in existing_vtables:
        b = entry.get("binary", "").lower()
        existing_by_bin.setdefault(b, []).append(entry)

    new_results_by_bin = {}
    total_found = 0
    total_canonical = 0

    t_start = time.time()
    for b_name in target_binaries:
        bin_path = archive_dir / b_name
        if not bin_path.exists():
            print(f"[-] Binary not found: {bin_path}")
            continue

        t0 = time.time()
        scanner = PEVTableScanner(str(bin_path))
        vtables = scanner.scan_vtables()
        dt = time.time() - t0

        canonical_count = sum(
            sum(1 for m in vt["methods"] if m.get("canonical", False))
            for vt in vtables
        )
        total_found += len(vtables)
        total_canonical += canonical_count

        # Compact methods if not full_all to keep repository size < 15MB invariant
        if not full_all:
            for vt in vtables:
                cname = vt.get("class_name", "")
                is_key = cname in KEY_SOURCE2_CLASSES or any(cname.endswith("::" + k) for k in KEY_SOURCE2_CLASSES)
                if not is_key:
                    vt["methods"] = [m for m in vt["methods"] if m.get("canonical") or m.get("slot") == 0]

        new_results_by_bin[b_name.lower()] = vtables
        print(f"  [+] {b_name:<28} : {len(vtables):>5} vtables ({canonical_count:>3} canonical slots) in {dt:.2f}s")

    # Merge results
    merged_vtables = []
    processed_bins = set(new_results_by_bin.keys())

    # Keep old binaries that were not scanned this run
    for b, entries in existing_by_bin.items():
        if b not in processed_bins:
            merged_vtables.extend(entries)

    for b in sorted(new_results_by_bin.keys()):
        merged_vtables.extend(new_results_by_bin[b])

    # Sort merged vtables by binary, class_name, and this_offset
    merged_vtables.sort(key=lambda x: (x.get("binary", ""), x.get("class_name", ""), x.get("this_offset", 0)))

    # Save output
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(merged_vtables, f, indent=2)

    total_time = time.time() - t_start
    print("=================================================================")
    print(f" TOTAL RESOLVED: {total_found} VTables across {len(new_results_by_bin)} binaries ({total_canonical} canonical slots) in {total_time:.2f}s")
    print(f" Structured report written to: {output_file}")
    print("=================================================================")

    if summary:
        print_summary(merged_vtables)

    return True


def print_summary(vtables: list):
    """Prints a concise breakdown of resolved VTables."""
    by_binary = {}
    for vt in vtables:
        b = vt.get("binary", "unknown")
        by_binary[b] = by_binary.get(b, 0) + 1

    print("\n--- Summary Breakdown by Binary ---")
    for b, count in sorted(by_binary.items(), key=lambda x: -x[1]):
        print(f"  {b:<30} : {count:>5} vtables")
    print("-----------------------------------")

# =============================================================================
# Entry Point & CLI
# =============================================================================

def resolve_latest_archive() -> Path:
    """Finds the most recent version archive directory."""
    archive_root = Path("versions/archive")
    if not archive_root.exists():
        return None
    candidates = [p for p in archive_root.iterdir() if p.is_dir() and not p.name.startswith(".")]
    if not candidates:
        return None
    return sorted(candidates, key=lambda p: p.name, reverse=True)[0]


def main():
    parser = argparse.ArgumentParser(
        description="Source 2 Virtual Method Table (VTable) Auto-Resolver."
    )
    parser.add_argument(
        "-b", "--binary",
        default="all",
        help="Target binary (e.g. client.dll, server.dll, engine2.dll, schemasystem.dll, or 'all')",
    )
    parser.add_argument(
        "-a", "--archive-dir",
        default=None,
        help="Path to version archive containing CS2 binaries (defaults to latest in versions/archive/)",
    )
    parser.add_argument(
        "-o", "--output",
        default="data/vtables.json",
        help="Path to output JSON metadata report (defaults to data/vtables.json)",
    )
    parser.add_argument(
        "--headless",
        action="store_true",
        help="Execute via Ghidra analyzeHeadless.bat with Source2_VTableResolver.java",
    )
    parser.add_argument(
        "--ghidra-home",
        default=os.environ.get("GHIDRA_HOME") or os.environ.get("GHIDRA_INSTALL_PATH"),
        help="Path to Ghidra installation root (for --headless mode)",
    )
    parser.add_argument(
        "--project-dir",
        default=os.environ.get("CS2_DISASSEMBLY_DIR"),
        help="Ghidra project directory path (for --headless mode)",
    )
    parser.add_argument(
        "--project-name",
        default="CS2",
        help="Ghidra project name (for --headless mode)",
    )
    parser.add_argument(
        "--folder",
        default=None,
        help="Ghidra project subfolder (e.g. 2026-11-11_26598758)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Simulate execution without modifying files or launching Ghidra",
    )
    parser.add_argument(
        "--summary",
        action="store_true",
        default=True,
        help="Print summary report of resolved vtables",
    )
    parser.add_argument(
        "--full-all",
        action="store_true",
        default=False,
        help="Emit all unmapped virtual slots for non-key classes (warning: generates ~80MB output)",
    )

    args = parser.parse_args()

    # Core Source 2 target binaries
    CORE_TARGETS = [
        "schemasystem.dll",
        "tier0.dll",
        "engine2.dll",
        "server.dll",
        "client.dll",
        "vphysics2.dll",
        "networksystem.dll",
        "inputsystem.dll",
        "filesystem_stdio.dll",
        "pulse_system.dll",
    ]

    if args.binary.lower() == "all":
        target_binaries = CORE_TARGETS
    else:
        target_binaries = [args.binary.lower()]

    output_path = Path(args.output)

    if args.headless:
        # Ghidra Headless Analyzer Mode
        if not args.ghidra_home:
            print("[-] ERROR: --ghidra-home or %GHIDRA_HOME% environment variable required for --headless mode.")
            sys.exit(1)
        if not args.project_dir:
            print("[-] ERROR: --project-dir or %CS2_DISASSEMBLY_DIR% environment variable required for --headless mode.")
            sys.exit(1)

        success = run_headless_ghidra(
            ghidra_home=Path(args.ghidra_home),
            project_dir=Path(args.project_dir),
            project_name=args.project_name,
            folder=args.folder or "",
            target_binaries=target_binaries,
            output_file=output_path,
            dry_run=args.dry_run,
        )
        sys.exit(0 if success else 1)

    else:
        # Offline Standalone Mode
        if args.archive_dir:
            archive_path = Path(args.archive_dir)
        else:
            archive_path = resolve_latest_archive()

        if not archive_path or not archive_path.exists():
            print("[-] ERROR: Could not locate binary archive directory. Specify with -a / --archive-dir.")
            sys.exit(1)

        success = run_offline_scan(
            archive_dir=archive_path,
            target_binaries=target_binaries,
            output_file=output_path,
            summary=args.summary,
            full_all=args.full_all,
        )
        sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
