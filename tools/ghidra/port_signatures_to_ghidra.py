#!/usr/bin/env python3
"""
port_signatures_to_ghidra.py - Source 2 CS2 Canonical Function Signatures Porter

Dual-mode script:
1. When executed inside Ghidra (via Headless Analyzer or Ghidra Script Manager):
   - Reads data/function_signatures.json
   - Resolves target addresses from ImageBase + current_rva
   - Disassembles, creates/renames functions with namespace resolution
   - Applies structured PLATE comments and bookmarks under category 'CS2 Canonical Signature'

2. When executed from command line (via standard Python 3):
   - Serves as the headless orchestration engine
   - Identifies all target binaries across data/function_signatures.json
   - Orchestrates analyzeHeadless execution across all programs in the project
   - Supports both Headless batch processing and MCP bridge live synchronization
"""

import sys
import os
import json
import argparse
import subprocess
from pathlib import Path

# =============================================================================
# GHIDRA IN-PROCESS SCRIPT EXECUTION (Jython / PyGhidra)
# =============================================================================

def run_inside_ghidra():
    """Execution path when running inside Ghidra environment."""
    from ghidra.program.model.symbol import SourceType
    from ghidra.program.model.listing import BookmarkType

    prog = currentProgram  # injected by Ghidra
    if not prog:
        print("[-] ERROR: No active program in Ghidra context.")
        return

    raw_name = prog.getName()
    prog_name = raw_name.lower()
    if "/" in prog_name:
        prog_name = prog_name.split("/")[-1]
    if "\\" in prog_name:
        prog_name = prog_name.split("\\")[-1]
    if "." in prog_name:
        parts = prog_name.split(".")
        if len(parts) > 2 and parts[-1].isdigit():
            prog_name = ".".join(parts[:-1])

    print("[*] Active Ghidra Program: {} (normalized: {})".format(raw_name, prog_name))

    # Resolve signatures file
    candidates = [
        "data/function_signatures.json",
        "../data/function_signatures.json",
        "../../data/function_signatures.json",
    ]
    sigs_path = None
    for cand in candidates:
        if os.path.exists(cand):
            sigs_path = os.path.abspath(cand)
            break

    if not sigs_path:
        print("[-] ERROR: Unable to locate data/function_signatures.json")
        return

    print("[*] Loading signatures from: {}".format(sigs_path))
    with open(sigs_path, "r") as f:
        sigs = json.load(f)

    image_base = prog.getImageBase()
    listing = prog.getListing()
    sym_table = prog.getSymbolTable()
    bm_mgr = prog.getBookmarkManager()

    tx = prog.startTransaction("Port CS2 Canonical Signatures")
    success = False
    ported = 0
    skipped = 0

    try:
        for entry in sigs:
            target_bin = entry.get("binary", "").lower()
            if target_bin != prog_name:
                skipped += 1
                continue

            sig_id = entry.get("id", "")
            name = entry.get("name", "")
            category = entry.get("category", "")
            description = entry.get("description", "")
            pattern = entry.get("pattern", "")
            status = entry.get("status", "")
            build = str(entry.get("last_verified_build", ""))

            rva_str = entry.get("current_rva") or entry.get("baseline_rva", "")
            if not rva_str:
                print("[-] Skipping {}: Missing RVA".format(sig_id))
                continue

            rva = int(rva_str, 16) if rva_str.lower().startswith("0x") else int(rva_str)
            target_addr = image_base.add(rva)

            # Ensure disassembled
            if not listing.getInstructionAt(target_addr):
                try:
                    disassemble(target_addr)
                except Exception:
                    pass

            # Resolve / create function
            func = getFunctionAt(target_addr)
            if not func:
                try:
                    func = createFunction(target_addr, None)
                except Exception:
                    func = getFunctionAt(target_addr)

            if func:
                if "::" in name:
                    ns_path, method_name = name.rsplit("::", 1)
                    ns = sym_table.getOrCreateNameSpace(
                        prog.getGlobalNamespace(), ns_path, SourceType.USER_DEFINED
                    )
                    try:
                        func.setParentNamespace(ns)
                        func.setName(method_name, SourceType.USER_DEFINED)
                    except Exception:
                        func.setName(name.replace("::", "_"), SourceType.USER_DEFINED)
                else:
                    try:
                        func.setName(name, SourceType.USER_DEFINED)
                    except Exception as ex:
                        print("[!] Warning setting function name: {}".format(ex))

            # Plate Comment
            plate = (
                "=================================================================\n"
                " Source 2 Canonical Function Signature\n"
                " ID:          {}\n"
                " Symbol:      {}\n"
                " Category:    {}\n"
                " Description: {}\n"
                " Status:      {} (Build: {})\n"
                " Pattern:     {}\n"
                "================================================================="
            ).format(sig_id, name, category, description, status, build, pattern)
            setPlateComment(target_addr, plate)

            # Bookmark
            bm_comment = "[{}] {}: {}".format(category, name, description)
            bm_mgr.setBookmark(target_addr, BookmarkType.INFO, "CS2 Canonical Signature", bm_comment)

            print("  [+] Ported {:<35} at {} (RVA: {})".format(name, target_addr, rva_str))
            ported += 1

        success = True
        print("[+] Finished: {} signatures ported to {} ({} skipped)".format(
            ported, raw_name, skipped
        ))
    finally:
        prog.endTransaction(tx, success)


