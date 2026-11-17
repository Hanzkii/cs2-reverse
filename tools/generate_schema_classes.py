#!/usr/bin/env python3
"""
generate_schema_classes.py - Source 2 C++20 Schema Class Generator

Parses versions/profiles/2026-11-11_26598758/schemas.json (2,276 classes)
and automatically synthesizes clean, type-safe C++20 schema classes with exact
byte offset resolution, inheritance hierarchies, and zero-overhead C++20
lvalue reference accessors into game/shared/schema_classes/.

Key Classes Emitted:
  - C_BaseEntity.h
  - C_CSPlayerPawn.h (and C_CSPlayerPawnBase.h)
  - CCSPlayerController.h
  - C_BasePlayerWeapon.h / C_CSWeaponBase.h
  - C_SmokeGrenadeProjectile.h
  - C_PlantedC4.h
  - schema_classes.h (master aggregator header)

Standards & Guarantees:
  - MSVC x64 ABI compliant (/std:c++20 /W4 /WX).
  - Exact class sizeof verification matching Source 2 engine schema layouts.
  - Zero local path disclosure (relative repository paths only).
"""

import os
import sys
import json
import argparse
from pathlib import Path
from typing import Dict, List, Set, Any, Optional

# =============================================================================
# Key Target Classes for Game Subsystems
# =============================================================================

KEY_TARGET_CLASSES = [
    "C_BaseEntity",
    "C_CSPlayerPawnBase",
    "C_CSPlayerPawn",
    "CCSPlayerController",
    "C_BasePlayerWeapon",
    "C_CSWeaponBase",
    "C_SmokeGrenadeProjectile",
    "C_PlantedC4",
    "CGameSceneNode",
]

# Additional classes useful for weapon/player services or game subsystems
EXTENDED_TARGET_CLASSES = [
    "C_BaseModelEntity",
    "CBaseModelEntity",
    "CBaseEntity",
    "CBaseAnimGraph",
    "C_EconEntity",
    "CBasePlayerController",
    "C_BaseCombatCharacter",
    "C_BasePlayerPawn",
    "C_BaseGrenade",
    "C_BaseCSGrenadeProjectile",
    "CEntityInstance",
]


# =============================================================================
# Type Deduction Logic (Hungarian Notation & Source 2 SDK Conventions)
# =============================================================================

def deduce_field_type(name: str, raw_type: str = "") -> str:
    """
    Deduce appropriate C++20 types using Hungarian notation and known Source 2 SDK types:
      - m_h... -> CEntityHandle
      - m_pGameSceneNode -> CGameSceneNode*
      - m_vec... / m_v... -> Vector3
      - m_ang... / m_qAngle -> QAngle
      - m_fl... -> float
      - m_b... -> bool
      - m_i... -> int32_t
      - m_n... -> int32_t
      - m_un... / m_ui... -> uint32_t
      - m_us... -> uint16_t
      - m_ub... -> uint8_t
      - m_p... -> void*
      - fallback -> uintptr_t or void*
    """
    # Special exact matches
    if name == "m_pGameSceneNode":
        return "CGameSceneNode*"

    # Entity handles (m_hOwnerEntity, m_hPawn, m_hPlayerPawn, etc.)
    if name.startswith("m_h") and (len(name) == 3 or name[3].isupper() or name[3] == "_"):
        return "CEntityHandle"

    # Vectors (m_vecOrigin, m_vecMins, m_vSmokeColor, m_vSmokeDetonationPos, m_vInitialPosition, etc.)
    if name.startswith("m_vec"):
        return "Vector3"
    if name.startswith("m_v") and len(name) > 3 and (name[3].isupper() or name[3] == "_"):
        # Check if it's a vector coordinate or color
        return "Vector3"

    # Euler angles
    if name.startswith("m_ang") or name.startswith("m_qAngle"):
        return "QAngle"

    # Floating point numbers
    if name.startswith("m_fl"):
        return "float"
    if name.startswith("m_f") and len(name) > 3 and name[2:4] in ("fl", "lt"):
        return "float"

    # Booleans
    if name.startswith("m_b"):
        return "bool"

    # Unsigned integers (32-bit, 16-bit, 8-bit)
    if name.startswith("m_un") or name.startswith("m_ui"):
        return "uint32_t"
    if name.startswith("m_us"):
        return "uint16_t"
    if name.startswith("m_ub"):
        return "uint8_t"

    # Signed integers
    if name.startswith("m_i") or name.startswith("m_n"):
        return "int32_t"

    # General pointers
    if name.startswith("m_p") and (len(name) == 3 or name[3].isupper() or name[3] == "_"):
        return "void*"

    # Fallback when unknown
    return "uintptr_t"


# =============================================================================
# Schema Class Header Generator
# =============================================================================

