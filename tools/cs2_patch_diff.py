#!/usr/bin/env python3
"""
cs2_patch_diff.py - Counter-Strike 2 Automated Binary Patch & Differential Analysis Engine

Analyzes, archives, and diffs Source 2 compiled PE binaries across updates:
- SchemaSystem classes, fields, types, and byte offset shifts (+/- bytes)
- Engine Interface version tokens (CreateInterface / InterfaceReg)
- PE export tables, section metrics (.text/.rdata), and SHA-256 hashes
- ConVars and ConCommands (client, server, engine, tier0)
- Automated human-readable Markdown diff reports and JSON summaries

Zero external pip dependencies (Standard Python 3 only).
"""

import os
import sys
import struct
import re
import json
import hashlib
import shutil
import argparse
import time
from datetime import datetime

CORE_BINARIES = [
    "animationsystem.dll", "client.dll", "cs2.exe", "engine2.dll", "filesystem_stdio.dll",
    "host.dll", "inputsystem.dll", "localize.dll", "matchmaking.dll", "materialsystem2.dll",
    "meshsystem.dll", "navsystem.dll", "networksystem.dll", "panorama.dll", "panorama_text_pango.dll",
    "panoramauiclient.dll", "particles.dll", "pulse_system.dll", "rendersystemdx11.dll",
    "rendersystemvulkan.dll", "resourcesystem.dll", "scenefilecache.dll", "scenesystem.dll",
    "schemasystem.dll", "server.dll", "soundsystem.dll", "steamaudio.dll", "steamnetworkingsockets.dll",
    "tier0.dll", "v8system.dll", "vconcomm.dll", "vphysics2.dll", "vscript.dll", "worldrenderer.dll"
]

