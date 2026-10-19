import explore_network as en
import re

print("=== Searching for Packet / Unpack / Receive functions ===")
# Search for strings containing 'ProcessPacket', 'ReceiveDatagram', 'ReadPacket', 'UnpackPacket', 'NET_ProcessPacket'
with open(r'versions/archive/2026-10-15_25966348\networksystem.dll', 'rb') as f:
    buf = f.read()

keywords = [b'ProcessPacket', b'ReceiveDatagram', b'ReadPacket', b'UnpackPacket', b'NET_ProcessPacket', b'ProcessDatagram', b'ProcessStream', b'ReadHeader']
for kw in keywords:
    idx = 0
    while True:
        idx = buf.find(kw, idx)
        if idx == -1: break
        va = en.offset_to_va(idx)
        print(f"Found '{kw.decode()}' at VA: 0x{va:X}")
        idx += len(kw)
