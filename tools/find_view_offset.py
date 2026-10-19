import struct
import re

with open(r"versions/archive/2026-10-15_25966348\client.dll", "rb") as f:
    data = f.read()

image_base = 0x180000000

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
            off = raw_ptr + (r - rva)
            if off < len(data):
                return off
    return None

def off_to_va(o):
    for name, rva, vsize, raw_ptr, raw_size in sections:
        if raw_ptr <= o < raw_ptr + raw_size:
            return image_base + rva + (o - raw_ptr)
    return None

def read_str(va):
    off = va_to_off(va)
    if off is None or off >= len(data): return ""
    end = data.find(b"\x00", off)
    if end == -1 or end - off > 256: return ""
    return data[off:end].decode('latin-1', errors='ignore')

for target in [b"m_vecViewOffset\0", b"m_vEyeOffset\0", b"m_vecAbsOrigin\0", b"m_angAbsRotation\0"]:
    idx = data.find(target)
    if idx != -1:
        str_va = off_to_va(idx)
        ptr_bytes = struct.pack("<Q", str_va)
        pidx = 0
        while True:
            pidx = data.find(ptr_bytes, pidx)
            if pidx == -1: break
            foff = pidx
            f_offset = struct.unpack_from("<I", data, foff + 0x10)[0]
            print(f"Found field '{target[:-1].decode()}' at offset +0x{f_offset:04x}")
            pidx += 1
