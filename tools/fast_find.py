import struct
import re

dll_path = r"versions/archive/2026-10-15_25966348\server.dll"

with open(dll_path, "rb") as f:
    data = f.read()

target_str = b"CLagCompensationManager::StartLagCompensation with NULL CUserCmd!!!"
str_offset = data.find(target_str)
print(f"Found string at file offset: 0x{str_offset:X}")

e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
num_sections = struct.unpack_from("<H", data, e_lfanew + 6)[0]
opt_hdr_size = struct.unpack_from("<H", data, e_lfanew + 20)[0]
image_base = struct.unpack_from("<Q", data, e_lfanew + 24 + 24)[0]

sections = []
sec_hdr_start = e_lfanew + 24 + opt_hdr_size
for i in range(num_sections):
    sec = data[sec_hdr_start + i * 40 : sec_hdr_start + (i + 1) * 40]
    name = sec[:8].rstrip(b"\x00").decode("ascii", errors="ignore")
    vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", sec, 8)
    sections.append({
        "name": name,
        "vaddr": vaddr,
        "vsize": vsize,
        "raddr": raddr,
        "rsize": rsize
    })

def file_offset_to_va(offset):
    for s in sections:
        if s["raddr"] <= offset < s["raddr"] + s["rsize"]:
            return image_base + s["vaddr"] + (offset - s["raddr"])
    return None

target_va = file_offset_to_va(str_offset)
print(f"Target String VA: 0x{target_va:X}")

text_sec = next(s for s in sections if s["name"] == ".text")
text_data = data[text_sec["raddr"] : text_sec["raddr"] + text_sec["rsize"]]
text_start_va = image_base + text_sec["vaddr"]

# Search regex for LEA RCX/RDX/R8/R9, [RIP + disp32]
# Pattern: (48 8d 0d | 48 8d 15 | 4c 8d 05 | 4c 8d 0d)
pattern = re.compile(b"(\x48\x8d[\x0d\x15]|\x4c\x8d[\x05\x0d])(....)", re.DOTALL)

for m in pattern.finditer(text_data):
    ins_offset = m.start()
    disp = struct.unpack("<i", m.group(2))[0]
    ins_va = text_start_va + ins_offset
    target = ins_va + 7 + disp
    if target == target_va:
        print(f"MATCH FOUND at VA: 0x{ins_va:X}!")
