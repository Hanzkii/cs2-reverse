#!/usr/bin/env python3
"""
Source 2 SchemaSystem Class & Field Extractor
Extracts declared SchemaSystem classes (CSchemaClassInfo), field names, types,
and byte offsets directly from offline PE binaries into structured JSON schemas.
Supports differential schema comparison across game updates.

Architecture Reference:
- ISchemaSystem (schemasystem.dll): SchemaSystem_001
- SchemaClassInfoData_t:
    +0x00: CSchemaClassInfo* m_pSelf
    +0x08: const char* m_pszName (e.g. "C_CSPlayerPawn")
    +0x10: const char* m_pszModule (e.g. "client.dll")
    +0x18/0x20: int32_t m_nSize
    +0x24: int16_t m_nFieldCount
    +0x30: SchemaClassFieldData_t* m_pFields
    +0x38: SchemaBaseClassInfoData_t* m_pBaseClasses
- SchemaClassFieldData_t:
    +0x00: const char* m_pszName (e.g. "m_iHealth")
    +0x08: SchemaType_t* m_pType (CSchemaType pointer or 0xf0000000000000XX atomic tag)
    +0x10: int32_t m_nSingleInheritanceOffset
"""

import os
import sys
import struct
import re
import json
import argparse
import time
from typing import Dict, List, Any, Optional, Tuple

# Mapping of Source 2 atomic/engine type tags (high byte 0xf0) discovered
# from schemasystem.dll's internal reflection registry.
KNOWN_ATOMIC_TYPES: Dict[int, str] = {
    0x01: "bool",
    0x02: "char*",
    0x04: "CUtlVector",
    0x06: "int8",
    0x07: "uint8",
    0x08: "int16",
    0x09: "uint16",
    0x0a: "int32",
    0x0b: "uint32",
    0x0c: "int64",
    0x0d: "uint64",
    0x0e: "float32",
    0x0f: "float64",
    0x10: "bool",
    0x12: "Vector",
    0x14: "Vector2D",
    0x15: "Vector4D",
    0x16: "QAngle",
    0x17: "Quaternion",
    0x1c: "matrix3x4_t",
    0x1d: "CTransform",
    0x1e: "CTransform",
    0x20: "Color",
    0x21: "CUtlString",
    0x22: "CUtlBinaryBlock",
    0x23: "CUtlSymbolLarge",
    0x24: "CUtlSymbol",
    0x26: "CUtlStringToken",
    0x27: "FourQuaternions",
    0x28: "CEntityIndex",
    0x2a: "CSplitScreenSlot",
    0x2b: "CPlayerSlot",
    0x2c: "HSCRIPT",
    0x30: "AttachmentHandle_t",
    0x31: "ParticleIndex_t",
    0x32: "CountdownTimer",
    0x34: "EngineLoopState_t",
    0x35: "CHandle<C_BaseEntity>",
    0x36: "CHandle<C_BaseEntity>",
    0x37: "CEntityOutputTemplate",
    0x45: "KeyValues3",
    0x4a: "CPathIndex",
    0x4c: "CNetworkOriginCellCoordQuantizedVector",
    0x4d: "HSequence",
    0x55: "CStrongHandle<InfoForResourceTypeIMaterial2>",
    0x56: "CStrongHandle<InfoForResourceTypeCModel>",
    0x57: "CStrongHandle<InfoForResourceTypeIParticleSnapshot>",
    0x58: "CStrongHandle<InfoForResourceTypeIParticleSystemDefinition>",
    0x59: "CStrongHandle<InfoForResourceTypeITexture>",
    0x72: "WorldGroupId_t",
    0x74: "CGlobalSymbol",
    0x75: "PulseValueType_t",
}

def deduce_hungarian_type(name: str) -> str:
    """Deduces Source 2 type based on Hungarian prefix convention."""
    if name.startswith("m_p") or (name.startswith("p") and len(name) > 2 and name[1].isupper()):
        return "void*"
    if name.startswith("m_b") or (name.startswith("b") and len(name) > 2 and name[1].isupper()):
        return "bool"
    if name.startswith("m_fl") or (name.startswith("fl") and len(name) > 3 and name[2].isupper()):
        return "float32"
    if name.startswith("m_h") or (name.startswith("h") and len(name) > 2 and name[1].isupper()):
        return "CEntityHandle"
    if name.startswith("m_vec"):
        return "Vector"
    if name.startswith("m_ang"):
        return "QAngle"
    if name.startswith("m_quat") or name.startswith("m_q"):
        return "Quaternion"
    if name.startswith("m_clr") or name.startswith("m_Color"):
        return "Color"
    if name.startswith("m_i") or name.startswith("m_n"):
        return "int32"
    if name.startswith("m_un") or name.startswith("m_u"):
        return "uint32"
    if name.startswith("m_sz") or name.startswith("m_str") or name.startswith("m_psz"):
        return "char*"
    return "void*"


