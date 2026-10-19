import explore_network as en

for s in en.sections:
    matches = [i for i in range(s['roff'], s['roff']+s['rsize']-10) 
               if b'huffman' in en.buf[i:i+30].lower() or b'fieldpath' in en.buf[i:i+30].lower()]
    for m in matches:
        print(f"Match in {s['name']} at VA 0x{en.offset_to_va(m):X}: {en.buf[m:m+50]}")
