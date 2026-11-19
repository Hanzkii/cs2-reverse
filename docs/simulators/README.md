# Source 2 (CS2) Clean-Room C++20 Simulation Suite

This directory contains 27 standalone, zero-dependency C++20 simulation testbenches and execution runners. Each simulator is an authoritative mathematical and control-flow reconstruction of an internal Source 2 game engine subsystem, compiled and validated under MSVC x64 (`/std:c++20 /O2 /EHsc /W4 /WX`).

---

## 1. Subsystem Simulation Matrix

| Simulator Source | Build Runner | Subsystem / Domain | Architecture Specification | Mathematical Invariants Verified |
| :--- | :--- | :--- | :--- | :--- |
| [`cs2_subtick_usercmd_prediction_sim.cpp`](cs2_subtick_usercmd_prediction_sim.cpp) | [`build_subtick_sim.bat`](build_subtick_sim.bat) | Sub-Tick & UserCmd Prediction | [`03_subtick_usercmd_and_prediction.md`](../03_subtick_usercmd_and_prediction.md) | Multi-slice temporal integration ($t_0 \to \text{when} \to t_1$), analog ground friction, jump impulse application |
| [`cs2_netmessage_framing_sim.cpp`](cs2_netmessage_framing_sim.cpp) | [`build_netmessage_framing_sim.bat`](build_netmessage_framing_sim.bat) | Wire Framing & Varints | [`01_network_framing.md`](../01_network_framing.md) | Protobuf LEB128 varint framing, net message ID bitfield routing, channel synchronization |
| [`cs2_flattened_serializer_sim.cpp`](cs2_flattened_serializer_sim.cpp) | [`build_flattened_serializer_sim.bat`](build_flattened_serializer_sim.bat) | Huffman & FieldPaths | [`02_flattened_serializers.md`](../02_flattened_serializers.md) | 40-Op Huffman FieldPath decoder, symbol table resolution, property delta consolidation |
| [`cs2_schemasystem_reflection_sim.cpp`](cs2_schemasystem_reflection_sim.cpp) | [`build_schemasystem_reflection_sim.bat`](build_schemasystem_reflection_sim.bat) | Schema Reflection | [`04_schemasystem_runtime_reflection.md`](../04_schemasystem_runtime_reflection.md) | `CSchemaSystemTypeScope` hash bucket lookup, MurmurHash2 key derivation, runtime field reflection |
| [`cs2_controller_pawn_lifecycle_sim.cpp`](cs2_controller_pawn_lifecycle_sim.cpp) | [`build_controller_pawn_sim.bat`](build_controller_pawn_sim.bat) | Entity System & Ownership | [`05_controller_vs_pawn_architecture.md`](../05_controller_vs_pawn_architecture.md) | Split controller-pawn lifecycle, bot takeover state machine, spectator handoff, telemetry mirroring |
| [`cs2_convar_replication_sim.cpp`](cs2_convar_replication_sim.cpp) | [`build_convar_replication_sim.bat`](build_convar_replication_sim.bat) | ConVar Replication | [`06_convar_storage_and_replication.md`](../06_convar_storage_and_replication.md) | `ConVar` FCVAR bitflag masking (`FCVAR_REPLICATED`, `FCVAR_CHEAT`), client shadow sync, cvar change callbacks |
| [`cs2_view_matrix_sim.cpp`](cs2_view_matrix_sim.cpp) | [`build_view_matrix_sim.bat`](build_view_matrix_sim.bat) | Camera & View Projection | [`07_camera_and_view_matrix_pipeline.md`](../07_camera_and_view_matrix_pipeline.md) | `AngleVectors` basis conversion, $4 \times 4$ `VMatrix` projection, recoil punch scaling ($2.0\times$), NDC clipping |
| [`cs2_game_events_sim.cpp`](cs2_game_events_sim.cpp) | [`build_game_events_sim.bat`](build_game_events_sim.bat) | Game Event Dispatch | [`08_game_events_pipeline.md`](../08_game_events_pipeline.md) | `IGameEventManager2` descriptor registration, network bitstream serialization, listener dispatch |
| [`cs2_skeleton_animation_sim.cpp`](cs2_skeleton_animation_sim.cpp) | [`build_skeleton_anim_sim.bat`](build_skeleton_anim_sim.bat) | Skeleton & Two-Bone IK | [`09_skeleton_bone_hierarchy_and_animgraph.md`](../09_skeleton_bone_hierarchy_and_animgraph.md) | 32-byte `CTransform` concatenation, Rodrigues rotation formula, analytical Law of Cosines Two-Bone IK |
| [`cs2_rubikon_physics_sim.cpp`](cs2_rubikon_physics_sim.cpp) | [`build_rubikon_sim.bat`](build_rubikon_sim.bat) | Rubikon Physics Engine | [`10_vphysics2_rubikon_collision_engine.md`](../10_vphysics2_rubikon_collision_engine.md) | Semi-implicit Euler rigid body integration, impulse solver, contact constraint stabilization |
| [`cs2_rubikon_collision_sim.cpp`](cs2_rubikon_collision_sim.cpp) | [`build_rubikon_collision_sim.bat`](build_rubikon_collision_sim.bat) | Continuous Collision Detection | [`rubikon_collision_physics.md`](../rubikon_collision_physics.md) | Gilbert-Johnson-Keerthi (GJK) distance solver, Conservative Advancement (TOI), Minkowski difference |
| [`cs2_volumetric_smoke_sim.cpp`](cs2_volumetric_smoke_sim.cpp) | [`build_smoke_sim.bat`](build_smoke_sim.bat) | Volumetric Smoke Grid | [`11_volumetric_smoke_and_3d_voxel_grids.md`](../11_volumetric_smoke_and_3d_voxel_grids.md) | $32 \times 32 \times 32$ voxel grid discretization, 3D Morton order bit-interleaving, DDA raymarching, bullet carving |
| [`cs2_lag_compensation_sim.cpp`](cs2_lag_compensation_sim.cpp) | [`build_lagcomp_sim.bat`](build_lagcomp_sim.bat) | Lag Compensation & History | [`lag_compensation_rollback.md`](../lag_compensation_rollback.md) | 64-tick circular history ring buffer, continuous sub-tick interpolation, teleport discontinuity breaker |
| [`cs2_pulse_vm_sim.cpp`](cs2_pulse_vm_sim.cpp) | [`build_pulse_sim.bat`](build_pulse_sim.bat) | Pulse Scripting VM | [`13_pulse_system_visual_scripting_engine.md`](../13_pulse_system_visual_scripting_engine.md) | Chunk bytecode dispatch, cursor lifecycle fork/join, register yield state machine |
| [`cs2_bullet_penetration_sim.cpp`](cs2_bullet_penetration_sim.cpp) | [`build_penetration_sim.bat`](build_penetration_sim.bat) | Ballistic Penetration | [`ballistic_penetration_physics.md`](../ballistic_penetration_physics.md) | Multi-material entry/exit ray tracing, kinetic dissipation ODE, armor mitigation jump table |
| [`cs2_weapon_spread_recoil_sim.cpp`](cs2_weapon_spread_recoil_sim.cpp) | [`build_weapon_spread_sim.bat`](build_weapon_spread_sim.bat) | Weapon Spread & Recoil | [`16_weapon_inaccuracy_and_recoil_decay.md`](../16_weapon_inaccuracy_and_recoil_decay.md) | Inaccuracy decay curves, damped harmonic punch spring, Box-Muller Gaussian circular spread |
| [`cs2_spatial_audio_sim.cpp`](cs2_spatial_audio_sim.cpp) | [`build_spatial_audio_sim.bat`](build_spatial_audio_sim.bat) | Spatial Audio & HRTF | [`spatial_audio_acoustics.md`](../spatial_audio_acoustics.md) | Steam Audio ray acoustics, binaural HRTF convolution, air absorption octave EQ, occlusion culling |
| [`cs2_particle_engine_sim.cpp`](cs2_particle_engine_sim.cpp) | [`build_particle_engine_sim.bat`](build_particle_engine_sim.bat) | Compute Particle Engine | [`particle_system_architecture.md`](../particle_system_architecture.md) | SIMD vector particle integration, operator dependency graphs, bounded sphere culling |
| [`cs2_kv3_deserializer_sim.cpp`](cs2_kv3_deserializer_sim.cpp) | [`build_kv3_deserializer_sim.bat`](build_kv3_deserializer_sim.bat) | KeyValues3 Parser | [`kv3_resource_system.md`](../kv3_resource_system.md) | Binary KV3 block decompression (LZ4 / ZSTD), text format serialization, type reflection table |
| [`cs2_network_delta_sim.cpp`](cs2_network_delta_sim.cpp) | [`build_network_delta_sim.bat`](build_network_delta_sim.bat) | Network Delta Codec | [`network_entity_delta_codec.md`](../network_entity_delta_codec.md) | Sparse property index encoding, baseline delta difference compression, worker bitstream merge |
| [`packet_entities_merge_sim.cpp`](packet_entities_merge_sim.cpp) | [`build_packet_entities_merge_sim.bat`](build_packet_entities_merge_sim.bat) | Snapshot Merger & Threads | [`net_packet_entities_pipeline.md`](../net_packet_entities_pipeline.md) | Pre-decode bitstream consolidation (`cl_packetentities_merge_before_decode`), parallel worker job stride |
| [`cs2_hitbox_bounding_capsule_sim.cpp`](cs2_hitbox_bounding_capsule_sim.cpp) | [`build_hitbox_sim.bat`](build_hitbox_sim.bat) | Hitbox Ray Intersections | [`09_skeleton_bone_hierarchy_and_animgraph.md`](../09_skeleton_bone_hierarchy_and_animgraph.md) | Analytical Ray-Capsule intersection solver, coordinate space alignment, closest-point-on-segment |
| [`cs2_ray_trace_shape_sim.cpp`](cs2_ray_trace_shape_sim.cpp) | [`build_ray_trace_shape_sim.bat`](build_ray_trace_shape_sim.bat) | Ray-AABB / Ray-OBB | [`10_vphysics2_rubikon_collision_engine.md`](../10_vphysics2_rubikon_collision_engine.md) | Slab method Ray-AABB intersection, rotated OBB ray cast, surface normal derivation |
| [`cs2_entity_system_chunk_table_sim.cpp`](cs2_entity_system_chunk_table_sim.cpp) | [`build_entity_chunk_sim.bat`](build_entity_chunk_sim.bat) | 2-Tier Entity Identity | [`05_controller_vs_pawn_architecture.md`](../05_controller_vs_pawn_architecture.md) | 32-bit `CEntityHandle` unpacking (Index: `0x7FFF`, Serial: `>> 15`), chunk pointer indirection ($512$ per chunk) |
| [`cs2_inferno_fire_sim.cpp`](cs2_inferno_fire_sim.cpp) | [`build_inferno_fire_sim.bat`](build_inferno_fire_sim.bat) | Fire Surface Propagation | [`10_vphysics2_rubikon_collision_engine.md`](../10_vphysics2_rubikon_collision_engine.md) | Molotov ground tracing, obstacle rejection, cellular fire spread automata |
| [`cs2_econ_item_wear_sim.cpp`](cs2_econ_item_wear_sim.cpp) | [`build_econ_item_wear_sim.bat`](build_econ_item_wear_sim.bat) | Skin Wear & Float Math | [`12_binary_patch_diffing_and_version_archiving.md`](../12_binary_patch_diffing_and_version_archiving.md) | IEEE-754 wear float interpolation, wear tier classification thresholds, pattern seed derivation |
| [`test_sdk_game_integration.cpp`](test_sdk_game_integration.cpp) | [`build_sdk_game_integration.bat`](build_sdk_game_integration.bat) | SDK Master Test Suite | [`20_source2_reconstructed_sdk_architecture.md`](../20_source2_reconstructed_sdk_architecture.md) | Comprehensive cross-module integration test uniting `sdk/` and `game/` C++20 clean-room headers |

---

## 2. Compilation Instructions

All simulators are self-contained single translation units with zero third-party dependencies (no Boost, GLM, DirectX SDK, or protobuf libraries required).

### Using the Automated Batch Runners
Run the corresponding `.bat` file from an x64 Native Tools Command Prompt for VS 2022:
```cmd
cd docs\simulators
build_subtick_sim.bat
```

### Manual Compilation via MSVC (Command Line)
```cmd
cl.exe /nologo /O2 /std:c++20 /EHsc /W4 /WX /DNDEBUG cs2_subtick_usercmd_prediction_sim.cpp
cs2_subtick_usercmd_prediction_sim.exe
```

---

## 3. Engineering Invariants & Standards

1. **Zero External Runtime Dependencies**: Standard C++20 STL only (`<iostream>`, `<vector>`, `<cmath>`, `<chrono>`, `<array>`, `<numbers>`).
2. **Determinism**: Fixed-point or deterministic floating-point mathematical formulation mirroring Source 2 release binaries.
3. **Strict Path Hygiene**: Clean relative paths only; no local machine or user directory paths serialized.
