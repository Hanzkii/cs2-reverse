#!/usr/bin/env python3
"""
cs2_ghidra_toolkit.py - Source 2 Ghidra Reverse Engineering Master Toolkit

A unified command-line orchestrator that extends GhidraMCP with advanced static analysis:
  1. status           - Inspect active Ghidra program, memory blocks, and symbols.
  2. inject-types     - Inject Source 2 data types into Ghidra's DataTypeManager.
  3. sync-prototypes  - Apply typed C++ prototypes and calling conventions.
  4. scan-rtti        - Discover and annotate MSVC x64 polymorphic classes and VTables.
  5. trace-xrefs      - Deep cross-reference (caller/callee/string) tracer.
  6. full-sync        - Run complete end-to-end type, prototype, and VTable synchronization.
"""

import os
import sys
import json
import argparse
import urllib.request
import urllib.parse
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple

# Ensure repository root is in sys.path
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

DEFAULT_PORT = 8089
DEFAULT_VERSION_FOLDER = "/2026-11-11_26598758"

ALL_CORE_BINARIES = [
    "client.dll",
    "server.dll",
    "engine2.dll",
    "schemasystem.dll",
    "vphysics2.dll",
    "networksystem.dll",
    "inputsystem.dll",
    "tier0.dll",
    "animationsystem.dll",
    "rendersystemdx11.dll",
    "pulse_system.dll",
    "steamnetworkingsockets.dll",
    "navsystem.dll",
]


class GhidraClient:
    def __init__(self, host="127.0.0.1", port=DEFAULT_PORT):
        self.base_url = f"http://{host}:{port}"

    def check_connection(self) -> Tuple[bool, str]:
        url = f"{self.base_url}/check_connection"
        try:
            with urllib.request.urlopen(url, timeout=5) as res:
                body = res.read().decode("utf-8", errors="replace")
                return (res.status == 200, body.strip())
        except Exception as ex:
            return (False, str(ex))

    def get_json(self, endpoint: str, params: Optional[dict] = None, timeout=30) -> dict:
        url = f"{self.base_url}{endpoint}"
        if params:
            url = f"{url}?{urllib.parse.urlencode(params)}"
        try:
            with urllib.request.urlopen(url, timeout=timeout) as res:
                return json.loads(res.read().decode("utf-8", errors="replace"))
        except Exception as ex:
            return {"error": str(ex)}

    def post_json(self, endpoint: str, data_dict: dict, timeout=60) -> dict:
        url = f"{self.base_url}{endpoint}"
        payload = json.dumps(data_dict).encode("utf-8")
        req = urllib.request.Request(url, data=payload, headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=timeout) as res:
                text = res.read().decode("utf-8", errors="replace")
                try:
                    return json.loads(text)
                except Exception:
                    return {"result": text}
        except Exception as ex:
            return {"error": str(ex)}

    def run_inline_script(self, java_code: str, program: str = "") -> dict:
        payload = {"code": java_code}
        if program:
            payload["program"] = program
        return self.post_json("/run_script_inline", payload, timeout=120)

    def set_prototype(self, address: str, prototype: str, convention: str = "__fastcall", program: str = "") -> dict:
        addr = address if address.startswith("0x") else f"0x{address}"
        payload = {
            "function_address": addr,
            "prototype": prototype,
            "calling_convention": convention,
        }
        if program:
            payload["program"] = program
        return self.post_json("/set_function_prototype", payload)

    def rename_function_by_address(self, address: str, new_name: str, program: str = "") -> dict:
        addr = address if address.startswith("0x") else f"0x{address}"
        payload = {"function_address": addr, "new_name": new_name}
        if program:
            payload["program"] = program
        return self.post_json("/rename_function_by_address", payload)

    def open_program(self, path: str) -> dict:
        url = f"{self.base_url}/open_program?{urllib.parse.urlencode({'path': path})}"
        req = urllib.request.Request(url, data=b"", headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=45) as res:
                return json.loads(res.read().decode("utf-8", errors="replace"))
        except Exception as ex:
            return {"error": str(ex)}

    def save_program(self, program_name: str) -> dict:
        url = f"{self.base_url}/save_program?{urllib.parse.urlencode({'program': program_name})}"
        try:
            with urllib.request.urlopen(url, timeout=30) as res:
                return json.loads(res.read().decode("utf-8", errors="replace"))
        except Exception as ex:
            return {"error": str(ex)}


