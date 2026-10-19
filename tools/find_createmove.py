import explore_client as ec
import struct

str_va = 0x181CA25F4
print(f"Searching for references to CreateMove string (0x{str_va:X})...")

text_sec = ec.sections[0]
t_data = ec.buf[text_sec['roff']:text_sec['roff']+text_sec['rsize']]

for i in range(len(t_data)-7):
    for disp_off in [2, 3]:
        disp = struct.unpack_from('<i', t_data, i + disp_off)[0]
        target = ec.image_base + text_sec['rva'] + i + disp_off + 4 + disp
        if target == str_va:
            instr_va = ec.image_base + text_sec['rva'] + i
            print(f"  Ref in .text at VA 0x{instr_va:X}")
