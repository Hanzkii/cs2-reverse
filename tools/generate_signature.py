#!/usr/bin/env python3
"""
generate_signature.py - Source 2 CS2 Automated Unique Binary Signature Generator

Generates minimal, robust, 100% unique IDA/Ghidra byte patterns with wildcard masks
for functions and subroutines in Source 2 compiled PE binaries.

Features:
- Disassembles x86_64 instructions using Capstone.
- Detects and wildcards (? / ??):
    * RIP-relative displacements ([rip + disp32]).
    * Relative near calls (E8 rel32).
    * Relative near jumps (E9 rel32, 0F 8X rel32).
    * Base-relocated absolute pointers and 64-bit immediate addresses.
- Iterative uniqueness engine: expands instruction-by-instruction until exactly
  one unique match exists across the binary.
- Sliding anchor window option if the prologue is generic or ambiguous.
- Emits formatted JSON entries compatible with data/function_signatures.json.
- Verifies patterns against the binary with scan_signatures verification engine.

Usage:
    python tools/generate_signature.py client.dll 0x01739120 --name AngleVectors
    python tools/generate_signature.py versions/archive/2026-11-11_26598758/engine2.dll 0x407190 --name CreateInterface
    python tools/generate_signature.py engine2.dll 0x180407190 --json
"""

import os
import sys
import re
import json
import struct
import argparse
import time

try:
    import capstone
    import capstone.x86 as x86
except ImportError:
    print("Error: capstone disassembly engine is required. Install via 'pip install capstone'.", file=sys.stderr)
    sys.exit(1)

# Ensure parent directory is in sys.path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

from tools.cs2_patch_diff import PEBinary

DEFAULT_SIGNATURES_FILE = os.path.join(PROJECT_ROOT, "data", "function_signatures.json")
ARCHIVE_BASE_DIR = os.path.join(PROJECT_ROOT, "versions", "archive")


def find_latest_archive() -> str:
    """Finds the most recent version archive directory in versions/archive/."""
    if not os.path.isdir(ARCHIVE_BASE_DIR):
        return ""
    dirs = [
        d for d in os.listdir(ARCHIVE_BASE_DIR)
        if os.path.isdir(os.path.join(ARCHIVE_BASE_DIR, d)) and not d.startswith(".")
    ]
    if not dirs:
        return ""
    dirs.sort(reverse=True)
    return dirs[0]


def resolve_binary_path(binary_arg: str, archive_arg: str = "") -> tuple[str, str]:
    """
    Resolves binary path and extracted build_id.
    Returns (clean_path, build_id).
    """
    # 1. Direct file path given
    if os.path.isfile(binary_arg):
        rel_path = os.path.relpath(binary_arg, PROJECT_ROOT).replace("\\", "/")
        parent = os.path.basename(os.path.dirname(binary_arg))
        build_id = parent.split("_")[-1] if "_" in parent else ""
        return binary_arg, build_id

    # 2. Binary name passed with or without archive directory
    archive_dir = archive_arg.strip() if archive_arg else find_latest_archive()
    if archive_dir:
        candidate = os.path.join(ARCHIVE_BASE_DIR, archive_dir, os.path.basename(binary_arg))
        if os.path.isfile(candidate):
            build_id = archive_dir.split("_")[-1] if "_" in archive_dir else archive_dir
            return candidate, build_id

    # 3. Check directly in ARCHIVE_BASE_DIR search
    if os.path.isdir(ARCHIVE_BASE_DIR):
        for arch in sorted(os.listdir(ARCHIVE_BASE_DIR), reverse=True):
            cand = os.path.join(ARCHIVE_BASE_DIR, arch, os.path.basename(binary_arg))
            if os.path.isfile(cand):
                build_id = arch.split("_")[-1] if "_" in arch else arch
                return cand, build_id

    raise FileNotFoundError(f"Could not locate binary '{binary_arg}'. Specify a valid path or archive.")


