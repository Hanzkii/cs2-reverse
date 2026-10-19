import os
import struct
import sys

DEFAULT_DIR = os.environ.get("CS2_DIR", "")

def scan_module_for_classes(dll_name, keywords):
    dll_path = os.path.join(DEFAULT_DIR, dll_name)
    if not os.path.exists(dll_path):
        print(f"File not found: {dll_path}")
        return

    print(f"\n=======================================================")
    print(f"Scanning {dll_name} for classes matching: {keywords}")
    print(f"=======================================================")

    with open(dll_path, "rb") as f:
        data = f.read()

    image_base = 0x180000000
    pe_sig = struct.unpack_from("<I", data, 0x3C)[0]
    num_sec = struct.unpack_from("<H", data, pe_sig + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe_sig + 20)[0]
    sec_off = pe_sig + 24 + opt_size

    sections = []
    for i in range(num_sec):
        s = data[sec_off + i*40 : sec_off + (i+1)*40]
        name = s[:8].decode('latin-1').strip('\x00')
        vsize, rva, raw_size, raw_ptr = struct.unpack_from("<IIII", s, 8)
        sections.append((name, rva, vsize, raw_ptr, raw_size))

    def va_to_off(v):
        r = v - image_base
        for name, rva, vsize, raw_ptr, raw_size in sections:
            if rva <= r < rva + vsize:
                off = raw_ptr + (r - rva)
                if off < len(data):
                    return off
        return None

    def off_to_va(o):
        for name, rva, vsize, raw_ptr, raw_size in sections:
            if raw_ptr <= o < raw_ptr + raw_size:
                return image_base + rva + (o - raw_ptr)
        return None

    def read_str(va):
        off = va_to_off(va)
        if off is None or off >= len(data): return ""
        end = data.find(b"\x00", off)
        if end == -1 or end - off > 256: return ""
        return data[off:end].decode('latin-1', errors='ignore')

    # Find candidate class names in ASCII strings
    found_classes = set()
    for kw in keywords:
        pos = 0
        kw_b = kw.encode('latin-1')
        while True:
            pos = data.find(kw_b, pos)
            if pos == -1: break
            
            # Check string bounds
            start = pos
            while start > 0 and data[start-1] >= 0x20 and data[start-1] <= 0x7E:
                start -= 1
            end = pos
            while end < len(data) and data[end] >= 0x20 and data[end] <= 0x7E:
                end += 1
            
            candidate_name = data[start:end].decode('latin-1', errors='ignore')
            pos = end + 1

            if candidate_name.startswith("C_") or candidate_name.startswith("C") or candidate_name.startswith("Smoke") or "Smoke" in candidate_name or "Voxel" in candidate_name or "Volume" in candidate_name:
                str_va = off_to_va(start)
                if not str_va: continue

                # Search for ptr to str_va in rdata/data
                ptr_bytes = struct.pack("<Q", str_va)
                pidx = 0
                while True:
                    pidx = data.find(ptr_bytes, pidx)
                    if pidx == -1: break
                    
                    class_off = pidx - 8
                    if class_off >= 0:
                        class_va = off_to_va(class_off)
                        size20 = struct.unpack_from("<I", data, class_off + 0x20)[0]
                        fields24 = struct.unpack_from("<H", data, class_off + 0x24)[0]
                        fields_ptr = struct.unpack_from("<Q", data, class_off + 0x30)[0]
                        base_ptr = struct.unpack_from("<Q", data, class_off + 0x38)[0]

                        if 0 < size20 < 2000000 and 0 <= fields24 < 2000 and class_va not in found_classes:
                            found_classes.add(class_va)
                            base_name = ""
                            if base_ptr != 0:
                                base_off = va_to_off(base_ptr)
                                if base_off and base_off + 16 <= len(data):
                                    binfo = struct.unpack_from("<Q", data, base_off + 8)[0]
                                    if binfo:
                                        binfo_off = va_to_off(binfo)
                                        if binfo_off and binfo_off + 16 <= len(data):
                                            bname_ptr = struct.unpack_from("<Q", data, binfo_off + 8)[0]
                                            base_name = read_str(bname_ptr)

                            print(f"\n[Class Found] {candidate_name} (VA: 0x{class_va:x}, Size: {size20} bytes, Base: '{base_name}', Fields: {fields24})")
                            f_off = va_to_off(fields_ptr)
                            if f_off and fields24 > 0:
                                for fi in range(fields24):
                                    field_chunk = data[f_off + fi * 0x20 : f_off + (fi + 1) * 0x20]
                                    if len(field_chunk) < 0x20: break
                                    fname_va = struct.unpack_from("<Q", field_chunk, 0)[0]
                                    ftype_va = struct.unpack_from("<Q", field_chunk, 8)[0]
                                    f_offset = struct.unpack_from("<I", field_chunk, 0x10)[0]
                                    fname = read_str(fname_va)
                                    type_name = ""
                                    t_off = va_to_off(ftype_va)
                                    if t_off:
                                        tname_va = struct.unpack_from("<Q", data, t_off + 8)[0]
                                        type_name = read_str(tname_va)
                                    print(f"    +0x{f_offset:04x}: {fname} ({type_name})")

                    pidx += 8

if __name__ == "__main__":
    keywords = ["SmokeGrenade", "SmokeVolume", "Volumetric", "Voxel"]
    scan_module_for_classes("server.dll", keywords)
    scan_module_for_classes("client.dll", keywords)
    scan_module_for_classes("particles.dll", keywords)
