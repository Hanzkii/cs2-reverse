#pragma once

// ============================================================================
// Source 2 SDK & Game Subsystems Integration Bridge
// Target Build: 2026-10-25_26144732 (CS2 Update Archive)
// Architecture: x86-64 (MSVC ABI)
// Language Standard: C++20 (/std:c++20 /W4 /WX)
//
// Unifies the public SDK class hierarchy (sdk/) with the clean-room
// reconstructed engine services and algorithms (game/).
// ============================================================================

#include "client_classes.hpp"
#include "entity_system.hpp"
#include "interfaces_registry.hpp"
#include "render_scene_material.hpp"
#include "input_usercmd.hpp"
#include "trace_events_aux.hpp"

// Modular Game Subsystem Headers
#include "../game/shared/math_types.h"
#include "../game/shared/entity_handle.h"
#include "../game/shared/skeleton.h"
#include "../game/shared/hitbox.h"
#include "../game/shared/ray_trace.h"
#include "../game/shared/usercmd.h"
#include "../game/shared/weapon_vdata.h"

#include "../game/interfaces/interfaces.h"
#include "../game/interfaces/cvar.h"
#include "../game/interfaces/engine_client.h"
#include "../game/interfaces/trace_manager.h"
#include "../game/interfaces/game_event_manager.h"
#include "../game/interfaces/sound_system.h"
#include "../game/interfaces/schema_interface.h"

#include "../game/tier0/memalloc.h"
#include "../game/tier0/utl_string_token.h"
#include "../game/tier0/utl_vector.h"

#include "../game/client/viewmatrix.h"
#include "../game/client/movement.h"
#include "../game/client/subtick_prediction.h"
#include "../game/client/weapon_accuracy.h"
#include "../game/client/bone_setup.h"
#include "../game/client/entity_system.h"

#include "../game/server/controller_pawn.h"
#include "../game/server/hit_registration_damage.h"
#include "../game/server/bullet_penetration.h"
#include "../game/server/lagcompensation.h"

#include "../game/animationsystem/two_bone_ik.h"
#include "../game/animationsystem/animgraph_blender.h"
#include "../game/vphysics2/rubikon_collision.h"
#include "../game/particles/smoke_voxels.h"
#include "../game/pulse_system/pulse_interpreter.h"
#include "../game/navsystem/navmesh.h"
#include "../game/materialsystem2/material_system2.h"
#include "../game/scenesystem/scene_system.h"
#include "../game/soundsystem/sound_ops.h"

namespace cs2::bridge {

// ============================================================================
// 1. Bi-directional Type Adapters (Zero-Cost Conversion Helpers)
// ============================================================================

[[nodiscard]] constexpr inline shared::Vector3D ToGame(const sdk::Vector3& v) noexcept {
    return {v.x, v.y, v.z};
}

[[nodiscard]] constexpr inline sdk::Vector3 ToSDK(const shared::Vector3D& v) noexcept {
    return {v.x, v.y, v.z};
}

[[nodiscard]] constexpr inline client::Vector3 ToClient(const sdk::Vector3& v) noexcept {
    return {v.x, v.y, v.z};
}

[[nodiscard]] constexpr inline sdk::Vector3 ToSDK(const client::Vector3& v) noexcept {
    return {v.x, v.y, v.z};
}

[[nodiscard]] constexpr inline shared::QAngle ToGame(const sdk::QAngle& a) noexcept {
    return {a.pitch, a.yaw, a.roll};
}

[[nodiscard]] constexpr inline sdk::QAngle ToSDK(const shared::QAngle& a) noexcept {
    return {a.pitch, a.yaw, a.roll};
}

[[nodiscard]] constexpr inline shared::CTransform ToGame(const sdk::CTransform& t) noexcept {
    return {t.qx, t.qy, t.qz, t.qw, t.px, t.py, t.pz, t.scale};
}

[[nodiscard]] constexpr inline sdk::CTransform ToSDK(const shared::CTransform& t) noexcept {
    return {t.qx, t.qy, t.qz, t.qw, t.px, t.py, t.pz, t.scale};
}

[[nodiscard]] constexpr inline shared::CEntityHandle ToGame(const sdk::CEntityHandle& h) noexcept {
    return shared::CEntityHandle(h.GetRaw());
}

[[nodiscard]] constexpr inline sdk::CEntityHandle ToSDK(const shared::CEntityHandle& h) noexcept {
    return sdk::CEntityHandle(h.GetRaw());
}

[[nodiscard]] inline shared::VMatrix ToGame(const sdk::VMatrix& m) noexcept {
    shared::VMatrix out;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out[r][c] = m[r][c];
        }
    }
    return out;
}

