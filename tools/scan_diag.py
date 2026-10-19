import struct

with open(r"versions/archive/2026-10-15_25966348\server.dll", "rb") as f:
    data = f.read()

start_off = 0xED6000 - 0xC00
end_off = 0xED9000 - 0xC00
chunk = data[start_off:end_off]

for i in range(len(chunk) - 7):
    ins_va = 0x180ED6000 + i
    disp = struct.unpack("<i", chunk[i+3:i+7])[0]
    target_va = ins_va + 7 + disp
    if 0x1819C0000 <= target_va <= 0x1819D0000:
        file_off = target_va - 0x180000000 - 0x1763000 + 0x1761800
        if 0 <= file_off < len(data):
            s = data[file_off:file_off+80].split(b"\x00")[0]
            if len(s) > 3 and all(32 <= b <= 126 for b in s):
                print(f"VA 0x{ins_va:X} -> {s.decode('ascii')}")
