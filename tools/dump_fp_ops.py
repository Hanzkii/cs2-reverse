import explore_network as en
import struct

# Let's find stride of the array starting at 0x180272420
# PlusOne is at 0x180272420 (ptr to 0x1802140A0)
# PlusTwo is at 0x180272430 (ptr to 0x1802140A8)
# That's 0x180272430 - 0x180272420 = 16 bytes! (0x10 bytes per entry!)

print("FieldPath Operations Array (stride = 16 bytes):")
for i in range(40):
    entry_va = 0x180272418 + i * 16 # or let's find exact start
    # Let's read (name_ptr, int_id, int_weight or fn_ptr)
    name_ptr = en.read_u64(0x180272420 + i * 16)
    name = en.read_str(name_ptr)
    # The 8 bytes before or after name_ptr:
    val0 = en.read_u32(0x180272420 + i * 16 - 8)
    val1 = en.read_u32(0x180272420 + i * 16 - 4)
    val2 = en.read_u32(0x180272420 + i * 16 + 8)
    val3 = en.read_u32(0x180272420 + i * 16 + 12)
    print(f"[{i:2d}] name='{name:38s}' (0x{name_ptr:X}) | post: {val2:2d}, {val3:2d} | pre: {val0:2d}, {val1:2d}")
