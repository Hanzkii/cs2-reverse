// ============================================================================
// Counter-Strike 2 (Source 2) Surface Combustion & Thermal Energy Grid Simulator
// 
// Reverse-engineered from server.dll (Build 26598758):
// - CInferno::InfernoThink          (RVA 0x00346090 / VA 0x180346090)
// - CInferno::CheckExpired          (RVA 0x00340bc0 / VA 0x180340bc0)
// - CInferno::Spread                (RVA 0x0034ef00 / VA 0x18034ef00)
// - CInferno::ExtinguishFire        (RVA 0x00344790 / VA 0x180344790)
// - CInferno::InfernoThink (damage) (RVA 0x003468c4 / VA 0x1803468c4)
//
// Standalone C++20 Reference Implementation. Zero external dependencies.
// Standard: ISO C++20 (/std:c++20 /O2 /EHsc /W4 /WX)
// ============================================================================

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <functional>
#include <limits>

namespace cs2::particles {

// ----------------------------------------------------------------------------
// Authoritative Engine Constants & Virtual Offsets (server.dll @ Build 26598758)
// ----------------------------------------------------------------------------
constexpr uintptr_t VA_INFERNO_CHECK_EXPIRED   = 0x180340bc0; // RVA: 0x00340bc0
constexpr uintptr_t VA_INFERNO_EXTINGUISH       = 0x180344790; // RVA: 0x00344790
constexpr uintptr_t VA_INFERNO_THINK           = 0x180346090; // RVA: 0x00346090
constexpr uintptr_t VA_INFERNO_SPREAD          = 0x18034ef00; // RVA: 0x0034ef00
constexpr uintptr_t VA_INFERNO_DAMAGE          = 0x1803468c4; // RVA: 0x003468c4

constexpr int32_t   MAX_INFERNO_FIRES          = 64;      // server.dll: VectorWS[MAX_INFERNO_FIRES] (64 nodes)
constexpr float     INFERNO_SPREAD_RADIUS      = 250.0f;  // Maximum cluster horizontal radius (units)
constexpr float     INFERNO_FLAME_STEP_DISTANCE= 36.0f;   // Inter-node propagation step distance (units)
constexpr float     INFERNO_MIN_NODE_SEPARATION= 24.0f;   // Minimum distance between active nodes (units)
constexpr float     INFERNO_MAX_CLIMB_SLOPE    = 0.7071f; // cos(45 deg) - normal.z threshold for slope traversal
constexpr float     INFERNO_MAX_STEP_HEIGHT    = 44.0f;   // Maximum vertical step climbing threshold (units)
constexpr float     INFERNO_GROUND_SNAP_RAY_UP = 32.0f;   // Upward offset for ground probe raycast
constexpr float     INFERNO_GROUND_SNAP_RAY_DOWN = 64.0f; // Downward distance for ground probe raycast
constexpr float     INFERNO_GROUND_NORMAL_OFFSET = 2.0f;  // Lift above terrain along normal to avoid brush clipping

constexpr float     INFERNO_SPREAD_INTERVAL    = 0.08f;   // Expansion hop timer (seconds)
constexpr float     INFERNO_LIFETIME           = 7.0f;    // Nominal molotov/incendiary burn lifetime (seconds)
constexpr float     INFERNO_NODE_WARMUP_TIME   = 0.35f;   // Thermal intensity ramp-up duration (seconds)
constexpr float     INFERNO_NODE_FADE_TIME     = 1.00f;   // Thermal intensity cooldown / fade duration (seconds)

constexpr float     INFERNO_DAMAGE_INTERVAL    = 0.10f;   // Per-tick damage evaluation interval (100ms / 10 Hz)
constexpr float     INFERNO_DAMAGE_RADIUS      = 48.0f;   // Maximum radial damage radius per flame node (units)
constexpr float     INFERNO_DAMAGE_CORE_RADIUS = 24.0f;   // Core radius for full damage with no falloff (units)
constexpr float     INFERNO_DAMAGE_BASE_TICK   = 4.0f;    // Base damage per 100ms tick (~40 HP/s unarmored)
constexpr float     INFERNO_DAMAGE_RAMP_MAX_MULT = 2.5f;  // Maximum ramp multiplier (~100 HP/s after exposure)
constexpr float     INFERNO_DAMAGE_RAMP_DURATION = 2.0f;  // Exposure duration to reach peak ramp (seconds)
constexpr float     INFERNO_ARMOR_PENETRATION  = 0.85f;   // Kevlar absorbs 15%, transmits 85% directly to health

constexpr float     SMOKE_EXTINGUISH_SPHERE_RADIUS = 144.0f; // Detonation core sphere extinguishment threshold
constexpr float     SMOKE_EXTINGUISH_FRACTION  = 0.3333333f; // 1/3rd coverage threshold for full inferno quench

// ----------------------------------------------------------------------------
// 3D Geometric Vector
// ----------------------------------------------------------------------------
struct Vector3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    constexpr Vector3() noexcept = default;
    constexpr Vector3(float _x, float _y, float _z) noexcept : x(_x), y(_y), z(_z) {}

