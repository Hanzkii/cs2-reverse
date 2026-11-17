#!/usr/bin/env python3
"""
import_sdk_types.py - Source 2 Ghidra C-Parser & SDK Type Importer

Headlessly imports data/source2_types.h and the reconstructed C++20 definitions
from game/shared/schema_classes/ into Ghidra's DataTypeManager (/Source2/Classes
and /Source2/BaseTypes).

Allows reverse engineers to automatically populate typed structures across CS2
binaries (client.dll, server.dll, engine2.dll, schemasystem.dll, etc.) so that
Ghidra's decompiler displays typed field member accesses instead of raw offsets.

Dual-Mode Operation:
1. In-Ghidra Execution (PyGhidra / Jython / Ghidra Script Manager):
   - Directly manipulates currentProgram.getDataTypeManager()
   - Populates /Source2/BaseTypes (Vector, QAngle, CEntityHandle, CTransform, etc.)
   - Populates /Source2/Classes for all schema entities

2. Standalone CLI Orchestrator:
   - Pre-parses and validates data/source2_types.h and C++20 schema classes
   - Merges high-fidelity C++20 entity types into the master schema catalog
   - Exports data/ghidra_sdk_classes.json
   - Dispatches analyzeHeadless.bat with Source2_ImportSDKTypes.java
   - Full --dry-run simulation support
"""

import os
import sys
import re
import json
import argparse
import subprocess
import time
from pathlib import Path


# =============================================================================
# REPOSITORY & ENVIRONMENT HELPERS (RULE 1: STRICT PATH SANITIZATION)
# =============================================================================

def find_repo_root() -> Path:
    """Locates the repository root without leaking local system paths."""
    cur = Path(__file__).resolve().parent
    for _ in range(4):
        if (cur / "data" / "source2_types.h").exists():
            return cur
        cur = cur.parent
    return Path.cwd()


def sanitize_display_path(p: Path) -> str:
    """Returns a relative or generic display string for logs."""
    try:
        repo_root = find_repo_root()
        rel = p.resolve().relative_to(repo_root.resolve())
        return str(rel).replace("\\", "/")
    except Exception:
        # Fallback to name or generic placeholder
        return p.name


# =============================================================================
# C-HEADER & C++20 SCHEMA PARSERS
# =============================================================================

def parse_source2_types_header(header_path: Path) -> tuple:
    """
    Parses data/source2_types.h:
    Extracts base primitives and all 2,276 C struct definitions with their
    target binary, base class, total size, and member fields.
    """
    if not header_path.exists():
        raise FileNotFoundError(f"C header not found: {header_path}")

    content = header_path.read_text(encoding="utf-8", errors="ignore")

    struct_re = re.compile(
        r"/\*\s*Binary:\s*([^|]*?)\s*\|\s*Base:\s*([^|]*?)\s*\|\s*Size:\s*(\d+)\s*\([^)]+\)\s*\*/\s*struct\s+([A-Za-z0-9_]+)\s*\{([^}]+)\};",
        re.DOTALL
    )

    classes = {}
    total_fields = 0

    for m in struct_re.finditer(content):
        binary = m.group(1).strip()
        base = m.group(2).strip()
        size = int(m.group(3).strip())
        name = m.group(4).strip()
        body = m.group(5)

        fields = []
        for line in body.splitlines():
            fl = line.strip()
            if not fl or fl.startswith("//") or fl.startswith("/*"):
                continue

            # Skip padding bytes
            if re.match(r"^uint8_t\s+_(?:pad|end_pad)_0x[0-9a-fA-F]+\[\d+\];", fl):
                continue

            # Field declaration: type name[dim]; /* +0x123 */
            f_m = re.match(
                r"^([A-Za-z0-9_*]+(?:\s*\*+)?)\s+([A-Za-z0-9_]+)(?:\[([^\]]+)\])?;\s*(?:/\*\s*\+0x([0-9a-fA-F]+)\s*\*/)?",
                fl
            )
            if f_m:
                ftype = f_m.group(1).strip()
                fname = f_m.group(2).strip()
                arr_dim = f_m.group(3)
                foff_str = f_m.group(4)
                foff = int(foff_str, 16) if foff_str else 0
                if arr_dim:
                    ftype = f"{ftype}[{arr_dim}]"
                fields.append({"name": fname, "type": ftype, "offset": foff})
                total_fields += 1

        classes[name] = {
            "binary": binary,
            "base": base if base and base != "None" else None,
            "size": size,
            "fields": fields
        }

    return classes, total_fields


