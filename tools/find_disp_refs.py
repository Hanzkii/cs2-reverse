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

def find_disp_refs(target_va):
    results = []
    for s in sections:
        s_data = buf[s['roff']:s['roff']+s['rsize']]
        for i in range(len(s_data) - 7):
            for disp_off in [2, 3, 4]:
                if i + disp_off + 4 <= len(s_data):
                    disp = struct.unpack_from('<i', s_data, i + disp_off)[0]
                    instr_end = image_base + s['rva'] + i + disp_off + 4
                    if instr_end + disp == target_va:
                        results.append((image_base + s['rva'] + i, s_data[i:i+disp_off+4]))
    return results

print("References to 0x1802A1658 (NetworkSystemVersion001 global?):")
for va, hex_bytes in find_disp_refs(0x1802A1658):
    print(f"  0x{va:X}: {hex_bytes.hex()}")

print("References to 0x180212CF0 (NetworkSystemVersion001 table entry):")
for va, hex_bytes in find_disp_refs(0x180212CF0):
    print(f"  0x{va:X}: {hex_bytes.hex()}")

print("References to 0x180212CD0 (Table start):")
for va, hex_bytes in find_disp_refs(0x180212CD0):
    print(f"  0x{va:X}: {hex_bytes.hex()}")
