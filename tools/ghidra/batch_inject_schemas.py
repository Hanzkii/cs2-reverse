#!/usr/bin/env python3
"""
batch_inject_schemas.py - Source 2 Schema Class & Struct Batch Injector for Ghidra

Parses dumped Source 2 schemas from data/extracted_schemas/*.json (with fallback to
versions/profiles/*/schemas.json and data/ghidra_sdk_classes.json), computes exact
memory layouts with base inheritance resolution, inserts explicit padding bytes
(char pad_XXXX[N];) between non-contiguous fields, synthesizes a valid C header
(data/source2_generated_classes.h), and injects the typed structures directly into
Ghidra's DataTypeManager via GhidraMCP REST API (port 8089).

Core High-Priority Target Classes:
  1.  C_BaseEntity
  2.  C_BaseModelEntity
  3.  C_CSPlayerPawn
  4.  CCSPlayerController
  5.  CCollisionProperty
  6.  CPlayer_MovementServices
  7.  C_CSWeaponBaseGun
  8.  CGameSceneNode
  9.  C_BasePlayerPawn
  10. CEntityInstance
  11. C_EconItemView
  12. C_AttributeContainer

Zero Absolute Path Disclosure: All paths use relative repository references.
"""

import os
import sys
import json
import re
import argparse
import urllib.request
import urllib.parse
from pathlib import Path
from typing import Dict, List, Set, Any, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

DEFAULT_PORT = 8089
DEFAULT_VERSION_FOLDER = "/2026-11-11_26598758"

# 12 Core High-Priority Target Classes
CORE_HIGH_PRIORITY_CLASSES = [
    "C_BaseEntity",
    "C_BaseModelEntity",
    "C_CSPlayerPawn",
    "CCSPlayerController",
    "CCollisionProperty",
    "CPlayer_MovementServices",
    "C_CSWeaponBaseGun",
    "CGameSceneNode",
    "C_BasePlayerPawn",
    "CEntityInstance",
    "C_EconItemView",
    "C_AttributeContainer",
]

# Primitives and known sizes (in bytes)
PRIMITIVE_SIZES: Dict[str, int] = {
    "bool": 1,
    "char": 1,
    "int8_t": 1,
    "uint8_t": 1,
    "byte": 1,
    "int16_t": 2,
    "uint16_t": 2,
    "short": 2,
    "int32_t": 4,
    "uint32_t": 4,
    "int": 4,
    "uint": 4,
    "float": 4,
    "float32": 4,
    "double": 8,
    "float64": 8,
    "int64_t": 8,
    "uint64_t": 8,
    "uintptr_t": 8,
    "intptr_t": 8,
    "void*": 8,
    "char*": 8,
    "CEntityIndex": 4,
    "CEntityHandle": 4,
    "CUtlStringToken": 4,
    "Color": 4,
    "Vector2D": 8,
    "Vector": 12,
    "Vector3": 12,
    "QAngle": 12,
    "Vector4D": 16,
    "Quaternion": 16,
    "CNetworkOriginCellCoordQuantizedVector": 16,
    "CTransform": 32,
    "Matrix3x4_t": 48,
    "matrix3x4_t": 48,
    "CUtlString": 8,
    "CUtlSymbolLarge": 8,
    "CUtlMemory": 16,
    "CUtlVector": 24,
}


class GhidraHttpClient:
    """HTTP client communicating with Ghidra via GhidraMCP plugin endpoint."""

    def __init__(self, host: str = "127.0.0.1", port: int = DEFAULT_PORT):
        self.base_url = f"http://{host}:{port}"

    def check_connection(self) -> Tuple[bool, str]:
        url = f"{self.base_url}/check_connection"
        try:
            with urllib.request.urlopen(url, timeout=5) as res:
                body = res.read().decode("utf-8", errors="replace")
                return (res.status == 200, body.strip())
        except Exception as ex:
            return (False, str(ex))

    def post_json(self, endpoint: str, data_dict: dict, timeout: int = 120) -> dict:
        url = f"{self.base_url}{endpoint}"
        payload = json.dumps(data_dict).encode("utf-8")
        req = urllib.request.Request(url, data=payload, headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=timeout) as res:
                text = res.read().decode("utf-8", errors="replace")
                try:
                    return json.loads(text)
                except Exception:
                    return {"result": text}
        except Exception as ex:
            return {"error": str(ex)}

    def open_program(self, path: str) -> dict:
        url = f"{self.base_url}/open_program?{urllib.parse.urlencode({'path': path})}"
        req = urllib.request.Request(url, data=b"", headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=45) as res:
                return json.loads(res.read().decode("utf-8", errors="replace"))
        except Exception as ex:
            return {"error": str(ex)}

    def save_program(self, program_name: str) -> dict:
        url = f"{self.base_url}/save_program?{urllib.parse.urlencode({'program': program_name})}"
        try:
            with urllib.request.urlopen(url, timeout=30) as res:
                return json.loads(res.read().decode("utf-8", errors="replace"))
        except Exception as ex:
            return {"error": str(ex)}

    def run_inline_script(self, java_code: str, program: str = "") -> dict:
        payload = {"code": java_code}
        if program:
            payload["program"] = program
        return self.post_json("/run_script_inline", payload, timeout=180)