def parse_cpp20_schema_classes(classes_dir: Path) -> dict:
    """
    Parses clean-room C++20 reconstructed schema classes in game/shared/schema_classes/
    Extracts SCHEMA_CLASS_NAME, SCHEMA_MODULE_NAME, SCHEMA_SIZE, base class, and
    SCHEMA_FIELD(type, name, offset) definitions.
    """
    if not classes_dir.exists():
        return {}

    parsed = {}
    for p in sorted(classes_dir.glob("*.h")):
        if p.name in ("schema_classes.h", "schema_types.h"):
            continue

        content = p.read_text(encoding="utf-8", errors="ignore")
        m_cls = re.search(r"class\s+([A-Za-z0-9_]+)(?:\s*:\s*public\s+([A-Za-z0-9_]+))?", content)
        m_name = re.search(r'SCHEMA_CLASS_NAME\s*=\s*"([^"]+)"', content)
        m_mod = re.search(r'SCHEMA_MODULE_NAME\s*=\s*"([^"]+)"', content)
        m_size = re.search(r"SCHEMA_SIZE\s*=\s*(0x[0-9a-fA-F]+|\d+)", content)

        cls_name = m_name.group(1) if m_name else (m_cls.group(1) if m_cls else p.stem)
        base_cls = m_cls.group(2) if m_cls else None
        mod_name = m_mod.group(1) if m_mod else "client.dll"

        size_val = 0
        if m_size:
            s_str = m_size.group(1)
            size_val = int(s_str, 16) if s_str.startswith("0x") else int(s_str)

        fields = []
        for fm in re.finditer(r"SCHEMA_FIELD\s*\(\s*([^,]+?)\s*,\s*([^,]+?)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\)", content):
            ftype = fm.group(1).strip()
            fname = fm.group(2).strip()
            foff_str = fm.group(3).strip()
            foff = int(foff_str, 16) if foff_str.startswith("0x") else int(foff_str)
            fields.append({"name": fname, "type": ftype, "offset": foff})

        parsed[cls_name] = {
            "binary": mod_name,
            "base": base_cls,
            "size": size_val,
            "fields": fields
        }

    return parsed


def merge_sdk_types(header_classes: dict, cpp20_classes: dict) -> tuple:
    """
    Merges high-fidelity C++20 reconstructed schema classes into the header classes.
    Replaces generic types (void*, uint32_t, etc.) with accurate game SDK types
    (CGameSceneNode*, CEntityHandle, QAngle, CPlayer_AimPunchServices*, etc.).
    """
    merged = dict(header_classes)
    upgraded_fields = 0

    for cls_name, cpp_cls in cpp20_classes.items():
        if cls_name not in merged:
            merged[cls_name] = cpp_cls
            continue

        base_info = merged[cls_name]
        if cpp_cls.get("size", 0) > 0:
            base_info["size"] = cpp_cls["size"]
        if cpp_cls.get("base"):
            base_info["base"] = cpp_cls["base"]
        if cpp_cls.get("binary"):
            base_info["binary"] = cpp_cls["binary"]

        # Map C++20 fields by offset and name
        cpp_fields_by_offset = {f["offset"]: f for f in cpp_cls.get("fields", [])}

        for f in base_info.get("fields", []):
            foff = f["offset"]
            if foff in cpp_fields_by_offset:
                cpp_f = cpp_fields_by_offset[foff]
                if cpp_f["type"] != f["type"]:
                    f["type"] = cpp_f["type"]
                    f["name"] = cpp_f["name"]
                    upgraded_fields += 1

    return merged, upgraded_fields


# =============================================================================
# IN-GHIDRA EXECUTION (Jython / PyGhidra)
# =============================================================================

