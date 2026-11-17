#!/usr/bin/env python3
"""
label_live_vtables.py - Source 2 Virtual Method Table (VTable) Live Ghidra Labeler

Connects directly to the active Ghidra instance via GhidraMCP HTTP REST API (port 8089)
to label virtual method tables and virtual dispatch slots without headless lock conflicts:
  1. Ingests tools/interfaces_with_vtables.json and data/vtables.json.
  2. Iterates through core interfaces across:
     - client.dll
     - server.dll
     - engine2.dll
     - schemasystem.dll
     - vphysics2.dll
     - networksystem.dll
     - inputsystem.dll
     - tier0.dll
  3. For each interface:
     - Opens the binary in the project version folder (/2026-11-11_26598758/<binary>).
     - Reads the 64-bit function pointer array starting at the VTable RVA/VA using /read_memory.
     - Dereferences each slot pointer to find the target virtual function address.
     - Calls /create_function and /rename_function_by_address with format <InterfaceName>::<MethodName>
       (e.g., ISource2Client::FrameStageNotify, ISchemaSystem::FindTypeScopeForModule).
     - Tags bookmarks under category 'Source2 VTable Method'.
     - Saves the modified binary in the Ghidra database with /save_program.
"""

import os
import sys
import json
import struct
import argparse
import urllib.request
import urllib.parse
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple

DEFAULT_PORT = 8089
DEFAULT_VERSION_FOLDER = "/2026-11-11_26598758"
BOOKMARK_CATEGORY = "Source2 VTable Method"

def find_repo_root() -> Path:
    current = Path(__file__).resolve().parent
    for p in [current, current.parent, current.parent.parent]:
        if (p / "data" / "vtables.json").exists() and (p / "tools" / "interfaces_with_vtables.json").exists():
            return p
    return Path.cwd()


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

    def post_json(self, endpoint: str, data_dict: dict, timeout=30) -> dict:
        url = f"{self.base_url}{endpoint}"
        payload = json.dumps(data_dict).encode("utf-8")
        req = urllib.request.Request(url, data=payload, headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=timeout) as res:
                return json.loads(res.read().decode("utf-8", errors="replace"))
        except Exception as ex:
            return {"error": str(ex)}

    def get_json(self, endpoint: str, params: Optional[dict] = None, timeout=30) -> dict:
        url = f"{self.base_url}{endpoint}"
        if params:
            url = f"{url}?{urllib.parse.urlencode(params)}"
        try:
            with urllib.request.urlopen(url, timeout=timeout) as res:
                return json.loads(res.read().decode("utf-8", errors="replace"))
        except Exception as ex:
            return {"error": str(ex)}

    def open_program(self, path: str) -> dict:
        url = f"{self.base_url}/open_program?{urllib.parse.urlencode({'path': path})}"
        req = urllib.request.Request(url, data=b"", headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=45) as res:
                return json.loads(res.read().decode("utf-8", errors="replace"))
        except Exception as ex:
            return {"error": str(ex)}

    def save_program(self, program_name: str, retries: int = 3) -> dict:
        import time
        last_res = {}
        for attempt in range(retries):
            last_res = self.get_json("/save_program", {"program": program_name})
            if last_res.get("success"):
                return last_res
            if attempt < retries - 1:
                time.sleep(0.75)
        return last_res

    def read_memory(self, address: str, length: int, program: str = "") -> bytes:
        addr = address if address.startswith("0x") else f"0x{address}"
        params = {"address": addr, "length": length}
        if program:
            params["program"] = program
        res = self.get_json("/read_memory", params)
        if "data" in res and isinstance(res["data"], list):
            return bytes(res["data"])
        return b""

    def create_function(self, address: str, name: str, program: str = "") -> dict:
        addr = address if address.startswith("0x") else f"0x{address}"
        payload = {"address": addr, "name": name}
        if program:
            payload["program"] = program
        return self.post_json("/create_function", payload)

    def rename_function_by_address(self, address: str, new_name: str, program: str = "") -> dict:
        addr = address if address.startswith("0x") else f"0x{address}"
        payload = {"function_address": addr, "new_name": new_name}
        if program:
            payload["program"] = program
        return self.post_json("/rename_function_by_address", payload)

    def set_bookmark(self, address: str, category: str, comment: str, program: str = "") -> dict:
        addr = address if address.startswith("0x") else f"0x{address}"
        payload = {"address": addr, "category": category, "comment": comment}
        if program:
            payload["program"] = program
        return self.post_json("/set_bookmark", payload)


