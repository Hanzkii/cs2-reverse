import explore_network as en

off = en.va_to_offset(0x18021FCE8)
# Scan backward to find the start of the proto descriptor (usually starts with filename like 'netmessages.proto' or 0x0A)
start = off - 200
end = off + 500
proto_slice = en.buf[start:end]

print("Embedded proto slice:")
# Find 'netmessages.proto'
p_idx = en.buf.find(b'netmessages.proto', off - 500)
if p_idx != -1:
    print(f"Found 'netmessages.proto' at file offset 0x{p_idx:X}, VA: 0x{en.offset_to_va(p_idx):X}")
    print("Dump:", repr(en.buf[p_idx-2:p_idx+300]))
