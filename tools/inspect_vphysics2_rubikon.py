"""
Source 2 Rubikon Physics Engine (vphysics2.dll) - RTTI & VTable Inspector
Extracts interface VTables, Rubikon rigid body & collision shape layouts,
ray/hull trace queries, and spatial acceleration dispatch tables.
"""

import struct
import re
import os
import argparse

DEFAULT_DLL = r"versions/archive/2026-10-15_25966348\vphysics2.dll"

def inspect_vphysics(dll_path):
    if not os.path.exists(dll_path):
        print(f"Error: {dll_path} not found.")
        return

    with open(dll_path, "rb") as f:
        buf = f.read()

    # Parse PE
    e_lfanew = struct.unpack_from("<I", buf, 0x3c)[0]
    num_sections = struct.unpack_from("<H", buf, e_lfanew + 6)[0]
    opt_hdr_size = struct.unpack_from("<H", buf, e_lfanew + 20)[0]
    image_base = struct.unpack_from("<Q", buf, e_lfanew + 24 + 24)[0]

    sec_start = e_lfanew + 24 + opt_hdr_size
    sections = []
    for i in range(num_sections):
        sec = buf[sec_start + i * 40 : sec_start + (i + 1) * 40]
        name = sec[:8].rstrip(b"\x00").decode("latin1")
        vsize, rva, rsize, roff = struct.unpack_from("<IIII", sec, 8)
        sections.append({
            "name": name,
            "vsize": vsize,
            "rva": rva,
            "rsize": rsize,
            "roff": roff
        })

    def rva_to_off(rva):
        for s in sections:
            if s["rva"] <= rva < s["rva"] + s["vsize"]:
                return s["roff"] + (rva - s["rva"])
        return None

    def va_to_off(va):
        return rva_to_off(va - image_base)

    def off_to_rva(off):
        for s in sections:
            if s["roff"] <= off < s["roff"] + s["rsize"]:
                return s["rva"] + (off - s["roff"])
        return 0

    def off_to_va(off):
        rva = off_to_rva(off)
        return image_base + rva if rva else 0

    def read_str(va):
        off = va_to_off(va)
        if off is None or off >= len(buf): return ""
        end = buf.find(b"\x00", off)
        return buf[off:end].decode("latin-1", errors="ignore")

    print("=" * 80)
    print(f" Source 2 Rubikon Physics Analyzer (vphysics2.dll)")
    print(f" Binary:    {dll_path}")
    print(f" ImageBase: 0x{image_base:016X} | Size: {len(buf):,} bytes")
    print("=" * 80)

    # Target classes to locate and profile
    target_classes = [
        "CVPhysics2Interface",
        "CVPhys2World",
        "CFnPhysicsWorldImpl",
        "CFnPhysicsBodyImpl",
        "CFnPhysicsShapeImpl",
        "CFnPhysicsCapsuleShapeImpl",
        "CFnPhysicsHullShapeImpl",
        "CFnPhysicsMeshShapeImpl",
        "CFnPhysicsCompoundShapeImpl",
        "CRnBody",
        "CRnShape",
        "CRnHullShape",
        "CRnMeshShape",
        "CRnCapsuleShape",
        "CRnSphereShape",
        "CRnCompoundShape",
        "CTraceRay",
        "CTracePoint",
        "CTraceCapsule",
        "CTraceHull",
        "CTraceAABB",
        "CTraceOBB",
        "CVPhysics2SurfacePropertyManager",
        "CWorldStepJob"
    ]

    print(f"\n[1] Resolving {len(target_classes)} Rubikon RTTI Descriptors & VTables:")
    for cls in target_classes:
        pattern = f".?AV{cls}@@".encode("latin1")
        off = buf.find(pattern)
        if off == -1:
            continue
        td_off = off - 16
        td_rva = off_to_rva(td_off)

        # Find CompleteObjectLocator
        td_rva_packed = struct.pack("<I", td_rva)
        col_off = -1
        idx = 0
        while True:
            idx = buf.find(td_rva_packed, idx)
            if idx == -1:
                break
            if idx >= 12:
                sig, disp, cdDisp = struct.unpack_from("<III", buf, idx - 12)
                if sig == 1 and disp == 0:
                    col_off = idx - 12
                    break
            idx += 1

        if col_off == -1:
            continue

        col_va = off_to_va(col_off)
        col_va_packed = struct.pack("<Q", col_va)
        p = 0
        vtables = []
        while True:
            p = buf.find(col_va_packed, p)
            if p == -1:
                break
            vtable_va = off_to_va(p + 8)
            vtables.append(vtable_va)
            p += 1

        vt_str = ", ".join(f"0x{v:X}" for v in vtables) if vtables else "None"
        print(f"[*] Class: {cls:<32} | COL: 0x{col_va:X} | VTable: {vt_str}")

        if vtables:
            primary_vt = vtables[0]
            vt_off = va_to_off(primary_vt)
            if vt_off:
                methods = []
                for m_idx in range(80):
                    m_va = struct.unpack_from("<Q", buf, vt_off + m_idx * 8)[0]
                    if 0x180000000 <= m_va <= 0x180400000:
                        methods.append(m_va)
                    else:
                        break
                print(f"    Methods: {len(methods):2d} slots | First 4 slots: {', '.join(f'0x{m:X}' for m in methods[:4])}")

    print("\n[2] Extracting Key Schema Registration Tables (physicslib):")
    # Table UNK_1803dadb0 contains 118 classes
    table_va = 0x1803dadb0
    table_off = va_to_off(table_va)
    if table_off:
        print(f"   Schema Registration Table at VA 0x{table_va:X} (118 declared classes)")
        sample_classes = [1, 3, 5, 6, 82, 87, 90, 93]
        for i in sample_classes:
            cls_ptr_va = struct.unpack_from("<Q", buf, table_off + i * 8)[0]
            cls_off = va_to_off(cls_ptr_va)
            if not cls_off: continue
            name_ptr = struct.unpack_from("<Q", buf, cls_off + 8)[0]
            cls_name = read_str(name_ptr)
            size = struct.unpack_from("<I", buf, cls_off + 0x20)[0]
            num_f = struct.unpack_from("<H", buf, cls_off + 0x24)[0]
            print(f"   -> [{i:2d}] {cls_name:<28} Size: 0x{size:04X} ({size:3d}b) | Fields: {num_f}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Source 2 Rubikon Physics Analyzer")
    parser.add_argument("--dll", default=DEFAULT_DLL, help="Path to vphysics2.dll")
    args = parser.parse_args()
    inspect_vphysics(args.dll)
