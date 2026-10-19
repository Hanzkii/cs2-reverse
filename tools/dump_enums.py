import explore_network as en
import re

print("=== Extracting Protobuf Message Enum IDs ===")
with open(r'versions/archive/2026-10-15_25966348\networksystem.dll', 'rb') as f:
    buf = f.read()

for enum_name in [b'NET_Messages', b'CLC_Messages', b'SVC_Messages']:
    off = buf.find(b'\n' + enum_name + b'\x12')
    if off != -1:
        end = buf.find(b'*\n', off)
        if end == -1: end = off + 1500
        chunk = buf[off:end]
        print(f"\nEnum: {enum_name.decode()}")
        # Each enum value is formatted in proto descriptor:
        # \x12 <len> <name> \x10 <varint id>
        matches = re.finditer(rb'\x12([^\x10]+)\x10([\x01-\x7F])', chunk)
        for m in matches:
            val_name = m.group(1).strip(b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f')
            val_id = m.group(2)[0]
            val_str = val_name.decode('latin1', errors='ignore')
            clean_str = "".join(c for c in val_str if c.isalnum() or c == '_')
            print(f"  {clean_str:35s} = {val_id}")
