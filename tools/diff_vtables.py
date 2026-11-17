#!/usr/bin/env python3
"""
Source 2 Virtual Method Table ABI Diffing & Shift Detection Tool
Compares two vtables.json datasets across Source 2 builds, detects slot additions,
removals, index displacement shifts, and emits critical warnings if canonical
virtual method slots have moved or changed.

Strict Invariants Enforced:
- Relative pathing only (no hardcoded absolute user/drive paths)
- Read-only analysis of extracted metadata (zero binary modifications)
"""

import sys
import os
import json
import argparse
from typing import Dict, List, Any, Optional, Tuple, Set


class VTableDiffEngine:
    """Computes virtual method table and ABI displacements between two builds."""

    def __init__(self, old_data: List[Dict[str, Any]], new_data: List[Dict[str, Any]]):
        self.old_data = old_data
        self.new_data = new_data
        self.old_map: Dict[Tuple[str, str, int], Dict[str, Any]] = {}
        self.new_map: Dict[Tuple[str, str, int], Dict[str, Any]] = {}
        self._index_tables()

    @staticmethod
    def _make_key(entry: Dict[str, Any]) -> Tuple[str, str, int]:
        binary = entry.get("binary", "").lower()
        class_name = entry.get("class_name", "")
        this_offset = int(entry.get("this_offset", 0))
        return (binary, class_name, this_offset)

    def _index_tables(self) -> None:
        for entry in self.old_data:
            key = self._make_key(entry)
            self.old_map[key] = entry
        for entry in self.new_data:
            key = self._make_key(entry)
            self.new_map[key] = entry

    def compute_diff(
        self,
        binary_filter: Optional[str] = None,
        class_filter: Optional[str] = None
    ) -> Dict[str, Any]:
        """Runs differential analysis across all indexed virtual method tables."""
        all_keys = set(self.old_map.keys()) | set(self.new_map.keys())

        if binary_filter:
            target_bin = binary_filter.lower()
            all_keys = {k for k in all_keys if k[0] == target_bin}

        if class_filter:
            target_cls = class_filter.lower()
            all_keys = {k for k in all_keys if target_cls in k[1].lower()}

        added_tables: List[Dict[str, Any]] = []
        removed_tables: List[Dict[str, Any]] = []
        modified_tables: List[Dict[str, Any]] = []
        canonical_shifts: List[Dict[str, Any]] = []

        binaries_seen: Set[str] = set()

        for key in sorted(all_keys):
            binary, class_name, this_offset = key
            binaries_seen.add(binary)
            in_old = key in self.old_map
            in_new = key in self.new_map

            if in_new and not in_old:
                new_entry = self.new_map[key]
                added_tables.append({
                    "binary": binary,
                    "class_name": class_name,
                    "this_offset": this_offset,
                    "method_count": new_entry.get("method_count", 0),
                    "vtable_rva": new_entry.get("vtable_rva", "")
                })
                continue

            if in_old and not in_new:
                old_entry = self.old_map[key]
                removed_tables.append({
                    "binary": binary,
                    "class_name": class_name,
                    "this_offset": this_offset,
                    "method_count": old_entry.get("method_count", 0),
                    "vtable_rva": old_entry.get("vtable_rva", "")
                })
                continue

            # Table exists in both builds; inspect methods and slots
            old_entry = self.old_map[key]
            new_entry = self.new_map[key]

            old_count = old_entry.get("method_count", len(old_entry.get("methods", [])))
            new_count = new_entry.get("method_count", len(new_entry.get("methods", [])))
            old_rva = old_entry.get("vtable_rva", "")
            new_rva = new_entry.get("vtable_rva", "")

            old_methods = {m.get("slot", i): m for i, m in enumerate(old_entry.get("methods", []))}
            new_methods = {m.get("slot", i): m for i, m in enumerate(new_entry.get("methods", []))}

            count_delta = new_count - old_count
            rva_changed = old_rva.lower() != new_rva.lower()

            slot_changes: List[Dict[str, Any]] = []
            max_slot = max(max(old_methods.keys(), default=-1), max(new_methods.keys(), default=-1))

            table_has_canon_shift = False

            # Check for canonical slot displacement
            # Track canonical symbols in old vs new
            old_canon_by_sym = {m.get("symbol"): s for s, m in old_methods.items() if m.get("canonical") and m.get("symbol")}
            new_canon_by_sym = {m.get("symbol"): s for s, m in new_methods.items() if m.get("canonical") and m.get("symbol")}

            for sym, old_slot in old_canon_by_sym.items():
                new_slot = new_canon_by_sym.get(sym)
                if new_slot is not None and old_slot != new_slot:
                    shift_info = {
                        "binary": binary,
                        "class_name": class_name,
                        "symbol": sym,
                        "old_slot": old_slot,
                        "new_slot": new_slot,
                        "delta": new_slot - old_slot,
                        "severity": "CRITICAL"
                    }
                    canonical_shifts.append(shift_info)
                    table_has_canon_shift = True
                elif new_slot is None:
                    # Canonical symbol missing in new table
                    shift_info = {
                        "binary": binary,
                        "class_name": class_name,
                        "symbol": sym,
                        "old_slot": old_slot,
                        "new_slot": None,
                        "delta": None,
                        "severity": "MISSING_CANONICAL"
                    }
                    canonical_shifts.append(shift_info)
                    table_has_canon_shift = True

            # Also check slot-by-slot discrepancies
            for slot in range(max_slot + 1):
                m_old = old_methods.get(slot)
                m_new = new_methods.get(slot)

                if m_old and not m_new:
                    slot_changes.append({
                        "slot": slot,
                        "type": "REMOVED",
                        "old_symbol": m_old.get("symbol", ""),
                        "old_rva": m_old.get("rva", ""),
                        "canonical": m_old.get("canonical", False)
                    })
                elif m_new and not m_old:
                    slot_changes.append({
                        "slot": slot,
                        "type": "ADDED",
                        "new_symbol": m_new.get("symbol", ""),
                        "new_rva": m_new.get("rva", ""),
                        "canonical": m_new.get("canonical", False)
                    })
                elif m_old and m_new:
                    old_sym = m_old.get("symbol", "")
                    new_sym = m_new.get("symbol", "")
                    old_m_rva = m_old.get("rva", "").lower()
                    new_m_rva = m_new.get("rva", "").lower()
                    is_canon = m_old.get("canonical", False) or m_new.get("canonical", False)

                    sym_changed = (old_sym != new_sym and not (old_sym.endswith(f"::vtable[{slot}]") and new_sym.endswith(f"::vtable[{slot}]")))
                    rva_diff = (old_m_rva != new_m_rva)

                    if sym_changed or (is_canon and sym_changed):
                        slot_changes.append({
                            "slot": slot,
                            "type": "SYMBOL_CHANGED",
                            "old_symbol": old_sym,
                            "new_symbol": new_sym,
                            "canonical": is_canon
                        })

            if count_delta != 0 or rva_changed or slot_changes or table_has_canon_shift:
                modified_tables.append({
                    "binary": binary,
                    "class_name": class_name,
                    "this_offset": this_offset,
                    "old_method_count": old_count,
                    "new_method_count": new_count,
                    "count_delta": count_delta,
                    "old_rva": old_rva,
                    "new_rva": new_rva,
                    "rva_changed": rva_changed,
                    "slot_changes_count": len(slot_changes),
                    "slot_changes": slot_changes[:20],  # Keep report compact
                    "has_canon_shift": table_has_canon_shift
                })

        return {
            "summary": {
                "binaries_evaluated": sorted(list(binaries_seen)),
                "total_old_vtables": len(self.old_data),
                "total_new_vtables": len(self.new_data),
                "added_vtables_count": len(added_tables),
                "removed_vtables_count": len(removed_tables),
                "modified_vtables_count": len(modified_tables),
                "canonical_shifts_count": len(canonical_shifts),
            },
            "canonical_shifts": canonical_shifts,
            "modified_tables": modified_tables,
            "added_tables": added_tables,
            "removed_tables": removed_tables
        }


