#ifndef CS2_SERVER_BULLET_PENETRATION_H
#define CS2_SERVER_BULLET_PENETRATION_H

#include "../client/viewmatrix.h"
#include "hit_registration_damage.h"
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>

namespace cs2::server {

using client::Vector3;

// -------------------------------------------------------------------------
// Authoritative Engine Constants & Virtual Offsets (server.dll @ Build 26598758)
// -------------------------------------------------------------------------
constexpr uintptr_t VA_EVALUATE_BULLET_PENETRATION     = 0x180a90280; // RVA: 0x00a90280
constexpr uintptr_t VA_APPLY_ARMOR_DAMAGE_CALCULATION = 0x180a81610; // RVA: 0x00a81610
constexpr uintptr_t VA_IS_ARMORED                     = 0x180acaca0; // RVA: 0x00acaca0

constexpr float RAY_STEP_SIZE              = 4.0f;    // 4-unit discrete ray interval stepping
constexpr float MAX_PENETRATION_DISTANCE  = 3000.0f; // Maximum travel distance gate for penetration
constexpr float MIN_PENETRATION_MODIFIER  = 0.10f;   // Minimum material penetration modifier gate

// -------------------------------------------------------------------------
// Source 2 / CS2 Authoritative Material Texture Characters
// (From surfacedata.txt / Rubikon Physics Surface Properties)
// -------------------------------------------------------------------------
constexpr uint16_t CHAR_TEX_ANTLION      = 'A'; // 0x41
constexpr uint16_t CHAR_TEX_BLOODYFLESH  = 'B'; // 0x42
constexpr uint16_t CHAR_TEX_CONCRETE     = 'C'; // 0x43
constexpr uint16_t CHAR_TEX_DIRT         = 'D'; // 0x44
constexpr uint16_t CHAR_TEX_EGGSHELL     = 'E'; // 0x45
constexpr uint16_t CHAR_TEX_FLESH        = 'F'; // 0x46
constexpr uint16_t CHAR_TEX_GRATE        = 'G'; // 0x47 (Grate / Chainlink)
constexpr uint16_t CHAR_TEX_GLASS        = 'G'; // 0x47 (Glass)
constexpr uint16_t CHAR_TEX_ALIENFLESH   = 'H'; // 0x48
constexpr uint16_t CHAR_TEX_CLIP         = 'I'; // 0x49
constexpr uint16_t CHAR_TEX_PLASTIC      = 'L'; // 0x4C (Light metal / sheet metal / plastic)
constexpr uint16_t CHAR_TEX_METAL        = 'M'; // 0x4D (Solid Metal / Steel)
constexpr uint16_t CHAR_TEX_SAND         = 'N'; // 0x4E
constexpr uint16_t CHAR_TEX_FOLIAGE      = 'O'; // 0x4F
constexpr uint16_t CHAR_TEX_COMPUTER     = 'P'; // 0x50
constexpr uint16_t CHAR_TEX_SLOSH        = 'S'; // 0x53
constexpr uint16_t CHAR_TEX_TILE         = 'T'; // 0x54
constexpr uint16_t CHAR_TEX_CARDBOARD    = 'U'; // 0x55 (Cardboard / Drywall)
constexpr uint16_t CHAR_TEX_VENT         = 'V'; // 0x56
constexpr uint16_t CHAR_TEX_WOOD         = 'W'; // 0x57 (Wood)
constexpr uint16_t CHAR_TEX_SHATTERGLASS = 'Y'; // 0x59 (Shatterglass / Plexiglass)

// Surface descriptor bitflags (Bit 13 tested via BT reg, 0xd @ 0x180a902eb)
constexpr uint32_t SURF_FLAG_GRATE_MESH  = (1 << 13); // 0x2000



// -------------------------------------------------------------------------
// Reconstructed 32-Byte Rubikon Physics Surface Property Layout
// -------------------------------------------------------------------------
struct SurfaceData_t {
    float maxSpeedFactor{1.0f};                    // +0x00
    float jumpFactor{1.0f};                        // +0x04
    float bulletPenetrationDistanceModifier{1.0f}; // +0x08 (flPenetrationDistance / flPenetrationModifier)
    float bulletPenetrationDamageModifier{0.16f};  // +0x0C (flDamageModifier, default 0.16)
    uint32_t stringToken{0};                       // +0x10 (MurmurHash2 / FNV-1a token)
    uint16_t materialChar{CHAR_TEX_CONCRETE};      // +0x14 (e.g. 'C', 'W', 'M', 'G', 'F', 'Y')
    uint16_t soundStackMaterial{0};                // +0x16
    uint8_t flags18{0};                            // +0x18
    bool isStairs{false};                          // +0x19
    bool isSlippery{false};                        // +0x1A
    uint8_t flags1B{0};                            // +0x1B
    bool reflectBullets{false};                    // +0x1C
    bool reduceWeaponImpact{false};                // +0x1D
    bool bulletsPenetrate{true};                   // +0x1E
    uint8_t pad1F{0};                              // +0x1F
};

// -------------------------------------------------------------------------
// In-Flight Bullet Parameter Block
// -------------------------------------------------------------------------
struct BulletData_t {
    float currentDamage{0.0f};      // param_2[0]: Current accumulated damage
    float penetrationPower{1.0f};   // param_2[1]: Weapon penetration power (e.g. 1.0, 2.0, 2.5)
    float rangeModifier{0.98f};     // param_2[2]: Weapon range falloff modifier (e.g. 0.98)
    float maxRange{8192.0f};        // param_2[3]: Trace scale / maximum weapon range
    int penetrationCount{4};        // param_2[4]: Remaining penetrating objects
    bool stopped{false};            // param_2[5]: Flag set if bullet is stopped
};

// -------------------------------------------------------------------------
// Weapon Ballistics Profile
// -------------------------------------------------------------------------
struct WeaponBallistics_t {
    const char* m_pName{"AK-47"};
    float       m_flDamage{36.0f};          // Base damage at 0 range
    float       m_flRangeModifier{0.98f};   // Falloff coefficient per 500 units
    float       m_flPenetrationPower{2.0f}; // Penetration power multiplier
    float       m_flArmorRatio{1.55f};      // Weapon Armor Penetration Ratio (e.g. 1.55f = 77.5%)
    float       m_flMaxRange{8192.0f};      // Maximum ballistic range
    int         m_nMaxPenetrations{4};      // Maximum material surfaces
};

// -------------------------------------------------------------------------
// Player Defense State (Vitality & Defensive Armor)
// -------------------------------------------------------------------------
struct PlayerPawnState_t {
    int32_t m_iHealth{100};
    int32_t m_ArmorValue{100};
    bool    m_bHasHelmet{true};
    bool    m_bHasHeavyArmor{false};
};

// -------------------------------------------------------------------------
// Armor Mitigation Calculation Output
// -------------------------------------------------------------------------
struct ArmorMitigationResult_t {
    float   m_flHealthDamage{0.0f};
    int32_t m_iArmorAbsorbed{0};
    int32_t m_iRemainingArmor{0};
    int32_t m_iRemainingHealth{0};
    bool    m_bArmoredCovered{false};
};

// -------------------------------------------------------------------------
// Penetration Step Diagnostic Record
// -------------------------------------------------------------------------
struct PenetrationStep_t {
    int         m_nStepIndex{0};
    std::string m_sMaterialName;
    float       m_flEntryDistance{0.0f};
    float       m_flThickness{0.0f};
    float       m_flDamageBefore{0.0f};
    float       m_flDamageAfter{0.0f};
    float       m_flDamageLost{0.0f};
    bool        m_bPenetrated{false};
    std::string m_sFailReason;
};

// -------------------------------------------------------------------------
// Surface Hit & Ray Interval Descriptor
// -------------------------------------------------------------------------
struct SurfaceHitData_t {
    float tEnter{0.0f};             // Entry fraction along ray [0.0, 1.0]
    float tExit{1.0f};              // Exit fraction along ray [0.0, 1.0]
    uint32_t flags{1};              // Bit 0: 0 = Air interval, 1 = Solid material interval
    Vector3 enterPoint;             // World coordinate where ray entered
    Vector3 exitPoint;              // World coordinate where ray exited
    Vector3 enterNormal;            // Surface normal at entry
    Vector3 exitNormal;             // Surface normal at exit
    SurfaceData_t enterSurface;     // Physical surface descriptor at entry
    SurfaceData_t exitSurface;      // Physical surface descriptor at exit
    bool enterIsGrate{false};       // Grate / chainlink flag at entry (Bit 13)
    bool exitIsGrate{false};        // Grate / chainlink flag at exit (Bit 13)
    bool isTeammate{false};         // True if hit pawn belongs to shooter's team
    uint32_t hitEntityHandle{0xFFFFFFFF}; // Entity handle of hit object
    int hitGroupId{0};              // Hitgroup (0=generic, 1=head, 2=chest, etc.)
};

// -------------------------------------------------------------------------
// Authoritative Bullet Impact Event Data
// -------------------------------------------------------------------------
struct BulletImpact_t {
    Vector3 position;
    Vector3 normal;
    float damageApplied{0.0f};
    float totalDistance{0.0f};
    int objectIndex{0};
    uint32_t hitEntityHandle{0xFFFFFFFF};
    int hitGroupId{0};
};

// -------------------------------------------------------------------------
// Player Hitbox & Capsule Model for Authoritative Hitscan
// -------------------------------------------------------------------------
struct PlayerHitbox_t {
    int hitboxId{0};
    int hitGroup{0};
    Vector3 mins;
    Vector3 maxs;
    float radius{0.0f};
};

struct HitPlayerPawn_t {
    uint32_t entityHandle{0xFFFFFFFF};
    uint32_t teamId{0};
    bool isAlive{true};
    Vector3 origin;
    Vector3 mins{-16.0f, -16.0f, 0.0f};
    Vector3 maxs{16.0f, 16.0f, 72.0f};
    std::vector<PlayerHitbox_t> hitboxes;
    PlayerPawnState_t pawnState{};
};

// -------------------------------------------------------------------------
// Weapon Fire Parameters
// -------------------------------------------------------------------------
struct FireBulletParams_t {
    Vector3 vStart;
    Vector3 vDirection;
    float baseDamage{30.0f};
    float flRangeModifier{0.98f};
    float flPenetrationPower{2.0f};
    float flArmorRatio{1.55f};
    float flMaxRange{8192.0f};
    int iPenetrationCount{4};
    int numPellets{1};
    float flSpread{0.0f};
    float flInaccuracy{0.0f};
    uint32_t attackerHandle{0xFFFFFFFF};
    uint32_t attackerTeam{2};
};

// -------------------------------------------------------------------------
// Authoritative Bullet Penetration & Surface Physics Pipeline
// Reconstructed from server.dll Build 26598758
// -------------------------------------------------------------------------
class CBulletPenetration {
public:
    /**
     * @brief Air Travel Distance Attenuation ODE:
     * D_rem = D_base * pow(flRangeModifier, distance / 500.0f)
     * server.dll @ 0x180a90280 / VA 0x180a902ea
     */
    static float ApplyDistanceFalloff(float baseDamage, float rangeModifier, float distance) noexcept;

