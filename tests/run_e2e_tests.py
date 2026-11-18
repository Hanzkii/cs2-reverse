#!/usr/bin/env python3
"""
CS2 Physics & Telemetry Lab — Unified E2E Test & Guardrails Runner
Executes Node.js E2E test runner, validates build pipelines, and enforces repository guardrails.
"""

import sys
import subprocess
import argparse
import pathlib
import json
import re

def parse_args():
    parser = argparse.ArgumentParser(description="CS2 Physics & Telemetry Lab E2E Test Runner")
    parser.add_argument("--tier", type=str, default="all", help="Test tier to execute: 1, 2, 3, 4, or all")
    parser.add_argument("--feature", type=str, default=None, help="Filter tests by feature ID (1-41) or name")
    parser.add_argument("-v", "--verbose", action="store_true", help="Print verbose test results")
    parser.add_argument("--json", action="store_true", help="Output machine-readable JSON telemetry")
    parser.add_argument("--checklist", action="store_true", help="Print 41-feature verification checklist")
    parser.add_argument("--skip-build-check", action="store_true", help="Skip python build pipeline check")
    return parser.parse_args()

def run_node_runner(args, repo_root):
    runner_script = repo_root / "tests" / "e2e" / "runner.js"
    cmd = [sys.executable if False else "node", str(runner_script)]

    if args.tier:
        cmd.extend(["--tier", args.tier])
    if args.feature:
        cmd.extend(["--feature", args.feature])
    if args.verbose:
        cmd.append("--verbose")
    if args.json:
        cmd.append("--json")
    if args.checklist:
        cmd.append("--checklist")

    result = subprocess.run(cmd, cwd=str(repo_root))
    return result.returncode == 0

def verify_build_pipeline(repo_root):
    # 1. generate_portal_data.py
    portal_script = repo_root / "tools" / "generate_portal_data.py"
    res1 = subprocess.run([sys.executable, str(portal_script)], cwd=str(repo_root), capture_output=True, text=True)
    if res1.returncode != 0:
        print(f"[FAIL] tools/generate_portal_data.py failed:\n{res1.stderr}")
        return False

    # 2. generate_docs_bundle.py
    bundle_script = repo_root / "tools" / "generate_docs_bundle.py"
    res2 = subprocess.run([sys.executable, str(bundle_script)], cwd=str(repo_root), capture_output=True, text=True)
    if res2.returncode != 0:
        print(f"[FAIL] tools/generate_docs_bundle.py failed:\n{res2.stderr}")
        return False

    # 3. Output files check
    portal_data = repo_root / "docs" / "portal-data.js"
    docs_data = repo_root / "docs" / "docs-data.js"
    if not portal_data.exists() or portal_data.stat().st_size < 1000000:
        print("[FAIL] docs/portal-data.js missing or undersized")
        return False
    if not docs_data.exists() or docs_data.stat().st_size < 100000:
        print("[FAIL] docs/docs-data.js missing or undersized")
        return False

    return True

def verify_guardrails(repo_root):
    # Rule 1: Zero absolute local paths (scanned across docs, tools, tests, sdk, game, and README.md)
    path_regex = re.compile(r"[a-zA-Z]:[\\/](?:Users|hannu)", re.IGNORECASE)
    scanned_extensions = {".js", ".md", ".json", ".py", ".html", ".css", ".cpp", ".h"}
    target_dirs = ["docs", "tools", "tests", "sdk", "game"]

    for d_name in target_dirs:
        dir_path = repo_root / d_name
        if not dir_path.exists():
            continue
        for p in dir_path.rglob("*"):
            if not p.is_file():
                continue
            rel_str = str(p.relative_to(repo_root)).replace("\\", "/")
            if p.suffix.lower() in scanned_extensions:
                try:
                    content = p.read_text(encoding="utf-8", errors="ignore")
                    matches = path_regex.findall(content)
                    if matches:
                        print(f"[FAIL] Path sanitization violation in {rel_str}: found {matches[:3]}")
                        return False
                except Exception:
                    pass

    readme_path = repo_root / "README.md"
    if readme_path.exists():
        content = readme_path.read_text(encoding="utf-8", errors="ignore")
        matches = path_regex.findall(content)
        if matches:
            print(f"[FAIL] Path sanitization violation in README.md: found {matches[:3]}")
            return False

    # Rule 2: Zero prohibited binaries in git tracked or working tree
    prohibited_extensions = {".exe", ".dll", ".obj", ".lib", ".so", ".pdb"}
    for d_name in target_dirs:
        dir_path = repo_root / d_name
        if not dir_path.exists():
            continue
        for p in dir_path.rglob("*"):
            if p.is_file() and p.suffix.lower() in prohibited_extensions:
                print(f"[FAIL] Prohibited binary file detected: {p.relative_to(repo_root)}")
                return False

    return True

def main():
    args = parse_args()
    repo_root = pathlib.Path(__file__).resolve().parent.parent

    # Step 1: Execute Node E2E Runner
    node_ok = run_node_runner(args, repo_root)
    if not node_ok:
        sys.exit(1)

    # Step 2: Build Pipeline & Guardrails (unless skipped or json output mode)
    if not args.json and not args.skip_build_check:
        build_ok = verify_build_pipeline(repo_root)
        if not build_ok:
            print("[FAIL] Build pipeline verification failed")
            sys.exit(1)

        guardrails_ok = verify_guardrails(repo_root)
        if not guardrails_ok:
            print("[FAIL] Repository guardrails verification failed")
            sys.exit(1)

        print("[PASS] Build pipeline & repository guardrails verified successfully.")

    sys.exit(0)

if __name__ == "__main__":
    main()