def compute_dependency_closure(schemas: Dict[str, Any], initial_classes: List[str]) -> List[str]:
    """
    Computes the transitive closure of base classes for the requested targets,
    returning them sorted in topological order (ancestors before descendants).
    """
    visited = set()
    order = []

    def visit(cls_name: str):
        if not cls_name or cls_name in visited or cls_name not in schemas:
            return
        base_name = schemas[cls_name].get("base", "")
        if base_name and base_name in schemas:
            visit(base_name)
        visited.add(cls_name)
        order.append(cls_name)

    for cls in initial_classes:
        visit(cls)

    return order


def generate_class_header(cls_name: str, cls_info: Dict[str, Any], schemas: Dict[str, Any]) -> str:
    """
    Generates a single, clean, self-contained C++20 header for a schema class.
    """
    base_name = cls_info.get("base", "")
    has_base = bool(base_name and base_name in schemas)
    class_size = cls_info.get("size", 0)
    size_hex = cls_info.get("size_hex", hex(class_size))
    binary = cls_info.get("binary", "client.dll")
    fields = cls_info.get("fields", [])

    base_size = schemas[base_name].get("size", 0) if has_base else 0
    delta_size = class_size - base_size

    lines = []
    lines.append("// ============================================================================")
    lines.append("// Source 2 Clean-Room Reconstructed Schema Class")
    lines.append(f"// Class:  {cls_name}")
    lines.append(f"// Module: {binary}")
    lines.append(f"// Base:   {base_name if has_base else '(none)'}")
    lines.append(f"// Size:   {size_hex} ({class_size} bytes)")
    lines.append("// Auto-generated by tools/generate_schema_classes.py")
    lines.append("// ============================================================================")
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include "schema_types.h"')
    if has_base:
        lines.append(f'#include "{base_name}.h"')
    lines.append("")
    lines.append("namespace cs2::schema {")
    lines.append("")
    lines.append("/**")
    lines.append(f" * @class {cls_name}")
    lines.append(f" * @brief Reconstructed Source 2 Schema Class for {cls_name}")
    lines.append(f" * Target Module: {binary}")
    lines.append(f" * Layout Footprint: {size_hex} bytes")
    lines.append(" */")

    inheritance = f" : public {base_name}" if has_base else ""
    lines.append(f"class {cls_name}{inheritance} {{")
    lines.append("public:")
    lines.append(f'    static constexpr const char* SCHEMA_CLASS_NAME = "{cls_name}";')
    lines.append(f'    static constexpr const char* SCHEMA_MODULE_NAME = "{binary}";')
    lines.append(f"    static constexpr std::size_t SCHEMA_SIZE = {size_hex};")
    lines.append("")

    # Fields
    if fields:
        lines.append("    // ------------------------------------------------------------------------")
        lines.append("    // Reconstructed Field Accessors (C++20 Zero-Overhead Lvalue References)")
        lines.append("    // ------------------------------------------------------------------------")
        for f in fields:
            fname = f["name"]
            offset = f["offset"]
            offset_hex = f.get("offset_hex", f"+0x{offset:04x}")
            cpp_type = deduce_field_type(fname, f.get("type", ""))
            lines.append(f"    SCHEMA_FIELD({cpp_type}, {fname}, 0x{offset:X}); // {offset_hex}")
        lines.append("")

    # Storage padding to guarantee exact engine sizeof
    if delta_size > 0:
        lines.append("private:")
        if has_base:
            lines.append(f"    [[maybe_unused]] std::uint8_t m_pad_class[0x{delta_size:X}]; // Storage extension past {base_name} (0x{base_size:X} -> {size_hex})")
        else:
            lines.append(f"    [[maybe_unused]] std::uint8_t m_pad_class[0x{delta_size:X}]; // Full class memory storage ({size_hex} bytes)")

    lines.append("};")
    lines.append("")
    lines.append(f'static_assert(sizeof({cls_name}) == {size_hex}, "{cls_name} size must match Source 2 schema exactly");')
    lines.append("")
    lines.append(f"}} // namespace cs2::schema")
    lines.append("")

    return "\n".join(lines)


