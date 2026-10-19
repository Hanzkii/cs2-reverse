import explore_network as en
import struct

e_lfanew = struct.unpack_from('<I', en.buf, 0x3c)[0]
import_rva = struct.unpack_from('<I', en.buf, e_lfanew + 24 + 112 + 8 * 1)[0]
import_size = struct.unpack_from('<I', en.buf, e_lfanew + 24 + 112 + 8 * 1 + 4)[0]
import_off = en.va_to_offset(en.image_base + import_rva)

print(f"Import Directory at RVA: 0x{import_rva:X}, file offset: 0x{import_off:X}")

# Loop through import descriptors
desc_idx = 0
iat_recvfrom = []
while True:
    desc = en.buf[import_off + desc_idx*20 : import_off + (desc_idx+1)*20]
    if len(desc) < 20 or desc == b'\x00'*20:
        break
    ilt_rva, ts, fc, name_rva, iat_rva = struct.unpack_from('<IIIII', desc, 0)
    dll_name = en.read_str(en.image_base + name_rva)
    # Check if WS2_32.dll
    if 'ws2' in dll_name.lower():
        print(f"Found DLL: {dll_name}, IAT at RVA: 0x{iat_rva:X}")
        # Walk thunks
        thunk_idx = 0
        while True:
            thunk_val = en.read_u64(en.image_base + iat_rva + thunk_idx * 8)
            if thunk_val == 0:
                break
            # Original first thunk / ILT gives the name
            ilt_val = en.read_u64(en.image_base + ilt_rva + thunk_idx * 8)
            if ilt_val & 0x8000000000000000 == 0: # import by name
                hint_name_va = en.image_base + ilt_val
                hint_name_off = en.va_to_offset(hint_name_va)
                if hint_name_off:
                    fn_name = en.buf[hint_name_off + 2:].split(b'\x00')[0].decode('latin1')
                    iat_entry_va = en.image_base + iat_rva + thunk_idx * 8
                    if 'recv' in fn_name.lower() or 'send' in fn_name.lower():
                        print(f"   IAT 0x{iat_entry_va:X}: {fn_name}")
                        iat_recvfrom.append((iat_entry_va, fn_name))
            thunk_idx += 1
    desc_idx += 1
