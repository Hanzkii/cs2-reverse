with open(r"versions/archive/2026-10-15_25966348\networksystem.dll", "rb") as f:
    data = f.read()

import re
for m in re.finditer(b"net_SetConVar|SetConVar", data):
    print(f"Match: {m.group(0).decode()} at 0x{m.start():x}")
    # print context
    print("  Context:", data[max(0, m.start()-100):min(len(data), m.start()+200)])
