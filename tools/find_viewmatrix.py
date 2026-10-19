with open(r"versions/archive/2026-10-15_25966348\client.dll", "rb") as f:
    data = f.read()

import re
matches = re.findall(b"[a-zA-Z0-9_]*ViewMatrix[a-zA-Z0-9_]*", data)
print("ViewMatrix strings in client.dll:", set(m.decode() for m in matches))

matches_view = re.findall(b"[a-zA-Z0-9_]*ViewRender[a-zA-Z0-9_]*", data)
print("ViewRender strings in client.dll:", set(m.decode() for m in matches_view))
