import struct

with open(r"versions/archive/2026-10-15_25966348\schemasystem.dll", "rb") as f:
    data = f.read()

# Let's find sections
pe_sig = struct.unpack_from("<I", data, 0x3C)[0]
num_sec = struct.unpack_from("<H", data, pe_sig + 6)[0]
opt_size = struct.unpack_from("<H", data, pe_sig + 20)[0]
sec_off = pe_sig + 24 + opt_size

sections = []
image_base = 0x180000000
for i in range(num_sec):
    s = data[sec_off + i*40 : sec_off + (i+1)*40]
    name = s[:8].decode('latin-1').strip('\x00')
    vsize, rva, raw_size, raw_ptr = struct.unpack_from("<IIII", s, 8)
    sections.append((name, rva, vsize, raw_ptr, raw_size))

def off_to_va(o):
    for name, rva, vsize, raw_ptr, raw_size in sections:
        if raw_ptr <= o < raw_ptr + raw_size:
            return image_base + rva + (o - raw_ptr)
    return 0

def va_to_off(v):
    r = v - image_base
    for name, rva, vsize, raw_ptr, raw_size in sections:
        if rva <= r < rva + vsize:
            return raw_ptr + (r - rva)
    return None

# Find "CSchemaSystem" RTTI Complete Object Locator or class name
rtti_str_off = data.find(b".?AVCSchemaSystem@@")
print("RTTI str offset:", hex(rtti_str_off) if rtti_str_off != -1 else "None")

# In MSVC x64 RTTI:
# Complete Object Locator has pointer/RVA to TypeDescriptor (which has .?AVCSchemaSystem@@)
# And vtable has a pointer to Complete Object Locator at vtable - 8.
if rtti_str_off != -1:
    td_rva = off_to_va(rtti_str_off - 16) - image_base
    print(f"TypeDescriptor RVA: 0x{td_rva:x}")
    # Search for TD RVA in rdata (COL references TD by 32-bit RVA)
    for o in range(0, len(data) - 4, 4):
        val = struct.unpack_from("<I", data, o)[0]
        if val == td_rva:
            # could be COL! COL has signature 1 (or 0), offset 0, cdOffset 0, typeDescriptor rva, classDescriptor rva, objectBase rva
            col_va = off_to_va(o - 12)
            col_off = o - 12
            print(f"Candidate COL at off 0x{col_off:x}, VA 0x{col_va:x}")
            # Search for pointer to COL
            col_ptr_bytes = struct.pack("<Q", col_va)
            vidx = 0
            while True:
                vidx = data.find(col_ptr_bytes, vidx)
                if vidx == -1: break
                vtable_va = off_to_va(vidx + 8)
                print(f"  --> FOUND VTABLE at off 0x{vidx+8:x}, VA 0x{vtable_va:016x}!")
                vidx += 1
