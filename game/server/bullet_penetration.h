#ifndef CS2_SERVER_BULLET_PENETRATION_H
#define CS2_SERVER_BULLET_PENETRATION_H

#include "../client/viewmatrix.h"
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>

namespace cs2::server {

using client::Vector3;

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
constexpr uint16_t CHAR_TEX_METAL        = 'M'; // 0x4D (Solid Metal)
constexpr uint16_t CHAR_TEX_SAND         = 'N'; // 0x4E
constexpr uint16_t CHAR_TEX_FOLIAGE      = 'O'; // 0x4F
constexpr uint16_t CHAR_TEX_COMPUTER     = 'P'; // 0x50
constexpr uint16_t CHAR_TEX_SLOSH        = 'S'; // 0x53
constexpr uint16_t CHAR_TEX_TILE         = 'T'; // 0x54
constexpr uint16_t CHAR_TEX_CARDBOARD    = 'U'; // 0x55 (Cardboard / thin panel)
constexpr uint16_t CHAR_TEX_VENT         = 'V'; // 0x56
constexpr uint16_t CHAR_TEX_WOOD         = 'W'; // 0x57 (Wood)
constexpr uint16_t CHAR_TEX_SHATTERGLASS = 'Y'; // 0x59 (Shatterglass / Plexiglass)

// Surface descriptor bitflags (Bit 13 tested via BT reg, 0xd @ 0x180a902eb)
constexpr uint32_t SURF_FLAG_GRATE_MESH  = (1 << 13); // 0x2000

// -------------------------------------------------------------------------
// Reconstructed 32-Byte Rubikon Physics Surface Property Layout
// Reversed from FUN_180be1120 in server.dll (surfaceproperties_game.txt)
// -------------------------------------------------------------------------
struct SurfaceData_t {
    float maxSpeedFactor{1.0f};                    // +0x00
    float jumpFactor{1.0f};                        // +0x04
    float bulletPenetrationDistanceModifier{1.0f}; // +0x08 (flPenetrationDistance)
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
// Corresponds to param_2 array in FUN_180a8fca0
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
// Surface Hit & Ray Interval Descriptor
// Corresponds to param_3 interval record in FUN_180a6ca20 / FUN_180a8fca0
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
// Emitted as "bullet_impact" event in FUN_180a70240 @ 0x180a70a61
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
    int hitGroup{0}; // 1 = Head, 2 = Chest, 3 = Stomach, 4 = Left Arm, 5 = Right Arm, 6 = Left Leg, 7 = Right Leg
    Vector3 mins;
    Vector3 maxs;
    float radius{0.0f}; // If radius > 0, treated as capsule; otherwise OBB/AABB
};

struct HitPlayerPawn_t {
    uint32_t entityHandle{0xFFFFFFFF};
    uint32_t teamId{0};
    bool isAlive{true};
    Vector3 origin;
    Vector3 mins{-16.0f, -16.0f, 0.0f};
    Vector3 maxs{16.0f, 16.0f, 72.0f};
    std::vector<PlayerHitbox_t> hitboxes;
};

// -------------------------------------------------------------------------
// Weapon Fire Parameters (CCSPlayerPawn::FireBullet @ FUN_1802975d0)
// -------------------------------------------------------------------------
struct FireBulletParams_t {
    Vector3 vStart;
    Vector3 vDirection;
    float baseDamage{30.0f};
    float flRangeModifier{0.98f};
    float flPenetrationPower{2.0f};
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
// Reconstructed clean-room from server.dll
// -------------------------------------------------------------------------
class CBulletPenetration {
public:
    /**
     * @brief Authoritative damage distance falloff formula:
     * flDamage = baseDamage * pow(flRangeModifier, distance / 500.0f)
     * Reverse engineered from FUN_180a8fca0 @ 0x180a8fd0a
     */
    static float ApplyDistanceFalloff(float baseDamage, float rangeModifier, float distance);

    /**
     * @brief Authoritative surface penetration evaluation loop.
     * Reverse engineered from FUN_180a8fca0 in server.dll.
     * Computes material absorption, penetration power reduction, and damage loss.
     * 
     * @param bullet In-flight bullet tracking state (damage, power, count, stopped)
     * @param hit Surface interval information (thickness, materials, normals)
     * @param ffBulletPenetration Value of ConVar ff_damage_bullet_penetration (default false)
     * @return true if bullet penetrates through surface; false if stopped
     */
    static bool EvaluateSurfacePenetration(BulletData_t& bullet,
                                          const SurfaceHitData_t& hit,
                                          bool ffBulletPenetration = false);

    /**
     * @brief Ray vs Player Bounding Capsule / Studio Hitbox Clipping
     * Reverse engineered from ClipTraceToPlayers (FUN_180181cd0 in server.dll).
     * Tests candidate players within a 40-unit skin expansion margin.
     * 
     * @param vStart Ray origin
     * @param vEnd Ray termination
     * @param players Candidate active player pawns
     * @param attackerHandle Handle of shooter to prevent self-collision
     * @param outHit Output hit information if a player was intercepted
     * @return true if a player was hit, false otherwise
     */
    static bool ClipTraceToPlayers(const Vector3& vStart,
                                  const Vector3& vEnd,
                                  const std::vector<HitPlayerPawn_t>& players,
                                  uint32_t attackerHandle,
                                  SurfaceHitData_t& outHit);

    /**
     * @brief Authoritative Bullet Hitscan Pipeline
     * Reverse engineered from CCSPlayerPawn::FireBullet (FUN_1802975d0) and FUN_180a70240.
     * Traces rays through world geometry and player hitboxes, evaluating penetration
     * across sequential intervals and collecting authoritative impacts.
     */
    static std::vector<BulletImpact_t> FireBulletsAuthoritative(
        const FireBulletParams_t& params,
        const std::vector<SurfaceHitData_t>& worldGeometry,
        const std::vector<HitPlayerPawn_t>& players,
        bool ffBulletPenetration = false);

    /**
     * @brief Standard surface property lookup table generator for common CS2 materials.
     */
    static SurfaceData_t GetSurfaceProperties(uint16_t materialChar);
};

} // namespace cs2::server

#endif // CS2_SERVER_BULLET_PENETRATION_H
