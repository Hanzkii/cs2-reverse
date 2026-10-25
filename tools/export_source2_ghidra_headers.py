#!/usr/bin/env python3
"""
Source 2 Header Generator for Ghidra / C Compilers
Generates a complete, cleanly padded C header file containing all Source 2
SchemaSystem classes and standard primitives extracted from CS2 binaries.
"""

import os
import sys
import json
import re

def deduce_type_and_size(name: str, span: int) -> tuple:
    """Deduces C type and byte size based on Hungarian prefix and byte span."""
    if name.startswith("m_p") or name.startswith("p") and len(name) > 2 and name[1].isupper():
        return "void*", 8
    if name.startswith("m_b") or name.startswith("b") and len(name) > 2 and name[1].isupper():
        return "bool", 1
    if name.startswith("m_fl") or name.startswith("fl") and len(name) > 3 and name[2].isupper():
        return "float", 4
    if name.startswith("m_h") or name.startswith("h") and len(name) > 2 and name[1].isupper():
        return "CEntityHandle", 4
    if name.startswith("m_vec"):
        if span == 8:
            return "Vector2D", 8
        elif span == 16:
            return "Vector4D", 16
        return "Vector", 12
    if name.startswith("m_ang"):
        return "QAngle", 12
    if name.startswith("m_quat") or name.startswith("m_q"):
        return "Quaternion", 16
    if name.startswith("m_clr") or name.startswith("m_Color"):
        return "uint32_t", 4
    if name.startswith("m_i") or name.startswith("m_n"):
        if span == 1:
            return "uint8_t", 1
        elif span == 2:
            return "uint16_t", 2
        elif span == 8:
            return "int64_t", 8
        return "int32_t", 4
    if name.startswith("m_sz") or name.startswith("m_str") or name.startswith("m_psz"):
        if span == 8:
            return "char*", 8
        return f"char[{span}]" if span > 0 else "char*", span if span > 0 else 8

    # Fallback based on span
    if span == 1:
        return "uint8_t", 1
    elif span == 2:
        return "uint16_t", 2
    elif span == 4:
        return "uint32_t", 4
    elif span == 8:
        return "uint64_t", 8
    elif span == 12:
        return "Vector", 12
    elif span == 16:
        return "Vector4D", 16
    elif span > 0:
        return f"uint8_t[{span}]", span
    return "uint32_t", 4

