import explore_network as en
import struct

# The 40 operations have standard default weights in Valve's fieldpathpacker.cpp:
# PlusOne is by far the highest frequency (~36000), PlusTwo (~1000), PlusThree, PlusFour, etc.
# FieldPathEncodeFinish is also common (~10000)
# Let's search in .rdata or .data for an array of 40 integers

rdata_sec = [s for s in en.sections if s['name'] == '.rdata'][0]
data = en.buf[rdata_sec['roff']:rdata_sec['roff']+rdata_sec['rsize']]

print(f"Scanning .rdata for 40-element frequency/weight table...")
for i in range(0, len(data) - 160, 4):
    v0, v1, v2, v3 = struct.unpack_from('<IIII', data, i)
    # Check if v0 is very large (e.g. > 10000) and v1, v2, v3 decrease or match known fieldpath weights
    if 20000 <= v0 <= 100000 and 100 <= v1 <= 10000 and 50 <= v2 <= 5000:
        va = en.image_base + rdata_sec['rva'] + i
        print(f"Candidate weight table at VA 0x{va:X}:")
        weights = struct.unpack_from('<40I', data, i)
        print(" ", weights[:10])
        print(" ", weights[10:20])
        print(" ", weights[20:30])
        print(" ", weights[30:40])
