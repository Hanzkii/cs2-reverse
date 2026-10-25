#ifndef CS2_CLIENT_WEAPON_ACCURACY_H
#define CS2_CLIENT_WEAPON_ACCURACY_H

#include "viewmatrix.h"
#include <cstdint>
#include <cmath>

namespace cs2::client {

/**
 * @brief Weapon configuration parameters parsed from CCSWeaponBaseVData (client.dll & server.dll).
 */
struct CCSWeaponBaseVData_Accuracy {
    float m_flSpread = 0.002f;
    float m_flInaccuracyCrouch = 0.004f;
    float m_flInaccuracyStand = 0.006f;
    float m_flInaccuracyJump = 0.350f;
    float m_flInaccuracyLand = 0.080f;
    float m_flInaccuracyLadder = 0.110f;
    float m_flInaccuracyFire = 0.055f;
    float m_flInaccuracyMove = 0.130f;
    float m_flInaccuracyReload = 0.020f;
    float m_flInaccuracyJumpInitial = 0.450f;
    float m_flInaccuracyJumpApex = 0.280f;

    // Recovery Times (exponential penalty decay)
    float m_flRecoveryTimeCrouch = 0.340f;
    float m_flRecoveryTimeStand = 0.450f;
    float m_flRecoveryTimeCrouchFinal = 0.420f;
    float m_flRecoveryTimeStandFinal = 0.580f;
    int32_t m_nRecoveryTransitionStartBullet = 2;
    int32_t m_nRecoveryTransitionEndBullet = 5;

    // Recoil Punch Properties
    float m_flRecoilAngle = 0.0f;
    float m_flRecoilAngleVariance = 0.0f;
    float m_flRecoilMagnitude = 21.0f;
    float m_flRecoilMagnitudeVariance = 1.0f;

    float m_flCycleTime = 0.09f;
    float m_flMaxSpeed = 250.0f;
    int32_t m_nNumBullets = 1;
};

/**
 * @brief Dynamic runtime weapon accuracy and recoil state (C_CSWeaponBase).
 */
struct WeaponAccuracyState {
    float m_fAccuracyPenalty = 0.0f;              // +0x1a18
    float m_flLastAccuracyUpdateTime = 0.0f;      // +0x1a1c
    float m_fAccuracySmoothedForZoom = 0.0f;      // +0x1a20
    int32_t m_iRecoilIndex = 0;                   // +0x1a24
    float m_flRecoilIndex = 0.0f;                 // +0x1a28
    float m_flTurningInaccuracyDelta = 0.0f;      // +0x1a04
    Vector3 m_vecTurningInaccuracyEyeDirLast{};   // +0x1a08
    float m_flTurningInaccuracy = 0.0f;           // +0x1a14
    float m_fLastShotTime = 0.0f;                 // +0x1b68
    int32_t m_weaponMode = 0;                     // +0x1a00
    bool m_bInReload = false;                     // +0x1a3c
};

/**
 * @brief Player Pawn physical state context required for accuracy evaluation.
 */
struct PlayerPawnAccuracyContext {
    Vector3 velocity{};
    QAngle eyeAngles{};
    bool onGround = true;
    bool ducking = false;
    bool onLadder = false;
    int32_t boostStackCount = 0; // Stacked atop other players
};

/**
 * @brief Recoil aim punch spring-damper state (CPlayer_AimPunchServices).
 */
struct AimPunchState {
    int32_t m_predictableBaseTick = 0;            // +0x0048
    float m_predictableBaseTickInterpAmount = 0.0f; // +0x004c
    QAngle m_aimPunchAngle{};                     // +0x0050 (m_predictableBaseAngle)
    QAngle m_aimPunchAngleVel{};                  // +0x005c (m_predictableBaseAngleVel)
};

/**
 * @brief Global engine ConVars controlling accuracy and recoil physics.
 */
struct AccuracyConVars {
    bool sv_turning_inaccuracy_enabled = true;
    float sv_turning_inaccuracy_angle_min = 1.0f;     // deg
    float sv_turning_inaccuracy_decay = 0.90f;        // rate per evaluation
    bool sv_strafing_inaccuracy_enabled = true;
    float sv_strafing_inaccuracy_bias = 0.45f;
    float sv_strafing_inaccuracy_scale = 0.65f;
    float weapon_accuracy_forcespread = 0.0f;
    bool weapon_accuracy_nospread = false;
    int32_t weapon_accuracy_stack_boost_limit = 1;
    float weapon_recoil_decay = 4.5f;                 // Second-order spring damping
};

/**
 * @brief Clean-room reconstructed CS2 Weapon Accuracy, Spread Cone, and Recoil Decay Pipeline.
 * Reverse engineered from client.dll (FUN_180827be0, FUN_18080c1a0, FUN_18080cef0, FUN_18085dc60).
 */
class CWeaponAccuracyPipeline {
public:
    static constexpr float TICK_INTERVAL = 1.0f / 64.0f; // 0.015625s