def load_all_schemas(schemas_dir: Path) -> Dict[str, dict]:
    """
    Loads all schema definitions from data/extracted_schemas/*.json,
    falling back to versions/profiles or data/ghidra_sdk_classes.json if needed.
    """
    all_classes: Dict[str, dict] = {}

    if schemas_dir.is_dir():
        for json_file in sorted(schemas_dir.glob("*.json")):
            try:
                with open(json_file, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    if isinstance(data, dict):
                        for k, v in data.items():
                            if isinstance(v, dict) and "class" in v or "fields" in v:
                                all_classes[k] = v
                                if "class" not in v:
                                    v["class"] = k
            except Exception as e:
                print(f"[-] Warning: Failed to read {json_file.name}: {e}")

    # Fallback to data/ghidra_sdk_classes.json
    sdk_file = REPO_ROOT / "data" / "ghidra_sdk_classes.json"
    if sdk_file.exists():
        try:
            with open(sdk_file, "r", encoding="utf-8") as f:
                sdk_data = json.load(f)
                classes = sdk_data.get("classes", {})
                for k, v in classes.items():
                    if k not in all_classes:
                        all_classes[k] = v
                        if "class" not in v:
                            v["class"] = k
        except Exception:
            pass

    # Fallback to latest profile schemas.json
    profiles_dir = REPO_ROOT / "versions" / "profiles"
    if profiles_dir.is_dir():
        for prof in sorted(profiles_dir.iterdir(), reverse=True):
            schemas_json = prof / "schemas.json"
            if schemas_json.exists():
                try:
                    with open(schemas_json, "r", encoding="utf-8") as f:
                        prof_data = json.load(f)
                        if isinstance(prof_data, dict):
                            for k, v in prof_data.items():
                                if k not in all_classes:
                                    all_classes[k] = v
                                    if "class" not in v:
                                        v["class"] = k
                except Exception:
                    pass
                break

    return all_classes


def resolve_inheritance_chain(class_name: str, all_classes: Dict[str, dict]) -> List[str]:
    """
    Resolves the linear inheritance chain from root base class to leaf derived class.
    E.g. CEntityInstance -> C_BaseEntity -> ... -> C_CSPlayerPawn
    """
    chain: List[str] = []
    curr = class_name
    visited: Set[str] = set()

    while curr and curr not in visited:
        visited.add(curr)
        chain.append(curr)

        cls_info = all_classes.get(curr)
        if not cls_info:
            # Try alternate prefix
            alt = curr.replace("C_", "C") if "C_" in curr else ("C_" + curr[1:] if curr.startswith("C") else "")
            if alt in all_classes and alt not in visited:
                cls_info = all_classes.get(alt)
                curr = alt

        if not cls_info:
            break

        base = cls_info.get("base")
        if base and base != "None" and base != "":
            curr = base
        else:
            # Check alternative prefix for base class
            curr = None

    return list(reversed(chain))


def deduce_field_type_and_size(raw_type: str, field_name: str, span: int) -> Tuple[str, int]:
    """
    Computes canonical C type name and byte size based on schema declared type,
    Hungarian prefix, and distance (span) to the next contiguous field.
    """
    t = raw_type.strip() if raw_type else "void*"

    # Explicit pointer
    if t.endswith("*") or field_name.startswith("m_p") or (field_name.startswith("p") and len(field_name) > 2 and field_name[1].isupper()):
        if t.endswith("*"):
            return t, 8
        return "void*", 8

    # Array type: e.g. uint8_t[48], char[64]
    m_arr = re.match(r"^([A-Za-z0-9_]+)\[(\d+)\]$", t)
    if m_arr:
        base_elem = m_arr.group(1)
        count = int(m_arr.group(2))
        elem_size = PRIMITIVE_SIZES.get(base_elem, 1)
        total_arr_size = count * elem_size
        return f"{base_elem}[{count}]", total_arr_size

    # Known primitive / Source 2 struct
    if t in PRIMITIVE_SIZES:
        return t, PRIMITIVE_SIZES[t]

    # Hungarian deduction
    if field_name.startswith("m_b") or (field_name.startswith("b") and len(field_name) > 2 and field_name[1].isupper()):
        return "bool", 1
    if field_name.startswith("m_fl") or (field_name.startswith("fl") and len(field_name) > 3 and field_name[2].isupper()):
        return "float", 4
    if field_name.startswith("m_h") or (field_name.startswith("h") and len(field_name) > 2 and field_name[1].isupper()):
        return "CEntityHandle", 4
    if field_name.startswith("m_vec"):
        if span == 8:
            return "Vector2D", 8
        if span == 16:
            return "Vector4D", 16
        return "Vector", 12
    if field_name.startswith("m_ang"):
        return "QAngle", 12
    if field_name.startswith("m_quat") or field_name.startswith("m_q"):
        return "Quaternion", 16
    if field_name.startswith("m_clr") or field_name.startswith("m_Color"):
        return "Color", 4
    if field_name.startswith("m_sz") or field_name.startswith("m_str") or field_name.startswith("m_psz"):
        if span == 8:
            return "char*", 8
        if span > 0:
            return f"char[{span}]", span
        return "char*", 8
    if field_name.startswith("m_i") or field_name.startswith("m_n"):
        if span == 1:
            return "int8_t", 1
        if span == 2:
            return "int16_t", 2
        if span == 8:
            return "int64_t", 8
        return "int32_t", 4
    if field_name.startswith("m_un") or field_name.startswith("m_u"):
        if span == 1:
            return "uint8_t", 1
        if span == 2:
            return "uint16_t", 2
        if span == 8:
            return "uint64_t", 8
        return "uint32_t", 4

    # Span-based fallback
    if span == 1:
        return "uint8_t", 1
    if span == 2:
        return "uint16_t", 2
    if span == 4:
        return "uint32_t", 4
    if span == 8:
        return "uint64_t", 8
    if span == 12:
        return "Vector", 12
    if span == 16:
        return "Vector4D", 16
    if span > 0:
        return f"uint8_t[{span}]", span

    return "uint32_t", 4


def sanitize_identifier(name: str) -> str:
    """Sanitizes names into valid C identifiers."""
    safe = re.sub(r"[^A-Za-z0-9_]", "_", name)
    if safe and safe[0].isdigit():
        safe = "_" + safe
    return safe


def build_class_memory_layout(class_name: str, all_classes: Dict[str, dict]) -> Optional[dict]:
    """
    Computes exact struct memory layout, resolving base inheritance,
    ordering fields by byte offset, and computing gaps and explicit padding bytes.
    """
    cls_info = all_classes.get(class_name)
    if not cls_info:
        return None

    total_size = cls_info.get("size", 0)
    binary_name = cls_info.get("binary", "unknown.dll")
    base_name = cls_info.get("base", "")

    # Collect fields across inheritance chain
    chain = resolve_inheritance_chain(class_name, all_classes)
    fields_by_offset: Dict[int, dict] = {}

    for ancestor in chain:
        anc_info = all_classes.get(ancestor, {})
        for f in anc_info.get("fields", []):
            off = f.get("offset", 0)
            fields_by_offset[off] = f

    sorted_offsets = sorted(fields_by_offset.keys())

    # Build sequence of components (fields and explicit padding bytes)
    components: List[dict] = []
    curr_offset = 0

    for i, off in enumerate(sorted_offsets):
        f = fields_by_offset[off]
        f_name = sanitize_identifier(f.get("name", f"field_0x{off:x}"))

        # Gap between current offset and field offset: insert explicit padding
        if off > curr_offset:
            pad_size = off - curr_offset
            pad_name = f"pad_{curr_offset:04x}"
            components.append({
                "kind": "padding",
                "name": pad_name,
                "type": f"char[{pad_size}]",
                "offset": curr_offset,
                "size": pad_size,
                "comment": f"Padding: 0x{pad_size:x} ({pad_size}) bytes",
            })
            curr_offset = off
        elif off < curr_offset:
            # Overlapping field: skip or clamp
            continue

        # Compute span to next field or total size
        if i + 1 < len(sorted_offsets):
            next_off = sorted_offsets[i + 1]
            span = max(1, next_off - off)
        else:
            span = max(1, total_size - off) if total_size > off else 8

        raw_type = f.get("type", "")
        deduced_type, deduced_size = deduce_field_type_and_size(raw_type, f_name, span)
        effective_size = min(deduced_size, span)

        components.append({
            "kind": "field",
            "name": f_name,
            "type": deduced_type,
            "offset": off,
            "size": effective_size,
            "comment": f"+0x{off:04x} ({f_name})",
        })
        curr_offset = off + effective_size

    # Trailing padding up to total_size
    if total_size > curr_offset:
        tail_pad_size = total_size - curr_offset
        tail_pad_name = f"pad_{curr_offset:04x}"
        components.append({
            "kind": "padding",
            "name": tail_pad_name,
            "type": f"char[{tail_pad_size}]",
            "offset": curr_offset,
            "size": tail_pad_size,
            "comment": f"Trailing alignment padding: 0x{tail_pad_size:x} ({tail_pad_size}) bytes",
        })
        curr_offset = total_size

    final_size = max(total_size, curr_offset)

    return {
        "class_name": class_name,
        "binary": binary_name,
        "base": base_name,
        "total_size": final_size,
        "components": components,
        "fields_count": len([c for c in components if c["kind"] == "field"]),
        "padding_count": len([c for c in components if c["kind"] == "padding"]),
    }


def generate_c_header(layouts: List[dict], out_path: Path) -> str:
    """
    Generates a valid, complete C header file (source2_generated_classes.h)
    containing foundational primitives, typedefs, forward declarations,
    and fully padded struct definitions.
    """
    lines: List[str] = [
        "/* ========================================================================= */",
        "/* Auto-Generated Source 2 Class Definitions for Ghidra / C Compilers       */",
        "/* Generated by tools/ghidra/batch_inject_schemas.py                        */",
        "/* Exact runtime byte offsets with explicit padding: char pad_XXXX[N];      */",
        "/* ========================================================================= */",
        "",
        "#ifndef SOURCE2_GENERATED_CLASSES_H",
        "#define SOURCE2_GENERATED_CLASSES_H",
        "",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "#include <stdbool.h>",
        "",
        "/* Foundational Source 2 Math & Primitives */",
        "typedef struct Vector2D { float x; float y; } Vector2D;",
        "typedef struct Vector { float x; float y; float z; } Vector;",
        "typedef Vector Vector3;",
        "typedef struct Vector4D { float x; float y; float z; float w; } Vector4D;",
        "typedef struct QAngle { float pitch; float yaw; float roll; } QAngle;",
        "typedef struct Quaternion { float x; float y; float z; float w; } Quaternion;",
        "typedef struct Matrix3x4_t { float m_flMatVal[3][4]; } Matrix3x4_t;",
        "typedef struct CTransform { Vector m_vecPosition; Quaternion m_rotOrientation; } CTransform;",
        "",
        "/* Source 2 Handles & Identifiers */",
        "typedef uint32_t CEntityIndex;",
        "typedef uint32_t CEntityHandle;",
        "typedef uint32_t CUtlStringToken;",
        "typedef uint32_t Color;",
        "",
        "/* Source 2 Container Primitives */",
        "typedef struct CUtlString { char* m_pString; } CUtlString;",
        "typedef struct CUtlSymbolLarge { char* m_pString; } CUtlSymbolLarge;",
        "typedef struct CUtlMemory { void* m_pMemory; int32_t m_nAllocationCount; int32_t m_nGrowSize; } CUtlMemory;",
        "typedef struct CUtlVector { CUtlMemory m_Memory; int32_t m_Size; char pad_0014[4]; } CUtlVector;",
        "typedef struct CNetworkOriginCellCoordQuantizedVector { uint16_t m_cellX; uint16_t m_cellY; uint16_t m_cellZ; uint16_t m_nOutsideWorld; Vector m_vecX; } CNetworkOriginCellCoordQuantizedVector;",
        "",
        "/* Forward Declarations */",
    ]

    for lay in layouts:
        c_name = sanitize_identifier(lay["class_name"])
        lines.append(f"typedef struct {c_name} {c_name};")

    lines.append("")
    lines.append("/* Class Definitions with Explicit Byte-Accurate Padding */")

    for lay in layouts:
        c_name = sanitize_identifier(lay["class_name"])
        b_name = lay.get("base", "None") or "None"
        bin_name = lay.get("binary", "unknown")
        sz = lay.get("total_size", 0)

        lines.append(f"/* Binary: {bin_name} | Base: {b_name} | Size: 0x{sz:x} ({sz} bytes) */")
        lines.append(f"struct {c_name} {{")

        for comp in lay.get("components", []):
            c_type = comp["type"]
            c_field = comp["name"]
            c_off = comp["offset"]
            c_sz = comp["size"]
            c_comment = comp.get("comment", "")

            if "[" in c_type:
                base_t, arr_part = c_type.split("[", 1)
                lines.append(f"    {base_t} {c_field}[{arr_part}; /* +0x{c_off:04x} ({c_sz} bytes) */")
            else:
                lines.append(f"    {c_type} {c_field}; /* +0x{c_off:04x} */")

        lines.append("};")
        lines.append("")

    lines.append("#endif /* SOURCE2_GENERATED_CLASSES_H */\n")
    header_content = "\n".join(lines)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(header_content)

    return header_content


def generate_java_injection_script(layouts: List[dict], category_prefix: str = "/Source2/Classes") -> str:
    """
    Generates a pure Java Ghidra script that registers the foundational types
    and parses/injects all structured classes with explicit padding into Ghidra's DataTypeManager.
    Uses Gson to deserialize the compact layout definitions.
    """
    compact_payload: List[dict] = []
    for lay in layouts:
        items = []
        for comp in lay.get("components", []):
            items.append({
                "n": comp["name"],
                "t": comp["type"],
                "o": comp["offset"],
                "s": comp["size"],
                "k": comp["kind"],
            })
        compact_payload.append({
            "name": lay["class_name"],
            "size": lay["total_size"],
            "binary": lay.get("binary", ""),
            "components": items,
        })

    payload_json = json.dumps(compact_payload)
    # Escape quotes for Java string literal
    escaped_json = payload_json.replace("\\", "\\\\").replace('"', '\\"')

    java_code = r'''
import ghidra.program.model.data.*;
import ghidra.program.model.data.DataTypeManager;
import java.util.*;
import com.google.gson.*;

DataTypeManager dtm = currentProgram.getDataTypeManager();
int txId = currentProgram.startTransaction("Batch Inject Source 2 Schema Classes");

int classesCreated = 0;
int fieldsInjected = 0;
int padsCreated = 0;

try {
    CategoryPath root = CategoryPath.ROOT;
    CategoryPath mathCat = new CategoryPath("/Source2/Math");
    CategoryPath handleCat = new CategoryPath("/Source2/Handles");
    CategoryPath classCat = new CategoryPath("''' + category_prefix + r'''");

    FloatDataType dtFloat = FloatDataType.dataType;
    IntegerDataType dtInt = IntegerDataType.dataType;
    UnsignedIntegerDataType dtUInt = UnsignedIntegerDataType.dataType;
    BooleanDataType dtBool = BooleanDataType.dataType;
    ByteDataType dtByte = ByteDataType.dataType;
    CharDataType dtChar = CharDataType.dataType;
    ShortDataType dtShort = ShortDataType.dataType;
    UnsignedShortDataType dtUShort = UnsignedShortDataType.dataType;
    LongLongDataType dtInt64 = LongLongDataType.dataType;
    UnsignedLongLongDataType dtUInt64 = UnsignedLongLongDataType.dataType;
    PointerDataType dtVoidPtr = new PointerDataType(VoidDataType.dataType);

    // Standard Typedefs
    dtm.addDataType(new TypedefDataType(root, "uint32_t", dtUInt), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "int32_t", dtInt), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "uint64_t", dtUInt64), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "int64_t", dtInt64), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "uint16_t", dtUShort), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "int16_t", dtShort), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "uint8_t", dtByte), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "int8_t", dtByte), DataTypeConflictHandler.REPLACE_HANDLER);

    // Foundational Primitives
    StructureDataType vec2 = new StructureDataType(root, "Vector2D", 0);
    vec2.add(dtFloat, 4, "x", ""); vec2.add(dtFloat, 4, "y", "");
    dtm.addDataType(vec2, DataTypeConflictHandler.REPLACE_HANDLER);

    StructureDataType vec3 = new StructureDataType(root, "Vector", 0);
    vec3.add(dtFloat, 4, "x", ""); vec3.add(dtFloat, 4, "y", ""); vec3.add(dtFloat, 4, "z", "");
    dtm.addDataType(vec3, DataTypeConflictHandler.REPLACE_HANDLER);

    StructureDataType vec4 = new StructureDataType(root, "Vector4D", 0);
    vec4.add(dtFloat, 4, "x", ""); vec4.add(dtFloat, 4, "y", ""); vec4.add(dtFloat, 4, "z", ""); vec4.add(dtFloat, 4, "w", "");
    dtm.addDataType(vec4, DataTypeConflictHandler.REPLACE_HANDLER);

    StructureDataType qangle = new StructureDataType(root, "QAngle", 0);
    qangle.add(dtFloat, 4, "pitch", ""); qangle.add(dtFloat, 4, "yaw", ""); qangle.add(dtFloat, 4, "roll", "");
    dtm.addDataType(qangle, DataTypeConflictHandler.REPLACE_HANDLER);

    StructureDataType quat = new StructureDataType(root, "Quaternion", 0);
    quat.add(dtFloat, 4, "x", ""); quat.add(dtFloat, 4, "y", ""); quat.add(dtFloat, 4, "z", ""); quat.add(dtFloat, 4, "w", "");
    dtm.addDataType(quat, DataTypeConflictHandler.REPLACE_HANDLER);

    StructureDataType mat3x4 = new StructureDataType(root, "Matrix3x4_t", 0);
    mat3x4.add(new ArrayDataType(dtFloat, 12, 4), 48, "m_flMatVal", "");
    dtm.addDataType(mat3x4, DataTypeConflictHandler.REPLACE_HANDLER);

    StructureDataType xform = new StructureDataType(root, "CTransform", 0);
    xform.add(vec3, 12, "m_vecPosition", ""); xform.add(quat, 16, "m_rotOrientation", "");
    dtm.addDataType(xform, DataTypeConflictHandler.REPLACE_HANDLER);

    dtm.addDataType(new TypedefDataType(root, "CEntityIndex", dtUInt), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "CEntityHandle", dtUInt), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "CUtlStringToken", dtUInt), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "Color", dtUInt), DataTypeConflictHandler.REPLACE_HANDLER);

    StructureDataType utlStr = new StructureDataType(root, "CUtlString", 0);
    utlStr.add(new PointerDataType(dtChar), 8, "m_pString", "");
    dtm.addDataType(utlStr, DataTypeConflictHandler.REPLACE_HANDLER);

    StructureDataType utlSym = new StructureDataType(root, "CUtlSymbolLarge", 0);
    utlSym.add(new PointerDataType(dtChar), 8, "m_pString", "");
    dtm.addDataType(utlSym, DataTypeConflictHandler.REPLACE_HANDLER);

    StructureDataType utlMem = new StructureDataType(root, "CUtlMemory", 0);
    utlMem.add(dtVoidPtr, 8, "m_pMemory", ""); utlMem.add(dtInt, 4, "m_nAllocationCount", ""); utlMem.add(dtInt, 4, "m_nGrowSize", "");
    dtm.addDataType(utlMem, DataTypeConflictHandler.REPLACE_HANDLER);

    StructureDataType utlVec = new StructureDataType(root, "CUtlVector", 0);
    utlVec.add(utlMem, 16, "m_Memory", ""); utlVec.add(dtInt, 4, "m_Size", ""); utlVec.add(new ArrayDataType(dtByte, 4, 1), 4, "pad_0014", "");
    dtm.addDataType(utlVec, DataTypeConflictHandler.REPLACE_HANDLER);

    // Parse JSON Payload
    String jsonPayload = "''' + escaped_json + r'''";
    Gson gson = new Gson();
    List<Map<String, Object>> classes = gson.fromJson(jsonPayload, List.class);

    // Pass 1: Forward-declare / pre-create all structures so pointers resolve
    Map<String, StructureDataType> createdStructs = new HashMap<>();
    for (Map<String, Object> c : classes) {
        String clsName = (String) c.get("name");
        int totalSize = ((Number) c.get("size")).intValue();
        if (totalSize <= 0) totalSize = 8;
        StructureDataType struct = new StructureDataType(classCat, clsName, totalSize, dtm);
        struct.setDescription("Source 2 Schema: " + clsName + " (size: 0x" + Integer.toHexString(totalSize) + ")");
        dtm.addDataType(struct, DataTypeConflictHandler.REPLACE_HANDLER);
        createdStructs.put(clsName, struct);
    }

    // Pass 2: Populate components (fields and explicit padding arrays)
    for (Map<String, Object> c : classes) {
        String clsName = (String) c.get("name");
        int totalSize = ((Number) c.get("size")).intValue();
        if (totalSize <= 0) totalSize = 8;

        StructureDataType struct = new StructureDataType(classCat, clsName, totalSize, dtm);
        struct.setDescription("Source 2 Schema: " + clsName + " (size: 0x" + Integer.toHexString(totalSize) + ")");

        List<Map<String, Object>> comps = (List<Map<String, Object>>) c.get("components");
        if (comps != null) {
            for (Map<String, Object> comp : comps) {
                String name = (String) comp.get("n");
                String typeStr = (String) comp.get("t");
                int off = ((Number) comp.get("o")).intValue();
                int sz = ((Number) comp.get("s")).intValue();
                String kind = (String) comp.get("k");

                if (off + sz > totalSize) {
                    sz = totalSize - off;
                }
                if (sz <= 0) continue;

                DataType dt = null;
                if ("padding".equals(kind)) {
                    dt = new ArrayDataType(dtChar, sz, 1);
                    padsCreated++;
                } else if (typeStr.endsWith("*")) {
                    String baseName = typeStr.substring(0, typeStr.length() - 1).trim();
                    DataType pointee = dtm.getDataType(classCat, baseName);
                    if (pointee == null) pointee = dtm.getDataType(root, baseName);
                    if (pointee == null) pointee = VoidDataType.dataType;
                    dt = new PointerDataType(pointee);
                    fieldsInjected++;
                } else if (typeStr.contains("[") && typeStr.endsWith("]")) {
                    int bOpen = typeStr.indexOf('[');
                    String elemName = typeStr.substring(0, bOpen).trim();
                    int count = Integer.parseInt(typeStr.substring(bOpen + 1, typeStr.length() - 1));
                    DataType elemDt = "char".equals(elemName) ? dtChar : dtByte;
                    dt = new ArrayDataType(elemDt, count, 1);
                    fieldsInjected++;
                } else if ("Vector".equals(typeStr) || "Vector3".equals(typeStr)) {
                    dt = vec3; fieldsInjected++;
                } else if ("Vector2D".equals(typeStr)) {
                    dt = vec2; fieldsInjected++;
                } else if ("Vector4D".equals(typeStr)) {
                    dt = vec4; fieldsInjected++;
                } else if ("QAngle".equals(typeStr)) {
                    dt = qangle; fieldsInjected++;
                } else if ("Quaternion".equals(typeStr)) {
                    dt = quat; fieldsInjected++;
                } else if ("Matrix3x4_t".equals(typeStr)) {
                    dt = mat3x4; fieldsInjected++;
                } else if ("CTransform".equals(typeStr)) {
                    dt = xform; fieldsInjected++;
                } else if ("CEntityHandle".equals(typeStr) || "CEntityIndex".equals(typeStr) || "CUtlStringToken".equals(typeStr) || "Color".equals(typeStr)) {
                    dt = dtUInt; fieldsInjected++;
                } else if ("CUtlString".equals(typeStr)) {
                    dt = utlStr; fieldsInjected++;
                } else if ("CUtlSymbolLarge".equals(typeStr)) {
                    dt = utlSym; fieldsInjected++;
                } else if ("CUtlVector".equals(typeStr)) {
                    dt = utlVec; fieldsInjected++;
                } else if ("bool".equals(typeStr)) {
                    dt = dtBool; fieldsInjected++;
                } else if ("float".equals(typeStr) || "float32".equals(typeStr)) {
                    dt = dtFloat; fieldsInjected++;
                } else if ("double".equals(typeStr) || "float64".equals(typeStr)) {
                    dt = DoubleDataType.dataType; fieldsInjected++;
                } else if ("int32_t".equals(typeStr) || "int".equals(typeStr)) {
                    dt = dtInt; fieldsInjected++;
                } else if ("uint32_t".equals(typeStr) || "uint".equals(typeStr)) {
                    dt = dtUInt; fieldsInjected++;
                } else if ("int64_t".equals(typeStr)) {
                    dt = dtInt64; fieldsInjected++;
                } else if ("uint64_t".equals(typeStr) || "uintptr_t".equals(typeStr)) {
                    dt = dtUInt64; fieldsInjected++;
                } else if ("int16_t".equals(typeStr)) {
                    dt = dtShort; fieldsInjected++;
                } else if ("uint16_t".equals(typeStr)) {
                    dt = dtUShort; fieldsInjected++;
                } else if ("int8_t".equals(typeStr)) {
                    dt = dtByte; fieldsInjected++;
                } else if ("uint8_t".equals(typeStr)) {
                    dt = dtByte; fieldsInjected++;
                } else {
                    // Fallback based on size
                    if (sz == 1) dt = dtByte;
                    else if (sz == 2) dt = dtUShort;
                    else if (sz == 4) dt = dtUInt;
                    else if (sz == 8) dt = dtVoidPtr;
                    else dt = new ArrayDataType(dtByte, sz, 1);
                    fieldsInjected++;
                }

                struct.replaceAtOffset(off, dt, sz, name, "");
            }
        }

        dtm.addDataType(struct, DataTypeConflictHandler.REPLACE_HANDLER);
        classesCreated++;
    }

    println("SUCCESS: Injected " + classesCreated + " schema classes (" + fieldsInjected + " fields, " + padsCreated + " pads) into " + classCat);
} catch (Exception e) {
    println("ERROR: " + e.getMessage());
    e.printStackTrace();
} finally {
    currentProgram.endTransaction(txId, true);
}
'''
    return java_code


def inject_schemas_into_binary(
    client: GhidraHttpClient,
    layouts: List[dict],
    binary_name: str,
    folder: str = DEFAULT_VERSION_FOLDER,
    batch_size: int = 1,
) -> bool:
    """Injects computed schema struct layouts into target binary in Ghidra."""
    full_path = f"{folder}/{binary_name}"
    print(f"[*] Ensuring program {full_path} is active in Ghidra...")
    open_res = client.open_program(full_path)
    if "error" in open_res:
        print(f"[-] Warning opening {binary_name}: {open_res['error']}")

    print(f"[*] Injecting {len(layouts)} schema classes into {binary_name}...")
    success_count = 0
    total_fields = 0
    total_pads = 0

    for i in range(0, len(layouts), batch_size):
        chunk = layouts[i : i + batch_size]
        chunk_names = [c["class_name"] for c in chunk]
        script = generate_java_injection_script(chunk, category_prefix="/Source2/Classes")
        run_res = client.run_inline_script(script, program=binary_name)
        output = run_res.get("result", "")

        if "SUCCESS" in output:
            for line in output.splitlines():
                if "SUCCESS:" in line:
                    # Parse counts
                    m = re.search(r"Injected (\d+) schema classes \((\d+) fields, (\d+) pads\)", line)
                    if m:
                        success_count += int(m.group(1))
                        total_fields += int(m.group(2))
                        total_pads += int(m.group(3))
                    print(f"  [+] {', '.join(chunk_names)} -> Injected successfully")
        else:
            print(f"  [-] Error injecting {chunk_names}: {output[:250]}")

    save_res = client.save_program(binary_name)
    print(f"[+] Saved {binary_name} in Ghidra database. Total classes: {success_count}/{len(layouts)} ({total_fields} fields, {total_pads} pads)")
    return success_count == len(layouts)


def run_batch_inject_schemas(
    target_classes: Optional[List[str]] = None,
    module_filter: Optional[str] = None,
    header_only: bool = False,
    out_header: str = "data/source2_generated_classes.h",
    port: int = DEFAULT_PORT,
    binary: str = "",
    all_binaries: bool = False,
    folder: str = DEFAULT_VERSION_FOLDER,
    all_classes_flag: bool = False,
) -> bool:
    """
    Main orchestration entry point:
    1. Loads extracted schemas.
    2. Builds exact memory layouts with padding.
    3. Generates C header file.
    4. Injects types into active Ghidra instance.
    """
    schemas_dir = REPO_ROOT / "data" / "extracted_schemas"
    print(f"[*] Loading Source 2 schemas from {schemas_dir.relative_to(REPO_ROOT).as_posix()}...")
    all_schemas = load_all_schemas(schemas_dir)
    print(f"[+] Total available classes loaded: {len(all_schemas)}")

    # Determine which classes to build
    if all_classes_flag:
        chosen_classes = sorted(all_schemas.keys())
    elif target_classes:
        chosen_classes = target_classes
    else:
        chosen_classes = CORE_HIGH_PRIORITY_CLASSES

    if module_filter:
        mod_clean = module_filter.lower()
        chosen_classes = [
            c for c in chosen_classes
            if mod_clean in all_schemas.get(c, {}).get("binary", "").lower()
        ]

    print(f"[*] Computing layouts with byte padding for {len(chosen_classes)} classes...")
    layouts: List[dict] = []
    for c_name in chosen_classes:
        lay = build_class_memory_layout(c_name, all_schemas)
        if lay:
            layouts.append(lay)
        else:
            print(f"[-] Warning: Could not resolve layout for class {c_name}")

    print(f"[+] Successfully computed {len(layouts)} struct layouts.")

    # Generate C Header
    out_header_path = REPO_ROOT / out_header
    print(f"[*] Generating C header at {out_header_path.relative_to(REPO_ROOT).as_posix()}...")
    generate_c_header(layouts, out_header_path)
    header_size = out_header_path.stat().st_size
    print(f"[+] Wrote {len(layouts)} structs to {out_header_path.relative_to(REPO_ROOT).as_posix()} ({header_size:,} bytes)")

    if header_only:
        print("[*] --header-only specified. Skipping Ghidra DataTypeManager injection.")
        return True

    # Connect to Ghidra
    client = GhidraHttpClient(port=port)
    conn_ok, conn_msg = client.check_connection()
    if not conn_ok:
        print(f"[-] Failed to connect to Ghidra on port {port}: {conn_msg}")
        print("[-] Header was generated, but Ghidra injection was skipped.")
        return False

    print(f"[+] Connected to Ghidra: {conn_msg}")

    # Determine target binary
    if all_binaries:
        from tools.ghidra.cs2_ghidra_toolkit import ALL_CORE_BINARIES
        target_bins = ALL_CORE_BINARIES
    elif binary:
        target_bins = [binary]
    else:
        # Default: check current active program or default to server.dll
        cur_info = client.post_json("/get_current_program_info", {})
        active_prog = cur_info.get("name", "server.dll")
        target_bins = [active_prog]

    success = True
    for b in target_bins:
        if not inject_schemas_into_binary(client, layouts, b, folder=folder):
            success = False

    print("\n=======================================================")
    print(f"  Batch Schema Injection Complete: {len(layouts)} classes registered")
    print(f"  Target Header: {out_header_path.relative_to(REPO_ROOT).as_posix()}")
    print("=======================================================\n")
    return success


def main():
    parser = argparse.ArgumentParser(description="Source 2 Schema Class Batch Injector for Ghidra")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="GhidraMCP HTTP port (default: 8089)")
    parser.add_argument("--folder", type=str, default=DEFAULT_VERSION_FOLDER, help="Project version folder")
    parser.add_argument("--binary", type=str, default="", help="Specific binary to inject into (default: active program)")
    parser.add_argument("--all-binaries", action="store_true", help="Inject into all core CS2 binaries")
    parser.add_argument("--classes", type=str, default="", help="Comma-separated class names to inject")
    parser.add_argument("--all", action="store_true", help="Inject ALL extracted schema classes (2,000+)")
    parser.add_argument("--module", type=str, default="", help="Filter classes by module name (e.g. client.dll, server.dll)")
    parser.add_argument("--header-only", action="store_true", help="Generate C header without contacting Ghidra")
    parser.add_argument("--out-header", type=str, default="data/source2_generated_classes.h", help="Output header path")

    args = parser.parse_args()

    custom_classes = [c.strip() for c in args.classes.split(",") if c.strip()] if args.classes else None

    run_batch_inject_schemas(
        target_classes=custom_classes,
        module_filter=args.module,
        header_only=args.header_only,
        out_header=args.out_header,
        port=args.port,
        binary=args.binary,
        all_binaries=args.all_binaries,
        folder=args.folder,
        all_classes_flag=args.all,
    )


if __name__ == "__main__":
    main()
