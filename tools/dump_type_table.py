import explore_network as en
import string

def clean_str(s):
    if not s or not all(c in string.printable for c in s):
        return ""
    return s[:30]

print("Dumping Field Type Serializer Table around 0x180271700:")
for va in range(0x1802716E0, 0x180271A50, 16):
    p0 = en.read_u64(va)
    p1 = en.read_u64(va + 8)
    s0 = clean_str(en.read_str(p0))
    s1 = clean_str(en.read_str(p1))
    print(f"0x{va:X}: [0] 0x{p0:X} ('{s0}'), [1] 0x{p1:X} ('{s1}')")
