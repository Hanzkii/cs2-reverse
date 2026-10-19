with open(r"versions/archive/2026-10-15_25966348\networksystem.dll", "rb") as f:
    data = f.read()

import re
m = re.search(b'message CMsg_CVars|CMsg_CVars', data)
if m:
    off = m.start()
    print("Found CMsg_CVars at 0x%x" % off)
    print("Context:", data[max(0, off-50):min(len(data), off+250)])
