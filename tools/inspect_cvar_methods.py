import struct

with open(r"versions/archive/2026-10-15_25966348\tier0.dll", "rb") as f:
    data = f.read()

image_base = 0x180000000
vtable_va = 0x180313208

pe_sig = struct.unpack_from("<I", data, 0x3C)[0]
num_sec = struct.unpack_from("<H", data, pe_sig + 6)[0]
opt_size = struct.unpack_from("<H", data, pe_sig + 20)[0]
sec_off = pe_sig + 24 + opt_size

sections = []
for i in range(num_sec):
    s = data[sec_off + i*40 : sec_off + (i+1)*40]
    name = s[:8].decode('latin-1').strip('\x00')
    vsize, rva, raw_size, raw_ptr = struct.unpack_from("<IIII", s, 8)
    sections.append((name, rva, vsize, raw_ptr, raw_size))

def va_to_off(v):
    r = v - image_base
    for name, rva, vsize, raw_ptr, raw_size in sections:
        if rva <= r < rva + vsize:
            return raw_ptr + (r - rva)
    return None

def off_to_va(o):
    for name, rva, vsize, raw_ptr, raw_size in sections:
        if raw_ptr <= o < raw_ptr + raw_size:
            return image_base + rva + (o - raw_ptr)
    return None

def read_str(off):
    end = data.find(b"\x00", off)
    if end == -1 or end - off > 120: return ""
    return data[off:end].decode('latin-1', errors='ignore')

off = va_to_off(vtable_va)
for i in range(46):
    method_va = struct.unpack_from("<Q", data, off + i*8)[0]
    m_off = va_to_off(method_va)
    # Search for strings referenced by LEA RDX/RCX/R8 [rip + disp] in the first 128 bytes
    strings_found = []
    for b in range(m_off, m_off + 128):
        disp = struct.unpack_from("<i", data, b)[0]
        tgt_va = off_to_va(b + 4) + disp
        tgt_off = va_to_off(tgt_va)
        if tgt_off and 0 <= tgt_off < len(data):
            # check if printable ASCII
            s = read_str(tgt_off)
            if len(s) > 3 and all(32 <= ord(c) < 127 for c in s):
                strings_found.append(s)
    str_summary = ", ".join(repr(s) for s in strings_found[:3])
    print(f"[{i:2d}] +0x{i*8:02x} (0x{method_va:x}): {str_summary}")
