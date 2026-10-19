import struct

with open(r"versions/archive/2026-10-15_25966348\networksystem.dll", "rb") as f:
    data = f.read()

import re
matches = re.findall(b"net_SetConVar|CNETMsg_SetConVar[a-zA-Z0-9_]*|CMsg[A-Za-z0-9_]*ConVar[A-Za-z0-9_]*", data)
print("Matches in networksystem.dll:", set(m.decode() for m in matches))

with open(r"versions/archive/2026-10-15_25966348\engine2.dll", "rb") as f:
    data_e = f.read()

matches_e = re.findall(b"CNETMsg_SetConVar[a-zA-Z0-9_]*|CMsg[A-Za-z0-9_]*ConVar[A-Za-z0-9_]*", data_e)
print("Matches in engine2.dll:", set(m.decode() for m in matches_e))
