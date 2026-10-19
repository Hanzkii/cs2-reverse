import explore_network as en
import struct

print("Scanning vtables around 0x18021F900..0x18021FA50:")
for va in range(0x18021F900, 0x18021FA50, 8):
    val = en.read_u64(va)
    # Check if val is a COL
    if val >= en.image_base:
        off = en.va_to_offset(val)
        if off and off + 24 <= len(en.buf):
            sig, offset, cdOff, rva_td, rva_chd, rva_self = struct.unpack_from('<IIIIII', en.buf, off)
            if sig == 1:
                td_va = en.image_base + rva_td
                td_off = en.va_to_offset(td_va)
                name = en.buf[td_off+16:td_off+80].split(b'\x00')[0].decode('latin1', errors='ignore')
                print(f"0x{va:X}: COL pointer -> class '{name}' (vtable starts at 0x{va+8:X})")
