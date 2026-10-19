import explore_network as en
import re

print("Searching for Field Type encoder/decoder names...")
types = [b'uint32', b'int32', b'float32', b'Vector', b'QAngle', b'CUtlStringToken', b'bool', b'CNetworkedQuantizedFloat', b'uint64']

for t in types:
    off = en.buf.find(t + b'\x00')
    if off != -1:
        va = en.offset_to_va(off)
        print(f"Type '{t.decode()}' at VA: 0x{va:X}")
        # check ptr refs
        import struct
        b_va = struct.pack('<Q', va)
        for i in range(len(en.buf)-7):
            if en.buf[i:i+8] == b_va:
                print(f"  ptr ref at VA 0x{en.offset_to_va(i):X}")
