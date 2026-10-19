#!/usr/bin/env python3
"""
scan_signatures.py - Source 2 Function Signature & Offset Verification Engine

Scans compiled CS2 binaries using IDA/Ghidra byte patterns with wildcards:
- Verifies pattern uniqueness (1 match = Valid, 0 = Broken, >1 = Ambiguous)
- Resolves exact RVAs and VAs in target binaries
- Computes RVA offset shifts (+/- bytes) across game updates
- Compares signatures between two version archives

Usage:
  python tools/scan_signatures.py --dir versions/archive/2026-10-15_25966348
  python tools/scan_signatures.py --dll versions/archive/2026-10-15_25966348/client.dll
  python tools/scan_signatures.py --diff versions/archive/2026-10-15_25966348 versions/archive/NEW_BUILD
"""

import os
import sys
import json
import re
import argparse
import time

# Ensure parent directory is in path to import PEBinary
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tools.cs2_patch_diff import PEBinary

DEFAULT_SIGNATURES_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "data",
    "function_signatures.json"
)

def pattern_to_regex(pattern_str: str) -> re.Pattern:
    """Converts an IDA-style hex string (e.g. '48 8B C4 ? ? 55') into a compiled regex bytes pattern."""
    tokens = pattern_str.strip().split()
    regex_parts = []
    for token in tokens:
        if token in ("?", "??"):
            regex_parts.append(b".")
        else:
            byte_val = bytes([int(token, 16)])
            regex_parts.append(re.escape(byte_val))
    return re.compile(b"".join(regex_parts), re.DOTALL)

def scan_binary_for_signatures(pe: PEBinary, signatures: list) -> list:
    """Scans a PEBinary instance against a list of signature definitions."""
    results = []
    for sig in signatures:
        if sig.get("binary", "").lower() != pe.filename.lower():
            continue

        sig_id = sig.get("id", "")
        name = sig.get("name", "")
        pattern_str = sig.get("pattern", "")
        baseline_rva_str = sig.get("baseline_rva", "0x0")
        baseline_rva = int(baseline_rva_str, 16)

        try:
            regex = pattern_to_regex(pattern_str)
        except Exception as e:
            results.append({
                "id": sig_id,
                "name": name,
                "binary": pe.filename,
                "status": "PATTERN_ERROR",
                "error": str(e),
                "matches": 0,
                "baseline_rva": baseline_rva_str,
                "current_rva": None,
                "delta": None
            })
            continue

        # Find all occurrences
        matches = [m.start() for m in regex.finditer(pe.data)]
        num_matches = len(matches)

        if num_matches == 0:
            status = "BROKEN"
            current_rva = None
            current_va = None
            delta = None
        elif num_matches == 1:
            file_off = matches[0]
            current_va = pe.off_to_va(file_off)
            current_rva = current_va - pe.image_base if current_va else None
            delta = (current_rva - baseline_rva) if current_rva is not None else None
            status = "UNCHANGED" if delta == 0 else "SHIFTED"
        else:
            status = "AMBIGUOUS"
            current_rva = None
            current_va = None
            delta = None

        results.append({
            "id": sig_id,
            "name": name,
            "binary": pe.filename,
            "category": sig.get("category", ""),
            "status": status,
            "matches": num_matches,
            "baseline_rva": baseline_rva_str,
            "current_rva": f"0x{current_rva:07x}" if current_rva is not None else None,
            "current_va": f"0x{current_va:016x}" if current_va is not None else None,
            "delta_bytes": delta,
            "delta_str": (f"+0x{delta:x}" if delta > 0 else (f"-0x{-delta:x}" if delta < 0 else "+0x0")) if delta is not None else "N/A",
            "pattern": pattern_str,
            "description": sig.get("description", "")
        })

    return results

