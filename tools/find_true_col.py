import explore_network as en
import struct

td_va = 0x1802921A0
td_rva = td_va - en.image_base

rdata_sec = [s for s in en.sections if s['name'] == '.rdata'][0]
data = en.buf[rdata_sec['roff']:rdata_sec['roff']+rdata_sec['rsize']]

print(f"Scanning .rdata for COL of 0x{td_va:X} (RVA 0x{td_rva:X})...")
for i in range(0, len(data) - 24, 4):
    sig, off, cdOff, rva_td, rva_chd, rva_self = struct.unpack_from('<IIIIII', data, i)
    if sig == 1 and rva_td == td_rva:
        col_va = en.image_base + rdata_sec['rva'] + i
        print(f"Found true COL at VA 0x{col_va:X} (rva_self=0x{rva_self:X})")
        # Now search for pointer to this COL
        col_bytes = struct.pack('<Q', col_va)
        for s in en.sections:
            idx = en.buf[s['roff']:s['roff']+s['rsize']].find(col_bytes)
            if idx != -1:
                vtab_va = en.image_base + s['rva'] + idx + 8
                print(f"   VTable at VA 0x{vtab_va:X}")
                for m in range(12):
                    fn = en.read_u64(vtab_va + m*8)
                    print(f"     slot {m}: 0x{fn:X}")