def get_pe_relocs(pe: PEBinary) -> set[int]:
    """Parses the PE Base Relocation Table (Data Directory 5) into a set of RVAs."""
    try:
        opt_hdr = pe.e_lfanew + 24
        # Reloc directory is offset 112 + 5 * 8 in PE32+
        reloc_rva, reloc_size = struct.unpack_from("<II", pe.data, opt_hdr + 112 + 5 * 8)
        if not reloc_rva or not reloc_size:
            return set()
        reloc_off = pe.rva_to_off(reloc_rva)
        if not reloc_off:
            return set()
        
        relocs = set()
        end_off = reloc_off + reloc_size
        curr = reloc_off
        while curr < end_off:
            page_rva, block_sz = struct.unpack_from("<II", pe.data, curr)
            if block_sz == 0:
                break
            num_entries = (block_sz - 8) // 2
            for i in range(num_entries):
                entry = struct.unpack_from("<H", pe.data, curr + 8 + i * 2)[0]
                reloc_type = entry >> 12
                offset = entry & 0xFFF
                if reloc_type in (3, 10):  # HIGHLOW (3) or DIR64 (10)
                    relocs.add(page_rva + offset)
            curr += block_sz
        return relocs
    except Exception:
        return set()


def mask_instruction(
    insn,
    insn_rva: int,
    relocs: set[int] | None = None,
    mask_rip: bool = True,
    mask_calls: bool = True,
    mask_jumps: bool = True,
    mask_relocs: bool = True,
    mask_all_imms: bool = False
) -> tuple[list[str], list[bool]]:
    """
    Masks volatile displacement and address bytes in an instruction.
    Returns (tokens, mask_flags) where tokens contains hex bytes or '??'.
    """
    enc = insn._raw.detail.contents.arch.x86.encoding
    raw_bytes = list(insn.bytes)
    mask = [False] * len(raw_bytes)

    # 1. RIP-relative displacements (ModR/M rm=101)
    if mask_rip:
        is_rip = any(op.type == x86.X86_OP_MEM and op.mem.base == x86.X86_REG_RIP for op in insn.operands)
        if is_rip and enc.disp_size > 0:
            for i in range(enc.disp_offset, enc.disp_offset + enc.disp_size):
                if i < len(mask):
                    mask[i] = True

    # 2. Relative near calls (E8 rel32)
    if mask_calls:
        if insn.id == x86.X86_INS_CALL:
            if len(insn.operands) > 0 and insn.operands[0].type == x86.X86_OP_IMM:
                if enc.imm_size == 4:
                    for i in range(enc.imm_offset, enc.imm_offset + enc.imm_size):
                        if i < len(mask):
                            mask[i] = True

    # 3. Relative near jumps (E9 rel32, 0F 8X rel32)
    if mask_jumps:
        if insn.group(x86.X86_GRP_JUMP) or insn.mnemonic.startswith("j"):
            if len(insn.operands) > 0 and insn.operands[0].type == x86.X86_OP_IMM:
                if enc.imm_size == 4:
                    for i in range(enc.imm_offset, enc.imm_offset + enc.imm_size):
                        if i < len(mask):
                            mask[i] = True

    # 4. Relocations from PE reloc table
    if mask_relocs and relocs:
        for i in range(len(raw_bytes)):
            if (insn_rva + i) in relocs:
                span = 8 if (i + 8 <= len(mask)) else 4
                for j in range(i, min(len(mask), i + span)):
                    mask[j] = True

    # 5. 64-bit absolute addresses (movabs reg, imm64)
    if enc.imm_size == 8:
        for i in range(enc.imm_offset, enc.imm_offset + enc.imm_size):
            if i < len(mask):
                mask[i] = True

    # 6. Optional all immediates >= 32-bit
    if mask_all_imms and enc.imm_size >= 4:
        for i in range(enc.imm_offset, enc.imm_offset + enc.imm_size):
            if i < len(mask):
                mask[i] = True

    tokens = ["??" if m else f"{b:02X}" for b, m in zip(raw_bytes, mask)]
    return tokens, mask


def pattern_to_regex(pattern_str: str) -> re.Pattern:
    """Converts space-separated hex pattern string to a compiled regex."""
    tokens = pattern_str.strip().split()
    regex_parts = []
    for token in tokens:
        if token in ("?", "??"):
            regex_parts.append(b".")
        else:
            byte_val = bytes([int(token, 16)])
            regex_parts.append(re.escape(byte_val))
    return re.compile(b"".join(regex_parts), re.DOTALL)


