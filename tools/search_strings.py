import explore_network as en
import re

print("=== Strings with 'packet' / 'split' / 'net_' / 'reliable' ===")
for s in en.sections:
    data = en.buf[s['roff']:s['roff']+s['rsize']]
    # Find all ascii strings of length >= 6
    matches = re.finditer(b'[a-zA-Z0-9_:\.]{4,60}', data)
    for m in matches:
        word = m.group().decode('latin1', errors='ignore')
        w_lower = word.lower()
        if any(k in w_lower for k in ['splitpacket', 'split_packet', 'net_message', 'cnetmsg', 'clcmsg', 'svcmsg', 'netmessage', 'packetheader', 'unreliable', 'sequencenumber', 'subtick']):
            va = en.image_base + s['rva'] + m.start()
            print(f"0x{va:X}: {word}")
