#include "inferno_fire_grid.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <limits>

namespace cs2::particles {

// ----------------------------------------------------------------------------
// Analytical Helper: Computes orthogonal surface tangent vector
// ----------------------------------------------------------------------------
static Vector3 ComputeSurfaceTangent(const Vector3& dir2D, const Vector3& normal) noexcept {
    // Project horizontal direction onto tangent plane defined by surface normal:
    // v_tangent = v - (v . n) * n
    const float dot = dir2D.Dot(normal);
    Vector3 tangent = dir2D - (normal * dot);
    const float len = tangent.Length();
    return (len > 1e-4f) ? (tangent / len) : dir2D;
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
CInfernoFireGrid::CInfernoFireGrid() {
    std::memset(m_firePositions, 0, sizeof(m_firePositions));
    std::memset(m_fireParentPositions, 0, sizeof(m_fireParentPositions));
    std::memset(m_bFireIsBurning, 0, sizeof(m_bFireIsBurning));
    std::memset(m_BurnNormal, 0, sizeof(m_BurnNormal));
}

// ----------------------------------------------------------------------------
// Default Terrain Raycaster (Analytical Plane & Wave Displacement)
// ----------------------------------------------------------------------------
TerrainTraceHit_t CInfernoFireGrid::DefaultTerrainRaycast(const Vector3& start, const Vector3& end) noexcept {
    TerrainTraceHit_t hit{};
    const Vector3 dir = end - start;
    const float totalDist = dir.Length();
    if (totalDist < 1e-5f) {
        return hit;
    }

    // Step-march along ray to detect uneven displacement terrain
    // Analytical displacement height function: z_ground(x, y) = 6.0 * sin(0.02 * x) * cos(0.02 * y)
    constexpr int kSteps = 32;
    const float stepFrac = 1.0f / static_cast<float>(kSteps);

    for (int i = 0; i <= kSteps; ++i) {
        const float t = static_cast<float>(i) * stepFrac;
        const Vector3 p = start + dir * t;

        // Ground elevation at (p.x, p.y)
        const float groundZ = 6.0f * std::sin(0.02f * p.x) * std::cos(0.02f * p.y);

        if (p.z <= groundZ) {
            hit.bHit = true;
            hit.flFraction = t;
            hit.vecHitPosition = Vector3{p.x, p.y, groundZ};

            // Analytical surface gradient / normal: n = (-dz/dx, -dz/dy, 1) normalized
            const float dzdx = 6.0f * 0.02f * std::cos(0.02f * p.x) * std::cos(0.02f * p.y);
            const float dzdy = -6.0f * 0.02f * std::sin(0.02f * p.x) * std::sin(0.02f * p.y);
            const Vector3 unnormNormal{-dzdx, -dzdy, 1.0f};
            hit.vecHitNormal = unnormNormal.Normalized();
            return hit;
        }
    }

    return hit;
}

// ----------------------------------------------------------------------------
// Spawn Combustion Cluster
// ----------------------------------------------------------------------------
void CInfernoFireGrid::Spawn(const Vector3& vecOrigin,
                             int32_t nStartTick,
                             float flCurrentTime,
                             const CSmokeVoxelGrid* pSmoke,
                             const TerrainRaycastFn& terrainQuery)
{
    m_vecOriginalSpawnLocation = vecOrigin;
    m_nFireEffectTickBegin = nStartTick;
    m_flSpawnTime = flCurrentTime;
    m_flLastSpreadTime = flCurrentTime;
    m_flLastDamageTime = flCurrentTime;
    m_fireCount = 0;
    m_nSpreadCount = 0;
    m_nExtinguishedFlames = 0;
    m_bInPostEffectTime = false;
    m_bWasCreatedInSmoke = false;

    std::memset(m_firePositions, 0, sizeof(m_firePositions));
    std::memset(m_fireParentPositions, 0, sizeof(m_fireParentPositions));
    std::memset(m_bFireIsBurning, 0, sizeof(m_bFireIsBurning));
    std::memset(m_BurnNormal, 0, sizeof(m_BurnNormal));

    // Mutual Smoke Interaction Check upon Spawn:
    // If the incendiary projectile detonates directly inside an active smoke cloud volume,
    // it is quenched immediately without propagating any fire nodes.
    if (pSmoke && pSmoke->IsActive()) {
        const float distToSmoke = (vecOrigin - pSmoke->GetOrigin()).Length();
        bool bDetonatedInSmokeVoxel = false;
        int vx = 0, vy = 0, vz = 0;
        if (pSmoke->WorldToVoxel(vecOrigin, vx, vy, vz)) {
            bDetonatedInSmokeVoxel = pSmoke->IsVoxelActive(vx, vy, vz);
        }

        if (distToSmoke <= SMOKE_EXTINGUISH_SPHERE_RADIUS || bDetonatedInSmokeVoxel) {
            m_bWasCreatedInSmoke = true;
            m_bInPostEffectTime = true;
            m_bActive = false;
            return;
        }
    }

    // Ground snapping raycast for seed flame node (Node 0)
    TerrainTraceHit_t hit{};
    const Vector3 traceStart = vecOrigin + Vector3{0.0f, 0.0f, INFERNO_GROUND_SNAP_RAY_UP};
    const Vector3 traceEnd   = vecOrigin - Vector3{0.0f, 0.0f, INFERNO_GROUND_SNAP_RAY_DOWN};

    if (terrainQuery) {
        hit = terrainQuery(traceStart, traceEnd);
    } else {
        hit = DefaultTerrainRaycast(traceStart, traceEnd);
    }

    Vector3 seedPos = vecOrigin;
    Vector3 seedNormal{0.0f, 0.0f, 1.0f};

    if (hit.bHit) {
        seedPos = hit.vecHitPosition + hit.vecHitNormal * INFERNO_GROUND_NORMAL_OFFSET;
        seedNormal = hit.vecHitNormal;
    }

    // Initialize Root Seed Node 0
    m_firePositions[0] = seedPos;
    m_fireParentPositions[0] = vecOrigin;
    m_BurnNormal[0] = seedNormal;
    m_bFireIsBurning[0] = true;

    InfernoFlameNode_t& node0 = m_nodes[0];
    node0.vecPosition = seedPos;
    node0.vecParentPosition = vecOrigin;
    node0.vecNormal = seedNormal;
    node0.flIgnitionTime = flCurrentTime;
    node0.flLifetime = m_nFireLifetime;
    node0.flWarmupTime = INFERNO_NODE_WARMUP_TIME;
    node0.flFadeTime = INFERNO_NODE_FADE_TIME;
    node0.nParentIndex = -1;
    node0.nClusterGeneration = 0;
    node0.bIsBurning = true;
    node0.bExtinguished = false;

    m_fireCount = 1;
    m_bActive = true;
}

// ----------------------------------------------------------------------------
// Per-Tick Simulation Loop (VA 0x180346090 / RVA 0x00346090)
// ----------------------------------------------------------------------------
void CInfernoFireGrid::OnTick(float flCurrentTime,
                             int32_t nCurrentTick,
                             const CSmokeVoxelGrid* pActiveSmoke,
                             const TerrainRaycastFn& terrainQuery)
{
    (void)nCurrentTick;

    if (!m_bActive || m_bInPostEffectTime) {
        return;
    }

    // 1. Check expired nodes and burnout timers (VA 0x180340bc0)
    CheckExpired(flCurrentTime);

    if (!m_bActive || m_bInPostEffectTime) {
        return;
    }

    // 2. Mutual Smoke Voxel Interaction & Extinguishment (RVA 0x00346330-0x0034645f)
    if (pActiveSmoke && pActiveSmoke->IsActive()) {
        ExtinguishOverlappingSmoke(*pActiveSmoke, flCurrentTime);
        if (m_bInPostEffectTime || !m_bActive) {
            return;
        }
    }

    // 3. Dynamic 2D/3D Surface Propagation (VA 0x18034ef00)
    if (flCurrentTime - m_flLastSpreadTime >= INFERNO_SPREAD_INTERVAL) {
        if (m_fireCount < m_nMaxFlames) {
            SpreadStep(flCurrentTime, terrainQuery);
        }
        m_flLastSpreadTime = flCurrentTime;
    }
}

// ----------------------------------------------------------------------------
// Lifecycle & Burnout Evaluation (VA 0x180340bc0 / RVA 0x00340bc0)
// ----------------------------------------------------------------------------
void CInfernoFireGrid::CheckExpired(float flCurrentTime) {
    if (!m_bActive) {
        return;
    }

    int32_t activeCount = 0;
    for (int32_t i = 0; i < m_fireCount; ++i) {
        if (m_nodes[i].bIsBurning && !m_nodes[i].bExtinguished) {
            const float flElapsed = flCurrentTime - m_nodes[i].flIgnitionTime;
            if (flElapsed >= m_nodes[i].flLifetime) {
                m_nodes[i].bIsBurning = false;
                m_bFireIsBurning[i] = false;
            } else {
                ++activeCount;
            }
        }
    }

    if (activeCount == 0 && m_fireCount > 0) {
        m_bInPostEffectTime = true;
        m_bActive = false;
    }
}

// ----------------------------------------------------------------------------
// Dynamic 2D/3D Surface Propagation Algorithm (VA 0x18034ef00 / RVA 0x0034ef00)
// ----------------------------------------------------------------------------
int32_t CInfernoFireGrid::SpreadStep(float flCurrentTime, const TerrainRaycastFn& terrainQuery) {
    if (m_fireCount >= m_nMaxFlames || !m_bActive) {
        return 0;
    }

    int32_t nodesAddedThisStep = 0;
    constexpr int32_t kMaxNewNodesPerStep = 4;
    constexpr int32_t kDirectionAngles = 16;
    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr float kAngleDelta = kTwoPi / static_cast<float>(kDirectionAngles);

    // Snapshot existing count to avoid cascading within same step
    const int32_t snapshotCount = m_fireCount;

    // Pick active burning nodes to expand from
    for (int32_t pIdx = 0; pIdx < snapshotCount; ++pIdx) {
        if (!m_nodes[pIdx].bIsBurning || m_nodes[pIdx].bExtinguished) {
            continue;
        }

        const Vector3 parentPos = m_nodes[pIdx].vecPosition;
        const Vector3 parentNormal = m_nodes[pIdx].vecNormal;

        // Radial angular sampling with directional offset based on spread count
        const float angleOffset = static_cast<float>(m_nSpreadCount * 3 + pIdx) * 0.392699f; // 22.5 deg offset

        for (int32_t d = 0; d < kDirectionAngles; ++d) {
            if (m_fireCount >= m_nMaxFlames || nodesAddedThisStep >= kMaxNewNodesPerStep) {
                break;
            }

            const float angle = angleOffset + static_cast<float>(d) * kAngleDelta;
            const Vector3 dir2D{std::cos(angle), std::sin(angle), 0.0f};

            // Project horizontal expansion vector onto surface tangent plane
            const Vector3 tangent = ComputeSurfaceTangent(dir2D, parentNormal);
            const Vector3 candidatePlanarPos = parentPos + tangent * INFERNO_FLAME_STEP_DISTANCE;

            // Maximum cluster radius limit from seed origin
            const float distFromOrigin2D = std::hypot(
                candidatePlanarPos.x - m_vecOriginalSpawnLocation.x,
                candidatePlanarPos.y - m_vecOriginalSpawnLocation.y
            );
            if (distFromOrigin2D > INFERNO_SPREAD_RADIUS) {
                continue;
            }

            // Ground Snapping Raycast
            const Vector3 traceStart = candidatePlanarPos + Vector3{0.0f, 0.0f, INFERNO_GROUND_SNAP_RAY_UP};
            const Vector3 traceEnd   = candidatePlanarPos - Vector3{0.0f, 0.0f, INFERNO_GROUND_SNAP_RAY_DOWN};

            TerrainTraceHit_t hit{};
            if (terrainQuery) {
                hit = terrainQuery(traceStart, traceEnd);
            } else {
                hit = DefaultTerrainRaycast(traceStart, traceEnd);
            }

            if (!hit.bHit) {
                continue; // No ground surface found under probe
            }

            // Slope Climbing Limit Check (server.dll normal.z check)
            // If surface is steeper than 45 degrees (normal.z < 0.7071f), flames cannot climb
            if (hit.vecHitNormal.z < INFERNO_MAX_CLIMB_SLOPE) {
                continue;
            }

            // Step Height Limitation Check
            const float stepHeight = std::abs(hit.vecHitPosition.z - parentPos.z);
            if (stepHeight > INFERNO_MAX_STEP_HEIGHT) {
                continue;
            }

            const Vector3 finalChildPos = hit.vecHitPosition + hit.vecHitNormal * INFERNO_GROUND_NORMAL_OFFSET;

            // Clearance & Minimum Separation Check against all existing active nodes
            bool bTooClose = false;
            for (int32_t existing = 0; existing < m_fireCount; ++existing) {
                if (!m_nodes[existing].bIsBurning) {
                    continue;
                }
                const float dist = (finalChildPos - m_nodes[existing].vecPosition).Length();
                if (dist < INFERNO_MIN_NODE_SEPARATION) {
                    bTooClose = true;
                    break;
                }
            }
            if (bTooClose) {
                continue;
            }

            // Append child flame node
            const int32_t newIdx = m_fireCount;
            m_firePositions[newIdx] = finalChildPos;
            m_fireParentPositions[newIdx] = parentPos;
            m_BurnNormal[newIdx] = hit.vecHitNormal;
            m_bFireIsBurning[newIdx] = true;

            InfernoFlameNode_t& newNode = m_nodes[newIdx];
            newNode.vecPosition = finalChildPos;
            newNode.vecParentPosition = parentPos;
            newNode.vecNormal = hit.vecHitNormal;
            newNode.flIgnitionTime = flCurrentTime;
            newNode.flLifetime = m_nFireLifetime;
            newNode.flWarmupTime = INFERNO_NODE_WARMUP_TIME;
            newNode.flFadeTime = INFERNO_NODE_FADE_TIME;
            newNode.nParentIndex = pIdx;
            newNode.nClusterGeneration = m_nodes[pIdx].nClusterGeneration + 1;
            newNode.bIsBurning = true;
            newNode.bExtinguished = false;

            ++m_fireCount;
            ++nodesAddedThisStep;
        }
    }

    ++m_nSpreadCount;
    return nodesAddedThisStep;
}

// ----------------------------------------------------------------------------
// Mutual Smoke Voxel Interaction & Extinguishment (server.dll RVA 0x00346330)
// ----------------------------------------------------------------------------
int32_t CInfernoFireGrid::ExtinguishOverlappingSmoke(const CSmokeVoxelGrid& smokeGrid, float flCurrentTime) {
    (void)flCurrentTime;

    if (!m_bActive || m_bInPostEffectTime || m_fireCount == 0) {
        return 0;
    }

    int32_t quenchedThisPass = 0;
    const Vector3 smokeCenter = smokeGrid.GetOrigin();

    for (int32_t i = 0; i < m_fireCount; ++i) {
        if (!m_nodes[i].bIsBurning || m_nodes[i].bExtinguished) {
            continue;
        }

        const Vector3& nodePos = m_nodes[i].vecPosition;

        // Query 1: Detonation Core Sphere Check (144.0 unit radius)
        const float distToSmokeCenter = (nodePos - smokeCenter).Length();
        bool bQuench = (distToSmokeCenter <= SMOKE_EXTINGUISH_SPHERE_RADIUS);

        // Query 2: Active 3D Morton Voxel Grid Traversal Check
        if (!bQuench) {
            int vx = 0, vy = 0, vz = 0;
            if (smokeGrid.WorldToVoxel(nodePos, vx, vy, vz)) {
                if (smokeGrid.IsVoxelActive(vx, vy, vz)) {
                    bQuench = true;
                }
            }
        }

        if (bQuench) {
            ExtinguishNode(i);
            ++quenchedThisPass;
        }
    }

    // Authoritative 1/3rd Rule Check:
    // server.dll: "Molotov extinguished: when %d/%d fire areas were covered by smoke (exceeded 1/3rd)."
    // If active covered fire areas >= 1/3rd of total nodes (and total >= 3), extinguish entire inferno.
    if (m_fireCount >= 3) {
        const float coveredFraction = static_cast<float>(m_nExtinguishedFlames) / static_cast<float>(m_fireCount);
        if (coveredFraction >= SMOKE_EXTINGUISH_FRACTION) {
            ExtinguishAll();
        }
    } else if (GetActiveBurningCount() == 0) {
        ExtinguishAll();
    }

    return quenchedThisPass;
}

// ----------------------------------------------------------------------------
// Extinguish Individual Node (VA 0x180344790 / RVA 0x00344790)
// ----------------------------------------------------------------------------
void CInfernoFireGrid::ExtinguishNode(int32_t nIndex) {
    if (nIndex < 0 || nIndex >= m_fireCount) {
        return;
    }

    if (m_nodes[nIndex].bIsBurning && !m_nodes[nIndex].bExtinguished) {
        m_nodes[nIndex].bIsBurning = false;
        m_nodes[nIndex].bExtinguished = true;
        m_bFireIsBurning[nIndex] = false;
        ++m_nExtinguishedFlames;
    }
}

// ----------------------------------------------------------------------------
// Extinguish Entire Inferno
// ----------------------------------------------------------------------------
void CInfernoFireGrid::ExtinguishAll() {
    for (int32_t i = 0; i < m_fireCount; ++i) {
        if (m_nodes[i].bIsBurning) {
            m_nodes[i].bIsBurning = false;
            m_nodes[i].bExtinguished = true;
            m_bFireIsBurning[i] = false;
        }
    }
    m_bInPostEffectTime = true;
    m_bActive = false;
}

// ----------------------------------------------------------------------------
// Area Damage Evaluation (VA 0x1803468c4 / RVA 0x003468c4)
// ----------------------------------------------------------------------------
InfernoDamageRecord_t CInfernoFireGrid::EvaluateDamageAtPosition(
    const Vector3& vecTargetPos,
    float flTargetExposureTime,
    float flCurrentTime) const
{
    InfernoDamageRecord_t record{};
    if (!m_bActive || m_bInPostEffectTime || m_fireCount == 0) {
        return record;
    }

    float minDistance = std::numeric_limits<float>::max();
    int32_t closestNodeIdx = -1;
    float peakIntensityAtClosest = 0.0f;

    // Find closest active burning flame node
    for (int32_t i = 0; i < m_fireCount; ++i) {
        if (!m_nodes[i].bIsBurning || m_nodes[i].bExtinguished) {
            continue;
        }

        const Vector3& nodePos = m_nodes[i].vecPosition;
        // Check vertical height clearance (within 64 units)
        const float vertDelta = std::abs(vecTargetPos.z - nodePos.z);
        if (vertDelta > 64.0f) {
            continue;
        }

        const float dist2D = std::hypot(vecTargetPos.x - nodePos.x, vecTargetPos.y - nodePos.y);
        if (dist2D < minDistance) {
            minDistance = dist2D;
            closestNodeIdx = i;
            peakIntensityAtClosest = m_nodes[i].GetThermalIntensity(flCurrentTime);
        }
    }

    record.flDistanceToClosestNode = minDistance;
    record.nClosestNodeIndex = closestNodeIdx;

    // If within effective damage radius (48 units)
    if (closestNodeIdx >= 0 && minDistance <= INFERNO_DAMAGE_RADIUS) {
        record.bInFire = true;
        record.flThermalIntensity = peakIntensityAtClosest;

        // Radial Distance Falloff:
        // [0..24 units]: Full damage (1.0)
        // [24..48 units]: Linear falloff from 1.0 down to 0.5
        float distFactor = 1.0f;
        if (minDistance > INFERNO_DAMAGE_CORE_RADIUS) {
            const float t = (minDistance - INFERNO_DAMAGE_CORE_RADIUS) /
                            (INFERNO_DAMAGE_RADIUS - INFERNO_DAMAGE_CORE_RADIUS);
            distFactor = 1.0f - (t * 0.5f);
        }

        // Damage Ramp Timer Multiplier:
        // Escalates linearly over 2.0s from 1.0x to 2.5x
        const float rampFraction = std::clamp(flTargetExposureTime / INFERNO_DAMAGE_RAMP_DURATION, 0.0f, 1.0f);
        const float rampMult = 1.0f + (INFERNO_DAMAGE_RAMP_MAX_MULT - 1.0f) * rampFraction;

        // Base tick damage scaled by ramp, distance falloff, and node combustion intensity
        const float rawTickDamage = INFERNO_DAMAGE_BASE_TICK * rampMult * distFactor * peakIntensityAtClosest;

        // Armor Absorption Model (85% to health, 15% to armor)
        record.flHealthDamage = rawTickDamage * INFERNO_ARMOR_PENETRATION;
        record.flArmorDamage  = rawTickDamage * (1.0f - INFERNO_ARMOR_PENETRATION);
    }

    return record;
}

// ----------------------------------------------------------------------------
// Active Node Count
// ----------------------------------------------------------------------------
int32_t CInfernoFireGrid::GetActiveBurningCount() const noexcept {
    int32_t count = 0;
    for (int32_t i = 0; i < m_fireCount; ++i) {
        if (m_nodes[i].bIsBurning && !m_nodes[i].bExtinguished) {
            ++count;
        }
    }
    return count;
}

// ----------------------------------------------------------------------------
// Bounding Box Calculation
// ----------------------------------------------------------------------------
void CInfernoFireGrid::GetBoundingBox(Vector3& outMin, Vector3& outMax) const noexcept {
    if (m_fireCount == 0) {
        outMin = m_vecOriginalSpawnLocation;
        outMax = m_vecOriginalSpawnLocation;
        return;
    }

    outMin = Vector3{1e9f, 1e9f, 1e9f};
    outMax = Vector3{-1e9f, -1e9f, -1e9f};

    for (int32_t i = 0; i < m_fireCount; ++i) {
        const Vector3& p = m_firePositions[i];
        outMin.x = std::min(outMin.x, p.x - INFERNO_DAMAGE_RADIUS);
        outMin.y = std::min(outMin.y, p.y - INFERNO_DAMAGE_RADIUS);
        outMin.z = std::min(outMin.z, p.z - 16.0f);

        outMax.x = std::max(outMax.x, p.x + INFERNO_DAMAGE_RADIUS);
        outMax.y = std::max(outMax.y, p.y + INFERNO_DAMAGE_RADIUS);
        outMax.z = std::max(outMax.z, p.z + 64.0f);
    }
}

} // namespace cs2::particles
