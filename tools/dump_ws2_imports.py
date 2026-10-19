import explore_network as en
import struct

e_lfanew = struct.unpack_from('<I', en.buf, 0x3c)[0]
import_rva = struct.unpack_from('<I', en.buf, e_lfanew + 24 + 112 + 8 * 1)[0]
import_off = en.va_to_offset(en.image_base + import_rva)

desc_idx = 0
while True:
    desc = en.buf[import_off + desc_idx*20 : import_off + (desc_idx+1)*20]
    if len(desc) < 20 or desc == b'\x00'*20:
        break
    ilt_rva, ts, fc, name_rva, iat_rva = struct.unpack_from('<IIIII', desc, 0)
    dll_name = en.read_str(en.image_base + name_rva)
    if 'ws2' in dll_name.lower():
        print(f"DLL: {dll_name}, ILT=0x{ilt_rva:X}, IAT=0x{iat_rva:X}")
        # When ILT is 0 or non-zero, let's walk whichever is non-zero
        table_rva = ilt_rva if ilt_rva != 0 else iat_rva
        for i in range(100):
            entry = en.read_u64(en.image_base + table_rva + i * 8)
            if entry == 0: break
            if entry & 0x8000000000000000 != 0:
                print(f"  Ordinal: {entry & 0xFFFF} at IAT 0x{en.image_base + iat_rva + i*8:X}")
            else:
                hint_name_va = en.image_base + entry
                fn_name = en.read_str(hint_name_va + 2)
                print(f"  Name: {fn_name} at IAT 0x{en.image_base + iat_rva + i*8:X}")
    desc_idx += 1