    /**
     * @brief Authoritative Solid Surface Absorption ODE.
     * Evaluates material absorption, penetration power reduction, and damage loss.
     * Reconstructed from EvaluateBulletPenetration @ RVA 0x00a90280 / VA 0x180a90280.
     */
    static bool EvaluateSurfacePenetration(BulletData_t& bullet,
                                          const SurfaceHitData_t& hit,
                                          bool ffBulletPenetration = false);

    /**
     * @brief Detailed evaluation overload logging diagnostic step.
     */
    static bool EvaluateSurfacePenetrationDetailed(
        const WeaponBallistics_t& weapon,
        const SurfaceData_t& enterSurf,
        const SurfaceData_t& exitSurf,
        float thickness,
        float currentDistance,
        float& currentDamage,
        int& remainingPenetrations,
        PenetrationStep_t& stepOut);

    /**
     * @brief Authoritative 8-Way HitGroup Armor Coverage Check
     * Reconstructed from CCSPlayerPawn::IsArmored @ RVA 0x00acaca0 / VA 0x180acaca0.
     */
    static bool IsArmored(const PlayerPawnState_t& pawn, HitGroup_t hitGroup) noexcept;

    /**
     * @brief Authoritative Armor Mitigation & Durability Depletion
     * Reconstructed from CCSPlayerPawn::ApplyArmorDamageCalculation @ RVA 0x00a81610 / VA 0x180a81610.
     */
    static ArmorMitigationResult_t ApplyArmorDamageCalculation(
        PlayerPawnState_t& pawn,
        float incomingDamage,
        float weaponArmorRatio,
        HitGroup_t hitGroup,
        bool isHeavyArmorMode = false) noexcept;