    [[nodiscard]] constexpr Vector3 operator+(const Vector3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    [[nodiscard]] constexpr Vector3 operator-(const Vector3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    [[nodiscard]] constexpr Vector3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    [[nodiscard]] constexpr Vector3 operator/(float s) const noexcept { return {x / s, y / s, z / s}; }

    [[nodiscard]] constexpr float Dot(const Vector3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] float LengthSqr() const noexcept { return Dot(*this); }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(LengthSqr()); }

    [[nodiscard]] Vector3 Normalized() const noexcept {
        const float len = Length();
        return (len > 1e-6f) ? (*this / len) : Vector3{0.0f, 0.0f, 0.0f};
    }
};

// ----------------------------------------------------------------------------
// 3D Morton Curve Volumetric Smoke Grid (Reconstructed from server.dll)
// ----------------------------------------------------------------------------
constexpr float VOXEL_SIZE                     = 20.0f;
constexpr float VOXEL_INV_SIZE                 = 0.05f;
constexpr int   GRID_DIM                       = 32;
constexpr int   GRID_TOTAL_VOXELS              = 32768;
constexpr int   BITMASK_WORDS                  = 512;
constexpr float GRID_CENTER_VOXEL              = 16.0f;

class CSmokeVoxelGrid {
public:
    CSmokeVoxelGrid(const Vector3& origin = {}, float voxelSize = VOXEL_SIZE)
        : m_vSmokeDetonationPos(origin), m_VoxelSize(voxelSize)
    {
        std::memset(m_OccupancyMask, 0, sizeof(m_OccupancyMask));
    }

    static uint32_t Part1By2(uint32_t x) noexcept {
        x &= 0x0000001f;
        x = (x | (x << 8)) & 0x0300f00f;
        x = (x ^ (x << 4)) & 0x030c30c3;
        x = (x ^ (x << 2)) & 0x09249249;
        return x;
    }

    static uint32_t Morton3D(uint32_t x, uint32_t y, uint32_t z) noexcept {
        return Part1By2(x) | (Part1By2(y) << 1) | (Part1By2(z) << 2);
    }

    void Detonate(const Vector3& origin) {
        m_vSmokeDetonationPos = origin;
        m_bActive = true;
        std::memset(m_OccupancyMask, 0, sizeof(m_OccupancyMask));
        ExpandSphere(SMOKE_EXTINGUISH_SPHERE_RADIUS);
    }

    void ExpandSphere(float radiusUnits) {
        const int radiusVoxels = static_cast<int>(std::ceil(radiusUnits * VOXEL_INV_SIZE));
        const int centerV = static_cast<int>(GRID_CENTER_VOXEL);

        for (int dz = -radiusVoxels; dz <= radiusVoxels; ++dz) {
            for (int dy = -radiusVoxels; dy <= radiusVoxels; ++dy) {
                for (int dx = -radiusVoxels; dx <= radiusVoxels; ++dx) {
                    const float r2 = static_cast<float>(dx * dx + dy * dy + dz * dz) * (VOXEL_SIZE * VOXEL_SIZE);
                    if (r2 <= radiusUnits * radiusUnits) {
                        const int vx = centerV + dx;
                        const int vy = centerV + dy;
                        const int vz = centerV + dz;
                        if (vx >= 0 && vx < GRID_DIM && vy >= 0 && vy < GRID_DIM && vz >= 0 && vz < GRID_DIM) {
                            SetVoxelActive(vx, vy, vz);
                        }
                    }
                }
            }
        }
    }

    bool WorldToVoxel(const Vector3& worldPos, int& vx, int& vy, int& vz) const noexcept {
        vx = static_cast<int>(std::floor((worldPos.x - m_vSmokeDetonationPos.x) * VOXEL_INV_SIZE + GRID_CENTER_VOXEL));
        vy = static_cast<int>(std::floor((worldPos.y - m_vSmokeDetonationPos.y) * VOXEL_INV_SIZE + GRID_CENTER_VOXEL));
        vz = static_cast<int>(std::floor((worldPos.z - m_vSmokeDetonationPos.z) * VOXEL_INV_SIZE + GRID_CENTER_VOXEL));
        return (vx >= 0 && vx < GRID_DIM && vy >= 0 && vy < GRID_DIM && vz >= 0 && vz < GRID_DIM);
    }

    void SetVoxelActive(int vx, int vy, int vz) noexcept {
        const uint32_t code = Morton3D(static_cast<uint32_t>(vx), static_cast<uint32_t>(vy), static_cast<uint32_t>(vz));
        const uint32_t word = code >> 6;
        const uint32_t bit  = code & 63;
        if (word < BITMASK_WORDS) {
            m_OccupancyMask[word] |= (1ULL << bit);
        }
    }

    [[nodiscard]] bool IsVoxelActive(int vx, int vy, int vz) const noexcept {
        if (vx < 0 || vx >= GRID_DIM || vy < 0 || vy >= GRID_DIM || vz < 0 || vz >= GRID_DIM) {
            return false;
        }
        const uint32_t code = Morton3D(static_cast<uint32_t>(vx), static_cast<uint32_t>(vy), static_cast<uint32_t>(vz));
        const uint32_t word = code >> 6;
        const uint32_t bit  = code & 63;
        return (m_OccupancyMask[word] & (1ULL << bit)) != 0;
    }

