import explore_network as en
import struct

# Let's search for functions in 0x180080000..0x180088000
# Let's check all functions in .pdata in this range
e_lfanew = struct.unpack_from('<I', en.buf, 0x3c)[0]
pdata_rva = struct.unpack_from('<I', en.buf, e_lfanew + 24 + 112 + 8 * 3)[0]
pdata_size = struct.unpack_from('<I', en.buf, e_lfanew + 24 + 112 + 8 * 3 + 4)[0]
pdata_off = en.va_to_offset(en.image_base + pdata_rva)

funcs = []
for i in range(pdata_size // 12):
    start_rva, end_rva, unwind_rva = struct.unpack_from('<III', en.buf, pdata_off + i * 12)
    start_va = en.image_base + start_rva
    if 0x180082000 <= start_va <= 0x180087000:
        funcs.append((start_va, en.image_base + end_rva))

print(f"Found {len(funcs)} functions in 0x180082000..0x180087000:")
for s, e in funcs:
    print(f"  0x{s:X} - 0x{e:X} (size {e-s} bytes)")
