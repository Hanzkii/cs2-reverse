import os
import struct
import re

bin_dir = os.environ.get("CS2_BIN_DIR", ".")
for dll in ["engine2.dll", "networksystem.dll", "server.dll"]:
    dll_path = os.path.join(bin_dir, dll)
    if not os.path.exists(dll_path):
        continue
    with open(dll_path, "rb") as f:
        data = f.read()
    
    # 1. Interface strings
    interfaces = re.findall(b"GAMEEVENTS[A-Za-z0-9_]*|GameEvents[A-Za-z0-9_]*", data)
    print(f"{dll}: GameEvents interfaces found:", set(i.decode() for i in interfaces))

    # 2. RTTI
    rtti = re.findall(b"\\.\\?AV[A-Za-z0-9_]*GameEvent[A-Za-z0-9_]*@@", data)
    print(f"{dll}: GameEvent RTTI found:", set(r.decode() for r in rtti))