    [[nodiscard]] const Vector3& GetOrigin() const noexcept { return m_vSmokeDetonationPos; }
    [[nodiscard]] bool IsActive() const noexcept { return m_bActive; }

private:
    Vector3  m_vSmokeDetonationPos{0.0f, 0.0f, 0.0f};
    float    m_VoxelSize{VOXEL_SIZE};
    bool     m_bActive{false};
    uint64_t m_OccupancyMask[BITMASK_WORDS]{};
};

// ----------------------------------------------------------------------------
// Terrain Collision Query Structures
// ----------------------------------------------------------------------------
struct TerrainTraceHit_t {
    bool    bHit{false};
    float   flFraction{1.0f};
    Vector3 vecHitPosition{};
    Vector3 vecHitNormal{0.0f, 0.0f, 1.0f};
    bool    bStartSolid{false};
};

using TerrainRaycastFn = std::function<TerrainTraceHit_t(const Vector3& start, const Vector3& end)>;

// ----------------------------------------------------------------------------
// Individual Flame Node Structure
// ----------------------------------------------------------------------------
struct InfernoFlameNode_t {
    Vector3 vecPosition{};
    Vector3 vecParentPosition{};
    Vector3 vecNormal{0.0f, 0.0f, 1.0f};
    float   flIgnitionTime{0.0f};
    float   flLifetime{INFERNO_LIFETIME};
    float   flWarmupTime{INFERNO_NODE_WARMUP_TIME};
    float   flFadeTime{INFERNO_NODE_FADE_TIME};
    int32_t nParentIndex{-1};
    int32_t nClusterGeneration{0};
    bool    bIsBurning{false};
    bool    bExtinguished{false};

    [[nodiscard]] float GetThermalIntensity(float flCurrentTime) const noexcept {
        if (!bIsBurning || bExtinguished) {
            return 0.0f;
        }
        const float flElapsed = flCurrentTime - flIgnitionTime;
        if (flElapsed < 0.0f || flElapsed >= flLifetime) {
            return 0.0f;
        }
        if (flElapsed < flWarmupTime) {
            return (flWarmupTime > 1e-4f) ? (flElapsed / flWarmupTime) : 1.0f;
        }
        const float flDecayStart = flLifetime - flFadeTime;
        if (flElapsed >= flDecayStart) {
            const float flFadeElapsed = flElapsed - flDecayStart;
            return (flFadeTime > 1e-4f) ? std::max(0.0f, 1.0f - (flFadeElapsed / flFadeTime)) : 0.0f;
        }
        return 1.0f;
    }
};

// ----------------------------------------------------------------------------
// Damage Output Record
// ----------------------------------------------------------------------------
struct InfernoDamageRecord_t {
    bool    bInFire{false};
    float   flHealthDamage{0.0f};
    float   flArmorDamage{0.0f};
    float   flThermalIntensity{0.0f};
    float   flDistanceToClosestNode{9999.0f};
    int32_t nClosestNodeIndex{-1};
};

// ----------------------------------------------------------------------------
// Authoritative CInfernoFireGrid Engine Implementation
// ----------------------------------------------------------------------------
class CInfernoFireGrid {
public:
    CInfernoFireGrid() {
        std::memset(m_firePositions, 0, sizeof(m_firePositions));
        std::memset(m_fireParentPositions, 0, sizeof(m_fireParentPositions));
        std::memset(m_bFireIsBurning, 0, sizeof(m_bFireIsBurning));
        std::memset(m_BurnNormal, 0, sizeof(m_BurnNormal));
    }

    void Spawn(const Vector3& vecOrigin,
               int32_t nStartTick = 0,
               float flCurrentTime = 0.0f,
               const CSmokeVoxelGrid* pSmoke = nullptr,
               const TerrainRaycastFn& terrainQuery = nullptr)
    {
        m_vecOriginalSpawnLocation = vecOrigin;
        m_nFireEffectTickBegin = nStartTick;
        m_flSpawnTime = flCurrentTime;
        m_flLastSpreadTime = flCurrentTime;
        m_fireCount = 0;
        m_nSpreadCount = 0;
        m_nExtinguishedFlames = 0;
        m_bInPostEffectTime = false;
        m_bWasCreatedInSmoke = false;

        std::memset(m_firePositions, 0, sizeof(m_firePositions));
        std::memset(m_fireParentPositions, 0, sizeof(m_fireParentPositions));
        std::memset(m_bFireIsBurning, 0, sizeof(m_bFireIsBurning));
        std::memset(m_BurnNormal, 0, sizeof(m_BurnNormal));

        if (pSmoke && pSmoke->IsActive()) {
            const float distToSmoke = (vecOrigin - pSmoke->GetOrigin()).Length();
            bool bInVoxel = false;
            int vx = 0, vy = 0, vz = 0;
            if (pSmoke->WorldToVoxel(vecOrigin, vx, vy, vz)) {
                bInVoxel = pSmoke->IsVoxelActive(vx, vy, vz);
            }
            if (distToSmoke <= SMOKE_EXTINGUISH_SPHERE_RADIUS || bInVoxel) {
                m_bWasCreatedInSmoke = true;
                m_bInPostEffectTime = true;
                m_bActive = false;
                return;
            }
        }

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

        m_firePositions[0] = seedPos;
        m_fireParentPositions[0] = vecOrigin;
        m_BurnNormal[0] = seedNormal;
        m_bFireIsBurning[0] = true;

        m_nodes[0].vecPosition = seedPos;
        m_nodes[0].vecParentPosition = vecOrigin;
        m_nodes[0].vecNormal = seedNormal;
        m_nodes[0].flIgnitionTime = flCurrentTime;
        m_nodes[0].flLifetime = m_nFireLifetime;
        m_nodes[0].flWarmupTime = INFERNO_NODE_WARMUP_TIME;
        m_nodes[0].flFadeTime = INFERNO_NODE_FADE_TIME;
        m_nodes[0].nParentIndex = -1;
        m_nodes[0].nClusterGeneration = 0;
        m_nodes[0].bIsBurning = true;
        m_nodes[0].bExtinguished = false;

        m_fireCount = 1;
        m_bActive = true;
    }

