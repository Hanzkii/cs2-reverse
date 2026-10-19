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

# 1. Search for CViewSetup in Schema
schema_classes = [b"CViewSetup\0", b"CCSViewRender\0", b"CViewRender\0", b"C_CSPlayerPawn\0"]
for sc in schema_classes:
    str_idx = data.find(sc)
    if str_idx != -1:
        str_va = off_to_va(str_idx)
        print(f"Found schema class string '{sc[:-1].decode()}' at VA 0x{str_va:x}")
        # Search for SchemaClassInfoData_t
        ptr_bytes = struct.pack("<Q", str_va)
        pidx = 0
        while True:
            pidx = data.find(ptr_bytes, pidx)
            if pidx == -1: break
            s_off = pidx - 8
            size = struct.unpack_from("<I", data, s_off + 0x20)[0]
            num_f = struct.unpack_from("<H", data, s_off + 0x24)[0]
            fields_ptr = struct.unpack_from("<Q", data, s_off + 0x30)[0]
            base_ptr = struct.unpack_from("<Q", data, s_off + 0x38)[0]
            print(f"  SchemaClassInfoData_t for {sc[:-1].decode()} at VA 0x{off_to_va(s_off):x}: Size=0x{size:x} ({size}), Fields={num_f}")
            if fields_ptr != 0 and 0 < num_f < 300:
                foff = va_to_off(fields_ptr)
                if foff and foff + num_f * 0x20 <= len(data):
                    for f in range(num_f):
                        f_entry = foff + f * 0x20
                        fn_ptr = struct.unpack_from("<Q", data, f_entry)[0]
                        fn_name = read_str(fn_ptr)
                        f_offset = struct.unpack_from("<I", data, f_entry + 0x10)[0]
                        print(f"    +0x{f_offset:04x}: {fn_name}")
            pidx += 1

# 2. Search for aimPunch in C_CSPlayerPawn
pawn_str = b"C_CSPlayerPawn\0"
str_idx = data.find(pawn_str)
if str_idx != -1:
    str_va = off_to_va(str_idx)
    ptr_bytes = struct.pack("<Q", str_va)
    pidx = 0
    while True:
        pidx = data.find(ptr_bytes, pidx)
        if pidx == -1: break
        s_off = pidx - 8
        num_f = struct.unpack_from("<H", data, s_off + 0x24)[0]
        fields_ptr = struct.unpack_from("<Q", data, s_off + 0x30)[0]
        if fields_ptr != 0 and num_f > 10:
            foff = va_to_off(fields_ptr)
            if foff and foff + num_f * 0x20 <= len(data):
                print(f"\nScanning {num_f} fields of C_CSPlayerPawn for aim/punch/view/fov:")
                for f in range(num_f):
                    f_entry = foff + f * 0x20
                    fn_ptr = struct.unpack_from("<Q", data, f_entry)[0]
                    fn_name = read_str(fn_ptr)
                    f_offset = struct.unpack_from("<I", data, f_entry + 0x10)[0]
                    if any(k in fn_name.lower() for k in ["punch", "aim", "fov", "view", "eye"]):
                        print(f"    +0x{f_offset:04x}: {fn_name}")
        pidx += 1
