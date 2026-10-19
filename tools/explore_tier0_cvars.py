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

def rva_to_off(rva):
    for name, rva_s, vsize, raw_ptr, raw_size in sections:
        if rva_s <= rva < rva_s + vsize:
            return raw_ptr + (rva - rva_s)
    return None

export_dir_rva = struct.unpack_from("<I", data, opt_hdr + 112)[0]
export_dir_off = rva_to_off(export_dir_rva)

num_funcs = struct.unpack_from("<I", data, export_dir_off + 20)[0]
num_names = struct.unpack_from("<I", data, export_dir_off + 24)[0]
funcs_rva = struct.unpack_from("<I", data, export_dir_off + 28)[0]
names_rva = struct.unpack_from("<I", data, export_dir_off + 32)[0]
ords_rva = struct.unpack_from("<I", data, export_dir_off + 36)[0]

names_off = rva_to_off(names_rva)
ords_off = rva_to_off(ords_rva)
funcs_off = rva_to_off(funcs_rva)

print(f"tier0.dll exports ({num_names} named exports):")
cvar_exports = []
for i in range(num_names):
    name_rva = struct.unpack_from("<I", data, names_off + i*4)[0]
    noff = rva_to_off(name_rva)
    end = data.find(b"\x00", noff)
    name = data[noff:end].decode('latin-1')
    if any(k in name.lower() for k in ["cvar", "convar", "command"]):
        cvar_exports.append(name)

print("CVar / ConVar related exports in tier0.dll:")
for e in sorted(cvar_exports):
    print("  ", e)
