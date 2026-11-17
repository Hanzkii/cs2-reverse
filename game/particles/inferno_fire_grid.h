#ifndef CS2_PARTICLES_INFERNO_FIRE_GRID_H
#define CS2_PARTICLES_INFERNO_FIRE_GRID_H

#include "smoke_voxels.h"
#include <cstdint>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <functional>

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
// Thermal Energy Dissipation: Individual Flame Cluster Node
// Reconstructed from CInferno internal node structures (Build 26598758)
// ----------------------------------------------------------------------------
struct InfernoFlameNode_t {
    Vector3 vecPosition{};              // Snapped 3D world position
    Vector3 vecParentPosition{};        // Propagation ancestor position
    Vector3 vecNormal{0.0f, 0.0f, 1.0f};// Terrain contact surface normal
    float   flIgnitionTime{0.0f};       // Timestamp when node ignited
    float   flLifetime{INFERNO_LIFETIME};// Total allocated burn lifetime
    float   flWarmupTime{INFERNO_NODE_WARMUP_TIME};
    float   flFadeTime{INFERNO_NODE_FADE_TIME};
    int32_t nParentIndex{-1};           // Index of parent node in array (-1 = seed)
    int32_t nClusterGeneration{0};      // Propagation depth generation
    bool    bIsBurning{false};          // Active combustion flag
    bool    bExtinguished{false};       // Quenched by volumetric smoke flag

    /**
     * @brief Evaluates normalized thermal intensity in range [0.0, 1.0].
     * Models combustion phase:
     * - [0, t_warmup): Linear ramp from 0.0 to 1.0.
     * - [t_warmup, t_lifetime - t_fade): Peak burn plateau at 1.0.
     * - [t_lifetime - t_fade, t_lifetime): Linear decay from 1.0 to 0.0.
     */
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

    [[nodiscard]] bool IsActiveAtTime(float flCurrentTime) const noexcept {
        if (!bIsBurning || bExtinguished) {
            return false;
        }
        const float flElapsed = flCurrentTime - flIgnitionTime;
        return (flElapsed >= 0.0f && flElapsed < flLifetime);
    }
};

// ----------------------------------------------------------------------------
// Damage Evaluation Output Record
// ----------------------------------------------------------------------------
struct InfernoDamageRecord_t {
    bool    bInFire{false};
    float   flHealthDamage{0.0f};
    float   flArmorDamage{0.0f};
    float   flThermalIntensity{0.0f};
    float   flDistanceToClosestNode{9999.0f};
    int32_t nClosestNodeIndex{-1};
};

/**
 * @brief Authoritative Surface Combustion & Thermal Energy Grid (CInferno)
 * Reconstructed from server.dll Build 26598758:
 * - 64-node flame cluster storage (m_firePositions, m_BurnNormal, m_bFireIsBurning).
 * - Downward ground-snapping raycasting with displacement brush alignment.
 * - Dynamic 2D/3D surface propagation with slope climbing and obstacle clearance gates.
 * - Mutual interaction & immediate quench against CSmokeVoxelGrid (Morton 32^3 & 144u sphere).
 * - 1/3rd threshold full-quench rule ("Molotov extinguished: when %d/%d fire areas were covered").
 * - Area damage evaluation with tick-rate intervals, radial falloff, and exposure ramp timers.
 */
class CInfernoFireGrid {
public:
    CInfernoFireGrid();

    /**
     * @brief Spawns the combustion cluster at detonation origin.
     * Evaluates immediate smoke quench if detonated inside an active smoke cloud.
     */
    void Spawn(const Vector3& vecOrigin,
               int32_t nStartTick = 0,
               float flCurrentTime = 0.0f,
               const CSmokeVoxelGrid* pSmoke = nullptr,
               const TerrainRaycastFn& terrainQuery = nullptr);

    /**
     * @brief Per-tick simulation loop (VA 0x180346090 / RVA 0x00346090).
     * Executes check expired, mutual smoke extinguishment, surface spread, and timer decay.
     */
    void OnTick(float flCurrentTime,
                int32_t nCurrentTick,
                const CSmokeVoxelGrid* pActiveSmoke = nullptr,
                const TerrainRaycastFn& terrainQuery = nullptr);

    /**
     * @brief Flame node lifecycle & burnout evaluation (VA 0x180340bc0 / RVA 0x00340bc0).
     * Updates burning state; transitions to post-effect if all nodes burn out.
     */
    void CheckExpired(float flCurrentTime);

    /**
     * @brief Dynamic 2D/3D surface propagation algorithm (VA 0x18034ef00 / RVA 0x0034ef00).
     * Expands cluster nodes radially along terrain tangent planes up to 64 nodes.
     */
    int32_t SpreadStep(float flCurrentTime, const TerrainRaycastFn& terrainQuery = nullptr);

