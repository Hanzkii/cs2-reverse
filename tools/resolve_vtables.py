#!/usr/bin/env python3
"""
tools/resolve_vtables.py - Forwarding proxy to tools/ghidra/resolve_vtables.py.
Maintains backwards compatibility for CLI invocations, scripts, and documentation links.
"""

import sys
import runpy
from pathlib import Path

TARGET = Path(__file__).resolve().parent / "ghidra" / "resolve_vtables.py"

if __name__ == "__main__":
    if not TARGET.exists():
        sys.stderr.write(f"[-] Error: Target script not found: {TARGET}\n")
        sys.exit(1)
    runpy.run_path(str(TARGET), run_name="__main__")
