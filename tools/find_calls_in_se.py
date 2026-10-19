import explore_network as en
import struct

# Functions:
# ReadVarInt32: 0x1801bf770
# ReadUBitLong: let's find callers of 0x1801bf770 in 0x180108000..0x18010E000
target_va = 0x1801bf770
text_sec = en.sections[0]
t_data = en.buf[text_sec['roff']:text_sec['roff']+text_sec['rsize']]

print(f"Calls to ReadVarInt32 in CSerializedEntities range (0x180108000..0x18010E000):")
for i in range(len(t_data) - 5):
    if t_data[i] == 0xE8:
        disp = struct.unpack_from('<i', t_data, i + 1)[0]
        call_target = en.image_base + text_sec['rva'] + i + 5 + disp
        if call_target == target_va:
            instr_va = en.image_base + text_sec['rva'] + i
            if 0x180108000 <= instr_va <= 0x18010E000:
                print(f"  Call at 0x{instr_va:X}")
