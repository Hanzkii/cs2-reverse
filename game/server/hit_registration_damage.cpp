/**
 * @file hit_registration_damage.cpp
 * @brief Clean-room C++17 implementation of CS2 Hit Registration, HitGroup Scaling & Armor Damage Absorption.
 *
 * Reconstructed from server.dll:
 *   - CCSPlayerPawn::IsArmored                    (Baseline RVA: 0x00aca700, Build 26144732 RVA: 0x00ad7ba0)
 *   - CCSPlayerPawn::ApplyArmorDamageCalculation  (Baseline RVA: 0x00a81050, Build 26144732 RVA: 0x00a8e500)
 */

#include "hit_registration_damage.h"

namespace cs2::server {

/**
 * Reconstructed from server.dll!CCSPlayerPawn::IsArmored
 * Baseline RVA: 0x00aca700 | Build 26144732 RVA: 0x00ad7ba0
 *
 * Disassembly logic:
 *   45 32 C0                   xor r8b, r8b
 *   83 B9 0C 15 00 00 00       cmp dword ptr [rcx + 0x150c], 0  ; m_ArmorValue <= 0
 *   7E 3C                      jle short loc_ret_false
 *   83 FA 08                   cmp edx, 8                       ; hitgroup > 8
 *   77 37                      ja short loc_ret_false
 *   ...                        ; Jump table switch on hitGroup
 */
bool CCSPlayerPawnDamageEvaluator::IsArmored(HitGroup_t hitGroup) const {
    if (this->m_ArmorValue <= 0) {
        return false;
    }

    switch (hitGroup) {
        case HitGroup_t::HITGROUP_GENERIC:
        case HitGroup_t::HITGROUP_CHEST:
        case HitGroup_t::HITGROUP_STOMACH:
        case HitGroup_t::HITGROUP_LEFTARM:
        case HitGroup_t::HITGROUP_RIGHTARM:
            return true;

        case HitGroup_t::HITGROUP_HEAD:
        case HitGroup_t::HITGROUP_NECK:
            // Head/Neck requires m_pItemServices->m_bHasHelmet (+0xB18 -> +0x41)
            return this->m_ItemServices.m_bHasHelmet;

        case HitGroup_t::HITGROUP_LEFTLEG:
        case HitGroup_t::HITGROUP_RIGHTLEG:
        case HitGroup_t::HITGROUP_GEAR:
        default:
            // Legs are never protected by Kevlar in Counter-Strike 2
            return false;
    }
}

/**
 * Evaluates distance falloff:
 *   D(d) = D_base * (flRangeModifier ^ (d / 500.0f))
 */
float CCSPlayerPawnDamageEvaluator::ApplyDistanceDropoff(
    float baseDamage,
    float rangeModifier,
    float distanceUnits
) {
    if (distanceUnits <= 0.0f) {
        return baseDamage;
    }
    return baseDamage * std::pow(rangeModifier, distanceUnits / 500.0f);
}

/**
 * Returns HitGroup_t damage multiplier combined with team-specific ConVars:
 *   - Head:    4.00x * mp_damage_scale_{ct,t}_head
 *   - Stomach: 1.25x * mp_damage_scale_{ct,t}_body
 *   - Chest:   1.00x * mp_damage_scale_{ct,t}_body
 *   - Arms:    1.00x * mp_damage_scale_{ct,t}_body
 *   - Legs:    0.75x * mp_damage_scale_{ct,t}_body
 */
float CCSPlayerPawnDamageEvaluator::GetHitgroupDamageMultiplier(
    HitGroup_t hitGroup,
    const ServerDamageConVars& cvars
) const {
    if (cvars.mp_damage_headshot_only && hitGroup != HitGroup_t::HITGROUP_HEAD) {
        return 0.0f;
    }

    const bool isCT = (this->m_iTeamNum == 3);
    const float headScale = isCT ? cvars.mp_damage_scale_ct_head : cvars.mp_damage_scale_t_head;
    const float bodyScale = isCT ? cvars.mp_damage_scale_ct_body : cvars.mp_damage_scale_t_body;

    switch (hitGroup) {
        case HitGroup_t::HITGROUP_HEAD:
            return 4.0f * headScale;

        case HitGroup_t::HITGROUP_STOMACH:
            return 1.25f * bodyScale;

        case HitGroup_t::HITGROUP_LEFTLEG:
        case HitGroup_t::HITGROUP_RIGHTLEG:
            return 0.75f * bodyScale;

        case HitGroup_t::HITGROUP_CHEST:
        case HitGroup_t::HITGROUP_LEFTARM:
        case HitGroup_t::HITGROUP_RIGHTARM:
        case HitGroup_t::HITGROUP_GENERIC:
        default:
            return 1.0f * bodyScale;
    }
}

/**
 * Reconstructed from server.dll!CCSPlayerPawn::ApplyArmorDamageCalculation
 * Baseline RVA: 0x00a81050 | Build 26144732 RVA: 0x00a8e500
 *
 * Key Constants Verified in Disassembly:
 *   - RVA 0x00a810e7: movss xmm6, cs:flHalf (0.5f) -> flArmorBonus = 0.5f, flArmorRatioScale = 0.5f
 *   - RVA 0x00a8121a: movss xmm0, cs:flOne  (1.0f) -> minimum 1 point armor durability loss on armored hit
 */
DamageCalculationResult CCSPlayerPawnDamageEvaluator::ApplyArmorDamageCalculation(
    CTakeDamageInfo& info,
    float baseWeaponDamage,
    float rangeModifier,
    float distanceUnits,
    const ServerDamageConVars& cvars
) {
    DamageCalculationResult res{};

    // 1. Distance attenuation
    res.flRawDistanceDamage = ApplyDistanceDropoff(baseWeaponDamage, rangeModifier, distanceUnits);

    // 2. Hitgroup multiplier
    const float hgMult = GetHitgroupDamageMultiplier(info.m_nHitGroup, cvars);
    res.flHitgroupScaledDmg = res.flRawDistanceDamage * hgMult;

    info.m_flOriginalDamage = res.flHitgroupScaledDmg;
    info.m_flDamage         = res.flHitgroupScaledDmg;

    float flHealthDamage = info.m_flDamage;
    float flArmorDamage  = 0.0f;

    // 3. Check if damage is bullet/slash/club/generic and hitgroup is protected by Kevlar/Helmet
    const bool eligibleDamageType = !(info.m_bitsDamageType & (DMG_FALL | DMG_SHOCK | DMG_BURN));
    if (flHealthDamage > 0.0f && eligibleDamageType && this->IsArmored(info.m_nHitGroup)) {
        res.bWasArmoredHit = true;

        constexpr float kArmorRatioMultiplier = 0.5f;
        constexpr float kArmorBonus           = 0.5f;

        // Effective armor ratio: weapon m_flArmorRatio * 0.5f
        const float flEffectiveArmorRatio = info.m_flArmorPenetration * kArmorRatioMultiplier;

        // Post-mitigation health damage
        float flNewDamage = flHealthDamage * flEffectiveArmorRatio;

        // Armor durability cost: half of the blocked damage
        flArmorDamage = (flHealthDamage - flNewDamage) * kArmorBonus;
        if (flArmorDamage > 0.0f && flArmorDamage < 1.0f) {
            flArmorDamage = 1.0f;
        }

        // 4. Armor exhaustion overflow: if hit requires more armor than pawn currently has
        if (flArmorDamage > static_cast<float>(this->m_ArmorValue)) {
            flArmorDamage = static_cast<float>(this->m_ArmorValue);
            flNewDamage   = flHealthDamage - (flArmorDamage / kArmorBonus);
        }

        res.nArmorLost     = static_cast<int32_t>(flArmorDamage);
        this->m_ArmorValue = std::max(0, this->m_ArmorValue - res.nArmorLost);
        res.flAbsorbedByArmor = flHealthDamage - flNewDamage;
        flHealthDamage     = flNewDamage;
    }

    // 5. Integer health truncation (Source 2 truncates post-armor float damage to int32)
    info.m_flDamage        = flHealthDamage;
    res.flFinalHealthDamage = flHealthDamage;
    res.nHealthLost        = static_cast<int32_t>(flHealthDamage);
    this->m_iHealth        = std::max(0, this->m_iHealth - res.nHealthLost);

    res.nRemainingHealth   = this->m_iHealth;
    res.nRemainingArmor    = this->m_ArmorValue;
    res.bIsFatal           = (this->m_iHealth == 0);

    return res;
}

} // namespace cs2::server
