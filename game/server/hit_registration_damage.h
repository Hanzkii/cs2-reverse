/**
 * @file hit_registration_damage.h
 * @brief Clean-room C++20 reconstruction of CS2 Hit Registration, HitGroup Scaling & Armor Damage Absorption.
 *
 * Reconstructed from server.dll:
 *   - CCSPlayerPawn::IsArmored                    (Baseline RVA: 0x00aca700, Build 26144732 RVA: 0x00ad7ba0)
 *   - CCSPlayerPawn::ApplyArmorDamageCalculation  (Baseline RVA: 0x00a81050, Build 26144732 RVA: 0x00a8e500)
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace cs2::server {

/**
 * @brief Source 2 HitGroup_t enumeration used in CGameTrace & CTakeDamageInfo.
 */
enum class HitGroup_t : int32_t {
    HITGROUP_GENERIC   = 0,
    HITGROUP_HEAD      = 1,
    HITGROUP_CHEST     = 2,
    HITGROUP_STOMACH   = 3,
    HITGROUP_LEFTARM   = 4,
    HITGROUP_RIGHTARM  = 5,
    HITGROUP_LEFTLEG   = 6,
    HITGROUP_RIGHTLEG  = 7,
    HITGROUP_NECK      = 8,
    HITGROUP_GEAR      = 10
};

/**
 * @brief Damage type bitflags inside CTakeDamageInfo (+0x4C).
 */
enum DamageBits_t : uint32_t {
    DMG_GENERIC             = 0,
    DMG_CRUSH               = (1 << 0),
    DMG_BULLET              = (1 << 1),
    DMG_SLASH               = (1 << 2),
    DMG_BURN                = (1 << 3),
    DMG_BLAST               = (1 << 6),
    DMG_CLUB                = (1 << 7),
    DMG_SHOCK               = (1 << 8),
    DMG_FALL                = (1 << 5),
    DMG_HEADSHOT            = (1 << 30)
};

/**
 * @brief Reconstructed server-side damage scaling ConVars.
 */
struct ServerDamageConVars {
    float mp_damage_scale_ct_head = 1.0f;
    float mp_damage_scale_t_head  = 1.0f;
    float mp_damage_scale_ct_body = 1.0f;
    float mp_damage_scale_t_body  = 1.0f;
    bool  mp_damage_headshot_only = false;
};

/**
 * @brief Reconstructed CCSPlayer_ItemServices subset (+0xB18 on CCSPlayerPawn).
 */
struct CCSPlayer_ItemServices {
    bool m_bHasDefuser = false; // +0x40
    bool m_bHasHelmet  = false; // +0x41
    bool m_bHasHeavyArmor = false; // +0x42
};

/**
 * @brief Reconstructed CTakeDamageInfo layout in server.dll.
 */
struct CTakeDamageInfo {
    float       m_flDamage           = 0.0f;  // +0x44: Scaled damage accumulator
    float       m_flOriginalDamage   = 0.0f;  // +0x48: Base pre-armor damage
    uint32_t    m_bitsDamageType     = DMG_BULLET; // +0x4C: Damage type bitfield
    float       m_flArmorPenetration = 1.0f;  // Weapon m_flArmorRatio (e.g. 1.55 for AK-47, 1.40 for M4A4)
    HitGroup_t  m_nHitGroup          = HitGroup_t::HITGROUP_GENERIC;
};

/**
 * @brief Result structure emitted after armor absorption and health deduction.
 */
struct DamageCalculationResult {
    float   flRawDistanceDamage  = 0.0f;
    float   flHitgroupScaledDmg  = 0.0f;
    float   flFinalHealthDamage  = 0.0f;
    float   flAbsorbedByArmor    = 0.0f;
    int32_t nHealthLost          = 0;
    int32_t nArmorLost           = 0;
    int32_t nRemainingHealth     = 0;
    int32_t nRemainingArmor      = 0;
    bool    bWasArmoredHit       = false;
    bool    bIsFatal             = false;
};

/**
 * @brief Reconstructed CCSPlayerPawn combat damage & Kevlar/Helmet evaluator.
 */
class CCSPlayerPawnDamageEvaluator {
public:
    int32_t                 m_iHealth       = 100;  // +0x0334
    int32_t                 m_ArmorValue    = 100;  // +0x150C (Baseline) / +0x1514 (Build 26144732)
    int32_t                 m_iTeamNum      = 3;    // 2 = Terrorist, 3 = Counter-Terrorist
    CCSPlayer_ItemServices  m_ItemServices{};       // +0x0B18

    /**
     * @brief Checks whether a specific hitgroup is protected by the pawn's current Kevlar/Helmet state.
     * Reconstructed from server.dll @ RVA 0x00aca700 (Build 26144732: 0x00ad7ba0).
     */
    bool IsArmored(HitGroup_t hitGroup) const;

    /**
     * @brief Evaluates distance-based exponential bullet damage decay:
     *        D(d) = D_base * (flRangeModifier ^ (d / 500.0f))
     */
    static float ApplyDistanceDropoff(float baseDamage, float rangeModifier, float distanceUnits);

    /**
     * @brief Returns hitgroup damage multiplier (Head 4.0x, Stomach 1.25x, Chest/Arms 1.0x, Legs 0.75x)
     *        combined with team-specific mp_damage_scale_* ConVars.
     */
    float GetHitgroupDamageMultiplier(
        HitGroup_t hitGroup,
        const ServerDamageConVars& cvars = ServerDamageConVars{}
    ) const;

    /**
     * @brief Executes Kevlar & Helmet damage reduction, durability exhaustion clamping, and health deduction.
     * Reconstructed from server.dll @ RVA 0x00a81050 (Build 26144732: 0x00a8e500).
     */
    DamageCalculationResult ApplyArmorDamageCalculation(
        CTakeDamageInfo& info,
        float baseWeaponDamage,
        float rangeModifier,
        float distanceUnits,
        const ServerDamageConVars& cvars = ServerDamageConVars{}
    );
};

} // namespace cs2::server
