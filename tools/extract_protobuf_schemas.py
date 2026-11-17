#!/usr/bin/env python3
"""
Source 2 Compiled Google Protobuf Schema Extractor
Scans Source 2 binaries (.rdata sections) for compiled Google Protobuf
DescriptorTable and FileDescriptorProto/FileDescriptorSet structures,
decodes the wire descriptor headers, and reconstructs canonical .proto
schema definitions into data/protobuf/.

Strict Invariants Enforced:
- Relative pathing only (no hardcoded absolute user/drive paths)
- Pure Python wire-format decoder immune to protobuf version mismatches
- Read-only analysis of extracted metadata (zero binary modifications)
"""

import sys
import os
import glob
import struct
import re
import json
import argparse
from typing import Dict, List, Any, Optional, Tuple, Set


# Protobuf wire type constants
WIRETYPE_VARINT = 0
WIRETYPE_FIXED64 = 1
WIRETYPE_LENGTH_DELIMITED = 2
WIRETYPE_START_GROUP = 3
WIRETYPE_END_GROUP = 4
WIRETYPE_FIXED32 = 5

# Protobuf field type mapping
TYPE_NAMES: Dict[int, str] = {
    1: "double",
    2: "float",
    3: "int64",
    4: "uint64",
    5: "int32",
    6: "fixed64",
    7: "fixed32",
    8: "bool",
    9: "string",
    10: "group",
    11: "message",
    12: "bytes",
    13: "uint32",
    14: "enum",
    15: "sfixed32",
    16: "sfixed64",
    17: "sint32",
    18: "sint64",
}

LABEL_NAMES: Dict[int, str] = {
    1: "optional",
    2: "required",
    3: "repeated",
}


def decode_varint(buf: bytes, pos: int) -> Tuple[int, int]:
    """Decodes an unsigned LEB128 varint from the buffer."""
    res = 0
    shift = 0
    while True:
        if pos >= len(buf):
            raise ValueError("Unexpected EOF while decoding varint")
        b = buf[pos]
        pos += 1
        res |= (b & 0x7F) << shift
        shift += 7
        if not (b & 0x80):
            break
    return res, pos


def parse_raw_fields(buf: bytes, pos: int = 0, end: Optional[int] = None) -> List[Tuple[int, int, Any]]:
    """Decodes raw protobuf tag-value fields from a buffer slice."""
    if end is None:
        end = len(buf)
    fields: List[Tuple[int, int, Any]] = []
    while pos < end:
        tag, pos = decode_varint(buf, pos)
        field_num = tag >> 3
        wire_type = tag & 7
        if wire_type == WIRETYPE_VARINT:
            val, pos = decode_varint(buf, pos)
        elif wire_type == WIRETYPE_LENGTH_DELIMITED:
            length, pos = decode_varint(buf, pos)
            val = buf[pos : pos + length]
            pos += length
        elif wire_type == WIRETYPE_FIXED64:
            val = buf[pos : pos + 8]
            pos += 8
        elif wire_type == WIRETYPE_FIXED32:
            val = buf[pos : pos + 4]
            pos += 4
        else:
            # Skip or stop on unsupported/group wire types
            break
        fields.append((field_num, wire_type, val))
    return fields


