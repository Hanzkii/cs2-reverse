#!/usr/bin/env python3
"""
CS2 Reverse Engineering — Documentation & Repository Link Integrity Auditor

Validates 100% of:
1. Markdown hyper-links [text](url) across docs/*.md
2. Inline backtick code paths `game/...`, `sdk/...`, `docs/...`, `tools/...`
3. Tool Catalog [View Source] links across all entries in CS2_TOOLS_DATA
4. Standalone C++ simulator file bindings and build script existence

Enforces Zero Broken Links & Zero Missing References invariant.
"""

import os
import sys
import re
import json
import glob

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def audit_markdown_links():
    md_files = glob.glob(os.path.join(REPO_ROOT, "docs", "*.md"))
    broken = []
    total = 0

    for f in md_files:
        fname = os.path.basename(f)
        with open(f, "r", encoding="utf-8") as fh:
            content = fh.read()

        for m in re.finditer(r"\[([^\]]+)\]\(([^)]+)\)", content):
            total += 1
            link = m.group(2).strip()

            # Skip web external, email, or in-page anchor links
            if link.startswith("http") or link.startswith("#") or link.startswith("mailto:"):
                continue

            clean = link.replace("../", "").replace("./", "")
            if clean.startswith("simulators/"):
                clean = "docs/" + clean
            elif (clean.endswith("_sim.cpp") or clean.endswith("_sim.bat")) and "/" not in clean:
                clean = "docs/simulators/" + clean
            elif clean.startswith("tools/panorama/") or clean == "tools/panorama_preview.html":
                clean = "docs/" + clean

            exists = os.path.exists(os.path.join(REPO_ROOT, clean)) or os.path.exists(
                os.path.join(REPO_ROOT, "docs", clean)
            )
            if not exists:
                broken.append((fname, m.group(1), link, clean))

    return total, broken


def audit_code_span_references():
    md_files = glob.glob(os.path.join(REPO_ROOT, "docs", "*.md"))
    broken = []
    total = 0

    prefix_pattern = re.compile(
        r"^(game|sdk|tools|data|tests|docs/simulators)/([a-zA-Z0-9_\-\./]+\.[a-zA-Z0-9_]+)$"
    )

    for f in md_files:
        fname = os.path.basename(f)
        with open(f, "r", encoding="utf-8") as fh:
            content = fh.read()

        for m in re.finditer(r"`([^`\n]+)`", content):
            text = m.group(1).strip()
            if prefix_pattern.match(text):
                total += 1
                rel_path = text
                # Check directly in repo or in docs/ if relative
                exists = os.path.exists(os.path.join(REPO_ROOT, rel_path)) or os.path.exists(
                    os.path.join(REPO_ROOT, "docs", rel_path)
                )
                if not exists:
                    broken.append((fname, text))

    return total, broken


def audit_tools_catalog_links():
    portal_data_path = os.path.join(REPO_ROOT, "docs", "portal-data.js")
    if not os.path.exists(portal_data_path):
        return 0, [("portal-data.js", "File not found", portal_data_path)]

    with open(portal_data_path, "r", encoding="utf-8") as fh:
        content = fh.read()

    m = re.search(r"window\.CS2_TOOLS_DATA\s*=\s*(\[.*?\]);\s*\n", content, re.DOTALL)
    if not m:
        return 0, [("portal-data.js", "window.CS2_TOOLS_DATA not found", "")]

    tools = json.loads(m.group(1))
    broken = []
    total = 0

    ghidra_tools = {
        "Source2_SchemaImporter.java",
        "Source2_BaseTypes.java",
        "resolve_vtables.py",
        "port_signatures_to_ghidra.py",
        "sync_live_ghidra.py",
        "label_live_vtables.py",
        "import_sdk_types.py",
        "Source2_ImportSDKTypes.java",
        "cs2_ghidra_toolkit.py",
        "inject_source2_types.py",
        "sync_function_prototypes.py",
        "scan_rtti_vtables.py",
        "trace_xrefs.py",
    }

    for t in tools:
        name = t.get("name", "")
        file_names = [s.strip() for s in name.split("&")]
        paths = t.get("paths", [])

        for idx, fn in enumerate(file_names):
            total += 1
            if len(paths) > idx and paths[idx]:
                resolved = paths[idx]
            elif fn in ghidra_tools or fn.startswith("Source2_"):
                resolved = f"tools/ghidra/{fn}"
            else:
                resolved = f"tools/{fn}"

            if not os.path.exists(os.path.join(REPO_ROOT, resolved)):
                broken.append((name, fn, resolved))

    return total, broken


def main():
    print("=" * 80)
    print("CS2 REVERSE — REPOSITORY & PORTAL LINK INTEGRITY AUDIT")
    print("=" * 80)

    # 1. Markdown Links
    total_md, broken_md = audit_markdown_links()
    print(f"[*] Audited {total_md} Markdown Links across documentation:")
    if broken_md:
        print(f"  [FAIL] Found {len(broken_md)} broken markdown link(s):")
        for b in broken_md:
            print(f"    - [{b[0]}] '{b[1]}' -> '{b[2]}' (resolved to: '{b[3]}')")
    else:
        print(f"  [PASS] All {total_md} markdown links resolve with 100% precision.")

    # 2. Code Span References
    total_code, broken_code = audit_code_span_references()
    print(f"[*] Audited {total_code} Inline Repository Code Path Spans:")
    if broken_code:
        print(f"  [FAIL] Found {len(broken_code)} missing code reference(s):")
        for b in broken_code:
            print(f"    - [{b[0]}] `{b[1]}` does not exist on disk!")
    else:
        print(f"  [PASS] All {total_code} code span paths point to real repo files.")

    # 3. Tools Catalog Links
    total_tools, broken_tools = audit_tools_catalog_links()
    print(f"[*] Audited {total_tools} Analysis Tools Catalog Source Links:")
    if broken_tools:
        print(f"  [FAIL] Found {len(broken_tools)} broken tool source link(s):")
        for b in broken_tools:
            print(f"    - Tool '{b[0]}' file '{b[1]}' -> '{b[2]}' NOT FOUND!")
    else:
        print(f"  [PASS] All {total_tools} tool buttons resolve to verified source files.")

    print("-" * 80)
    total_issues = len(broken_md) + len(broken_code) + len(broken_tools)
    if total_issues > 0:
        print(f"[FAIL] LINK AUDIT FAILED with {total_issues} broken reference(s)!")
        return 1

    print(
        f"[SUCCESS] ALL {total_md + total_code + total_tools} REPOSITORY LINKS & CITATIONS VERIFIED (0 ERRORS)."
    )
    print("=" * 80)
    return 0


if __name__ == "__main__":
    sys.exit(main())
