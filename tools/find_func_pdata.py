import explore_network as en
import struct

def find_runtime_function(target_va):
    target_rva = target_va - en.image_base
    # Get exception directory from optional header
    e_lfanew = struct.unpack_from('<I', en.buf, 0x3c)[0]
    pdata_rva = struct.unpack_from('<I', en.buf, e_lfanew + 24 + 112 + 8 * 3)[0] # Exception dir RVA
    pdata_size = struct.unpack_from('<I', en.buf, e_lfanew + 24 + 112 + 8 * 3 + 4)[0]
    pdata_off = en.va_to_offset(en.image_base + pdata_rva)
    
    num_entries = pdata_size // 12
    print(f"Exception directory has {num_entries} runtime functions.")
    
    # Binary search or scan
    for i in range(num_entries):
        start_rva, end_rva, unwind_rva = struct.unpack_from('<III', en.buf, pdata_off + i * 12)
        if start_rva <= target_rva < end_rva:
            return en.image_base + start_rva, en.image_base + end_rva
    return None, None

va = 0x1800c16f5
start_va, end_va = find_runtime_function(va)
if start_va:
    print(f"Target 0x{va:X} is inside function: 0x{start_va:X} - 0x{end_va:X}")
else:
    print(f"Target 0x{va:X} not found in .pdata")