class ProtoWireDecoder:
    """Decodes serialized FileDescriptorProto message trees directly from binary."""

    @staticmethod
    def get_str(fields: List[Tuple[int, int, Any]], field_num: int, default: str = "") -> str:
        for fn, wt, val in fields:
            if fn == field_num and wt == WIRETYPE_LENGTH_DELIMITED:
                return val.decode("latin-1", errors="ignore")
        return default

    @staticmethod
    def get_int(fields: List[Tuple[int, int, Any]], field_num: int, default: int = 0) -> int:
        for fn, wt, val in fields:
            if fn == field_num and wt == WIRETYPE_VARINT:
                return val
        return default

    @classmethod
    def decode_enum_value(cls, buf: bytes) -> Dict[str, Any]:
        fields = parse_raw_fields(buf)
        return {
            "name": cls.get_str(fields, 1, ""),
            "number": cls.get_int(fields, 2, 0),
        }

    @classmethod
    def decode_enum(cls, buf: bytes) -> Dict[str, Any]:
        fields = parse_raw_fields(buf)
        name = cls.get_str(fields, 1, "")
        values = [cls.decode_enum_value(v) for fn, wt, v in fields if fn == 2 and wt == WIRETYPE_LENGTH_DELIMITED]
        return {
            "name": name,
            "values": values,
        }

    @classmethod
    def decode_field(cls, buf: bytes) -> Dict[str, Any]:
        fields = parse_raw_fields(buf)
        name = cls.get_str(fields, 1, "")
        extendee = cls.get_str(fields, 2, "")
        number = cls.get_int(fields, 3, 0)
        label = cls.get_int(fields, 4, 1)
        ftype = cls.get_int(fields, 5, 0)
        type_name = cls.get_str(fields, 6, "")
        default_val = cls.get_str(fields, 7, "")
        oneof_index = cls.get_int(fields, 9, -1) if any(fn == 9 for fn, wt, _ in fields) else None

        # Clean type name (strip leading dot for readability)
        clean_type_name = type_name.lstrip(".")

        return {
            "name": name,
            "number": number,
            "label": LABEL_NAMES.get(label, "optional"),
            "label_id": label,
            "type_id": ftype,
            "type": TYPE_NAMES.get(ftype, clean_type_name or "bytes"),
            "type_name": clean_type_name,
            "default_value": default_val,
            "extendee": extendee.lstrip("."),
            "oneof_index": oneof_index,
        }

    @classmethod
    def decode_extension_range(cls, buf: bytes) -> Dict[str, int]:
        fields = parse_raw_fields(buf)
        start = cls.get_int(fields, 1, 0)
        end = cls.get_int(fields, 2, 0)
        return {"start": start, "end": end}

    @classmethod
    def decode_message(cls, buf: bytes) -> Dict[str, Any]:
        fields = parse_raw_fields(buf)
        name = cls.get_str(fields, 1, "")

        msg_fields = [cls.decode_field(v) for fn, wt, v in fields if fn == 2 and wt == WIRETYPE_LENGTH_DELIMITED]
        nested_msgs = [cls.decode_message(v) for fn, wt, v in fields if fn == 3 and wt == WIRETYPE_LENGTH_DELIMITED]
        nested_enums = [cls.decode_enum(v) for fn, wt, v in fields if fn == 4 and wt == WIRETYPE_LENGTH_DELIMITED]
        ext_ranges = [cls.decode_extension_range(v) for fn, wt, v in fields if fn == 5 and wt == WIRETYPE_LENGTH_DELIMITED]
        extensions = [cls.decode_field(v) for fn, wt, v in fields if fn == 6 and wt == WIRETYPE_LENGTH_DELIMITED]

        oneof_decls = []
        for fn, wt, v in fields:
            if fn == 8 and wt == WIRETYPE_LENGTH_DELIMITED:
                of_fields = parse_raw_fields(v)
                oneof_decls.append(cls.get_str(of_fields, 1, ""))

        return {
            "name": name,
            "fields": msg_fields,
            "nested_messages": nested_msgs,
            "nested_enums": nested_enums,
            "extension_ranges": ext_ranges,
            "extensions": extensions,
            "oneof_decls": oneof_decls,
        }

    @classmethod
    def decode_service_method(cls, buf: bytes) -> Dict[str, Any]:
        fields = parse_raw_fields(buf)
        return {
            "name": cls.get_str(fields, 1, ""),
            "input_type": cls.get_str(fields, 2, "").lstrip("."),
            "output_type": cls.get_str(fields, 3, "").lstrip("."),
        }

    @classmethod
    def decode_service(cls, buf: bytes) -> Dict[str, Any]:
        fields = parse_raw_fields(buf)
        name = cls.get_str(fields, 1, "")
        methods = [cls.decode_service_method(v) for fn, wt, v in fields if fn == 2 and wt == WIRETYPE_LENGTH_DELIMITED]
        return {
            "name": name,
            "methods": methods,
        }

    @classmethod
    def decode_file_descriptor(cls, buf: bytes) -> Dict[str, Any]:
        """Decodes top-level FileDescriptorProto."""
        fields = parse_raw_fields(buf)
        name = cls.get_str(fields, 1, "")
        package = cls.get_str(fields, 2, "")
        dependencies = [v.decode("latin-1", errors="ignore") for fn, wt, v in fields if fn == 3 and wt == WIRETYPE_LENGTH_DELIMITED]
        messages = [cls.decode_message(v) for fn, wt, v in fields if fn == 4 and wt == WIRETYPE_LENGTH_DELIMITED]
        enums = [cls.decode_enum(v) for fn, wt, v in fields if fn == 5 and wt == WIRETYPE_LENGTH_DELIMITED]
        services = [cls.decode_service(v) for fn, wt, v in fields if fn == 6 and wt == WIRETYPE_LENGTH_DELIMITED]
        extensions = [cls.decode_field(v) for fn, wt, v in fields if fn == 7 and wt == WIRETYPE_LENGTH_DELIMITED]
        syntax = cls.get_str(fields, 12, "proto2")

        return {
            "name": name,
            "package": package,
            "syntax": syntax,
            "dependencies": dependencies,
            "messages": messages,
            "enums": enums,
            "services": services,
            "extensions": extensions,
        }