[[nodiscard]] inline sdk::VMatrix ToSDK(const shared::VMatrix& m) noexcept {
    sdk::VMatrix out;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out[r][c] = m[r][c];
        }
    }
    return out;
}

// ============================================================================
// 2. High-Level Entity Helpers & Cross-Module Invocations
// ============================================================================

/**
 * @brief Resolves the player pawn entity associated with a player controller.
 */
[[nodiscard]] inline sdk::C_CSPlayerPawn* GetPlayerPawn(
    const sdk::CCSPlayerController* pController,
    const sdk::CGameEntitySystem* pEntitySystem
) noexcept {
    if (!pController || !pEntitySystem) return nullptr;
    return pEntitySystem->GetEntityFromHandle<sdk::C_CSPlayerPawn>(pController->m_hPlayerPawn);
}

/**
 * @brief Computes the player pawn's current eye position (world origin + view offset).
 */
[[nodiscard]] inline sdk::Vector3 GetEyePosition(const sdk::C_CSPlayerPawn* pPawn) noexcept {
    if (!pPawn) return {0.0f, 0.0f, 0.0f};
    return {
        pPawn->m_vOldOrigin.x + pPawn->m_vecViewOffset.x,
        pPawn->m_vOldOrigin.y + pPawn->m_vecViewOffset.y,
        pPawn->m_vOldOrigin.z + pPawn->m_vecViewOffset.z
    };
}

/**
 * @brief Evaluates whether a pawn is actively alive and in combat.
 */
[[nodiscard]] inline bool IsPawnAlive(const sdk::C_CSPlayerPawn* pPawn) noexcept {
    if (!pPawn) return false;
    return pPawn->m_iHealth > 0 && pPawn->m_lifeState == 0;
}

/**
 * @brief Computes the camera aim punch angle scaled by CS2's 2.0x recoil punch scalar.
 */
[[nodiscard]] inline sdk::QAngle GetPunchAngles(const sdk::C_CSPlayerPawn* pPawn, float scalar = 2.0f) noexcept {
    if (!pPawn) return {0.0f, 0.0f, 0.0f};
    const auto& punch = pPawn->GetAimPunchAngle();
    return {punch.pitch * scalar, punch.yaw * scalar, punch.roll * scalar};
}

/**
 * @brief Performs world-to-screen coordinate transformation using the active view matrix.
 */
[[nodiscard]] inline bool WorldToScreen(
    const sdk::Vector3& worldPos,
    float& screenX,
    float& screenY,
    const sdk::VMatrix* pMatrix,
    int screenWidth,
    int screenHeight
) noexcept {
    if (!pMatrix) return false;
    return pMatrix->WorldToScreen(worldPos, screenX, screenY, screenWidth, screenHeight);
}

/**
 * @brief Solves analytical Two-Bone Inverse Kinematics (Law of Cosines) for player limbs.
 */
[[nodiscard]] inline animationsystem::IKSolveResult SolveTwoBoneIK(
    const sdk::Vector3& root,
    const sdk::Vector3& target,
    const sdk::Vector3& pole,
    float length1,
    float length2
) noexcept {
    const animationsystem::Vector3 r{root.x, root.y, root.z};
    const animationsystem::Vector3 t{target.x, target.y, target.z};
    const animationsystem::Vector3 p{pole.x, pole.y, pole.z};
    return animationsystem::CTwoBoneIKSolver::Solve(r, t, p, length1, length2);
}

/**
 * @brief Evaluates Kevlar/Helmet armor protection and damage absorption for a combat character.
 */
[[nodiscard]] inline server::DamageCalculationResult EvaluateDamage(
    int victimHealth,
    int victimArmor,
    int victimTeam,
    bool hasHelmet,
    float baseDamage,
    float rangeModifier,
    float distanceUnits,
    server::HitGroup_t hitGroup,
    float armorRatio
) noexcept {
    server::CCSPlayerPawnDamageEvaluator eval;
    eval.m_iHealth = victimHealth;
    eval.m_ArmorValue = victimArmor;
    eval.m_iTeamNum = victimTeam;
    eval.m_ItemServices.m_bHasHelmet = hasHelmet;

    server::CTakeDamageInfo info;
    info.m_flDamage = baseDamage;
    info.m_flOriginalDamage = baseDamage;
    info.m_flArmorPenetration = armorRatio;
    info.m_nHitGroup = hitGroup;

    return eval.ApplyArmorDamageCalculation(info, baseDamage, rangeModifier, distanceUnits);
}

} // namespace cs2::bridge
