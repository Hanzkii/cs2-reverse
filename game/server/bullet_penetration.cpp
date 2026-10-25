#include "bullet_penetration.h"
#include <limits>
#include <random>

namespace cs2::server {

// -------------------------------------------------------------------------
// Authoritative Distance Falloff Formula
// Reverse engineered from FUN_180a8fca0 @ 0x180a8fd0a in server.dll:
//   DIVSS XMM7, dword ptr [0x18178c014]   ; 500.0f
//   CALL powf                             ; powf(rangeModifier, distance / 500.0f)
//   MULSS XMM0, XMM6                      ; * baseDamage
// -------------------------------------------------------------------------
float CBulletPenetration::ApplyDistanceFalloff(float baseDamage, float rangeModifier, float distance) {
    if (rangeModifier <= 0.0f || baseDamage <= 0.0f) {
        return 0.0f;
    }
    const float exponent = distance / 500.0f;
    return baseDamage * std::pow(rangeModifier, exponent);
}

// -------------------------------------------------------------------------
// Authoritative Surface Penetration Evaluation
// Reverse engineered from FUN_180a8fca0 in server.dll:
// - Evaluates air travel intervals via distance falloff
// - Evaluates solid surface material absorption via Rubikon descriptors
// - Computes entry, exit, and distance damage absorption
// -------------------------------------------------------------------------
bool CBulletPenetration::EvaluateSurfacePenetration(BulletData_t& bullet,
                                                   const SurfaceHitData_t& hit,
                                                   bool ffBulletPenetration) {
    // ---------------------------------------------------------------------
    // Interval Type 1: Air Travel (flags bit 0 == 0)
    // ---------------------------------------------------------------------
    if ((hit.flags & 1) == 0) {
        const float segmentStart = bullet.maxRange * hit.tEnter;
        const float segmentEnd   = bullet.maxRange * hit.tExit;
        const float segmentDist  = std::max(0.0f, segmentEnd - segmentStart);

        bullet.currentDamage = ApplyDistanceFalloff(bullet.currentDamage,
                                                   bullet.rangeModifier,
                                                   segmentDist);
        return true;
    }

    // ---------------------------------------------------------------------
    // Interval Type 2: Solid Material Penetration (flags bit 0 == 1)
    // ---------------------------------------------------------------------
    float penMod = hit.enterSurface.bulletPenetrationDistanceModifier;
    const float totalDistanceTraveled = bullet.maxRange * hit.tExit;

    // Failure condition check @ 0x180a8fdf0:
    // If distance > 3000 units, penMod < 0.1, or no penetration power remains
    if (totalDistanceTraveled > 3000.0f || penMod < 0.1f || bullet.penetrationCount < 1) {
        bullet.stopped = true;
        return false;
    }

    // Measure material thickness via Euclidean distance between enter and exit
    const float thickness = (hit.exitPoint - hit.enterPoint).Length();

    // Friendly fire flesh penetration check @ 0x180a90107
    if (hit.enterSurface.materialChar == CHAR_TEX_FLESH && hit.isTeammate && !ffBulletPenetration) {
        bullet.stopped = true;
        return false;
    }

    // Default damage modifier constant @ 0x1819196c8
    float damageModifier = 0.16f;

    // ---------------------------------------------------------------------
    // Material Pairing & Thickness Discriminators @ 0x180a9028a - 0x180a90328
    // ---------------------------------------------------------------------
    if (hit.enterSurface.materialChar == hit.exitSurface.materialChar) {
        // Same material on entry and exit faces
        const uint16_t mat = hit.enterSurface.materialChar;

        // Wood ('W') and Cardboard ('U') have low density absorption
        if (mat == CHAR_TEX_CARDBOARD || mat == CHAR_TEX_WOOD) {
            penMod = 3.0f;
        } else if (mat == CHAR_TEX_PLASTIC) {
            penMod = 2.0f;
        }

        // Thin surface branch: thickness < 6.0 units (@ 0x180a902c4)
        if (thickness < 6.0f) {
            // Glass ('G') and Shatterglass ('Y')
            if (mat == CHAR_TEX_GLASS || mat == CHAR_TEX_SHATTERGLASS) {
                damageModifier = 0.05f;
                penMod = 3.0f;
            }

            // Chainlink fence / wire mesh (Bit 13 set on both surfaces @ 0x180a902eb)
            if (hit.enterIsGrate && hit.exitIsGrate) {
                penMod = 32.0f;
                damageModifier = 0.00001f; // Virtually zero damage lost
            }
        } else {
            // Thick surface branch: thickness >= 6.0 units
            if (hit.enterIsGrate && hit.exitIsGrate) {
                penMod = 3.0f;
            }
        }
    } else {
        // Multi-material composite / transition: minimum penetration modifier applies
        penMod = std::min(hit.enterSurface.bulletPenetrationDistanceModifier,
                          hit.exitSurface.bulletPenetrationDistanceModifier);
        damageModifier = 0.16f;
    }

    // ---------------------------------------------------------------------
    // Authoritative Damage Loss Formulation @ 0x180a90328 - 0x180a903d1
    // ---------------------------------------------------------------------
    // penModInv = max(0.0f, 1.0f / penMod)
    const float penModInv = std::max(0.0f, 1.0f / penMod);

    // lostDistance = (thickness^2 * penModInv) / 24.0f
    const float lostDistance = (thickness * thickness * penModInv) / 24.0f;

    // lostEntry = max(0.0f, (3.0f / penPower) * 1.25f) * (penModInv * 3.0f) + (damageModifier * currentDamage)
    const float penPowerTerm = std::max(0.0f, (3.0f / std::max(0.01f, bullet.penetrationPower)) * 1.25f);
    const float lostEntry    = (penPowerTerm * (penModInv * 3.0f)) + (damageModifier * bullet.currentDamage);

    const float totalLost = lostDistance + lostEntry;
    const float remainingDamage = bullet.currentDamage - totalLost;

    bullet.currentDamage = remainingDamage;

    // Damage threshold check @ 0x180a9049a:
    // If remaining damage < 1.0f, the bullet is spent and stopped in the surface
    if (remainingDamage < 1.0f) {
        bullet.stopped = true;
        return false;
    }

    // Decrement remaining penetrated object counter
    bullet.penetrationCount--;
    bullet.stopped = false;
    return true;
}

// -------------------------------------------------------------------------
// Authoritative ClipTraceToPlayers Implementation
// Reverse engineered from FUN_180181cd0 in server.dll
// -------------------------------------------------------------------------
bool CBulletPenetration::ClipTraceToPlayers(const Vector3& vStart,
                                           const Vector3& vEnd,
                                           const std::vector<HitPlayerPawn_t>& players,
                                           uint32_t attackerHandle,
                                           SurfaceHitData_t& outHit) {
    bool bHitAny = false;
    float flSmallestFraction = 1.0f;
    const Vector3 vRayDelta = vEnd - vStart;
    const float flRayLength = vRayDelta.Length();

    if (flRayLength < 1e-4f) {
        return false;
    }

    const Vector3 vRayDir = vRayDelta / flRayLength;

    // Skin margin expansion constant (+40.0 units for player hull bounding box query)
    constexpr float kPlayerSkinMargin = 40.0f;

    for (const auto& player : players) {
        if (!player.isAlive || player.entityHandle == attackerHandle) {
            continue;
        }

        // 1. Broadphase: Expand player bounding box by skin margin
        const Vector3 boxMin = player.origin + player.mins - Vector3{kPlayerSkinMargin, kPlayerSkinMargin, kPlayerSkinMargin};
        const Vector3 boxMax = player.origin + player.maxs + Vector3{kPlayerSkinMargin, kPlayerSkinMargin, kPlayerSkinMargin};

        // Ray vs AABB slab intersection
        float tmin = 0.0f;
        float tmax = flRayLength;
        bool bBoxIntersect = true;

        for (int i = 0; i < 3; ++i) {
            const float rStart = (i == 0) ? vStart.x : (i == 1 ? vStart.y : vStart.z);
            const float rDir   = (i == 0) ? vRayDir.x : (i == 1 ? vRayDir.y : vRayDir.z);
            const float bMin   = (i == 0) ? boxMin.x : (i == 1 ? boxMin.y : boxMin.z);
            const float bMax   = (i == 0) ? boxMax.x : (i == 1 ? boxMax.y : boxMax.z);

            if (std::abs(rDir) < 1e-8f) {
                if (rStart < bMin || rStart > bMax) {
                    bBoxIntersect = false;
                    break;
                }
            } else {
                float invD = 1.0f / rDir;
                float t0 = (bMin - rStart) * invD;
                float t1 = (bMax - rStart) * invD;
                if (t0 > t1) std::swap(t0, t1);
                tmin = std::max(tmin, t0);
                tmax = std::min(tmax, t1);
                if (tmin > tmax) {
                    bBoxIntersect = false;
                    break;
                }
            }
        }

        if (!bBoxIntersect || tmin > flSmallestFraction * flRayLength) {
            continue;
        }

        // 2. Narrowphase: Iterate player hitboxes (capsules / AABBs)
        for (const auto& hitbox : player.hitboxes) {
            const Vector3 hbCenter = player.origin + (hitbox.mins + hitbox.maxs) * 0.5f;
            const float hbRadius = hitbox.radius > 0.0f ? hitbox.radius : (hitbox.maxs - hitbox.mins).Length() * 0.5f;

            // Sphere / capsule raycast approximation
            const Vector3 vToSphere = hbCenter - vStart;
            const float tProj = vToSphere.Dot(vRayDir);

            if (tProj < 0.0f || tProj > flRayLength) {
                continue;
            }

            const Vector3 vClosest = vStart + vRayDir * tProj;
            const float distSqr = (hbCenter - vClosest).LengthSqr();

            if (distSqr <= hbRadius * hbRadius) {
                const float dDist = std::sqrt(std::max(0.0f, hbRadius * hbRadius - distSqr));
                const float tHit = std::max(0.0f, tProj - dDist);
                const float fraction = tHit / flRayLength;

                if (fraction < flSmallestFraction) {
                    flSmallestFraction = fraction;
                    bHitAny = true;

                    outHit.tEnter = fraction;
                    outHit.tExit = std::min(1.0f, (tProj + dDist) / flRayLength);
                    outHit.flags = 1; // Solid material
                    outHit.enterPoint = vStart + vRayDir * tHit;
                    outHit.exitPoint  = vStart + vRayDir * (tProj + dDist);
                    outHit.enterNormal = (outHit.enterPoint - hbCenter).Normalized();
                    outHit.exitNormal  = (outHit.exitPoint - hbCenter).Normalized();
                    outHit.enterSurface = GetSurfaceProperties(CHAR_TEX_FLESH);
                    outHit.exitSurface  = GetSurfaceProperties(CHAR_TEX_FLESH);
                    outHit.hitEntityHandle = player.entityHandle;
                    outHit.hitGroupId = hitbox.hitGroup;
                }
            }
        }
    }

    return bHitAny;
}

// -------------------------------------------------------------------------
// Authoritative FireBullets Simulation
// Reverse engineered from CCSPlayerPawn::FireBullet (FUN_1802975d0)
// and FUN_180a70240 in server.dll
// -------------------------------------------------------------------------
std::vector<BulletImpact_t> CBulletPenetration::FireBulletsAuthoritative(
    const FireBulletParams_t& params,
    const std::vector<SurfaceHitData_t>& worldGeometry,
    const std::vector<HitPlayerPawn_t>& players,
    bool ffBulletPenetration) {

    std::vector<BulletImpact_t> allImpacts;

    for (int pellet = 0; pellet < std::max(1, params.numPellets); ++pellet) {
        BulletData_t bullet;
        bullet.currentDamage    = params.baseDamage;
        bullet.penetrationPower = params.flPenetrationPower;
        bullet.rangeModifier    = params.flRangeModifier;
        bullet.maxRange         = params.flMaxRange;
        bullet.penetrationCount = params.iPenetrationCount;
        bullet.stopped          = false;

        Vector3 rayDir = params.vDirection.Normalized();

        // Integrate spread / inaccuracy perturbation if multiple pellets or spread > 0
        if (params.flSpread > 0.0f || params.flInaccuracy > 0.0f) {
            float totalSpread = params.flSpread + params.flInaccuracy;
            float angle = (float)pellet * (6.2831853f / (float)std::max(1, params.numPellets));
            rayDir.x += std::cos(angle) * totalSpread * 0.05f;
            rayDir.y += std::sin(angle) * totalSpread * 0.05f;
            rayDir = rayDir.Normalized();
        }

        const Vector3 vRayStart = params.vStart;
        const Vector3 vRayEnd   = params.vStart + rayDir * params.flMaxRange;

        // Collect all candidate intervals (world geometry + player hitboxes)
        std::vector<SurfaceHitData_t> intervals = worldGeometry;

        SurfaceHitData_t playerHit;
        if (ClipTraceToPlayers(vRayStart, vRayEnd, players, params.attackerHandle, playerHit)) {
            intervals.push_back(playerHit);
        }

        // Sort intervals in topological distance order along ray
        std::sort(intervals.begin(), intervals.end(),
                  [](const SurfaceHitData_t& a, const SurfaceHitData_t& b) {
                      return a.tEnter < b.tEnter;
                  });

        float totalTravelDistance = 0.0f;
        Vector3 lastPoint = vRayStart;

        for (size_t i = 0; i < intervals.size(); ++i) {
            SurfaceHitData_t& interval = intervals[i];

            // 1. Distance falloff across air gap prior to this interval
            float gapDist = (interval.enterPoint - lastPoint).Length();
            if (gapDist > 0.0f) {
                bullet.currentDamage = ApplyDistanceFalloff(bullet.currentDamage,
                                                           bullet.rangeModifier,
                                                           gapDist);
                totalTravelDistance += gapDist;
            }

            // 2. Solid impact event emission
            if (interval.flags & 1) {
                BulletImpact_t impact;
                impact.position = interval.enterPoint;
                impact.normal = interval.enterNormal;
                impact.damageApplied = bullet.currentDamage;
                impact.totalDistance = totalTravelDistance;
                impact.objectIndex = static_cast<int>(i);
                impact.hitEntityHandle = interval.hitEntityHandle;
                impact.hitGroupId = interval.hitGroupId;
                allImpacts.push_back(impact);

                // 3. Evaluate surface penetration
                bool penetrated = EvaluateSurfacePenetration(bullet, interval, ffBulletPenetration);
                totalTravelDistance += (interval.exitPoint - interval.enterPoint).Length();
                lastPoint = interval.exitPoint;

                if (!penetrated || bullet.stopped) {
                    break; // Bullet energy exhausted
                }
            } else {
                lastPoint = interval.exitPoint;
            }
        }
    }

    return allImpacts;
}

// -------------------------------------------------------------------------
// Physics Material Default Lookup Table
// -------------------------------------------------------------------------
SurfaceData_t CBulletPenetration::GetSurfaceProperties(uint16_t materialChar) {
    SurfaceData_t data;
    data.materialChar = materialChar;
    data.bulletsPenetrate = true;

    switch (materialChar) {
        case CHAR_TEX_CONCRETE:
            data.bulletPenetrationDistanceModifier = 1.0f;
            data.bulletPenetrationDamageModifier = 0.16f;
            break;

        case CHAR_TEX_WOOD:
            data.bulletPenetrationDistanceModifier = 3.0f; // High penetration through wood
            data.bulletPenetrationDamageModifier = 0.16f;
            break;

        case CHAR_TEX_CARDBOARD:
            data.bulletPenetrationDistanceModifier = 3.0f;
            data.bulletPenetrationDamageModifier = 0.16f;
            break;

        case CHAR_TEX_PLASTIC:
            data.bulletPenetrationDistanceModifier = 2.0f;
            data.bulletPenetrationDamageModifier = 0.16f;
            break;

        case CHAR_TEX_METAL:
            data.bulletPenetrationDistanceModifier = 0.5f; // Hard to penetrate solid metal
            data.bulletPenetrationDamageModifier = 0.16f;
            break;

        case CHAR_TEX_GLASS:
        case CHAR_TEX_SHATTERGLASS:
            data.bulletPenetrationDistanceModifier = 3.0f;
            data.bulletPenetrationDamageModifier = 0.05f; // Very low damage loss on thin glass
            break;

        case CHAR_TEX_FLESH:
            data.bulletPenetrationDistanceModifier = 1.0f;
            data.bulletPenetrationDamageModifier = 0.16f;
            break;

        case CHAR_TEX_DIRT:
        case CHAR_TEX_SAND:
            data.bulletPenetrationDistanceModifier = 0.8f;
            data.bulletPenetrationDamageModifier = 0.16f;
            break;

        default:
            data.bulletPenetrationDistanceModifier = 1.0f;
            data.bulletPenetrationDamageModifier = 0.16f;
            break;
    }

    return data;
}

} // namespace cs2::server