class ProtoFormatter:
    """Emits canonical Google Protobuf schema (.proto) source code."""

    @classmethod
    def format_enum(cls, enum_info: Dict[str, Any], indent: int = 0) -> List[str]:
        pad = "  " * indent
        lines = [f"{pad}enum {enum_info['name']} {{"]
        for val in enum_info.get("values", []):
            lines.append(f"{pad}  {val['name']} = {val['number']};")
        lines.append(f"{pad}}}")
        return lines

    @classmethod
    def format_field(cls, f: Dict[str, Any], indent: int = 1, is_proto3: bool = False) -> str:
        pad = "  " * indent
        field_type = f["type_name"] if f["type_id"] in (11, 14) and f["type_name"] else f["type"]

        label_part = ""
        if not is_proto3 or f["label"] == "repeated":
            label_part = f"{f['label']} "

        opts = []
        if f.get("default_value"):
            opts.append(f"default = {f['default_value']}")

        opt_str = f" [{', '.join(opts)}]" if opts else ""
        return f"{pad}{label_part}{field_type} {f['name']} = {f['number']}{opt_str};"

    @classmethod
    def format_message(cls, msg: Dict[str, Any], indent: int = 0, is_proto3: bool = False) -> List[str]:
        pad = "  " * indent
        lines = [f"{pad}message {msg['name']} {{"]

        # Nested enums
        for e in msg.get("nested_enums", []):
            lines.extend(cls.format_enum(e, indent + 1))
            lines.append("")

        # Nested messages
        for sub_m in msg.get("nested_messages", []):
            lines.extend(cls.format_message(sub_m, indent + 1, is_proto3))
            lines.append("")

        # Extension ranges
        for er in msg.get("extension_ranges", []):
            end_val = "max" if er["end"] >= 536870911 else er["end"] - 1
            lines.append(f"{pad}  extensions {er['start']} to {end_val};")
        if msg.get("extension_ranges"):
            lines.append("")

        # Group fields by oneof
        oneof_decls = msg.get("oneof_decls", [])
        oneof_fields: Dict[int, List[Dict[str, Any]]] = {i: [] for i in range(len(oneof_decls))}
        standard_fields: List[Dict[str, Any]] = []

        for f in msg.get("fields", []):
            idx = f.get("oneof_index")
            if idx is not None and 0 <= idx < len(oneof_decls):
                oneof_fields[idx].append(f)
            else:
                standard_fields.append(f)

        # Standard fields
        for f in standard_fields:
            lines.append(cls.format_field(f, indent + 1, is_proto3))

        # Oneofs
        for idx, of_name in enumerate(oneof_decls):
            if lines and lines[-1] != "":
                lines.append("")
            lines.append(f"{pad}  oneof {of_name} {{")
            for of_f in oneof_fields.get(idx, []):
                # Fields inside oneof omit label
                field_type = of_f["type_name"] if of_f["type_id"] in (11, 14) and of_f["type_name"] else of_f["type"]
                lines.append(f"{pad}    {field_type} {of_f['name']} = {of_f['number']};")
            lines.append(f"{pad}  }}")

        # Nested extensions
        if msg.get("extensions"):
            lines.append("")
            # Group extensions by extendee
            by_extendee: Dict[str, List[Dict[str, Any]]] = {}
            for ext in msg["extensions"]:
                target = ext.get("extendee") or "google.protobuf.MessageOptions"
                by_extendee.setdefault(target, []).append(ext)
            for target, ext_list in by_extendee.items():
                lines.append(f"{pad}  extend {target} {{")
                for ext in ext_list:
                    lines.append(cls.format_field(ext, indent + 2, is_proto3))
                lines.append(f"{pad}  }}")

        # Clean trailing empty lines
        while len(lines) > 1 and lines[-1] == "":
            lines.pop()

        lines.append(f"{pad}}}")
        return lines

    @classmethod
    def format_service(cls, svc: Dict[str, Any]) -> List[str]:
        lines = [f"service {svc['name']} {{"]
        for m in svc.get("methods", []):
            lines.append(f"  rpc {m['name']} ({m['input_type']}) returns ({m['output_type']});")
        lines.append("}")
        return lines

    @classmethod
    def emit_proto_file(
        cls,
        schema: Dict[str, Any],
        source_dll: str = "",
        size: int = 0
    ) -> str:
        """Formats the full .proto source file text."""
        lines = []
        lines.append("// ============================================================================")
        lines.append(f"// Extracted from Source 2 Binary: {os.path.basename(source_dll)}")
        lines.append(f"// File Descriptor: {schema['name']} (Size: {size:,} bytes)")
        lines.append("// Extracted via tools/extract_protobuf_schemas.py")
        lines.append("// ============================================================================")
        lines.append("")

        syntax = schema.get("syntax") or "proto2"
        is_proto3 = (syntax == "proto3")
        lines.append(f'syntax = "{syntax}";')
        lines.append("")

        if schema.get("package"):
            lines.append(f"package {schema['package']};")
            lines.append("")

        if schema.get("dependencies"):
            for dep in schema["dependencies"]:
                lines.append(f'import "{dep}";')
            lines.append("")

        # Top-level Enums
        for e in schema.get("enums", []):
            lines.extend(cls.format_enum(e, 0))
            lines.append("")

        # Top-level Messages
        for m in schema.get("messages", []):
            lines.extend(cls.format_message(m, 0, is_proto3))
            lines.append("")

        # Top-level Extensions
        if schema.get("extensions"):
            by_extendee: Dict[str, List[Dict[str, Any]]] = {}
            for ext in schema["extensions"]:
                target = ext.get("extendee") or "google.protobuf.MessageOptions"
                by_extendee.setdefault(target, []).append(ext)
            for target, ext_list in by_extendee.items():
                lines.append(f"extend {target} {{")
                for ext in ext_list:
                    lines.append(cls.format_field(ext, 1, is_proto3))
                lines.append("}")
                lines.append("")

        # Top-level Services
        for s in schema.get("services", []):
            lines.extend(cls.format_service(s))
            lines.append("")

        return "\n".join(lines).strip() + "\n"


