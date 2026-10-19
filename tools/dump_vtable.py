import explore_network as en

print("Dumping pointers around 0x18023A798:")
for va in range(0x18023A700, 0x18023A820, 8):
    p = en.read_u64(va)
    s = en.read_str(p)
    s_clean = repr(s[:30]) if s else ""
    print(f"0x{va:X}: 0x{p:X} {s_clean}")
