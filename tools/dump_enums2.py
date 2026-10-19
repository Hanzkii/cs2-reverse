import explore_network as en
import re

for enum_name in [b'NET_Messages', b'CLC_Messages', b'SVC_Messages']:
    idx = en.buf.find(enum_name)
    if idx != -1:
        print(f"\n=== {enum_name.decode()} at 0x{en.offset_to_va(idx):X} ===")
        # Dump next 1200 bytes
        chunk = en.buf[idx:idx+1200]
        # Find entries like: \x12 <len> \n <len> <name> \x10 <id>
        # e.g.: \x12\x12\n\x0eclc_ClientInfo\x10\x14
        entries = re.findall(rb'\n\x0e?([a-zA-Z0-9_]+)\x10([^\x12\n*]+)', chunk)
        for name, id_bytes in entries:
            val_id = id_bytes[0]
            print(f"  {name.decode('latin1'):35s} = {val_id} (0x{val_id:02X})")