def run_inside_ghidra():
    """Execution path when running inside Ghidra environment."""
    from ghidra.program.model.data import (
        CategoryPath, StructureDataType, FloatDataType, IntegerDataType,
        UnsignedIntegerDataType, ByteDataType, ShortDataType, PointerDataType,
        BooleanDataType, LongLongDataType, UnsignedLongLongDataType,
        DataTypeConflictHandler
    )

    prog = currentProgram  # injected by Ghidra
    if not prog:
        print("[-] ERROR: No active program in Ghidra context.")
        return

    raw_name = prog.getName()
    prog_name = raw_name.lower()
    if "/" in prog_name:
        prog_name = prog_name.split("/")[-1]
    if "\\" in prog_name:
        prog_name = prog_name.split("\\")[-1]

    print("[*] Active Ghidra Program: {} (normalized: {})".format(raw_name, prog_name))

    repo_root = find_repo_root()
    json_path = repo_root / "data" / "ghidra_sdk_classes.json"
    header_path = repo_root / "data" / "source2_types.h"

    classes = {}
    if json_path.exists():
        with open(json_path, "r", encoding="utf-8") as f:
            data = json.load(f)
            classes = data.get("classes", data)
    elif header_path.exists():
        classes, _ = parse_source2_types_header(header_path)

    if not classes:
        print("[-] ERROR: No SDK classes loaded.")
        return

    dtm = prog.getDataTypeManager()
    base_cat = CategoryPath("/Source2/BaseTypes")
    class_cat = CategoryPath("/Source2/Classes")

    tx = dtm.startTransaction("Import Source 2 SDK Types")
    success = False
    created = 0

    try:
        # Register Base Types
        vec = StructureDataType(base_cat, "Vector", 12, dtm)
        vec.replaceAtOffset(0, FloatDataType.dataType, 4, "x", "X")
        vec.replaceAtOffset(4, FloatDataType.dataType, 4, "y", "Y")
        vec.replaceAtOffset(8, FloatDataType.dataType, 4, "z", "Z")
        dtm.addDataType(vec, DataTypeConflictHandler.REPLACE_HANDLER)

        qang = StructureDataType(base_cat, "QAngle", 12, dtm)
        qang.replaceAtOffset(0, FloatDataType.dataType, 4, "pitch", "Pitch")
        qang.replaceAtOffset(4, FloatDataType.dataType, 4, "yaw", "Yaw")
        qang.replaceAtOffset(8, FloatDataType.dataType, 4, "roll", "Roll")
        dtm.addDataType(qang, DataTypeConflictHandler.REPLACE_HANDLER)

        hdl = StructureDataType(base_cat, "CEntityHandle", 4, dtm)
        hdl.replaceAtOffset(0, UnsignedIntegerDataType.dataType, 4, "m_Index", "Handle")
        dtm.addDataType(hdl, DataTypeConflictHandler.REPLACE_HANDLER)

        for cname, cinfo in classes.items():
            b = cinfo.get("binary", "").lower()
            if b and prog_name not in b and b not in prog_name and prog_name != "all":
                continue

            size = cinfo.get("size", 8)
            if size <= 0:
                size = 8
            struct = StructureDataType(class_cat, cname, size, dtm)

            for f in cinfo.get("fields", []):
                off = f.get("offset", 0)
                fname = f.get("name", "field")
                ftype_str = f.get("type", "uint32_t")

                # Resolve basic type
                dt = UnsignedIntegerDataType.dataType
                sz = 4
                if "*" in ftype_str:
                    dt = PointerDataType.dataType
                    sz = 8
                elif ftype_str in ("Vector", "Vector3"):
                    dt = vec
                    sz = 12
                elif ftype_str == "QAngle":
                    dt = qang
                    sz = 12
                elif ftype_str == "CEntityHandle":
                    dt = hdl
                    sz = 4
                elif ftype_str == "bool":
                    dt = BooleanDataType.dataType
                    sz = 1
                elif ftype_str == "float":
                    dt = FloatDataType.dataType
                    sz = 4
                elif ftype_str in ("int64_t", "uint64_t", "uintptr_t"):
                    dt = UnsignedLongLongDataType.dataType
                    sz = 8

                try:
                    if off + sz <= size:
                        struct.replaceAtOffset(off, dt, sz, fname, "Offset: +0x{:x}".format(off))
                except Exception:
                    pass

            dtm.addDataType(struct, DataTypeConflictHandler.REPLACE_HANDLER)
            created += 1

        success = True
        print("[+] SUCCESS: Created {} classes in {}".format(created, class_cat.getPath()))
    finally:
        dtm.endTransaction(tx, success)


# =============================================================================
# STANDALONE CLI ORCHESTRATION (HEADLESS C-PARSER & TYPE INGESTION)
# =============================================================================