class BinaryProtoScanner:
    """Scans compiled PE binaries for Google Protobuf DescriptorTable entries."""

    def __init__(self, filepath: str):
        self.filepath = filepath
        self.filename = os.path.basename(filepath)
        with open(filepath, "rb") as f:
            self.data = f.read()
        self.image_base: int = 0x180000000
        self.sections: List[Tuple[str, int, int, int, int]] = []
        self._parse_pe()

    def _parse_pe(self) -> None:
        if len(self.data) < 0x40:
            return
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
            s_entry = self.data[sec_off + i * 40 : sec_off + (i + 1) * 40]
            name = s_entry[:8].decode("latin-1", errors="ignore").strip("\x00")
            vsize, rva, raw_size, raw_ptr = struct.unpack_from("<IIII", s_entry, 8)
            self.sections.append((name, rva, vsize, raw_ptr, raw_size))

    def va_to_off(self, va: int) -> Optional[int]:
        rva = va - self.image_base
        for name, rva_sec, vsize, raw_ptr, raw_size in self.sections:
            if rva_sec <= rva < rva_sec + vsize:
                off = raw_ptr + (rva - rva_sec)
                if 0 <= off < len(self.data):
                    return off
        return None

    def off_to_va(self, off: int) -> Optional[int]:
        for name, rva_sec, vsize, raw_ptr, raw_size in self.sections:
            if raw_ptr <= off < raw_ptr + raw_size:
                return self.image_base + rva_sec + (off - raw_ptr)
        return None

    def scan_descriptors(self) -> List[Tuple[str, int, int]]:
        """
        Scans for compiled DescriptorTable instances.
        Returns list of (proto_name, descriptor_size, file_offset).
        """
        results: List[Tuple[str, int, int]] = []
        seen_names: Set[str] = set()

        # Match .proto null-terminated strings
        proto_str_regex = re.compile(rb"([a-zA-Z0-9_\-\./]+\.proto)\x00")
        for m in proto_str_regex.finditer(self.data):
            proto_name = m.group(1).decode("latin-1", errors="ignore")
            off_str = m.start()

            # Filter out non-canonical paths
            if proto_name.startswith(".") or proto_name.endswith(".google.proto"):
                continue

            # Convert string file offset to VA
            va_str = self.off_to_va(off_str)
            if va_str is None:
                continue

            va_bytes = struct.pack("<Q", va_str)
            pos = 0
            while True:
                pos = self.data.find(va_bytes, pos)
                if pos == -1:
                    break
                if pos >= 12:
                    # In DescriptorTable: [padding/flags: 4, size: 4, desc_va: 8, filename_va: 8]
                    desc_va = struct.unpack_from("<Q", self.data, pos - 8)[0]
                    size = struct.unpack_from("<I", self.data, pos - 12)[0]
                    desc_off = self.va_to_off(desc_va)

                    if desc_off is not None and 0 <= desc_off < len(self.data) and 10 < size < 3000000:
                        # FileDescriptorProto starts with tag 1 (0x0A)
                        if self.data[desc_off] == 0x0A:
                            if proto_name not in seen_names:
                                results.append((proto_name, size, desc_off))
                                seen_names.add(proto_name)
                pos += 8

        return sorted(results, key=lambda x: x[0])


