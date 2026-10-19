import struct

# Let's inspect C_CSPlayerPawn and C_BasePlayerPawn schema fields to find m_hController and handle fields
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

def inspect_class(class_name):
    tc = class_name.encode() + b"\x00"
    str_idx = data.find(tc)
    if str_idx == -1:
        print(f"Class name string '{class_name}' not found.")
        return
    str_va = off_to_va(str_idx)
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
        
        base_name = ""
        if base_ptr != 0:
            boff = va_to_off(base_ptr)
            if boff and boff + 16 <= len(data):
                binfo = struct.unpack_from("<Q", data, boff + 8)[0]
                if binfo:
                    binfo_off = va_to_off(binfo)
                    if binfo_off and binfo_off + 16 <= len(data):
                        bname_ptr = struct.unpack_from("<Q", data, binfo_off + 8)[0]
                        base_name = read_str(bname_ptr)

        if fields_ptr != 0 and 0 < num_f < 500:
            foff = va_to_off(fields_ptr)
            if foff and foff + num_f * 0x20 <= len(data):
                print(f"\n=======================================================")
                print(f"Class '{class_name}': Size={size} (0x{size:x}), Base='{base_name}', Fields={num_f}")
                for f in range(num_f):
                    f_entry = foff + f * 0x20
                    fn_ptr = struct.unpack_from("<Q", data, f_entry)[0]
                    fn_name = read_str(fn_ptr)
                    f_offset = struct.unpack_from("<I", data, f_entry + 0x10)[0]
                    # Print interesting fields
                    if any(k in fn_name.lower() for k in ["controller", "pawn", "handle", "health", "armor", "alive", "observer"]):
                        print(f"  +0x{f_offset:04x}: {fn_name}")
        pidx += 1

inspect_class("C_BasePlayerPawn")
inspect_class("CBasePlayerController")
inspect_class("CCSPlayerController")
inspect_class("C_CSPlayerPawnBase")
inspect_class("C_CSPlayerPawn")
