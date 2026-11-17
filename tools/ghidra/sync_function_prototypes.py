#!/usr/bin/env python3
"""
sync_function_prototypes.py - Source 2 Function Prototype Synchronizer for Ghidra

Connects to the active Ghidra instance via GhidraMCP HTTP REST API (port 8089)
to apply typed C++ function signatures and calling conventions (__fastcall)
to all canonical reversed functions across Counter-Strike 2 binaries.

Transforms default Ghidra decompiler views from:
  undefined8 FUN_1808230d0(longlong param_1, undefined4 param_2)
into clean, typed Source 2 signatures:
  void __fastcall C_CSWeaponBase::UpdateAccuracyPenalty(void* this, float flDeltaTime)
"""

import os
import sys
import json
import argparse
import urllib.request
import urllib.parse
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple

DEFAULT_PORT = 8089
DEFAULT_VERSION_FOLDER = "/2026-11-11_26598758"

# Authoritative C++ function prototypes for all 46 canonical signatures
CANONICAL_PROTOTYPES: Dict[str, Dict[str, str]] = {
    # client.dll
    "AngleVectors": {
        "prototype": "void AngleVectors(void* angles, Vector* forward, Vector* right, Vector* up)",
        "convention": "__fastcall",
    },
    "CalculateWorldBones": {
        "prototype": "void CalculateWorldBones(void* this, uint boneMask)",
        "convention": "__fastcall",
    },
    "CTransform::ConcatenateTransforms": {
        "prototype": "void ConcatenateTransforms(void* t1, void* t2, void* out)",
        "convention": "__fastcall",
    },
    "C_CSWeaponBase::UpdateAccuracyPenalty": {
        "prototype": "void UpdateAccuracyPenalty(void* this, float flDeltaTime)",
        "convention": "__fastcall",
    },
    "CPlayer_AimPunchServices::SimulateAimPunchDecay": {
        "prototype": "void SimulateAimPunchDecay(void* this, float flDeltaTime)",
        "convention": "__fastcall",
    },
    "CCSPlayer_MovementServices::WalkMove": {
        "prototype": "void WalkMove(void* this)",
        "convention": "__fastcall",
    },
    "CCSPlayer_MovementServices::AirAccelerate": {
        "prototype": "void AirAccelerate(void* this, Vector* wishdir, float wishspeed, float accel)",
        "convention": "__fastcall",
    },
    "CSubtickMoveStep::_InternalParse": {
        "prototype": "bool _InternalParse(void* this, void* pData, int nSize)",
        "convention": "__fastcall",
    },
    "CBaseUserCmdPB::_InternalSerialize": {
        "prototype": "bool _InternalSerialize(void* this, void* pBuffer, int nCapacity)",
        "convention": "__fastcall",
    },
    "CSGOInput::CreateMove": {
        "prototype": "bool CreateMove(void* this, int nSlot, bool bActive)",
        "convention": "__fastcall",
    },
    "C_CSWeaponBase::GetInaccuracy": {
        "prototype": "float GetInaccuracy(void* this)",
        "convention": "__fastcall",
    },
    "FX_CalculateSpread": {
        "prototype": "void FX_CalculateSpread(void* this, int nSeed, float flInaccuracy, float flSpread, float* pflSpreadX, float* pflSpreadY)",
        "convention": "__fastcall",
    },

    # server.dll
    "LineOfSightThroughSmoke": {
        "prototype": "bool LineOfSightThroughSmoke(Vector* vecStart, Vector* vecEnd, int nDensityThreshold)",
        "convention": "__fastcall",
    },
    "CSmokeGrenadeProjectile::Spawn": {
        "prototype": "void Spawn(void* this)",
        "convention": "__fastcall",
    },
    "CSmokeGrenadeProjectile::TickUpdate": {
        "prototype": "void TickUpdate(void* this, float flCurTime)",
        "convention": "__fastcall",
    },
    "VoxelGrid_Raymarch": {
        "prototype": "bool VoxelGrid_Raymarch(void* pGrid, Vector* vecStart, Vector* vecEnd, RaycastHit_t* pHit)",
        "convention": "__fastcall",
    },
    "VoxelGrid_ExpandAndCarve": {
        "prototype": "void VoxelGrid_ExpandAndCarve(void* pGrid, Vector* vecOrigin, float flRadius, int nCarveMode)",
        "convention": "__fastcall",
    },
    "SerializeSparseDelta": {
        "prototype": "int SerializeSparseDelta(void* pEntity, void* pSnapshot, void* pOutBuffer, int nMaxBytes)",
        "convention": "__fastcall",
    },
    "CLagCompensationManager::StartLagCompensation": {
        "prototype": "void StartLagCompensation(void* this, void* pPlayer, int nTick, float flTargetTime)",
        "convention": "__fastcall",
    },
    "EvaluateBulletPenetration": {
        "prototype": "bool EvaluateBulletPenetration(void* pAttacker, Ray_t* pRay, void* pPenetrationParams, RaycastHit_t* pHit)",
        "convention": "__fastcall",
    },
    "CLagCompensationManager::FinishLagCompensation": {
        "prototype": "void FinishLagCompensation(void* this, void* pPlayer)",
        "convention": "__fastcall",
    },
    "CLagCompensationManager::RestoreEntityTo": {
        "prototype": "void RestoreEntityTo(void* this, void* pEntity, void* pRecord)",
        "convention": "__fastcall",
    },
    "CCSPlayerPawn::ApplyArmorDamageCalculation": {
        "prototype": "float ApplyArmorDamageCalculation(void* this, float flDamage, float flArmorRatio, int nHitGroup, bool* pbArmorAbsorbed)",
        "convention": "__fastcall",
    },
    "CCSPlayerPawn::IsArmored": {
        "prototype": "bool IsArmored(void* this, int nHitGroup)",
        "convention": "__fastcall",
    },
    "CInferno::InfernoThink": {
        "prototype": "void InfernoThink(void* this)",
        "convention": "__fastcall",
    },
    "CInferno::Spread": {
        "prototype": "void Spread(void* this, Vector* vecOrigin, int nClusterIdx)",
        "convention": "__fastcall",
    },
    "CInferno::ExtinguishFire": {
        "prototype": "bool ExtinguishFire(void* this, Vector* vecExtinguishOrigin, float flRadius)",
        "convention": "__fastcall",
    },

    # pulse_system.dll
    "PulseInstructionDispatchLoop": {
        "prototype": "int PulseInstructionDispatchLoop(void* pCursor, int nMaxInstructions)",
        "convention": "__fastcall",
    },

    # tier0.dll
    "CCvar::FindVar": {
        "prototype": "void* FindVar(void* this, char* pszVarName)",
        "convention": "__fastcall",
    },
    "HashStringCaseless": {
        "prototype": "uint HashStringCaseless(char* pszString)",
        "convention": "__fastcall",
    },

    # networksystem.dll
    "ReadVariableBitMessageId": {
        "prototype": "int ReadVariableBitMessageId(void* pBitRead)",
        "convention": "__fastcall",
    },
    "SerializeNetMessage": {
        "prototype": "bool SerializeNetMessage(void* pNetMsg, void* pBitWrite)",
        "convention": "__fastcall",
    },
    "WriteVarInt32": {
        "prototype": "void WriteVarInt32(void* pBuffer, uint nValue)",
        "convention": "__fastcall",
    },
    "ReadVarInt32": {
        "prototype": "uint ReadVarInt32(void* pBuffer, int* pBytesRead)",
        "convention": "__fastcall",
    },
    "DecodeFieldPaths": {
        "prototype": "int DecodeFieldPaths(void* pBitRead, void* pFieldPaths, int nMaxPaths)",
        "convention": "__fastcall",
    },

    # vphysics2.dll
    "ConvexPolyhedronRaycast": {
        "prototype": "bool ConvexPolyhedronRaycast(Ray_t* pRay, void* pPolyhedron, RaycastHit_t* pHit)",
        "convention": "__fastcall",
    },
    "CapsuleSweepIntersection": {
        "prototype": "bool CapsuleSweepIntersection(Ray_t* pRay, float flRadius, RaycastHit_t* pHit)",
        "convention": "__fastcall",
    },

    # animationsystem.dll
    "CTwoBoneIK_LawOfCosines": {
        "prototype": "bool CTwoBoneIK_LawOfCosines(Vector* pStart, Vector* pTarget, float flLen1, float flLen2, Vector* pJointOut)",
        "convention": "__fastcall",
    },
    "CBlendUpdateNode_ComputeWeights": {
        "prototype": "void ComputeWeights(void* this, float flParam, float* pWeightsOut)",
        "convention": "__fastcall",
    },
    "CStateMachineUpdateNode_EvaluateTransitions": {
        "prototype": "int EvaluateTransitions(void* this, void* pContext)",
        "convention": "__fastcall",
    },
    "Quaternion_FromAngleAxis": {
        "prototype": "void Quaternion_FromAngleAxis(void* pOut, float flAngleDegrees, Vector* pAxis)",
        "convention": "__fastcall",
    },

    # navsystem.dll
    "NavAreaBuildPath": {
        "prototype": "bool NavAreaBuildPath(void* pNavMesh, void* pStartArea, void* pEndArea, Vector* pGoalPos, void* pPathOut)",
        "convention": "__fastcall",
    },

    # rendersystemdx11.dll
    "CDX11Device::Present": {
        "prototype": "int Present(void* this, uint nSyncInterval, uint nFlags)",
        "convention": "__fastcall",
    },

    # engine2.dll
    "VProfLite_ParseCommandLineOption": {
        "prototype": "void VProfLite_ParseCommandLineOption(char* pszOption)",
        "convention": "__fastcall",
    },
    "CreateInterface": {
        "prototype": "void* CreateInterface(char* pName, int* pReturnCode)",
        "convention": "__cdecl",
    },

    # steamnetworkingsockets.dll
    "CSteamNetworkConnectionUDP::ProcessPacket": {
        "prototype": "bool ProcessPacket(void* this, void* pPacket, int nBytes)",
        "convention": "__fastcall",
    },
}


