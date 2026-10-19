import os
import struct
import re

bin_dir = os.environ.get("CS2_BIN_DIR", ".")
for dll in ["tier0.dll", "engine2.dll"]:
    dll_path = os.path.join(bin_dir, dll)
    if not os.path.exists(dll_path):
        continue
    with open(dll_path, "rb") as f:
        data = f.read()
    
    cvars = re.findall(b"VEngineCvar[a-zA-Z0-9_]+", data)
    print(f"{dll}: VEngineCvar matches:", set(c.decode() for c in cvars))
    
    cvars2 = re.findall(b"Cvar[0-9_]+", data)
    print(f"{dll}: Cvar matches:", set(c.decode() for c in cvars2))

    rtti = re.findall(b"\\.\\?AV[a-zA-Z0-9_]*Cvar[a-zA-Z0-9_]*@@", data)
    print(f"{dll}: Cvar RTTI matches:", set(r.decode() for r in rtti))

    rtti_convar = re.findall(b"\\.\\?AV[a-zA-Z0-9_]*ConVar[a-zA-Z0-9_]*@@", data)
    print(f"{dll}: ConVar RTTI matches:", set(r.decode() for r in rtti_convar))