class PEBinary:
    """Offline x86-64 Portable Executable parser for Source 2 modules."""

    def __init__(self, filepath: str):
        self.filepath = filepath
        self.filename = os.path.basename(filepath)
        with open(filepath, "rb") as f:
            self.data = f.read()
        self.image_base: int = 0x180000000
        self.sections: List[Dict[str, Any]] = []
        self._parse_pe()

    def _parse_pe(self) -> None:
        if len(self.data) < 0x40:
            return
        # e_lfanew
        pe_sig = struct.unpack_from("<I", self.data, 0x3C)[0]
        if pe_sig + 24 > len(self.data):
            return
        num_sec = struct.unpack_from("<H", self.data, pe_sig + 6)[0]
        opt_size = struct.unpack_from("<H", self.data, pe_sig + 20)[0]
        opt_header_off = pe_sig + 24

        if opt_size >= 28:
            magic = struct.unpack_from("<H", self.data, opt_header_off)[0]
            if magic == 0x20B and opt_size >= 32:  # PE32+ (64-bit)
                self.image_base = struct.unpack_from("<Q", self.data, opt_header_off + 24)[0]

        sec_off = pe_sig + 24 + opt_size
        for i in range(num_sec):
            s_entry = self.data[sec_off + i*40 : sec_off + (i+1)*40]
            name = s_entry[:8].decode("latin-1", errors="ignore").strip("\x00")
            vsize, rva, raw_size, raw_ptr = struct.unpack_from("<IIII", s_entry, 8)
            self.sections.append({
                "name": name,
                "rva": rva,
                "vsize": vsize,
                "roff": raw_ptr,
                "rsize": raw_size
            })

    def va_to_off(self, va: int) -> Optional[int]:
        """Translates a 64-bit Virtual Address into raw file offset."""
        rva = va - self.image_base
        for s in self.sections:
            if s["rva"] <= rva < s["rva"] + s["vsize"]:
                off = s["roff"] + (rva - s["rva"])
                if off < len(self.data):
                    return off
        return None

    def read_str(self, va: int, max_len: int = 256) -> str:
        """Reads a null-terminated ASCII string at the specified VA."""
        off = self.va_to_off(va)
        if off is None or off >= len(self.data):
            return ""
        end = self.data.find(b"\x00", off)
        if end == -1 or end - off > max_len:
            return ""
        return self.data[off:end].decode("latin-1", errors="ignore")

    def extract_schemas(self, class_filter: Optional[str] = None) -> Dict[str, Dict[str, Any]]:
        """
        Scans data sections for compiled SchemaClassInfoData_t descriptors
        and reconstructs class name, base class, field names, types, and offsets.
        """
        ident_re = re.compile(r"^[A-Za-z_][A-Za-z0-9_:]*$")
        classes: Dict[str, Dict[str, Any]] = {}

        for s in self.sections:
            if s["name"] not in [".data", ".rdata"]:
                continue
            sec_data = self.data[s["roff"] : s["roff"] + s["rsize"]]
            for i in range(0, len(sec_data) - 0x40, 8):
                name_va = struct.unpack_from("<Q", sec_data, i + 8)[0]
                size = struct.unpack_from("<I", sec_data, i + 0x20)[0]
                num_fields = struct.unpack_from("<H", sec_data, i + 0x24)[0]
                fields_va = struct.unpack_from("<Q", sec_data, i + 0x30)[0]
                base_va = struct.unpack_from("<Q", sec_data, i + 0x38)[0]

                if 0 < size < 3000000 and 0 <= num_fields < 2000:
                    cname = self.read_str(name_va)
                    if not (cname and len(cname) > 2 and ident_re.match(cname)):
                        continue

                    if class_filter and cname.lower() != class_filter.lower():
                        continue

                    fields_list: List[Dict[str, Any]] = []
                    if num_fields > 0:
                        f_off = self.va_to_off(fields_va)
                        if not f_off or f_off + num_fields * 0x20 > len(self.data):
                            continue
                        fn1_va = struct.unpack_from("<Q", self.data, f_off)[0]
                        fn1 = self.read_str(fn1_va)
                        if not (fn1 and ident_re.match(fn1)):
                            continue

                        for fi in range(num_fields):
                            chunk = self.data[f_off + fi*0x20 : f_off + (fi+1)*0x20]
                            fn_va = struct.unpack_from("<Q", chunk, 0)[0]
                            ft_va = struct.unpack_from("<Q", chunk, 8)[0]
                            f_offset = struct.unpack_from("<I", chunk, 0x10)[0]
                            fname = self.read_str(fn_va)

                            ftype = ""
                            # Check high byte for 0xf0 atomic tag
                            if ft_va >> 56 == 0xf0:
                                tag = ft_va & 0xff
                                ftype = KNOWN_ATOMIC_TYPES.get(tag, "")
                            elif ft_va != 0:
                                t_off = self.va_to_off(ft_va)
                                if t_off:
                                    tname_va = struct.unpack_from("<Q", self.data, t_off + 8)[0]
                                    ftype = self.read_str(tname_va)

                            if not ftype:
                                ftype = deduce_hungarian_type(fname)

                            fields_list.append({
                                "name": fname,
                                "type": ftype or "void*",
                                "offset": f_offset,
                                "offset_hex": f"+0x{f_offset:04x}"
                            })

                    # Base class resolution
                    base_name = ""
                    if base_va != 0:
                        b_off = self.va_to_off(base_va)
                        if b_off and b_off + 16 <= len(self.data):
                            binfo_va = struct.unpack_from("<Q", self.data, b_off + 8)[0]
                            if binfo_va:
                                bi_off = self.va_to_off(binfo_va)
                                if bi_off and bi_off + 16 <= len(self.data):
                                    bname_ptr = struct.unpack_from("<Q", self.data, bi_off + 8)[0]
                                    base_name = self.read_str(bname_ptr)

                    classes[cname] = {
                        "class": cname,
                        "binary": self.filename,
                        "size": size,
                        "size_hex": f"0x{size:x}",
                        "base": base_name,
                        "fields_count": num_fields,
                        "fields": fields_list
                    }

        return classes