    /**
     * @brief Evaluates recovery time based on stance, air status, and recoil index.
     * Reconstructed from FUN_18080cef0 @ RVA 0x0080cef0.
     */
    static float GetRecoveryTime(
        const CCSWeaponBaseVData_Accuracy& vdata,
        const PlayerPawnAccuracyContext& ctx,
        int32_t recoilIndex
    );

    /**
     * @brief Computes strafing-induced inaccuracy from movement velocity and eye angle.
     * Reconstructed from FUN_18080d4c0 @ RVA 0x0080d4c0.
     */
    static float CalculateStrafingInaccuracy(
        const PlayerPawnAccuracyContext& ctx,
        const AccuracyConVars& cvars
    );

    /**
     * @brief Updates turning delta and angular inaccuracy.
     * Reconstructed from FUN_180828d90 @ RVA 0x00828d90.
     */
    static void UpdateTurningInaccuracy(
        WeaponAccuracyState& state,
        const QAngle& eyeAngles,
        float dt,
        const AccuracyConVars& cvars
    );

    /**
     * @brief Main tick update for weapon accuracy penalty and recoil index decay.
     * Reconstructed from FUN_180827be0 @ RVA 0x00827be0.
     */
    static void UpdateAccuracyPenalty(
        WeaponAccuracyState& state,
        const CCSWeaponBaseVData_Accuracy& vdata,
        const PlayerPawnAccuracyContext& ctx,
        float curTime,
        float dt,
        const AccuracyConVars& cvars
    );

    /**
     * @brief Evaluates total composite weapon inaccuracy and base spread.
     * Reconstructed from FUN_18080c1a0 @ RVA 0x0080c1a0.
     */
    static float GetInaccuracy(
        const WeaponAccuracyState& state,
        const CCSWeaponBaseVData_Accuracy& vdata,
        const PlayerPawnAccuracyContext& ctx,
        float* outSpread,
        float* outJumpInacc,
        const AccuracyConVars& cvars
    );

    /**
     * @brief Handles weapon firing impulse: applies fire inaccuracy and recoil punch.
     */
    static void OnWeaponFired(
        WeaponAccuracyState& state,
        const CCSWeaponBaseVData_Accuracy& vdata,
        AimPunchState& punchState,
        float curTime
    );

    /**
     * @brief Simulates second-order recoil aim punch ODE spring damping per tick.
     * Reconstructed from FUN_18085dc60 @ RVA 0x0085dc60 and FUN_18084d8b0 @ RVA 0x0084d8b0.
     */
    static void SimulateAimPunchDecay(
        AimPunchState& punchState,
        float dt,
        float dampingRate = 4.5f
    );

    /**
     * @brief Generates 3D perturbed firing ray direction vector from spread cone.
     */
    static Vector3 CalculateSpreadConeRay(
        float inaccuracy,
        float spread,
        float randU1,
        float randU2,
        const Vector3& forward,
        const Vector3& right,
        const Vector3& up
    );
};

} // namespace cs2::client

#endif // CS2_CLIENT_WEAPON_ACCURACY_H
