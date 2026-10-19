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

def offset_to_va(offset):
    for s in sections:
        if s['roff'] <= offset < s['roff'] + s['rsize']:
            return image_base + s['rva'] + (offset - s['roff'])
    return None

def read_str(va):
    off = va_to_offset(va)
    if off is None or off >= len(buf): return ''
    end = buf.find(b'\x00', off)
    if end == -1: return ''
    return buf[off:end].decode('latin1', errors='ignore')

def read_u64(va):
    off = va_to_offset(va)
    if off is None or off + 8 > len(buf): return 0
    return struct.unpack_from('<Q', buf, off)[0]

def read_u32(va):
    off = va_to_offset(va)
    if off is None or off + 4 > len(buf): return 0
    return struct.unpack_from('<I', buf, off)[0]
