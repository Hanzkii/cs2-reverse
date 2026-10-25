#ifndef CS2_CLIENT_MOVEMENT_H
#define CS2_CLIENT_MOVEMENT_H

#include "viewmatrix.h"
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>

namespace cs2::client {

// Standard Source 2 button bitmasks
enum MoveButtonBits : uint64_t {
    IN_ATTACK       = (1ULL << 0),
    IN_JUMP         = (1ULL << 1),
    IN_DUCK         = (1ULL << 2),
    IN_FORWARD      = (1ULL << 3),
    IN_BACK         = (1ULL << 4),
    IN_USE          = (1ULL << 5),
    IN_CANCEL       = (1ULL << 6),
    IN_LEFT         = (1ULL << 7),
    IN_RIGHT        = (1ULL << 8),
    IN_MOVELEFT     = (1ULL << 9),
    IN_MOVERIGHT    = (1ULL << 10),
    IN_ATTACK2      = (1ULL << 11),
    IN_RUN          = (1ULL << 12),
    IN_RELOAD       = (1ULL << 13),
    IN_ALT1         = (1ULL << 14),
    IN_ALT2         = (1ULL << 15),
    IN_SPEED        = (1ULL << 16),
    IN_WALK         = (1ULL << 17),
    IN_ZOOM         = (1ULL << 19),
    IN_BULLRUSH     = (1ULL << 22),
    IN_GRENADE1     = (1ULL << 23),
    IN_GRENADE2     = (1ULL << 24),
    IN_ATTACK3      = (1ULL << 25),
};

enum MoveType_t : uint8_t {
    MOVETYPE_NONE           = 0,
    MOVETYPE_ISOMETRIC      = 1,
    MOVETYPE_WALK           = 2,
    MOVETYPE_STEP           = 3,
    MOVETYPE_FLY            = 4,
    MOVETYPE_FLYGRAVITY     = 5,
    MOVETYPE_VPHYSICS       = 6,
    MOVETYPE_PUSH           = 7,
    MOVETYPE_NOCLIP         = 8,
    MOVETYPE_LADDER         = 9,
    MOVETYPE_OBSERVER       = 10,
    MOVETYPE_CUSTOM         = 11,
};

// Sub-tick input action event from CSGOUserCmdPB / CSubtickMoveStep
struct SubtickInputStep {
    float when = 0.0f;       // Sub-tick fractional offset [0.0, 1.0]
    uint64_t button = 0;     // MoveButtonBits
    bool pressed = false;    // True if key-down, false if key-up
};

// Movement simulation trace / collision result
struct MoveTrace_t {
    Vector3 startPos;
    Vector3 endPos;
    Vector3 planeNormal;
    float fraction = 1.0f;
    bool allSolid = false;
    bool startSolid = false;
    uint32_t hitEntityHandle = 0xFFFFFFFF;
};

// Player movement parameters passed into ProcessMovement (matching CMoveData)
struct PlayerMoveData {
    uint32_t playerHandle = 0xFFFFFFFF;
    MoveType_t moveType = MOVETYPE_WALK;
    Vector3 origin;
    Vector3 velocity;
    Vector3 baseVelocity;
    Vector3 viewAngles;
    Vector3 wishVelocity;
    Vector3 wishDirection;
    float wishSpeed = 0.0f;
    float forwardMove = 0.0f;
    float sideMove = 0.0f;
    float upMove = 0.0f;
    float maxSpeed = 320.0f;
    float clientMaxSpeed = 250.0f;
    uint64_t buttons = 0;
    uint64_t oldButtons = 0;
    bool onGround = false;
    std::vector<SubtickInputStep> subtickSteps;
};

// Movement services state container matching CCSPlayer_MovementServices & CPlayer_MovementServices
struct PlayerMovementServices {
    // Humanoid & surface physics
    float m_flSurfaceFriction = 1.0f;         // +0x026c
    float m_flStepSoundTime = 0.0f;           // +0x0258
    float m_flFallVelocity = 0.0f;            // +0x025c
    Vector3 m_groundNormal{0.0f, 0.0f, 1.0f}; // +0x0260

    // Ducking state
    bool m_bDucked = false;                   // +0x0408
    float m_flDuckAmount = 0.0f;              // +0x040c
    float m_flDuckSpeed = 0.0f;               // +0x0410
    bool m_bDuckOverride = false;             // +0x0414
    bool m_bDesiresDuck = false;              // +0x0415
    bool m_bDucking = false;                  // +0x0416