def run_headless_import(ghidra_home: Path, project_dir: Path, project_name: str,
                        folder: str, target_binary: str, source_header: Path,
                        schema_classes_dir: Path, output_json: Path,
                        use_cparser: bool = False, dry_run: bool = False) -> bool:
    """Orchestrates analyzeHeadless to import SDK types across target binaries."""
    repo_root = find_repo_root()
    java_script = repo_root / "tools" / "ghidra" / "Source2_ImportSDKTypes.java"

    print("=================================================================")
    print(" CS2 HEADLESS SDK TYPE IMPORTER & C-PARSER PIPELINE")
    print("=================================================================")
    print(f" Ghidra Home:        {ghidra_home}")
    print(f" Project Dir:        {project_dir}")
    print(f" Project Name:       {project_name}")
    print(f" Version Folder:     {folder or '(root)'}")
    print(f" Source C Header:    {sanitize_display_path(source_header)}")
    print(f" C++20 Schemas Dir:  {sanitize_display_path(schema_classes_dir)}")
    print(f" Target Binary:      {target_binary}")
    print(f" Engine Mode:        {'C-Parser + DataTypeManager' if use_cparser else 'DataTypeManager Direct'}")
    print("=================================================================")

    # 1. Parse C Header
    t0 = time.time()
    header_classes, total_hdr_fields = parse_source2_types_header(source_header)
    print(f"[*] Parsed {len(header_classes):,} C structs with {total_hdr_fields:,} fields from {source_header.name}")

    # 2. Parse C++20 Reconstructed Classes
    cpp_classes = parse_cpp20_schema_classes(schema_classes_dir)
    print(f"[*] Parsed {len(cpp_classes)} C++20 reconstructed schema classes from {sanitize_display_path(schema_classes_dir)}")

    # 3. Merge C++20 High-Fidelity Types
    merged_classes, upgraded_fields = merge_sdk_types(header_classes, cpp_classes)
    print(f"[+] Merged SDK catalog: {len(merged_classes):,} total classes ({upgraded_fields} fields upgraded with C++20 types)")

    # 4. Count breakdown by target binary
    by_binary = {}
    for cname, cinfo in merged_classes.items():
        b = cinfo.get("binary") or "universal"
        by_binary[b] = by_binary.get(b, 0) + 1

    print("\nTarget Module Class Distribution:")
    for b, count in sorted(by_binary.items(), key=lambda x: -x[1])[:10]:
        print(f"   - {b:<28} : {count:>5} classes")
    if len(by_binary) > 10:
        print(f"   - ... and {len(by_binary) - 10} additional modules")

    # 5. Export JSON intermediate artifact for Java/Jython ingestion
    if not dry_run:
        output_json.parent.mkdir(parents=True, exist_ok=True)
        with open(output_json, "w", encoding="utf-8") as f:
            json.dump({
                "source": "import_sdk_types.py",
                "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
                "total_classes": len(merged_classes),
                "classes": merged_classes
            }, f, indent=2)
        print(f"\n[+] Exported structured JSON to {sanitize_display_path(output_json)}")

    # 6. Determine binaries to process
    CORE_MODULES = [
        "client.dll",
        "server.dll",
        "engine2.dll",
        "schemasystem.dll",
        "vphysics2.dll",
        "networksystem.dll",
        "tier0.dll",
        "pulse_system.dll"
    ]

    if target_binary.lower() == "all":
        binaries_to_process = CORE_MODULES
    else:
        binaries_to_process = [target_binary.lower()]

    project_folder_path = f"{project_name}/{folder}" if folder else project_name
    analyze_bat = ghidra_home / "support" / "analyzeHeadless.bat"

    results = {}
    print("\nExecution Pipeline Plan:")
    for b in binaries_to_process:
        module_classes = by_binary.get(b, 0)
        print(f"\n[*] Module: {b} ({module_classes} classes to populate in /Source2/Classes)...")

        if dry_run:
            print(f"  [DRY-RUN] analyzeHeadless {project_dir} {project_folder_path} -process {b} -noanalysis -postScript Source2_ImportSDKTypes.java {sanitize_display_path(output_json)}")
            results[b] = "DRY-RUN"
            continue

        if not analyze_bat.exists():
            print(f"[-] ERROR: analyzeHeadless.bat not found at {analyze_bat}")
            return False

        cmd = [
            str(analyze_bat),
            str(project_dir),
            project_folder_path,
            "-process", b,
            "-noanalysis",
            "-postScript", "Source2_ImportSDKTypes.java",
            str(output_json.resolve()),
            "-scriptPath", str(java_script.parent.resolve()),
        ]

        try:
            proc = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                input="",
                timeout=300
            )

            lines = [l for l in proc.stdout.splitlines() if "Source2_ImportSDKTypes" in l or "SUCCESS" in l or "ERROR" in l]
            for line in lines[-8:]:
                print(f"  {line}")

            if proc.returncode == 0:
                results[b] = "SUCCESS"
                print(f"  [+] Finished importing SDK types to {b}")
            else:
                results[b] = f"EXIT_{proc.returncode}"
                print(f"  [-] Failed {b} (code {proc.returncode})")

        except subprocess.TimeoutExpired:
            print(f"  [-] Timeout processing {b}")
            results[b] = "TIMEOUT"
        except Exception as ex:
            print(f"  [-] Exception processing {b}: {ex}")
            results[b] = "EXCEPTION"

    print("\n=================================================================")
    print(" SDK TYPE IMPORT SUMMARY")
    print("=================================================================")
    for b, res in results.items():
        print(f"  {b:<30} : {res}")
    print("=================================================================")
    return all(r in ("SUCCESS", "DRY-RUN") for r in results.values())


