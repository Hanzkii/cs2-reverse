import struct

with open(r"versions/archive/2026-10-15_25966348\engine2.dll", "rb") as f:
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

methods = [
    (11, 0x180217200),
    (12, 0x180217250),
    (13, 0x180217370),
    (14, 0x180217460),
    (15, 0x180217bd0),
    (16, 0x1802178b0),
    (17, 0x1802174e0),
    (18, 0x180218260)
]

for idx, mva in methods:
    moff = va_to_off(mva)
    strs = []
    for b in range(moff, moff + 150):
        disp = struct.unpack_from("<i", data, b)[0]
        tgt_va = off_to_va(b + 4) + disp
        tgt_off = va_to_off(tgt_va)
        if tgt_off and 0 <= tgt_off < len(data):
            s = read_str(tgt_off)
            if len(s) > 3 and all(32 <= ord(c) < 127 for c in s):
                strs.append(s)
    print(f"[{idx}] 0x{mva:x}: {', '.join(repr(s) for s in strs[:3])}")
