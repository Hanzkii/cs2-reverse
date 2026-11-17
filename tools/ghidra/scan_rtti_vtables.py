#!/usr/bin/env python3
"""
scan_rtti_vtables.py - MSVC x64 RTTI Scanner & VTable Auto-Discovery Engine for Ghidra

Connects to the active Ghidra instance via GhidraMCP HTTP REST API (port 8089)
to scan PE .rdata sections for MSVC x64 Run-Time Type Information (RTTI),
automatically discovering:
  1. Complete Object Locators (signature == 1, pSelf == RVA)
  2. Demangled C++ class/interface names from TypeDescriptors (.?AV<Name>@@)
  3. Authoritative VTable addresses in .rdata
  4. Virtual method slot counts (consecutive code pointers into .text)
  5. Optionally labels and bookmarks discovered VTables directly in Ghidra!
"""

import os
import sys
import json
import argparse
import urllib.request
import urllib.parse
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple

DEFAULT_PORT = 8089
DEFAULT_VERSION_FOLDER = "/2026-11-11_26598758"


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

    def post_json(self, endpoint: str, data_dict: dict, timeout=120) -> dict:
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

    def run_inline_script(self, java_code: str, program: str = "") -> dict:
        payload = {"code": java_code}
        if program:
            payload["program"] = program
        return self.post_json("/run_script_inline", payload, timeout=180)


def build_rtti_scanner_script(annotate: bool = False, max_classes: int = 500) -> str:
    """Generates Java Ghidra script to scan .rdata for MSVC x64 RTTI and VTables."""
    annotate_str = "true" if annotate else "false";
    return f"""
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.BookmarkManager;
import ghidra.program.model.listing.BookmarkType;
import java.util.*;

boolean doAnnotate = {annotate_str};
int maxClasses = {max_classes};

Memory mem = currentProgram.getMemory();
Address base = currentProgram.getImageBase();
long baseVal = base.getOffset();

MemoryBlock rdata = mem.getBlock(".rdata");
if (rdata == null) {{
    println("JSON_RESULT:{{\\"error\\": \\"No .rdata block found\\"}}");
    return;
}}

Address rdataStart = rdata.getStart();
long rdataSize = rdata.getSize();

ReferenceManager refMgr = currentProgram.getReferenceManager();
SymbolTable symTable = currentProgram.getSymbolTable();

// Build inverted pointer index across .rdata in single fast O(N) pass
Map<Long, List<Address>> ptrMap = new HashMap<>();
for (long off = 0; off + 8 <= rdataSize; off += 8) {{
    try {{
        long val = mem.getLong(rdataStart.add(off));
        if (val >= baseVal && val < baseVal + currentProgram.getMaxAddress().getOffset()) {{
            List<Address> list = ptrMap.get(val);
            if (list == null) {{
                list = new ArrayList<>();
                ptrMap.put(val, list);
            }}
            list.add(rdataStart.add(off));
        }}
    }} catch (Exception ex) {{}}
}}

int txId = -1;
if (doAnnotate) {{
    txId = currentProgram.startTransaction("Annotate MSVC RTTI VTables");
}}

StringBuilder sb = new StringBuilder();
sb.append("JSON_RESULT:{{");
sb.append("\\"binary\\": \\"").append(currentProgram.getName()).append("\\",");
sb.append("\\"classes\\": [");

int foundCount = 0;

try {{
    for (long off = 0; off + 24 <= rdataSize; off += 4) {{
        Address locAddr = rdataStart.add(off);
        int sig = mem.getInt(locAddr);
        if (sig != 1) continue;
        
        int pSelf = mem.getInt(locAddr.add(20));
        long expectedRva = locAddr.getOffset() - baseVal;
        if (pSelf != (int)expectedRva) continue;
        
        // Valid RTTI Complete Object Locator!
        int pTypeDesc = mem.getInt(locAddr.add(12));
        Address typeDescNameAddr = base.add((long)pTypeDesc + 16);
        
        StringBuilder nameSb = new StringBuilder();
        try {{
            for (int i = 0; i < 128; i++) {{
                byte b = mem.getByte(typeDescNameAddr.add(i));
                if (b == 0) break;
                nameSb.append((char)b);
            }}
        }} catch (Exception e) {{}}
        
        String mangled = nameSb.toString();
        if (mangled.isEmpty() || !mangled.startsWith(".?A")) continue;
        
        // Clean class name
        String className = mangled;
        if (mangled.startsWith(".?AV") || mangled.startsWith(".?AU")) {{
            className = mangled.substring(4);
            if (className.endsWith("@@")) {{
                className = className.substring(0, className.length() - 2);
            }}
        }}
        
        // Find VTable pointer pointing to this locator using inverted index
        List<Address> ptrs = ptrMap.get(locAddr.getOffset());
        Address vtableAddr = null;
        if (ptrs != null && !ptrs.isEmpty()) {{
            // In MSVC x64, the vtable begins at (ptr + 8)
            vtableAddr = ptrs.get(0).add(8);
        }}
        
        int slotCount = 0;
        if (vtableAddr != null) {{
            // Count consecutive function pointers
            for (int slot = 0; slot < 256; slot++) {{
                try {{
                    long ptr = mem.getLong(vtableAddr.add(slot * 8));
                    Address target = base.add(ptr - baseVal);
                    MemoryBlock blk = mem.getBlock(target);
                    if (blk != null && blk.isExecute()) {{
                        slotCount++;
                    }} else {{
                        break;
                    }}
                }} catch (Exception ex) {{
                    break;
                }}
            }}
            
            if (doAnnotate && slotCount > 0) {{
                try {{
                    symTable.createLabel(vtableAddr, "vftable_" + className, SourceType.USER_DEFINED);
                    BookmarkManager bmMgr = currentProgram.getBookmarkManager();
                    bmMgr.setBookmark(vtableAddr, BookmarkType.NOTE, "Source2 RTTI VTable", className + " (" + slotCount + " slots)");
                }} catch (Exception ex) {{}}
            }}
        }}
        
        if (foundCount > 0) sb.append(",");
        sb.append("{{");
        sb.append("\\"class_name\\": \\"").append(className).append("\\",");
        sb.append("\\"mangled_name\\": \\"").append(mangled).append("\\",");
        sb.append("\\"locator_va\\": \\"0x").append(Long.toHexString(locAddr.getOffset())).append("\\",");
        sb.append("\\"locator_rva\\": \\"0x").append(Long.toHexString(expectedRva)).append("\\",");
        if (vtableAddr != null) {{
            sb.append("\\"vtable_va\\": \\"0x").append(Long.toHexString(vtableAddr.getOffset())).append("\\",");
            sb.append("\\"vtable_rva\\": \\"0x").append(Long.toHexString(vtableAddr.getOffset() - baseVal)).append("\\",");
            sb.append("\\"slot_count\\": ").append(slotCount);
        }} else {{
            sb.append("\\"vtable_va\\": null,");
            sb.append("\\"vtable_rva\\": null,");
            sb.append("\\"slot_count\\": 0");
        }}
        sb.append("}}");
        
        foundCount++;
        if (foundCount >= maxClasses) break;
    }}
}} finally {{
    if (doAnnotate && txId != -1) {{
        currentProgram.endTransaction(txId, true);
    }}
}}

sb.append("],");
sb.append("\\"total_classes\\": ").append(foundCount);
sb.append("}}");
println(sb.toString());
"""


