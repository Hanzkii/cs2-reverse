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

with open("interfaces.json") as f:
    interfaces = json.load(f)

print(f"{'Interface':<35} {'Singleton':<14} {'VTable':<14} {'Methods'}")
print("-" * 75)

for item in interfaces:
    singleton = item.get("singleton")
    if not singleton:
        continue
    
    # Read first 8 bytes of singleton
    try:
        mem = req(f"/read_memory?address={singleton}&length=8")
        vtable_ptr = struct.unpack("<Q", bytes(mem["data"]))[0]
        if vtable_ptr == 0:
            continue
        vtable_hex = hex(vtable_ptr)
        
        # Read up to 64 pointers from the vtable to count valid method pointers
        vmem = req(f"/read_memory?address={vtable_hex}&length=512")
        vbytes = bytes(vmem["data"])
        methods = []
        for i in range(0, len(vbytes), 8):
            fn = struct.unpack("<Q", vbytes[i:i+8])[0]
            # Valid code address in engine2.dll is around 0x180000000 .. 0x180450000
            if 0x180000000 <= fn <= 0x180450000:
                methods.append(hex(fn))
            else:
                break
        print(f"{item['interface']:<35} {singleton:<14} {vtable_hex:<14} {len(methods)}")
        item["vtable"] = vtable_hex
        item["methods"] = methods
    except Exception as e:
        print(f"Error {item['interface']}: {e}")

with open("interfaces_with_vtables.json", "w") as f:
    json.dump(interfaces, f, indent=2)
