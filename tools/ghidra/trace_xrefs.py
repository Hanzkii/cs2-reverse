#!/usr/bin/env python3
"""
trace_xrefs.py - Source 2 Cross-Reference (XRef) & Call Graph Analyzer for Ghidra

Connects to the active Ghidra instance via GhidraMCP HTTP REST API (port 8089)
to perform deep cross-reference lookups, call graph analysis, and data/string xref tracing.

Capabilities:
  1. Incoming XRefs (--to <address/symbol>):
     Finds all functions, instructions, and data references pointing TO the target address.
  2. Outgoing Callees (--from <address/symbol>):
     Finds all functions and external calls invoked FROM the target function.
  3. String XRefs (--string <text>):
     Finds all memory locations and code instructions referencing a given string literal.
  4. JSON Export (--json):
     Outputs machine-readable structured reference graphs for automated pipelines.
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


def build_xref_script(target: str, mode: str = "both", max_results: int = 50) -> str:
    """Generates Java Ghidra script to trace references and output JSON."""
    return f"""
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.*;
import java.util.*;

String targetQuery = "{target.replace('\\', '\\\\')}";
String mode = "{mode}";
int maxResults = {max_results};

Address targetAddr = null;

// Try parsing as hex address first
try {{
    if (targetQuery.startsWith("0x") || targetQuery.startsWith("18")) {{
        targetAddr = toAddr(targetQuery);
    }}
}} catch (Exception e) {{}}

// If not an address, try resolving via SymbolTable
if (targetAddr == null) {{
    SymbolTable symTable = currentProgram.getSymbolTable();
    SymbolIterator symIter = symTable.getSymbols(targetQuery);
    if (symIter.hasNext()) {{
        targetAddr = symIter.next().getAddress();
    }}
}}

if (targetAddr == null) {{
    println("JSON_RESULT:{{\\"error\\": \\"Address or symbol not found: " + targetQuery + "\\"}}");
    return;
}}

long imageBase = currentProgram.getImageBase().getOffset();
Function func = getFunctionAt(targetAddr);
if (func == null) {{
    func = getFunctionContaining(targetAddr);
}}

StringBuilder sb = new StringBuilder();
sb.append("JSON_RESULT:{{");
sb.append("\\"query\\": \\"").append(targetQuery).append("\\",");
sb.append("\\"address\\": \\"0x").append(Long.toHexString(targetAddr.getOffset())).append("\\",");
sb.append("\\"rva\\": \\"0x").append(Long.toHexString(targetAddr.getOffset() - imageBase)).append("\\",");
sb.append("\\"function_name\\": \\"").append(func != null ? func.getName() : "none").append("\\",");

ReferenceManager refMgr = currentProgram.getReferenceManager();

// 1. Incoming XRefs
sb.append("\\"incoming_xrefs\\": [");
if (mode.equals("to") || mode.equals("both")) {{
    ReferenceIterator refsTo = refMgr.getReferencesTo(targetAddr);
    int count = 0;
    while (refsTo.hasNext() && count < maxResults) {{
        Reference ref = refsTo.next();
        Address from = ref.getFromAddress();
        Function caller = getFunctionContaining(from);
        
        if (count > 0) sb.append(",");
        sb.append("{{");
        sb.append("\\"from_address\\": \\"0x").append(Long.toHexString(from.getOffset())).append("\\",");
        sb.append("\\"from_rva\\": \\"0x").append(Long.toHexString(from.getOffset() - imageBase)).append("\\",");
        sb.append("\\"caller\\": \\"").append(caller != null ? caller.getName() : "unassigned").append("\\",");
        sb.append("\\"type\\": \\"").append(ref.getReferenceType().toString()).append("\\"");
        sb.append("}}");
        count++;
    }}
}}
sb.append("],");

// 2. Outgoing Callees
sb.append("\\"outgoing_callees\\": [");
if ((mode.equals("from") || mode.equals("both")) && func != null) {{
    Set<Function> called = func.getCalledFunctions(monitor);
    int cCount = 0;
    for (Function callee : called) {{
        if (cCount >= maxResults) break;
        if (cCount > 0) sb.append(",");
        sb.append("{{");
        sb.append("\\"to_address\\": \\"0x").append(Long.toHexString(callee.getEntryPoint().getOffset())).append("\\",");
        sb.append("\\"to_rva\\": \\"0x").append(Long.toHexString(callee.getEntryPoint().getOffset() - imageBase)).append("\\",");
        sb.append("\\"callee\\": \\"").append(callee.getName()).append("\\"");
        sb.append("}}");
        cCount++;
    }}
}}
sb.append("]");

sb.append("}}");
println(sb.toString());
"""


def build_string_xref_script(search_text: str, max_results: int = 50) -> str:
    """Generates Java script to find defined strings and their code references."""
    return f"""
import ghidra.program.model.address.Address;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;

String query = "{search_text.replace('\\', '\\\\')}";
int maxResults = {max_results};
long imageBase = currentProgram.getImageBase().getOffset();
ReferenceManager refMgr = currentProgram.getReferenceManager();

StringBuilder sb = new StringBuilder();
sb.append("JSON_RESULT:{{");
sb.append("\\"string_query\\": \\"").append(query).append("\\",");
sb.append("\\"matches\\": [");

