import explore_network as en
import re
import struct

# Let's search for references to string "CNETMsg_Tick", "CSVCMsg_PacketEntities", "CCLCMsg_Move"
test_names = [b'CNETMsg_NOP', b'CNETMsg_Tick', b'CNETMsg_SetConVar', b'CNETMsg_SignonState', b'CSVCMsg_PacketEntities', b'CCLCMsg_Move']

for name in test_names:
    off = 0
    while True:
        off = en.buf.find(name, off)
        if off == -1: break
        va = en.offset_to_va(off)
        print(f"String '{name.decode()}' at VA: 0x{va:X}")
        
        # Search for rip-relative references or ptr references
        b_va = struct.pack('<Q', va)
        ptr_refs = [i for i in range(len(en.buf)-7) if en.buf[i:i+8] == b_va]
        for pr in ptr_refs:
            print(f"   ptr ref at VA 0x{en.offset_to_va(pr):X}")
            
        # Search rip-relative LEA in .text
        text_sec = en.sections[0]
        t_data = en.buf[text_sec['roff']:text_sec['roff']+text_sec['rsize']]
        for i in range(len(t_data)-7):
            for disp_off in [2, 3]:
                disp = struct.unpack_from('<i', t_data, i + disp_off)[0]
                target = en.image_base + text_sec['rva'] + i + disp_off + 4 + disp
                if target == va:
                    instr_va = en.image_base + text_sec['rva'] + i
                    print(f"   LEA ref in .text at VA 0x{instr_va:X}")
        off += len(name)
