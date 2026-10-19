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

def read_str(va):
    off = va_to_off(va)
    if off is None or off >= len(data): return ""
    end = data.find(b"\x00", off)
    if end == -1 or end - off > 256: return ""
    return data[off:end].decode('latin-1', errors='ignore')

table_va = 0x181d33c20
table_off = va_to_off(table_va)

print(f"Reading SchemaClassInfoData_t table from 0x{table_va:x} (file off 0x{table_off:x})...")

classes = {}
idx = 0
while True:
    if table_off + idx * 8 + 8 > len(data):
        break
    class_ptr = struct.unpack_from("<Q", data, table_off + idx * 8)[0]
    if class_ptr == 0:
        break
    
    idx += 1
    class_off = va_to_off(class_ptr)
    if class_off is None or class_off + 0x40 > len(data):
        continue
    
    name_ptr = struct.unpack_from("<Q", data, class_off + 8)[0]
    class_name = read_str(name_ptr)
    if not class_name:
        continue
    
    size = struct.unpack_from("<I", data, class_off + 0x20)[0]
    num_fields = struct.unpack_from("<H", data, class_off + 0x24)[0]
    fields_ptr = struct.unpack_from("<Q", data, class_off + 0x30)[0]
    base_ptr = struct.unpack_from("<Q", data, class_off + 0x38)[0]
    
    base_name = ""
    if base_ptr != 0:
        base_off = va_to_off(base_ptr)
        if base_off and base_off + 16 <= len(data):
            base_info_ptr = struct.unpack_from("<Q", data, base_off + 8)[0]
            if base_info_ptr:
                base_info_off = va_to_off(base_info_ptr)
                if base_info_off and base_info_off + 16 <= len(data):
                    base_name_ptr = struct.unpack_from("<Q", data, base_info_off + 8)[0]
                    base_name = read_str(base_name_ptr)
    
    fields = []
    if fields_ptr != 0 and num_fields > 0:
        fields_off = va_to_off(fields_ptr)
        if fields_off and fields_off + num_fields * 0x20 <= len(data):
            for f in range(num_fields):
                foff = fields_off + f * 0x20
                fname_ptr = struct.unpack_from("<Q", data, foff)[0]
                fname = read_str(fname_ptr)
                offset = struct.unpack_from("<I", data, foff + 0x10)[0]
                fields.append((fname, offset))
    
    classes[class_name] = {
        'va': class_ptr,
        'size': size,
        'base': base_name,
        'fields': fields
    }

print(f"Total classes found in this registration chunk: {len(classes)}")

# Print sample classes
for name in sorted(list(classes.keys()))[:25]:
    c = classes[name]
    print(f"  Class '{name}': size={c['size']}, base='{c['base']}', num_fields={len(c['fields'])}")