    /**
     * @brief 4-Unit Discrete Ray Interval Stepping Helper
     * Steps forward along ray by 4.0 units (RAY_STEP_SIZE) to sample solid bounds.
     */
    static bool TraceToExit(
        const Vector3& vStart,
        const Vector3& vDirection,
        float maxDistance,
        Vector3& outExitPoint,
        float stepSize = RAY_STEP_SIZE);

    /**
     * @brief Ray vs Player Bounding Capsule / Studio Hitbox Clipping
     */
    static bool ClipTraceToPlayers(const Vector3& vStart,
                                  const Vector3& vEnd,
                                  const std::vector<HitPlayerPawn_t>& players,
                                  uint32_t attackerHandle,
                                  SurfaceHitData_t& outHit);

    /**
     * @brief Authoritative Bullet Hitscan Pipeline
     */
    static std::vector<BulletImpact_t> FireBulletsAuthoritative(
        const FireBulletParams_t& params,
        const std::vector<SurfaceHitData_t>& worldGeometry,
        std::vector<HitPlayerPawn_t>& players,
        bool ffBulletPenetration = false);

    /**
     * @brief Standard surface property lookup table for CS2 materials.
     */
    static SurfaceData_t GetSurfaceProperties(uint16_t materialChar);

    // Material convenience helpers matching simulator
    static SurfaceData_t GetSurfaceWood();
    static SurfaceData_t GetSurfaceConcrete();
    static SurfaceData_t GetSurfaceMetal();
    static SurfaceData_t GetSurfaceFlesh();
    static SurfaceData_t GetSurfaceCardboard();
    static SurfaceData_t GetSurfaceGlass();
    static SurfaceData_t GetSurfacePlastic();
};

} // namespace cs2::server

#endif // CS2_SERVER_BULLET_PENETRATION_H
