import struct

with open(r'versions/archive/2026-10-15_25966348\networksystem.dll', 'rb') as f:
    buf = f.read()

e_lfanew = struct.unpack_from('<I', buf, 0x3c)[0]
num_sections = struct.unpack_from('<H', buf, e_lfanew + 6)[0]
opt_hdr_size = struct.unpack_from('<H', buf, e_lfanew + 20)[0]
image_base = struct.unpack_from('<Q', buf, e_lfanew + 24 + 24)[0]

sections = []
sec_start = e_lfanew + 24 + opt_hdr_size
for i in range(num_sections):
    sec = buf[sec_start + i*40 : sec_start + (i+1)*40]
    name = sec[:8].rstrip(b'\x00').decode('latin1')
    vsize, rva, rsize, roff = struct.unpack_from('<IIII', sec, 8)
    sections.append({'name': name, 'vsize': vsize, 'rva': rva, 'rsize': rsize, 'roff': roff})

def va_to_offset(va):
    rva = va - image_base
    for s in sections:
        if s['rva'] <= rva < s['rva'] + s['vsize']:
            return s['roff'] + (rva - s['rva'])
    return None

def read_str(va):
    off = va_to_offset(va)
    if off is None: return ''
    end = buf.find(b'\x00', off)
    return buf[off:end].decode('latin1', errors='ignore')

# Target is InterfaceReg constructor at 0x1801c27c0
target_va = 0x1801c27c0
print(f"Searching calls to InterfaceReg constructor at 0x{target_va:X}:")

for s in sections:
    s_data = buf[s['roff']:s['roff']+s['rsize']]
    for i in range(len(s_data) - 5):
        if s_data[i] == 0xE8: # CALL rel32
            disp = struct.unpack_from('<i', s_data, i + 1)[0]
            call_target = image_base + s['rva'] + i + 5 + disp
            if call_target == target_va:
                instr_va = image_base + s['rva'] + i
                print(f"Call at 0x{instr_va:X}")
                # Print preceding 32 bytes to see arguments
                window = s_data[max(0, i-32):i]
                print(f"   preceding hex: {window.hex()}")