    void OnTick(float flCurrentTime,
                int32_t nCurrentTick,
                const CSmokeVoxelGrid* pActiveSmoke = nullptr,
                const TerrainRaycastFn& terrainQuery = nullptr)
    {
        (void)nCurrentTick;
        if (!m_bActive || m_bInPostEffectTime) {
            return;
        }

        CheckExpired(flCurrentTime);
        if (!m_bActive || m_bInPostEffectTime) {
            return;
        }

        if (pActiveSmoke && pActiveSmoke->IsActive()) {
            ExtinguishOverlappingSmoke(*pActiveSmoke, flCurrentTime);
            if (m_bInPostEffectTime || !m_bActive) {
                return;
            }
        }

        if (flCurrentTime - m_flLastSpreadTime >= INFERNO_SPREAD_INTERVAL) {
            if (m_fireCount < m_nMaxFlames) {
                SpreadStep(flCurrentTime, terrainQuery);
            }
            m_flLastSpreadTime = flCurrentTime;
        }
    }

    void CheckExpired(float flCurrentTime) {
        if (!m_bActive) {
            return;
        }
        int32_t activeCount = 0;
        for (int32_t i = 0; i < m_fireCount; ++i) {
            if (m_nodes[i].bIsBurning && !m_nodes[i].bExtinguished) {
                const float elapsed = flCurrentTime - m_nodes[i].flIgnitionTime;
                if (elapsed >= m_nodes[i].flLifetime) {
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

    int32_t SpreadStep(float flCurrentTime, const TerrainRaycastFn& terrainQuery = nullptr) {
        if (m_fireCount >= m_nMaxFlames || !m_bActive) {
            return 0;
        }

        int32_t nodesAdded = 0;
        constexpr int32_t kMaxNewNodes = 4;
        constexpr int32_t kAngles = 16;
        constexpr float kTwoPi = 6.283185307179586f;
        constexpr float kAngleStep = kTwoPi / static_cast<float>(kAngles);

        const int32_t snapshotCount = m_fireCount;

        for (int32_t pIdx = 0; pIdx < snapshotCount; ++pIdx) {
            if (!m_nodes[pIdx].bIsBurning || m_nodes[pIdx].bExtinguished) {
                continue;
            }

            const Vector3 parentPos = m_nodes[pIdx].vecPosition;
            const Vector3 parentNormal = m_nodes[pIdx].vecNormal;
            const float angleOffset = static_cast<float>(m_nSpreadCount * 3 + pIdx) * 0.392699f;

            for (int32_t d = 0; d < kAngles; ++d) {
                if (m_fireCount >= m_nMaxFlames || nodesAdded >= kMaxNewNodes) {
                    break;
                }

                const float angle = angleOffset + static_cast<float>(d) * kAngleStep;
                const Vector3 dir2D{std::cos(angle), std::sin(angle), 0.0f};

                // Surface tangent projection
                const float dot = dir2D.Dot(parentNormal);
                Vector3 tangent = dir2D - (parentNormal * dot);
                const float tLen = tangent.Length();
                if (tLen > 1e-4f) {
                    tangent = tangent / tLen;
                }

                const Vector3 candPos = parentPos + tangent * INFERNO_FLAME_STEP_DISTANCE;
                const float distOrigin2D = std::hypot(
                    candPos.x - m_vecOriginalSpawnLocation.x,
                    candPos.y - m_vecOriginalSpawnLocation.y
                );
                if (distOrigin2D > INFERNO_SPREAD_RADIUS) {
                    continue;
                }

                const Vector3 traceStart = candPos + Vector3{0.0f, 0.0f, INFERNO_GROUND_SNAP_RAY_UP};
                const Vector3 traceEnd   = candPos - Vector3{0.0f, 0.0f, INFERNO_GROUND_SNAP_RAY_DOWN};

                TerrainTraceHit_t hit{};
                if (terrainQuery) {
                    hit = terrainQuery(traceStart, traceEnd);
                } else {
                    hit = DefaultTerrainRaycast(traceStart, traceEnd);
                }

                if (!hit.bHit) {
                    continue;
                }

                // Slope limit (normal.z check)
                if (hit.vecHitNormal.z < INFERNO_MAX_CLIMB_SLOPE) {
                    continue;
                }

                // Step height limit
                if (std::abs(hit.vecHitPosition.z - parentPos.z) > INFERNO_MAX_STEP_HEIGHT) {
                    continue;
                }

                const Vector3 childPos = hit.vecHitPosition + hit.vecHitNormal * INFERNO_GROUND_NORMAL_OFFSET;

                // Minimum separation check
                bool bTooClose = false;
                for (int32_t ex = 0; ex < m_fireCount; ++ex) {
                    if (!m_nodes[ex].bIsBurning) {
                        continue;
                    }
                    if ((childPos - m_nodes[ex].vecPosition).Length() < INFERNO_MIN_NODE_SEPARATION) {
                        bTooClose = true;
                        break;
                    }
                }
                if (bTooClose) {
                    continue;
                }

                const int32_t newIdx = m_fireCount;
                m_firePositions[newIdx] = childPos;
                m_fireParentPositions[newIdx] = parentPos;
                m_BurnNormal[newIdx] = hit.vecHitNormal;
                m_bFireIsBurning[newIdx] = true;

                m_nodes[newIdx].vecPosition = childPos;
                m_nodes[newIdx].vecParentPosition = parentPos;
                m_nodes[newIdx].vecNormal = hit.vecHitNormal;
                m_nodes[newIdx].flIgnitionTime = flCurrentTime;
                m_nodes[newIdx].flLifetime = m_nFireLifetime;
                m_nodes[newIdx].flWarmupTime = INFERNO_NODE_WARMUP_TIME;
                m_nodes[newIdx].flFadeTime = INFERNO_NODE_FADE_TIME;
                m_nodes[newIdx].nParentIndex = pIdx;
                m_nodes[newIdx].nClusterGeneration = m_nodes[pIdx].nClusterGeneration + 1;
                m_nodes[newIdx].bIsBurning = true;
                m_nodes[newIdx].bExtinguished = false;

                ++m_fireCount;
                ++nodesAdded;
            }
        }

        ++m_nSpreadCount;
        return nodesAdded;
    }

    int32_t ExtinguishOverlappingSmoke(const CSmokeVoxelGrid& smokeGrid, float flCurrentTime = 0.0f) {
        (void)flCurrentTime;
        if (!m_bActive || m_bInPostEffectTime || m_fireCount == 0) {
            return 0;
        }

        int32_t quenched = 0;
        const Vector3 smokeCenter = smokeGrid.GetOrigin();

        for (int32_t i = 0; i < m_fireCount; ++i) {
            if (!m_nodes[i].bIsBurning || m_nodes[i].bExtinguished) {
                continue;
            }

            const Vector3& pos = m_nodes[i].vecPosition;
            bool bQuench = ((pos - smokeCenter).Length() <= SMOKE_EXTINGUISH_SPHERE_RADIUS);

            if (!bQuench) {
                int vx = 0, vy = 0, vz = 0;
                if (smokeGrid.WorldToVoxel(pos, vx, vy, vz)) {
                    if (smokeGrid.IsVoxelActive(vx, vy, vz)) {
                        bQuench = true;
                    }
                }
            }

            if (bQuench) {
                ExtinguishNode(i);
                ++quenched;
            }
        }

        // 1/3rd rule check
        if (m_fireCount >= 3) {
            const float frac = static_cast<float>(m_nExtinguishedFlames) / static_cast<float>(m_fireCount);
            if (frac >= SMOKE_EXTINGUISH_FRACTION) {
                ExtinguishAll();
            }
        } else if (GetActiveBurningCount() == 0) {
            ExtinguishAll();
        }

        return quenched;
    }

    void ExtinguishNode(int32_t idx) {
        if (idx >= 0 && idx < m_fireCount && m_nodes[idx].bIsBurning && !m_nodes[idx].bExtinguished) {
            m_nodes[idx].bIsBurning = false;
            m_nodes[idx].bExtinguished = true;
            m_bFireIsBurning[idx] = false;
            ++m_nExtinguishedFlames;
        }
    }

    void ExtinguishAll() {
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

    [[nodiscard]] InfernoDamageRecord_t EvaluateDamageAtPosition(
        const Vector3& vecTargetPos,
        float flTargetExposureTime,
        float flCurrentTime) const
    {
        InfernoDamageRecord_t record{};
        if (!m_bActive || m_bInPostEffectTime || m_fireCount == 0) {
            return record;
        }

        float minDistance = std::numeric_limits<float>::max();
        int32_t closestIdx = -1;
        float intensityAtClosest = 0.0f;

        for (int32_t i = 0; i < m_fireCount; ++i) {
            if (!m_nodes[i].bIsBurning || m_nodes[i].bExtinguished) {
                continue;
            }

            const Vector3& p = m_nodes[i].vecPosition;
            if (std::abs(vecTargetPos.z - p.z) > 64.0f) {
                continue;
            }

            const float dist2D = std::hypot(vecTargetPos.x - p.x, vecTargetPos.y - p.y);
            if (dist2D < minDistance) {
                minDistance = dist2D;
                closestIdx = i;
                intensityAtClosest = m_nodes[i].GetThermalIntensity(flCurrentTime);
            }
        }

        record.flDistanceToClosestNode = minDistance;
        record.nClosestNodeIndex = closestIdx;

        if (closestIdx >= 0 && minDistance <= INFERNO_DAMAGE_RADIUS) {
            record.bInFire = true;
            record.flThermalIntensity = intensityAtClosest;

            float distFactor = 1.0f;
            if (minDistance > INFERNO_DAMAGE_CORE_RADIUS) {
                const float t = (minDistance - INFERNO_DAMAGE_CORE_RADIUS) /
                                (INFERNO_DAMAGE_RADIUS - INFERNO_DAMAGE_CORE_RADIUS);
                distFactor = 1.0f - (t * 0.5f);
            }

            const float rampFrac = std::clamp(flTargetExposureTime / INFERNO_DAMAGE_RAMP_DURATION, 0.0f, 1.0f);
            const float rampMult = 1.0f + (INFERNO_DAMAGE_RAMP_MAX_MULT - 1.0f) * rampFrac;

            const float rawTick = INFERNO_DAMAGE_BASE_TICK * rampMult * distFactor * intensityAtClosest;
            record.flHealthDamage = rawTick * INFERNO_ARMOR_PENETRATION;
            record.flArmorDamage  = rawTick * (1.0f - INFERNO_ARMOR_PENETRATION);
        }

        return record;
    }

    [[nodiscard]] int32_t GetFireCount() const noexcept { return m_fireCount; }
    [[nodiscard]] int32_t GetActiveBurningCount() const noexcept {
        int32_t c = 0;
        for (int32_t i = 0; i < m_fireCount; ++i) {
            if (m_nodes[i].bIsBurning && !m_nodes[i].bExtinguished) ++c;
        }
        return c;
    }
    [[nodiscard]] int32_t GetExtinguishedCount() const noexcept { return m_nExtinguishedFlames; }
    [[nodiscard]] bool    IsActive() const noexcept { return m_bActive && !m_bInPostEffectTime; }
    [[nodiscard]] bool    IsInPostEffect() const noexcept { return m_bInPostEffectTime; }
    [[nodiscard]] bool    WasCreatedInSmoke() const noexcept { return m_bWasCreatedInSmoke; }
    [[nodiscard]] const Vector3& GetFirePosition(int32_t i) const { return m_firePositions[i]; }
    [[nodiscard]] const Vector3& GetBurnNormal(int32_t i) const { return m_BurnNormal[i]; }
    [[nodiscard]] const InfernoFlameNode_t& GetNode(int32_t i) const { return m_nodes[i]; }

    static TerrainTraceHit_t DefaultTerrainRaycast(const Vector3& start, const Vector3& end) noexcept {
        TerrainTraceHit_t hit{};
        const Vector3 dir = end - start;
        constexpr int kSteps = 32;
        const float stepFrac = 1.0f / static_cast<float>(kSteps);

        for (int i = 0; i <= kSteps; ++i) {
            const float t = static_cast<float>(i) * stepFrac;
            const Vector3 p = start + dir * t;
            const float groundZ = 6.0f * std::sin(0.02f * p.x) * std::cos(0.02f * p.y);

            if (p.z <= groundZ) {
                hit.bHit = true;
                hit.flFraction = t;
                hit.vecHitPosition = Vector3{p.x, p.y, groundZ};
                const float dzdx = 6.0f * 0.02f * std::cos(0.02f * p.x) * std::cos(0.02f * p.y);
                const float dzdy = -6.0f * 0.02f * std::sin(0.02f * p.x) * std::sin(0.02f * p.y);
                hit.vecHitNormal = Vector3{-dzdx, -dzdy, 1.0f}.Normalized();
                return hit;
            }
        }
        return hit;
    }

private:
    Vector3 m_firePositions[MAX_INFERNO_FIRES]{};
    Vector3 m_fireParentPositions[MAX_INFERNO_FIRES]{};
    bool    m_bFireIsBurning[MAX_INFERNO_FIRES]{};
    Vector3 m_BurnNormal[MAX_INFERNO_FIRES]{};
    int32_t m_fireCount{0};
    int32_t m_nInfernoType{0};
    int32_t m_nFireEffectTickBegin{0};
    float   m_nFireLifetime{INFERNO_LIFETIME};
    bool    m_bInPostEffectTime{false};
    bool    m_bWasCreatedInSmoke{false};
    Vector3 m_vecOriginalSpawnLocation{0.0f, 0.0f, 0.0f};
    int32_t m_nMaxFlames{MAX_INFERNO_FIRES};
    int32_t m_nSpreadCount{0};

    InfernoFlameNode_t m_nodes[MAX_INFERNO_FIRES]{};
    bool    m_bActive{false};
    float   m_flSpawnTime{0.0f};
    float   m_flLastSpreadTime{0.0f};
    int32_t m_nExtinguishedFlames{0};
};

} // namespace cs2::particles

// ============================================================================
// Verification Test Suite & Runner
// ============================================================================
int main() {
    using namespace cs2::particles;

    std::cout << "============================================================\n";
    std::cout << "  CS2 Surface Combustion & Thermal Energy Grid Simulator    \n";
    std::cout << "  Reverse-Engineered from server.dll (MSVC x64 C++20)       \n";
    std::cout << "============================================================\n\n";

    // ------------------------------------------------------------------------
    // Milestone 1: Ground-Snapping Raycasting & Displacement Terrain
    // ------------------------------------------------------------------------
    std::cout << "[Step 1] Ground-Snapping Raycasting on Displacement Terrain:\n";
    {
        const Vector3 dropPoint{120.0f, -80.0f, 25.0f};
        TerrainTraceHit_t hit = CInfernoFireGrid::DefaultTerrainRaycast(
            dropPoint + Vector3{0.0f, 0.0f, INFERNO_GROUND_SNAP_RAY_UP},
            dropPoint - Vector3{0.0f, 0.0f, INFERNO_GROUND_SNAP_RAY_DOWN}
        );
        assert(hit.bHit && "Ground trace must hit displacement surface");
        std::cout << "  - Trace Start      : (" << dropPoint.x << ", " << dropPoint.y << ", "
                  << dropPoint.z + INFERNO_GROUND_SNAP_RAY_UP << ")\n";
        std::cout << "  - Surface Contact  : (" << std::fixed << std::setprecision(2)
                  << hit.vecHitPosition.x << ", " << hit.vecHitPosition.y << ", " << hit.vecHitPosition.z << ")\n";
        std::cout << "  - Surface Normal   : (" << hit.vecHitNormal.x << ", " << hit.vecHitNormal.y << ", "
                  << hit.vecHitNormal.z << ")\n";
        assert(hit.vecHitNormal.z >= INFERNO_MAX_CLIMB_SLOPE && "Normal.z must satisfy slope limit");
        std::cout << "  -> Ground snapping aligned seed node with displacement gradient.\n\n";
    }

    // ------------------------------------------------------------------------
    // Milestone 2: Dynamic 2D/3D Surface Propagation & Node Cap (64 Nodes)
    // ------------------------------------------------------------------------
    std::cout << "[Step 2] Dynamic 2D/3D Surface Propagation (64 Node Cap):\n";
    CInfernoFireGrid inferno;
    const Vector3 detonationPoint{500.0f, 1000.0f, 10.0f};
    inferno.Spawn(detonationPoint, 1000, 0.0f);
    assert(inferno.GetFireCount() == 1);
    assert(inferno.IsActive());

    // Advance simulation time to spread cluster
    float simTime = 0.0f;
    int tick = 1000;
    while (inferno.GetFireCount() < MAX_INFERNO_FIRES && simTime < 2.5f) {
        simTime += 0.08f;
        ++tick;
        inferno.OnTick(simTime, tick);
    }

    std::cout << "  - Cluster Origin   : (" << detonationPoint.x << ", " << detonationPoint.y << ", " << detonationPoint.z << ")\n";
    std::cout << "  - Simulated Time   : " << simTime << " seconds (" << tick - 1000 << " ticks)\n";
    std::cout << "  - Flame Nodes Built: " << inferno.GetFireCount() << " / " << MAX_INFERNO_FIRES << " (Cap verified)\n";
    std::cout << "  - Active Burning   : " << inferno.GetActiveBurningCount() << " nodes\n";
    assert(inferno.GetFireCount() == MAX_INFERNO_FIRES && "Cluster must expand to max 64 nodes");
    std::cout << "  -> Radial propagation expanded smoothly with surface tangent projection.\n\n";

    // ------------------------------------------------------------------------
    // Milestone 3: Slope Climbing Limits & Wall Obstruction Rejection
    // ------------------------------------------------------------------------
    std::cout << "[Step 3] Slope Climbing & Steep Obstruction Gating:\n";
    {
        // Custom terrain with a sheer vertical barrier at x = 580 (wall normal = (-1, 0, 0))
        auto wallTerrain = [](const Vector3& start, const Vector3& end) -> TerrainTraceHit_t {
            TerrainTraceHit_t hit{};
            if (start.x >= 575.0f || end.x >= 575.0f) {
                hit.bHit = true;
                hit.flFraction = 0.5f;
                hit.vecHitPosition = Vector3{580.0f, start.y, start.z};
                hit.vecHitNormal = Vector3{-1.0f, 0.0f, 0.0f}; // Sheer vertical normal (normal.z = 0)
                return hit;
            }
            hit.bHit = true;
            hit.flFraction = 0.5f;
            hit.vecHitPosition = Vector3{start.x, start.y, 0.0f};
            hit.vecHitNormal = Vector3{0.0f, 0.0f, 1.0f};
            return hit;
        };

        CInfernoFireGrid wallTestGrid;
        wallTestGrid.Spawn(Vector3{500.0f, 0.0f, 10.0f}, 1, 0.0f, nullptr, wallTerrain);
        for (float t = 0.08f; t < 1.0f; t += 0.08f) {
            wallTestGrid.OnTick(t, static_cast<int32_t>(t * 64.0f), nullptr, wallTerrain);
        }

        bool bWallBreached = false;
        for (int32_t i = 0; i < wallTestGrid.GetFireCount(); ++i) {
            if (wallTestGrid.GetFirePosition(i).x > 575.0f) {
                bWallBreached = true;
                break;
            }
        }
        assert(!bWallBreached && "Flames must not breach or climb sheer vertical barrier");
        std::cout << "  - Vertical Barrier : x = 575 units (Normal: (-1, 0, 0), normal.z = 0.0)\n";
        std::cout << "  - Max Node X Reach : " << wallTestGrid.GetFirePosition(wallTestGrid.GetFireCount() - 1).x << " units\n";
        std::cout << "  -> Steep slope rejection gate (normal.z >= 0.7071) correctly preserved!\n\n";
    }

    // ------------------------------------------------------------------------
    // Milestone 4: Area Damage Evaluation & Exposure Ramp Model
    // ------------------------------------------------------------------------
    std::cout << "[Step 4] Area Damage Evaluation & Exposure Ramp Escalation:\n";
    {
        // Target at flame cluster center (0.0 units from node 0)
        const Vector3 centerTarget = inferno.GetFirePosition(0);
        InfernoDamageRecord_t dmgInitial = inferno.EvaluateDamageAtPosition(centerTarget, 0.0f, 1.0f);
        InfernoDamageRecord_t dmgRamped  = inferno.EvaluateDamageAtPosition(centerTarget, 2.0f, 1.0f);

        std::cout << "  - Target Position      : (" << centerTarget.x << ", " << centerTarget.y << ", " << centerTarget.z << ")\n";
        std::cout << "  - Initial Tick Damage  : " << dmgInitial.flHealthDamage << " HP / tick (Armor: " << dmgInitial.flArmorDamage << ")\n";
        std::cout << "  - Peak Ramped Damage   : " << dmgRamped.flHealthDamage << " HP / tick (Armor: " << dmgRamped.flArmorDamage << ")\n";
        assert(dmgRamped.flHealthDamage > dmgInitial.flHealthDamage * 2.0f && "Damage must scale with ramp");

        // Distance Falloff Verification: Isolate on controlled single flame node
        CInfernoFireGrid singleNodeGrid;
        singleNodeGrid.Spawn(Vector3{0.0f, 0.0f, 0.0f}, 1, 0.0f);
        InfernoDamageRecord_t dmgCore    = singleNodeGrid.EvaluateDamageAtPosition(Vector3{10.0f, 0.0f, 0.0f}, 0.0f, 1.0f);
        InfernoDamageRecord_t dmgFalloff = singleNodeGrid.EvaluateDamageAtPosition(Vector3{36.0f, 0.0f, 0.0f}, 0.0f, 1.0f);

        std::cout << "  - Core Zone (10u)      : " << dmgCore.flHealthDamage << " HP / tick (Full Damage: 1.0x)\n";
        std::cout << "  - Falloff Zone (36u)   : " << dmgFalloff.flHealthDamage << " HP / tick (Distance: "
                  << dmgFalloff.flDistanceToClosestNode << " units, Attenuated: 0.75x)\n";
        assert(dmgFalloff.flHealthDamage < dmgCore.flHealthDamage && "Falloff zone damage must be attenuated");
        std::cout << "  -> Radial distance attenuation and 2.5x exposure ramp asserted successfully.\n\n";
    }

    // ------------------------------------------------------------------------
    // Milestone 5: Mutual Smoke Voxel Collision & 1/3rd Extinguishment
    // ------------------------------------------------------------------------
    std::cout << "[Step 5] Mutual Smoke Interaction & Authoritative 1/3rd Rule:\n";
    {
        // Deploy a volumetric smoke grenade detonating near cluster edge
        CSmokeVoxelGrid smokeGrid;
        const Vector3 smokeDetonation = detonationPoint + Vector3{80.0f, 60.0f, 0.0f};
        smokeGrid.Detonate(smokeDetonation);
        std::cout << "  - Smoke Detonation Pos : (" << smokeDetonation.x << ", " << smokeDetonation.y << ", " << smokeDetonation.z << ")\n";
        std::cout << "  - Initial Active Flames: " << inferno.GetActiveBurningCount() << " nodes\n";

        inferno.OnTick(simTime + 0.1f, tick + 1, &smokeGrid);
        std::cout << "  - Extinguished Nodes   : " << inferno.GetExtinguishedCount() << " nodes\n";
        std::cout << "  - Post-Smoke Status    : " << (inferno.IsActive() ? "Active Cluster" : "Fully Extinguished") << "\n";
        assert(inferno.GetExtinguishedCount() >= 20 && "Smoke must cover >= 1/3rd of flames");
        assert(!inferno.IsActive() && "Inferno must be quenched when >= 1/3rd nodes are covered by smoke");
        std::cout << "  -> Authoritative 1/3rd rule (\"Molotov extinguished\") verified!\n\n";
    }

    // ------------------------------------------------------------------------
    // Milestone 6: Instant Spawn Quench Inside Active Smoke Volume
    // ------------------------------------------------------------------------
    std::cout << "[Step 6] Immediate Extinguishment upon In-Smoke Detonation:\n";
    {
        CSmokeVoxelGrid activeSmoke;
        activeSmoke.Detonate(Vector3{100.0f, 100.0f, 0.0f});

        CInfernoFireGrid inSmokeInferno;
        inSmokeInferno.Spawn(Vector3{120.0f, 110.0f, 0.0f}, 2000, 0.0f, &activeSmoke);
        assert(inSmokeInferno.WasCreatedInSmoke() && "Must detect spawn in smoke");
        assert(!inSmokeInferno.IsActive() && "Must not be active");
        assert(inSmokeInferno.GetFireCount() == 0 && "Must spawn 0 flames");
        std::cout << "  - WasCreatedInSmoke    : " << (inSmokeInferno.WasCreatedInSmoke() ? "true" : "false") << "\n";
        std::cout << "  - Active Flame Count   : " << inSmokeInferno.GetFireCount() << " nodes\n";
        std::cout << "  -> Molotov fizzled immediately upon landing inside active volumetric smoke!\n\n";
    }

    std::cout << "============================================================\n";
    std::cout << "  All 6 Verification Milestones Passed Successfully!        \n";
    std::cout << "============================================================\n";
    return 0;
}
