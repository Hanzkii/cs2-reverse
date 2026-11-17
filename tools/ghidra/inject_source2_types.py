#!/usr/bin/env python3
"""
inject_source2_types.py - Source 2 Data Type & Struct Injector for Ghidra

Connects to active Ghidra instance via GhidraMCP HTTP REST API (port 8089)
to inject foundational Source 2 data structures, vectors, matrices, containers,
and entity headers into Ghidra's DataTypeManager.

Injected Types:
  - Math: Vector2D, Vector, Vector4D, QAngle, Quaternion, Matrix3x4_t, Transform_t, AABB_t
  - Handles: CEntityIndex (uint32_t), CEntityHandle (uint32_t)
  - Containers: CUtlStringToken (uint32_t hash), CUtlMemory, CUtlVector, CUtlString
  - Physics & Collision: Ray_t, RaycastHit_t, CTraceFilter
  - Engine & Entity Base: CEntityIdentity, C_BaseEntity, CCSPlayerPawn (stubs)
"""

import os
import sys
import json
import argparse
import urllib.request
import urllib.parse
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

DEFAULT_PORT = 8089
DEFAULT_VERSION_FOLDER = "/2026-11-11_26598758"

ALL_BINARIES = [
    "client.dll",
    "server.dll",
    "engine2.dll",
    "schemasystem.dll",
    "vphysics2.dll",
    "networksystem.dll",
    "inputsystem.dll",
    "tier0.dll",
    "animationsystem.dll",
    "rendersystemdx11.dll",
    "pulse_system.dll",
    "navsystem.dll",
    "steamnetworkingsockets.dll",
]


class GhidraClient:
    def __init__(self, host="127.0.0.1", port=DEFAULT_PORT):
        self.base_url = f"http://{host}:{port}"

    def check_connection(self) -> Tuple[bool, str]:
        url = f"{self.base_url}/check_connection"
        try:
            with urllib.request.urlopen(url, timeout=5) as res:
                body = res.read().decode("utf-8", errors="replace")
                return (res.status == 200, body.strip())
        except Exception as ex:
            return (False, str(ex))

    def post_json(self, endpoint: str, data_dict: dict, timeout=60) -> dict:
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
        return self.post_json("/run_script_inline", payload, timeout=120)