def cmd_status(client: GhidraClient):
    print("==========================================================================================")
    print("  GHIDRA MCP BRIDGE STATUS REPORT")
    print("==========================================================================================")
    conn_ok, conn_msg = client.check_connection()
    if not conn_ok:
        print(f"[-] Status: Offline ({conn_msg})")
        print("    Ensure Ghidra is running with the GhidraMCP plugin active on port 8089.")
        return

    print(f"[+] Status: ONLINE ({conn_msg})")
    info = client.get_json("/get_current_program_info")
    if "error" in info:
        print(f"[-] Program info error: {info['error']}")
        return

    print(f"  Program Name:      {info.get('name')}")
    print(f"  Project Path:      {info.get('path')}")
    print(f"  Architecture:      {info.get('language')} ({info.get('compiler')})")
    print(f"  Image Base:        0x{info.get('image_base')}")
    print(f"  Address Range:     0x{info.get('min_address')} - 0x{info.get('max_address')}")
    print(f"  Memory Size:       {info.get('memory_size'):,} bytes ({info.get('memory_block_count')} blocks)")
    print(f"  Function Count:    {info.get('function_count'):,}")
    print(f"  Symbol Count:      {info.get('symbol_count'):,}")
    print(f"  Data Type Count:   {info.get('data_type_count'):,}")
    print("==========================================================================================\n")


def cmd_full_sync(client: GhidraClient, binaries: List[str], folder: str = DEFAULT_VERSION_FOLDER):
    print("\n==========================================================================================")
    print("  EXECUTING SOURCE 2 FULL-BINARY AUTOMATION & GHIDRA SYNC")
    print("==========================================================================================")

    from tools.ghidra.inject_source2_types import inject_types_into_binary
    from tools.ghidra.sync_function_prototypes import sync_prototypes_for_binary, find_repo_root
    from tools.ghidra.scan_rtti_vtables import scan_binary_rtti

    repo_root = find_repo_root()
    sigs_path = repo_root / "data" / "function_signatures.json"
    signatures = []
    if sigs_path.exists():
        with open(sigs_path, "r", encoding="utf-8") as f:
            signatures = json.load(f)

    by_binary: Dict[str, List[dict]] = {}
    for s in signatures:
        b = s.get("binary")
        if b:
            by_binary.setdefault(b, []).append(s)

    for b in binaries:
        print(f"\n>>> PROCESSING: {b}")
        # 1. Open
        full_path = f"{folder}/{b}"
        open_res = client.open_program(full_path)
        if "error" in open_res:
            print(f"  [-] Skip {b}: {open_res['error']}")
            continue

        # 2. Inject Types
        inject_types_into_binary(client, b, folder)

        # 3. Prototypes
        if b in by_binary:
            sync_prototypes_for_binary(client, b, by_binary[b], folder)

        # 4. RTTI Scanner & Annotate
        scan_binary_rtti(client, b, annotate=True, max_classes=300, folder=folder)

        # 5. Save
        client.save_program(b)
        print(f"  [+] Saved {b} in Ghidra database.\n")

    print("\n[+] Full Synchronization Completed Successfully!\n")