def count_pattern_matches(pe: PEBinary, regex: re.Pattern, scan_scope: str = "all") -> list[int]:
    """Finds all match file offsets in the binary."""
    if scan_scope == "text" and ".text" in pe.sections_dict:
        s = pe.sections_dict[".text"]
        text_bytes = pe.data[s["roff"] : s["roff"] + s["rsize"]]
        matches = [s["roff"] + m.start() for m in regex.finditer(text_bytes)]
    else:
        matches = [m.start() for m in regex.finditer(pe.data)]
    return matches


def find_unique_pattern(
    pe: PEBinary,
    target_rva: int,
    relocs: set[int] | None = None,
    min_insns: int = 1,
    max_insns: int = 35,
    min_bytes: int = 0,
    max_bytes: int = 128,
    extra_insns: int = 0,
    search_unique: bool = False,
    max_slide: int = 20,
    anchor_offset: int | None = None,
    mask_rip: bool = True,
    mask_calls: bool = True,
    mask_jumps: bool = True,
    mask_relocs: bool = True,
    mask_all_imms: bool = False,
    scan_scope: str = "all",
) -> dict:
    """
    Iteratively finds the shortest unique pattern starting at target_rva.
    If search_unique is enabled or anchor_offset is specified, it can slide within the function.
    """
    target_off = pe.rva_to_off(target_rva)
    if target_off is None:
        raise ValueError(f"RVA {hex(target_rva)} does not map to a valid file offset in {pe.filename}")

    target_va = pe.image_base + target_rva

    # Disassemble up to 512 bytes from target_rva
    disasm_len = min(512, len(pe.data) - target_off)
    code_bytes = pe.data[target_off : target_off + disasm_len]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    disasm_list = list(md.disasm(code_bytes, target_va))

    if not disasm_list:
        raise ValueError(f"Could not disassemble code at RVA {hex(target_rva)} ({hex(target_va)})")

    # Determine slide start candidates
    if anchor_offset is not None:
        # Find instruction at explicit byte offset
        desired_va = target_va + anchor_offset
        start_indices = []
        for idx, insn in enumerate(disasm_list):
            if insn.address == desired_va:
                start_indices = [idx]
                break
        if not start_indices:
            raise ValueError(f"No instruction aligned with anchor offset +{hex(anchor_offset)} (VA {hex(desired_va)})")
    elif search_unique:
        start_indices = list(range(min(len(disasm_list), max_slide + 1)))
    else:
        start_indices = [0]

    best_result = None

    for start_idx in start_indices:
        insns = disasm_list[start_idx:]
        start_insn = insns[0]
        start_va = start_insn.address
        start_rva = start_va - pe.image_base
        offset_from_target = start_va - target_va

        curr_tokens = []
        curr_insn_meta = []
        curr_rva = start_rva

        for idx, insn in enumerate(insns):
            if idx >= max_insns:
                break

            tokens, mask = mask_instruction(
                insn,
                curr_rva,
                relocs=relocs,
                mask_rip=mask_rip,
                mask_calls=mask_calls,
                mask_jumps=mask_jumps,
                mask_relocs=mask_relocs,
                mask_all_imms=mask_all_imms,
            )

            curr_tokens.extend(tokens)
            curr_insn_meta.append({
                "address": insn.address,
                "mnemonic": insn.mnemonic,
                "op_str": insn.op_str,
                "raw_hex": insn.bytes.hex().upper(),
                "masked_tokens": tokens,
            })
            curr_rva += len(insn.bytes)

            total_insns = idx + 1
            total_bytes = len(curr_tokens)

            if total_bytes > max_bytes:
                break

            if total_insns < min_insns or total_bytes < min_bytes:
                continue

            pattern_str = " ".join(curr_tokens)
            regex = pattern_to_regex(pattern_str)
            matches = count_pattern_matches(pe, regex, scan_scope=scan_scope)

            if len(matches) == 1:
                # Target matched uniquely! Add extra discriminator instructions if requested
                if extra_insns > 0 and (idx + 1 + extra_insns) <= len(insns):
                    for extra_idx in range(idx + 1, idx + 1 + extra_insns):
                        e_insn = insns[extra_idx]
                        e_tokens, _ = mask_instruction(
                            e_insn,
                            curr_rva,
                            relocs=relocs,
                            mask_rip=mask_rip,
                            mask_calls=mask_calls,
                            mask_jumps=mask_jumps,
                            mask_relocs=mask_relocs,
                            mask_all_imms=mask_all_imms,
                        )
                        curr_tokens.extend(e_tokens)
                        curr_insn_meta.append({
                            "address": e_insn.address,
                            "mnemonic": e_insn.mnemonic,
                            "op_str": e_insn.op_str,
                            "raw_hex": e_insn.bytes.hex().upper(),
                            "masked_tokens": e_tokens,
                        })
                        curr_rva += len(e_insn.bytes)
                    pattern_str = " ".join(curr_tokens)
                    total_insns = len(curr_insn_meta)
                    total_bytes = len(curr_tokens)

                num_wildcards = curr_tokens.count("??")
                result = {
                    "pattern": pattern_str,
                    "length_bytes": total_bytes,
                    "num_instructions": total_insns,
                    "num_wildcards": num_wildcards,
                    "anchor_offset": offset_from_target,
                    "matched_rva": hex(start_rva),
                    "matched_va": hex(start_va),
                    "pattern_type": "prologue" if offset_from_target == 0 else "signature",
                    "instructions": curr_insn_meta,
                    "matches_count": 1,
                }
                
                # If offset 0 or shortest found, pick it
                if best_result is None or result["length_bytes"] < best_result["length_bytes"]:
                    best_result = result
                    if offset_from_target == 0:
                        break

    if best_result is not None:
        return best_result

    raise RuntimeError(
        f"Could not generate a unique pattern within {max_insns} instructions ({max_bytes} bytes). "
        f"Try enabling --search-unique, setting --anchor-offset, or relaxing masking options."
    )


