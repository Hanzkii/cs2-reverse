with open(r"versions/archive/2026-10-15_25966348\server.dll", "rb") as f:
    data = f.read()

idx = data.find(b"player_death\x00")
print("player_death at file offset: 0x%x" % idx)

# Inspect the 500 bytes surrounding this string
chunk = data[max(0, idx-50):min(len(data), idx+600)]
import re
strs = re.findall(b"[a-zA-Z0-9_]{2,}", chunk)
print("Strings around player_death:")
for s in strs:
    print("  ", s.decode('latin-1'))