def main():
    parser = argparse.ArgumentParser(description="Source 2 Ghidra Reverse Engineering Master Toolkit")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="GhidraMCP HTTP port (default: 8089)")
    parser.add_argument("--folder", type=str, default=DEFAULT_VERSION_FOLDER, help="Project version folder")

    subparsers = parser.add_subparsers(dest="command", help="Toolkit Command")

    # status
    subparsers.add_parser("status", help="Inspect active Ghidra program info and connection")

    # inject-types
    p_inject = subparsers.add_parser("inject-types", help="Inject Source 2 core data types into DataTypeManager")
    p_inject.add_argument("--binary", type=str, default="vphysics2.dll", help="Binary to process")
    p_inject.add_argument("--all", action="store_true", help="Process all core binaries")

    # sync-prototypes
    p_proto = subparsers.add_parser("sync-prototypes", help="Apply typed C++ prototypes to canonical functions")
    p_proto.add_argument("--binary", type=str, default="", help="Binary to process")
    p_proto.add_argument("--all", action="store_true", help="Process all binaries with canonical signatures")

    # scan-rtti
    p_rtti = subparsers.add_parser("scan-rtti", help="Discover and label MSVC x64 RTTI classes and VTables")
    p_rtti.add_argument("--binary", type=str, default="vphysics2.dll", help="Binary to scan")
    p_rtti.add_argument("--annotate", action="store_true", default=True, help="Annotate symbols into Ghidra")
    p_rtti.add_argument("--max", type=int, default=300, help="Max classes to scan")

    # trace-xrefs
    p_xref = subparsers.add_parser("trace-xrefs", help="Deep cross-reference and call graph analysis")
    p_xref.add_argument("--target", type=str, help="Target address or function symbol")
    p_xref.add_argument("--string", type=str, help="Search for string literal and its references")
    p_xref.add_argument("--max", type=int, default=50, help="Max references to show")

    # full-sync
    p_full = subparsers.add_parser("full-sync", help="Run end-to-end synchronization across binaries")
    p_full.add_argument("--binary", type=str, default="", help="Specific binary or all")

    args = parser.parse_args()

    client = GhidraClient(port=args.port)

    if not args.command or args.command == "status":
        cmd_status(client)
        return

    conn_ok, conn_msg = client.check_connection()
    if not conn_ok:
        print(f"[-] Failed to connect to Ghidra on port {args.port}: {conn_msg}")
        sys.exit(1)

    if args.command == "inject-types":
        from tools.ghidra.inject_source2_types import inject_types_into_binary
        targets = ALL_CORE_BINARIES if args.all else [args.binary]
        for b in targets:
            inject_types_into_binary(client, b, args.folder)

    elif args.command == "sync-prototypes":
        from tools.ghidra.sync_function_prototypes import sync_prototypes_for_binary, find_repo_root
        repo_root = find_repo_root()
        sigs_path = repo_root / "data" / "function_signatures.json"
        with open(sigs_path, "r", encoding="utf-8") as f:
            sigs = json.load(f)
        by_bin: Dict[str, List[dict]] = {}
        for s in sigs:
            if s.get("binary"):
                by_bin.setdefault(s["binary"], []).append(s)
        targets = list(by_bin.keys()) if (args.all or not args.binary) else [args.binary]
        for b in targets:
            if b in by_bin:
                sync_prototypes_for_binary(client, b, by_bin[b], args.folder)

    elif args.command == "scan-rtti":
        from tools.ghidra.scan_rtti_vtables import scan_binary_rtti, print_rtti_summary
        data = scan_binary_rtti(client, args.binary, annotate=args.annotate, max_classes=args.max, folder=args.folder)
        if data:
            print_rtti_summary(data)

    elif args.command == "trace-xrefs":
        from tools.ghidra.trace_xrefs import build_xref_script, build_string_xref_script, parse_json_from_output, print_xref_report, print_string_report
        if args.string:
            code = build_string_xref_script(args.string, args.max)
            res = client.get_json("/get_current_program_info") # ensure active
            run_res = client.post_json("/run_script_inline", {"code": code})
            data = parse_json_from_output(run_res.get("result", ""))
            if data:
                print_string_report(data)
        elif args.target:
            code = build_xref_script(args.target, "both", args.max)
            run_res = client.post_json("/run_script_inline", {"code": code})
            data = parse_json_from_output(run_res.get("result", ""))
            if data:
                print_xref_report(data)
        else:
            print("[-] Must provide --target or --string for trace-xrefs")

    elif args.command == "full-sync":
        targets = [args.binary] if args.binary else ALL_CORE_BINARIES
        cmd_full_sync(client, targets, args.folder)


if __name__ == "__main__":
    main()
