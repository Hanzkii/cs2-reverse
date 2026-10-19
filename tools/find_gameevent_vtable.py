import struct

with open(r"versions/archive/2026-10-15_25966348\server.dll", "rb") as f:
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

td_off = data.find(b".?AVIGameEventManager2@@\x00")
if td_off != -1:
    td_va = off_to_va(td_off - 16)
    td_rva = td_va - image_base
    print(f"TypeDescriptor for IGameEventManager2 at VA 0x{td_va:x} (RVA 0x{td_rva:x})")
    
    td_bytes = struct.pack("<I", td_rva)
    idx = 0
    while True:
        idx = data.find(td_bytes, idx)
        if idx == -1: break
        col_off = idx - 12
        col_va = off_to_va(col_off)
        print(f"  Candidate COL at 0x{col_va:x}")
        col_ptr_bytes = struct.pack("<Q", col_va)
        vidx = 0
        while True:
            vidx = data.find(col_ptr_bytes, vidx)
            if vidx == -1: break
            vtable_va = off_to_va(vidx + 8)
            print(f"    --> FOUND IGameEventManager2 VTABLE at VA 0x{vtable_va:016x}!")
            vidx += 1
        idx += 1
