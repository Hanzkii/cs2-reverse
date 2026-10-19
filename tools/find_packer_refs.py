import explore_network as en
import struct

target_va = 0x1802144FE
b_va = struct.pack('<Q', target_va)
print('Ptr refs:', [hex(en.offset_to_va(i)) for i in range(len(en.buf)-7) if en.buf[i:i+8] == b_va])

for s in en.sections:
    s_data = en.buf[s['roff']:s['roff']+s['rsize']]
    for i in range(len(s_data)-7):
        for disp_off in [1, 2, 3, 4]:
            if i + disp_off + 4 <= len(s_data):
                disp = struct.unpack_from('<i', s_data, i + disp_off)[0]
                if en.image_base + s['rva'] + i + disp_off + 4 + disp == target_va:
                    print(f"Displacement ref in {s['name']} at 0x{en.image_base + s['rva'] + i:X}")