def format_signature_entry(
    pe: PEBinary,
    target_rva: int,
    result: dict,
    name: str = "",
    sig_id: str = "",
    category: str = "",
    description: str = "",
    build_id: str = "",
) -> dict:
    """Constructs a JSON dictionary matching data/function_signatures.json schema."""
    stem = os.path.splitext(pe.filename)[0].lower()
    clean_name = name.strip() or f"sub_{target_rva:08X}"
    
    if not sig_id:
        slug = re.sub(r"[^a-zA-Z0-9_]+", "_", clean_name).lower().strip("_")
        sig_id = f"{stem}_{slug}"

    rva_str = f"0x{target_rva:08x}"
    va_str = f"0x{pe.image_base + target_rva:x}"

    entry = {
        "id": sig_id,
        "name": clean_name,
        "binary": pe.filename,
        "category": category.strip() or "Core Engine",
        "baseline_rva": rva_str,
        "baseline_va": va_str,
        "pattern": result["pattern"],
        "pattern_type": result["pattern_type"],
        "description": description.strip() or f"Source 2 disassembled subroutine {clean_name}.",
        "explanation": f"Disassembled instruction routine in {pe.filename} located at baseline RVA {rva_str}.",
        "code_snippet": f"// Reconstructed prototype for {clean_name}\nvoid {clean_name}();",
        "reconstructed_file": f"game/{stem}/{clean_name.lower().replace('::', '_')}.cpp",
        "current_rva": rva_str,
        "current_va": va_str,
        "delta_bytes": 0,
        "delta_str": "+0x0",
        "last_verified_build": build_id or "26598758",
        "status": "UNCHANGED",
    }
    return entry


