import struct

with open(r"versions/archive/2026-10-15_25966348\tier0.dll", "rb") as f:
    data = f.read()

image_base = 0x180000000
pe_sig = struct.unpack_from("<I", data, 0x3C)[0]
opt_hdr = pe_sig + 24
opt_size = struct.unpack_from("<H", data, pe_sig + 20)[0]
sec_off = opt_hdr + opt_size
num_sec = struct.unpack_from("<H", data, pe_sig + 6)[0]

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

# Locate .?AVCCvar@@
td_str_off = data.find(b".?AVCCvar@@\x00")
td_va = off_to_va(td_str_off - 16)
td_rva = td_va - image_base
print(f"TypeDescriptor for CCvar at VA 0x{td_va:x} (RVA 0x{td_rva:x})")

# Find Complete Object Locator
for o in range(0, len(data) - 4, 4):
    val = struct.unpack_from("<I", data, o)[0]
    if val == td_rva:
        col_off = o - 12
        col_va = off_to_va(col_off)
        print(f"Candidate COL at off 0x{col_off:x}, VA 0x{col_va:x}")
        
        # Search for vtable referencing col_va
        col_ptr_bytes = struct.pack("<Q", col_va)
        vidx = 0
        while True:
            vidx = data.find(col_ptr_bytes, vidx)
            if vidx == -1: break
            vtable_va = off_to_va(vidx + 8)
            print(f"  --> FOUND CCvar VTABLE at off 0x{vidx+8:x}, VA 0x{vtable_va:016x}!")
            vidx += 1
