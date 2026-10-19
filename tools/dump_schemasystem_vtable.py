import struct

with open(r"versions/archive/2026-10-15_25966348\schemasystem.dll", "rb") as f:
    data = f.read()

image_base = 0x180000000
vtable_va = 0x180058598

# Section translation
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

off = va_to_off(vtable_va)
print(f"CSchemaSystem vtable at VA 0x{vtable_va:x} (file off 0x{off:x}):")

# Read up to 45 methods
for i in range(45):
    method_va = struct.unpack_from("<Q", data, off + i*8)[0]
    # Check if method_va is in .text
    if 0x180001000 <= method_va < 0x180050000:
        print(f"  [{i:2d}] offset +0x{i*8:02x} (vtable index {i}): 0x{method_va:016x}")
    else:
        print(f"  [{i:2d}] non-text pointer: 0x{method_va:016x} -> end of vtable")
        break
