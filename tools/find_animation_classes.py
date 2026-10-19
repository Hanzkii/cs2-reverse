import struct
import sys

def analyze_module(module_path, target_names):
    print(f"\n==================================================")
    print(f"Analyzing {module_path}")
    print(f"==================================================")
    
    with open(module_path, "rb") as f:
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

    # Find schema classes
    # Look for each target_name as a string
    for name in target_names:
        needle = name.encode('ascii') + b"\x00"
        idx = 0
        found_classes = set()
        while True:
            idx = data.find(needle, idx)
            if idx == -1: break
            str_va = off_to_va(idx)
            idx += len(needle)
            if not str_va: continue
            
            # Now find references to str_va in pointers
            ptr_bytes = struct.pack("<Q", str_va)
            pidx = 0
            while True:
                pidx = data.find(ptr_bytes, pidx)
                if pidx == -1: break
                
                # Check if this pointer is at class_off + 8
                class_off = pidx - 8
                if class_off >= 0:
                    class_va = off_to_va(class_off)
                    # Check if class has valid fields
                    # class structure:
                    # +0x00: self ptr or 0
                    # +0x08: name_ptr
                    # +0x10: module_name_ptr
                    # +0x18: size (or +0x20 depending on version)
                    # Let's inspect fields
                    # Let's read field count: +0x1c (short) or +0x28 (short)
                    # In cs2:
                    # class_info:
                    # +0x00: pNext
                    # +0x08: pszName
                    # +0x10: pszModule
                    # +0x18: nSize
                    # +0x1c: nFields
                    size18 = struct.unpack_from("<I", data, class_off + 0x18)[0]
                    fields1c = struct.unpack_from("<H", data, class_off + 0x1c)[0]
                    pfields28 = struct.unpack_from("<Q", data, class_off + 0x28)[0]
                    
                    size20 = struct.unpack_from("<I", data, class_off + 0x20)[0]
                    fields24 = struct.unpack_from("<H", data, class_off + 0x24)[0]
                    fields_ptr = struct.unpack_from("<Q", data, class_off + 0x30)[0]
                    base_ptr = struct.unpack_from("<Q", data, class_off + 0x38)[0]

                    matched = False
                    if 0 < size20 < 1000000 and 0 <= fields24 < 1000:
                        cls_size = size20
                        num_fields = fields24
                        matched = True
                        fields_va = fields_ptr
                    
                    if matched and class_va not in found_classes:
                        found_classes.add(class_va)
                        base_name = ""
                        if base_ptr != 0:
                            base_off = va_to_off(base_ptr)
                            if base_off and base_off + 16 <= len(data):
                                base_info_ptr = struct.unpack_from("<Q", data, base_off + 8)[0]
                                if base_info_ptr:
                                    base_info_off = va_to_off(base_info_ptr)
                                    if base_info_off and base_info_off + 16 <= len(data):
                                        base_name_ptr = struct.unpack_from("<Q", data, base_info_off + 8)[0]
                                        base_name = read_str(base_name_ptr)

                        print(f"\n--- Class: {name} (VA: 0x{class_va:x}, Size: {cls_size} bytes, Base: '{base_name}', Fields: {num_fields}) ---")
                        
                        # Walk fields
                        f_off = va_to_off(fields_va)
                        if f_off and num_fields > 0:
                            for fi in range(num_fields):
                                field_chunk = data[f_off + fi * 0x20 : f_off + (fi + 1) * 0x20]
                                if len(field_chunk) < 0x20: break
                                fname_va = struct.unpack_from("<Q", field_chunk, 0)[0]
                                ftype_va = struct.unpack_from("<Q", field_chunk, 8)[0]
                                f_offset = struct.unpack_from("<I", field_chunk, 0x10)[0]
                                
                                fname = read_str(fname_va)
                                # Type info
                                type_name = ""
                                t_off = va_to_off(ftype_va)
                                if t_off:
                                    tname_va = struct.unpack_from("<Q", data, t_off + 8)[0]
                                    type_name = read_str(tname_va)
                                
                                print(f"  +0x{f_offset:04x} : {fname} ({type_name})")
                
                pidx += 8

if __name__ == "__main__":
    targets = [
        "CSkeletonInstance",
        "CGameSceneNode",
        "CModelState",
        "CSkeletonAnimationController",
        "CBaseAnimGraphController",
        "CAnimGraphControllerBase",
        "CHitBox",
        "CHitBoxSet",
        "CHitBoxSetList",
        "BoneJointData_t",
        "CAnimationGroup",
        "CSeqAutoLayerFlag",
        "CAnimDesc_Flag"
    ]
    analyze_module(r"versions/archive/2026-10-15_25966348\client.dll", targets)
    analyze_module(r"versions/archive/2026-10-15_25966348\animationsystem.dll", targets)
