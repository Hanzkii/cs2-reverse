import explore_network as en

# Let's find all string literals that match messages in netmessages.proto
off = en.va_to_offset(0x18021FD18)
# Look at bytes 0x21F000 to 0x21FD20
desc_data = en.buf[off-1500:off]
# Find strings
import re
print("Strings in netmessages.proto descriptor region:")
for s in re.findall(b'[\x20-\x7E]{3,}', desc_data):
    print(" ", s.decode('latin1'))
