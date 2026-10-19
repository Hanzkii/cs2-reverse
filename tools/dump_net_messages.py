import explore_network as en
import re

print("=== Extracting All CS2 Network Message Types ===")
with open(r'versions/archive/2026-10-15_25966348\networksystem.dll', 'rb') as f:
    buf = f.read()

# Match CNETMsg_*, CSVCMsg_*, CCLCMsg_*
patterns = [b'CNETMsg_[A-Za-z0-9_]+', b'CSVCMsg_[A-Za-z0-9_]+', b'CCLCMsg_[A-Za-z0-9_]+']
found = set()
for pat in patterns:
    for m in re.finditer(pat, buf):
        name = m.group().decode('latin1')
        if not name.endswith('_t') and not '.' in name:
            found.add(name)

net_msgs = sorted([x for x in found if x.startswith('CNETMsg_')])
svc_msgs = sorted([x for x in found if x.startswith('CSVCMsg_')])
clc_msgs = sorted([x for x in found if x.startswith('CCLCMsg_')])

print(f"\n--- CNETMsg ({len(net_msgs)}) ---")
for m in net_msgs:
    print(" ", m)

print(f"\n--- CCLCMsg (Client-to-Server) ({len(clc_msgs)}) ---")
for m in clc_msgs:
    print(" ", m)

print(f"\n--- CSVCMsg (Server-to-Client) ({len(svc_msgs)}) ---")
for m in svc_msgs:
    print(" ", m)