class PEBinary:
    """Lightweight 64-bit PE Parser for Source 2 binaries."""
    def __init__(self, file_path):
        self.path = file_path
        self.filename = os.path.basename(file_path)
        with open(file_path, "rb") as f:
            self.data = f.read()
        self.file_size = len(self.data)
        self.sha256 = hashlib.sha256(self.data).hexdigest()
        self._parse_headers()

    def _parse_headers(self):
        if len(self.data) < 0x40 or self.data[:2] != b"MZ":
            raise ValueError(f"Not a valid PE binary: {self.filename}")
        self.e_lfanew = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[self.e_lfanew : self.e_lfanew + 4] != b"PE\x00\x00":
            raise ValueError(f"Invalid PE signature in {self.filename}")
        
        self.machine = struct.unpack_from("<H", self.data, self.e_lfanew + 4)[0]
        self.num_sections = struct.unpack_from("<H", self.data, self.e_lfanew + 6)[0]
        self.timestamp = struct.unpack_from("<I", self.data, self.e_lfanew + 8)[0]
        self.opt_hdr_size = struct.unpack_from("<H", self.data, self.e_lfanew + 20)[0]
        
        opt_hdr = self.e_lfanew + 24
        self.magic = struct.unpack_from("<H", self.data, opt_hdr)[0]
        self.image_base = struct.unpack_from("<Q", self.data, opt_hdr + 24)[0]
        self.size_of_image = struct.unpack_from("<I", self.data, opt_hdr + 56)[0]

        # Sections
        sec_off = opt_hdr + self.opt_hdr_size
        self.sections = []
        self.sections_dict = {}
        for i in range(self.num_sections):
            s = self.data[sec_off + i*40 : sec_off + (i+1)*40]
            name = s[:8].rstrip(b"\x00").decode("latin1", errors="ignore")
            vsize, rva, rsize, roff = struct.unpack_from("<IIII", s, 8)
            sec_bytes = self.data[roff : roff + rsize]
            sec_hash = hashlib.sha256(sec_bytes).hexdigest()[:16] if rsize > 0 else ""
            entry = {
                "name": name,
                "vsize": vsize,
                "rva": rva,
                "rsize": rsize,
                "roff": roff,
                "sha256_short": sec_hash
            }
            self.sections.append(entry)
            self.sections_dict[name] = entry

        # Export Directory Data Directory (offset 112 in optional header for PE32+)
        self.export_rva, self.export_size = struct.unpack_from("<II", self.data, opt_hdr + 112)

    def rva_to_off(self, rva):
        for s in self.sections:
            if s["rva"] <= rva < s["rva"] + s["vsize"]:
                off = s["roff"] + (rva - s["rva"])
                if off < len(self.data):
                    return off
        return None

    def va_to_off(self, va):
        return self.rva_to_off(va - self.image_base)

    def off_to_va(self, off):
        for s in self.sections:
            if s["roff"] <= off < s["roff"] + s["rsize"]:
                return self.image_base + s["rva"] + (off - s["roff"])
        return None

    def read_str(self, va, max_len=128):
        off = self.va_to_off(va)
        if off is None or off >= len(self.data):
            return ""
        end = self.data.find(b"\x00", off)
        if end == -1 or end - off > max_len:
            return ""
        return self.data[off:end].decode("latin1", errors="ignore")

    def get_exports(self):
        """Extract PE exported functions."""
        if not self.export_rva or not self.export_size:
            return []
        exp_off = self.rva_to_off(self.export_rva)
        if not exp_off or exp_off + 40 > len(self.data):
            return []

        num_funcs, num_names, func_rva, name_rva, ord_rva = struct.unpack_from("<IIIII", self.data, exp_off + 20)
        names_off = self.rva_to_off(name_rva)
        funcs_off = self.rva_to_off(func_rva)
        ords_off = self.rva_to_off(ord_rva)

        if not names_off or not funcs_off or not ords_off:
            return []

        exports = []
        for i in range(min(num_names, 4096)):
            n_rva = struct.unpack_from("<I", self.data, names_off + i*4)[0]
            n_off = self.rva_to_off(n_rva)
            if not n_off: continue
            end = self.data.find(b"\x00", n_off)
            name = self.data[n_off:end].decode("latin1", errors="ignore")
            
            ord_val = struct.unpack_from("<H", self.data, ords_off + i*2)[0]
            fn_rva = struct.unpack_from("<I", self.data, funcs_off + ord_val*4)[0]
            exports.append({
                "name": name,
                "rva": f"0x{fn_rva:x}",
                "va": f"0x{self.image_base + fn_rva:x}",
                "ordinal": ord_val
            })
        return exports

    def get_interfaces(self):
        """Extract CreateInterface registered interfaces."""
        interfaces = []
        # Find CreateInterface export or InterfaceReg occurrences
        # InterfaceReg: struct { void* CreateFn; const char* m_pName; InterfaceReg* m_pNext; }
        # Name strings usually match [A-Za-z0-9_]{3,48}[0-9]{3}
        if_re = re.compile(rb"([A-Za-z0-9_]{3,48}\d{3})\x00")
        for match in if_re.finditer(self.data):
            str_off = match.start(1)
            str_va = self.off_to_va(str_off)
            if not str_va: continue
            token = match.group(1).decode("latin1", errors="ignore")

            # Check if this string pointer exists in an InterfaceReg structure
            pat = struct.pack("<Q", str_va)
            pos = 0
            while True:
                pos = self.data.find(pat, pos)
                if pos == -1: break
                reg_off = pos - 8
                if reg_off >= 0 and reg_off + 24 <= len(self.data):
                    fn_ptr, name_ptr, next_ptr = struct.unpack_from("<QQQ", self.data, reg_off)
                    if name_ptr == str_va and (fn_ptr > self.image_base or fn_ptr == 0):
                        interfaces.append({
                            "token": token,
                            "fn_va": f"0x{fn_ptr:x}",
                            "binary": self.filename
                        })
                        break
                pos += 8
        return sorted(interfaces, key=lambda x: x["token"])

    def get_cvars(self):
        """Extract ConVars and ConCommands by string pattern scanning."""
        cvar_re = re.compile(rb"(?:sv_|cl_|net_|fps_|r_|snd_|host_|mp_|rate\b)([A-Za-z0-9_]{3,36})\x00")
        cvars = set()
        for match in cvar_re.finditer(self.data):
            full_name = match.group(0)[:-1].decode("latin1", errors="ignore")
            # Filter non-cvar strings
            if not any(c in full_name for c in [" ", "%", "/", "\\", "(", ")"]):
                cvars.add(full_name)
        return sorted(list(cvars))

    def get_schemas(self):
        """Extract SchemaClassInfoData_t classes and member fields."""
        ident_re = re.compile(r"^[A-Za-z_][A-Za-z0-9_:]*$")
        classes = {}

        for s in self.sections:
            if s["name"] not in [".data", ".rdata"]: continue
            sec_data = self.data[s["roff"] : s["roff"] + s["rsize"]]
            for i in range(0, len(sec_data) - 0x40, 8):
                name_va = struct.unpack_from("<Q", sec_data, i + 8)[0]
                size = struct.unpack_from("<I", sec_data, i + 0x20)[0]
                num_fields = struct.unpack_from("<H", sec_data, i + 0x24)[0]
                fields_va = struct.unpack_from("<Q", sec_data, i + 0x30)[0]
                base_va = struct.unpack_from("<Q", sec_data, i + 0x38)[0]

                if 0 < size < 3000000 and 0 < num_fields < 500:
                    cname = self.read_str(name_va)
                    if cname and len(cname) > 2 and ident_re.match(cname):
                        f_off = self.va_to_off(fields_va)
                        if f_off and f_off + num_fields * 0x20 <= len(self.data):
                            # Validate first field name
                            fn1_va = struct.unpack_from("<Q", self.data, f_off)[0]
                            fn1 = self.read_str(fn1_va)
                            if fn1 and ident_re.match(fn1):
                                # Base class name
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

                                # Extract all fields
                                fields_list = []
                                for fi in range(num_fields):
                                    chunk = self.data[f_off + fi*0x20 : f_off + (fi+1)*0x20]
                                    fn_va = struct.unpack_from("<Q", chunk, 0)[0]
                                    ft_va = struct.unpack_from("<Q", chunk, 8)[0]
                                    f_offset = struct.unpack_from("<I", chunk, 0x10)[0]
                                    fname = self.read_str(fn_va)
                                    ftype = ""
                                    t_off = self.va_to_off(ft_va)
                                    if t_off:
                                        tname_va = struct.unpack_from("<Q", self.data, t_off + 8)[0]
                                        ftype = self.read_str(tname_va)
                                    
                                    fields_list.append({
                                        "name": fname,
                                        "type": ftype or "void*",
                                        "offset": f_offset,
                                        "offset_hex": f"+0x{f_offset:04x}"
                                    })

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