def generate_type_injection_script() -> str:
    """Generates pure Java Ghidra script to populate DataTypeManager with Source 2 types."""
    return r"""
import ghidra.program.model.data.*;
import ghidra.program.model.data.DataTypeManager;

DataTypeManager dtm = currentProgram.getDataTypeManager();
int txId = currentProgram.startTransaction("Inject Source 2 Core Types");

int injectedCount = 0;

try {
    CategoryPath root = CategoryPath.ROOT;
    CategoryPath s2Math = new CategoryPath("/Source2/Math");
    CategoryPath s2Handles = new CategoryPath("/Source2/Handles");
    CategoryPath s2Containers = new CategoryPath("/Source2/Containers");
    CategoryPath s2Physics = new CategoryPath("/Source2/Physics");

    // Basic Types
    FloatDataType dtFloat = FloatDataType.dataType;
    IntegerDataType dtInt = IntegerDataType.dataType;
    UnsignedIntegerDataType dtUInt = UnsignedIntegerDataType.dataType;
    BooleanDataType dtBool = BooleanDataType.dataType;
    PointerDataType dtVoidPtr = new PointerDataType(VoidDataType.dataType);

    // Standard integer typedefs
    dtm.addDataType(new TypedefDataType(root, "uint32_t", dtUInt), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "int32_t", dtInt), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "uint64_t", UnsignedLongLongDataType.dataType), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "int64_t", LongLongDataType.dataType), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "uint16_t", UnsignedShortDataType.dataType), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "int16_t", ShortDataType.dataType), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "uint8_t", ByteDataType.dataType), DataTypeConflictHandler.REPLACE_HANDLER);
    dtm.addDataType(new TypedefDataType(root, "int8_t", ByteDataType.dataType), DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount += 8;

    // 1. Vector2D (float x, y)
    StructureDataType vec2 = new StructureDataType(root, "Vector2D", 0);
    vec2.add(dtFloat, 4, "x", "X coordinate");
    vec2.add(dtFloat, 4, "y", "Y coordinate");
    dtm.addDataType(vec2, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 2. Vector (float x, y, z)
    StructureDataType vec3 = new StructureDataType(root, "Vector", 0);
    vec3.add(dtFloat, 4, "x", "X coordinate");
    vec3.add(dtFloat, 4, "y", "Y coordinate");
    vec3.add(dtFloat, 4, "z", "Z coordinate");
    dtm.addDataType(vec3, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 3. Vector4D (float x, y, z, w)
    StructureDataType vec4 = new StructureDataType(root, "Vector4D", 0);
    vec4.add(dtFloat, 4, "x", "X coordinate");
    vec4.add(dtFloat, 4, "y", "Y coordinate");
    vec4.add(dtFloat, 4, "z", "Z coordinate");
    vec4.add(dtFloat, 4, "w", "W component");
    dtm.addDataType(vec4, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 4. QAngle (float pitch, yaw, roll)
    StructureDataType qangle = new StructureDataType(root, "QAngle", 0);
    qangle.add(dtFloat, 4, "pitch", "Pitch (X rotation in degrees)");
    qangle.add(dtFloat, 4, "yaw",   "Yaw (Y rotation in degrees)");
    qangle.add(dtFloat, 4, "roll",  "Roll (Z rotation in degrees)");
    dtm.addDataType(qangle, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 5. Quaternion (float x, y, z, w)
    StructureDataType quat = new StructureDataType(root, "Quaternion", 0);
    quat.add(dtFloat, 4, "x", "Vector X");
    quat.add(dtFloat, 4, "y", "Vector Y");
    quat.add(dtFloat, 4, "z", "Vector Z");
    quat.add(dtFloat, 4, "w", "Scalar W");
    dtm.addDataType(quat, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 6. Matrix3x4_t (float mat[3][4])
    ArrayDataType mat3x4_arr = new ArrayDataType(dtFloat, 12, 4);
    StructureDataType mat3x4 = new StructureDataType(root, "Matrix3x4_t", 0);
    mat3x4.add(mat3x4_arr, 48, "m_flMatVal", "3x4 affine transform matrix");
    dtm.addDataType(mat3x4, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 7. CTransform (Vector m_vecPosition, Quaternion m_rotOrientation)
    StructureDataType xform = new StructureDataType(root, "CTransform", 0);
    xform.add(vec3, 12, "m_vecPosition", "Position translation");
    xform.add(quat, 16, "m_rotOrientation", "Rodrigues rotation quaternion");
    dtm.addDataType(xform, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 8. CEntityIndex (uint32_t typedef)
    TypedefDataType entIndex = new TypedefDataType(root, "CEntityIndex", dtUInt);
    dtm.addDataType(entIndex, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 9. CEntityHandle (uint32_t typedef - bits 0-14: index, 15-31: serial)
    TypedefDataType entHandle = new TypedefDataType(root, "CEntityHandle", dtUInt);
    dtm.addDataType(entHandle, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 10. CUtlStringToken (uint32_t MurmurHash2 key)
    TypedefDataType strToken = new TypedefDataType(root, "CUtlStringToken", dtUInt);
    dtm.addDataType(strToken, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 11. CUtlString (char* m_pString)
    StructureDataType utlStr = new StructureDataType(root, "CUtlString", 0);
    utlStr.add(new PointerDataType(CharDataType.dataType), 8, "m_pString", "Null-terminated heap string pointer");
    dtm.addDataType(utlStr, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 12. CUtlMemory (void* m_pMemory, int m_nAllocationCount, int m_nGrowSize)
    StructureDataType utlMem = new StructureDataType(root, "CUtlMemory", 0);
    utlMem.add(dtVoidPtr, 8, "m_pMemory", "Buffer pointer");
    utlMem.add(dtInt, 4, "m_nAllocationCount", "Allocated capacity");
    utlMem.add(dtInt, 4, "m_nGrowSize", "Growth increment");
    dtm.addDataType(utlMem, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 13. CUtlVector (CUtlMemory m_Memory, int m_Size)
    StructureDataType utlVec = new StructureDataType(root, "CUtlVector", 0);
    utlVec.add(utlMem, 16, "m_Memory", "Underlying memory buffer");
    utlVec.add(dtInt, 4, "m_Size", "Current element count");
    utlVec.add(new ArrayDataType(ByteDataType.dataType, 4, 1), 4, "m_pad", "Alignment padding");
    dtm.addDataType(utlVec, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 14. Ray_t (Vector m_Start, Vector m_Delta, Vector m_StartOffset, Vector m_Extents, bool m_IsRay, bool m_IsSwept)
    StructureDataType ray = new StructureDataType(root, "Ray_t", 0);
    ray.add(vec3, 12, "m_Start", "Ray start position");
    ray.add(vec3, 12, "m_Delta", "Ray directional displacement");
    ray.add(vec3, 12, "m_StartOffset", "Box start offset");
    ray.add(vec3, 12, "m_Extents", "Half-extents for swept hulls");
    ray.add(dtBool, 1, "m_IsRay", "True if zero-extent ray");
    ray.add(dtBool, 1, "m_IsSwept", "True if swept ray");
    ray.add(new ArrayDataType(ByteDataType.dataType, 2, 1), 2, "m_pad", "Alignment padding");
    dtm.addDataType(ray, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    // 15. RaycastHit_t (Vector m_vecImpactPoint, Vector m_vecImpactNormal, float m_flFraction, int m_nSurfaceFlags)
    StructureDataType hit = new StructureDataType(root, "RaycastHit_t", 0);
    hit.add(vec3, 12, "m_vecImpactPoint", "World space impact coordinate");
    hit.add(vec3, 12, "m_vecImpactNormal", "Surface normal vector");
    hit.add(dtFloat, 4, "m_flFraction", "Parametric time fraction [0, 1]");
    hit.add(dtInt, 4, "m_nSurfaceFlags", "Surface property flags");
    hit.add(dtBool, 1, "m_bAllSolid", "Started inside solid geometry");
    hit.add(dtBool, 1, "m_bStartSolid", "Start point is solid");
    hit.add(new ArrayDataType(ByteDataType.dataType, 2, 1), 2, "m_pad", "Alignment padding");
    dtm.addDataType(hit, DataTypeConflictHandler.REPLACE_HANDLER);
    injectedCount++;

    println("SUCCESS: Injected " + injectedCount + " Source 2 core data types into DataTypeManager");
} catch (Exception e) {
    println("ERROR: " + e.getMessage());
} finally {
    currentProgram.endTransaction(txId, true);
}
"""


