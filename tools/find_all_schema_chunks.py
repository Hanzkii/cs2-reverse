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

# The registration functions call:
# (*param_2 + 0x110)(..., &Table, ...)
# In x64 assembly:
# 4C 8D 8C 24 ... or LEA R9/R8, [rip + disp]
# In FUN_181231840, it referenced &UNK_181d33c20 (0x181d33c20).
# Let's find all tables passed to ResolveDeclaredClasses (+0x110).
# Alternatively, any table of SchemaClassInfoData_t pointers in .rdata:
# A table in .rdata has a sequence of pointers to addresses in .data where +8 points to an ASCII string (class name) and +0x20 is an int <= 1000000!

# Let's search for "CCSPlayerController\0" string
target_classes = [b"CCSPlayerController\0", b"C_CSPlayerPawn\0", b"C_BaseEntity\0", b"C_CSWeaponBase\0"]
for tc in target_classes:
    str_idx = data.find(tc)
    if str_idx != -1:
        str_va = off_to_va(str_idx)
        print(f"\nFound string '{tc[:-1].decode()}' at VA 0x{str_va:x} (file off 0x{str_idx:x})")
        # Search for SchemaClassInfoData_t where +8 points to str_va
        ptr_bytes = struct.pack("<Q", str_va)
        pidx = 0
        while True:
            pidx = data.find(ptr_bytes, pidx)
            if pidx == -1: break
            # if this is at +8, struct start is pidx - 8
            s_va = off_to_va(pidx - 8)
            s_off = pidx - 8
            size = struct.unpack_from("<I", data, s_off + 0x20)[0]
            num_f = struct.unpack_from("<H", data, s_off + 0x24)[0]
            fields_ptr = struct.unpack_from("<Q", data, s_off + 0x30)[0]
            print(f"  --> Potential SchemaClassInfoData_t at VA 0x{s_va:x}: Size={size} (0x{size:x}), NumFields={num_f}, FieldsPtr=0x{fields_ptr:x}")
            
            # Print all fields!
            if fields_ptr != 0 and 0 < num_f < 500:
                foff = va_to_off(fields_ptr)
                if foff and foff + num_f * 0x20 <= len(data):
                    print(f"      Fields ({num_f}):")
                    for f in range(num_f):
                        f_entry = foff + f * 0x20
                        fn_ptr = struct.unpack_from("<Q", data, f_entry)[0]
                        fn_name = read_str(fn_ptr)
                        f_offset = struct.unpack_from("<I", data, f_entry + 0x10)[0]
                        print(f"        +0x{f_offset:04x}: {fn_name}")
            pidx += 1