def scan_directory(source_dir: str, signatures_file: str) -> dict:
    """Scans all relevant binaries in source_dir against the signatures registry."""
    if not os.path.exists(signatures_file):
        raise FileNotFoundError(f"Signatures database not found: {signatures_file}")

    with open(signatures_file, "r", encoding="utf-8") as f:
        signatures = json.load(f)

    # Group signatures by target binary
    bin_map = {}
    for sig in signatures:
        b = sig.get("binary", "").lower()
        bin_map.setdefault(b, []).append(sig)

    results = []
    t0 = time.time()

    for bin_name, sig_list in bin_map.items():
        bin_path = os.path.join(source_dir, bin_name)
        if not os.path.exists(bin_path):
            for s in sig_list:
                results.append({
                    "id": s.get("id"),
                    "name": s.get("name"),
                    "binary": bin_name,
                    "status": "FILE_MISSING",
                    "matches": 0,
                    "baseline_rva": s.get("baseline_rva"),
                    "current_rva": None,
                    "delta_bytes": None,
                    "description": s.get("description", "")
                })
            continue

        pe = PEBinary(bin_path)
        bin_results = scan_binary_for_signatures(pe, sig_list)
        results.extend(bin_results)

    duration = time.time() - t0

    # Summary metrics
    total = len(results)
    unchanged = sum(1 for r in results if r.get("status") == "UNCHANGED")
    shifted = sum(1 for r in results if r.get("status") == "SHIFTED")
    broken = sum(1 for r in results if r.get("status") == "BROKEN")
    ambiguous = sum(1 for r in results if r.get("status") == "AMBIGUOUS")
    missing = sum(1 for r in results if r.get("status") == "FILE_MISSING")

    return {
        "source_dir": source_dir,
        "scanned_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "duration_seconds": round(duration, 3),
        "total_signatures": total,
        "metrics": {
            "unchanged": unchanged,
            "shifted": shifted,
            "broken": broken,
            "ambiguous": ambiguous,
            "file_missing": missing,
            "valid_unique": unchanged + shifted
        },
        "signatures": results
    }

def print_scan_report(report: dict):
    """Prints a beautiful formatted ASCII report."""
    print("=" * 115)
    print(f"  SOURCE 2 FUNCTION SIGNATURE & OFFSET VERIFICATION REPORT")
    print(f"  Directory: {report.get('source_dir')}")
    print(f"  Scanned At: {report.get('scanned_at')} | Duration: {report.get('duration_seconds')}s")
    print("=" * 115)
    m = report["metrics"]
    print(f"  Summary: Total: {report['total_signatures']} | "
          f"Unchanged: {m['unchanged']} | "
          f"Shifted: {m['shifted']} | "
          f"Broken: {m['broken']} | "
          f"Ambiguous: {m['ambiguous']}")
    print("-" * 115)
    print(f" {'Binary':<18} | {'Function Symbol':<32} | {'Status':<10} | {'Base RVA':<11} | {'Curr RVA':<11} | {'Delta':<8} | {'Matches'}")
    print("-" * 115)

    for s in report["signatures"]:
        status = s.get("status", "UNKNOWN")
        base_rva = s.get("baseline_rva", "")
        curr_rva = s.get("current_rva") or "-"
        delta = s.get("delta_str") or "-"
        matches = str(s.get("matches", 0))

        # Status tag formatting
        if status == "UNCHANGED":
            status_tag = "[PASS]"
        elif status == "SHIFTED":
            status_tag = "[SHIFT]"
        elif status == "BROKEN":
            status_tag = "[FAIL]"
        elif status == "AMBIGUOUS":
            status_tag = "[AMBIG]"
        else:
            status_tag = f"[{status[:5]}]"

        print(f" {s.get('binary', ''):<18} | {s.get('name', ''):<32} | {status_tag:<10} | {base_rva:<11} | {curr_rva:<11} | {delta:<8} | {matches}")

    print("=" * 115)

