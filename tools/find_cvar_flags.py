with open(r"versions/archive/2026-10-15_25966348\tier0.dll", "rb") as f:
    data = f.read()

import re
matches = re.findall(b"FCVAR_[a-zA-Z0-9_]+", data)
print("FCVAR strings in tier0.dll:", set(m.decode() for m in matches))

with open(r"versions/archive/2026-10-15_25966348\engine2.dll", "rb") as f:
    data_e = f.read()

matches_e = re.findall(b"FCVAR_[a-zA-Z0-9_]+", data_e)
print("FCVAR strings in engine2.dll:", set(m.decode() for m in matches_e))

# Let's search for ConVar messages in engine2.dll
cvar_msgs = re.findall(b"[a-zA-Z0-9_]*Cvar[a-zA-Z0-9_]*", data_e)
print("\nCvar strings in engine2.dll:", set(m.decode() for m in cvar_msgs if len(m) > 6))

# Search for SetConVar in engine2.dll
setconvar = re.findall(b"[a-zA-Z0-9_]*SetConVar[a-zA-Z0-9_]*", data_e)
print("\nSetConVar strings in engine2.dll:", set(m.decode() for m in setconvar))