def generate_headers(schemas_path: str, out_path: str, module_filter: str = None):
    with open(schemas_path, "r", encoding="utf-8") as f:
        schemas = json.load(f)

    classes_to_process = []
    for cls_name, cls_info in schemas.items():
        if module_filter and cls_info.get("binary", "").lower() != module_filter.lower():
            continue
        classes_to_process.append((cls_name, cls_info))

    classes_to_process.sort(key=lambda x: x[0])

    lines = []
    lines.append("/* Auto-generated Source 2 Ghidra C Header */")
    lines.append("/* Compatible with Ghidra C-Parser and C/C++ decompilers */")
    lines.append("#ifndef SOURCE2_TYPES_H")
    lines.append("#define SOURCE2_TYPES_H\n")
    lines.append("typedef unsigned char uint8_t;")
    lines.append("typedef unsigned short uint16_t;")
    lines.append("typedef unsigned int uint32_t;")
    lines.append("typedef unsigned long long uint64_t;")
    lines.append("typedef signed char int8_t;")
    lines.append("typedef short int16_t;")
    lines.append("typedef int int32_t;")
    lines.append("typedef long long int64_t;")
    lines.append("typedef unsigned char bool;\n")

    lines.append("typedef struct Vector { float x; float y; float z; } Vector;")
    lines.append("typedef struct Vector2D { float x; float y; } Vector2D;")
    lines.append("typedef struct Vector4D { float x; float y; float z; float w; } Vector4D;")
    lines.append("typedef struct QAngle { float pitch; float yaw; float roll; } QAngle;")
    lines.append("typedef struct Quaternion { float x; float y; float z; float w; } Quaternion;")
    lines.append("typedef struct CTransform { Quaternion rot; Vector pos; float pad; } CTransform;")
    lines.append("typedef struct CEntityHandle { uint32_t m_Index; } CEntityHandle;")
    lines.append("typedef struct CUtlString { char* m_pszString; } CUtlString;")
    lines.append("typedef struct CUtlSymbolLarge { char* m_pString; } CUtlSymbolLarge;")
    lines.append("typedef struct CUtlStringToken { uint32_t m_nHashCode; } CUtlStringToken;")
    lines.append("typedef struct CUtlVector_void { int m_Size; int m_Capacity; void* m_pElements; } CUtlVector_void;\n")

    # Forward declare all classes
    lines.append("/* Forward Declarations */")
    for cls_name, _ in classes_to_process:
        # Sanitize name
        safe_name = re.sub(r"[^A-Za-z0-9_]", "_", cls_name)
        lines.append(f"typedef struct {safe_name} {safe_name};")
    lines.append("\n/* Class Definitions */\n")

    for cls_name, cls_info in classes_to_process:
        safe_cls_name = re.sub(r"[^A-Za-z0-9_]", "_", cls_name)
        total_size = cls_info.get("size", 0)
        raw_fields = cls_info.get("fields", [])
        
        # Sort fields by offset
        sorted_fields = sorted(raw_fields, key=lambda f: f.get("offset", 0))

        # Filter duplicates at same offset
        unique_fields = []
        seen_offsets = set()
        for f in sorted_fields:
            off = f.get("offset", 0)
            if off not in seen_offsets:
                seen_offsets.add(off)
                unique_fields.append(f)

        lines.append(f"/* Binary: {cls_info.get('binary', 'unknown')} | Base: {cls_info.get('base', 'None')} | Size: {total_size} (0x{total_size:x}) */")
        lines.append(f"struct {safe_cls_name} {{")

        curr_offset = 0
        pad_idx = 0

        for i, f in enumerate(unique_fields):
            f_off = f.get("offset", 0)
            f_name = re.sub(r"[^A-Za-z0-9_]", "_", f.get("name", f"field_{f_off}"))

            if f_off > curr_offset:
                pad_size = f_off - curr_offset
                lines.append(f"    uint8_t _pad_0x{curr_offset:x}[{pad_size}];")
                curr_offset = f_off

            # Calculate span to next field or end of struct
            if i + 1 < len(unique_fields):
                next_off = unique_fields[i+1].get("offset", f_off + 4)
                span = next_off - f_off
            else:
                span = total_size - f_off if total_size > f_off else 4

            t_name, t_size = deduce_type_and_size(f_name, span)

            if "[" in t_name:
                base_t, arr_part = t_name.split("[", 1)
                lines.append(f"    {base_t} {f_name}[{arr_part}; /* +0x{f_off:x} */")
            else:
                lines.append(f"    {t_name} {f_name}; /* +0x{f_off:x} */")

            curr_offset = f_off + t_size

        if total_size > curr_offset:
            pad_size = total_size - curr_offset
            lines.append(f"    uint8_t _end_pad_0x{curr_offset:x}[{pad_size}];")

        lines.append("};\n")

    lines.append("#endif /* SOURCE2_TYPES_H */\n")

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    print(f"[+] Successfully exported {len(classes_to_process)} classes to {out_path} ({os.path.getsize(out_path):,} bytes)")

if __name__ == "__main__":
    schemas_file = "versions/profiles/2026-10-25_26144732/schemas.json"
    if len(sys.argv) > 1:
        schemas_file = sys.argv[1]
    out_file = "data/source2_types.h"
    if len(sys.argv) > 2:
        out_file = sys.argv[2]
    mod_filter = sys.argv[3] if len(sys.argv) > 3 else None

    generate_headers(schemas_file, out_file, mod_filter)
