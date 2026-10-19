import explore_client as ec
import re

print("=== Searching for Subtick / UserCmd / Prediction strings in client.dll ===")
keywords = [b'CSGOUserCmd', b'CSubtickMoveStep', b'CInButtonState', b'subtick', b'SubTick', b'sub_tick', b'CreateMove', b'RunCommand', b'CPrediction']

found_strings = {}
for kw in keywords:
    idx = 0
    count = 0
    while count < 10:
        idx = ec.buf.find(kw, idx)
        if idx == -1: break
        va = ec.offset_to_va(idx)
        # Grab surrounding ASCII
        start = max(0, idx - 20)
        end = min(len(ec.buf), idx + 60)
        s = ec.buf[start:end]
        found_strings[va] = (kw.decode(), s)
        count += 1
        idx += len(kw)

for va in sorted(found_strings.keys())[:30]:
    kw, raw = found_strings[va]
    print(f"0x{va:X} [{kw}]: {raw}")
