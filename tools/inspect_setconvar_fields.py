import struct

with open(r"versions/archive/2026-10-15_25966348\networksystem.dll", "rb") as f:
    data = f.read()

# Let's search for "CNETMsg_SetConVar" in the protobuf descriptor table
off = data.find(b"CNETMsg_SetConVar\x00")
print(f"CNETMsg_SetConVar string at 0x{off:x}")

# Find strings nearby (field names)
start = max(0, off - 300)
end = min(len(data), off + 500)
chunk = data[start:end]

import re
strs = re.findall(b"[a-zA-Z0-9_]{3,}", chunk)
print("Strings near CNETMsg_SetConVar:")
for s in strs:
    print("  ", s.decode('latin-1'))
