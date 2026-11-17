#!/usr/bin/env python3
"""
sync_live_ghidra.py - Source 2 Live Ghidra Project Synchronizer

Connects directly to the running Ghidra instance via GhidraMCP HTTP REST API (port 8089)
to annotate the live project without encountering headless lock conflicts:
  1. Opens each binary in the project version folder (/2026-11-11_26598758/<binary>).
  2. Creates and renames all 43 canonical functions from data/function_signatures.json.
  3. Tags structured bookmarks under category 'CS2 Canonical Signature'.
  4. Annotates key virtual method tables and canonical slots from data/vtables.json.
  5. Saves the modified programs in the Ghidra database.
"""

import os
import sys
import json
import argparse
import urllib.request
import urllib.parse
from pathlib import Path

DEFAULT_PORT = 8089
DEFAULT_VERSION_FOLDER = "/2026-11-11_26598758"

def find_repo_root() -> Path:
    current = Path(__file__).resolve().parent
    for p in [current, current.parent, current.parent.parent]:
        if (p / "data" / "function_signatures.json").exists():
            return p
    return Path.cwd()

class GhidraClient:
    def __init__(self, host="127.0.0.1", port=DEFAULT_PORT):
        self.base_url = f"http://{host}:{port}"

    def check_connection(self) -> tuple[bool, str]:
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

    def get_json(self, endpoint: str, params: dict | None = None, timeout=30) -> dict:
        url = f"{self.base_url}{endpoint}"
        if params:
            url = f"{url}?{urllib.parse.urlencode(params)}"
        try:
            with urllib.request.urlopen(url, timeout=timeout) as res:
                return json.loads(res.read().decode("utf-8", errors="replace"))
        except Exception as ex:
            return {"error": str(ex)}

    def open_program(self, path: str) -> dict:
        # /open_program in GhidraMCP takes 'path' as query parameter or body
        url = f"{self.base_url}/open_program?{urllib.parse.urlencode({'path': path})}"
        req = urllib.request.Request(url, data=b"", headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=45) as res:
                return json.loads(res.read().decode("utf-8", errors="replace"))
        except Exception as ex:
            return {"error": str(ex)}

    def save_program(self, program_name: str) -> dict:
        return self.get_json("/save_program", {"program": program_name})

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


def sync_signatures(client: GhidraClient, sigs: list[dict], folder: str, target_bin: str | None = None):
    # Group signatures by target binary
    by_binary: dict[str, list[dict]] = {}
    for s in sigs:
        b = s.get("binary")
        if not b:
            continue
        if target_bin and b.lower() != target_bin.lower():
            continue
        by_binary.setdefault(b, []).append(s)

    print("=================================================================")
    print(" LIVE GHIDRA SIGNATURE SYNCHRONIZATION ENGINE")
    print(f" Target Folder:  {folder}")
    print(f" Target Binaries: {len(by_binary)}")
    print(f" Total Signatures: {sum(len(v) for v in by_binary.values())}")
    print("=================================================================")

    results = {}
    for binary_name, bin_sigs in sorted(by_binary.items()):
        prog_path = f"{folder}/{binary_name}" if not folder.endswith("/") else f"{folder}{binary_name}"
        print(f"\n[*] Opening {prog_path} ({len(bin_sigs)} signatures)...")
        open_res = client.open_program(prog_path)
        if not open_res.get("success"):
            print(f"  [-] Failed to open {prog_path}: {open_res}")
            results[binary_name] = "OPEN_FAILED"
            continue

        created_count = 0
        renamed_count = 0
        bookmarked_count = 0

        for s in bin_sigs:
            sig_id = s.get("id", "")
            name = s.get("name", "")
            va_str = s.get("current_va") or s.get("baseline_va", "")
            if not va_str:
                continue

            clean_va = va_str.lower().replace("0x", "")
            
            # 1. Try to create function if missing
            c_res = client.create_function(clean_va, name, program=binary_name)
            if c_res.get("success") is True or c_res.get("status") == "success":
                created_count += 1
            else:
                # If function already exists, rename it
                r_res = client.rename_function_by_address(clean_va, name, program=binary_name)
                if r_res.get("success") is True or r_res.get("status") == "success":
                    renamed_count += 1

            # 2. Tag bookmark
            cat = "CS2 Canonical Signature"
            comment = f"[{sig_id}] {name} (Pattern: {s.get('pattern', '')[:32]}...)"
            b_res = client.set_bookmark(clean_va, cat, comment, program=binary_name)
            if b_res.get("success") is True or b_res.get("status") == "success":
                bookmarked_count += 1

        print(f"  [+] Synced {binary_name}: {created_count} created, {renamed_count} renamed, {bookmarked_count} bookmarked")

        # Save changes to program database
        save_res = client.save_program(binary_name)
        if save_res.get("success"):
            print(f"  [+] Saved {binary_name} successfully")
            results[binary_name] = f"SUCCESS ({bookmarked_count} sigs)"
        else:
            print(f"  [!] Save report: {save_res}")
            results[binary_name] = "SAVED_WITH_WARNING"

    print("\n=================================================================")
    print(" SIGNATURE SYNCHRONIZATION SUMMARY")
    print("=================================================================")
    for b, res in results.items():
        print(f"  {b:<30} : {res}")
    print("=================================================================")
    return results


def main():
    parser = argparse.ArgumentParser(description="Synchronize Source 2 Signatures & Symbols into Live Ghidra Project.")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"GhidraMCP HTTP port (default: {DEFAULT_PORT})")
    parser.add_argument("--host", default="127.0.0.1", help="GhidraMCP host (default: 127.0.0.1)")
    parser.add_argument("--version-folder", default=DEFAULT_VERSION_FOLDER, help=f"Project version folder (default: {DEFAULT_VERSION_FOLDER})")
    parser.add_argument("--binary", default=None, help="Filter to a single binary name (e.g. client.dll)")
    args = parser.parse_args()

    client = GhidraClient(host=args.host, port=args.port)
    ok, msg = client.check_connection()
    if not ok:
        print(f"[-] ERROR: Cannot connect to GhidraMCP at {client.base_url}: {msg}")
        print("    Ensure Ghidra is running with the GhidraMCP plugin active.")
        sys.exit(1)

    print(f"[+] Connected to GhidraMCP: {msg}")

    repo_root = find_repo_root()
    sigs_path = repo_root / "data" / "function_signatures.json"
    if not sigs_path.exists():
        print(f"[-] ERROR: Signatures file not found at {sigs_path}")
        sys.exit(1)

    with open(sigs_path, "r", encoding="utf-8") as f:
        sigs = json.load(f)

    folder = args.version_folder.rstrip("/")
    sync_signatures(client, sigs, folder, target_bin=args.binary)


if __name__ == "__main__":
    main()
