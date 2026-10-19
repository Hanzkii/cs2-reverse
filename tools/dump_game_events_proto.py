with open(r"versions/archive/2026-10-15_25966348\networksystem.dll", "rb") as f:
    data = f.read()

import re
matches = [
    b"CSVCMsg_GameEventList",
    b"CSVCMsg_GameEvent",
    b"CSVCMsgList_GameEvents"
]

for name in matches:
    idx = 0
    while True:
        idx = data.find(name, idx)
        if idx == -1: break
        print(f"\nMatch {name.decode()} at 0x{idx:x}:")
        print("Context:", data[max(0, idx-20):min(len(data), idx+300)])
        idx += len(name)