    // Locomotion & jumping
    bool m_bHasWalkMovedSinceLastJump = false; // +0x0439
    int32_t m_nTraceCount = 0;                // +0x0648
    float m_flStamina = 0.0f;                 // +0x069c
    float m_flHeightAtJumpStart = 0.0f;       // +0x06a0
    float m_flMaxJumpHeightThisJump = 0.0f;   // +0x06a4
    float m_flMaxJumpHeightLastJump = 0.0f;   // +0x06a8
    float m_flStaminaAtJumpStart = 0.0f;      // +0x06ac
    float m_flVelMulAtJumpStart = 1.0f;       // +0x06b0
    float m_flAccumulatedJumpError = 0.0f;    // +0x06b4
    int32_t m_nLastJumpTick = 0;              // +0x0708
    float m_flLastJumpFrac = 0.0f;            // +0x070c
    float m_flLastJumpVelocityZ = 0.0f;       // +0x0710
    bool m_bJumpApexPending = false;          // +0x0714
    Vector3 m_vecWalkWishVel;                 // +0x07a8
};

/**
 * @brief Source 2 Player Movement & Ground Physics Pipeline
 * Reconstructed clean-room implementation from client.dll (FUN_1808df090, FUN_1808ea1b0, FUN_1808bee90, FUN_1808bfa30)
 */
class CPlayerMovement {
public:
    // Core Engine Physics Constants
    static constexpr float TICK_INTERVAL             = 1.0f / 64.0f; // 0.015625s
    static constexpr float SV_GRAVITY                = 800.0f;       // u/s^2 (sv_gravity)
    static constexpr float SV_FRICTION               = 5.2f;         // sv_friction
    static constexpr float SV_STOPSPEED              = 100.0f;       // sv_stopspeed
    static constexpr float SV_ACCELERATE             = 5.5f;         // sv_accelerate
    static constexpr float SV_AIRACCELERATE          = 12.0f;        // sv_airaccelerate
    static constexpr float SV_AIR_MAX_WISHSPEED      = 30.0f;        // sv_air_max_wishspeed
    static constexpr float SV_MAXSPEED               = 320.0f;       // sv_maxspeed
    static constexpr float SV_STEPSIZE               = 18.0f;        // sv_stepsize
    static constexpr float SV_STANDABLE_NORMAL_Z     = 0.7f;         // Cosine of 45.57 deg max walkable slope
    static constexpr float SV_JUMP_IMPULSE           = 301.993378f;  // sqrt(2 * 800 * 57)
    static constexpr float CS_DUCK_SPEED_MULTIPLIER  = 0.34f;        // Crouch speed scalar
    static constexpr float CS_WALK_SPEED_MULTIPLIER  = 0.52f;        // Shift walking scalar

    // Master execution dispatcher (FUN_1808df090)
    static void ProcessMovement(PlayerMovementServices& services, PlayerMoveData& mv, float dt);

    // Ground locomotion pipeline (FUN_1808ea1b0)
    static void WalkMove(PlayerMovementServices& services, PlayerMoveData& mv, float dt);

    // Airborne locomotion pipeline (FUN_1808e57b0)
    static void AirMove(PlayerMovementServices& services, PlayerMoveData& mv, float dt);

    // Ground surface friction (FUN_1808cbf50)
    static void Friction(PlayerMovementServices& services, PlayerMoveData& mv, float dt);

    // Directional ground acceleration with weapon and stance scaling (FUN_1808bee90 / FUN_1808bef60)
    static void Accelerate(PlayerMovementServices& services, PlayerMoveData& mv,
                           const Vector3& wishdir, float wishspeed, float accel, float dt);

    // Airborne strafe acceleration clamped to sv_air_max_wishspeed (FUN_1808bfa30)
    static void AirAccelerate(PlayerMovementServices& services, PlayerMoveData& mv,
                              const Vector3& wishdir, float wishspeed, float accel, float dt);

    // Surface detection and ground snapping (FUN_1808e6020)
    static void CategorizePosition(PlayerMovementServices& services, PlayerMoveData& mv, float dt);

    // Stair and curb obstacle stepping (FUN_1808e1940)
    static void StepMove(PlayerMovementServices& services, PlayerMoveData& mv, float dt);

    // Jump button handling and sub-tick impulse generation (FUN_1808c34f0 / FUN_1808e7e70)
    static bool CheckJumpButton(PlayerMovementServices& services, PlayerMoveData& mv, float dt);

    // Sub-tick continuous jump velocity modulation (FUN_1808c0460)
    static float CalculateSubtickJumpVelocity(const PlayerMovementServices& services,
                                              float subtickFraction, float horizontalSpeed);

    // Helper: Environment trace simulation (swept AABB against world geometry)
    static MoveTrace_t TracePlayerBBox(const Vector3& start, const Vector3& end,
                                       const PlayerMovementServices& services);

    // Helper: Clip velocity against collision plane normal
    static void ClipVelocity(const Vector3& in, const Vector3& normal, Vector3& out, float overbounce = 1.0f);
};

} // namespace cs2::client

#endif // CS2_CLIENT_MOVEMENT_H