def main():
    parser = argparse.ArgumentParser(
        description="Source 2 Compiled Google Protobuf Schema Extractor"
    )
    parser.add_argument(
        "--archive",
        default="versions/archive/2026-11-11_26598758",
        help="Path to archive version directory containing game DLLs"
    )
    parser.add_argument(
        "--dll",
        help="Specific DLL filename to scan (e.g. networksystem.dll or client.dll)"
    )
    parser.add_argument(
        "--outdir",
        default="data/protobuf",
        help="Output directory to emit extracted .proto files (default: data/protobuf)"
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List discovered descriptors without extracting files"
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Display detailed message and field decoding logs"
    )

    args = parser.parse_args()

    if not os.path.isdir(args.archive):
        print(f"[-] Error: Archive directory not found: {args.archive}", file=sys.stderr)
        sys.exit(1)

    # Determine DLLs to inspect
    if args.dll:
        target_path = os.path.join(args.archive, args.dll) if not os.path.isabs(args.dll) else args.dll
        if not os.path.isfile(target_path):
            print(f"[-] Error: DLL file not found: {target_path}", file=sys.stderr)
            sys.exit(1)
        dll_files = [target_path]
    else:
        dll_files = sorted(glob.glob(os.path.join(args.archive, "*.dll")))

    print("=================================================================")
    print(" SOURCE 2 PROTOBUF SCHEMA EXTRACTOR")
    print("=================================================================")
    print(f"[*] Scanning {len(dll_files)} binaries in: {args.archive}...")

    all_schemas: Dict[str, Tuple[Dict[str, Any], str, int]] = {}
    total_descriptors_found = 0

    for dll_path in dll_files:
        dll_name = os.path.basename(dll_path)
        scanner = BinaryProtoScanner(dll_path)
        descs = scanner.scan_descriptors()
        if not descs:
            continue

        total_descriptors_found += len(descs)
        print(f"  [+] {dll_name:25s}: Discovered {len(descs)} descriptor tables")

        for proto_name, size, desc_off in descs:
            raw_chunk = scanner.data[desc_off : desc_off + size]
            try:
                decoded = ProtoWireDecoder.decode_file_descriptor(raw_chunk)
                # Keep largest descriptor if duplicate across DLLs
                if proto_name not in all_schemas or all_schemas[proto_name][2] < size:
                    all_schemas[proto_name] = (decoded, dll_name, size)
            except Exception as e:
                if args.verbose:
                    print(f"    [-] Failed decoding {proto_name} in {dll_name}: {e}")

    print(f"\n[*] Total unique schemas successfully decoded: {len(all_schemas)} / {total_descriptors_found} tables")

    if args.list:
        print("\nDiscovered Schemas:")
        for name in sorted(all_schemas.keys()):
            decoded, dll_name, size = all_schemas[name]
            msg_count = len(decoded.get("messages", []))
            enum_count = len(decoded.get("enums", []))
            print(f"  - {name:40s} ({size:6d} B) [{msg_count} msgs, {enum_count} enums] from {dll_name}")
        sys.exit(0)

    # Ensure output directory exists
    os.makedirs(args.outdir, exist_ok=True)
    manifest: Dict[str, Any] = {
        "metadata": {
            "source_archive": os.path.basename(args.archive.rstrip("/\\")),
            "total_schemas": len(all_schemas),
        },
        "schemas": {},
    }

    print(f"[*] Writing canonical .proto files to: {args.outdir}...")
    written_count = 0

    for proto_name, (schema, dll_name, size) in sorted(all_schemas.items()):
        # Handle subdirectories in proto_name (e.g. google/protobuf/descriptor.proto)
        clean_name = os.path.normpath(proto_name).replace("\\", "/")
        out_filepath = os.path.join(args.outdir, clean_name)
        os.makedirs(os.path.dirname(out_filepath), exist_ok=True)

        proto_code = ProtoFormatter.emit_proto_file(schema, dll_name, size)
        with open(out_filepath, "w", encoding="utf-8") as f:
            f.write(proto_code)

        written_count += 1
        msg_names = [m["name"] for m in schema.get("messages", [])]
        enum_names = [e["name"] for e in schema.get("enums", [])]
        svc_names = [s["name"] for s in schema.get("services", [])]

        manifest["schemas"][clean_name] = {
            "source_binary": dll_name,
            "size_bytes": size,
            "package": schema.get("package", ""),
            "syntax": schema.get("syntax", "proto2"),
            "dependencies": schema.get("dependencies", []),
            "message_count": len(msg_names),
            "messages": msg_names,
            "enum_count": len(enum_names),
            "enums": enum_names,
            "service_count": len(svc_names),
            "services": svc_names,
        }

    manifest_path = os.path.join(args.outdir, "manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    print(f"[+] Successfully extracted {written_count} canonical .proto files!")
    print(f"[+] Generated schema manifest at: {manifest_path}")

    # Display highlight samples
    print("\n[+] Highlight Wire Protocol Schemas Extracted:")
    highlight_keys = [
        "cstrike15_usermessages.proto",
        "cs_usercmd.proto",
        "netmessages.proto",
        "gameevents.proto",
        "cs_gameevents.proto",
        "networkbasetypes.proto",
    ]
    for hk in highlight_keys:
        if hk in manifest["schemas"]:
            info = manifest["schemas"][hk]
            print(f"    - {hk:30s}: {info['message_count']:3d} msgs, {info['enum_count']:2d} enums (from {info['source_binary']})")

    sys.exit(0)


if __name__ == "__main__":
    main()
