import struct

dll_path = r"versions/archive/2026-10-15_25966348\server.dll"

with open(dll_path, "rb") as f:
    data = f.read()

# Search for the string
target_str = b"CLagCompensationManager::StartLagCompensation with NULL CUserCmd!!!"
str_offset = data.find(target_str)
if str_offset == -1:
    print("String not found in binary")
    exit(1)

print(f"Found string at file offset: 0x{str_offset:X}")

# Parse PE header to get ImageBase and section headers
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

def va_to_file_offset(va):
    rva = va - image_base
    for s in sections:
        if s["vaddr"] <= rva < s["vaddr"] + s["vsize"]:
            return s["raddr"] + (rva - s["vaddr"])
    return None

target_va = file_offset_to_va(str_offset)
print(f"Target String VA: 0x{target_va:X}")

# Find .text section
text_sec = next(s for s in sections if s["name"] == ".text")
text_data = data[text_sec["raddr"] : text_sec["raddr"] + text_sec["rsize"]]
text_start_va = image_base + text_sec["vaddr"]

print(f".text VA: 0x{text_start_va:X} - 0x{text_start_va + text_sec['vsize']:X}")

# Search for RIP-relative references (displacement = target_va - (ins_va + ins_len))
# Common instruction lengths for LEA/MOV/CALL are 7 bytes (REX + opcode + modrm + disp32)
# or 6 bytes (opcode + modrm + disp32) or 5 bytes (opcode + disp32)
hits = []
for ins_len in [7, 6, 5]:
    for i in range(len(text_data) - ins_len):
        ins_va = text_start_va + i
        next_ip = ins_va + ins_len
        expected_disp = target_va - next_ip
        if -0x80000000 <= expected_disp <= 0x7FFFFFFF:
            disp_bytes = struct.pack("<i", expected_disp)
            if text_data[i + ins_len - 4 : i + ins_len] == disp_bytes:
                hits.append((ins_va, ins_len, expected_disp))

print(f"Found {len(hits)} reference(s):")
for va, l, d in hits:
    print(f"  Ref at VA: 0x{va:X} (len: {l}, disp: 0x{d:X})")