def find_latest_archive(root_dir: str = ".") -> Tuple[str, str]:
    """Finds the most recent build directory in versions/archive/."""
    archive_base = os.path.join(root_dir, "versions", "archive")
    if not os.path.isdir(archive_base):
        raise FileNotFoundError(f"Archive directory not found: {archive_base}")
    builds = sorted([d for d in os.listdir(archive_base) if os.path.isdir(os.path.join(archive_base, d))])
    if not builds:
        raise FileNotFoundError(f"No archived builds found in: {archive_base}")
    latest = builds[-1]
    return os.path.join(archive_base, latest), latest


def extract_from_directory(dir_path: str, module_filter: Optional[str] = None,
                           class_filter: Optional[str] = None,
                           scoped: bool = False) -> Dict[str, Any]:
    """Extracts schema classes across all PE binaries in a directory."""
    if not os.path.isdir(dir_path):
        raise FileNotFoundError(f"Directory not found: {dir_path}")

    files = sorted(os.listdir(dir_path))
    results: Dict[str, Any] = {}

    for fname in files:
        if not (fname.lower().endswith(".dll") or fname.lower().endswith(".exe")):
            continue
        if module_filter and fname.lower() != module_filter.lower():
            continue

        fpath = os.path.join(dir_path, fname)
        pe = PEBinary(fpath)
        schemas = pe.extract_schemas(class_filter=class_filter)
        if schemas:
            if scoped:
                results[fname] = schemas
            else:
                for cname, cdata in schemas.items():
                    results[cname] = cdata

    return results


