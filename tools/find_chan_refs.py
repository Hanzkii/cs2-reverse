import explore_network as en
import struct

def find_string_refs(str_va):
    refs = []
    for s in en.sections:
        s_data = en.buf[s['roff']:s['roff']+s['rsize']]
        for i in range(len(s_data) - 7):
            # RIP-relative LEA / MOV
            for disp_off in [2, 3]:
                if i + disp_off + 4 <= len(s_data):
                    disp = struct.unpack_from('<i', s_data, i + disp_off)[0]
                    target = en.image_base + s['rva'] + i + disp_off + 4 + disp
                    if target == str_va:
                        refs.append(en.image_base + s['rva'] + i)
    return refs

targets = [
    (0x180236ED8, "CNetChan::BUF_UNRELIABLE"),
    (0x180237220, "NetChanTransmitUnreliable"),
    (0x180237B60, "CNetChan::SendNetMessage"),
    (0x1802385B0, "CNetworkMessages::FindOrCreateNetMessage")
]

for va, name in targets:
    refs = find_string_refs(va)
    print(f"References to '{name}' (0x{va:X}):")
    for r in refs:
        print(f"  at 0x{r:X}")