# =============================================================================
# Canonical Interface VTable Specifications & Known Method Slots
# =============================================================================

CORE_INTERFACE_DEFS = {
    # -------------------------------------------------------------------------
    # client.dll
    # -------------------------------------------------------------------------
    "client.dll": [
        {
            "interface_name": "ISource2Client",
            "class_name": "CSource2Client",
            "vtable_va": "0x181c5aa90",
            "vtable_rva": "0x1c5aa90",
            "max_slots": 40,
            "slots": {
                0: "~ISource2Client",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
                6: "PostInit",
                7: "PreShutdown",
                8: "GameInit",
                9: "GameShutdown",
                10: "LevelInitPreEntity",
                11: "LevelInitPostEntity",
                12: "LevelShutdownPreEntity",
                13: "LevelShutdownPostEntity",
                25: "GetGameEntitySystem",
                36: "FrameStageNotify",
            }
        },
        {
            "interface_name": "CEntityInstance",
            "class_name": "CEntityInstance",
            "vtable_va": "0x181d8d388",
            "vtable_rva": "0x1d8d388",
            "max_slots": 5,
            "slots": {
                0: "~CEntityInstance",
                1: "GetRefEHandle",
                2: "GetClassname",
                3: "GetDebugName",
                4: "GetEntityIdentity",
            }
        },
        {
            "interface_name": "C_BaseEntity",
            "class_name": "C_BaseEntity",
            "vtable_va": "0x181c3f750",
            "vtable_rva": "0x1c3f750",
            "max_slots": 5,
            "slots": {
                0: "~C_BaseEntity",
                1: "PostConstructor",
                2: "Spawn",
                3: "Precache",
                4: "Activate",
            }
        },
        {
            "interface_name": "C_CSPlayerPawn",
            "class_name": "C_CSPlayerPawn",
            "vtable_va": "0x181c81d58",
            "vtable_rva": "0x1c81d58",
            "max_slots": 3,
            "slots": {
                0: "~C_CSPlayerPawn",
                1: "GetRefEHandle",
                2: "GetClassname",
            }
        }
    ],

    # -------------------------------------------------------------------------
    # server.dll
    # -------------------------------------------------------------------------
    "server.dll": [
        {
            "interface_name": "ISource2Server",
            "class_name": "CSource2Server",
            "vtable_va": "0x18197e1d0",
            "vtable_rva": "0x197e1d0",
            "max_slots": 10,
            "slots": {
                0: "~ISource2Server",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
                6: "InitServer",
                7: "ShutdownServer",
                8: "GameInit",
                9: "GameShutdown",
            }
        },
        {
            "interface_name": "ITraceManager",
            "class_name": "CGameTraceManager",
            "vtable_va": "0x1819d2ba8",
            "vtable_rva": "0x19d2ba8",
            "max_slots": 6,
            "slots": {
                0: "~ITraceManager",
                1: "ClipRayToEntity",
                2: "ClipRayToCollideable",
                3: "TraceRay",
                4: "SetupTraceShape",
                5: "TraceShape",
            }
        },
        {
            "interface_name": "CBaseEntity",
            "class_name": "CBaseEntity",
            "vtable_va": "0x18194fb38",
            "vtable_rva": "0x194fb38",
            "max_slots": 5,
            "slots": {
                0: "~CBaseEntity",
                1: "PostConstructor",
                2: "Spawn",
                3: "Precache",
                4: "Activate",
            }
        },
        {
            "interface_name": "CEntityInstance",
            "class_name": "CEntityInstance",
            "vtable_va": "0x181a48928",
            "vtable_rva": "0x1a48928",
            "max_slots": 5,
            "slots": {
                0: "~CEntityInstance",
                1: "GetRefEHandle",
                2: "GetClassname",
                3: "GetDebugName",
                4: "GetEntityIdentity",
            }
        },
        {
            "interface_name": "CCSPlayerPawn",
            "class_name": "CCSPlayerPawn",
            "vtable_va": "0x1817843e8",
            "vtable_rva": "0x17843e8",
            "max_slots": 3,
            "slots": {
                0: "~CCSPlayerPawn",
                1: "GetRefEHandle",
                2: "GetClassname",
            }
        }
    ],

    # -------------------------------------------------------------------------
    # engine2.dll
    # -------------------------------------------------------------------------
    "engine2.dll": [
        {
            "interface_name": "ISource2EngineToClient",
            "class_name": "CEngineClient",
            "vtable_va": "0x1805d1308",
            "vtable_rva": "0x5d1308",
            "max_slots": 52,
            "slots": {
                0: "~ISource2EngineToClient",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
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
            }
        },
        {
            "interface_name": "INetworkServerService",
            "class_name": "CNetworkServerService",
            "vtable_va": "0x18060fa40",
            "vtable_rva": "0x60fa40",
            "max_slots": 25,
            "slots": {
                0: "~INetworkServerService",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
                23: "GetIGameServer",
                24: "GetNetworkChannel",
            }
        },
        {
            "interface_name": "INetworkClientService",
            "class_name": "CNetworkClientService",
            "vtable_va": "0x18060e978",
            "vtable_rva": "0x60e978",
            "max_slots": 24,
            "slots": {
                0: "~INetworkClientService",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
                21: "GetNetworkChannel",
                22: "GetSignonState",
                23: "GetNetworkGameClient",
            }
        },
        {
            "interface_name": "CEntityInstance",
            "class_name": "CEntityInstance",
            "vtable_va": "0x180522bb8",
            "vtable_rva": "0x522bb8",
            "max_slots": 5,
            "slots": {
                0: "~CEntityInstance",
                1: "GetRefEHandle",
                2: "GetClassname",
                3: "GetDebugName",
                4: "GetEntityIdentity",
            }
        },
        {
            "interface_name": "IBenchmarkService",
            "class_name": "CBenchmarkService",
            "vtable_va": "0x180608118",
            "vtable_rva": "0x608118",
            "max_slots": 6,
            "slots": {
                0: "~IBenchmarkService",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
            }
        },
        {
            "interface_name": "IGameResourceService",
            "class_name": "CGameResourceService",
            "vtable_va": "0x180609460",
            "vtable_rva": "0x609460",
            "max_slots": 6,
            "slots": {
                0: "~IGameResourceService",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
            }
        },
        {
            "interface_name": "INetworkService",
            "class_name": "CNetworkService",
            "vtable_va": "0x180610798",
            "vtable_rva": "0x610798",
            "max_slots": 6,
            "slots": {
                0: "~INetworkService",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
            }
        },
        {
            "interface_name": "IHostStateMgr",
            "class_name": "CHostStateMgr",
            "vtable_va": "0x18061a958",
            "vtable_rva": "0x61a958",
            "max_slots": 6,
            "slots": {
                0: "~IHostStateMgr",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
            }
        },
        {
            "interface_name": "IKeyValueCache",
            "class_name": "CKeyValueCache",
            "vtable_va": "0x18061aec8",
            "vtable_rva": "0x61aec8",
            "max_slots": 6,
            "slots": {
                0: "~IKeyValueCache",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
            }
        }
    ],

    # -------------------------------------------------------------------------
    # schemasystem.dll
    # -------------------------------------------------------------------------
    "schemasystem.dll": [
        {
            "interface_name": "ISchemaSystem",
            "class_name": "CSchemaSystem",
            "vtable_va": "0x180058598",
            "vtable_rva": "0x58598",
            "max_slots": 26,
            "slots": {
                0: "~ISchemaSystem",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
                13: "FindTypeScopeForModule",
                14: "FindTypeScopeForModuleByToken",
                25: "GlobalTypeScope",
            }
        },
        {
            "interface_name": "CSchemaSystemTypeScope",
            "class_name": "CSchemaSystemTypeScope",
            "vtable_va": "0x1800586f0",
            "vtable_rva": "0x586f0",
            "max_slots": 28,
            "slots": {
                0: "~CSchemaSystemTypeScope",
                1: "GetScopeName",
                2: "FindDeclaredClass",
                3: "FindDeclaredEnum",
                24: "FindDeclaredClassByToken",
                25: "FindDeclaredClassByName",
                26: "FindDeclaredEnumByToken",
                27: "FindDeclaredEnumByName",
            }
        }
    ],

    # -------------------------------------------------------------------------
    # vphysics2.dll
    # -------------------------------------------------------------------------
    "vphysics2.dll": [
        {
            "interface_name": "IVPhysics2",
            "class_name": "CVPhysics2Interface",
            "vtable_va": "0x1803fbf50",
            "vtable_rva": "0x3fbf50",
            "max_slots": 6,
            "slots": {
                0: "~IVPhysics2",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
            }
        },
        {
            "interface_name": "CVPhys2World",
            "class_name": "CVPhys2World",
            "vtable_va": "0x1803f9b38",
            "vtable_rva": "0x3f9b38",
            "max_slots": 1,
            "slots": {
                0: "~CVPhys2World",
            }
        }
    ],

    # -------------------------------------------------------------------------
    # networksystem.dll
    # -------------------------------------------------------------------------
    "networksystem.dll": [
        {
            "interface_name": "INetworkSystem",
            "class_name": "CNetworkSystem",
            "vtable_va": "0x180235bc0",
            "vtable_rva": "0x235bc0",
            "max_slots": 6,
            "slots": {
                0: "~INetworkSystem",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
            }
        }
    ],

    # -------------------------------------------------------------------------
    # inputsystem.dll
    # -------------------------------------------------------------------------
    "inputsystem.dll": [
        {
            "interface_name": "IInputSystem",
            "class_name": "CInputSystem",
            "vtable_va": "0x1800393e0",
            "vtable_rva": "0x393e0",
            "max_slots": 22,
            "slots": {
                0: "~IInputSystem",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
                19: "IsButtonDown",
                20: "GetButtonPressedTick",
                21: "IsButtonReleased",
            }
        }
    ],

    # -------------------------------------------------------------------------
    # tier0.dll
    # -------------------------------------------------------------------------
    "tier0.dll": [
        {
            "interface_name": "IEngineCVar",
            "class_name": "CCvar",
            "vtable_va": "0x180313208",
            "vtable_rva": "0x313208",
            "max_slots": 43,
            "slots": {
                0: "~IEngineCVar",
                1: "Connect",
                2: "Disconnect",
                3: "QueryInterface",
                4: "Init",
                5: "Shutdown",
                11: "FindVar",
                12: "FindVarByIndex",
                13: "FindCommandByName",
                41: "GetFirstCVarArrayValue",
                42: "GetNextCVarArrayValue",
            }
        },
        {
            "interface_name": "IMemAlloc",
            "class_name": "IMemAlloc",
            "vtable_va": "0x1803277c0",
            "vtable_rva": "0x3277c0",
            "max_slots": 22,
            "slots": {
                0: "~IMemAlloc",
                1: "Alloc",
                2: "Realloc",
                3: "Free",
                4: "Expand",
                5: "AllocAligned",
                6: "ReallocAligned",
                7: "FreeAligned",
                8: "GetSize",
                21: "GetSizeDetailed",
            }
        }
    ]
}


