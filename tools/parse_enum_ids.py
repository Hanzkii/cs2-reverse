import explore_network as en

def parse_enum_block(start_idx):
    # EnumDescriptorProto:
    # 0x0A <len> <name>
    # repeated EnumValueDescriptorProto:
    #   0x12 <len> (0x0A <len> <name>, 0x10 <number>)
    idx = start_idx
    results = []
    # scan for 0x12 followed by nested fields
    chunk = en.buf[start_idx:start_idx+2500]
    pos = 0
    while pos < len(chunk):
        # find \x12
        p12 = chunk.find(b'\x12', pos)
        if p12 == -1 or p12 + 4 >= len(chunk): break
        vlen = chunk[p12+1]
        val_block = chunk[p12+2:p12+2+vlen]
        # within val_block: \x0A <len> <name> \x10 <id>
        if val_block.startswith(b'\n') or b'\n' in val_block:
            name_start = val_block.find(b'\n') + 2
            p10 = val_block.find(b'\x10')
            if p10 != -1 and name_start < p10:
                name = val_block[name_start:p10].decode('latin1', errors='ignore')
                val_id = val_block[p10+1]
                results.append((name, val_id))
        pos = p12 + 2 + vlen
    return results

for name in [b'NET_Messages', b'CLC_Messages', b'SVC_Messages']:
    idx = en.buf.find(name)
    print(f"\n==================== {name.decode()} ====================")
    res = parse_enum_block(idx)
    for n, i in res:
        print(f"  {n:35s} = {i} (0x{i:02X})")
