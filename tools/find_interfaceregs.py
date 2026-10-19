import struct

with open(r"versions/archive/2026-10-15_25966348\schemasystem.dll", "rb") as f:
    data = f.read()

# Let's find all references to "SchemaSystem_001"
str_off = data.find(b"SchemaSystem_001\x00")
print("SchemaSystem_001 offset:", hex(str_off))

# Let's find rip-relative LEA or MOV instructions referencing str_off in .text
# In x64, LEA R8, [rip + disp] is: 4C 8D 05 / 4C 8D 15 / etc.
# Or 48 8D 15 (RDX)
text_start = 0x1000
text_size = 0x482ea
image_base = 0x180000000

# String VA
str_va = image_base + 0x4a000 + (str_off - 0x4a000) # wait, let's calculate exact VA from section
# Let's read section headers
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

str_va = off_to_va(str_off)
print(f"str_va = 0x{str_va:x}")

# Let's find any RIP-relative instruction targeting str_va in text
for o in range(0x1000, 0x49000):
    # check 4-byte displacement
    disp = struct.unpack_from("<i", data, o)[0]
    target_va = off_to_va(o + 4) + disp
    if target_va == str_va:
        print(f"Found reference to SchemaSystem_001 at text offset 0x{o-3:x} (inst ~ 0x{off_to_va(o-3):x})")