def update_signatures_db(entry: dict, db_path: str = DEFAULT_SIGNATURES_FILE) -> tuple[bool, str]:
    """Appends or updates the signature entry in data/function_signatures.json."""
    if not os.path.isfile(db_path):
        data = [entry]
        action = "created"
    else:
        with open(db_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        
        found = False
        for idx, item in enumerate(data):
            if item.get("id") == entry["id"]:
                data[idx].update(entry)
                found = True
                action = "updated"
                break
        if not found:
            data.append(entry)
            action = "appended"

    with open(db_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")

    return True, action


def verify_signature(pe: PEBinary, sig: dict) -> dict:
    """Verifies uniqueness and RVA resolution against the PE binary."""
    pattern_str = sig.get("pattern", "")
    regex = pattern_to_regex(pattern_str)
    matches = [m.start() for m in regex.finditer(pe.data)]
    
    baseline_rva = int(sig.get("baseline_rva", "0x0"), 16)
    num_matches = len(matches)

    if num_matches == 1:
        match_va = pe.off_to_va(matches[0])
        match_rva = match_va - pe.image_base if match_va else None
        delta = match_rva - baseline_rva if match_rva is not None else None
        status = "UNCHANGED" if delta == 0 else "SHIFTED"
        return {
            "status": status,
            "matches": 1,
            "resolved_rva": hex(match_rva) if match_rva is not None else None,
            "resolved_va": hex(match_va) if match_va is not None else None,
            "delta": delta,
            "verified": True,
        }
    elif num_matches == 0:
        return {"status": "BROKEN", "matches": 0, "verified": False}
    else:
        return {"status": "AMBIGUOUS", "matches": num_matches, "verified": False}


def main():
    parser = argparse.ArgumentParser(
        description="Source 2 CS2 Automated Unique Binary Signature Generator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("binary", help="Target PE binary path or name (e.g. client.dll, engine2.dll)")
    parser.add_argument("address", help="Target RVA or VA in hex format (e.g. 0x01739120 or 0x181739120)")
    parser.add_argument("-n", "--name", default="", help="Function / symbol name (e.g. AngleVectors)")
    parser.add_argument("-c", "--category", default="", help="Category (e.g. 'Rendering & Math')")
    parser.add_argument("-i", "--id", default="", help="Unique signature identifier")
    parser.add_argument("-d", "--desc", default="", help="Description of subroutine functionality")
    parser.add_argument("--archive", default="", help="Archive directory under versions/archive/")
    parser.add_argument("--min-insns", type=int, default=1, help="Minimum instructions in pattern")
    parser.add_argument("--max-insns", type=int, default=35, help="Maximum instructions to search")
    parser.add_argument("--min-bytes", type=int, default=0, help="Minimum byte length of pattern")
    parser.add_argument("--max-bytes", type=int, default=128, help="Maximum byte length of pattern")
    parser.add_argument("--extra-insns", type=int, default=0, help="Extra discriminator instructions after unique match")
    parser.add_argument("--search-unique", action="store_true", help="Slide window within function if prologue is ambiguous")
    parser.add_argument("--max-slide", type=int, default=20, help="Max instruction slide when --search-unique is active")
    parser.add_argument("--anchor-offset", type=lambda x: int(x, 0), default=None, help="Explicit byte offset from function start")
    parser.add_argument("--no-mask-rip", action="store_true", help="Disable masking of RIP-relative displacements")
    parser.add_argument("--no-mask-calls", action="store_true", help="Disable masking of relative near calls (E8 rel32)")
    parser.add_argument("--no-mask-jumps", action="store_true", help="Disable masking of relative near jumps")
    parser.add_argument("--no-mask-relocs", action="store_true", help="Disable masking of PE base relocations")
    parser.add_argument("--mask-all-imms", action="store_true", help="Mask all immediate values >= 32-bit")
    parser.add_argument("--scan-scope", choices=["all", "text"], default="all", help="Scan scope for uniqueness check")
    parser.add_argument("--json", action="store_true", help="Output JSON format only")
    parser.add_argument("--append-db", action="store_true", help="Append/update entry in data/function_signatures.json")
    parser.add_argument("--no-verify", action="store_true", help="Skip post-generation verification check")

    args = parser.parse_args()

    # 1. Resolve binary
    bin_path, build_id = resolve_binary_path(args.binary, args.archive)
    pe = PEBinary(bin_path)

    # 2. Parse target address (auto-detect VA vs RVA)
    raw_addr = int(args.address, 16)
    if raw_addr >= pe.image_base:
        target_rva = raw_addr - pe.image_base
        target_va = raw_addr
    else:
        target_rva = raw_addr
        target_va = pe.image_base + raw_addr

    # 3. Check for export symbol if name not supplied
    func_name = args.name.strip()
    if not func_name:
        for exp in pe.get_exports():
            exp_rva = int(exp.get("rva", "0x0"), 16) if isinstance(exp.get("rva"), str) else exp.get("rva")
            if exp_rva == target_rva:
                func_name = exp.get("name", "")
                break

    # 4. Parse PE base relocations
    relocs = get_pe_relocs(pe)

    # 5. Iteratively search for unique pattern
    t0 = time.time()
    result = find_unique_pattern(
        pe=pe,
        target_rva=target_rva,
        relocs=relocs,
        min_insns=args.min_insns,
        max_insns=args.max_insns,
        min_bytes=args.min_bytes,
        max_bytes=args.max_bytes,
        extra_insns=args.extra_insns,
        search_unique=args.search_unique,
        max_slide=args.max_slide,
        anchor_offset=args.anchor_offset,
        mask_rip=not args.no_mask_rip,
        mask_calls=not args.no_mask_calls,
        mask_jumps=not args.no_mask_jumps,
        mask_relocs=not args.no_mask_relocs,
        mask_all_imms=args.mask_all_imms,
        scan_scope=args.scan_scope,
    )
    elapsed = time.time() - t0

    # 6. Format JSON entry
    entry = format_signature_entry(
        pe=pe,
        target_rva=target_rva,
        result=result,
        name=func_name,
        sig_id=args.id,
        category=args.category,
        description=args.desc,
        build_id=build_id,
    )

    # 7. Verification check
    verif = None
    if not args.no_verify:
        verif = verify_signature(pe, entry)

    # 8. Append to database if requested
    db_action = None
    if args.append_db:
        _, db_action = update_signatures_db(entry)

    # 9. Output presentation
    if args.json:
        print(json.dumps(entry, indent=2))
        return

    # Relative path for display (Guardrail Rule 1: No absolute paths)
    display_path = os.path.relpath(bin_path, PROJECT_ROOT).replace("\\", "/")

    print("=" * 78)
    print("SOURCE 2 UNIQUE FUNCTION SIGNATURE GENERATOR")
    print("=" * 78)
    print(f"Target Binary:     {pe.filename} ({display_path})")
    print(f"Target Symbol:     {entry['name']}")
    print(f"Address:           RVA 0x{target_rva:08x} | VA 0x{target_va:x}")
    print(f"Build ID:          {entry['last_verified_build']}")
    print(f"Pattern Type:      {result['pattern_type']}")
    if result["anchor_offset"] != 0:
        print(f"Anchor Offset:     +{hex(result['anchor_offset'])} bytes from function entry")
    print(f"Pattern Length:    {result['length_bytes']} bytes ({result['num_instructions']} instructions, {result['num_wildcards']} wildcards)")
    print(f"Scan Duration:     {elapsed:.4f} seconds")
    print("-" * 78)
    print("IDA / Ghidra Pattern:")
    print(f"  {result['pattern']}")
    print("-" * 78)
    print("Disassembled Instruction Chain:")
    for insn_meta in result["instructions"]:
        tokens_str = " ".join(insn_meta["masked_tokens"])
        print(f"  0x{insn_meta['address']:x}:  {tokens_str:<26s}  {insn_meta['mnemonic']:<6s} {insn_meta['op_str']}")
    print("-" * 78)
    if verif:
        verif_status = "PASS (100% Unique Match)" if verif["verified"] else "FAIL"
        print(f"Verification:      {verif_status} [Matches: {verif['matches']}, Status: {verif['status']}]")
    if db_action:
        print(f"Database:          Successfully {db_action} entry '{entry['id']}' in function_signatures.json")
    print("=" * 78)
    print("Generated Schema JSON Entry:")
    print(json.dumps(entry, indent=2))
    print("=" * 78)


if __name__ == "__main__":
    main()
