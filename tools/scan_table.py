import explore_network as en
import string

def is_printable(s):
    return len(s) > 3 and all(c in string.printable for c in s)

for va in range(0x180212C00, 0x180212E50, 16):
    name_va = en.read_u64(va)
    target_va = en.read_u64(va + 8)
    s_name = en.read_str(name_va)
    if is_printable(s_name):
        print(f"0x{va:X}: name='{s_name}' (0x{name_va:X}), target=0x{target_va:X}")