def format_markdown_report(diff_res: Dict[str, Any], old_path: str, new_path: str) -> str:
    """Generates structured Markdown ABI comparison report."""
    summary = diff_res["summary"]
    shifts = diff_res["canonical_shifts"]
    modified = diff_res["modified_tables"]
    added = diff_res["added_tables"]
    removed = diff_res["removed_tables"]

    md = []
    md.append("# Source 2 Virtual Method Table ABI Diff Report\n")
    md.append(f"- **Baseline Build**: `{os.path.basename(old_path)}`")
    md.append(f"- **Target Build**: `{os.path.basename(new_path)}`")
    md.append(f"- **Binaries Audited**: `{', '.join(summary['binaries_evaluated'])}`\n")

    md.append("## Executive Summary\n")
    md.append("| Metric | Count | Status |")
    md.append("| :--- | :--- | :--- |")
    md.append(f"| Baseline Total VTables | {summary['total_old_vtables']:,} | Baseline |")
    md.append(f"| Target Total VTables | {summary['total_new_vtables']:,} | Target |")
    md.append(f"| Added VTables | +{summary['added_vtables_count']:,} | Notice |")
    md.append(f"| Removed VTables | -{summary['removed_vtables_count']:,} | Warning |")
    md.append(f"| Modified VTables | {summary['modified_vtables_count']:,} | Changed |")
    
    canon_status = "**CRITICAL ABI BREAK**" if summary["canonical_shifts_count"] > 0 else "PASS (Clean)"
    md.append(f"| Canonical Slot Shifts | **{summary['canonical_shifts_count']}** | {canon_status} |\n")

    if shifts:
        md.append("> [!WARNING]")
        md.append(f"> **Critical ABI Shift Alert**: Detected {len(shifts)} canonical interface method displacements! Virtual dispatch callers (e.g. game/interfaces/) must adjust slot indices accordingly.\n")
        md.append("### Canonical Interface Slot Displacements\n")
        md.append("| Binary | Interface Class | Method Symbol | Old Slot | New Slot | Delta | Severity |")
        md.append("| :--- | :--- | :--- | :--- | :--- | :--- | :--- |")
        for s in shifts:
            old_s = f"[{s['old_slot']}]" if s['old_slot'] is not None else "-"
            new_s = f"[{s['new_slot']}]" if s['new_slot'] is not None else "DELETED"
            delta = f"{s['delta']:+d}" if s['delta'] is not None else "N/A"
            md.append(f"| `{s['binary']}` | `{s['class_name']}` | `{s['symbol']}` | {old_s} | {new_s} | `{delta}` | **{s['severity']}** |")
        md.append("\n")

    # Slot Count Changes
    count_changes = [m for m in modified if m["count_delta"] != 0]
    if count_changes:
        md.append("### VTable Slot Count Changes (ABI Growth / Shrinkage)\n")
        md.append("| Binary | Class Name | Old Slots | New Slots | Delta | Old RVA | New RVA |")
        md.append("| :--- | :--- | :--- | :--- | :--- | :--- | :--- |")
        for m in sorted(count_changes, key=lambda x: abs(x["count_delta"]), reverse=True)[:30]:
            sign = "+" if m["count_delta"] > 0 else ""
            md.append(f"| `{m['binary']}` | `{m['class_name']}` | {m['old_method_count']} | {m['new_method_count']} | `{sign}{m['count_delta']}` | `{m['old_rva']}` | `{m['new_rva']}` |")
        if len(count_changes) > 30:
            md.append(f"\n*...and {len(count_changes) - 30} additional tables with slot count changes.*")
        md.append("\n")

    # Added VTables
    if added:
        md.append("### Newly Discovered Virtual Method Tables\n")
        md.append("| Binary | Class Name | This Offset | Slot Count | VTable RVA |")
        md.append("| :--- | :--- | :--- | :--- | :--- |")
        for a in added[:25]:
            md.append(f"| `{a['binary']}` | `{a['class_name']}` | +0x{a['this_offset']:x} | {a['method_count']} | `{a['vtable_rva']}` |")
        if len(added) > 25:
            md.append(f"\n*...and {len(added) - 25} additional newly added vtables.*")
        md.append("\n")

    # Removed VTables
    if removed:
        md.append("### Deprecated / Removed Virtual Method Tables\n")
        md.append("| Binary | Class Name | This Offset | Old Slot Count | Old VTable RVA |")
        md.append("| :--- | :--- | :--- | :--- | :--- |")
        for r in removed[:25]:
            md.append(f"| `{r['binary']}` | `{r['class_name']}` | +0x{r['this_offset']:x} | {r['method_count']} | `{r['vtable_rva']}` |")
        if len(removed) > 25:
            md.append(f"\n*...and {len(removed) - 25} additional deprecated vtables.*")
        md.append("\n")

    return "\n".join(md)


