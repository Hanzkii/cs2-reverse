with open(r"versions/archive/2026-10-15_25966348\client.dll", "rb") as f:
    data = f.read()

import re
for m in re.finditer(b"GetViewRenderInstance", data):
    print("Match at off 0x%x" % m.start())
    print("Context:", data[max(0, m.start()-50):min(len(data), m.start()+100)])