int matchCount = 0;
Data data = getFirstData();
while (data != null && matchCount < maxResults) {{
    StringDataInstance strInst = StringDataInstance.getStringDataInstance(data);
    if (strInst != null && strInst.getStringValue() != null) {{
        String val = strInst.getStringValue();
        if (val.contains(query)) {{
            if (matchCount > 0) sb.append(",");
            sb.append("{{");
            sb.append("\\"string_address\\": \\"0x").append(Long.toHexString(data.getAddress().getOffset())).append("\\",");
            sb.append("\\"string_value\\": \\"").append(val.replace("\"", "\\\"").replace("\n", " ")).append("\\",");
            sb.append("\\"references\\": [");
            
            ReferenceIterator refs = refMgr.getReferencesTo(data.getAddress());
            int refCount = 0;
            while (refs.hasNext() && refCount < 20) {{
                Reference ref = refs.next();
                Address from = ref.getFromAddress();
                Function caller = getFunctionContaining(from);
                if (refCount > 0) sb.append(",");
                sb.append("{{");
                sb.append("\\"from_address\\": \\"0x").append(Long.toHexString(from.getOffset())).append("\\",");
                sb.append("\\"caller\\": \\"").append(caller != null ? caller.getName() : "unassigned").append("\\",");
                sb.append("\\"type\\": \\"").append(ref.getReferenceType().toString()).append("\\"");
                sb.append("}}");
                refCount++;
            }}
            sb.append("]");
            sb.append("}}");
            matchCount++;
        }}
    }}
    data = getNextData(data);
}}

sb.append("]");
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


def print_xref_report(data: dict):
    if "error" in data:
        print(f"[-] Error: {data['error']}")
        return

    print("==========================================================================================")
    print(f"  TARGET: {data.get('query')} ({data.get('function_name', 'none')})")
    print(f"  Address: {data.get('address')} | RVA: {data.get('rva')}")
    print("==========================================================================================")

    in_refs = data.get("incoming_xrefs", [])
    print(f"\n[+] Incoming Cross-References ({len(in_refs)} found):")
    if in_refs:
        print(f"  {'FROM ADDRESS':<16} | {'RVA':<10} | {'CALLER FUNCTION':<40} | {'TYPE'}")
        print("  " + "-" * 80)
        for r in in_refs:
            print(f"  {r['from_address']:<16} | {r['from_rva']:<10} | {r['caller'][:38]:<40} | {r['type']}")
    else:
        print("  (None found)")

    out_calls = data.get("outgoing_callees", [])
    print(f"\n[+] Outgoing Function Calls ({len(out_calls)} found):")
    if out_calls:
        print(f"  {'TARGET ADDRESS':<16} | {'RVA':<10} | {'CALLEE FUNCTION'}")
        print("  " + "-" * 60)
        for c in out_calls:
            print(f"  {c['to_address']:<16} | {c['to_rva']:<10} | {c['callee']}")
    else:
        print("  (None found)")
    print()


def print_string_report(data: dict):
    matches = data.get("matches", [])
    print("==========================================================================================")
    print(f"  STRING SEARCH: \"{data.get('string_query')}\" ({len(matches)} matches)")
    print("==========================================================================================")
    for m in matches:
        print(f"\n  String: \"{m['string_value']}\" @ {m['string_address']}")
        refs = m.get("references", [])
        if refs:
            for r in refs:
                print(f"    <- Referenced from {r['from_address']} in [{r['caller']}] ({r['type']})")
        else:
            print("    <- No direct code references found.")
    print()


def main():
    parser = argparse.ArgumentParser(description="Source 2 Cross-Reference (XRef) & Call Graph Analyzer for Ghidra")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="GhidraMCP HTTP port (default: 8089)")
    parser.add_argument("--program", type=str, default="", help="Target program name in Ghidra (optional)")
    parser.add_argument("--to", type=str, help="Find incoming callers/references to address or symbol")
    parser.add_argument("--from-func", dest="from_func", type=str, help="Find outgoing calls from function")
    parser.add_argument("--target", type=str, help="Find both incoming and outgoing references for address or symbol")
    parser.add_argument("--string", type=str, help="Search for defined string literal and its code references")
    parser.add_argument("--max", type=int, default=50, help="Maximum number of references to return")
    parser.add_argument("--json", action="store_true", help="Output raw JSON format")
    args = parser.parse_args()

    client = GhidraClient(port=args.port)
    conn_ok, conn_msg = client.check_connection()
    if not conn_ok:
        print(f"[-] Failed to connect to Ghidra on port {args.port}: {conn_msg}")
        sys.exit(1)

    if args.string:
        code = build_string_xref_script(args.string, args.max)
        res = client.run_inline_script(code, program=args.program)
        data = parse_json_from_output(res.get("result", ""))
        if not data:
            print(f"[-] Script execution returned non-JSON:\n{res.get('result', '')[:400]}")
            sys.exit(1)
        if args.json:
            print(json.dumps(data, indent=2))
        else:
            print_string_report(data)
        return

    target = args.target or args.to or args.from_func
    if not target:
        print("[-] Must specify --target, --to, --from-func, or --string")
        parser.print_help()
        sys.exit(1)

    mode = "to" if args.to else "from" if args.from_func else "both"
    code = build_xref_script(target, mode, args.max)
    res = client.run_inline_script(code, program=args.program)
    data = parse_json_from_output(res.get("result", ""))

    if not data:
        print(f"[-] Script execution returned non-JSON:\n{res.get('result', '')[:400]}")
        sys.exit(1)

    if args.json:
        print(json.dumps(data, indent=2))
    else:
        print_xref_report(data)


if __name__ == "__main__":
    main()
