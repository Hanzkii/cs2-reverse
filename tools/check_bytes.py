with open(r"versions/archive/2026-10-15_25966348\schemasystem.dll", "rb") as f:
    data = f.read()

import struct

off = 0x4aa50
print("Bytes around 0x4aa50:")
for i in range(-32, 48, 8):
    val = struct.unpack_from("<Q", data, off + i)[0]
    print(f"  off 0x{off+i:x}: 0x{val:016x}")
