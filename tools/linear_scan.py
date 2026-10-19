import explore_client as ec
import struct

target_va = 0x181CA25F4
text_sec = ec.sections[0]
roff = text_sec['roff']
rsize = text_sec['rsize']
base_va = ec.image_base + text_sec['rva']
const_check = target_va - base_va - 4

print(f"Target VA: 0x{target_va:X}, const_check: {const_check}")
t_data = ec.buf[roff:roff+rsize]

# Scan 4-byte aligned or unaligned
import array
# Convert to int32 array for blazing fast check
# In 25MB, let's scan unaligned:
matches = []
for k in range(0, len(t_data) - 4):
    # To be extremely fast, check last byte first or check in chunks
    # k + val32 == const_check -> val32 = const_check - k
    pass
