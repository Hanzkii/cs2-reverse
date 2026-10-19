import struct

with open(r"versions/archive/2026-10-15_25966348\schemasystem.dll", "rb") as f:
    data = f.read()

# Let's find PE sections to translate file offsets to RVA / VA
pe_sig_offset = struct.unpack_from("<I", data, 0x3C)[0]
num_sections = struct.unpack_from("<H", data, pe_sig_offset + 6)[0]
opt_header_size = struct.unpack_from("<H", data, pe_sig_offset + 20)[0]
image_base = struct.unpack_from("<Q", data, pe_sig_offset + 24 + 24)[0]

sections = []
section_table_offset = pe_sig_offset + 24 + opt_header_size
for i in range(num_sections):
    sec = data[section_table_offset + i*40 : section_table_offset + (i+1)*40]
    name = sec[:8].decode('latin-1').strip('\x00')
    vsize, rva, raw_size, raw_ptr = struct.unpack_from("<IIII", sec, 8)
    sections.append({
        'name': name,
        'va': image_base + rva,
        'rva': rva,
        'vsize': vsize,
        'raw_ptr': raw_ptr,
        'raw_size': raw_size
    })
    print(f"Section {name:8s}: RVA 0x{rva:08x} -> VA 0x{image_base + rva:016x}, size 0x{vsize:x}")

def rva_to_offset(rva):
    for s in sections:
        if s['rva'] <= rva < s['rva'] + s['vsize']:
            return s['raw_ptr'] + (rva - s['rva'])
    return None

def offset_to_va(off):
    for s in sections:
        if s['raw_ptr'] <= off < s['raw_ptr'] + s['raw_size']:
            return image_base + s['rva'] + (off - s['raw_ptr'])
    return None

def va_to_offset(va):
    return rva_to_offset(va - image_base)

# Locate "SchemaSystem_001"
schema_str_off = data.find(b"SchemaSystem_001\x00")
print(f"'SchemaSystem_001' at offset 0x{schema_str_off:x}, VA: 0x{offset_to_va(schema_str_off):016x}")
schema_str_va = offset_to_va(schema_str_off)

# Find references to this string pointer in the binary
schema_str_ptr_bytes = struct.pack("<Q", schema_str_va)
refs = []
idx = 0
while True:
    idx = data.find(schema_str_ptr_bytes, idx)
    if idx == -1: break
    print(f"Pointer to 'SchemaSystem_001' found at offset 0x{idx:x}, VA 0x{offset_to_va(idx):016x}")
    refs.append(idx)
    idx += 1

# Inspect the InterfaceReg structure:
# struct InterfaceReg {
#   InstantiateInterfaceFn m_CreateFn; // +0
#   const char *m_pName;              // +8
#   InterfaceReg *m_pNext;            // +16
# };
for ref in refs:
    # If ref is m_pName (+8), then m_CreateFn is at ref - 8
    reg_off = ref - 8
    fn_ptr = struct.unpack_from("<Q", data, reg_off)[0]
    next_ptr = struct.unpack_from("<Q", data, reg_off + 16)[0]
    print(f"InterfaceReg at 0x{offset_to_va(reg_off):016x}: CreateFn=0x{fn_ptr:016x}, Next=0x{next_ptr:016x}")