def diff_schemas(old_schemas: Dict[str, Any], new_schemas: Dict[str, Any]) -> Dict[str, Any]:
    """
    Compares two schema sets and produces a detailed differential report
    including added classes, removed classes, modified class sizes, and field offset shifts.
    """
    added_classes = []
    removed_classes = []
    modified_classes = []
    total_shifted_fields = 0

    for cname, new_cls in new_schemas.items():
        if cname not in old_schemas:
            added_classes.append(new_cls)
        else:
            old_cls = old_schemas[cname]
            old_fields = {f["name"]: f for f in old_cls.get("fields", [])}
            new_fields = {f["name"]: f for f in new_cls.get("fields", [])}

            shifted_fields = []
            added_fields = []
            removed_fields = []

            for fname, nf in new_fields.items():
                if fname not in old_fields:
                    added_fields.append(nf)
                else:
                    of = old_fields[fname]
                    if of["offset"] != nf["offset"]:
                        delta = nf["offset"] - of["offset"]
                        delta_str = f"+0x{delta:x}" if delta > 0 else f"-0x{-delta:x}"
                        shifted_fields.append({
                            "name": fname,
                            "type": nf.get("type", "void*"),
                            "old_offset": of["offset"],
                            "old_offset_hex": of.get("offset_hex", f"+0x{of['offset']:04x}"),
                            "new_offset": nf["offset"],
                            "new_offset_hex": nf.get("offset_hex", f"+0x{nf['offset']:04x}"),
                            "delta_bytes": delta,
                            "delta_hex": delta_str
                        })

            for fname, of in old_fields.items():
                if fname not in new_fields:
                    removed_fields.append(of)

            size_delta = new_cls["size"] - old_cls["size"]
            if shifted_fields or added_fields or removed_fields or size_delta != 0:
                total_shifted_fields += len(shifted_fields)
                modified_classes.append({
                    "class": cname,
                    "binary": new_cls.get("binary", old_cls.get("binary", "")),
                    "old_size": old_cls["size"],
                    "new_size": new_cls["size"],
                    "size_delta": size_delta,
                    "shifted_fields_count": len(shifted_fields),
                    "shifted_fields": shifted_fields,
                    "added_fields": added_fields,
                    "removed_fields": removed_fields
                })

    for cname, old_cls in old_schemas.items():
        if cname not in new_schemas:
            removed_classes.append(old_cls)

    return {
        "classes_added_count": len(added_classes),
        "classes_removed_count": len(removed_classes),
        "classes_modified_count": len(modified_classes),
        "fields_shifted_total": total_shifted_fields,
        "added_classes": added_classes,
        "removed_classes": removed_classes,
        "modified_classes": modified_classes
    }


def resolve_schema_input(target: str, root_dir: str = ".") -> Tuple[Dict[str, Any], str]:
    """Resolves a target parameter into a dictionary of schemas."""
    # 1. Direct JSON file
    if os.path.isfile(target) and target.endswith(".json"):
        with open(target, "r", encoding="utf-8") as f:
            return json.load(f), os.path.basename(target)

    # 2. Direct PE binary
    if os.path.isfile(target) and (target.endswith(".dll") or target.endswith(".exe")):
        pe = PEBinary(target)
        return pe.extract_schemas(), os.path.basename(target)

    # 3. Direct directory of binaries
    if os.path.isdir(target):
        schemas = extract_from_directory(target)
        return schemas, os.path.basename(os.path.normpath(target))

    # 4. Profile name in versions/profiles/<target>/schemas.json
    profile_path = os.path.join(root_dir, "versions", "profiles", target, "schemas.json")
    if os.path.isfile(profile_path):
        with open(profile_path, "r", encoding="utf-8") as f:
            return json.load(f), target

    # 5. Archive name in versions/archive/<target>
    archive_path = os.path.join(root_dir, "versions", "archive", target)
    if os.path.isdir(archive_path):
        schemas = extract_from_directory(archive_path)
        return schemas, target

    raise ValueError(f"Could not resolve schema target: {target}")