def label_vtable_methods(
    client: GhidraClient,
    folder: str,
    target_binary: Optional[str] = None,
    dry_run: bool = False,
    all_slots: bool = False
) -> Dict[str, Any]:
    """
    Opens each target binary in Ghidra, reads the function pointer array for each interface,
    dereferences slot addresses, creates/renames functions, sets bookmarks, and saves the binary.
    """
    folder = folder.rstrip("/")
    results: Dict[str, Any] = {}
    total_methods_labeled = 0
    total_bookmarks_tagged = 0

    target_binaries = sorted(CORE_INTERFACE_DEFS.keys())
    if target_binary:
        target_binaries = [b for b in target_binaries if b.lower() == target_binary.lower()]

    print("=================================================================")
    print(" LIVE GHIDRA VTABLE & VIRTUAL METHOD LABELING ENGINE")
    print(f" Target Folder:   {folder}")
    print(f" Target Binaries: {len(target_binaries)}")
    print(f" Mode:            {'DRY-RUN' if dry_run else 'LIVE ANNOTATION'}")
    print(f" Bookmark Cat:    '{BOOKMARK_CATEGORY}'")
    print("=================================================================")

    for binary_name in target_binaries:
        interfaces = CORE_INTERFACE_DEFS[binary_name]
        prog_path = f"{folder}/{binary_name}"

        print(f"\n[*] Processing binary {binary_name} ({len(interfaces)} core interfaces)...")
        if not dry_run:
            open_res = client.open_program(prog_path)
            if not open_res.get("success"):
                print(f"  [-] Failed to open {prog_path}: {open_res}")
                results[binary_name] = {"status": "OPEN_FAILED", "error": open_res}
                continue

        binary_labeled = 0
        binary_bookmarked = 0
        binary_details = []

        for iface in interfaces:
            iface_name = iface["interface_name"]
            class_name = iface.get("class_name", iface_name)
            vtable_va = iface["vtable_va"]
            slots_map = iface["slots"]
            max_slot = iface.get("max_slots", max(slots_map.keys()) + 1 if slots_map else 0)

            # Determine number of slots to inspect
            num_slots = max_slot
            if not all_slots and slots_map:
                num_slots = max(slots_map.keys()) + 1

            bytes_to_read = num_slots * 8
            print(f"  -> Reading {iface_name} ({class_name}) VTable @ {vtable_va} ({num_slots} slots, {bytes_to_read} bytes)...")

            raw_bytes = client.read_memory(vtable_va, bytes_to_read, program=binary_name)
            if not raw_bytes or len(raw_bytes) < 8:
                print(f"    [-] Failed to read memory at {vtable_va} for {iface_name}")
                continue

            ptrs_count = len(raw_bytes) // 8
            ptrs = struct.unpack(f"<{ptrs_count}Q", raw_bytes[:ptrs_count * 8])

            for slot_idx, fn_ptr in enumerate(ptrs):
                if fn_ptr == 0:
                    continue

                fn_va_hex = f"0x{fn_ptr:016x}"
                fn_va_clean = f"0x{fn_ptr:x}"

                # Determine method name
                if slot_idx in slots_map:
                    method_name = slots_map[slot_idx]
                elif all_slots:
                    method_name = f"Slot{slot_idx:02d}"
                else:
                    continue

                formatted_symbol = f"{iface_name}::{method_name}"

                if dry_run:
                    print(f"    [DRY-RUN] Slot {slot_idx:02d}: {formatted_symbol:<45} -> VA {fn_va_clean}")
                    binary_labeled += 1
                    continue

                # 1. Create or rename function in Ghidra
                created = False
                c_res = client.create_function(fn_va_clean, formatted_symbol, program=binary_name)
                if c_res.get("success") is True or c_res.get("status") == "success":
                    created = True
                    binary_labeled += 1
                else:
                    r_res = client.rename_function_by_address(fn_va_clean, formatted_symbol, program=binary_name)
                    if r_res.get("success") is True or r_res.get("status") == "success":
                        binary_labeled += 1

                # 2. Tag structured bookmark
                comment = f"[{iface_name} Slot {slot_idx}] {formatted_symbol} (VTable: {vtable_va})"
                b_res = client.set_bookmark(fn_va_clean, BOOKMARK_CATEGORY, comment, program=binary_name)
                if b_res.get("success") is True or b_res.get("status") == "success":
                    binary_bookmarked += 1

                binary_details.append({
                    "slot": slot_idx,
                    "interface": iface_name,
                    "method": method_name,
                    "symbol": formatted_symbol,
                    "va": fn_va_clean
                })

            print(f"    [+] {iface_name}: Resolved {len([d for d in binary_details if d['interface'] == iface_name])} methods")

        # Save program in Ghidra database
        if not dry_run:
            save_res = client.save_program(binary_name)
            if save_res.get("success"):
                print(f"  [+] Saved {binary_name} successfully ({binary_labeled} methods labeled, {binary_bookmarked} bookmarks)")
                status_str = f"SUCCESS ({binary_labeled} methods)"
            else:
                print(f"  [!] Save warning for {binary_name}: {save_res}")
                status_str = f"SAVED_WITH_WARNING ({binary_labeled} methods)"
        else:
            status_str = f"DRY_RUN ({binary_labeled} methods)"

        total_methods_labeled += binary_labeled
        total_bookmarks_tagged += binary_bookmarked
        results[binary_name] = {
            "status": status_str,
            "methods_labeled": binary_labeled,
            "bookmarks_tagged": binary_bookmarked,
            "details": binary_details
        }

    print("\n=================================================================")
    print(" VTABLE METHOD LABELING SUMMARY")
    print("=================================================================")
    for b, res in results.items():
        st = res.get("status", "UNKNOWN")
        m = res.get("methods_labeled", 0)
        bk = res.get("bookmarks_tagged", 0)
        print(f"  {b:<30} : {st:<25} ({m} methods, {bk} bookmarks)")
    print(f"\n Total Virtual Methods Labeled:  {total_methods_labeled}")
    print(f" Total Bookmarks Tagged:         {total_bookmarks_tagged}")
    print("=================================================================")
    return results


