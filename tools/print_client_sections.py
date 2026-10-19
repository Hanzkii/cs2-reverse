import explore_client as ec

for s in ec.sections:
    start = ec.image_base + s['rva']
    end = start + s['vsize']
    name = s['name']
    print(f"{name:10s}: 0x{start:X} - 0x{end:X} (file off: 0x{s['roff']:X}, size: 0x{s['rsize']:X})")
