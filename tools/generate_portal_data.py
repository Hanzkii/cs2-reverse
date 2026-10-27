#!/usr/bin/env python3
"""
generate_portal_data.py
Generates docs/portal-data.js containing:
- CS2_BINARIES_DETAIL: Detailed reverse-engineering information for all 24 binaries.
- CS2_SIMULATORS_DATA: Complete metadata and C++ reference code for all 9 simulators.
"""

import os
import json

SIMULATOR_FILES = {
    "03": "cs2_subtick_usercmd_prediction_sim.cpp",
    "04": "cs2_entity_system_chunk_table_sim.cpp",
    "05": "cs2_ray_trace_shape_sim.cpp",
    "07": "cs2_view_matrix_sim.cpp",
    "08": "cs2_hitbox_bounding_capsule_sim.cpp",
    "09": "cs2_skeleton_animation_sim.cpp",
    "10": "cs2_rubikon_physics_sim.cpp",
    "11": "cs2_volumetric_smoke_sim.cpp",
    "16": "cs2_weapon_spread_recoil_sim.cpp",
}

SIMULATORS_METADATA = [
    {
        "id": "03",
        "name": "cs2_subtick_usercmd_prediction_sim.cpp",
        "title": "Sub-Tick UserCmds & Multi-Slice Prediction",
        "target": "client.dll & server.dll",
        "runner": "build_subtick_sim.bat",
        "category": "Physics & Prediction",
        "badge": "Sub-Tick Input",
        "checks": "Continuous analog sub-tick fraction t in [0.0, 1.0), multi-slice physics splitting around input transitions (+jump), continuous analog jump trajectory, lag-compensated hitbox backtracking.",
        "description": "Simulates continuous temporal slicing in Source 2 physics. Demonstrates that jump commands split a tick into a ground friction slice, instantaneous impulse at t_when, and airborne gravity slice across the 15.625 ms tick interval.",
        "addresses": [
            {"name": "CSGOInput Singleton", "addr": "0x181a4a9b0", "desc": "Sub-tick hardware input accumulator and UserCmd builder"},
            {"name": "CLagCompensationManager::StartLagCompensation", "addr": "0x18080f320", "desc": "Hermite/linear hitbox interpolation at (tick + fraction)"},
            {"name": "sv_jump_impulse", "addr": "ConVar (301.993 u/s)", "desc": "Instantaneous vertical velocity impulse"}
        ],
        "docId": "subtick-prediction"
    },
    {
        "id": "04",
        "name": "cs2_entity_system_chunk_table_sim.cpp",
        "title": "Entity System 2D Chunk Page-Table & Handle Resolution",
        "target": "client.dll (FUN_1807357c0)",
        "runner": "build_entity_chunk_sim.bat",
        "category": "Entities & Memory",
        "badge": "Entity Chunk Table",
        "checks": "64 chunks x 512 entries (32,768 max entities), 15-bit index & 17-bit serial CEntityHandle arithmetic, CEntityIdentity (0x70 bytes) layout, O(1) page-table lookup, stale handle detection and serial invalidation.",
        "description": "Simulates Source 2's CGameEntitySystem 2D chunk page-table resolution. Models bitwise handle decomposition (chunk = index >> 9, slot = index & 0x1FF), CEntityIdentity (0x70 bytes) cacheline inspection, serial increment on deletion, and stale pointer prevention.",
        "addresses": [
            {"name": "CGameEntitySystem Global", "addr": "0x18271c8b8", "desc": "Active entity system singleton in client.dll"},
            {"name": "GetEntityIdentity (Slot 2)", "addr": "0x1807357c0", "desc": "O(1) 2D chunk page-table resolution routine"},
            {"name": "m_EntityChunks Table", "addr": "+0x10", "desc": "Array of 64 pointers to 512-slot CEntityIdentity pages"},
            {"name": "CEntityIdentity Layout", "addr": "0x70 bytes", "desc": "Entity pointer (+0x00), ClassInfo (+0x08), Handle (+0x10), Flags (+0x14)"}
        ],
        "docId": "schemasystem"
    },
    {
        "id": "05",
        "name": "cs2_ray_trace_shape_sim.cpp",
        "title": "CGameTraceManager Spatial Ray Tracing & Smoke Optical Occlusion",
        "target": "client.dll & vphysics2.dll",
        "runner": "build_trace_sim.bat",
        "category": "Physics & Collision",
        "badge": "Spatial Ray Tracing",
        "checks": "Ray_t (64 bytes, alignas(16)), CTraceFilter (72 bytes), GameTrace_t (192 bytes with 40-byte Rubikon sweep payload), CGameTraceManager::TraceShape (VTable Slot 5 @ 0x180a1ae30), LineOfSightThroughSmoke (client.dll + 0x007ce470) with 0.20 optical density gate and dynamic bullet/blast cavity carving.",
        "description": "Simulates Source 2's primary world ray tracing and line-of-sight determination pipeline in client.dll. Reconstructs Ray_t alignment, CTraceFilter entity exclusion rules, GameTrace_t physics payload, and authoritative smoke raymarching with 0.20 optical density threshold.",
        "addresses": [
            {"name": "CGameTraceManager Global", "addr": "0x182229020", "desc": "Active trace manager singleton in client.dll"},
            {"name": "CGameTraceManager::TraceShape", "addr": "0x180a1ae30", "desc": "Slot 5: Master world and entity raycast dispatcher"},
            {"name": "CGameTraceManager::ClipRayToEntity", "addr": "0x180a1b8a0", "desc": "Slot 1: Entity-specific collision boundary clipping"},
            {"name": "LineOfSightThroughSmoke", "addr": "0x1807ce470", "desc": "Authoritative line-of-sight raymarcher with 0.20 density gate"},
            {"name": "CTraceFilter VTable", "addr": "0x181ad4838", "desc": "Spatial filter virtual method table in client.dll"}
        ],
        "docId": "rubikon-physics"
    },
    {
        "id": "07",
        "name": "cs2_view_matrix_sim.cpp",
        "title": "Camera, View Matrix & Recoil Punch Pipeline",
        "target": "client.dll (FUN_1817402a0)",
        "runner": "build_viewmatrix_sim.bat",
        "category": "Math & Rendering",
        "badge": "Camera & ViewMatrix",
        "checks": "AngleVectors degrees-to-rad (0.017453292), 2.0x recoil punch multiplier, 4x4 row-major/column-major VMatrix construction, homogeneous frustum culling (w < 0.001 rejection).",
        "description": "Replicates client.dll's camera projection pipeline. Verifies the exact trigonometric transformations that map 3D world coordinates to 2D screen coordinates, factoring in player eye offset and 2.0x weapon aim punch recoil.",
        "addresses": [
            {"name": "AngleVectors / AngleMatrix", "addr": "0x1817402a0", "desc": "Euler degrees to Forward/Right/Up basis vectors"},
            {"name": "CViewRender::SetUpView", "addr": "0x180c147b0", "desc": "Camera setup and view matrix orchestrator"},
            {"name": "VMatrix Global Array", "addr": "0x18256ad00", "desc": "Active World-to-Projection matrix memory in client.dll"},
            {"name": "m_pAimPunchServices", "addr": "+0x1598", "desc": "Pawn service containing m_aimPunchCache (+0x40)"}
        ],
        "docId": "camera-view-matrix"
    },
    {
        "id": "08",
        "name": "cs2_hitbox_bounding_capsule_sim.cpp",
        "title": "Hitbox Studio Bone OBB & Capsule Raycast Geometry",
        "target": "client.dll & vphysics2.dll",
        "runner": "build_hitbox_sim.bat",
        "category": "Rendering & Geometry",
        "badge": "Hitbox Geometry",
        "checks": "CHitBox (0x70 bytes) and CHitBoxSet (0x30 bytes) struct layout, 3x4 affine bone matrix point transformation, analytical finite ray-capsule cylinder/sphere intersection solver (FUN_18016caf0), hitgroup damage multiplier hierarchy (Head=4.0x, Stomach=1.25x, Legs=0.75x).",
        "description": "Simulates Source 2's skeletal hitbox volumes and bullet raycast intersection. Models 3D capsule mantle and hemispherical endcap analytical ray intersection, bone-to-world coordinate transforms, and authoritative CS2 hitgroup damage multipliers.",
        "addresses": [
            {"name": "CHitBox Definition", "addr": "0x70 bytes", "desc": "Studio bone hitbox bounds, radius (+0x30), bone (+0x34), group (+0x38)"},
            {"name": "CHitBoxSet Definition", "addr": "0x30 bytes", "desc": "Hitbox set array header containing count and CHitBox array pointer"},
            {"name": "RayCapsuleIntersection", "addr": "0x18016caf0", "desc": "vphysics2.dll analytical finite cylinder and spherical endcap solver"},
            {"name": "HitGroup Multiplier Table", "addr": "client.dll", "desc": "Head 4.0x, Stomach 1.25x, Chest 1.0x, Legs 0.75x damage scaling"}
        ],
        "docId": "lag-compensation"
    },
    {
        "id": "09",
        "name": "cs2_skeleton_animation_sim.cpp",
        "title": "Skeleton Bone Hierarchy & AnimGraph Two-Bone IK",
        "target": "client.dll & animationsystem.dll",
        "runner": "build_skeleton_anim_sim.bat",
        "category": "Animation & Kinematics",
        "badge": "Animation & IK",
        "checks": "32-byte CTransform layout, Rodrigues quaternion transform concatenation (FUN_181748480), forward kinematics dirty bitmask (+0x0458), Two-Bone IK Law of Cosines solver (FUN_1802f9750).",
        "description": "Simulates client.dll's CSkeletonInstance bone hierarchy and animationsystem.dll's CTwoBoneIKPoseOperation. Verifies how bone matrices are lazily propagated and analytically solved for limb target reaches.",
        "addresses": [
            {"name": "CSkeletonInstance VTable", "addr": "0x181c41ce0", "desc": "Client skeletal animation instance vtable"},
            {"name": "CalculateWorldBones", "addr": "0x180aa0f80", "desc": "Forward kinematics hierarchical evaluation loop"},
            {"name": "CTransform Concatenate", "addr": "0x181748480", "desc": "Rodrigues SIMD quaternion & translation composer"},
            {"name": "CTwoBoneIKPoseOperation VTable", "addr": "0x180689268", "desc": "Analytical limb IK operator in animationsystem.dll"},
            {"name": "Two-Bone IK Law of Cosines", "addr": "0x1802f9750", "desc": "Analytical joint angle solver: cos(alpha)=(L1^2+d^2-L2^2)/(2*L1*d)"}
        ],
        "docId": "skeleton-animgraph"
    },
    {
        "id": "10",
        "name": "cs2_rubikon_physics_sim.cpp",
        "title": "Rubikon Physics Engine & Collision Detection",
        "target": "vphysics2.dll (0x1803fbf50)",
        "runner": "build_rubikon_sim.bat",
        "category": "Physics & Collision",
        "badge": "Rubikon Physics",
        "checks": "Polyhedral slab-clipping raycast (FUN_1802544a0) with 1/32\" skin tolerance (0.03125f), capsule swept cylinder & hemispherical endcaps (FUN_18016caf0), GJK/EPA support functions (CTraceAABB, CTraceCapsule, CTraceHull), 48-byte SIMD BVH spatial tree.",
        "description": "Simulates Valve's proprietary Rubikon physics engine in vphysics2.dll. Models convex hull half-space ray clipping with exact Source 2 tolerances, capsule sweeps for player hitboxes, and hierarchical BVH spatial acceleration tree traversal.",
        "addresses": [
            {"name": "CVPhysics2Interface VTable", "addr": "0x1803fbf50", "desc": "80-slot IVPhysics2 master interface jump table"},
            {"name": "CVPhys2World VTable", "addr": "0x1803f9b20", "desc": "80-slot IVPhysics2World simulation world jump table"},
            {"name": "Convex Polyhedron Raycast", "addr": "0x1802544a0", "desc": "Polyhedral half-space clipping raycaster with 0.03125f skin tolerance"},
            {"name": "Capsule Sweep Intersection", "addr": "0x18016caf0", "desc": "Analytical finite cylinder & hemispherical endcap solver"},
            {"name": "CTraceAABB Support Function", "addr": "0x1802ea210", "desc": "Branchless sign-bit direction argmax support function"},
            {"name": "CTraceHull Support Function", "addr": "0x1802ea5d0", "desc": "SIMD 4-wide unrolled vertex projection argmax"},
            {"name": "RnCompoundTree BVH Traversal", "addr": "0x18000f4e0", "desc": "Hierarchical 48-byte SIMD spatial tree query"}
        ],
        "docId": "rubikon-physics"
    },
    {
        "id": "11",
        "name": "cs2_volumetric_smoke_sim.cpp",
        "title": "Volumetric Smoke Grenades & 3D Voxel Grid Simulation",
        "target": "server.dll & client.dll",
        "runner": "build_smoke_sim.bat",
        "category": "Volumetric & Particles",
        "badge": "Smoke Volumetrics",
        "checks": "32x32x32 voxel grid (32,768 cells, 20-unit spacing), 3D Morton Order (Z-Order curve) bit-interleaving, double-buffered 512-word uint64 bitmask, authoritative line-of-sight raymarcher with 0.20 density threshold, bullet tunnel carving, HE grenade blast displacement (+30.0 units), 10-byte sparse wire protocol.",
        "description": "Simulates Valve's flagship Source 2 volumetric smoke system. Models 3D Morton-ordered voxel addressing (32x32x32 = 32,768 cells), double-buffered 512-word uint64 bitmasks, authoritative line-of-sight raymarching (0.20 density gate), dynamic bullet/HE grenade void carving, and 10-byte sparse delta wire compression.",
        "addresses": [
            {"name": "CSmokeGrenadeProjectile::Spawn", "addr": "0x1809cb810", "desc": "VTable Slot 9 initializing 3,072-byte buffer and grenade model"},
            {"name": "CSmokeGrenadeProjectile::TickUpdate", "addr": "0x1809cea90", "desc": "Tick generation check and delta serialization invocation"},
            {"name": "LineOfSightThroughSmoke", "addr": "0x1809b7f50", "desc": "Authoritative vision test checking active smokes against 0.20 density gate"},
            {"name": "VoxelGrid_Raymarch", "addr": "0x1809cd500", "desc": "3D Digital Differential Analyzer (DDA) raymarcher over 640x640x640 volume"},
            {"name": "VoxelGrid_ExpandAndCarve", "addr": "0x1809c8e50", "desc": "3D Morton bit-interleaving (Z-Order curve) and radial blast displacement (+30 units)"},
            {"name": "SerializeSparseDelta", "addr": "0x1809b1bd0", "desc": "10-byte sparse record network serializer (2-byte word index + 8-byte bitmask)"}
        ],
        "docId": "volumetric-smoke"
    },
    {
        "id": "16",
        "name": "cs2_weapon_spread_recoil_sim.cpp",
        "title": "Weapon Inaccuracy, Recoil Decay & Dual-Disk Spread RNG",
        "target": "client.dll & server.dll",
        "runner": "build_weapon_spread_sim.bat",
        "category": "Weapons & Ballistics",
        "badge": "Weapon Accuracy & Spread",
        "checks": "Exponential accuracy penalty recovery exp(-(ln10/tau)*dt), 34%-95% movement speed remap with 0.25 crouch power curve, airborne apex penalty, dual uniform disk sampling (FX_CalculateSpread), R8 Revolver (1-r^2) and Negev spin-up laser convergence.",
        "description": "Simulates CS2's weapon accuracy, recoil decay, and dual uniform disk bullet spread mechanics. Models continuous exponential ODE penalty recovery, stance/movement inaccuracy curves, and Park-Miller LCG + 32-entry Bays-Durham shuffle table RNG.",
        "addresses": [
            {"name": "C_CSWeaponBase::UpdateAccuracyPenalty", "addr": "0x180828a10", "desc": "Exponential accuracy decay and movement/stance penalty integrator"},
            {"name": "C_CSWeaponBase::GetInaccuracy", "addr": "0x18080cfd0", "desc": "Composite weapon inaccuracy scalar evaluator in [0.0, 1.0]"},
            {"name": "FX_CalculateSpread", "addr": "0x180d25630", "desc": "Dual uniform disk (inaccuracy + intrinsic spread) 2D deflection sampler"},
            {"name": "CPlayer_AimPunchServices::SimulateAimPunchDecay", "addr": "0x18085ea90", "desc": "Damped spring-mass-damper recoil aim punch integrator"}
        ],
        "docId": "weapon-inaccuracy"
    }
]

