import explore_client as ec
import re

off = ec.va_to_offset(0x181C0F100)
# Look before 0x181C0F100 to find class name
chunk = ec.buf[off-500:off+200]
print("Context around subtick schema fields:")
for s in re.findall(rb'[a-zA-Z0-9_]{4,}', chunk):
    print(" ", s.decode('latin1'))
