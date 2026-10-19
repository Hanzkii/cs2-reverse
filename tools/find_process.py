import explore_network as en
import re

# In Ghidra, let's find references to g_pNetworkMessages (0x1802a3f30) or find NetMessage reading routines
# Let's search for "CNetworkMessages::FindOrCreateNetMessage" or calls to GetNetMessageInfo
print("Searching for NetMessage reading functions...")
# Let's find xrefs to FUN_1800e23d0 or related functions
# Let's search for the string "net_messageinfo" or "NetMessage"
with open(r'versions/archive/2026-10-15_25966348\networksystem.dll', 'rb') as f:
    buf = f.read()

# Let's search for CNetChan::ProcessPacket or ProcessStream
matches = re.finditer(b'CNetChan::Process[A-Za-z0-9_]*', buf)
for m in matches:
    print("Found string:", m.group().decode())
    str_va = en.offset_to_va(m.start())
    print(f"  at VA 0x{str_va:X}")