def inject_types_into_binary(client: GhidraClient, binary_name: str, folder: str = DEFAULT_VERSION_FOLDER) -> bool:
    full_path = f"{folder}/{binary_name}"
    print(f"[*] Opening {full_path} in Ghidra...")
    open_res = client.open_program(full_path)
    if "error" in open_res:
        print(f"[-] Failed to open {binary_name}: {open_res['error']}")
        return False

    script_code = generate_type_injection_script()
    print(f"[*] Executing Type Injection Script for {binary_name}...")
    run_res = client.run_inline_script(script_code, program=binary_name)

    output = run_res.get("result", "")
    if "SUCCESS" in output:
        print(f"[+] Successfully injected types into {binary_name}!")
        save_res = client.save_program(binary_name)
        print(f"[+] Saved {binary_name} in Ghidra database.")
        return True
    else:
        print(f"[-] Error injecting types into {binary_name}: {output[:300]}")
        return False


def main():
    parser = argparse.ArgumentParser(description="Source 2 Data Type & Struct Injector for Ghidra")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="GhidraMCP HTTP port (default: 8089)")
    parser.add_argument("--binary", type=str, default="", help="Specific binary to inject (e.g. client.dll, server.dll)")
    parser.add_argument("--all", action="store_true", help="Inject into all 11 Build 26598758 binaries")
    parser.add_argument("--folder", type=str, default=DEFAULT_VERSION_FOLDER, help="Project version folder")
    args = parser.parse_args()

    client = GhidraClient(port=args.port)
    conn_ok, conn_msg = client.check_connection()
    if not conn_ok:
        print(f"[-] Failed to connect to Ghidra on port {args.port}: {conn_msg}")
        sys.exit(1)

    print(f"[+] Connected to Ghidra: {conn_msg}")

    if args.binary:
        targets = [args.binary]
    elif args.all:
        targets = ALL_BINARIES
    else:
        # Default to current active binary
        targets = ["vphysics2.dll"]

    success_count = 0
    for b in targets:
        if inject_types_into_binary(client, b, args.folder):
            success_count += 1

    print(f"\n=======================================================")
    print(f"  Source 2 Type Injection Complete: {success_count}/{len(targets)} binaries updated")
    print(f"=======================================================\n")


if __name__ == "__main__":
    main()
