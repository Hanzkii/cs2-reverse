with open(r"versions/archive/2026-10-15_25966348\server.dll", "rb") as f:
    data = f.read()

import re
matches = [
    b"player_death\x00",
    b"bomb_planted\x00",
    b"round_start\x00",
    b"bullet_impact\x00"
]

for name in matches:
    idx = 0
    count = 0
    while True:
        idx = data.find(name, idx)
        if idx == -1: break
        count += 1
        idx += len(name)
    print(f"Event '{name[:-1].decode()}' occurs {count} times in server.dll")