def parse_json_from_output(raw_output: str) -> Optional[dict]:
    for line in raw_output.splitlines():
        if line.startswith("JSON_RESULT:"):
            json_str = line[len("JSON_RESULT:"):].strip()
            try:
                return json.loads(json_str)
            except Exception as e:
                print(f"[-] JSON parse error: {e}")
                return None
    return None


def print_rtti_summary(data: dict):
    if "error" in data:
        print(f"[-] Error: {data['error']}")
        return

    classes = data.get("classes", [])
    total = data.get("total_classes", len(classes))
    bin_name = data.get("binary", "unknown")

    print("==========================================================================================================")
    print(f"  MSVC x64 RTTI & VTABLE SCAN REPORT: {bin_name}")
    print(f"  Total Polymorphic Classes Recovered: {total}")
    print("==========================================================================================================")
    print(f"  {'CLASS NAME':<40} | {'VTABLE RVA':<12} | {'VTABLE VA':<18} | {'SLOTS'}")
    print("  " + "-" * 85)

    with_vtables = [c for c in classes if c.get("vtable_rva")]
    for c in with_vtables[:40]:
        print(f"  {c['class_name'][:38]:<40} | {c['vtable_rva']:<12} | {c['vtable_va']:<18} | {c['slot_count']}")

    if len(with_vtables) > 40:
        print(f"  ... and {len(with_vtables) - 40} more polymorphic classes")

    print(f"\n[+] Classes with Valid VTables: {len(with_vtables)} / {len(classes)}\n")


def scan_binary_rtti(client: GhidraClient, binary_name: str, annotate: bool = False, max_classes: int = 500, folder: str = DEFAULT_VERSION_FOLDER) -> Optional[dict]:
    full_path = f"{folder}/{binary_name}"
    print(f"[*] Opening {full_path} in Ghidra...")
    open_res = client.open_program(full_path)
    if "error" in open_res:
        print(f"[-] Failed to open {binary_name}: {open_res['error']}")
        return None

    code = build_rtti_scanner_script(annotate=annotate, max_classes=max_classes)
    print(f"[*] Scanning .rdata for MSVC x64 RTTI descriptors in {binary_name}...")
    res = client.run_inline_script(code, program=binary_name)
    data = parse_json_from_output(res.get("result", ""))

    if not data:
        print(f"[-] Script execution returned non-JSON:\n{res.get('result', '')[:400]}")
        return None

    if annotate:
        save_res = client.save_program(binary_name)
        print(f"[+] Saved annotated symbols to {binary_name} in Ghidra database.")

    return data


def main():
    parser = argparse.ArgumentParser(description="MSVC x64 RTTI Scanner & VTable Auto-Discovery Engine for Ghidra")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="GhidraMCP HTTP port (default: 8089)")
    parser.add_argument("--binary", type=str, default="", help="Specific binary to scan (e.g. vphysics2.dll)")
    parser.add_argument("--annotate", action="store_true", help="Annotate labels and bookmarks into Ghidra")
    parser.add_argument("--max", type=int, default=500, help="Maximum classes to recover")
    parser.add_argument("--export-json", type=str, help="Path to export JSON results")
    parser.add_argument("--folder", type=str, default=DEFAULT_VERSION_FOLDER, help="Project version folder")
    args = parser.parse_args()

    client = GhidraClient(port=args.port)
    conn_ok, conn_msg = client.check_connection()
    if not conn_ok:
        print(f"[-] Failed to connect to Ghidra on port {args.port}: {conn_msg}")
        sys.exit(1)

    print(f"[+] Connected to Ghidra: {conn_msg}")

    target = args.binary or "vphysics2.dll"
    data = scan_binary_rtti(client, target, annotate=args.annotate, max_classes=args.max, folder=args.folder)

    if data:
        print_rtti_summary(data)
        if args.export_json:
            out_p = Path(args.export_json).resolve()
            with open(out_p, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2)
            print(f"[+] Exported {data.get('total_classes', 0)} RTTI classes to {out_p.name}")


if __name__ == "__main__":
    main()
