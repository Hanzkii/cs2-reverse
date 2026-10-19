import struct
import re

with open(r"versions/archive/2026-10-15_25966348\client.dll", "rb") as f:
    data = f.read()

# Let's search for entity list / handle masks in client.dll
# Usually: entity index = handle & 0x7FFF (32768) or handle & 0x1FFF (8192) or 0x3FFF (16384)
# In Source 2:
# ENT_ENTRY_MASK is typically 0x7fff (15 bits) or 0x3fff (14 bits)
for mask in [0x7fff, 0x3fff, 0x1fff]:
    mask_bytes = struct.pack("<I", mask)
    count = data.count(mask_bytes)
    print(f"Mask 0x{mask:x} occurs {count} times in client.dll")