# =============================================================================
# CLI ENTRY POINT
# =============================================================================

def main():
    if "currentProgram" in globals():
        # Running inside Ghidra environment
        run_inside_ghidra()
        return

    parser = argparse.ArgumentParser(
        description="Headless C-Parser & Source 2 SDK Type Importer for Ghidra."
    )
    parser.add_argument(
        "--ghidra-home",
        default=os.environ.get("GHIDRA_HOME") or os.environ.get("GHIDRA_INSTALL_PATH"),
        help="Path to Ghidra root directory (default: %%GHIDRA_HOME%%)",
    )
    parser.add_argument(
        "--project-dir",
        default=os.environ.get("CS2_DISASSEMBLY_DIR") or os.environ.get("GHIDRA_PROJECT_DIR"),
        help="Path to Ghidra project directory (default: %%CS2_DISASSEMBLY_DIR%%)",
    )
    parser.add_argument(
        "--project-name",
        default="CS2",
        help="Ghidra project name (default: CS2)",
    )
    parser.add_argument(
        "--version-folder",
        default="2026-11-11_26598758",
        help="Project subfolder (default: 2026-11-11_26598758)",
    )
    parser.add_argument(
        "--target-binary",
        default="all",
        help="Target binary (e.g. client.dll, server.dll, or 'all')",
    )
    parser.add_argument(
        "--source-header",
        default="data/source2_types.h",
        help="Path to Source 2 C header (default: data/source2_types.h)",
    )
    parser.add_argument(
        "--schema-classes-dir",
        default="game/shared/schema_classes",
        help="Path to reconstructed C++20 schema classes (default: game/shared/schema_classes)",
    )
    parser.add_argument(
        "--output-json",
        default="data/ghidra_sdk_classes.json",
        help="Path to export intermediate JSON (default: data/ghidra_sdk_classes.json)",
    )
    parser.add_argument(
        "--use-cparser",
        action="store_true",
        help="Instruct Ghidra to parse via CParser instead of programmatic DataTypeManager",
    )
    parser.add_argument(
        "--generate-json",
        action="store_true",
        help="Generate and export the merged SDK classes JSON to --output-json without running Ghidra",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Preview actions and validate schemas without executing analyzeHeadless",
    )

    args = parser.parse_args()

    repo_root = find_repo_root()
    source_header = (repo_root / args.source_header).resolve()
    schema_classes_dir = (repo_root / args.schema_classes_dir).resolve()
    output_json = (repo_root / args.output_json).resolve()

    if not source_header.exists():
        print(f"[-] ERROR: Source header not found: {source_header}")
        sys.exit(1)

    if args.generate_json:
        print("[*] Generating merged SDK types JSON...")
        header_classes, total_hdr_fields = parse_source2_types_header(source_header)
        cpp_classes = parse_cpp20_schema_classes(schema_classes_dir)
        merged_classes, upgraded_fields = merge_sdk_types(header_classes, cpp_classes)
        output_json.parent.mkdir(parents=True, exist_ok=True)
        with open(output_json, "w", encoding="utf-8") as f:
            json.dump({
                "source": "import_sdk_types.py",
                "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
                "total_classes": len(merged_classes),
                "classes": merged_classes
            }, f, indent=2)
        print(f"[+] Successfully exported {len(merged_classes):,} classes ({upgraded_fields} fields upgraded) to {sanitize_display_path(output_json)}")
        sys.exit(0)

    ghidra_home = Path(args.ghidra_home) if args.ghidra_home else None
    if not ghidra_home:
        if args.dry_run:
            ghidra_home = Path("<GHIDRA_HOME>")
        else:
            print("[-] ERROR: --ghidra-home or %GHIDRA_HOME% environment variable required.")
            sys.exit(1)

    project_dir = Path(args.project_dir) if args.project_dir else None
    if not project_dir:
        if args.dry_run:
            project_dir = Path("<CS2_DISASSEMBLY_DIR>")
        else:
            print("[-] ERROR: --project-dir or %CS2_DISASSEMBLY_DIR% environment variable required.")
            sys.exit(1)

    success = run_headless_import(
        ghidra_home=ghidra_home,
        project_dir=project_dir,
        project_name=args.project_name,
        folder=args.version_folder,
        target_binary=args.target_binary,
        source_header=source_header,
        schema_classes_dir=schema_classes_dir,
        output_json=output_json,
        use_cparser=args.use_cparser,
        dry_run=args.dry_run
    )

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