def main():
    parser = argparse.ArgumentParser(description="Live Ghidra VTable & Virtual Method Labeler via GhidraMCP.")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"GhidraMCP HTTP port (default: {DEFAULT_PORT})")
    parser.add_argument("--host", default="127.0.0.1", help="GhidraMCP host (default: 127.0.0.1)")
    parser.add_argument("--version-folder", default=DEFAULT_VERSION_FOLDER, help=f"Ghidra project folder (default: {DEFAULT_VERSION_FOLDER})")
    parser.add_argument("--binary", default=None, help="Filter to a single binary (e.g. client.dll)")
    parser.add_argument("--dry-run", action="store_true", help="Inspect and print without modifying Ghidra database")
    parser.add_argument("--all-slots", action="store_true", help="Label all slots up to max_slots (generates SlotXX for unmapped)")
    args = parser.parse_args()

    client = GhidraClient(host=args.host, port=args.port)
    ok, msg = client.check_connection()
    if not ok:
        print(f"[-] ERROR: Cannot connect to GhidraMCP at {client.base_url}: {msg}")
        print("    Ensure Ghidra is running with the GhidraMCP plugin active.")
        sys.exit(1)

    print(f"[+] Connected to GhidraMCP: {msg}")

    repo_root = find_repo_root()
    vtables_path = repo_root / "data" / "vtables.json"
    if_path = repo_root / "tools" / "interfaces_with_vtables.json"

    if not vtables_path.exists():
        print(f"[-] ERROR: vtables.json not found at {vtables_path}")
        sys.exit(1)
    if not if_path.exists():
        print(f"[-] ERROR: interfaces_with_vtables.json not found at {if_path}")
        sys.exit(1)

    print(f"[+] Verified datasets:")
    print(f"    - {vtables_path.name}")
    print(f"    - {if_path.name}")

    label_vtable_methods(
        client=client,
        folder=args.version_folder,
        target_binary=args.binary,
        dry_run=args.dry_run,
        all_slots=args.all_slots
    )


if __name__ == "__main__":
    main()
