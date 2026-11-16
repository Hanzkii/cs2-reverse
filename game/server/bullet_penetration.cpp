#include "bullet_penetration.h"
#include <limits>
#include <cmath>
#include <algorithm>

namespace cs2::server {

// -------------------------------------------------------------------------
// Authoritative Distance Falloff Formula (Air Attenuation ODE)
// server.dll: EvaluateBulletPenetration @ RVA 0x00a90280 / VA 0x180a90280
// D_rem = D_base * pow(flRangeModifier, distance / 500.0f)
// -------------------------------------------------------------------------
float CBulletPenetration::ApplyDistanceFalloff(float baseDamage, float rangeModifier, float distance) noexcept {
    if (rangeModifier <= 0.0f || baseDamage <= 0.0f) {
        return 0.0f;
    }
    const float exponent = distance / 500.0f;
    return baseDamage * std::pow(rangeModifier, exponent);
}

// -------------------------------------------------------------------------
// Authoritative Material Lookup Helpers
// -------------------------------------------------------------------------
SurfaceData_t CBulletPenetration::GetSurfaceWood() {
    SurfaceData_t s{};
    s.materialChar = CHAR_TEX_WOOD;
    s.bulletPenetrationDistanceModifier = 3.0f;
    s.bulletPenetrationDamageModifier = 0.16f;
    s.bulletsPenetrate = true;
    return s;
}

SurfaceData_t CBulletPenetration::GetSurfaceConcrete() {
    SurfaceData_t s{};
    s.materialChar = CHAR_TEX_CONCRETE;
    s.bulletPenetrationDistanceModifier = 1.0f;
    s.bulletPenetrationDamageModifier = 0.22f;
    s.bulletsPenetrate = true;
    return s;
}

SurfaceData_t CBulletPenetration::GetSurfaceMetal() {
    SurfaceData_t s{};
    s.materialChar = CHAR_TEX_METAL;
    s.bulletPenetrationDistanceModifier = 1.0f;
    s.bulletPenetrationDamageModifier = 0.20f;
    s.bulletsPenetrate = true;
    return s;
}

SurfaceData_t CBulletPenetration::GetSurfaceFlesh() {
    SurfaceData_t s{};
    s.materialChar = CHAR_TEX_FLESH;
    s.bulletPenetrationDistanceModifier = 1.0f;
    s.bulletPenetrationDamageModifier = 0.16f;
    s.bulletsPenetrate = true;
    return s;
}

SurfaceData_t CBulletPenetration::GetSurfaceCardboard() {
    SurfaceData_t s{};
    s.materialChar = CHAR_TEX_CARDBOARD;
    s.bulletPenetrationDistanceModifier = 3.0f;
    s.bulletPenetrationDamageModifier = 0.10f;
    s.bulletsPenetrate = true;
    return s;
}

SurfaceData_t CBulletPenetration::GetSurfaceGlass() {
    SurfaceData_t s{};
    s.materialChar = CHAR_TEX_GLASS;
    s.bulletPenetrationDistanceModifier = 3.0f;
    s.bulletPenetrationDamageModifier = 0.05f;
    s.bulletsPenetrate = true;
    return s;
}

SurfaceData_t CBulletPenetration::GetSurfacePlastic() {
    SurfaceData_t s{};
    s.materialChar = CHAR_TEX_PLASTIC;
    s.bulletPenetrationDistanceModifier = 2.0f;
    s.bulletPenetrationDamageModifier = 0.16f;
    s.bulletsPenetrate = true;
    return s;
}

SurfaceData_t CBulletPenetration::GetSurfaceProperties(uint16_t materialChar) {
    switch (materialChar) {
        case CHAR_TEX_WOOD:         return GetSurfaceWood();
        case CHAR_TEX_CONCRETE:     return GetSurfaceConcrete();
        case CHAR_TEX_METAL:        return GetSurfaceMetal();
        case CHAR_TEX_FLESH:        return GetSurfaceFlesh();
        case CHAR_TEX_CARDBOARD:    return GetSurfaceCardboard();
        case CHAR_TEX_GLASS:
        case CHAR_TEX_SHATTERGLASS: return GetSurfaceGlass();
        case CHAR_TEX_PLASTIC:      return GetSurfacePlastic();
        default:                    return GetSurfaceConcrete();
    }
}

// -------------------------------------------------------------------------
// Authoritative Surface Penetration Evaluation (Kinetic Dissipation ODE)
// Reconstructed from EvaluateBulletPenetration @ RVA 0x00a90280 / VA 0x180a90280
// -------------------------------------------------------------------------
bool CBulletPenetration::EvaluateSurfacePenetration(BulletData_t& bullet,
                                                   const SurfaceHitData_t& hit,
                                                   bool ffBulletPenetration)
{
    // Interval Type 1: Air Travel (flags bit 0 == 0)
    if ((hit.flags & 1) == 0) {
        const float segmentStart = bullet.maxRange * hit.tEnter;
        const float segmentEnd   = bullet.maxRange * hit.tExit;
        const float segmentDist  = std::max(0.0f, segmentEnd - segmentStart);

        bullet.currentDamage = ApplyDistanceFalloff(bullet.currentDamage,
                                                   bullet.rangeModifier,
                                                   segmentDist);
        return true;
    }

    // Interval Type 2: Solid Material Penetration (flags bit 0 == 1)
    if (!hit.enterSurface.bulletsPenetrate || !hit.exitSurface.bulletsPenetrate) {
        bullet.stopped = true;
        return false;
    }

    if (bullet.penetrationCount <= 0) {
        bullet.stopped = true;
        return false;
    }

    const float totalDistanceTraveled = bullet.maxRange * hit.tExit;
    if (totalDistanceTraveled > MAX_PENETRATION_DISTANCE) {
        bullet.stopped = true;
        return false;
    }

    // Friendly fire flesh check
    if (hit.enterSurface.materialChar == CHAR_TEX_FLESH && hit.isTeammate && !ffBulletPenetration) {
        bullet.stopped = true;
        return false;
    }

    // Measure material thickness
    const float thickness = (hit.exitPoint - hit.enterPoint).Length();

    // Determine Effective Penetration Modifier & Shock Absorption
    float penModifier = hit.enterSurface.bulletPenetrationDistanceModifier;
    float damageModifier = hit.enterSurface.bulletPenetrationDamageModifier;

    if (hit.enterSurface.materialChar == hit.exitSurface.materialChar) {
        const uint16_t mat = hit.enterSurface.materialChar;
        if (mat == CHAR_TEX_WOOD || mat == CHAR_TEX_CARDBOARD) {
            penModifier = 3.0f;
        } else if (mat == CHAR_TEX_PLASTIC) {
            penModifier = 2.0f;
        } else if (mat == CHAR_TEX_GLASS || mat == CHAR_TEX_SHATTERGLASS) {
            penModifier = 3.0f;
            damageModifier = 0.05f;
        }

        // Grate / chainlink check
        if (hit.enterIsGrate && hit.exitIsGrate) {
            penModifier = 32.0f;
            damageModifier = 0.00001f;
        }
    } else {
        penModifier = std::min(hit.enterSurface.bulletPenetrationDistanceModifier,
                               hit.exitSurface.bulletPenetrationDistanceModifier);
        damageModifier = 0.16f;
    }

    if (penModifier < MIN_PENETRATION_MODIFIER) {
        bullet.stopped = true;
        return false;
    }

    // Kinetic Energy Dissipation ODE:
    // penModInv = max(0.0, 1.0 / penMod)
    // lostDist  = (thickness^2 * penModInv) / 24.0
    // lostEntry = max(0.0, (3.0 / penPower) * 1.25) * (penModInv * 3.0) + (damageMod * D_curr)
    // D_exit    = D_curr - (lostDist + lostEntry)
    const float penModInv = std::max(0.0f, 1.0f / penModifier);
    const float lostDistance = (thickness * thickness * penModInv) / 24.0f;
    const float penPowerTerm = std::max(0.0f, (3.0f / bullet.penetrationPower) * 1.25f);
    const float lostEntry = penPowerTerm * (penModInv * 3.0f) + (damageModifier * bullet.currentDamage);
    const float totalLost = lostDistance + lostEntry;

    if (totalLost >= bullet.currentDamage || (bullet.currentDamage - totalLost) < 1.0f) {
        bullet.currentDamage = 0.0f;
        bullet.stopped = true;
        return false;
    }

    bullet.currentDamage -= totalLost;
    bullet.penetrationCount--;
    return true;
}

bool CBulletPenetration::EvaluateSurfacePenetrationDetailed(
    const WeaponBallistics_t& weapon,
    const SurfaceData_t& enterSurf,
    const SurfaceData_t& exitSurf,
    float thickness,
    float currentDistance,
    float& currentDamage,
    int& remainingPenetrations,
    PenetrationStep_t& stepOut)
{
    stepOut.m_sMaterialName = (enterSurf.materialChar == CHAR_TEX_WOOD) ? "wood" :
                              (enterSurf.materialChar == CHAR_TEX_CONCRETE) ? "concrete" :
                              (enterSurf.materialChar == CHAR_TEX_METAL) ? "solid_metal" :
                              (enterSurf.materialChar == CHAR_TEX_FLESH) ? "player_flesh" :
                              (enterSurf.materialChar == CHAR_TEX_CARDBOARD) ? "cardboard_drywall" :
                              (enterSurf.materialChar == CHAR_TEX_GLASS) ? "glass" : "default";
    stepOut.m_flThickness = thickness;
    stepOut.m_flDamageBefore = currentDamage;
    stepOut.m_bPenetrated = false;

    if (!enterSurf.bulletsPenetrate || !exitSurf.bulletsPenetrate) {
        stepOut.m_sFailReason = "Material flags reject penetration";
        return false;
    }

    if (remainingPenetrations <= 0) {
        stepOut.m_sFailReason = "Penetration count exhausted (0 remaining)";
        return false;
    }

    if (currentDistance > MAX_PENETRATION_DISTANCE) {
        stepOut.m_sFailReason = "Total bullet distance exceeded 3,000 units";
        return false;
    }

    float penModifier = enterSurf.bulletPenetrationDistanceModifier;
    float damageModifier = enterSurf.bulletPenetrationDamageModifier;

    if (enterSurf.materialChar == exitSurf.materialChar) {
        if (enterSurf.materialChar == CHAR_TEX_WOOD || enterSurf.materialChar == CHAR_TEX_CARDBOARD) {
            penModifier = 3.0f;
        } else if (enterSurf.materialChar == CHAR_TEX_PLASTIC) {
            penModifier = 2.0f;
        } else if (enterSurf.materialChar == CHAR_TEX_GLASS || enterSurf.materialChar == CHAR_TEX_SHATTERGLASS) {
            penModifier = 3.0f;
            damageModifier = 0.05f;
        }
    } else {
        penModifier = std::min(enterSurf.bulletPenetrationDistanceModifier, exitSurf.bulletPenetrationDistanceModifier);
        damageModifier = 0.16f;
    }

    if (penModifier < MIN_PENETRATION_MODIFIER) {
        stepOut.m_sFailReason = "Penetration modifier too low (< 0.10)";
        return false;
    }

    const float penModInv = std::max(0.0f, 1.0f / penModifier);
    const float lostDistance = (thickness * thickness * penModInv) / 24.0f;
    const float penPowerTerm = std::max(0.0f, (3.0f / weapon.m_flPenetrationPower) * 1.25f);
    const float lostEntry = penPowerTerm * (penModInv * 3.0f) + (damageModifier * currentDamage);
    const float totalLost = lostDistance + lostEntry;

    if (totalLost >= currentDamage || (currentDamage - totalLost) < 1.0f) {
        stepOut.m_flDamageLost = currentDamage;
        currentDamage = 0.0f;
        stepOut.m_flDamageAfter = 0.0f;
        stepOut.m_sFailReason = "Kinetic energy fully dissipated (< 1.0 dmg remaining)";
        return false;
    }

    currentDamage -= totalLost;
    remainingPenetrations--;

    stepOut.m_flDamageLost = totalLost;
    stepOut.m_flDamageAfter = currentDamage;
    stepOut.m_bPenetrated = true;
    return true;
}

// -------------------------------------------------------------------------
// Authoritative HitGroup Coverage Check (8-Way Dispatch Table)
// server.dll: CCSPlayerPawn::IsArmored @ RVA 0x00acaca0 / VA 0x180acaca0
// -------------------------------------------------------------------------
bool CBulletPenetration::IsArmored(const PlayerPawnState_t& pawn, HitGroup_t hitGroup) noexcept {
    if (pawn.m_ArmorValue <= 0) return false;

    switch (hitGroup) {
        case HITGROUP_GENERIC:
        case HITGROUP_CHEST:
        case HITGROUP_STOMACH:
        case HITGROUP_LEFTARM:
        case HITGROUP_RIGHTARM:
        case HITGROUP_NECK:
            return true;

        case HITGROUP_HEAD:
            // Checks if pawn possesses helmet
            return pawn.m_bHasHelmet;

        case HITGROUP_LEFTLEG:
        case HITGROUP_RIGHTLEG:
        case HITGROUP_GEAR:
        default:
            return false;
    }
}

// -------------------------------------------------------------------------
// Authoritative Armor Mitigation & Durability Depletion Calculation
// server.dll: CCSPlayerPawn::ApplyArmorDamageCalculation @ RVA 0x00a81610 / VA 0x180a81610
// -------------------------------------------------------------------------
ArmorMitigationResult_t CBulletPenetration::ApplyArmorDamageCalculation(
    PlayerPawnState_t& pawn,
    float incomingDamage,
    float weaponArmorRatio,
    HitGroup_t hitGroup,
    bool isHeavyArmorMode) noexcept
{
    ArmorMitigationResult_t result{};
    float flHealthDamage = incomingDamage;
    int32_t nArmorAbsorbed = 0;
    const bool bArmored = IsArmored(pawn, hitGroup);
    result.m_bArmoredCovered = bArmored;

    if (bArmored && incomingDamage > 0.0f) {
        const float flArmorBonus = isHeavyArmorMode ? 1.0f : 0.5f;
        const float flArmorRatio = weaponArmorRatio * 0.5f;

        float flNewDamage = incomingDamage * flArmorRatio;
        float flArmorDamage = (incomingDamage - flNewDamage) * flArmorBonus;
        const float flCurrentArmor = static_cast<float>(pawn.m_ArmorValue);

        if (flArmorDamage > flCurrentArmor) {
            // Durability exhaustion overflow: absorbs up to remaining AP
            flNewDamage = incomingDamage - (flCurrentArmor / flArmorBonus);
            nArmorAbsorbed = pawn.m_ArmorValue;
            pawn.m_ArmorValue = 0;
        } else {
            if (flArmorDamage < 0.0f) flArmorDamage = 1.0f;
            nArmorAbsorbed = static_cast<int32_t>(flArmorDamage);
            pawn.m_ArmorValue -= nArmorAbsorbed;
        }
        flHealthDamage = flNewDamage;
    }

    pawn.m_iHealth = std::max(0, pawn.m_iHealth - static_cast<int32_t>(flHealthDamage));

    result.m_flHealthDamage = flHealthDamage;
    result.m_iArmorAbsorbed = nArmorAbsorbed;
    result.m_iRemainingArmor = pawn.m_ArmorValue;
    result.m_iRemainingHealth = pawn.m_iHealth;
    return result;
}

// -------------------------------------------------------------------------
// 4-Unit Discrete Ray Interval Stepping Helper
// -------------------------------------------------------------------------
bool CBulletPenetration::TraceToExit(
    const Vector3& vStart,
    const Vector3& vDirection,
    float maxDistance,
    Vector3& outExitPoint,
    float stepSize)
{
    float dist = 0.0f;
    Vector3 current = vStart;
    const float len = std::sqrt(vDirection.x * vDirection.x + vDirection.y * vDirection.y + vDirection.z * vDirection.z);
    if (len <= 1e-6f) return false;
    const Vector3 dirNorm = { vDirection.x / len, vDirection.y / len, vDirection.z / len };

    while (dist <= maxDistance) {
        dist += stepSize;
        current.x = vStart.x + dirNorm.x * dist;
        current.y = vStart.y + dirNorm.y * dist;
        current.z = vStart.z + dirNorm.z * dist;
    }

    outExitPoint = current;
    return true;
}

bool CBulletPenetration::ClipTraceToPlayers(
    const Vector3& vStart,
    const Vector3& vEnd,
    const std::vector<HitPlayerPawn_t>& players,
    uint32_t attackerHandle,
    SurfaceHitData_t& outHit)
{
    const Vector3 rayDir = { vEnd.x - vStart.x, vEnd.y - vStart.y, vEnd.z - vStart.z };
    const float rayLength = std::sqrt(rayDir.x * rayDir.x + rayDir.y * rayDir.y + rayDir.z * rayDir.z);
    if (rayLength <= 1e-6f) return false;

    float closestFraction = 1.0f;
    bool hitAny = false;

    for (const auto& player : players) {
        if (!player.isAlive || player.entityHandle == attackerHandle) continue;

        // Player bounding box check
        const Vector3 worldMins = { player.origin.x + player.mins.x, player.origin.y + player.mins.y, player.origin.z + player.mins.z };
        const Vector3 worldMaxs = { player.origin.x + player.maxs.x, player.origin.y + player.maxs.y, player.origin.z + player.maxs.z };

        float tNear = 0.0f, tFar = 1.0f;
        // Simple AABB intersection test
        for (int i = 0; i < 3; ++i) {
            const float o = (i == 0) ? vStart.x : (i == 1) ? vStart.y : vStart.z;
            const float d = (i == 0) ? rayDir.x : (i == 1) ? rayDir.y : rayDir.z;
            const float minVal = (i == 0) ? worldMins.x : (i == 1) ? worldMins.y : worldMins.z;
            const float maxVal = (i == 0) ? worldMaxs.x : (i == 1) ? worldMaxs.y : worldMaxs.z;

            if (std::abs(d) < 1e-7f) {
                if (o < minVal || o > maxVal) { tNear = 1.0f; tFar = -1.0f; break; }
            } else {
                float invD = 1.0f / d;
                float t0 = (minVal - o) * invD;
                float t1 = (maxVal - o) * invD;
                if (t0 > t1) std::swap(t0, t1);
                tNear = std::max(tNear, t0);
                tFar = std::min(tFar, t1);
                if (tNear > tFar) break;
            }
        }

        if (tNear <= tFar && tNear >= 0.0f && tNear < closestFraction) {
            closestFraction = tNear;
            hitAny = true;
            outHit.tEnter = tNear;
            outHit.tExit = std::min(1.0f, tFar);
            outHit.flags = 1; // Solid material
            outHit.enterPoint = { vStart.x + rayDir.x * tNear, vStart.y + rayDir.y * tNear, vStart.z + rayDir.z * tNear };
            outHit.exitPoint = { vStart.x + rayDir.x * tFar, vStart.y + rayDir.y * tFar, vStart.z + rayDir.z * tFar };
            outHit.enterSurface = GetSurfaceFlesh();
            outHit.exitSurface = GetSurfaceFlesh();
            outHit.hitEntityHandle = player.entityHandle;
            outHit.hitGroupId = static_cast<int>(HITGROUP_CHEST);
        }
    }

    return hitAny;
}

std::vector<BulletImpact_t> CBulletPenetration::FireBulletsAuthoritative(
    const FireBulletParams_t& params,
    const std::vector<SurfaceHitData_t>& worldGeometry,
    std::vector<HitPlayerPawn_t>& players,
    bool ffBulletPenetration)
{
    std::vector<BulletImpact_t> impacts;

    BulletData_t bullet{};
    bullet.currentDamage = params.baseDamage;
    bullet.penetrationPower = params.flPenetrationPower;
    bullet.rangeModifier = params.flRangeModifier;
    bullet.maxRange = params.flMaxRange;
    bullet.penetrationCount = params.iPenetrationCount;
    bullet.stopped = false;

    // Check world geometry intervals
    for (const auto& hit : worldGeometry) {
        if (bullet.stopped) break;

        const float distToEntry = bullet.maxRange * hit.tEnter;
        bullet.currentDamage = ApplyDistanceFalloff(bullet.currentDamage, bullet.rangeModifier, distToEntry);

        BulletImpact_t imp{};
        imp.position = hit.enterPoint;
        imp.normal = hit.enterNormal;
        imp.damageApplied = bullet.currentDamage;
        imp.totalDistance = distToEntry;
        imp.hitEntityHandle = hit.hitEntityHandle;
        imp.hitGroupId = hit.hitGroupId;
        impacts.push_back(imp);

        EvaluateSurfacePenetration(bullet, hit, ffBulletPenetration);
    }

    // Clip trace to player pawns
    Vector3 vEnd = {
        params.vStart.x + params.vDirection.x * params.flMaxRange,
        params.vStart.y + params.vDirection.y * params.flMaxRange,
        params.vStart.z + params.vDirection.z * params.flMaxRange
    };
    SurfaceHitData_t playerHit{};
    if (ClipTraceToPlayers(params.vStart, vEnd, players, params.attackerHandle, playerHit)) {
        for (auto& p : players) {
            if (p.entityHandle == playerHit.hitEntityHandle) {
                const float dist = (playerHit.enterPoint - params.vStart).Length();
                float dmg = ApplyDistanceFalloff(params.baseDamage, params.flRangeModifier, dist);
                ApplyArmorDamageCalculation(p.pawnState, dmg, params.flArmorRatio, static_cast<HitGroup_t>(playerHit.hitGroupId));

                BulletImpact_t imp{};
                imp.position = playerHit.enterPoint;
                imp.normal = playerHit.enterNormal;
                imp.damageApplied = dmg;
                imp.totalDistance = dist;
                imp.hitEntityHandle = p.entityHandle;
                imp.hitGroupId = playerHit.hitGroupId;
                impacts.push_back(imp);
                break;
            }
        }
    }

    return impacts;
}

} // namespace cs2::server