# ============================================================================
# Archiving & Profiling
# ============================================================================
def create_profile(source_dir, output_profile_dir, tag, build_id="", date="", update_db=True):
    """Scans all CS2 binaries in source_dir and produces a complete profile JSON directory."""
    os.makedirs(output_profile_dir, exist_ok=True)
    t0 = time.time()
    print(f"[*] Profiling Source 2 Binaries from: {source_dir}")
    print(f"[*] Tag: {tag} | Build ID: {build_id or 'unknown'} | Date: {date or 'unknown'}")

    manifest = {
        "tag": tag,
        "build_id": build_id or tag,
        "date": date or datetime.now().strftime("%Y-%m-%d"),
        "created_at": datetime.now().isoformat(),
        "binaries": {}
    }

    all_schemas = {}
    all_interfaces = []
    all_cvars = {}
    all_exports = {}
    pe_summaries = {}

    binaries_found = 0
    for fname in sorted(os.listdir(source_dir)):
        if not (fname.lower().endswith(".dll") or fname.lower().endswith(".exe")):
            continue
        full_path = os.path.join(source_dir, fname)
        try:
            pe = PEBinary(full_path)
            binaries_found += 1
            print(f"  -> Parsing {fname} ({pe.file_size / (1024*1024):.2f} MB)...", end="", flush=True)

            manifest["binaries"][fname] = {
                "size": pe.file_size,
                "sha256": pe.sha256,
                "timestamp": pe.timestamp,
                "sections": {s["name"]: s["vsize"] for s in pe.sections}
            }

            pe_summaries[fname] = {
                "size": pe.file_size,
                "image_base": f"0x{pe.image_base:x}",
                "sections": pe.sections
            }

            # Exports
            exps = pe.get_exports()
            if exps:
                all_exports[fname] = exps

            # Interfaces
            ifs = pe.get_interfaces()
            if ifs:
                all_interfaces.extend(ifs)

            # ConVars
            cvs = pe.get_cvars()
            if cvs:
                all_cvars[fname] = cvs

            # Schemas (especially client.dll, server.dll, particles.dll, etc.)
            schemas = pe.get_schemas()
            if schemas:
                print(f" [{len(schemas)} schemas]", end="", flush=True)
                for cname, cdata in schemas.items():
                    all_schemas[cname] = cdata

            print(" [OK]")

        except Exception as e:
            print(f" [Skipped: {e}]")

    # Write JSON files
    with open(os.path.join(output_profile_dir, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    with open(os.path.join(output_profile_dir, "schemas.json"), "w", encoding="utf-8") as f:
        json.dump(all_schemas, f, indent=2)

    with open(os.path.join(output_profile_dir, "interfaces.json"), "w", encoding="utf-8") as f:
        json.dump(all_interfaces, f, indent=2)

    with open(os.path.join(output_profile_dir, "cvars.json"), "w", encoding="utf-8") as f:
        json.dump(all_cvars, f, indent=2)

    with open(os.path.join(output_profile_dir, "exports.json"), "w", encoding="utf-8") as f:
        json.dump(all_exports, f, indent=2)

    with open(os.path.join(output_profile_dir, "pe_summary.json"), "w", encoding="utf-8") as f:
        json.dump(pe_summaries, f, indent=2)

    # Function Signatures & Offsets Scan
    sig_db_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "function_signatures.json")
    all_signatures = []
    if os.path.exists(sig_db_path):
        try:
            tools_dir = os.path.dirname(os.path.abspath(__file__))
            if tools_dir not in sys.path:
                sys.path.insert(0, tools_dir)
            from scan_signatures import scan_directory, update_signatures_db
            sig_report = scan_directory(source_dir, sig_db_path)
            all_signatures = sig_report.get("signatures", [])
            with open(os.path.join(output_profile_dir, "signatures.json"), "w", encoding="utf-8") as f:
                json.dump(sig_report, f, indent=2)
            print(f"    - Function Signatures: {len(all_signatures)} scanned ({sig_report['metrics']['valid_unique']} unique verified)")

            # Automatically synchronize verified RVAs, VAs, deltas, and status into data/function_signatures.json
            if update_db:
                up_count, _ = update_signatures_db(source_dir, sig_db_path, report=sig_report, build_id=build_id or tag)
                print(f"    - Function Signatures DB: Updated {up_count} entries in data/function_signatures.json (Build: {build_id or tag})")
        except Exception as e:
            print(f"    - Function Signatures: scan/update skipped ({e})")

    elapsed = time.time() - t0
    print(f"\n[+] Profile successfully generated in {elapsed:.2f}s!")
    print(f"    - Binaries Profiled: {binaries_found}")
    print(f"    - Schema Classes:    {len(all_schemas)}")
    print(f"    - Engine Interfaces: {len(all_interfaces)}")
    print(f"    - ConVars Tracked:   {sum(len(v) for v in all_cvars.values())}")
    print(f"    - Output Directory:  {output_profile_dir}")
    return manifest


def archive_version(source_dir, archive_root, profiles_root, tag, build_id="", date="", update_db=True):
    """Copies source binaries to archive/<tag>/ and generates profile in profiles/<tag>/."""
    archive_dir = os.path.join(archive_root, tag)
    profile_dir = os.path.join(profiles_root, tag)
    os.makedirs(archive_dir, exist_ok=True)

    print(f"[*] Archiving binaries into: {archive_dir}")
    copied = 0
    target_names = {b.lower(): b for b in CORE_BINARIES}

    # Support both flat directory and full game directory hierarchy (game/bin/win64, game/csgo/bin/win64)
    for root, _, files in os.walk(source_dir):
        for fname in files:
            fl = fname.lower()
            if fl in target_names:
                canonical_name = target_names[fl]
                src = os.path.join(root, fname)
                dst = os.path.join(archive_dir, canonical_name)
                if not os.path.exists(dst):
                    shutil.copy2(src, dst)
                    copied += 1
                    print(f"  -> Archived {canonical_name}")

    print(f"[+] Archived {copied} core binaries.")

    # Now build profile from the archived folder
    return create_profile(archive_dir, profile_dir, tag, build_id, date, update_db=update_db)


# ============================================================================
# Differential Analysis Engine
# ============================================================================
def diff_profiles(old_profile_dir, new_profile_dir, output_dir=None):
    """Compares two profiled versions and produces structured JSON and Markdown diffs."""
    print(f"[*] Diffing Source 2 Updates:")
    print(f"    Base Profile:   {old_profile_dir}")
    print(f"    Target Profile: {new_profile_dir}")

    # Load manifests
    with open(os.path.join(old_profile_dir, "manifest.json"), "r", encoding="utf-8") as f:
        old_mani = json.load(f)
    with open(os.path.join(new_profile_dir, "manifest.json"), "r", encoding="utf-8") as f:
        new_mani = json.load(f)

    # Load schemas
    with open(os.path.join(old_profile_dir, "schemas.json"), "r", encoding="utf-8") as f:
        old_schemas = json.load(f)
    with open(os.path.join(new_profile_dir, "schemas.json"), "r", encoding="utf-8") as f:
        new_schemas = json.load(f)

    # Load interfaces
    with open(os.path.join(old_profile_dir, "interfaces.json"), "r", encoding="utf-8") as f:
        old_ifs = {item["token"]: item for item in json.load(f)}
    with open(os.path.join(new_profile_dir, "interfaces.json"), "r", encoding="utf-8") as f:
        new_ifs = {item["token"]: item for item in json.load(f)}

    # Load cvars
    with open(os.path.join(old_profile_dir, "cvars.json"), "r", encoding="utf-8") as f:
        old_cvars_dict = json.load(f)
    with open(os.path.join(new_profile_dir, "cvars.json"), "r", encoding="utf-8") as f:
        new_cvars_dict = json.load(f)

    old_cvars = set()
    for cvlist in old_cvars_dict.values(): old_cvars.update(cvlist)
    new_cvars = set()
    for cvlist in new_cvars_dict.values(): new_cvars.update(cvlist)

    # 1. Binary Size & Hash Diff
    bin_diffs = []
    all_bins = sorted(set(list(old_mani["binaries"].keys()) + list(new_mani["binaries"].keys())))
    for b in all_bins:
        old_b = old_mani["binaries"].get(b)
        new_b = new_mani["binaries"].get(b)
        if not old_b:
            bin_diffs.append({"binary": b, "status": "ADDED", "old_size": 0, "new_size": new_b["size"], "delta": new_b["size"]})
        elif not new_b:
            bin_diffs.append({"binary": b, "status": "REMOVED", "old_size": old_b["size"], "new_size": 0, "delta": -old_b["size"]})
        else:
            delta = new_b["size"] - old_b["size"]
            hash_changed = old_b["sha256"] != new_b["sha256"]
            if delta != 0 or hash_changed:
                bin_diffs.append({
                    "binary": b,
                    "status": "MODIFIED",
                    "old_size": old_b["size"],
                    "new_size": new_b["size"],
                    "delta": delta,
                    "delta_kb": round(delta / 1024, 2)
                })

    # 2. Schema Diff (Classes & Shifted Offsets)
    added_classes = []
    removed_classes = []
    modified_classes = []

    for cname, new_cls in new_schemas.items():
        if cname not in old_schemas:
            added_classes.append(new_cls)
        else:
            old_cls = old_schemas[cname]
            size_delta = new_cls["size"] - old_cls["size"]

            # Map fields by name
            old_f_map = {f["name"]: f for f in old_cls["fields"]}
            new_f_map = {f["name"]: f for f in new_cls["fields"]}

            added_fields = [f for fn, f in new_f_map.items() if fn not in old_f_map]
            removed_fields = [f for fn, f in old_f_map.items() if fn not in new_f_map]

            shifted_fields = []
            type_changed_fields = []

            for fn, nf in new_f_map.items():
                if fn in old_f_map:
                    of = old_f_map[fn]
                    offset_diff = nf["offset"] - of["offset"]
                    if offset_diff != 0:
                        shifted_fields.append({
                            "field": fn,
                            "type": nf["type"],
                            "old_offset": of["offset"],
                            "new_offset": nf["offset"],
                            "old_hex": of["offset_hex"],
                            "new_hex": nf["offset_hex"],
                            "shift": offset_diff,
                            "shift_str": f"{'+' if offset_diff > 0 else ''}{offset_diff} bytes"
                        })
                    if nf["type"] != of["type"]:
                        type_changed_fields.append({
                            "field": fn,
                            "old_type": of["type"],
                            "new_type": nf["type"]
                        })

            if size_delta != 0 or added_fields or removed_fields or shifted_fields or type_changed_fields:
                modified_classes.append({
                    "class": cname,
                    "binary": new_cls["binary"],
                    "size_delta": size_delta,
                    "old_size": old_cls["size"],
                    "new_size": new_cls["size"],
                    "added_fields": added_fields,
                    "removed_fields": removed_fields,
                    "shifted_fields": shifted_fields,
                    "type_changed_fields": type_changed_fields
                })

    for cname, old_cls in old_schemas.items():
        if cname not in new_schemas:
            removed_classes.append(old_cls)

    # 3. Interface Diff
    added_ifs = [v for k, v in new_ifs.items() if k not in old_ifs]
    removed_ifs = [v for k, v in old_ifs.items() if k not in new_ifs]

    # 4. ConVar Diff
    added_cvars = sorted(list(new_cvars - old_cvars))
    removed_cvars = sorted(list(old_cvars - new_cvars))

    # 5. Function Signatures & RVAs Diff
    signatures_diff = []
    old_sig_path = os.path.join(old_profile_dir, "signatures.json")
    new_sig_path = os.path.join(new_profile_dir, "signatures.json")
    if os.path.exists(old_sig_path) and os.path.exists(new_sig_path):
        try:
            with open(old_sig_path, "r", encoding="utf-8") as f:
                old_sigs = {s["id"]: s for s in json.load(f).get("signatures", [])}
            with open(new_sig_path, "r", encoding="utf-8") as f:
                new_sigs = {s["id"]: s for s in json.load(f).get("signatures", [])}

            for sig_id, s_old in old_sigs.items():
                s_new = new_sigs.get(sig_id, {})
                rva_old = s_old.get("current_rva")
                rva_new = s_new.get("current_rva")
                va_old = s_old.get("current_va")
                va_new = s_new.get("current_va")
                if rva_old and rva_new:
                    delta = int(rva_new, 16) - int(rva_old, 16)
                    status = "UNCHANGED" if delta == 0 else "SHIFTED"
                    delta_str = f"+0x{delta:x}" if delta > 0 else (f"-0x{-delta:x}" if delta < 0 else "+0x0")
                elif rva_old and not rva_new:
                    status = "BROKEN"
                    delta = None
                    delta_str = "BROKEN"
                elif not rva_old and rva_new:
                    status = "RESTORED"
                    delta = None
                    delta_str = "NEW"
                else:
                    status = "UNRESOLVED"
                    delta = None
                    delta_str = "N/A"

                signatures_diff.append({
                    "id": sig_id,
                    "name": s_old.get("name"),
                    "binary": s_old.get("binary"),
                    "category": s_old.get("category"),
                    "status": status,
                    "baseline_rva": s_old.get("baseline_rva"),
                    "old_rva": rva_old,
                    "new_rva": rva_new,
                    "old_va": va_old,
                    "new_va": va_new,
                    "delta_bytes": delta,
                    "delta_str": delta_str,
                    "description": s_old.get("description", "")
                })
        except Exception as e:
            print(f"Warning: Signature diffing failed: {e}")

    # Compile diff report object
    diff_data = {
        "old_tag": old_mani["tag"],
        "new_tag": new_mani["tag"],
        "old_build_id": old_mani.get("build_id", old_mani["tag"]),
        "new_build_id": new_mani.get("build_id", new_mani["tag"]),
        "old_date": old_mani.get("date", ""),
        "new_date": new_mani.get("date", ""),
        "generated_at": datetime.now().isoformat(),
        "summary": {
            "binaries_modified_count": len([b for b in bin_diffs if b["status"] == "MODIFIED"]),
            "classes_added_count": len(added_classes),
            "classes_removed_count": len(removed_classes),
            "classes_modified_count": len(modified_classes),
            "fields_shifted_total": sum(len(c["shifted_fields"]) for c in modified_classes),
            "interfaces_added_count": len(added_ifs),
            "interfaces_removed_count": len(removed_ifs),
            "cvars_added_count": len(added_cvars),
            "cvars_removed_count": len(removed_cvars),
            "signatures_total_count": len(signatures_diff),
            "signatures_shifted_count": len([s for s in signatures_diff if s["status"] == "SHIFTED"]),
            "signatures_unchanged_count": len([s for s in signatures_diff if s["status"] == "UNCHANGED"]),
            "signatures_broken_count": len([s for s in signatures_diff if s["status"] == "BROKEN"]),
            "signatures_restored_count": len([s for s in signatures_diff if s["status"] == "RESTORED"])
        },
        "binary_diffs": bin_diffs,
        "schemas": {
            "added_classes": added_classes,
            "removed_classes": removed_classes,
            "modified_classes": modified_classes
        },
        "interfaces": {
            "added": added_ifs,
            "removed": removed_ifs
        },
        "cvars": {
            "added": added_cvars,
            "removed": removed_cvars
        },
        "signatures": signatures_diff
    }

    # Generate Markdown Report
    md_report = generate_markdown_report(diff_data)

    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
        json_path = os.path.join(output_dir, f"diff_{diff_data['old_tag']}_vs_{diff_data['new_tag']}.json")
        md_path = os.path.join(output_dir, f"diff_{diff_data['old_tag']}_vs_{diff_data['new_tag']}.md")

        with open(json_path, "w", encoding="utf-8") as f:
            json.dump(diff_data, f, indent=2)

        with open(md_path, "w", encoding="utf-8") as f:
            f.write(md_report)

        print(f"\n[+] Diff completed and written to:")
        print(f"    JSON: {json_path}")
        print(f"    Markdown: {md_path}")

    return diff_data, md_report


def generate_markdown_report(diff):
    """Formats the differential analysis into a GitHub-style markdown report."""
    old_t = diff["old_tag"]
    new_t = diff["new_tag"]
    s = diff["summary"]

    lines = []
    lines.append(f"# CS2 Binary Update Differential Analysis")
    lines.append(f"**Baseline:** `{old_t}` ({diff['old_date']}) &rarr; **Target:** `{new_t}` ({diff['new_date']})\n")
    lines.append(f"> **Automated Reverse Engineering Report** generated by `tools/cs2_patch_diff.py`.\n")

    lines.append(f"## 1. High-Level Update Telemetry")
    lines.append(f"| Category | Baseline | Target | Delta | Impact |")
    lines.append(f"| :--- | :--- | :--- | :--- | :--- |")
    lines.append(f"| **Modified Binaries** | - | - | `{s['binaries_modified_count']}` files | Recompiled PE |")
    lines.append(f"| **Schema Classes Added** | - | - | `+{s['classes_added_count']}` classes | New game engine entities |")
    lines.append(f"| **Schema Classes Modified** | - | - | `{s['classes_modified_count']}` classes | Structural layout delta |")
    lines.append(f"| **Field Offset Shifts** | - | - | `{s['fields_shifted_total']}` offsets | SDK offset realignment |")
    lines.append(f"| **Function Signatures Shifted** | - | - | `{s.get('signatures_shifted_count', 0)}` shifted | RVA offset changes |")
    lines.append(f"| **Function Signatures Broken** | - | - | `{s.get('signatures_broken_count', 0)}` broken | Code refactored/changed |")
    lines.append(f"| **ConVars Added** | - | - | `+{s['cvars_added_count']}` cvars | Gameplay/engine tunables |")
    lines.append(f"| **Interface Version Changes** | - | - | `+{s['interfaces_added_count']} / -{s['interfaces_removed_count']}` | Factory export bumps |\n")

    # Binary Table
    lines.append(f"## 2. Recompiled PE Binary Inventory")
    lines.append(f"| Binary Name | Status | Old Size | New Size | Size Delta |")
    lines.append(f"| :--- | :--- | :--- | :--- | :--- |")
    for b in diff["binary_diffs"]:
        d_str = f"{b['delta_kb']:+,.2f} KB" if "delta_kb" in b else f"{b['delta']:+} B"
        lines.append(f"| `{b['binary']}` | **{b['status']}** | {b['old_size']:,} B | {b['new_size']:,} B | `{d_str}` |")
    lines.append("")

    # Schema Offset Shifts
    lines.append(f"## 3. SchemaSystem Offset Shifts & Field Mutations")
    if s["fields_shifted_total"] == 0 and not diff["schemas"]["modified_classes"]:
        lines.append(f"*No schema offset shifts detected between these updates.*\n")
    else:
        for c in diff["schemas"]["modified_classes"]:
            if not c["shifted_fields"] and not c["added_fields"] and not c["removed_fields"]:
                continue
            lines.append(f"### `{c['class']}` ({c['binary']})")
            lines.append(f"- **Instance Size:** `{c['old_size']} B` &rarr; `{c['new_size']} B` (`{c['size_delta']:+} B`)")
            
            if c["added_fields"]:
                lines.append(f"- **Added Fields:**")
                for af in c["added_fields"]:
                    lines.append(f"  - `{af['name']}` ({af['type']}) at `{af['offset_hex']}`")

            if c["removed_fields"]:
                lines.append(f"- **Removed Fields:**")
                for rf in c["removed_fields"]:
                    lines.append(f"  - `{rf['name']}` ({rf['type']})")

            if c["shifted_fields"]:
                lines.append(f"- **Shifted Member Offsets:**")
                lines.append(f"  | Field Name | Type | Old Offset | New Offset | Net Shift |")
                lines.append(f"  | :--- | :--- | :--- | :--- | :--- |")
                for sf in c["shifted_fields"]:
                    lines.append(f"  | `{sf['field']}` | `{sf['type']}` | `{sf['old_hex']}` | `{sf['new_hex']}` | `{sf['shift_str']}` |")
            lines.append("")

    # ConVars
    if diff["cvars"]["added"]:
        lines.append(f"## 4. Newly Introduced ConVars & Commands")
        for cv in diff["cvars"]["added"]:
            lines.append(f"- `{cv}`")
        lines.append("")

    # Interfaces
    if diff["interfaces"]["added"]:
        lines.append(f"## 5. Engine Factory Interface Changes")
        for iface in diff["interfaces"]["added"]:
            lines.append(f"- Added: `{iface['token']}` in `{iface['binary']}`")
        lines.append("")

    # Function Signatures Diff
    if diff.get("signatures"):
        lines.append(f"## 6. Canonical Function Signatures & Virtual Addresses / RVAs Delta")
        lines.append(f"| Binary | Function Symbol | Status | Baseline RVA | Target RVA | Target Virtual Address (VA) | Shift / Delta |")
        lines.append(f"| :--- | :--- | :--- | :--- | :--- | :--- | :--- |")
        for sig in diff["signatures"]:
            st = sig["status"]
            badge = f"**{st}**" if st != "UNCHANGED" else "UNCHANGED"
            old_r = sig.get("old_rva") or "-"
            new_r = sig.get("new_rva") or "-"
            new_va = sig.get("new_va") or "-"
            lines.append(f"| `{sig['binary']}` | `{sig['name']}` | {badge} | `{old_r}` | `{new_r}` | `{new_va}` | `{sig['delta_str']}` |")
        lines.append("")

    return "\n".join(lines)


# ============================================================================
# CLI Command Dispatcher
# ============================================================================
def main():
    parser = argparse.ArgumentParser(description="Counter-Strike 2 Automated Binary Patch & Differential Analysis Engine")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # Archive command
    p_arch = subparsers.add_parser("archive", help="Archive a game update and generate its profile")
    p_arch.add_argument("--source", required=True, help="Directory containing CS2 DLLs")
    p_arch.add_argument("--tag", required=True, help="Version tag (e.g. 2026-10-15_25966348)")
    p_arch.add_argument("--build-id", default="", help="SteamDB patch / build ID")
    p_arch.add_argument("--date", default="", help="Date of update (YYYY-MM-DD)")
    p_arch.add_argument("--no-update-db", action="store_false", dest="update_db", help="Skip updating data/function_signatures.json")

    # Profile command
    p_prof = subparsers.add_parser("profile", help="Extract profile from a directory without archiving")
    p_prof.add_argument("--source", required=True, help="Directory containing CS2 DLLs")
    p_prof.add_argument("--tag", required=True, help="Version tag")
    p_prof.add_argument("--build-id", default="", help="SteamDB patch / build ID")
    p_prof.add_argument("--date", default="", help="Date of update (YYYY-MM-DD)")
    p_prof.add_argument("--no-update-db", action="store_false", dest="update_db", help="Skip updating data/function_signatures.json")

    # Diff command
    p_diff = subparsers.add_parser("diff", help="Diff two profiled versions")
    p_diff.add_argument("old_tag", help="Baseline version tag or path to profile dir")
    p_diff.add_argument("new_tag", help="Target version tag or path to profile dir")
    p_diff.add_argument("--output", default="versions/diffs", help="Output directory for diffs")

    # Scan-signatures command
    p_sigs = subparsers.add_parser("scan-signatures", help="Scan and synchronize function signatures & virtual addresses across binaries")
    p_sigs.add_argument("--source", help="Directory containing CS2 binaries (defaults to latest archive)")
    p_sigs.add_argument("--no-update-db", action="store_false", dest="update_db", help="Do not update data/function_signatures.json")

    # List command
    p_list = subparsers.add_parser("list", help="List all profiled and archived versions")

    args = parser.parse_args()

    # Base workspace directories
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    versions_root = os.path.join(repo_root, "versions")
    archive_root = os.path.join(versions_root, "archive")
    profiles_root = os.path.join(versions_root, "profiles")
    diffs_root = os.path.join(versions_root, "diffs")

    if args.command == "archive":
        archive_version(args.source, archive_root, profiles_root, args.tag, args.build_id, args.date, update_db=getattr(args, "update_db", True))

    elif args.command == "profile":
        out_dir = os.path.join(profiles_root, args.tag)
        create_profile(args.source, out_dir, args.tag, args.build_id, args.date, update_db=getattr(args, "update_db", True))

    elif args.command == "scan-signatures":
        tools_dir = os.path.dirname(os.path.abspath(__file__))
        if tools_dir not in sys.path:
            sys.path.insert(0, tools_dir)
        from scan_signatures import get_latest_archive_dir, scan_directory, print_scan_report, update_signatures_db
        src = args.source or get_latest_archive_dir()
        sig_db_path = os.path.join(repo_root, "data", "function_signatures.json")
        rep = scan_directory(src, sig_db_path)
        print_scan_report(rep)
        if getattr(args, "update_db", True):
            count, _ = update_signatures_db(src, sig_db_path, report=rep)
            print(f"[+] Successfully updated {count} signatures in {sig_db_path}")

    elif args.command == "diff":
        old_dir = args.old_tag if os.path.isdir(args.old_tag) else os.path.join(profiles_root, args.old_tag)
        new_dir = args.new_tag if os.path.isdir(args.new_tag) else os.path.join(profiles_root, args.new_tag)
        if not os.path.exists(old_dir):
            sys.exit(f"Error: Base profile directory not found: {old_dir}")
        if not os.path.exists(new_dir):
            sys.exit(f"Error: Target profile directory not found: {new_dir}")
        diff_profiles(old_dir, new_dir, args.output)

    elif args.command == "list":
        print("Archived Versions:")
        if os.path.exists(archive_root):
            for d in sorted(os.listdir(archive_root)):
                print(f"  - {d}")
        else:
            print("  (None)")
        print("\nExtracted Profiles:")
        if os.path.exists(profiles_root):
            for d in sorted(os.listdir(profiles_root)):
                m_path = os.path.join(profiles_root, d, "manifest.json")
                if os.path.exists(m_path):
                    with open(m_path, "r", encoding="utf-8") as f:
                        m = json.load(f)
                    print(f"  - {d} (Build {m.get('build_id', 'N/A')}, {m.get('date', 'N/A')}, {len(m.get('binaries', {}))} binaries)")
                else:
                    print(f"  - {d}")
        else:
            print("  (None)")

if __name__ == "__main__":
    main()
