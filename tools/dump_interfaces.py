import urllib.request
import json
import re

URL = "http://127.0.0.1:8089"

def req(endpoint, data=None):
    url = f"{URL}{endpoint}"
    if data is not None:
        body = json.dumps(data).encode("utf-8")
        r = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
    else:
        r = urllib.request.Request(url)
    with urllib.request.urlopen(r) as resp:
        text = resp.read().decode("utf-8")
        try:
            return json.loads(text)
        except Exception:
            return text

xrefs_text = req("/get_xrefs_to?address=0x180409510")
matches = re.findall(r"From ([0-9a-fA-F]+)", xrefs_text)

print(f"Found {len(matches)} callers to InitializeInterfaceReg:")
interfaces = []

for c in matches:
    call_addr = int(c, 16)
    start_addr = hex(call_addr - 0x15)
    dis = req("/disassemble_bytes", {"start_address": start_addr, "length": 24})
    
    name = "unknown"
    factory = None
    singleton = None
    
    for ins in dis.get("instructions", []):
        ops = ins.get("operands", "")
        if "R8" in ops:
            m = re.search(r"\[(0x[0-9a-fA-F]+)\]", ops)
            if m:
                str_addr = m.group(1)
                mem = req(f"/read_memory?address={str_addr}&length=64")
                raw_bytes = bytes(mem.get("data", []))
                null_pos = raw_bytes.find(b"\x00")
                if null_pos != -1:
                    name = raw_bytes[:null_pos].decode("ascii", errors="replace")
                else:
                    name = raw_bytes.decode("ascii", errors="replace")
        elif "RDX" in ops:
            m = re.search(r"\[(0x[0-9a-fA-F]+)\]", ops)
            if m:
                factory = m.group(1)
    
    # Try reading the factory to see if it returns a singleton instance
    if factory:
        fdis = req("/disassemble_bytes", {"start_address": factory, "length": 16})
        for fins in fdis.get("instructions", []):
            fops = fins.get("operands", "")
            if "RAX" in fops:
                fm = re.search(r"\[(0x[0-9a-fA-F]+)\]", fops)
                if fm:
                    singleton = fm.group(1)

    print(f"[{c}] Interface: {name:<35} Factory: {str(factory):<12} Singleton: {str(singleton)}")
    interfaces.append({
        "caller": c,
        "interface": name,
        "factory": factory,
        "singleton": singleton
    })

with open("interfaces.json", "w") as f:
    json.dump(interfaces, f, indent=2)
