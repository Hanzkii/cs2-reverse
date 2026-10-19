with open(r"versions/archive/2026-10-15_25966348\networksystem.dll", "rb") as f:
    data = f.read()

import re
for name in [b"GetCvarValue", b"RespondCvarValue"]:
    for m in re.finditer(name, data):
        off = m.start()
        print(f"Match {name.decode()} at 0x{off:x}")
        print("Context:", data[max(0, off-30):min(len(data), off+250)])