BINARIES_DETAIL = [
    {
        "name": "client.dll",
        "cluster": "Gameplay & Entities",
        "size": "39.2 MB",
        "path": "game/csgo/bin/win64/client.dll",
        "status": "Extensively Reversed & Verified",
        "statusColor": "emerald",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Local prediction, HUD view rendering, CSkeletonInstance, camera setup, aim punch recoil injection, UserCmd sampling.",
        "description": "The client-side game logic binary contains the core representation of players, weapons, HUD components, animations, and local prediction. It coordinates input gathering with inputsystem.dll, packages UserCmds into clc_Move, predicts pawn state before receiving server packets, and computes bone transforms for rendering.",
        "addresses": [
            {"symbol": "CSkeletonInstance::vftable", "addr": "0x181c41ce0", "type": "VTable", "desc": "Virtual table for skeletal animation instance containing CalculateWorldBones."},
            {"symbol": "CalculateWorldBones", "addr": "0x180aa0f80", "type": "Function", "desc": "Hierarchical forward kinematics bone transform propagation loop."},
            {"symbol": "CTransform::ConcatenateTransforms", "addr": "0x181748480", "type": "Function", "desc": "SIMD Rodrigues quaternion and translation concatenation."},
            {"symbol": "AngleVectors", "addr": "0x1817402a0", "type": "Function", "desc": "Trigonometric conversion from Euler angles (deg * 0.017453292) to Forward/Right/Up basis."},
            {"symbol": "VMatrix Global Array", "addr": "0x18256ad00", "type": "Global Data", "desc": "Active 4x4 row/column-major view projection matrix array."},
            {"symbol": "CViewRender::SetUpView", "addr": "0x180c147b0", "type": "Function", "desc": "Camera setup, FOV interpolation, and view matrix construction."},
            {"symbol": "CSGOInput", "addr": "0x181a4a9b0", "type": "Global Pointer", "desc": "Input manager capturing raw mouse deltas and building sub-tick UserCmds."}
        ],
        "schemas": [
            {"class": "C_CSPlayerPawn", "field": "m_pGameSceneNode", "offset": "+0x0330", "type": "CGameSceneNode*", "desc": "Spatial scene node for origin, angles, and bone hierarchy."},
            {"class": "C_CSPlayerPawn", "field": "m_pAimPunchServices", "offset": "+0x1598", "type": "CPlayer_AimPunchServices*", "desc": "Weapon recoil punch service containing m_aimPunchCache (+0x40)."},
            {"class": "C_CSPlayerPawn", "field": "m_hController", "offset": "+0x1600", "type": "CEntityHandle", "desc": "32-bit handle linking physical pawn to client player controller."},
            {"class": "C_CSPlayerPawn", "field": "m_iHealth", "offset": "+0x0344", "type": "int32", "desc": "Current player health points."},
            {"class": "C_CSPlayerPawn", "field": "m_lifeState", "offset": "+0x0348", "type": "uint8", "desc": "Life state flag: 0 = Alive, 1 = Dying, 2 = Dead."},
            {"class": "C_CSPlayerPawn", "field": "m_vecViewOffset", "offset": "+0x0f60", "type": "Vector", "desc": "Eye camera offset from base origin (default 64 units standing)."},
            {"class": "C_CSPlayerPawn", "field": "m_angEyeAngles", "offset": "+0x3600", "type": "QAngle", "desc": "Replicated eye viewing angles (pitch, yaw)."},
            {"class": "CSkeletonInstance", "field": "m_modelState", "offset": "+0x0160", "type": "CModelState", "desc": "Model state containing bone array (+0x80) and bone count (+0x94)."},
            {"class": "CSkeletonInstance", "field": "m_nDirtyFlag", "offset": "+0x0458", "type": "uint32", "desc": "Bitmask tracking lazily invalidated bone transforms."}
        ],
        "wireProtocols": [
            {"proto": "CSGOUserCmdPB", "desc": "Sub-tick user command protobuf carrying input history and attack timings."},
            {"proto": "CSubtickMoveStep", "desc": "Discrete button transitions tagged with continuous analog fraction t in [0.0, 1.0)."},
            {"proto": "CEntityHandle", "desc": "32-bit index (15 bits) and serial number (17 bits) entity identifier."}
        ],
        "docs": ["subtick-prediction", "controller-pawn", "camera-view-matrix", "skeleton-animgraph", "weapon-inaccuracy"],
        "simulators": ["03", "07", "09", "16"]
    },
    {
        "name": "server.dll",
        "cluster": "Gameplay & Entities",
        "size": "34.6 MB",
        "path": "game/csgo/bin/win64/server.dll",
        "status": "Extensively Reversed & Verified",
        "statusColor": "emerald",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Authoritative gameplay simulation, bot execution, lag compensation history buffers, damage calculations, CCSPlayerController session management.",
        "description": "The authoritative server-side binary executes the game loop, validates physics movements, resolves weapon hit registration via sub-tick lag compensation, arbitrates damage and health state, and serializes delta entities over network packets.",
        "addresses": [
            {"symbol": "CCSPlayerController::vftable", "addr": "0x1819d44f8", "type": "VTable", "desc": "Virtual table for authoritative player controller."},
            {"symbol": "CLagCompensationManager::StartLagCompensation", "addr": "0x18080f320", "type": "Function", "desc": "Sub-tick matrix interpolator rewinding hitboxes to shot instant."},
            {"symbol": "IGameEventManager2::FireEvent", "addr": "0x1806f12a0", "type": "Function", "desc": "Authoritative game event broadcaster."},
            {"symbol": "CSmokeGrenadeProjectile::vftable", "addr": "0x1818e0e08", "type": "VTable", "desc": "Virtual table for volumetric smoke grenade projectile."},
            {"symbol": "LineOfSightThroughSmoke", "addr": "0x1809b7f50", "type": "Function", "desc": "Authoritative vision test checking active smokes against 0.20 density gate."},
            {"symbol": "CSmokeGrenadeProjectile::TickUpdate", "addr": "0x1809cea90", "type": "Function", "desc": "Tick generation check and delta serialization invocation."}
        ],
        "schemas": [
            {"class": "CCSPlayerController", "field": "m_hPlayerPawn", "offset": "+0x07dc", "type": "CEntityHandle", "desc": "Handle pointing to currently possessed player pawn."},
            {"class": "CCSPlayerController", "field": "m_sSanitizedPlayerName", "offset": "+0x0720", "type": "CUtlSymbolLarge", "desc": "Player username string."},
            {"class": "CCSPlayerController", "field": "m_iPawnHealth", "offset": "+0x0818", "type": "uint32", "desc": "Mirrored health integer for UI scoreboard display."},
            {"class": "CCSPlayerController", "field": "m_iScore", "offset": "+0x0730", "type": "int32", "desc": "Match score displayed on scoreboard."},
            {"class": "CSmokeGrenadeProjectile", "field": "m_vSmokeDetonationPos", "offset": "+0x0b80", "type": "Vector", "desc": "3D world center of the 640x640x640 volumetric bounding volume."},
            {"class": "CSmokeGrenadeProjectile", "field": "m_VoxelFrameData", "offset": "+0x0b90", "type": "CUtlVector<uint8>", "desc": "Serialized 10-byte sparse voxel delta stream."},
            {"class": "CSmokeGrenadeProjectile", "field": "m_nVoxelUpdate", "offset": "+0x0bac", "type": "int32", "desc": "Monotonically increasing voxel generation tick counter."}
        ],
        "wireProtocols": [
            {"proto": "CSVCMsg_PacketEntities", "desc": "Server-to-client delta updates traversed via 40-op Huffman fieldpaths."},
            {"proto": "CSVCMsgList_GameEvents", "desc": "Tick-batched game event wire transmission."}
        ],
        "docs": ["subtick-prediction", "controller-pawn", "game-events", "volumetric-smoke", "weapon-inaccuracy"],
        "simulators": ["03", "11", "16"]
    },
    {
        "name": "engine2.dll",
        "cluster": "Engine Core",
        "size": "6.8 MB",
        "path": "game/bin/win64/engine2.dll",
        "status": "Extensively Reversed & Verified",
        "statusColor": "emerald",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Main engine executive: CGameEventSystem, frame loop dispatch, console command routing, map change orchestrator, network supervisor.",
        "description": "Engine2 coordinates the entire application lifecycle. It manages the master frame loop, dispatches tick scheduling between client and server, hosts the CGameEventSystem (GAMEEVENTSMANAGER002), and enforces network replication security gates like net_SetConVar.",
        "addresses": [
            {"symbol": "CGameEventSystem::vftable", "addr": "0x18058aea8", "type": "VTable", "desc": "GAMEEVENTSMANAGER002 virtual interface jump table."},
            {"symbol": "CGameEvent", "addr": "0x18058a970", "type": "Struct/VTable", "desc": "Game event descriptor and typed key variant container."},
            {"symbol": "CGameEventDispatcher", "addr": "0x18058b0c0", "type": "Function", "desc": "PubSub dispatcher notifying registered IGameEventListener2 listeners."},
            {"symbol": "net_SetConVar Gatekeeper", "addr": "0x1800825e0", "type": "Function", "desc": "Validates (flags >> 13) & 1 before allowing client-side ConVar replication changes."},
            {"symbol": "CSVCMsgList_GameEvents Dispatcher", "addr": "0x1802951d0", "type": "Function", "desc": "Frame-end batched event wire deserializer."}
        ],
        "schemas": [
            {"class": "CGameEventSystem", "field": "m_KnownEvents", "offset": "+0x0028", "type": "CUtlVector<EventDescriptor*>", "desc": "Registry of all declared game event definitions."},
            {"class": "CGameEventSystem", "field": "m_Listeners", "offset": "+0x0040", "type": "CUtlVector<ListenerBinding>", "desc": "Active event listener subscriber list."}
        ],
        "wireProtocols": [
            {"proto": "CNETMsg_SetConVar", "desc": "ConVar update message (ID 0x06) checked against FCVAR_REPLICATED bit 13."},
            {"proto": "CSVCMsg_GameEventList", "desc": "Phase 1 dictionary message synchronizing event IDs and key types."}
        ],
        "docs": ["convar-replication", "game-events"],
        "simulators": []
    },
    {
        "name": "networksystem.dll",
        "cluster": "Engine Core",
        "size": "2.8 MB",
        "path": "game/bin/win64/networksystem.dll",
        "status": "Extensively Reversed & Verified",
        "statusColor": "emerald",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Low-level wire packet framing, Protobuf message serialization/deserialization, 40-op Huffman flattened fieldpath serializer.",
        "description": "NetworkSystem implements the wire transport encoding. It replaces legacy fixed-width message headers with an adaptive variable-bit format (6/10/14/34 bits), executes 40-op Huffman bit-tree delta compression, and provides primitive serializers for quantized floats.",
        "addresses": [
            {"symbol": "ReadVariableBitMessageId", "addr": "0x1801bf5c0", "type": "Function", "desc": "Variable-bit NetMessage ID decoder reading prefix tiers."},
            {"symbol": "SerializeNetMessage", "addr": "0x1800e23d0", "type": "Function", "desc": "Variable-bit NetMessage ID encoder with VarInt32 length framing."},
            {"symbol": "FieldPath Huffman Table", "addr": "0x180272420", "type": "Static Table", "desc": "40-entry instruction jump table for entity delta tree traversal."},
            {"symbol": "TypeSerializer Table", "addr": "0x180271700", "type": "Static Table", "desc": "Encoders/decoders for uint32, float32, QuantizedFloat, QAngle, Vector."},
            {"symbol": "FindOrAddFieldPathToDictionary", "addr": "0x1801d8ef0", "type": "Function", "desc": "FNV-1a hash caching field paths in global dictionary."},
            {"symbol": "WriteVarInt32", "addr": "0x1801c0760", "type": "Function", "desc": "LEB128 7-bit chunk integer encoder."},
            {"symbol": "ReadVarInt32", "addr": "0x1801bf770", "type": "Function", "desc": "LEB128 integer decoder."}
        ],
        "schemas": [
            {"class": "CFieldPath", "field": "m_iIndices[10]", "offset": "+0x0000", "type": "uint16[10]", "desc": "Array of hierarchical property indices (depth <= 10)."},
            {"class": "CFieldPath", "field": "m_nCount", "offset": "+0x0014", "type": "uint16", "desc": "Current depth count of the path."}
        ],
        "wireProtocols": [
            {"proto": "Variable-Bit NetMessage Header", "desc": "6, 10, 14, or 34-bit header encoding 53% fewer bits than fixed 32-bit headers."},
            {"proto": "LEB128 VarInt32", "desc": "Variable-length integer framing for message and payload lengths."}
        ],
        "docs": ["network-framing", "flattened-serializers"],
        "simulators": []
    },
    {
        "name": "schemasystem.dll",
        "cluster": "Engine Core",
        "size": "0.5 MB",
        "path": "game/bin/win64/schemasystem.dll",
        "status": "Extensively Reversed & Verified",
        "statusColor": "emerald",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Runtime type reflection (SchemaSystem_001), MurmurHash2 type scopes, dynamic class field offset resolution.",
        "description": "SchemaSystem provides engine-wide dynamic runtime reflection. It exposes class descriptors, member field byte offsets, inheritance hierarchies, and metadata across all engine modules via MurmurHash2 hash lookup tables.",
        "addresses": [
            {"symbol": "SchemaSystem_001 Factory", "addr": "0x180037560", "type": "Export", "desc": "Interface creation export in schemasystem.dll."},
            {"symbol": "CSchemaSystem::vftable", "addr": "0x180058598", "type": "VTable", "desc": "40-method virtual function jump table."},
            {"symbol": "CSchemaSystemTypeScope::vftable", "addr": "0x1800586f0", "type": "VTable", "desc": "35-method virtual function jump table for per-module type scopes."},
            {"symbol": "MurmurHash2 Algorithm", "addr": "0x180010380", "type": "Function", "desc": "Reconstructed hash routine: seed = len ^ 0xbaadfeed, m = 0x5bd1e995."}
        ],
        "schemas": [
            {"class": "SchemaClassInfoData_t", "field": "m_pszName", "offset": "+0x0008", "type": "const char*", "desc": "String name of the declared class."},
            {"class": "SchemaClassInfoData_t", "field": "m_nSize", "offset": "+0x0018", "type": "int32", "desc": "Total sizeof the class in bytes."},
            {"class": "SchemaClassInfoData_t", "field": "m_nFieldCount", "offset": "+0x001c", "type": "int16", "desc": "Count of declared schema fields in the class."},
            {"class": "SchemaClassFieldData_t", "field": "m_pszName", "offset": "+0x0000", "type": "const char*", "desc": "Field identifier string."},
            {"class": "SchemaClassFieldData_t", "field": "m_nSingleInheritanceOffset", "offset": "+0x0010", "type": "int32", "desc": "Byte offset of the field within the class memory."}
        ],
        "wireProtocols": [
            {"proto": "4-Pass Registration Handshake", "desc": "Pass 0 (Declare), Pass 1 (Resolve Classes), Pass 2 (Resolve Fields), Pass 3 (Post-Init)."}
        ],
        "docs": ["schemasystem"],
        "simulators": []
    },
    {
        "name": "tier0.dll",
        "cluster": "Engine Core",
        "size": "4.0 MB",
        "path": "game/bin/win64/tier0.dll",
        "status": "Extensively Reversed & Verified",
        "statusColor": "emerald",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Foundational utilities: CCvar / VEngineCvar007 ConVar registry, memory allocators (g_pMemAlloc), thread synchronization primitives, logging (Msg, Warning).",
        "description": "Tier0 provides the foundational layer for all Source 2 modules. It hosts the CCvar registry, the memory allocation manager g_pMemAlloc, string manipulation utilities, atomic synchronization primitives, and logging facilities.",
        "addresses": [
            {"symbol": "CCvar::vftable (VEngineCvar007)", "addr": "0x180313208", "type": "VTable", "desc": "46-method virtual function jump table in tier0.dll."},
            {"symbol": "ConVar Hash Seed", "addr": "0x31415926", "type": "Constant", "desc": "Pi-seed used across ConVar registry caseless bucket lookups."},
            {"symbol": "g_pMemAlloc", "addr": "0x180314050", "type": "Global Pointer", "desc": "Engine master memory allocation interface pointer."}
        ],
        "schemas": [
            {"class": "ConVarRef", "field": "m_nFlags", "offset": "+0x0018", "type": "uint64", "desc": "64-bit FCVAR_* bitfield controlling replication and cheats."},
            {"class": "ConVarRef", "field": "m_pszDefaultValue", "offset": "+0x0020", "type": "const char*", "desc": "Default string value of the ConVar."}
        ],
        "wireProtocols": [
            {"proto": "64-bit FCVAR_* Bitmask", "desc": "Bit 13 = FCVAR_REPLICATED (0x2000), Bit 14 = FCVAR_CHEAT (0x4000), Bit 7 = FCVAR_ARCHIVE (0x80)."}
        ],
        "docs": ["convar-replication"],
        "simulators": []
    },
    {
        "name": "animationsystem.dll",
        "cluster": "Animation & Audio",
        "size": "9.3 MB",
        "path": "game/bin/win64/animationsystem.dll",
        "status": "Extensively Reversed & Verified",
        "statusColor": "emerald",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "AnimGraph 2 runtime (CNm Navier Motion task graph), CTwoBoneIKPoseOperation analytical IK solver, CHitBox definition tables, blend trees.",
        "description": "AnimationSystem executes the AnimGraph 2 runtime. It processes character blend spaces, state machines, cloth physics, bone retargeting, and analytical inverse kinematics (Two-Bone IK) using Law of Cosines trigonometry.",
        "addresses": [
            {"symbol": "CTwoBoneIKPoseOperation::vftable", "addr": "0x180689268", "type": "VTable", "desc": "Two-Bone IK analytical operator virtual function table."},
            {"symbol": "Two-Bone IK Solver (FUN_1802f9750)", "addr": "0x1802f9750", "type": "Function", "desc": "Law of Cosines limb solver: cos(alpha) = (L1^2 + d^2 - L2^2) / (2 * L1 * d)."},
            {"symbol": "CHitBox Descriptor", "addr": "0x18081d840", "type": "Schema Descriptor", "desc": "Defines hitbox bounds, shape radius, group ID, and bone binding."},
            {"symbol": "CHitBoxSet Descriptor", "addr": "0x18081d940", "type": "Schema Descriptor", "desc": "Collection of hitboxes belonging to a character skeleton."}
        ],
        "schemas": [
            {"class": "CHitBox", "field": "m_vMinBounds", "offset": "+0x0000", "type": "Vector", "desc": "Bounding box minimum coordinates in bone local space."},
            {"class": "CHitBox", "field": "m_vMaxBounds", "offset": "+0x000c", "type": "Vector", "desc": "Bounding box maximum coordinates in bone local space."},
            {"class": "CHitBox", "field": "m_flShapeRadius", "offset": "+0x0018", "type": "float32", "desc": "Capsule shape radius for cylindrical hitboxes."},
            {"class": "CHitBox", "field": "m_nGroupId", "offset": "+0x001c", "type": "int32", "desc": "Hitgroup identifier (Head = 1, Chest = 2, Stomach = 3...)."}
        ],
        "wireProtocols": [
            {"proto": "AnimGraph 2 Task Graph", "desc": "Node-based evaluation graph processing pose blending and IK constraints."}
        ],
        "docs": ["skeleton-animgraph"],
        "simulators": ["09"]
    },
    {
        "name": "vphysics2.dll",
        "cluster": "Physics & Collision",
        "size": "4.8 MB",
        "path": "game/bin/win64/vphysics2.dll",
        "status": "Extensively Reversed & Verified",
        "statusColor": "emerald",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Valve Rubikon physics engine: rigid body simulation, collision mesh acceleration structures (BVH/QBVH), raycast intersection pipelines, GJK/EPA continuous collision.",
        "description": "VPhysics2 replaces Havok with Valve's in-house Rubikon physics engine. It handles rigid body dynamics, multi-threaded simulation stepping via CWorldStepJob, spatial acceleration trees (RnCompoundTree_t), half-space polyhedral ray clipping (FUN_1802544a0), and GJK support function dispatch for hitboxes and capsule player collision.",
        "addresses": [
            {"symbol": "CVPhysics2Interface::vftable", "addr": "0x1803fbf50", "type": "VTable", "desc": "80-method virtual table for IVPhysics2 master interface."},
            {"symbol": "CVPhys2World::vftable", "addr": "0x1803f9b20", "type": "VTable", "desc": "80-method virtual table for IVPhysics2World simulation world."},
            {"symbol": "CFnPhysicsWorldImpl::vftable", "addr": "0x1803fa798", "type": "VTable", "desc": "38-method functional physics world adapter."},
            {"symbol": "CFnPhysicsBodyImpl::vftable", "addr": "0x1803fa610", "type": "VTable", "desc": "48-method rigid body kinematic and mass property adapter."},
            {"symbol": "CRnShape::vftable", "addr": "0x1803e1268", "type": "VTable", "desc": "33-method base Rubikon collision shape jump table."},
            {"symbol": "CRnHullShape::vftable", "addr": "0x1803e1140", "type": "VTable", "desc": "33-method convex hull shape jump table."},
            {"symbol": "CRnCapsuleShape::vftable", "addr": "0x1803e1030", "type": "VTable", "desc": "33-method capsule shape jump table."},
            {"symbol": "CWorldStepJob::vftable", "addr": "0x1803fa418", "type": "VTable", "desc": "7-method ThreadPool simulation step job."},
            {"symbol": "ConvexPolyhedronRaycast", "addr": "0x1802544a0", "type": "Function", "desc": "Exact half-space slab-clipping raycast with 0.03125f skin tolerance."},
            {"symbol": "CapsuleRaycastSweep", "addr": "0x18016caf0", "type": "Function", "desc": "Finite cylinder and hemispherical endcap analytical ray/sweep solver."},
            {"symbol": "CTraceAABB::GetSupportPoint", "addr": "0x1802ea210", "type": "Function", "desc": "Sign-bit extraction argmax support function for GJK."},
            {"symbol": "CTraceHull::GetSupportPoint", "addr": "0x1802ea5d0", "type": "Function", "desc": "SIMD 4-wide unrolled vertex projection argmax for GJK."},
            {"symbol": "VPhysics2_Interface_001", "addr": "0x180350120", "type": "Factory", "desc": "CreateInterface factory export returning singleton at 0x180460e60."}
        ],
        "schemas": [
            {"class": "RnHull_t", "field": "m_vCentroid", "offset": "+0x0000", "type": "Vector", "desc": "Center of mass in local coordinates."},
            {"class": "RnHull_t", "field": "m_Bounds", "offset": "+0x0014", "type": "AABB", "desc": "Cached local bounding box min/max."},
            {"class": "RnHull_t", "field": "m_VertexPositions", "offset": "+0x0070", "type": "CUtlVector<Vector>", "desc": "Array of support vertices for GJK/EPA searches."},
            {"class": "RnHull_t", "field": "m_FacePlanes", "offset": "+0x0088", "type": "CUtlVector<RnPlane_t>", "desc": "Array of half-space planes (nx, ny, nz, d) for ray clipping."},
            {"class": "RnCapsule_t", "field": "m_vCenter", "offset": "+0x0000", "type": "Vector[2]", "desc": "Segment endpoints A and B."},
            {"class": "RnCapsule_t", "field": "m_flRadius", "offset": "+0x0018", "type": "float32", "desc": "Capsule cylinder expansion radius."},
            {"class": "RnCompoundTree_t", "field": "m_Nodes", "offset": "+0x0000", "type": "CUtlVector<RnCompoundTreeNode_t>", "desc": "Contiguous array of 48-byte SIMD BVH spatial tree nodes."}
        ],
        "wireProtocols": [],
        "docs": ["tooling", "subtick-prediction", "rubikon-physics"],
        "simulators": ["03", "10"]
    },
    {
        "name": "soundsystem.dll",
        "cluster": "Animation & Audio",
        "size": "5.8 MB",
        "path": "game/bin/win64/soundsystem.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Sound operator engine (CSoundOpSystem), sound event graphs, dynamic audio ducking, voice compression/decompression pipeline.",
        "description": "SoundSystem executes Valve's Sound Operator System (SOS). Sounds are defined as directed acyclic graphs of operators that modulate pitch, volume, spatial pan, and ducking dynamically based on game state.",
        "addresses": [
            {"symbol": "SoundOpSystem001", "addr": "Export", "type": "Factory", "desc": "Master sound operator engine interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "steamaudio.dll",
        "cluster": "Animation & Audio",
        "size": "3.5 MB",
        "path": "game/bin/win64/steamaudio.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Steam Audio spatialization backend: binaural HRTF filtering, sound occlusion rays, physics-based environmental reverb.",
        "description": "SteamAudio provides physics-based spatial audio simulation. It traces audio rays through map geometry to model sound occlusion, diffraction around corners, and realistic environmental reverberation.",
        "addresses": [
            {"symbol": "iplCreateContext", "addr": "Export", "type": "API Export", "desc": "Steam Audio initialization context creator."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "rendersystemvulkan.dll",
        "cluster": "Rendering & Graphics",
        "size": "6.2 MB",
        "path": "game/bin/win64/rendersystemvulkan.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Vulkan rendering backend: SPIR-V pipeline state objects, memory barrier sync, command buffer submission.",
        "description": "Provides the low-level Vulkan graphics backend for Source 2. Compiles and binds SPIR-V shader stages, manages device memory heaps, and submits draw command buffers to the GPU.",
        "addresses": [
            {"symbol": "RenderSystemSurface001", "addr": "Export", "type": "Factory", "desc": "Vulkan surface and swapchain orchestrator."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling", "camera-view-matrix"],
        "simulators": []
    },
    {
        "name": "rendersystemdx11.dll",
        "cluster": "Rendering & Graphics",
        "size": "4.5 MB",
        "path": "game/bin/win64/rendersystemdx11.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "DirectX 11 rendering backend: HLSL shader binding, constant buffer management, draw call batching.",
        "description": "DirectX 11 rendering driver for Source 2 on Windows. Interacts with ID3D11DeviceContext to bind vertex and pixel shaders, constant buffers, and rasterizer states.",
        "addresses": [
            {"symbol": "RenderSystem002", "addr": "Export", "type": "Factory", "desc": "DirectX 11 render system interface."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling", "camera-view-matrix"],
        "simulators": []
    },
    {
        "name": "scenesystem.dll",
        "cluster": "Rendering & Graphics",
        "size": "7.1 MB",
        "path": "game/bin/win64/scenesystem.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "High-level scene graph management, render pass orchestration, lighting clusters, shadow map generation.",
        "description": "Manages high-level scene graph evaluation. Organizes render passes (depth prepass, forward lighting, post-processing), evaluates dynamic light clusters, and orchestrates shadow map cascades.",
        "addresses": [
            {"symbol": "SceneSystem_002", "addr": "Export", "type": "Factory", "desc": "Master scene graph interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling", "camera-view-matrix"],
        "simulators": []
    },
    {
        "name": "worldrenderer.dll",
        "cluster": "Rendering & Graphics",
        "size": "2.0 MB",
        "path": "game/bin/win64/worldrenderer.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Map geometry clustering, occlusion culling, Potentially Visible Set (PVS) calculations, mesh batching.",
        "description": "Renders static map geometry. Evaluates BSP occlusion culling, Potentially Visible Sets (PVS), and batches static world meshes for GPU rendering.",
        "addresses": [
            {"symbol": "WorldRendererBuilder001", "addr": "Export", "type": "Factory", "desc": "World geometry builder and octree traverser."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "particles.dll",
        "cluster": "Rendering & Graphics",
        "size": "6.5 MB",
        "path": "game/bin/win64/particles.dll",
        "status": "Extensively Reversed & Verified",
        "statusColor": "emerald",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Source 2 particle system: volumetric smoke grenade expansion, bullet tracers, fire, blood splatters.",
        "description": "Drives the Source 2 particle simulation. Powers CS2's groundbreaking volumetric smoke grenades, bullet tracer effects, fire propagation, and explosion debris.",
        "addresses": [
            {"symbol": "ParticleSystemFactory001", "addr": "Export", "type": "Factory", "desc": "Particle system factory interface."},
            {"symbol": "C_OP_RenderVolumetricEmitter", "addr": "0x1805c4b60", "type": "Class Descriptor", "desc": "Volumetric particle emitter renderer managing density, temperature, and kill radius."}
        ],
        "schemas": [
            {"class": "C_OP_RenderVolumetricEmitter", "field": "m_flRadius", "offset": "+0x1848", "type": "CParticleCollectionFloatInput", "desc": "Emitter volumetric outer radius."},
            {"class": "C_OP_RenderVolumetricEmitter", "field": "m_flDensity", "offset": "+0x19c0", "type": "CParticleCollectionFloatInput", "desc": "Volumetric density multiplier."},
            {"class": "C_OP_RenderVolumetricEmitter", "field": "m_flKillRadius", "offset": "+0x1e28", "type": "CParticleCollectionFloatInput", "desc": "Blast kill radius for carving voids."}
        ],
        "wireProtocols": [],
        "docs": ["tooling", "volumetric-smoke"],
        "simulators": ["11"]
    },
    {
        "name": "inputsystem.dll",
        "cluster": "Input & Transport",
        "size": "1.3 MB",
        "path": "game/bin/win64/inputsystem.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Raw Windows input capture, mouse delta accumulation, sub-tick input timestamping.",
        "description": "Hooks Windows RawInput to capture high-resolution mouse and keyboard events. Timestamps hardware events with microsecond precision to feed CSGOInput's sub-tick history buffer.",
        "addresses": [
            {"symbol": "InputSystemVersion001", "addr": "Export", "type": "Factory", "desc": "Input system interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling", "subtick-prediction"],
        "simulators": ["03"]
    },
    {
        "name": "navsystem.dll",
        "cluster": "Input & Transport",
        "size": "1.3 MB",
        "path": "game/bin/win64/navsystem.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "CS2 navigation mesh (.nav) parsing, bot pathfinding, tactical cover evaluations.",
        "description": "Parses and navigates polygonal navigation meshes (.nav). Evaluates A* pathfinding, jump connections, ladder traversal, and tactical hiding spots for bots.",
        "addresses": [
            {"symbol": "NavSystem001", "addr": "Export", "type": "Factory", "desc": "Navigation system interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "resourcesystem.dll",
        "cluster": "Input & Transport",
        "size": "0.5 MB",
        "path": "game/bin/win64/resourcesystem.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "KeyValues3 (KV3) resource compiler cache, asset manifest loader, texture/model streaming.",
        "description": "Manages game asset manifests and KeyValues3 (KV3) binary deserialization. Coordinates asynchronous streaming of textures, models, and sound assets from VPK archives.",
        "addresses": [
            {"symbol": "ResourceSystem013", "addr": "Export", "type": "Factory", "desc": "Resource compiler cache and streaming interface."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "steamnetworkingsockets.dll",
        "cluster": "Input & Transport",
        "size": "4.3 MB",
        "path": "game/bin/win64/steamnetworkingsockets.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Valve low-level UDP transport layer, MTU fragmentation, Steam Datagram Relay (SDR) encryption.",
        "description": "Provides Valve's modern UDP networking layer. Handles MTU fragmentation, SDR multi-path relaying, anti-DDoS IP hiding, and end-to-end cryptographic packet signing.",
        "addresses": [
            {"symbol": "SteamNetworkingSocketsSerialized005", "addr": "Export", "type": "Factory", "desc": "SDR transport interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling", "network-framing"],
        "simulators": []
    },
    {
        "name": "panorama.dll",
        "cluster": "Panorama UI",
        "size": "6.1 MB",
        "path": "game/bin/win64/panorama.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Valve GUI runtime: XML/CSS/JS DOM tree parser, JavaScript engine bindings, UI event bus.",
        "description": "Executes Valve's Panorama UI runtime. Parses XML templates and CSS stylesheets, binds JavaScript DOM manipulation, and manages UI event propagation.",
        "addresses": [
            {"symbol": "PanoramaUIEngine001", "addr": "Export", "type": "Factory", "desc": "Master Panorama UI engine factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "panoramauiclient.dll",
        "cluster": "Panorama UI",
        "size": "2.7 MB",
        "path": "game/bin/win64/panoramauiclient.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Game-specific Panorama glue code: HUD scoreboard, radar, killfeed, inventory, buy menu.",
        "description": "Connects client.dll game state with Panorama UI panels. Powers the in-game HUD, mini-map radar, weapon buy wheel, scoreboard, and main menu views.",
        "addresses": [
            {"symbol": "PanoramaUIClient001", "addr": "Export", "type": "Factory", "desc": "Game client UI binding interface."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "panorama_text_pango.dll",
        "cluster": "Panorama UI",
        "size": "3.0 MB",
        "path": "game/bin/win64/panorama_text_pango.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Pango/FreeType font rasterization, multilingual unicode text shaping, glyph rendering.",
        "description": "Renders high-quality typography for Panorama UI. Integrates Pango, HarfBuzz, and FreeType to support complex script shaping, bidirectional text, and font caching.",
        "addresses": [
            {"symbol": "PanoramaTextServices001", "addr": "Export", "type": "Factory", "desc": "Font rasterization and text shaping service."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "host.dll",
        "cluster": "Gameplay & Entities",
        "size": "1.4 MB",
        "path": "game/csgo/bin/win64/host.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Host tick scheduling, engine initialization loop, game-to-engine handoff bridge.",
        "description": "Coordinates host frame timing and game-to-engine bridge functions. Regulates tick cadence, enforces fps_max clamping, and coordinates map load handoffs.",
        "addresses": [
            {"symbol": "Host001", "addr": "Export", "type": "Factory", "desc": "Master host scheduling interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "matchmaking.dll",
        "cluster": "Gameplay & Entities",
        "size": "1.9 MB",
        "path": "game/csgo/bin/win64/matchmaking.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Match state machine, lobby management, game server reservation handshake.",
        "description": "Manages multiplayer matchmaking session states. Communicates with Valve Game Coordinator (GC), negotiates server reservations, and tracks match rankings.",
        "addresses": [
            {"symbol": "Matchmaking001", "addr": "Export", "type": "Factory", "desc": "Matchmaking session interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "pulse_system.dll",
        "cluster": "Scripting & Graph Execution",
        "size": "1.9 MB",
        "path": "game/bin/win64/pulse_system.dll",
        "status": "Extensively Reversed & Verified",
        "statusColor": "emerald",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Source 2 Pulse visual node graph interpreter, runtime bytecode VM, event state machine execution.",
        "description": "Source 2's graph execution engine. Executes compiled graph definitions (CPulseGraphDef), cell executions (CPulseCell_Base), 56-byte fixed-width bytecode instructions (PGDInstruction_t), and asynchronous coroutine cursors.",
        "addresses": [
            {"symbol": "IPulseSystem_001", "addr": "Export", "type": "Factory", "desc": "Master Pulse system engine interface factory."},
            {"symbol": "CPulseCell_Base::vftable", "addr": "0x180143dd8", "type": "VTable", "desc": "Base virtual method table for all Pulse graph executable cells (Slots 0-5)."},
            {"symbol": "Instruction Dispatch Loop", "addr": "0x18008f810", "type": "Function", "desc": "Fixed-size 56-byte PGDInstruction_t bytecode dispatch loop."},
            {"symbol": "Opcode String Table", "addr": "0x13f150", "type": "Global Data", "desc": "122 PulseInstructionCode_t opcode string definitions."}
        ],
        "schemas": [
            {"class": "CPulse_OutflowConnection", "field": "m_SourceOutflowName", "offset": "+0x0000", "type": "PulseSymbol_t", "desc": "Source output pin identifier."},
            {"class": "CPulse_OutflowConnection", "field": "m_nDestChunk", "offset": "+0x0008", "type": "PulseRuntimeChunkIndex_t", "desc": "Target execution bytecode chunk index."},
            {"class": "CPulse_OutflowConnection", "field": "m_nInstruction", "offset": "+0x000c", "type": "int32", "desc": "Instruction index within chunk."}
        ],
        "wireProtocols": [],
        "docs": ["tooling", "pulse-system"],
        "simulators": []
    },
    {
        "name": "meshsystem.dll",
        "cluster": "Rendering & Geometry",
        "size": "1.6 MB",
        "path": "game/bin/win64/meshsystem.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "3D geometry streaming, vertex skinning buffers, morph targets (CMorphData), flex controllers (CFlexRule).",
        "description": "Manages model mesh streaming and procedural deformation. Evaluates facial morphs, hit registration deformation shapes, and dynamic vertex buffers.",
        "addresses": [
            {"symbol": "MeshSystem001", "addr": "Export", "type": "Factory", "desc": "Master MeshSystem interface factory."}
        ],
        "schemas": [
            {"class": "CMorphSetData", "field": "m_nWidth", "offset": "+0x0010", "type": "int32", "desc": "Morph texture atlas width."},
            {"class": "CMorphSetData", "field": "m_nHeight", "offset": "+0x0014", "type": "int32", "desc": "Morph texture atlas height."}
        ],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "materialsystem2.dll",
        "cluster": "Rendering & Graphics",
        "size": "1.4 MB",
        "path": "game/bin/win64/materialsystem2.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Shader pipeline management, post-processing parameters (HDR tonemapping, bloom, vignette, exposure).",
        "description": "The next-generation Source 2 material system. Manages compiled material resources (.vmat_c), shader uniforms, and camera post-processing parameters.",
        "addresses": [
            {"symbol": "VMaterialSystem2_001", "addr": "Export", "type": "Factory", "desc": "MaterialSystem2 interface factory."}
        ],
        "schemas": [
            {"class": "PostProcessingTonemapParameters_t", "field": "m_flExposureBias", "offset": "+0x0000", "type": "float32", "desc": "Camera exposure adaptation bias."},
            {"class": "PostProcessingTonemapParameters_t", "field": "m_flMinExposure", "offset": "+0x0008", "type": "float32", "desc": "Minimum allowable HDR exposure limit."}
        ],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "filesystem_stdio.dll",
        "cluster": "Core Engine & OS",
        "size": "2.2 MB",
        "path": "game/bin/win64/filesystem_stdio.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "VPK file system (IFileSystem), LZ4 pak decompression, asset search paths, sv_pure verification.",
        "description": "Mounts and reads game archives (pak01_dir.vpk). Enforces asset integrity checking, asynchronous chunk caching, and loose file blocking.",
        "addresses": [
            {"symbol": "VFileSystem017", "addr": "Export", "type": "Factory", "desc": "Virtual file system interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "cs2.exe",
        "cluster": "Core Engine & OS",
        "size": "2.8 MB",
        "path": "game/bin/win64/cs2.exe",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Main PE launcher executable: process bootstrap, command-line arguments dispatch, engine2.dll loading.",
        "description": "The primary Counter-Strike 2 executable entry point. Initializes memory allocator hooks (g_pMemAlloc), parses command-line flags (-dedicated, -tools, -insecure), and starts Source2Main.",
        "addresses": [
            {"symbol": "main / WinMain", "addr": "Entry", "type": "Function", "desc": "Process initialization and Source2Main handoff."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "localize.dll",
        "cluster": "Panorama UI",
        "size": "0.4 MB",
        "path": "game/bin/win64/localize.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "String table localization, translation token lookup (#SFUI_..., #CSGO_...), chat formatting.",
        "description": "Provides localization services for UI and chat. Maps language tokens to localized unicode strings across all supported game languages.",
        "addresses": [
            {"symbol": "Localize_001", "addr": "Export", "type": "Factory", "desc": "Localization system interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "vscript.dll",
        "cluster": "Scripting & Graph Execution",
        "size": "1.3 MB",
        "path": "game/bin/win64/vscript.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Scripting language bridge (IVScriptManager) for community workshop maps and gamemode scripts.",
        "description": "Bridges external scripting engines with Source 2 entity classes and math helpers, enabling high-level gamemode customization.",
        "addresses": [
            {"symbol": "VScript_001", "addr": "Export", "type": "Factory", "desc": "VScript manager interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "v8system.dll",
        "cluster": "Panorama UI",
        "size": "0.2 MB",
        "path": "game/bin/win64/v8system.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Google V8 JavaScript runtime binding for Panorama UI panel scripts.",
        "description": "Integrates Google's V8 engine into Source 2, powering Panorama JavaScript contexts, UI timers, and event dispatchers.",
        "addresses": [
            {"symbol": "Source2V8System001", "addr": "Export", "type": "Factory", "desc": "V8 runtime system interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "scenefilecache.dll",
        "cluster": "Animation & Physics",
        "size": "1.2 MB",
        "path": "game/bin/win64/scenefilecache.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "Choreography scene cache, audio cue timing, synchronized facial gesture data.",
        "description": "Caches binary choreography data (.vcd_c) and sound events for player model gestures, radio commands, and announcer cues.",
        "addresses": [
            {"symbol": "SceneFileCache002", "addr": "Export", "type": "Factory", "desc": "Scene file cache interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    },
    {
        "name": "vconcomm.dll",
        "cluster": "Core Engine & OS",
        "size": "0.3 MB",
        "path": "game/bin/win64/vconcomm.dll",
        "status": "Synchronized in Ghidra & Mapped",
        "statusColor": "cyan",
        "ghidraSync": "Synchronized in CS2.gpr",
        "role": "VConsole communications protocol layer, TCP socket listener for developer inspection.",
        "description": "Provides the engine-side communication socket connecting Counter-Strike 2 to the VConsole2 developer diagnostic tool.",
        "addresses": [
            {"symbol": "VConComm001", "addr": "Export", "type": "Factory", "desc": "VConsole communications interface factory."}
        ],
        "schemas": [],
        "wireProtocols": [],
        "docs": ["tooling"],
        "simulators": []
    }
]

TOOLS_DATA = [
    {
        "id": "extract_schema_classes",
        "name": "extract_schema_classes.py",
        "category": "Schema & Type Reflection",
        "language": "Python 3",
        "targets": ["client.dll", "server.dll", "schemasystem.dll"],
        "summary": "Parses compiled SchemaClassInfoData_t arrays, class sizes, field counts, and member byte offsets directly from PE .rdata.",
        "purpose": "Source 2 entities are driven by engine-wide reflection. This utility directly parses the binary PE Section Headers to map Virtual Addresses (VA) to file offsets, walks the 32-byte SchemaClassInfoData_t table passed to ResolveDeclaredClasses, resolves type descriptors, and dumps every member field offset (e.g. m_pGameSceneNode at +0x0330, m_pAimPunchServices at +0x1598, m_hPlayerPawn at +0x07dc).",
        "usage": "python tools/extract_schema_classes.py",
        "exampleOutput": "Reading SchemaClassInfoData_t table from 0x181d33c20 (file off 0x1d31c20)...\nClass C_CSPlayerPawn (Size: 0x3b80, Fields: 34):\n  +0x0330: m_pGameSceneNode (CGameSceneNode*)\n  +0x0344: m_iHealth (int32)\n  +0x0348: m_lifeState (uint8)\n  +0x1598: m_pAimPunchServices (CPlayer_AimPunchServices*)\n  +0x1600: m_hController (CEntityHandle)\n  +0x3600: m_angEyeAngles (QAngle)",
        "codeSnippet": "def va_to_off(v):\n    r = v - image_base\n    for name, rva, vsize, raw_ptr, raw_size in sections:\n        if rva <= r < rva + vsize:\n            return raw_ptr + (r - rva)\n    return None"
    },
    {
        "id": "find_all_schema_chunks",
        "name": "find_all_schema_chunks.py",
        "category": "Schema & Type Reflection",
        "language": "Python 3",
        "targets": ["client.dll"],
        "summary": "Scans .text for all partitioned schema registration callback chunks across monolithic modules.",
        "purpose": "Monolithic binaries like client.dll (39.2 MB) contain over 4,000 declared classes. To avoid MSVC compilation and section layout limits, Valve splits schema registrations into dozens of independent registration callbacks. This tool disassembles the call references leading into InstallSchemaBindings and enumerates all chunk registration pointers.",
        "usage": "python tools/find_all_schema_chunks.py",
        "exampleOutput": "Scanning client.dll .text for Schema Registration Chunks...\nChunk 0: 0x181d33c20 (148 classes)\nChunk 1: 0x181d45a00 (210 classes)\nChunk 2: 0x181d60180 (185 classes)\nTotal Schema Classes Discovered across 18 chunks: 4,120 classes",
        "codeSnippet": "# Scans for LEA R8, [table] followed by CALL RegisterDeclaredClasses\npattern = b'\\x48\\x8d\\x15' # LEA RDX, ...\nfor match in find_all(data, pattern):\n    table_va = resolve_rip_rel(match)"
    },
    {
        "id": "inspect_controller_pawn",
        "name": "inspect_controller_pawn.py",
        "category": "Schema & Type Reflection",
        "language": "Python 3",
        "targets": ["client.dll", "server.dll"],
        "summary": "Cross-module schema comparator inspecting controller vs. pawn memory layouts, handles, and mirrored telemetry.",
        "purpose": "Verifies the separation of player identity (CCSPlayerController) from physical body (C_CSPlayerPawn). Extracts the bi-directional 32-bit handle links (m_hPlayerPawn at +0x07dc, m_hController at +0x1600), checks proxy telemetry mirroring (m_iPawnHealth, m_iPawnArmor, m_bPawnIsAlive), and validates bot takeover fields.",
        "usage": "python tools/inspect_controller_pawn.py",
        "exampleOutput": "=== CCSPlayerController (Server) ===\n  +0x0720: m_sSanitizedPlayerName (CUtlSymbolLarge)\n  +0x07dc: m_hPlayerPawn (CEntityHandle)\n  +0x0818: m_iPawnHealth (uint32 - Mirrored)\n  +0x081c: m_iPawnArmor (uint32 - Mirrored)\n  +0x08e0: m_bControllingBot (bool)\n  +0x0918: m_hOriginalControllerOfCurrentPawn (CEntityHandle)",
        "codeSnippet": "print(f'Checking bi-directional handle linkage...')\nassert controller['m_hPlayerPawn']['type'] == 'CEntityHandle'\nassert pawn['m_hController']['type'] == 'CEntityHandle'"
    },
    {
        "id": "dump_schemasystem_vtable",
        "name": "dump_schemasystem_vtable.py",
        "category": "Schema & Type Reflection",
        "language": "Python 3",
        "targets": ["schemasystem.dll"],
        "summary": "Extracts and labels the complete 40-method virtual function jump table of CSchemaSystem.",
        "purpose": "Locates the CSchemaSystem RTTI Complete Object Locator in schemasystem.dll .rdata and dumps every 64-bit function pointer in the vtable. Labels virtual methods including InstallSchemaBindings, FindTypeScopeForModule, RegisterDeclaredClasses, and ResolveDeclaredClasses.",
        "usage": "python tools/dump_schemasystem_vtable.py",
        "exampleOutput": "CSchemaSystem vtable at VA 0x180058598 (file off 0x56d98):\n  [ 0] offset +0x00: 0x1800100a0 (QueryInterface)\n  [ 1] offset +0x08: 0x1800100b0 (AddRef)\n  [ 2] offset +0x10: 0x1800100c0 (Release)\n  [ 5] offset +0x28: 0x180011420 (FindTypeScopeForModule)\n  [13] offset +0x68: 0x180012890 (RegisterDeclaredClasses)\n  [25] offset +0xc8: 0x180014020 (ResolveDeclaredClasses)",
        "codeSnippet": "col_va = off_to_va(rtti_col_off)\nvtable_va = find_pointer_to(col_va) + 8\nfor i in range(40):\n    fn = read_qword(vtable_va + i * 8)"
    },
    {
        "id": "find_vtable_rtti",
        "name": "find_vtable_rtti.py",
        "category": "RTTI & VTable Crawlers",
        "language": "Python 3",
        "targets": ["Universal (All 24 DLLs)"],
        "summary": "Universal MSVC x64 RTTI crawler locating Complete Object Locators and virtual method jump tables.",
        "purpose": "MSVC x64 builds encode Run-Time Type Information in .rdata via TypeDescriptor and _s__RTTICompleteObjectLocator structs. This tool searches for mangled class names (e.g. .?AVCCvar@@, .?AVCViewRender@@), computes the 32-bit RVA, finds the referencing COL, and resolves the vtable pointer located at COL_PTR + 8.",
        "usage": "python tools/find_vtable_rtti.py",
        "exampleOutput": "RTTI str offset: 0x64508 (.?AVCSchemaSystem@@)\nTypeDescriptor RVA: 0x65ef8\nCandidate COL at off 0x5a44c, VA 0x18005bc4c\n  --> FOUND VTABLE at off 0x56d98, VA 0x0000000180058598!",
        "codeSnippet": "if rtti_str_off != -1:\n    td_rva = off_to_va(rtti_str_off - 16) - image_base\n    for o in range(0, len(data) - 4, 4):\n        if struct.unpack_from('<I', data, o)[0] == td_rva:\n            # Found COL reference"
    },
    {
        "id": "dump_interfaces",
        "name": "dump_interfaces.py",
        "category": "RTTI & VTable Crawlers",
        "language": "Python 3",
        "targets": ["All 24 Binaries"],
        "summary": "Traverses Source 2's CreateInterface singly-linked list (InterfaceReg) to enumerate all factory versions.",
        "purpose": "Every modular dynamic library in Source 2 exports CreateInterface to provide decoupled abstraction. This tool locates the InterfaceReg linked list head, follows m_pNext pointers, and extracts interface strings like SchemaSystem_001, VEngineCvar007, GAMEEVENTSMANAGER002, and InputSystemVersion001.",
        "usage": "python tools/dump_interfaces.py",
        "exampleOutput": "Enumerating InterfaceReg linked list in tier0.dll...\n  * 'VEngineCvar007' -> Factory VA 0x180068f40 -> Instance 0x180313208\n  * 'VProcessUtils001' -> Factory VA 0x180069020\n  * 'VFileSystem017' -> Factory VA 0x180069150\nTotal Interfaces Exported: 8",
        "codeSnippet": "while curr != 0:\n    name_va = read_qword(curr + 8)\n    factory_va = read_qword(curr + 0)\n    curr = read_qword(curr + 16) # m_pNext"
    },
    {
        "id": "dump_cvar_vtable",
        "name": "dump_cvar_vtable.py",
        "category": "RTTI & VTable Crawlers",
        "language": "Python 3",
        "targets": ["tier0.dll"],
        "summary": "Disassembles and maps the 46-method jump table of CCvar (VEngineCvar007).",
        "purpose": "Maps the ConVar registry interface in tier0.dll. Resolves function pointers for RegisterConCommand (+0x48), FindVar (+0x58), Caseless String Hash Lookup (+0x60), and flag validation logic, verifying hash seed 0x31415926.",
        "usage": "python tools/dump_cvar_vtable.py",
        "exampleOutput": "CCvar vtable at VA 0x180313208 (file off 0x311c08):\n  [ 0] offset +0x00: 0x1800789e0\n  [11] offset +0x58: 0x18006c6b0 (CCvar::FindVar)\n  [12] offset +0x60: 0x18006c8c0 (HashStringCaseless)\n  [45] offset +0x168: 0x18006b460\n  Total: 46 virtual methods mapped.",
        "codeSnippet": "cvar_vtable_va = 0x180313208\nfor i in range(46):\n    fn = read_qword(va_to_off(cvar_vtable_va + i * 8))\n    print(f'  [{i:2d}] offset +0x{i*8:02x}: 0x{fn:016x}')"
    },
    {
        "id": "dump_net_messages",
        "name": "dump_net_messages.py",
        "category": "Protobuf & Network Inspection",
        "language": "Python 3",
        "targets": ["networksystem.dll"],
        "summary": "Extracts all 68 compiled Protobuf network message types across CNETMsg, CCLCMsg, and CSVCMsg.",
        "purpose": "Parses compiled Google Protobuf descriptor tables in networksystem.dll to reconstruct the complete enum registry of network messages. Discovers core engine messages (net_Tick, net_SetConVar), client-to-server messages (clc_Move, clc_RespondCvarValue), and server-to-client messages (svc_PacketEntities, svc_FlattenedSerializer).",
        "usage": "python tools/dump_net_messages.py",
        "exampleOutput": "=== Extracting All CS2 Network Message Types ===\n--- CNETMsg (12) ---\n  CNETMsg_NOP\n  CNETMsg_Tick\n  CNETMsg_SetConVar\n--- CCLCMsg (16) ---\n  CCLCMsg_Move\n  CCLCMsg_ClientInfo\n--- CSVCMsg (40) ---\n  CSVCMsg_PacketEntities\n  CSVCMsg_FlattenedSerializer\n  CSVCMsg_GameEventList",
        "codeSnippet": "proto_patterns = [b'CNETMsg_', b'CCLCMsg_', b'CSVCMsg_']\nfor pat in proto_patterns:\n    for match in find_all(data, pat):\n        msg_name = read_null_terminated(match)"
    },
    {
        "id": "dump_fp_ops",
        "name": "dump_fp_ops.py",
        "category": "Protobuf & Network Inspection",
        "language": "Python 3",
        "targets": ["networksystem.dll"],
        "summary": "Extracts the 40-operation Huffman bit-tree table and weights for entity delta FieldPath traversal.",
        "purpose": "Source 2 entity delta updates do not send property names or field IDs over the wire; they traverse the property tree using a 40-op Huffman code table at 0x180272420. This utility extracts each operation name (PlusOne, PushOne, PopOnePlusOne...), function pointer, pre-weight, and post-weight.",
        "usage": "python tools/dump_fp_ops.py",
        "exampleOutput": "FieldPath Operations Array (stride = 16 bytes):\n[ 0] name='PlusOne                               ' (0x1802140A0) | post:  0,  0\n[ 1] name='PlusTwo                               ' (0x1802140A8) | post:  1,  0\n[ 5] name='PushOneLeftDeltaZeroRightZero         ' (0x1802140D8) | post:  5,  0\n[27] name='PopOnePlusOne                         ' (0x180214360) | post: 27,  0\n[39] name='FieldPathEncodeFinish                 ' (0x180214470) | post: 39,  0",
        "codeSnippet": "fp_ops_va = 0x180272420\nfor i in range(40):\n    name_va = read_qword(va_to_off(fp_ops_va + i * 16))\n    name = read_str(name_va)"
    },
    {
        "id": "explore_camera_pipeline",
        "name": "explore_camera_pipeline.py",
        "category": "Physics & Camera Reverse Engineering",
        "language": "Python 3",
        "targets": ["client.dll"],
        "summary": "Disassembles AngleVectors, view matrix construction, and weapon aim punch recoil scaling.",
        "purpose": "Analyzes the mathematical conversion from Euler view angles to Forward/Right/Up basis vectors (AngleVectors at FUN_1817402a0). Extracts the 0.017453292 rad constant, the 2.0x recoil punch multiplier in AimPunchServices (+0x1598), and the active 4x4 VMatrix memory buffer at 0x18256ad00.",
        "usage": "python tools/explore_camera_pipeline.py",
        "exampleOutput": "AngleVectors signature located at 0x1817402a0\nFound float constant 0.017453292519943295 (deg-to-rad)\nLocated 2.0x weapon aim punch multiplier in SetUpView\nVMatrix global array mapped at VA 0x18256ad00",
        "codeSnippet": "# Searches for radians constant 0x3c8efa35 (0.017453292f)\nrad_pattern = struct.pack('<f', 0.0174532925f)\noff = data.find(rad_pattern)"
    },
    {
        "id": "find_animation_classes",
        "name": "find_animation_classes.py",
        "category": "Physics & Camera Reverse Engineering",
        "language": "Python 3",
        "targets": ["animationsystem.dll", "client.dll"],
        "summary": "Extracts skeletal animation structures, bone arrays, Two-Bone IK operators, and procedural hitboxes.",
        "purpose": "Disassembles CSkeletonInstance (vtable 0x181c41ce0), CalculateWorldBones (FUN_180aa0f80), CTransform concatenation (FUN_181748480), and CTwoBoneIKPoseOperation (vtable 0x180689268). Extracts procedural hitbox bounds (CHitBox, CHitBoxSet) and Law of Cosines IK formulas.",
        "usage": "python tools/find_animation_classes.py",
        "exampleOutput": "CSkeletonInstance VTable: 0x181c41ce0\nCalculateWorldBones: 0x180aa0f80\nCTwoBoneIKPoseOperation VTable: 0x180689268\nTwo-Bone IK Law of Cosines Solver: 0x1802f9750\nCHitBox Descriptor: 0x18081d840 (Size: 112 bytes)",
        "codeSnippet": "print(f'Checking Law of Cosines analytical solver...')\n# Verifies FUN_1802f9750 math: cos(alpha) = (L1^2 + d^2 - L2^2) / (2 * L1 * d)"
    },
    {
        "id": "inspect_vphysics2_rubikon",
        "name": "inspect_vphysics2_rubikon.py",
        "category": "Physics & Camera Reverse Engineering",
        "language": "Python 3",
        "targets": ["vphysics2.dll"],
        "summary": "Extracts Rubikon physics interfaces, 80-slot VTables, convex hull half-spaces, capsule parameters, and GJK support function addresses.",
        "purpose": "Inspects Valve's proprietary Rubikon physics engine. Resolves RTTI Complete Object Locators and VTables for IVPhysics2 (0x1803fbf50), IVPhysics2World (0x1803f9b20), and CRnShape hierarchies. Extracts the 118 schema classes in physicslib (RnHull_t, RnCapsule_t, RnCompoundTree_t) and locates exact decompilation addresses for polyhedral ray clipping and GJK support mapping.",
        "usage": "python tools/inspect_vphysics2_rubikon.py --dll \"game/bin/win64/vphysics2.dll\"",
        "exampleOutput": "[1] Resolving 24 Rubikon RTTI Descriptors & VTables:\n[*] Class: CVPhysics2Interface              | COL: 0x180407D20 | VTable: 0x1803FBF50 (80 slots)\n[*] Class: CVPhys2World                     | COL: 0x180405468 | VTable: 0x1803F9B20 (80 slots)\n[*] Class: CRnHullShape                     | COL: 0x180400FA0 | VTable: 0x1803E1140 (33 slots)\n[*] Class: CRnCapsuleShape                  | COL: 0x180400F20 | VTable: 0x1803E1030 (33 slots)\n[*] Class: CTraceAABB                       | COL: 0x1804021F0 | VTable: 0x1803E3128\n[2] Extracting Key Schema Registration Tables (physicslib):\n   Schema Registration Table at VA 0x1803DADB0 (118 declared classes)\n   -> [ 1] RnCapsule_t                  Size: 0x001C ( 28b) | Fields: 2\n   -> [82] RnHull_t                     Size: 0x00F8 (248b) | Fields: 15",
        "codeSnippet": "def off_to_rva(off):\n    for s in sections:\n        if s['roff'] <= off < s['roff'] + s['rsize']:\n            return s['rva'] + (off - s['roff'])\n    return 0"
    },
    {
        "id": "fast_scan",
        "name": "fast_scan.cpp & run_fast_scan.bat",
        "category": "Binary Scanning & Automation",
        "language": "C++20 (MSVC x64)",
        "targets": ["All 24 Binaries"],
        "summary": "High-performance multithreaded byte pattern and string scanner using memory-mapped I/O.",
        "purpose": "Searches gigabytes of game binaries in milliseconds. Uses Windows memory-mapped files (CreateFileMapping) and SIMD bitwise scanning to locate byte patterns, RTTI descriptors, interface names, and instruction signatures without Python overhead.",
        "usage": "tools\\run_fast_scan.bat",
        "exampleOutput": "Compiling fast_scan.cpp with MSVC x64 /O2...\nScanning 24 binaries across CS2 installation...\nScanned 142.8 MB in 18.4 ms (7,760 MB/s)\nMatches found: 42 RTTI descriptors, 18 VTables, 68 NetMessages",
        "codeSnippet": "HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);\nHANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);\nconst uint8_t* pData = (const uint8_t*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);"
    },
    {
        "id": "generate_portal_data",
        "name": "generate_portal_data.py & generate_docs_bundle.py",
        "category": "Binary Scanning & Automation",
        "language": "Python 3",
        "targets": ["Repository Build Pipeline"],
        "summary": "Bundles architectural documentation, binary profiles, C++ simulators, and the tools catalog into web portal data.",
        "purpose": "Automates the continuous deployment pipeline for GitHub Pages. Reads all markdown documentation files, embeds standalone C++ simulator source code, formats all 24 binary profiles, and exports docs/docs-data.js and docs/portal-data.js for zero-latency client-side browsing.",
        "usage": "python tools/generate_portal_data.py && python tools/generate_docs_bundle.py",
        "exampleOutput": "Bundled 10 documentation files into docs/docs-data.js (111,070 bytes)\nExporting 24 binary profiles, 9 simulator suites, and 14 tools into docs/portal-data.js (248,312 bytes)\nBuild pipeline complete!",
        "codeSnippet": "with open(out_file, 'w', encoding='utf-8') as f:\n    f.write('window.CS2_TOOLS_DATA = ' + json.dumps(TOOLS_DATA, indent=2) + ';\\n')"
    },
    {
        "id": "cs2_patch_diff",
        "name": "cs2_patch_diff.py",
        "category": "Binary Scanning & Automation",
        "language": "Python 3",
        "targets": ["All 24 Binaries across Updates"],
        "summary": "Automated patch differential analysis engine: tracks SchemaSystem offset shifts, ConVar mutations, interface version bumps, and PE code changes between game updates.",
        "purpose": "Provides deep static binary diffing between CS2 updates far beyond SteamDB. When Valve releases a new patch, this engine archives the binaries, extracts 2,200+ schema classes, tracks exact byte offset shifts (+/- bytes), detects newly introduced cvars and CreateInterface exports, and generates comprehensive markdown reports.",
        "usage": "python tools/cs2_patch_diff.py archive --source \"<PATH>\" --tag \"2026-10-15_25966348\" --build-id \"25966348\"\npython tools/cs2_patch_diff.py diff <old_tag> <new_tag>",
        "exampleOutput": "[*] Diffing Source 2 Updates: 2026-10-15_25966348 vs 2026-10-22_26011420\n[+] Modified Binaries: 4 (client.dll, server.dll, particles.dll, engine2.dll)\n[+] Schema Classes Modified: 12\n[+] Field Offset Shifts: 38 (e.g. CCSPlayerController::m_hPlayerPawn +0x07dc -> +0x07e0 (+4b))\n[+] ConVars Added: +6 (cl_predict_subtick_interp, sv_smoke_density_threshold...)\n[+] Generated diffs/diff_25966348_vs_26011420.md",
        "codeSnippet": "def diff_profiles(old_profile_dir, new_profile_dir):\n    # Compares schemas, shifted offsets, cvars, and interfaces\n    for cname, new_cls in new_schemas.items():\n        shift = nf['offset'] - of['offset']"
    },
    {
        "id": "scan_signatures",
        "name": "scan_signatures.py",
        "category": "Binary Scanning & Automation",
        "language": "Python 3",
        "targets": ["All 34 Binaries across Updates"],
        "summary": "Function signature and offset verification engine: scans IDA/Ghidra byte patterns with wildcards, validates 100% uniqueness, and tracks/syncs RVA shifts across updates.",
        "purpose": "Locates stripped and internal functions across CS2 binaries (AngleVectors, CalculateWorldBones, LineOfSightThroughSmoke, Pulse dispatch loop, etc.) without symbols. Verifies pattern uniqueness (1 match), computes exact RVAs and deltas (+/- bytes), and dynamically updates function_signatures.json via --update-db on every new game build.",
        "usage": "python tools/scan_signatures.py --dir versions/archive/2026-10-25_26144732 --update-db\npython tools/scan_signatures.py --diff <old_dir> <new_dir>",
        "exampleOutput": "SOURCE 2 FUNCTION SIGNATURE & OFFSET VERIFICATION REPORT\nclient.dll         | AngleVectors        | [PASS] | 0x017402a0 | 0x17402a0 | +0x0 | 1\nclient.dll         | CalculateWorldBones | [PASS] | 0x00aa0f80 | 0x0aa0f80 | +0x0 | 1\nserver.dll         | LineOfSightThroughSmoke | [PASS] | 0x009b7f50 | 0x09b7f50 | +0x0 | 1\npulse_system.dll   | PulseInstructionDispatchLoop | [PASS] | 0x0008f810 | 0x008f810 | +0x0 | 1\nTotal: 21 | Unchanged: 21 | Shifted: 0 | Broken: 0",
        "codeSnippet": "regex = pattern_to_regex(sig['pattern'])\nmatches = [m.start() for m in regex.finditer(pe.data)]\nassert len(matches) == 1 # Validates unique function signature"
    },
    {
        "id": "find_smoke_classes",
        "name": "find_smoke_classes.py",
        "category": "Physics & Camera Reverse Engineering",
        "language": "Python 3",
        "targets": ["server.dll", "particles.dll"],
        "summary": "Analyzes volumetric smoke grenade classes, 32x32x32 voxel grid structures, and 3D Morton-order bit-interleaving.",
        "purpose": "Extracts CSmokeGrenadeProjectile and CParticleProperty schema reflection tables. Locates the double-buffered 512-word uint64 voxel occupancy bitmasks, bullet tunnel carving routines (FUN_1809cb810), and line-of-sight raymarching density thresholds.",
        "usage": "python tools/find_smoke_classes.py",
        "exampleOutput": "Found 12 Smoke & Voxel Classes in server.dll\n  CSmokeGrenadeProjectile (Size: 0x1180, Fields: 42)\n    +0x0df8: m_bSmokeVolumeCreated (bool)\n    +0x0e00: m_vSmokeColor (Vector3)\n  Voxel occupancy grid: 32x32x32 = 32,768 cells (512 QWORDs)",
        "codeSnippet": "def part1_by_2(n):\n    n &= 0x000003ff\n    n = (n ^ (n << 16)) & 0xff0000ff\n    n = (n ^ (n <<  8)) & 0x0300f00f\n    n = (n ^ (n <<  4)) & 0x030c30c3\n    n = (n ^ (n <<  2)) & 0x09249249\n    return n"
    },
    {
        "id": "find_lag_comp",
        "name": "find_lag_comp.py",
        "category": "Protobuf & Network Inspection",
        "language": "Python 3",
        "targets": ["server.dll"],
        "summary": "Analyzes server-side player lag compensation history buffers, hitbox track records, and sub-tick time rewinds.",
        "purpose": "Locates CLagCompensationManager (FUN_18084a920) and CHitboxHistoryRecord allocations. Extracts the 64-tick circular history buffer, bone matrix interpolation formulas, and raycast hitbox rewinding logic used during authoritative bullet hit registration.",
        "usage": "python tools/find_lag_comp.py",
        "exampleOutput": "Scanning server.dll for CLagCompensationManager...\n  VTable: 0x180bb2940 (24 slots)\n  Record size: 0x0380 bytes per player tick\n  History buffer: 64 entries (1.000s window at 64Hz)",
        "codeSnippet": "def get_lerp_time(cl_interp, cl_updaterate):\n    return max(cl_interp, 1.0 / cl_updaterate)"
    },
    {
        "id": "dump_game_events_proto",
        "name": "dump_game_events_proto.py",
        "category": "Protobuf & Network Inspection",
        "language": "Python 3",
        "targets": ["networksystem.dll"],
        "summary": "Extracts CSVCMsg_GameEventList and CSVCMsgList_GameEvents protobuf wire descriptor tables.",
        "purpose": "Parses compiled Google Protobuf FileDescriptorProto structures embedded in networksystem.dll .rdata to extract event declaration wire tags, field numbers, and serialization schemas for multiplayer killfeed, round start, and bomb events.",
        "usage": "python tools/dump_game_events_proto.py",
        "exampleOutput": "Extracted GameEvents Protobuf Descriptors:\n  CSVCMsg_GameEventList (wire tag: 0x1e / 30)\n  CSVCMsgList_GameEvents (wire tag: 0x22 / 34)\n  Fields: descriptor_keys, event_data, passthrough_tags",
        "codeSnippet": "with open('networksystem.dll', 'rb') as f:\n    pos = data.find(b'CSVCMsg_GameEventList')"
    },
    {
        "id": "dump_usercmd_proto",
        "name": "dump_usercmd_proto.py",
        "category": "Protobuf & Network Inspection",
        "language": "Python 3",
        "targets": ["networksystem.dll"],
        "summary": "Extracts CSGOUserCmdPB and CSubtickMoveStep protobuf wire schemas and field tags.",
        "purpose": "Maps the sub-tick input serialization wire layout: legacy command number, input history protobuf array, and fractional sub-tick step timestamps (when: 0.0f..1.0f).",
        "usage": "python tools/dump_usercmd_proto.py",
        "exampleOutput": "CSGOUserCmdPB Wire Descriptors:\n  Tag 1: legacy_command_number (uint32)\n  Tag 2: client_tick (uint32)\n  Tag 3: subtick_moves (repeated CSubtickMoveStep)\n  Tag 4: input_history (repeated CSGOInputHistoryEntryPB)",
        "codeSnippet": "pos = data.find(b'CSGOUserCmdPB')"
    },
    {
        "id": "dump_setconvar_proto",
        "name": "dump_setconvar_proto.py",
        "category": "Protobuf & Network Inspection",
        "language": "Python 3",
        "targets": ["networksystem.dll"],
        "summary": "Extracts CNETMsg_SetConVar and CMsg_CVars network replication protobuf wire schemas.",
        "purpose": "Decodes the client-server replicated ConVar wire format (ID 0x06 / 6) carrying name-value pairs across the network boundary.",
        "usage": "python tools/dump_setconvar_proto.py",
        "exampleOutput": "CNETMsg_SetConVar Wire Descriptors:\n  Tag 1: convars (CMsg_CVars)\n  CMsg_CVars.CVar:\n    Tag 1: name (string)\n    Tag 2: value (string)",
        "codeSnippet": "pos = data.find(b'CNETMsg_SetConVar')"
    },
    {
        "id": "find_cvar_flags",
        "name": "find_cvar_flags.py",
        "category": "RTTI & VTable Crawlers",
        "language": "Python 3",
        "targets": ["tier0.dll", "engine2.dll"],
        "summary": "Extracts 64-bit ConVar flag bitmasks and security gates from ConVar creation routines.",
        "purpose": "Locates bitwise flag checks guarding FCVAR_REPLICATED (Bit 13 / 0x2000), FCVAR_CHEAT (Bit 14 / 0x4000), and FCVAR_DEVELOPMENTONLY in CCvar::RegisterConVar.",
        "usage": "python tools/find_cvar_flags.py",
        "exampleOutput": "Scanning tier0.dll for ConVar flag security bitmasks...\n  Found FCVAR_REPLICATED mask at 0x1800825e0\n  Found FCVAR_CHEAT gate at 0x180083110",
        "codeSnippet": "FCVAR_REPLICATED = 1 << 13\nFCVAR_CHEAT = 1 << 14\nFCVAR_PROTECTED = 1 << 5"
    },
    {
        "id": "enrich_signatures",
        "name": "enrich_signatures.py",
        "category": "Binary Scanning & Automation",
        "language": "Python 3",
        "targets": ["data/function_signatures.json"],
        "summary": "Enriches canonical function signature records with Ghidra decompiled C++ snippets and explanations.",
        "purpose": "Reads data/function_signatures.json and injects structured decompilation blocks and architecture references for web portal rendering.",
        "usage": "python tools/enrich_signatures.py",
        "exampleOutput": "Enriched 21 signatures with C++ snippets and verification docs.",
        "codeSnippet": "with open('data/function_signatures.json', 'r') as f:\n    sigs = json.load(f)"
    },
    {
        "id": "export_source2_ghidra_headers",
        "name": "export_source2_ghidra_headers.py",
        "category": "Ghidra Integration",
        "language": "Python 3",
        "targets": ["data/source2_types.h", "schemas.json", "client.dll", "server.dll"],
        "summary": "Generates C-compatible header files containing all 2,276 Source 2 Schema classes and primitives with exact struct padding for Ghidra C-Parser.",
        "purpose": "Transmutes raw extracted JSON schema tables into C struct definitions with explicit byte-aligned padding (uint8_t _pad_0x[N]), deduced member types (Vector, QAngle, CEntityHandle, pointers, floats, booleans), and verified total class byte allocations. Allows researchers to import 2,000+ typed structures into any Ghidra project in 2 seconds via File -> Parse C Source.",
        "usage": "python tools/export_source2_ghidra_headers.py versions/profiles/2026-10-25_26144732/schemas.json data/source2_types.h",
        "exampleOutput": "[+] Successfully exported 2276 classes to data/source2_types.h (1,189,283 bytes)",
        "codeSnippet": "def generate_headers(schemas_path, out_path, module_filter=None):\n    with open(schemas_path, 'r', encoding='utf-8') as f:\n        schemas = json.load(f)\n    # Generates C structs with uint8_t _pad_0x[N] padding"
    },
    {
        "id": "source2_schema_importer",
        "name": "Source2_SchemaImporter.java",
        "category": "Ghidra Integration",
        "language": "Java (GhidraScript)",
        "targets": ["client.dll", "server.dll", "DataTypeManager"],
        "summary": "Ghidra Script that parses Source 2 schema profiles and automatically creates StructureDataType definitions with exact field offsets and types.",
        "purpose": "Ingests schemas.json through Gson reflection directly into the active Ghidra program's DataTypeManager. Resolves Hungarian type conventions into Ghidra base types (Vector, QAngle, CEntityHandle, pointers) and populates every field via struct.replaceAtOffset(). Organizes structures into /Source2/<binary> categories, instantly turning raw pointer decompiler math (*(int*)(param_1 + 0x15a8)) into high-level field accesses (param_1->m_flHealthShotBoostExpirationTime).",
        "usage": "Ghidra Script Manager: Source2 -> Source2_SchemaImporter.java (or via MCP run_ghidra_script)",
        "exampleOutput": "[*] Target Program: client.dll\n[*] Total schema definitions parsed: 2276\n[+] SUCCESS: Created 183 classes with 2055 typed fields in /Source2/client.dll",
        "codeSnippet": "StructureDataType struct = new StructureDataType(progCat, className, classSize, dtm);\nstruct.replaceAtOffset(offset, fType, fSize, fieldName, comment);\ndtm.addDataType(struct, DataTypeConflictHandler.REPLACE_HANDLER);"
    },
    {
        "id": "source2_base_types",
        "name": "Source2_BaseTypes.java",
        "category": "Ghidra Integration",
        "language": "Java (GhidraScript)",
        "targets": ["DataTypeManager", "/Source2/BaseTypes"],
        "summary": "Ghidra Script that registers foundational Source 2 primitives, vectors, quaternions, handles, and containers into /Source2/BaseTypes.",
        "purpose": "Pre-defines core Source 2 types with exact memory alignments: Vector (12B), Vector2D (8B), Vector4D (16B), QAngle (12B), Quaternion (16B), CTransform (32B SIMD aligned), CEntityHandle (4B bitfield: index & 0x7FFF, serial >> 15), CUtlString (8B), CUtlSymbolLarge (8B), CUtlStringToken (4B), and CUtlVector_void (16B).",
        "usage": "Ghidra Script Manager: Source2 -> Source2_BaseTypes.java",
        "exampleOutput": "[+] Successfully registered 11 Source 2 base types in /Source2/BaseTypes",
        "codeSnippet": "StructureDataType transform = new StructureDataType(cat, \"CTransform\", 32, dtm);\ntransform.replaceAtOffset(0, quat, 16, \"rot\", \"Rodrigues rotation quaternion\");\ntransform.replaceAtOffset(16, vec, 12, \"pos\", \"Translation world position\");\ndtm.addDataType(transform, DataTypeConflictHandler.REPLACE_HANDLER);"
    }
]

MODULE_RECONSTRUCTED_DATA = {
    "client.dll": {
        "progress": {
            "status": "Extensively Reversed",
            "badgeColor": "emerald",
            "subsystems": [
                {"name": "Camera & ViewMatrix", "status": "Reconstructed", "color": "emerald"},
                {"name": "Sub-Tick Input Prediction", "status": "Reconstructed", "color": "emerald"},
                {"name": "Sub-Tick UserCmd Wire Codec", "status": "Reconstructed", "color": "emerald"},
                {"name": "Ground/Air Movement Physics", "status": "Reconstructed", "color": "emerald"},
                {"name": "Weapon Inaccuracy & Dual-Disk Spread", "status": "Reconstructed", "color": "emerald"},
                {"name": "Bone Hierarchy Forward Kinematics", "status": "Reconstructed", "color": "emerald"}
            ]
        },
        "reconstructedFiles": [
            {
                "name": "subtick_prediction.cpp",
                "path": "game/client/subtick_prediction.cpp",
                "header": "game/client/subtick_prediction.h",
                "title": "Sub-Tick Movement & Input Predictor",
                "desc": "Simulates continuous temporal slicing in Source 2 physics. Jump commands split a tick into a ground friction slice, instantaneous impulse at t_when, and airborne gravity slice.",
                "functions": ["CSubtickPredictor::PredictMovement", "CSubtickPredictor::ApplyJumpImpulse"]
            },
            {
                "name": "usercmd_wire.cpp",
                "path": "game/client/usercmd_wire.cpp",
                "header": "game/client/usercmd_wire.h",
                "title": "Sub-Tick UserCmd Protobuf Wire Codec",
                "desc": "Packs and unpacks CSubtickMoveStep analog timestamps, 64-bit button masks, and QAngle deltas inside CCSGOUserCmdPB.",
                "functions": ["CUserCmdWireCodec::CreateSubtickMove", "CUserCmdWireCodec::PackSubtickStepsToWire", "CUserCmdWireCodec::ProcessUserCmds"]
            },
            {
                "name": "movement.cpp",
                "path": "game/client/movement.cpp",
                "header": "game/client/movement.h",
                "title": "Player Movement Physics & Air Accelerate",
                "desc": "Reconstructed CGameMovement::Friction, Accelerate, AirAccelerate (30.0f wishspeed cap), WalkMove, and TryPlayerMove sliding clip.",
                "functions": ["CGameMovement::Friction", "CGameMovement::Accelerate", "CGameMovement::AirAccelerate", "CGameMovement::WalkMove", "CGameMovement::TryPlayerMove"]
            },
            {
                "name": "weapon_accuracy.cpp",
                "path": "game/client/weapon_accuracy.cpp",
                "header": "game/client/weapon_accuracy.h",
                "title": "Weapon Inaccuracy, Recoil Decay & Dual-Disk Spread",
                "desc": "Exponential accuracy recovery ODE, 34%-95% movement speed remap, dual uniform disk spread sampling (FX_CalculateSpread), and damped spring aim-punch decay.",
                "functions": ["C_CSWeaponBase::UpdateAccuracyPenalty", "C_CSWeaponBase::GetInaccuracy", "FX_CalculateSpread", "CPlayer_AimPunchServices::SimulateAimPunchDecay"]
            },
            {
                "name": "viewmatrix.cpp",
                "path": "game/client/viewmatrix.cpp",
                "header": "game/client/viewmatrix.h",
                "title": "Camera, ViewMatrix & Aim Punch",
                "desc": "Trigonometric conversion from Euler view angles (deg * 0.017453292) to Forward/Right/Up basis vectors, 2.0x recoil punch multiplier, and WorldToScreen projection.",
                "functions": ["AngleVectors", "WorldToScreen", "ApplyPunchAngle"]
            }
        ]
    },
    "server.dll": {
        "progress": {
            "status": "Extensively Reversed",
            "badgeColor": "emerald",
            "subsystems": [
                {"name": "Sub-Tick Lag Compensation", "status": "Reconstructed", "color": "emerald"},
                {"name": "Controller <-> Pawn Lifecycle", "status": "Reconstructed", "color": "emerald"},
                {"name": "Bullet Penetration & Wallbangs", "status": "Reconstructed", "color": "emerald"},
                {"name": "Hit Registration & Kevlar/Helmet Armor", "status": "Reconstructed", "color": "emerald"},
                {"name": "Volumetric Smoke LOS", "status": "Reconstructed", "color": "emerald"}
            ]
        },
        "reconstructedFiles": [
            {
                "name": "lagcompensation.cpp",
                "path": "game/server/lagcompensation.cpp",
                "header": "game/server/lagcompensation.h",
                "title": "Server Lag Compensation & Backtracking",
                "desc": "Sub-tick matrix interpolator rewinding hitboxes to shot instant (tick + fraction) via quaternion SLERP, 4096.0f teleport breaker, and sv_maxunlag window.",
                "functions": ["CLagCompensationManager::StartLagCompensation", "CLagCompensationManager::BacktrackPlayer", "CLagCompensationManager::RestoreEntityToSnapshot"]
            },
            {
                "name": "hit_registration_damage.cpp",
                "path": "game/server/hit_registration_damage.cpp",
                "header": "game/server/hit_registration_damage.h",
                "title": "Hit Registration & Kevlar/Helmet Armor Damage",
                "desc": "Authoritative hitgroup damage multipliers (4.0x Head, 1.25x Stomach, 0.75x Legs), 0.5f armor ratio scaling, and integer truncation in ApplyKevlarAndHealthDamage.",
                "functions": ["CCSPlayer_DamageReactServices::TraceAttack", "CCSPlayer_DamageReactServices::ScaleDamageByHitGroup", "CCSPlayer_DamageReactServices::OnTakeDamage_Alive"]
            },
            {
                "name": "bullet_penetration.cpp",
                "path": "game/server/bullet_penetration.cpp",
                "header": "game/server/bullet_penetration.h",
                "title": "Bullet Penetration & Surface Wallbang Raymarch",
                "desc": "4-step TraceToExit (90-unit max), combined surface penetration modifier, quadratic thickness energy loss, and 4-hit penetration cap.",
                "functions": ["CCSWeaponBase::FireBullets", "CCSWeaponBase::TraceToExit", "CCSWeaponBase::HandleBulletPenetration"]
            },
            {
                "name": "controller_pawn.cpp",
                "path": "game/server/controller_pawn.cpp",
                "header": "game/server/controller_pawn.h",
                "title": "Player Controller & Pawn Linkage",
                "desc": "Bi-directional possession link between persistent CCSPlayerController (scoreboard, stats) and transient C_CSPlayerPawn (physical ragdoll, health).",
                "functions": ["CPlayerLifecycleManager::PossessPawn", "CPlayerLifecycleManager::OnPawnDeath", "CPlayerLifecycleManager::TakeoverBotPawn"]
            }
        ]
    },
    "engine2.dll": {
        "progress": {
            "status": "Extensively Reversed",
            "badgeColor": "emerald",
            "subsystems": [
                {"name": "Engine ConVar Registry", "status": "Reconstructed", "color": "emerald"},
                {"name": "net_SetConVar Security Gate", "status": "Reconstructed", "color": "emerald"},
                {"name": "GAMEEVENTSMANAGER002 PubSub", "status": "Reconstructed", "color": "emerald"},
                {"name": "Frame Loop Scheduling", "status": "In Progress", "color": "cyan"}
            ]
        },
        "reconstructedFiles": [
            {
                "name": "cvar_engine.cpp",
                "path": "game/engine2/cvar_engine.cpp",
                "header": "game/engine2/cvar_engine.h",
                "title": "ConVar Registry & Security Gate",
                "desc": "Engine ConVar registry enforcing FCVAR_REPLICATED (bit 13) validation on net_SetConVar client commands.",
                "functions": ["CEngineCvar::RegisterConVar", "CEngineCvar::SetConVarReplicated", "CEngineCvar::FindVar"]
            },
            {
                "name": "game_events.cpp",
                "path": "game/engine2/game_events.cpp",
                "header": "game/engine2/game_events.h",
                "title": "GAMEEVENTSMANAGER002 Event Pipeline",
                "desc": "PubSub event dispatcher managing dynamic event descriptor registration, listener notification, and wire packet batches.",
                "functions": ["CGameEventSystem::RegisterEvent", "CGameEventSystem::FireEvent", "CGameEventSystem::AddListener"]
            }
        ]
    },
    "networksystem.dll": {
        "progress": {
            "status": "Extensively Reversed",
            "badgeColor": "emerald",
            "subsystems": [
                {"name": "Variable-Bit NetMessage Header", "status": "Reconstructed", "color": "emerald"},
                {"name": "40-Op Huffman FieldPath Walker", "status": "Reconstructed", "color": "emerald"},
                {"name": "LEB128 VarInt32 Codec", "status": "Reconstructed", "color": "emerald"}
            ]
        },
        "reconstructedFiles": [
            {
                "name": "netmessage_framing.cpp",
                "path": "game/networksystem/netmessage_framing.cpp",
                "header": "game/networksystem/netmessage_framing.h",
                "title": "Variable-Bit NetMessage Framing Codec",
                "desc": "Reconstructed Source 2 wire packet framing using 6, 10, 14, or 34-bit variable message headers and LEB128 VarInt32.",
                "functions": ["CNetMessageFraming::ReadHeader", "CNetMessageFraming::WriteHeader", "CBitReadStream::ReadVarInt32"]
            },
            {
                "name": "flattened_serializers.cpp",
                "path": "game/networksystem/flattened_serializers.cpp",
                "header": "game/networksystem/flattened_serializers.h",
                "title": "40-Op Huffman FieldPath Serializer",
                "desc": "Decodes entity delta bitstreams using the 40-entry Huffman instruction jump table (0x180272420).",
                "functions": ["CFlattenedSerializer::DecodeFieldPaths", "FieldPath::ToString"]
            }
        ]
    },
    "schemasystem.dll": {
        "progress": {
            "status": "Extensively Reversed",
            "badgeColor": "emerald",
            "subsystems": [
                {"name": "CSchemaSystem TypeScope", "status": "Reconstructed", "color": "emerald"},
                {"name": "MurmurHash2 Algorithm", "status": "Reconstructed", "color": "emerald"},
                {"name": "Field Offset Resolution", "status": "Reconstructed", "color": "emerald"},
                {"name": "Multi-Pass Registration", "status": "In Progress", "color": "cyan"}
            ]
        },
        "reconstructedFiles": [
            {
                "name": "schemasystem.cpp",
                "path": "game/schemasystem/schemasystem.cpp",
                "header": "game/schemasystem/schemasystem.h",
                "title": "SchemaSystem Runtime Type Scope & Hash Registry",
                "desc": "Dynamic runtime reflection resolving class descriptors and member field byte offsets via MurmurHash2.",
                "functions": ["CSchemaSystem::MurmurHash2", "CSchemaSystem::FindDeclaredClass", "CSchemaSystem::GetFieldOffset"]
            }
        ]
    },
    "vphysics2.dll": {
        "progress": {
            "status": "Extensively Reversed",
            "badgeColor": "emerald",
            "subsystems": [
                {"name": "Polyhedral Slab Raycast", "status": "Reconstructed", "color": "emerald"},
                {"name": "1/32\" Skin Margin Solver", "status": "Reconstructed", "color": "emerald"},
                {"name": "Capsule Swept Query", "status": "Reconstructed", "color": "emerald"},
                {"name": "48-Byte SIMD BVH Traversal", "status": "In Progress", "color": "cyan"}
            ]
        },
        "reconstructedFiles": [
            {
                "name": "rubikon_collision.cpp",
                "path": "game/vphysics2/rubikon_collision.cpp",
                "header": "game/vphysics2/rubikon_collision.h",
                "title": "Rubikon Physics Collision & BVH Traversal",
                "desc": "Valve Rubikon physics collision solver featuring exact 0.03125f skin margin raycasts, capsule sweeps, and 48-byte SIMD BVH node tree queries.",
                "functions": ["CRubikonCollision::IntersectRayAABB", "CRubikonCollision::SweepSphereAgainstCapsule", "CRubikonCollision::TraceRayBvh"]
            }
        ]
    },
    "animationsystem.dll": {
        "progress": {
            "status": "Extensively Reversed",
            "badgeColor": "emerald",
            "subsystems": [
                {"name": "Two-Bone Analytical IK", "status": "Reconstructed", "color": "emerald"},
                {"name": "Law of Cosines Solver", "status": "Reconstructed", "color": "emerald"},
                {"name": "AnimGraph 2 State Machine & Pose Blender", "status": "Reconstructed", "color": "emerald"},
                {"name": "8-Way Directional Strafe & AimMatrix", "status": "Reconstructed", "color": "emerald"}
            ]
        },
        "reconstructedFiles": [
            {
                "name": "two_bone_ik.cpp",
                "path": "game/animationsystem/two_bone_ik.cpp",
                "header": "game/animationsystem/two_bone_ik.h",
                "title": "Two-Bone Analytical Inverse Kinematics",
                "desc": "Analytical limb IK solver using Law of Cosines trigonometry and pole vector planes for foot planting and hand placement.",
                "functions": ["CTwoBoneIKSolver::Solve"]
            },
            {
                "name": "animgraph_blender.cpp",
                "path": "game/animationsystem/animgraph_blender.cpp",
                "header": "game/animationsystem/animgraph_blender.h",
                "title": "AnimGraph State Machine & Skeletal Pose Blender",
                "desc": "Evaluates 8-way directional locomotion blend weights, Hermite smoothstep transitions, upper-body additive aim matrices, and quaternion SLERP.",
                "functions": ["CAnimGraphPoseBlender::BlendBonePoses", "CAnimGraphPoseBlender::ComputeDirectionalWeights", "CAnimGraphPoseBlender::ApplyAimMatrixLayer"]
            }
        ]
    },
    "navsystem.dll": {
        "progress": {
            "status": "Extensively Reversed",
            "badgeColor": "emerald",
            "subsystems": [
                {"name": "CNavArea Geometry & Elevations", "status": "Reconstructed", "color": "emerald"},
                {"name": "Generation-Counter A* Solver", "status": "Reconstructed", "color": "emerald"},
                {"name": "Bot Tactical Danger Costs", "status": "Reconstructed", "color": "emerald"}
            ]
        },
        "reconstructedFiles": [
            {
                "name": "navmesh.cpp",
                "path": "game/navsystem/navmesh.cpp",
                "header": "game/navsystem/navmesh.h",
                "title": "Navigation Mesh (CNavArea) & A* Bot Pathfinding",
                "desc": "Reconstructed CNavArea bilinear Z-height query, point clamping, generation-counter A* shortest-path solver, and bot tactical traversal cost functor.",
                "functions": ["CNavArea::GetZ", "CNavArea::GetClosestPointOnArea", "CNavMeshPathfinder::BuildShortestPath"]
            }
        ]
    },
    "particles.dll": {
        "progress": {
            "status": "Extensively Reversed",
            "badgeColor": "emerald",
            "subsystems": [
                {"name": "32x32x32 Voxel Morton Grid", "status": "Reconstructed", "color": "emerald"},
                {"name": "3D DDA LineOfSightThroughSmoke", "status": "Reconstructed", "color": "emerald"},
                {"name": "Bullet Tunnel Carving", "status": "Reconstructed", "color": "emerald"},
                {"name": "HE Grenade Blast Cavity", "status": "Reconstructed", "color": "emerald"}
            ]
        },
        "reconstructedFiles": [
            {
                "name": "smoke_voxels.cpp",
                "path": "game/particles/smoke_voxels.cpp",
                "header": "game/particles/smoke_voxels.h",
                "title": "Volumetric Smoke 3D Voxel Grid & Raymarch",
                "desc": "32x32x32 Morton Z-order voxel grid with 3D DDA raymarching for optical density line-of-sight checks, bullet carving, and HE explosion voids.",
                "functions": ["CSmokeVoxelGrid::Morton3D", "CSmokeVoxelGrid::LineOfSight", "CSmokeVoxelGrid::CarveBulletTunnel", "CSmokeVoxelGrid::DetonateHE"]
            }
        ]
    },
    "pulse_system.dll": {
        "progress": {
            "status": "Extensively Reversed",
            "badgeColor": "emerald",
            "subsystems": [
                {"name": "56-Byte PGDInstruction_t VM", "status": "Reconstructed", "color": "emerald"},
                {"name": "Pulse Fiber Coroutines", "status": "Reconstructed", "color": "emerald"},
                {"name": "Opcode Switch Dispatcher", "status": "Reconstructed", "color": "emerald"},
                {"name": "Native Domain Method Binding", "status": "In Progress", "color": "cyan"}
            ]
        },
        "reconstructedFiles": [
            {
                "name": "pulse_interpreter.cpp",
                "path": "game/pulse_system/pulse_interpreter.cpp",
                "header": "game/pulse_system/pulse_interpreter.h",
                "title": "Pulse Bytecode Interpreter & Fiber VM",
                "desc": "Reconstructed Pulse Game Data graph interpreter executing 56-byte instructions across coroutine fibers with yield/resume support.",
                "functions": ["CPulseInterpreter::LoadProgram", "CPulseInterpreter::StepFiber", "CPulseInterpreter::SpawnFiber"]
            }
        ]
    }
}

def collect_sdk_modules(root):
    """Collects all clean-room reconstructed C++20 SDK headers and source files."""
    sdk_modules = []
    sdk_dir = os.path.join(root, "sdk")
    if os.path.exists(sdk_dir):
        sdk_files_map = {}
        for fname in sorted(os.listdir(sdk_dir)):
            if fname.endswith((".h", ".hpp", ".cpp")):
                fpath = os.path.join(sdk_dir, fname)
                with open(fpath, "r", encoding="utf-8") as f:
                    sdk_files_map[fname] = f.read()
        if sdk_files_map:
            sdk_modules.append({
                "module": "cs2_sdk",
                "directory": "sdk",
                "standard": "C++20 (/std:c++20 /W4 /WX)",
                "description": "Production clean-room reconstructed Source 2 master SDK reverse-engineered and verified against native PE binaries.",
                "files": sdk_files_map
            })

    game_dir = os.path.join(root, "game")
    if os.path.exists(game_dir):
        for mod_name in sorted(os.listdir(game_dir)):
            mod_path = os.path.join(game_dir, mod_name)
            if not os.path.isdir(mod_path):
                continue
            files_map = {}
            for fname in sorted(os.listdir(mod_path)):
                if fname.endswith((".h", ".hpp", ".cpp")):
                    fpath = os.path.join(mod_path, fname)
                    with open(fpath, "r", encoding="utf-8") as f:
                        files_map[fname] = f.read()
            if files_map:
                mod_label = mod_name if mod_name in ("interfaces", "shared") else f"{mod_name}.dll"
                sdk_modules.append({
                    "module": mod_label,
                    "directory": f"game/{mod_name}",
                    "standard": "C++20 (/std:c++20 /W4)",
                    "files": files_map
                })
    return sdk_modules


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    sim_dir = os.path.join(root, "docs", "simulators")
    out_file = os.path.join(root, "docs", "portal-data.js")
    versions_dir = os.path.join(root, "versions")
    data_dir = os.path.join(root, "data")
    sdk_modules = collect_sdk_modules(root)

    # Simulator metadata + auto-discovery of any new .cpp files in docs/simulators/
    simulators_list = []
    known_sim_files = set()
    for meta in SIMULATORS_METADATA:
        item = dict(meta)
        fname = item.get("name", "")
        known_sim_files.add(fname)
        cpp_path = os.path.join(sim_dir, fname)
        if os.path.exists(cpp_path):
            with open(cpp_path, "r", encoding="utf-8") as f_sim:
                code_str = f_sim.read()
                item["code"] = code_str
                item["lineCount"] = len(code_str.splitlines())
        simulators_list.append(item)

    if os.path.exists(sim_dir):
        next_auto_id = 20
        for fname in sorted(os.listdir(sim_dir)):
            if fname.endswith(".cpp") and fname not in known_sim_files:
                cpp_path = os.path.join(sim_dir, fname)
                with open(cpp_path, "r", encoding="utf-8") as f_sim:
                    code_str = f_sim.read()
                lines = code_str.splitlines()
                title_guess = fname.replace("cs2_", "").replace("_sim.cpp", "").replace("_", " ").title()
                for ln in lines[:25]:
                    if "Simulation" in ln or "Simulator" in ln or "Source 2" in ln:
                        cleaned = ln.lstrip("/ ").strip()
                        if len(cleaned) > 10 and not cleaned.startswith("==="):
                            title_guess = cleaned
                            break
                simulators_list.append({
                    "id": str(next_auto_id),
                    "name": fname,
                    "title": title_guess,
                    "target": "client.dll & server.dll",
                    "runner": fname.replace("cs2_", "build_").replace(".cpp", ".bat"),
                    "category": "Engine Simulation",
                    "badge": title_guess[:24],
                    "checks": f"Reconstructed C++20 simulation runner ({len(lines)} lines) verifying Source 2 internal algorithms.",
                    "description": f"Interactive engine visualizer and clean-room C++20 reference simulation for {title_guess}.",
                    "addresses": [],
                    "code": code_str,
                    "lineCount": len(lines)
                })
                next_auto_id += 1

    # Enrich binaries with progress tracking and reconstructed files
    for b in BINARIES_DETAIL:
        bname = b.get("name", "")
        if bname in MODULE_RECONSTRUCTED_DATA:
            b["progress"] = MODULE_RECONSTRUCTED_DATA[bname]["progress"]
            b["reconstructedFiles"] = MODULE_RECONSTRUCTED_DATA[bname]["reconstructedFiles"]
        else:
            b["progress"] = {
                "status": "Synchronized in Ghidra",
                "badgeColor": "cyan",
                "subsystems": [
                    {"name": "CreateInterface Export Discovery", "status": "Mapped", "color": "emerald"},
                    {"name": "PE Section Geometry & Imports", "status": "Mapped", "color": "emerald"},
                    {"name": "Virtual Method Table Resolution", "status": "In Progress", "color": "cyan"}
                ]
            }
            b["reconstructedFiles"] = []

    # Load version profiles and diffs
    profiles_dir = os.path.join(versions_dir, "profiles")
    diffs_dir = os.path.join(versions_dir, "diffs")
    
    version_manifests = []
    if os.path.exists(profiles_dir):
        for tag in sorted(os.listdir(profiles_dir)):
            p_dir = os.path.join(profiles_dir, tag)
            m_path = os.path.join(p_dir, "manifest.json")
            if os.path.exists(m_path):
                with open(m_path, "r", encoding="utf-8") as f:
                    m = json.load(f)

                # Compute aggregates
                binaries = m.get("binaries", {})
                total_bytes = sum(b.get("size", 0) for b in binaries.values())
                m["total_size_bytes"] = total_bytes
                m["total_size_mb"] = round(total_bytes / (1024 * 1024), 1)
                m["binary_count"] = len(binaries)

                sch_path = os.path.join(p_dir, "schemas.json")
                if os.path.exists(sch_path):
                    with open(sch_path, "r", encoding="utf-8") as f_sch:
                        m["schema_classes_count"] = len(json.load(f_sch))

                cv_path = os.path.join(p_dir, "cvars.json")
                if os.path.exists(cv_path):
                    with open(cv_path, "r", encoding="utf-8") as f_cv:
                        cv_data = json.load(f_cv)
                        m["cvars_count"] = sum(len(c) for c in cv_data.values())

                if_path = os.path.join(p_dir, "interfaces.json")
                if os.path.exists(if_path):
                    with open(if_path, "r", encoding="utf-8") as f_if:
                        m["interfaces_count"] = len(json.load(f_if))

                version_manifests.append(m)

    diff_reports = []
    if os.path.exists(diffs_dir):
        for fname in sorted(os.listdir(diffs_dir)):
            if fname.endswith(".json"):
                with open(os.path.join(diffs_dir, fname), "r", encoding="utf-8") as f:
                    diff_reports.append(json.load(f))

    # Load Function Signatures database
    signatures_data = []
    sig_file = os.path.join(data_dir, "function_signatures.json")
    if os.path.exists(sig_file):
        with open(sig_file, "r", encoding="utf-8") as f:
            signatures_data = json.load(f)

    # Load ConVars database
    convars_data = []
    convars_file = os.path.join(data_dir, "convars.json")
    if os.path.exists(convars_file):
        with open(convars_file, "r", encoding="utf-8") as f:
            convars_data = json.load(f)

    print(f"Exporting {len(BINARIES_DETAIL)} binary profiles, {len(simulators_list)} simulator suites, {len(TOOLS_DATA)} tool profiles, {len(signatures_data)} function signatures, {len(convars_data)} convars, and {len(version_manifests)} version manifests...")

    with open(out_file, "w", encoding="utf-8") as f:
        f.write("// Auto-generated by tools/generate_portal_data.py\n")
        f.write("window.CS2_BINARIES_DETAIL = ")
        json.dump(BINARIES_DETAIL, f, indent=2)
        f.write(";\n\n")
        f.write("window.CS2_SIMULATORS_DATA = ")
        json.dump(simulators_list, f, indent=2)
        f.write(";\n\n")
        f.write("window.CS2_TOOLS_DATA = ")
        json.dump(TOOLS_DATA, f, indent=2)
        f.write(";\n\n")
        f.write("window.CS2_SIGNATURES_DATA = ")
        json.dump(signatures_data, f, indent=2)
        f.write(";\n\n")
        f.write("window.CS2_CONVARS_DATA = ")
        json.dump(convars_data, f, indent=2)
        f.write(";\n\n")
        f.write("window.CS2_VERSIONS_DATA = ")
        json.dump(version_manifests, f, indent=2)
        f.write(";\n\n")
        f.write("window.CS2_DIFFS_DATA = ")
        json.dump(diff_reports, f, indent=2)
        f.write(";\n\n")
        f.write("window.CS2_SDK_DATA = ")
        json.dump(sdk_modules, f, indent=2)
        f.write(";\n")

    print(f"Successfully generated {out_file} ({os.path.getsize(out_file)} bytes)")

    # Generate Static AI & Modding API (docs/llms.txt, docs/llms-full.txt, docs/api/v1/*.json)
    generate_ai_modding_api(root, BINARIES_DETAIL, simulators_list, TOOLS_DATA, signatures_data, convars_data, version_manifests, diff_reports, sdk_modules)


def generate_ai_modding_api(root, binaries_detail, simulators_list, tools_data, signatures_data, convars_data, version_manifests, diff_reports, sdk_modules):
    """Generates static AI/LLM context indices (llms.txt, llms-full.txt) and REST-like JSON endpoints under docs/api/v1/."""
    docs_dir = os.path.join(root, "docs")
    api_dir = os.path.join(docs_dir, "api", "v1")
    os.makedirs(api_dir, exist_ok=True)

    base_url = "https://hanzkii.github.io/cs2-reverse/docs"
    repo_url = "https://github.com/Hanzkii/cs2-reverse"

    latest_ver = version_manifests[-1] if version_manifests else {"tag": "2026-10-25_26144732", "build_id": "26144732", "date": "2026-10-25"}
    latest_diff = diff_reports[-1] if diff_reports else {}

    # 2. Extract curated + latest schema classes for modding
    key_classes_names = [
        "C_BaseEntity", "CBaseEntity", "CGameSceneNode", "CSkeletonInstance", "CModelState",
        "CCSPlayerController", "C_CSPlayerPawn", "CCSPlayerPawn", "C_CSPlayerPawnBase", "CCSPlayerPawnBase",
        "CPlayer_MovementServices", "CCSPlayer_MovementServices", "CPlayer_WeaponServices",
        "CPlayer_CameraServices", "CCSPlayerBase_CameraServices", "CCSPlayer_DamageReactServices",
        "C_CSWeaponBase", "CCSWeaponBase", "CCSWeaponBaseVData", "C_BasePlayerWeapon",
        "CSmokeGrenadeProjectile", "C_SmokeGrenadeProjectile", "CPlantedC4", "CBaseAnimGraph"
    ]
    latest_schemas_path = os.path.join(root, "versions", "profiles", latest_ver["tag"], "schemas.json")
    curated_schemas = {}
    total_schema_classes = 0
    if os.path.exists(latest_schemas_path):
        with open(latest_schemas_path, "r", encoding="utf-8") as f:
            all_sch = json.load(f)
            total_schema_classes = len(all_sch)
            for k in key_classes_names:
                if k in all_sch:
                    curated_schemas[k] = all_sch[k]

    # 3. Write /docs/api/v1/ JSON endpoints
    index_payload = {
        "name": "CS2 (Source 2) Reverse Engineering & Modding API",
        "version": "1.0.0",
        "latest_build": {
            "tag": latest_ver.get("tag"),
            "build_id": latest_ver.get("build_id"),
            "date": latest_ver.get("date"),
            "binaries_count": len(binaries_detail),
            "signatures_count": len(signatures_data),
            "schema_classes_count": total_schema_classes,
            "convars_count": len(convars_data),
            "sdk_modules_count": len(sdk_modules)
        },
        "ai_context_files": {
            "llms_txt": f"{base_url}/llms.txt",
            "llms_full_txt": f"{base_url}/llms-full.txt"
        },
        "endpoints": {
            "index": f"{base_url}/api/v1/index.json",
            "signatures": f"{base_url}/api/v1/signatures.json",
            "schemas": f"{base_url}/api/v1/schemas.json",
            "sdk": f"{base_url}/api/v1/sdk.json",
            "convars": f"{base_url}/api/v1/convars.json",
            "binaries": f"{base_url}/api/v1/binaries.json",
            "latest_diff": f"{base_url}/api/v1/latest_diff.json"
        },
        "usage_instructions_for_ai": (
            "Use signatures.json for verified IDA/Ghidra byte patterns and current RVAs/VAs across client.dll, server.dll, "
            "engine2.dll, networksystem.dll, schemasystem.dll, vphysics2.dll, animationsystem.dll, navsystem.dll, and pulse_system.dll. "
            "Use schemas.json for exact member byte offsets. Use sdk.json for clean-room C++20 headers and implementations."
        ),
        "self_update_protocol": {
            "summary": "How AI agents and developers can obtain 100% up-to-date RVAs, signatures, and Schema offsets immediately after a CS2 Steam update, even if this GitHub repository has not been updated yet.",
            "method_1_wildcard_pattern_scanning": {
                "when_to_use": "Resolving shifted internal function RVAs/VAs (e.g. GetInaccuracy, FX_CalculateSpread, StartLagCompensation, AirAccelerate) on a newly patched CS2 binary.",
                "how_it_works": "Every entry in /docs/api/v1/signatures.json contains an IDA/Ghidra wildcard byte pattern ('pattern') where '??' masks out relocatable 32-bit RIP-relative displacements. While 'current_rva' shifts by +N bytes after a game update, the 'pattern' remains invariant across builds.",
                "cli_command": "python tools/scan_signatures.py --bin-dir \"<STEAM_LIBRARY>/Counter-Strike Global Offensive\" --update-db",
                "standalone_python_scanner": (
                    "import struct, re\n"
                    "def scan_pe_pattern(dll_path, ida_pattern):\n"
                    "    data = open(dll_path, 'rb').read()\n"
                    "    pe_off = struct.unpack_from('<I', data, 0x3C)[0]\n"
                    "    num_sec = struct.unpack_from('<H', data, pe_off + 6)[0]\n"
                    "    opt_sz = struct.unpack_from('<H', data, pe_off + 20)[0]\n"
                    "    sec_table = pe_off + 24 + opt_sz\n"
                    "    regex = b''.join(b'.' if tok == '??' else re.escape(bytes.fromhex(tok)) for tok in ida_pattern.split())\n"
                    "    for i in range(num_sec):\n"
                    "        s = sec_table + i * 40\n"
                    "        vsize, rva, rsize, rptr = struct.unpack_from('<IIII', data, s + 8)\n"
                    "        m = re.search(regex, data[rptr:rptr+rsize], re.DOTALL)\n"
                    "        if m: return hex(rva + m.start()), hex(0x180000000 + rva + m.start())\n"
                    "    return None"
                )
            },
            "method_2_runtime_schemasystem_reflection": {
                "when_to_use": "Resolving class member offsets (C_CSPlayerPawn, CCSPlayerController, C_CSWeaponBase, CSkeletonInstance) without relying on static schemas.json offsets.",
                "how_it_works": "Query SchemaSystem_001 exported by schemasystem.dll -> CSchemaSystem::FindTypeScopeForModule('client.dll') (VTable slot 13) -> CSchemaSystemTypeScope::FindDeclaredClass('C_CSPlayerPawn') (VTable slot 2) -> iterate SchemaClassInfoData_t::m_pFields (+0x28) to read m_nSingleInheritanceOffset (+0x10). See game/schemasystem/schemasystem.h in sdk.json."
            },
            "method_3_local_repository_crawler": {
                "when_to_use": "Regenerating all 2,276+ Schema classes, 138+ CreateInterface tokens, 2,695+ ConVars, and shifted RVAs locally from a user's Steam CS2 installation in ~30 seconds (zero Ghidra required).",
                "commands": [
                    "git clone https://github.com/Hanzkii/cs2-reverse.git && cd cs2-reverse",
                    "python tools/scan_signatures.py --bin-dir \"<STEAM_LIBRARY>/Counter-Strike Global Offensive\" --update-db",
                    "python tools/cs2_patch_diff.py archive --date YYYY-MM-DD --build <NEW_BUILD_ID> --bin-dir \"<STEAM_LIBRARY>/Counter-Strike Global Offensive\"",
                    "python tools/cs2_patch_diff.py profile --tag YYYY-MM-DD_<NEW_BUILD_ID>",
                    "python tools/cs2_patch_diff.py diff --old 2026-10-25_26144732 --new YYYY-MM-DD_<NEW_BUILD_ID>",
                    "python tools/generate_docs_bundle.py && python tools/generate_portal_data.py"
                ]
            }
        }
    }

    with open(os.path.join(api_dir, "index.json"), "w", encoding="utf-8") as f:
        json.dump(index_payload, f, indent=2)

    with open(os.path.join(api_dir, "signatures.json"), "w", encoding="utf-8") as f:
        json.dump({
            "build_id": latest_ver.get("build_id"),
            "tag": latest_ver.get("tag"),
            "count": len(signatures_data),
            "signatures": signatures_data
        }, f, indent=2)

    with open(os.path.join(api_dir, "schemas.json"), "w", encoding="utf-8") as f:
        json.dump({
            "build_id": latest_ver.get("build_id"),
            "tag": latest_ver.get("tag"),
            "total_classes_in_profile": total_schema_classes,
            "curated_modding_classes_count": len(curated_schemas),
            "classes": curated_schemas
        }, f, indent=2)

    with open(os.path.join(api_dir, "sdk.json"), "w", encoding="utf-8") as f:
        json.dump({
            "build_id": latest_ver.get("build_id"),
            "standard": "C++20 (/std:c++20 /W4 /WX)",
            "modules_count": len(sdk_modules),
            "modules": sdk_modules
        }, f, indent=2)

    with open(os.path.join(api_dir, "convars.json"), "w", encoding="utf-8") as f:
        json.dump({
            "build_id": latest_ver.get("build_id"),
            "count": len(convars_data),
            "convars": convars_data
        }, f, indent=2)

    with open(os.path.join(api_dir, "binaries.json"), "w", encoding="utf-8") as f:
        json.dump({
            "build_id": latest_ver.get("build_id"),
            "count": len(binaries_detail),
            "binaries": binaries_detail
        }, f, indent=2)

    with open(os.path.join(api_dir, "latest_diff.json"), "w", encoding="utf-8") as f:
        json.dump(latest_diff, f, indent=2)

    # 4. Generate /docs/llms.txt (Standard concise AI directory index)
    md_files = sorted([fn for fn in os.listdir(docs_dir) if fn.endswith(".md")])
    llms_lines = [
        "# CS2 (Source 2) Reverse Engineering & Modding Reference",
        "",
        f"> Authoritative clean-room C++20 SDK, byte-level function signatures, runtime SchemaSystem offsets, ConVars, and architectural documentation for Valve's Counter-Strike 2 (Source 2 Engine, Build {latest_ver.get('build_id')}).",
        "",
        "## Quick AI & Modding Context Links",
        f"- [Full AI Context Bundle (llms-full.txt)]({base_url}/llms-full.txt): Single-file concatenation of all 39 function signatures, C++20 SDK headers, key Schema offsets, and architecture docs.",
        f"- [API Master Index (JSON)]({base_url}/api/v1/index.json): Discoverable JSON endpoints for programmatic AI tool-calling.",
        f"- [Function Signatures & RVAs (JSON)]({base_url}/api/v1/signatures.json): 39 verified unique IDA/Ghidra byte patterns with baseline and current RVAs/VAs.",
        f"- [Reconstructed C++20 SDK (JSON)]({base_url}/api/v1/sdk.json): Complete clean-room C++20 headers and implementations across 9 Source 2 binaries.",
        f"- [Core Modding Schemas & Offsets (JSON)]({base_url}/api/v1/schemas.json): Exact field byte offsets for C_CSPlayerPawn, CCSPlayerController, C_CSWeaponBase, CSkeletonInstance, and CSmokeGrenadeProjectile.",
        f"- [ConVars & Commands Database (JSON)]({base_url}/api/v1/convars.json): 2,695 categorized Source 2 console variables with FCVAR flags and descriptions.",
        f"- [Latest Patch Diff Telemetry (JSON)]({base_url}/api/v1/latest_diff.json): Differential RVA shifts, schema offset drifts, and newly added ConVars.",
        "",
        "## Post-Patch Self-Recovery Protocol (If CS2 Updates Before GitHub Is Updated)",
        "When Valve ships a new Counter-Strike 2 Steam update before this repository is updated, AI agents and developers MUST use the following 3 mechanisms to obtain 100% accurate offsets and RVAs:",
        "",
        "### 1. Resolve Shifted Function RVAs via Wildcard Byte Signatures (`signatures.json`)",
        "- **Why RVAs Shift**: Minor code changes shift function addresses (`current_rva`) by `+N` bytes, but the compiled instruction opcodes (`pattern` in `signatures.json`, with `??` masking relocatable 32-bit RIP-relative displacements) remain invariant.",
        "- **In C++20 Mods**: Never hardcode `current_rva` alone. Scan the module's `.text` section at startup using the `pattern` string from `signatures.json`.",
        "- **Standalone Zero-Dependency Python Scanner (Run on user's local `.dll` to get new RVA/VA)**:",
        "```python",
        "import struct, re, urllib.request, json",
        "def resolve_new_rva(dll_path: str, ida_pattern: str):",
        "    data = open(dll_path, 'rb').read()",
        "    pe_off = struct.unpack_from('<I', data, 0x3C)[0]",
        "    num_sec, opt_sz = struct.unpack_from('<H', data, pe_off + 6)[0], struct.unpack_from('<H', data, pe_off + 20)[0]",
        "    sec_table = pe_off + 24 + opt_sz",
        "    regex = b''.join(b'.' if tok == '??' else re.escape(bytes.fromhex(tok)) for tok in ida_pattern.split())",
        "    for i in range(num_sec):",
        "        vsize, rva, rsize, rptr = struct.unpack_from('<IIII', data, sec_table + i * 40 + 8)",
        "        m = re.search(regex, data[rptr:rptr + rsize], re.DOTALL)",
        "        if m:",
        "            new_rva = rva + m.start()",
        "            return f'0x{new_rva:08x}', f'0x{0x180000000 + new_rva:09x}'",
        "    return None",
        "```",
        "",
        "### 2. Resolve Shifted Class Member Offsets via Runtime `SchemaSystem_001` Reflection",
        "- Instead of hardcoding static offsets from `schemas.json`, query `SchemaSystem_001` (`schemasystem.dll` export) at runtime:",
        "  1. Call `CSchemaSystem::FindTypeScopeForModule(\"client.dll\")` (VTable Slot 13).",
        "  2. Call `CSchemaSystemTypeScope::FindDeclaredClass(\"C_CSPlayerPawn\")` (VTable Slot 2).",
        "  3. Iterate `SchemaClassInfoData_t::m_pFields` (`+0x28`, count at `+0x1C`) and read `SchemaClassFieldData_t::m_nSingleInheritanceOffset` (`+0x10`).",
        "  4. Clean-room C++20 headers are provided in `game/schemasystem/schemasystem.h` and `game/interfaces/schemasystem_interface.h` (inside `sdk.json` and `llms-full.txt`).",
        "",
        "### 3. Full Local Repository Resync (~30 Seconds, Zero Ghidra Required)",
        "If you have shell access to the user's machine with CS2 installed (`<STEAM_LIBRARY>/Counter-Strike Global Offensive`), clone the repo and run the static PE crawlers to regenerate all 39 function RVAs, 2,276+ schema classes, 2,695+ ConVars, and local API files:",
        "```bash",
        "# 1. Rescan all 39 byte patterns against local Steam CS2 binaries and update data/function_signatures.json:",
        "python tools/scan_signatures.py --bin-dir \"<STEAM_LIBRARY>/Counter-Strike Global Offensive\" --update-db",
        "",
        "# 2. Crawl local PE binaries to extract all 2,276+ Schema classes, Interfaces, and ConVars:",
        "python tools/cs2_patch_diff.py archive --date YYYY-MM-DD --build <NEW_BUILD_ID> --bin-dir \"<STEAM_LIBRARY>/Counter-Strike Global Offensive\"",
        "python tools/cs2_patch_diff.py profile --tag YYYY-MM-DD_<NEW_BUILD_ID>",
        "python tools/cs2_patch_diff.py diff --old 2026-10-25_26144732 --new YYYY-MM-DD_<NEW_BUILD_ID>",
        "",
        "# 3. Regenerate local API JSONs (docs/api/v1/*.json), llms-full.txt, and Ghidra C headers:",
        "python tools/export_source2_ghidra_headers.py versions/profiles/YYYY-MM-DD_<NEW_BUILD_ID>/schemas.json data/source2_types.h",
        "python tools/generate_docs_bundle.py && python tools/generate_portal_data.py",
        "```",
        "",
        "## Reconstructed C++20 Modding SDK (`sdk/` & `game/`)",
    ]

    for mod in sdk_modules:
        fnames = ", ".join(f"`{k}`" for k in mod["files"].keys())
        llms_lines.append(f"- **{mod['module']}** (`{mod['directory']}/`): {fnames} — [View on GitHub]({repo_url}/tree/main/{mod['directory']})")

    llms_lines.extend([
        "",
        "## Verified Function Signatures Summary (Latest Build " + str(latest_ver.get("build_id")) + ")",
        "| Binary | Symbol / Routine | Current RVA | Current VA | Category |",
        "| :--- | :--- | :--- | :--- | :--- |"
    ])
    for sig in signatures_data:
        rva = sig.get("current_rva") or sig.get("baseline_rva")
        va = sig.get("current_va") or sig.get("baseline_va")
        llms_lines.append(f"| `{sig['binary']}` | `{sig['name']}` | `{rva}` | `{va}` | {sig.get('category', '')} |")

    llms_lines.extend([
        "",
        "## Architecture Documentation Reports (`docs/*.md`)"
    ])
    for fn in md_files:
        llms_lines.append(f"- [{fn}]({repo_url}/blob/main/docs/{fn})")

    llms_txt_path = os.path.join(docs_dir, "llms.txt")
    with open(llms_txt_path, "w", encoding="utf-8") as f:
        f.write("\n".join(llms_lines) + "\n")

    # 5. Generate /docs/llms-full.txt (Complete self-contained context for LLMs building mods)
    full_lines = list(llms_lines)
    full_lines.extend([
        "",
        "================================================================================",
        "# SECTION 1: CANONICAL FUNCTION SIGNATURES, BYTE PATTERNS & RVAs",
        "================================================================================",
        ""
    ])
    for sig in signatures_data:
        rva = sig.get("current_rva") or sig.get("baseline_rva")
        va = sig.get("current_va") or sig.get("baseline_va")
        full_lines.extend([
            f"### {sig['name']} ({sig['binary']})",
            f"- **ID**: `{sig['id']}`",
            f"- **Current RVA**: `{rva}` (Baseline: `{sig['baseline_rva']}`, Delta: `{sig.get('delta_str', '+0x0')}`)",
            f"- **Current VA**: `{va}`",
            f"- **IDA / Ghidra Pattern**: `{sig['pattern']}`",
            f"- **Description**: {sig.get('description', '')}",
            ""
        ])

    full_lines.extend([
        "================================================================================",
        "# SECTION 2: CLEAN-ROOM C++20 SDK HEADERS (`sdk/**/*.hpp` & `game/**/*.h`)",
        "================================================================================",
        ""
    ])
    for mod in sdk_modules:
        for fname, content in mod["files"].items():
            if fname.endswith((".h", ".hpp")):
                full_lines.extend([
                    f"### File: `{mod['directory']}/{fname}` ({mod['module']})",
                    "```cpp",
                    content.strip(),
                    "```",
                    ""
                ])

    full_lines.extend([
        "================================================================================",
        "# SECTION 3: KEY SCHEMA CLASS MEMBER OFFSETS (Build " + str(latest_ver.get("build_id")) + ")",
        "================================================================================",
        ""
    ])
    for cname, cdata in curated_schemas.items():
        full_lines.append(f"### `{cname}` (`{cdata.get('binary')}`, Size: `{cdata.get('size_hex')}` / {cdata.get('size')} bytes, Base: `{cdata.get('base') or 'None'}`)")
        for fld in cdata.get("fields", [])[:60]:
            full_lines.append(f"- `{fld['offset_hex']}` ({fld['offset']}): `{fld['type']} {fld['name']}`")
        if len(cdata.get("fields", [])) > 60:
            full_lines.append(f"- ... ({len(cdata['fields']) - 60} additional fields in `/docs/api/v1/schemas.json`)")
        full_lines.append("")

    full_lines.extend([
        "================================================================================",
        "# SECTION 4: ARCHITECTURAL REVERSE ENGINEERING REPORTS (`docs/00..19.md`)",
        "================================================================================",
        ""
    ])
    for fn in md_files:
        fpath = os.path.join(docs_dir, fn)
        with open(fpath, "r", encoding="utf-8") as f:
            md_text = f.read()
        full_lines.extend([
            f"## Document: {fn}",
            md_text.strip(),
            "",
            "--------------------------------------------------------------------------------",
            ""
        ])

    llms_full_path = os.path.join(docs_dir, "llms-full.txt")
    with open(llms_full_path, "w", encoding="utf-8") as f:
        f.write("\n".join(full_lines) + "\n")

    print(f"Successfully generated Static AI & Modding API in {api_dir}, llms.txt ({os.path.getsize(llms_txt_path)} B), and llms-full.txt ({os.path.getsize(llms_full_path)} B)")

if __name__ == "__main__":
    main()

