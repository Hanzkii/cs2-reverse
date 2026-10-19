import explore_network as en
import struct

td_va = 0x1802921A0 # TypeDescriptor
off = en.va_to_offset(td_va)
print(f"TD at 0x{td_va:X}:", en.buf[off:off+40])
td_rva = td_va - en.image_base

for s in en.sections:
    s_data = en.buf[s['roff']:s['roff']+s['rsize']]
    matches = [i for i in range(0, len(s_data)-4, 4) if struct.unpack_from('<I', s_data, i)[0] == td_rva]
    for m in matches:
        va = en.image_base + s['rva'] + m
        print(f"Match in {s['name']} at 0x{va:X}")
