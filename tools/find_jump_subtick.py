import explore_client as ec
import struct

target_off = ec.buf.find(b'sv_jump_spam_penalty_time\x00')
target_va = ec.offset_to_va(target_off)
print(f"sv_jump_spam_penalty_time string at VA: 0x{target_va:X}")

# Search for pointer to target_va
b_va = struct.pack('<Q', target_va)
ptr_refs = [i for i in range(len(ec.buf)-7) if ec.buf[i:i+8] == b_va]
print(f"Ptr refs in client.dll: {[hex(ec.offset_to_va(p)) for p in ptr_refs]}")

# If in ConVar table, check xrefs to that ConVar struct
for pr in ptr_refs:
    cvar_struct_va = ec.offset_to_va(pr)
    b_cvar = struct.pack('<Q', cvar_struct_va)
    # search for ptr to cvar_struct
    cvar_refs = [i for i in range(len(ec.buf)-7) if ec.buf[i:i+8] == b_cvar]
    print(f"  Refs to CVar struct: {[hex(ec.offset_to_va(cr)) for cr in cvar_refs]}")