def main():
    parser = argparse.ArgumentParser(
        description="Source 2 VTable ABI Diffing & Shift Detection Tool"
    )
    parser.add_argument(
        "--old",
        required=True,
        help="Path to baseline vtables.json file (e.g. previous build)"
    )
    parser.add_argument(
        "--new",
        default="data/vtables.json",
        help="Path to target vtables.json file (default: data/vtables.json)"
    )
    parser.add_argument(
        "--output-report",
        help="Optional path to write generated markdown/json report"
    )
    parser.add_argument(
        "--binary",
        help="Filter analysis to specific binary (e.g. client.dll, server.dll)"
    )
    parser.add_argument(
        "--class",
        dest="class_filter",
        help="Filter analysis to classes containing substring"
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output raw diff results in JSON format"
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit with non-zero error code if canonical ABI displacements are found"
    )

    args = parser.parse_args()

    if not os.path.isfile(args.old):
        print(f"[-] Error: Baseline file not found: {args.old}", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(args.new):
        print(f"[-] Error: Target file not found: {args.new}", file=sys.stderr)
        sys.exit(1)

    print(f"[*] Loading baseline: {args.old}...")
    with open(args.old, "r", encoding="utf-8") as f:
        old_data = json.load(f)

    print(f"[*] Loading target:   {args.new}...")
    with open(args.new, "r", encoding="utf-8") as f:
        new_data = json.load(f)

    print(f"[*] Diffing {len(old_data):,} baseline vtables against {len(new_data):,} target vtables...")
    engine = VTableDiffEngine(old_data, new_data)
    diff_res = engine.compute_diff(binary_filter=args.binary, class_filter=args.class_filter)

    summary = diff_res["summary"]
    shifts = diff_res["canonical_shifts"]

    # Print summary to stdout
    print("\n" + "=" * 65)
    print(" VTABLE ABI DIFF ANALYSIS SUMMARY")
    print("=" * 65)
    print(f"  Binaries Audited:        {', '.join(summary['binaries_evaluated'])}")
    print(f"  Baseline Total VTables:  {summary['total_old_vtables']:,}")
    print(f"  Target Total VTables:    {summary['total_new_vtables']:,}")
    print(f"  Added VTables:           +{summary['added_vtables_count']:,}")
    print(f"  Removed VTables:         -{summary['removed_vtables_count']:,}")
    print(f"  Modified VTables:        {summary['modified_vtables_count']:,}")
    print(f"  Canonical Slot Shifts:   {summary['canonical_shifts_count']}")
    print("=" * 65)

    if shifts:
        print("\n[!] CRITICAL WARNING: CANONICAL INTERFACE SHIFTS DETECTED:")
        for s in shifts:
            old_s = f"[{s['old_slot']}]" if s['old_slot'] is not None else "-"
            new_s = f"[{s['new_slot']}]" if s['new_slot'] is not None else "DELETED"
            delta = f"({s['delta']:+d})" if s['delta'] is not None else ""
            print(f"    - {s['binary']} :: {s['class_name']} :: {s['symbol']}: {old_s} -> {new_s} {delta}")

    if args.json:
        if args.output_report:
            with open(args.output_report, "w", encoding="utf-8") as f:
                json.dump(diff_res, f, indent=2)
            print(f"\n[+] Wrote JSON diff report to: {args.output_report}")
        else:
            print(json.dumps(diff_res, indent=2))
    else:
        report_md = format_markdown_report(diff_res, args.old, args.new)
        if args.output_report:
            with open(args.output_report, "w", encoding="utf-8") as f:
                f.write(report_md)
            print(f"\n[+] Wrote Markdown diff report to: {args.output_report}")

    if args.strict and len(shifts) > 0:
        print(f"\n[-] Strict mode failure: {len(shifts)} canonical slot shifts detected.", file=sys.stderr)
        sys.exit(2)

    sys.exit(0)


if __name__ == "__main__":
    main()
