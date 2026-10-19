import urllib.request
import json
import struct

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

vmem = req("/read_memory?address=0x180541b38&length=512")
vbytes = bytes(vmem["data"])

print(f"{'Slot':<5} {'Method Addr':<14} {'Disasm / Summary'}")
print("-" * 60)

for slot in range(40):
    fn_ptr = struct.unpack("<Q", vbytes[slot*8 : slot*8+8])[0]
    fn_hex = hex(fn_ptr)
    
    # Disassemble first 2 instructions
    dis = req("/disassemble_bytes", {"start_address": fn_hex, "length": 16})
    ins_str = "; ".join([f"{i.get('mnemonic')} {i.get('operands')}" for i in dis.get("instructions", [])[:3]])
    
    # Try decompile
    dec = req(f"/decompile_function?address={fn_hex}")
    dec_first_line = ""
    if isinstance(dec, str):
        lines = [l.strip() for l in dec.split("\n") if l.strip() and not l.strip().startswith("/*")]
        if len(lines) > 1:
            dec_first_line = " ".join(lines[1:3])
    
    print(f"{slot:<5} {fn_hex:<14} {ins_str:<35} | {dec_first_line[:40]}")