def format_diff_markdown(diff: Dict[str, Any], old_tag: str, new_tag: str) -> str:
    """Formats schema differential results as a markdown report."""
    lines = []
    lines.append(f"# SchemaSystem Differential Report: `{old_tag}` -> `{new_tag}`\n")
    lines.append(f"- **Classes Added:** {diff['classes_added_count']}")
    lines.append(f"- **Classes Removed:** {diff['classes_removed_count']}")
    lines.append(f"- **Classes Modified:** {diff['classes_modified_count']}")
    lines.append(f"- **Total Member Offset Shifts:** {diff['fields_shifted_total']}\n")

    if diff["added_classes"]:
        lines.append("## 1. Newly Added Classes\n")
        lines.append("| Class Name | Module | Instance Size | Member Fields |")
        lines.append("| :--- | :--- | :--- | :--- |")
        for c in diff["added_classes"]:
            lines.append(f"| `{c['class']}` | `{c.get('binary', '')}` | `{c.get('size_hex', hex(c['size']))}` | {c.get('fields_count', len(c.get('fields', [])))} |")
        lines.append("")

    if diff["removed_classes"]:
        lines.append("## 2. Removed Classes\n")
        lines.append("| Class Name | Module | Previous Size |")
        lines.append("| :--- | :--- | :--- |")
        for c in diff["removed_classes"]:
            lines.append(f"| `{c['class']}` | `{c.get('binary', '')}` | `{c.get('size_hex', hex(c['size']))}` |")
        lines.append("")

    if diff["modified_classes"]:
        lines.append("## 3. Shifted Member Offsets & Class Layout Mutations\n")
        for mc in diff["modified_classes"]:
            lines.append(f"### `{mc['class']}` (`{mc['binary']}`)")
            lines.append(f"- **Instance Size:** {mc['old_size']} -> {mc['new_size']} bytes (Δ {mc['size_delta']:+d} bytes)")
            lines.append(f"- **Shifted Fields:** {mc['shifted_fields_count']}\n")

            if mc["shifted_fields"]:
                lines.append("| Member Field | Type | Previous Offset | Current Offset | Shift Delta |")
                lines.append("| :--- | :--- | :--- | :--- | :--- |")
                for sf in mc["shifted_fields"]:
                    lines.append(f"| `{sf['name']}` | `{sf['type']}` | `{sf['old_offset_hex']}` | `{sf['new_offset_hex']}` | `{sf['delta_hex']}` |")
                lines.append("")

            if mc["added_fields"]:
                lines.append(f"**Added Fields:** {', '.join([f['name'] for f in mc['added_fields']])}\n")
            if mc["removed_fields"]:
                lines.append(f"**Removed Fields:** {', '.join([f['name'] for f in mc['removed_fields']])}\n")

    return "\n".join(lines)