def find_repo_root() -> Path:
    current = Path(__file__).resolve().parent
    for p in [current, current.parent, current.parent.parent]:
        if (p / "data" / "function_signatures.json").exists():
            return p
    return Path.cwd()


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

    def post_json(self, endpoint: str, data_dict: dict, timeout=30) -> dict:
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

    def set_prototype(self, address: str, prototype: str, convention: str = "__fastcall", program: str = "") -> dict:
        addr = address if address.startswith("0x") else f"0x{address}"
        payload = {
            "function_address": addr,
            "prototype": prototype,
            "calling_convention": convention,
        }
        if program:
            payload["program"] = program
        return self.post_json("/set_function_prototype", payload)

    def rename_function_by_address(self, address: str, new_name: str, program: str = "") -> dict:
        addr = address if address.startswith("0x") else f"0x{address}"
        payload = {"function_address": addr, "new_name": new_name}
        if program:
            payload["program"] = program
        return self.post_json("/rename_function_by_address", payload)


def sync_prototypes_for_binary(client: GhidraClient, binary_name: str, sig_list: List[dict], folder: str = DEFAULT_VERSION_FOLDER) -> Tuple[int, int]:
    full_path = f"{folder}/{binary_name}"
    print(f"\n[*] Target Binary: {binary_name} ({len(sig_list)} signatures)")
    print(f"[*] Opening {full_path} in Ghidra...")
    open_res = client.open_program(full_path)
    if "error" in open_res:
        print(f"[-] Failed to open {binary_name}: {open_res['error']}")
        return (0, len(sig_list))

    applied = 0
    failed = 0

    for item in sig_list:
        name = item["name"]
        va = item.get("current_va") or item.get("baseline_va")
        proto_info = CANONICAL_PROTOTYPES.get(name)

        if not proto_info:
            print(f"  [?] No prototype defined for {name}, skipping.")
            continue

        proto_str = proto_info["prototype"]
        conv_str = proto_info["convention"]

        # 1. Rename to ensure symbol matches
        client.rename_function_by_address(va, name, program=binary_name)

        # 2. Set prototype
        res = client.set_prototype(va, proto_str, conv_str, program=binary_name)
        res_str = str(res)
        if "Successfully" in res_str:
            print(f"  [+] {name} @ {va} -> {proto_str} [{conv_str}]")
            applied += 1
        else:
            print(f"  [-] Failed {name} @ {va}: {res_str[:120]}")
            failed += 1

    if applied > 0:
        save_res = client.save_program(binary_name)
        print(f"[+] Saved {binary_name} ({applied} prototypes applied)")

    return (applied, failed)