# =============================================================================
# STANDALONE CLI ORCHESTRATION (Headless & MCP Bridge)
# =============================================================================

def find_repo_root() -> Path:
    """Locate the root of the repository."""
    cur = Path(__file__).resolve().parent
    for _ in range(4):
        if (cur / "data" / "function_signatures.json").exists():
            return cur
        cur = cur.parent
    return Path.cwd()


def load_signatures(sigs_path: Path):
    """Load canonical signatures database."""
    with open(sigs_path, "r", encoding="utf-8") as f:
        return json.load(f)


def run_headless_porting(ghidra_home: Path, project_dir: Path, project_name: str, 
                         folder: str, signatures_path: Path, target_binary: str = None,
                         dry_run: bool = False):
    """Run Ghidra Headless Analyzer to port signatures across all project binaries."""
    analyze_bat = ghidra_home / "support" / "analyzeHeadless.bat"
    if not analyze_bat.exists():
        print(f"[-] ERROR: analyzeHeadless.bat not found at {analyze_bat}")
        return False

    repo_root = find_repo_root()
    java_script = repo_root / "tools" / "ghidra" / "Source2_PortSignatures.java"
    if not java_script.exists():
        print(f"[-] ERROR: Source2_PortSignatures.java not found at {java_script}")
        return False

    sigs = load_signatures(signatures_path)
    binary_counts = {}
    for s in sigs:
        b = s.get("binary")
        if b:
            binary_counts[b] = binary_counts.get(b, 0) + 1

    print("=================================================================")
    print(" CS2 HEADLESS SIGNATURE PORTING ORCHESTRATOR")
    print("=================================================================")
    print(f" Ghidra Home:    {ghidra_home}")
    print(f" Project Dir:    {project_dir}")
    print(f" Project Name:   {project_name}")
    print(f" Version Folder: {folder}")
    print(f" Total Sigs:     {len(sigs)}")
    print(" Target Binaries with Signatures:")
    for b, count in sorted(binary_counts.items()):
        print(f"   - {b:<28} ({count} signatures)")
    print("=================================================================")

    binaries_to_process = [target_binary] if target_binary else sorted(binary_counts.keys())

    project_folder_path = f"{project_name}/{folder}" if folder else project_name

    results = {}
    for binary_name in binaries_to_process:
        print(f"\n[*] Processing {binary_name} ({binary_counts.get(binary_name, 0)} signatures)...")
        if dry_run:
            print(f"  [DRY-RUN] Would execute analyzeHeadless on {project_folder_path} -process {binary_name}")
            results[binary_name] = "DRY-RUN"
            continue

        cmd = [
            str(analyze_bat),
            str(project_dir),
            project_folder_path,
            "-process", binary_name,
            "-noanalysis",
            "-postScript", "Source2_PortSignatures.java",
            str(signatures_path.resolve()),
            "-scriptPath", str(java_script.parent.resolve()),
        ]

        try:
            # Pass empty input to prevent 'pause' hanging on DOUBLE_CLICKED detection
            proc = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                input="",
                timeout=180
            )

            # Inspect output
            lines = [l for l in proc.stdout.splitlines() if "Source2_PortSignatures" in l or "Ported" in l or "ERROR" in l]
            for line in lines[-10:]:
                print(f"  {line}")

            if proc.returncode == 0:
                results[binary_name] = "SUCCESS"
                print(f"  [+] Finished {binary_name}")
            else:
                results[binary_name] = f"EXIT_CODE_{proc.returncode}"
                print(f"  [-] Failed {binary_name} (code {proc.returncode})")
                if proc.stderr:
                    print(f"      Stderr: {proc.stderr[:200]}")

        except subprocess.TimeoutExpired:
            print(f"  [-] Timeout processing {binary_name}")
            results[binary_name] = "TIMEOUT"
        except Exception as ex:
            print(f"  [-] Exception processing {binary_name}: {ex}")
            results[binary_name] = "EXCEPTION"

    print("\n=================================================================")
    print(" PORTING EXECUTION SUMMARY")
    print("=================================================================")
    for b, res in results.items():
        print(f"  {b:<30} : {res}")
    print("=================================================================")
    return all(r in ("SUCCESS", "DRY-RUN") for r in results.values())