def print_summary(schemas: Dict[str, Any], scoped: bool = False) -> None:
    """Prints diagnostic statistics for extracted schemas."""
    flat_schemas = {}
    if scoped:
        for mod, mod_classes in schemas.items():
            for cname, cdata in mod_classes.items():
                flat_schemas[f"{mod}::{cname}"] = cdata
    else:
        flat_schemas = schemas

    total_classes = len(flat_schemas)
    total_fields = sum(len(c.get("fields", [])) for c in flat_schemas.values())
    type_counts: Dict[str, int] = {}
    module_counts: Dict[str, int] = {}

    for c in flat_schemas.values():
        mod = c.get("binary", "unknown")
        module_counts[mod] = module_counts.get(mod, 0) + 1
        for f in c.get("fields", []):
            t = f.get("type", "void*")
            type_counts[t] = type_counts.get(t, 0) + 1

    print(f"\n{'='*70}")
    print(f" SchemaSystem Static Extraction Summary")
    print(f"{'='*70}")
    print(f"  Total Declared Classes: {total_classes:,}")
    print(f"  Total Member Fields:    {total_fields:,}")
    print(f"  Target Modules Count:   {len(module_counts):,}")

    print(f"\n  Top Classes by Module:")
    for mod, count in sorted(module_counts.items(), key=lambda x: x[1], reverse=True)[:10]:
        print(f"    - {mod:25s}: {count:5d} classes")

    print(f"\n  Top Resolved Field Types:")
    for t, count in sorted(type_counts.items(), key=lambda x: x[1], reverse=True)[:15]:
        print(f"    - {t:35s}: {count:5d} fields")
    print(f"{'='*70}\n")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Source 2 SchemaSystem Class & Field Extractor (Offline PE Reflection Parser)"
    )
    parser.add_argument("--archive", "-a", type=str, default="",
                        help="Path to archive directory or build tag (default: latest in versions/archive/)")
    parser.add_argument("--binary", "-b", type=str, default="",
                        help="Path to individual PE binary to analyze (e.g. client.dll)")
    parser.add_argument("--output", "-o", type=str, default="",
                        help="Output path for JSON schema profile (e.g. versions/profiles/<BUILD_ID>/schemas.json)")
    parser.add_argument("--module", "-m", type=str, default="",
                        help="Filter classes to a specific module name (e.g. client.dll, server.dll)")
    parser.add_argument("--class", "-c", dest="target_class", type=str, default="",
                        help="Filter extraction to a single target class (e.g. C_CSPlayerPawn)")
    parser.add_argument("--scoped", action="store_true",
                        help="Group classes by module name to prevent cross-module key collisions")
    parser.add_argument("--diff", nargs=2, metavar=("BUILD_A", "BUILD_B"),
                        help="Compare schemas between two builds (accepts build IDs, profile dirs, or json files)")
    parser.add_argument("--diff-output", type=str, default="",
                        help="Output path for differential markdown report")
    parser.add_argument("--summary", "-s", action="store_true",
                        help="Print statistical type and module summary")

    args = parser.parse_args()

    # Mode 1: Differential Comparison
    if args.diff:
        target_a, target_b = args.diff[0], args.diff[1]
        print(f"[*] Diffing SchemaSystem profiles: '{target_a}' vs '{target_b}'...")
        sch_a, tag_a = resolve_schema_input(target_a)
        sch_b, tag_b = resolve_schema_input(target_b)

        diff = diff_schemas(sch_a, sch_b)
        print(f"\n[+] Differential Analysis Results:")
        print(f"    - Classes Added:    {diff['classes_added_count']}")
        print(f"    - Classes Removed:  {diff['classes_removed_count']}")
        print(f"    - Classes Modified: {diff['classes_modified_count']}")
        print(f"    - Offset Shifts:    {diff['fields_shifted_total']} fields across {len(diff['modified_classes'])} classes")

        if diff["added_classes"]:
            print(f"\n[+] Newly Added Classes ({diff['classes_added_count']}):")
            for c in diff["added_classes"][:10]:
                print(f"    + {c['class']:30s} [{c.get('binary', '')}] (size: {c.get('size_hex', hex(c['size']))})")

        if diff["modified_classes"]:
            print(f"\n[+] Sample Modified Classes & Offset Shifts:")
            for mc in diff["modified_classes"][:5]:
                print(f"    * {mc['class']} ({mc['binary']}): Δ size = {mc['size_delta']:+d} bytes, shifted = {mc['shifted_fields_count']}")
                for sf in mc["shifted_fields"][:3]:
                    print(f"        {sf['name']}: {sf['old_offset_hex']} -> {sf['new_offset_hex']} ({sf['delta_hex']})")

        if args.diff_output:
            md_content = format_diff_markdown(diff, tag_a, tag_b)
            with open(args.diff_output, "w", encoding="utf-8") as f:
                f.write(md_content)
            print(f"\n[+] Differential report written to: {args.diff_output}")

        return

    # Mode 2: Extraction
    t0 = time.time()
    schemas: Dict[str, Any] = {}
    build_tag = ""

    if args.binary:
        if not os.path.isfile(args.binary):
            print(f"[-] Binary file not found: {args.binary}", file=sys.stderr)
            sys.exit(1)
        print(f"[*] Analyzing single binary: {args.binary}")
        pe = PEBinary(args.binary)
        schemas = pe.extract_schemas(class_filter=args.target_class)
        build_tag = os.path.basename(args.binary)
    else:
        archive_path = args.archive
        if not archive_path:
            archive_path, build_tag = find_latest_archive()
            print(f"[*] Auto-detected latest archive: {archive_path} (tag: {build_tag})")
        else:
            if not os.path.isdir(archive_path):
                # Try relative to versions/archive/
                candidate = os.path.join("versions", "archive", archive_path)
                if os.path.isdir(candidate):
                    archive_path = candidate
            build_tag = os.path.basename(os.path.normpath(archive_path))
            print(f"[*] Analyzing archive directory: {archive_path}")

        schemas = extract_from_directory(
            archive_path,
            module_filter=args.module,
            class_filter=args.target_class,
            scoped=args.scoped
        )

    elapsed = time.time() - t0
    class_count = sum(len(v) for v in schemas.values()) if args.scoped else len(schemas)
    print(f"[+] Extraction complete in {elapsed:.2f}s: {class_count:,} classes extracted.")

    if args.summary or not args.output:
        print_summary(schemas, scoped=args.scoped)

    # Output generation
    if args.output:
        out_path = args.output
        os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(schemas, f, indent=2)
        print(f"[+] Output written to: {out_path} ({os.path.getsize(out_path):,} bytes)")


if __name__ == "__main__":
    main()