    /**
     * @brief Mutual Smoke Voxel Interaction & Extinguishment (server.dll RVA 0x00346330-0x0034645f).
     * Quenches nodes overlapping active smoke voxels or 144-unit core detonation sphere.
     * Quenches entire inferno if >= 1/3rd of active nodes are extinguished.
     */
    int32_t ExtinguishOverlappingSmoke(const CSmokeVoxelGrid& smokeGrid, float flCurrentTime = 0.0f);

    /**
     * @brief Extinguishes an individual flame node (VA 0x180344790 / RVA 0x00344790).
     */
    void ExtinguishNode(int32_t nIndex);

    /**
     * @brief Extinguishes the entire inferno immediately.
     */
    void ExtinguishAll();

    /**
     * @brief Evaluates area damage and exposure ramp at target world position.
     * Reconstructed from CInferno::InfernoThink damage phase (VA 0x1803468c4).
     */
    [[nodiscard]] InfernoDamageRecord_t EvaluateDamageAtPosition(
        const Vector3& vecTargetPos,
        float flTargetExposureTime,
        float flCurrentTime) const;

    // State Inspection & Accessors
    [[nodiscard]] int32_t GetFireCount() const noexcept { return m_fireCount; }
    [[nodiscard]] int32_t GetActiveBurningCount() const noexcept;
    [[nodiscard]] int32_t GetExtinguishedCount() const noexcept { return m_nExtinguishedFlames; }
    [[nodiscard]] bool    IsActive() const noexcept { return m_bActive && !m_bInPostEffectTime; }
    [[nodiscard]] bool    IsInPostEffect() const noexcept { return m_bInPostEffectTime; }
    [[nodiscard]] bool    WasCreatedInSmoke() const noexcept { return m_bWasCreatedInSmoke; }
    [[nodiscard]] const Vector3& GetSpawnLocation() const noexcept { return m_vecOriginalSpawnLocation; }
    [[nodiscard]] const InfernoFlameNode_t& GetNode(int32_t idx) const { return m_nodes[idx]; }
    [[nodiscard]] const Vector3& GetFirePosition(int32_t idx) const { return m_firePositions[idx]; }
    [[nodiscard]] const Vector3& GetBurnNormal(int32_t idx) const { return m_BurnNormal[idx]; }
    [[nodiscard]] bool    IsFireBurning(int32_t idx) const { return m_bFireIsBurning[idx]; }

    void GetBoundingBox(Vector3& outMin, Vector3& outMax) const noexcept;

    // Built-in Default Terrain Raycaster (Analytical Plane & Wave Displacement)
    static TerrainTraceHit_t DefaultTerrainRaycast(const Vector3& start, const Vector3& end) noexcept;

private:
    // Core Engine Schema Reconstructed Fields
    Vector3 m_firePositions[MAX_INFERNO_FIRES]{};       // +0x0850: Node world positions
    Vector3 m_fireParentPositions[MAX_INFERNO_FIRES]{}; // +0x0b50: Parent node positions
    bool    m_bFireIsBurning[MAX_INFERNO_FIRES]{};      // +0x0e50: Active burning bitmask/array
    Vector3 m_BurnNormal[MAX_INFERNO_FIRES]{};          // +0x0e90: Contact surface normals
    int32_t m_fireCount{0};                             // +0x1190: Allocated flame count (0..64)
    int32_t m_nInfernoType{0};                          // +0x1194: 0 = Molotov, 1 = Incendiary
    int32_t m_nFireEffectTickBegin{0};                  // +0x1198: Ignition start tick
    float   m_nFireLifetime{INFERNO_LIFETIME};          // +0x119c: Lifetime in seconds
    bool    m_bInPostEffectTime{false};                 // +0x11a0: Post-burn decay state
    bool    m_bWasCreatedInSmoke{false};                // +0x11a1: Quenched upon spawn inside smoke
    Vector3 m_extent{INFERNO_SPREAD_RADIUS, INFERNO_SPREAD_RADIUS, 64.0f}; // +0x13a8
    Vector3 m_vecOriginalSpawnLocation{0.0f, 0.0f, 0.0f}; // +0x1414
    int32_t m_nMaxFlames{MAX_INFERNO_FIRES};            // +0x1434: Max flame cap (64)
    int32_t m_nSpreadCount{0};                          // +0x1438: Spread pulses executed

    // Simulation Internal State
    InfernoFlameNode_t m_nodes[MAX_INFERNO_FIRES]{};
    bool    m_bActive{false};
    float   m_flSpawnTime{0.0f};
    float   m_flLastSpreadTime{0.0f};
    float   m_flLastDamageTime{0.0f};
    int32_t m_nExtinguishedFlames{0};
};

} // namespace cs2::particles

#endif // CS2_PARTICLES_INFERNO_FIRE_GRID_H