def main():
    parser = argparse.ArgumentParser(description="Source 2 Function Prototype Synchronizer for Ghidra")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="GhidraMCP HTTP port (default: 8089)")
    parser.add_argument("--binary", type=str, default="", help="Specific binary to process (e.g. server.dll)")
    parser.add_argument("--all", action="store_true", help="Process all binaries with canonical signatures")
    parser.add_argument("--folder", type=str, default=DEFAULT_VERSION_FOLDER, help="Project version folder")
    args = parser.parse_args()

    repo_root = find_repo_root()
    sigs_path = repo_root / "data" / "function_signatures.json"
    if not sigs_path.exists():
        print(f"[-] Signature database not found: {sigs_path}")
        sys.exit(1)

    with open(sigs_path, "r", encoding="utf-8") as f:
        signatures = json.load(f)

    client = GhidraClient(port=args.port)
    conn_ok, conn_msg = client.check_connection()
    if not conn_ok:
        print(f"[-] Failed to connect to Ghidra on port {args.port}: {conn_msg}")
        sys.exit(1)

    print(f"[+] Connected to Ghidra: {conn_msg}")

    # Group by binary
    by_binary: Dict[str, List[dict]] = {}
    for s in signatures:
        bin_name = s.get("binary")
        if not bin_name:
            continue
        by_binary.setdefault(bin_name, []).append(s)

    targets = [args.binary] if args.binary else list(by_binary.keys()) if args.all else ["vphysics2.dll"]

    total_applied = 0
    total_failed = 0

    for b in targets:
        if b not in by_binary:
            print(f"[-] Binary {b} has no registered signatures in database")
            continue
        app, fail = sync_prototypes_for_binary(client, b, by_binary[b], args.folder)
        total_applied += app
        total_failed += fail

    print(f"\n=======================================================")
    print(f"  Prototype Synchronization Summary:")
    print(f"  Applied: {total_applied} | Failed: {total_failed} | Total: {total_applied + total_failed}")
    print(f"=======================================================\n")


if __name__ == "__main__":
    main()