def generate_schema_types_header() -> str:
    """
    Generates schema_types.h containing core type definitions, forward declarations,
    and the SCHEMA_FIELD accessor macro.
    """
    return """// ============================================================================
// Source 2 Schema Core Types & Field Accessor Macro
// Auto-generated by tools/generate_schema_classes.py
// Standard: ISO C++20 (/std:c++20 /W4 /WX)
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "../entity_handle.h"
#include "../math_types.h"

namespace cs2::schema {

// Import core geometric and handle types
using CEntityHandle = cs2::shared::CEntityHandle;
using Vector3       = cs2::shared::Vector3D;
using QAngle        = cs2::shared::QAngle;

// Forward declarations for pointer targets
class CGameSceneNode;
using CGameSceneNodePtr = CGameSceneNode*;

/**
 * @def SCHEMA_FIELD
 * @brief Zero-overhead C++20 field accessor macro.
 * 
 * Expands to non-const and const member accessors returning lvalue references:
 *   - Non-const: provides mutable lvalue reference to memory at this + offset.
 *   - Const:     provides read-only const lvalue reference.
 * 
 * Uses std::add_pointer_t<std::add_const_t<type>> to seamlessly support both
 * fundamental/value types and pointer types (e.g. CGameSceneNode*) without
 * cv-qualifier conversion conflicts.
 */
#ifndef SCHEMA_FIELD
#define SCHEMA_FIELD(type, name, offset) \\
    [[nodiscard]] std::add_lvalue_reference_t<type> name() noexcept { \\
        return *reinterpret_cast<type*>(reinterpret_cast<uintptr_t>(this) + offset); \\
    } \\
    [[nodiscard]] std::add_lvalue_reference_t<std::add_const_t<type>> name() const noexcept { \\
        return *reinterpret_cast<std::add_pointer_t<std::add_const_t<type>>>(reinterpret_cast<uintptr_t>(this) + offset); \\
    }
#endif

} // namespace cs2::schema
"""


def generate_master_aggregator_header(class_order: List[str]) -> str:
    """
    Generates schema_classes.h master header that includes all schema classes
    in correct topological inheritance order.
    """
    lines = []
    lines.append("// ============================================================================")
    lines.append("// Source 2 Master Schema Classes Aggregator")
    lines.append("// Auto-generated by tools/generate_schema_classes.py")
    lines.append("// Standard: ISO C++20 (/std:c++20 /W4 /WX)")
    lines.append("// ============================================================================")
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include "schema_types.h"')
    lines.append("")
    lines.append("// Topological include hierarchy")
    for cls in class_order:
        lines.append(f'#include "{cls}.h"')
    lines.append("")
    lines.append("namespace cs2::shared {")
    lines.append("    // Expose schema namespace to shared subsystem")
    lines.append("    namespace schema = cs2::schema;")
    lines.append("}")
    lines.append("")

    return "\n".join(lines)


# =============================================================================
# Main Driver
# =============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(description="Generate C++20 Source 2 schema classes from schemas.json profile.")
    parser.add_argument(
        "--schema-json",
        type=Path,
        default=Path("versions/profiles/2026-11-11_26598758/schemas.json"),
        help="Path to input schemas.json profile (default: versions/profiles/2026-11-11_26598758/schemas.json)"
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("game/shared/schema_classes"),
        help="Output directory for generated C++20 headers (default: game/shared/schema_classes)"
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Generate headers for all classes in schemas.json (default: generate key classes + dependency closure)"
    )
    args = parser.parse_args()

    if not args.schema_json.is_file():
        print(f"[ERROR] Schema JSON not found: {args.schema_json}", file=sys.stderr)
        return 1

    print(f"[*] Loading schema profile from {args.schema_json}...")
    with open(args.schema_json, "r", encoding="utf-8") as f:
        schemas: Dict[str, Any] = json.load(f)

    print(f"[+] Loaded {len(schemas)} classes from schema profile.")

    # Determine classes to generate
    if args.all:
        target_names = list(schemas.keys())
    else:
        # Combine key targets and extended targets
        combined = list(dict.fromkeys(KEY_TARGET_CLASSES + EXTENDED_TARGET_CLASSES))
        target_names = [name for name in combined if name in schemas]

    class_order = compute_dependency_closure(schemas, target_names)
    print(f"[+] Computed dependency closure: {len(class_order)} classes in topological order.")

    args.output_dir.mkdir(parents=True, exist_ok=True)

    # 1. Generate schema_types.h
    types_path = args.output_dir / "schema_types.h"
    with open(types_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(generate_schema_types_header())
    print(f"[+] Emitted {types_path.name}")

    # 2. Generate individual class headers
    emitted_count = 0
    for cls_name in class_order:
        header_path = args.output_dir / f"{cls_name}.h"
        header_content = generate_class_header(cls_name, schemas[cls_name], schemas)
        with open(header_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(header_content)
        emitted_count += 1

    print(f"[+] Emitted {emitted_count} class headers.")

    # 3. Generate master aggregator header
    master_path = args.output_dir / "schema_classes.h"
    with open(master_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(generate_master_aggregator_header(class_order))
    print(f"[+] Emitted master aggregator header: {master_path.name}")

    print("\n=================================================================")
    print(" SCHEMA CLASS GENERATION COMPLETE")
    print(f" Target Directory: {args.output_dir}")
    print(f" Total Headers:   {emitted_count + 2} files")
    print("=================================================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