def diff_signatures(dir_a: str, dir_b: str, signatures_file: str) -> dict:
    """Compares signatures between two archived versions (A = baseline, B = new)."""
    print(f"[*] Diffing function signatures between:")
    print(f"    Version A (Baseline): {dir_a}")
    print(f"    Version B (Target):   {dir_b}\n")

    res_a = scan_directory(dir_a, signatures_file)
    res_b = scan_directory(dir_b, signatures_file)

    map_a = {r["id"]: r for r in res_a["signatures"]}
    map_b = {r["id"]: r for r in res_b["signatures"]}

    diff_entries = []
    for sig_id, a in map_a.items():
        b = map_b.get(sig_id, {})
        rva_a = a.get("current_rva")
        rva_b = b.get("current_rva")

        if rva_a and rva_b:
            delta = int(rva_b, 16) - int(rva_a, 16)
            delta_str = f"+0x{delta:x}" if delta > 0 else (f"-0x{-delta:x}" if delta < 0 else "0x0")
            if delta == 0:
                change = "UNCHANGED"
            else:
                change = "SHIFTED"
        elif rva_a and not rva_b:
            change = "BROKEN_IN_TARGET"
            delta_str = "N/A"
        elif not rva_a and rva_b:
            change = "RESTORED"
            delta_str = "N/A"
        else:
            change = "UNRESOLVED_IN_BOTH"
            delta_str = "N/A"

        diff_entries.append({
            "id": sig_id,
            "name": a.get("name"),
            "binary": a.get("binary"),
            "category": a.get("category"),
            "change": change,
            "rva_a": rva_a,
            "rva_b": rva_b,
            "delta": delta_str,
            "matches_a": a.get("matches", 0),
            "matches_b": b.get("matches", 0),
            "description": a.get("description", "")
        })

    return {
        "baseline_dir": dir_a,
        "target_dir": dir_b,
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "total_compared": len(diff_entries),
        "changes": diff_entries
    }

def main():
    parser = argparse.ArgumentParser(description="Source 2 Function Signature & Offset Verification Engine")
    parser.add_argument("--dir", help="Path to directory containing binaries to scan")
    parser.add_argument("--dll", help="Path to single DLL to scan")
    parser.add_argument("--diff", nargs=2, metavar=("DIR_A", "DIR_B"), help="Compare signatures between two version directories")
    parser.add_argument("--db", default=DEFAULT_SIGNATURES_FILE, help="Path to function_signatures.json")
    parser.add_argument("--json", help="Export scan results to specified JSON file")

    args = parser.parse_args()

    if args.diff:
        diff_report = diff_signatures(args.diff[0], args.diff[1], args.db)
        print("=" * 115)
        print(f"  FUNCTION SIGNATURE DIFFERENTIAL ANALYSIS")
        print("=" * 115)
        for d in diff_report["changes"]:
            print(f" {d['binary']:<18} | {d['name']:<30} | {d['change']:<18} | {str(d['rva_a']):<11} -> {str(d['rva_b']):<11} | Delta: {d['delta']}")
        print("=" * 115)
        if args.json:
            with open(args.json, "w", encoding="utf-8") as f:
                json.dump(diff_report, f, indent=2)
            print(f"[+] Exported diff to {args.json}")
        return

    if args.dll:
        pe = PEBinary(args.dll)
        with open(args.db, "r", encoding="utf-8") as f:
            sigs = json.load(f)
        results = scan_binary_for_signatures(pe, sigs)
        report = {
            "source_dir": os.path.dirname(args.dll),
            "scanned_at": time.strftime("%Y-%m-%d %H:%M:%S"),
            "duration_seconds": 0.0,
            "total_signatures": len(results),
            "metrics": {
                "unchanged": sum(1 for r in results if r.get("status") == "UNCHANGED"),
                "shifted": sum(1 for r in results if r.get("status") == "SHIFTED"),
                "broken": sum(1 for r in results if r.get("status") == "BROKEN"),
                "ambiguous": sum(1 for r in results if r.get("status") == "AMBIGUOUS"),
                "file_missing": 0,
                "valid_unique": sum(1 for r in results if r.get("status") in ("UNCHANGED", "SHIFTED"))
            },
            "signatures": results
        }
        print_scan_report(report)
        return

    target_dir = args.dir or "versions/archive/2026-10-15_25966348"
    if not os.path.exists(target_dir):
        print(f"Error: Directory {target_dir} does not exist.")
        sys.exit(1)

    report = scan_directory(target_dir, args.db)
    print_scan_report(report)

    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2)
        print(f"[+] Exported results to {args.json}")

if __name__ == "__main__":
    main()
