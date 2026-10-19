import struct

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
            return raw_ptr + (r - rva)
    return None

def off_to_va(o):
    for name, rva, vsize, raw_ptr, raw_size in sections:
        if raw_ptr <= o < raw_ptr + raw_size:
            return image_base + rva + (o - raw_ptr)
    return None

str_off = data.find(b"GetViewRenderInstance\x00")
str_va = off_to_va(str_off)
print(f"'GetViewRenderInstance' string at file off 0x{str_off:x}, VA 0x{str_va:x}")

# Find references to this string
ptr_bytes = struct.pack("<Q", str_va)
pidx = 0
while True:
    pidx = data.find(ptr_bytes, pidx)
    if pidx == -1: break
    print(f"Pointer at 0x{off_to_va(pidx):x}")
    pidx += 1

# Search RIP relative in text
text_sec = [s for s in sections if s[0] == '.text'][0]
for o in range(text_sec[3], text_sec[3] + text_sec[4] - 4):
    disp = struct.unpack_from("<i", data, o)[0]
    inst_va = off_to_va(o + 4)
    if inst_va + disp == str_va:
        print(f"--> Referenced in code at VA 0x{off_to_va(o-3):x}!")
