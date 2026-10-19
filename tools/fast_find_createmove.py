import explore_client as ec
import struct

target_va = 0x181CA25F4
print(f"Target VA: 0x{target_va:X}")

text_sec = ec.sections[0]
roff = text_sec['roff']
rsize = text_sec['rsize']
base_va = ec.image_base + text_sec['rva']

# For LEA rdx, [rip + disp32] (48 8d 15 [disp32]) or LEA r8 / rcx
# disp = target_va - (instr_va + 7)
# Let's search in chunks
t_data = ec.buf[roff:roff+rsize]

# Search for any LEA reg, [rip + disp32] (48 8d ?? [disp32])
# We know disp32 must point to target_va
# Since instr_va = base_va + i:
# disp = target_va - (base_va + i + 7)
# So target_va - disp - 7 - base_va = i
# This means for any instruction ending at `instr_va + len`, disp = target_va - next_ip!
# Let's search all 4-byte matches of possible disp values!

found = []
for instr_len in [7, 8]:
    for i in range(len(t_data) - instr_len):
        instr_va = base_va + i
        disp = target_va - (instr_va + instr_len)
        if -0x80000000 <= disp <= 0x7FFFFFFF:
            disp_bytes = struct.pack('<i', disp)
            if t_data[i+instr_len-4:i+instr_len] == disp_bytes:
                # check if opcode is LEA (48 8d) or MOV (48 8b)
                op = t_data[i:i+3]
                print(f"Found match at VA 0x{instr_va:X}: {t_data[i:i+instr_len].hex()}")
                found.append(instr_va)

if not found:
    print("No direct rip-relative LEA found for exact string start.")
    # Check if substring or nearby address is referenced