def main():
    if "currentProgram" in globals():
        # Running inside Ghidra Jython / PyGhidra
        run_inside_ghidra()
        return

    parser = argparse.ArgumentParser(
        description="Port CS2 Canonical Function Signatures directly into Ghidra."
    )
    parser.add_argument(
        "--ghidra-home",
        default=os.environ.get("GHIDRA_HOME") or os.environ.get("GHIDRA_INSTALL_PATH"),
        help="Path to Ghidra installation root",
    )
    parser.add_argument(
        "--project-dir",
        default=os.environ.get("CS2_DISASSEMBLY_DIR") or os.environ.get("GHIDRA_PROJECT_DIR"),
        help="Path to Ghidra project directory (containing CS2.gpr and CS2.rep)",
    )
    parser.add_argument(
        "--project-name",
        default="CS2",
        help="Name of the Ghidra project (default: CS2)",
    )
    parser.add_argument(
        "--version-folder",
        default="2026-11-11_26598758",
        help="Project subfolder for the target version (default: 2026-11-11_26598758)",
    )
    parser.add_argument(
        "--signatures",
        default=None,
        help="Path to function_signatures.json (default: data/function_signatures.json)",
    )
    parser.add_argument(
        "--target-binary",
        default=None,
        help="Filter to a single binary name (e.g. client.dll, server.dll)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Preview actions without invoking headless modification",
    )

    args = parser.parse_args()

    repo_root = find_repo_root()
    sigs_path = Path(args.signatures) if args.signatures else repo_root / "data" / "function_signatures.json"
    if not sigs_path.exists():
        print(f"[-] ERROR: Signatures file not found at {sigs_path}")
        sys.exit(1)

    ghidra_home = Path(args.ghidra_home) if args.ghidra_home else None
    if not ghidra_home:
        print("[-] ERROR: --ghidra-home not specified and GHIDRA_HOME env var not set.")
        sys.exit(1)

    project_dir = Path(args.project_dir) if args.project_dir else None
    if not project_dir:
        print("[-] ERROR: --project-dir not specified and CS2_DISASSEMBLY_DIR env var not set.")
        sys.exit(1)

    success = run_headless_porting(
        ghidra_home=ghidra_home,
        project_dir=project_dir,
        project_name=args.project_name,
        folder=args.version_folder,
        signatures_path=sigs_path,
        target_binary=args.target_binary,
        dry_run=args.dry_run,
    )

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
