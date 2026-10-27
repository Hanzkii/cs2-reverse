#include "weapon_accuracy.h"
#include <algorithm>
#include <cmath>

namespace cs2::client {

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;

static float ClampFloat(float val, float minVal, float maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

static float DotProduct(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vector3 NormalizeVector(const Vector3& v) {
    float mag = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (mag < 1e-9f) return {0.0f, 0.0f, 0.0f};
    float inv = 1.0f / mag;
    return {v.x * inv, v.y * inv, v.z * inv};
}

// =======================================================================================
// CUniformRandomStream: Valve Park-Miller LCG with Bays-Durham Shuffle Table (tier0.dll)
// Reconstructed from ?SetSeed_Locked@?$CUniformRandomStreamImpl@VCThreadNullMutex@@@@AEAAXH@Z
// and ?GenerateRandomNumber_Locked@?$CUniformRandomStreamImpl@VCThreadNullMutex@@@@AEAAHXZ
// =======================================================================================

constexpr int32_t IA_PM   = 16807;
constexpr int32_t IM_PM   = 2147483647; // 2^31 - 1
constexpr int32_t IQ_PM   = 127773;
constexpr int32_t IR_PM   = 2836;
constexpr int32_t NTAB_PM = 32;

void CUniformRandomStream::SetSeed(int32_t seed) {
    m_idum = (seed < 0) ? seed : -seed;
    if (m_idum == 0) m_idum = -1;
    m_iy = 0;
    for (int i = 0; i < NTAB_PM; ++i) {
        m_iv[i] = 0;
    }

    // Warm-up shuffle table matching tier0.dll
    for (int j = NTAB_PM + 7; j >= 0; --j) {
        int32_t k = m_idum / IQ_PM;
        m_idum = IA_PM * (m_idum - k * IQ_PM) - IR_PM * k;
        if (m_idum < 0) {
            m_idum += IM_PM;
        }
        if (j < NTAB_PM) {
            m_iv[j] = m_idum;
        }
    }
    m_iy = m_iv[0];
}

float CUniformRandomStream::RandomFloat(float minVal, float maxVal) {
    if (m_idum == 0) {
        SetSeed(1);
    }
    int32_t k = m_idum / IQ_PM;
    m_idum = IA_PM * (m_idum - k * IQ_PM) - IR_PM * k;
    if (m_idum < 0) {
        m_idum += IM_PM;
    }

    int32_t j = m_iy / (1 + (IM_PM - 1) / NTAB_PM);
    if (j < 0) j = 0;
    if (j >= NTAB_PM) j = NTAB_PM - 1;
    m_iy = m_iv[j];
    m_iv[j] = m_idum;

    float fl = static_cast<float>(m_iy) * (1.0f / static_cast<float>(IM_PM));
    if (fl > (1.0f - 1.2e-7f)) {
        fl = 1.0f - 1.2e-7f;
    }
    return minVal + fl * (maxVal - minVal);
}

int32_t CUniformRandomStream::RandomInt(int32_t minVal, int32_t maxVal) {
    return static_cast<int32_t>(RandomFloat(static_cast<float>(minVal), static_cast<float>(maxVal) + 0.999f));
}

// =======================================================================================
// CWeaponAccuracyPipeline Reconstructed Functions
// =======================================================================================

// FUN_18080cef0 @ RVA 0x0080cef0
float CWeaponAccuracyPipeline::GetRecoveryTime(
    const CCSWeaponBaseVData_Accuracy& vdata,
    const PlayerPawnAccuracyContext& ctx,
    int32_t recoilIndex
) {
    // Air recovery penalty: 4x slower recovery while airborne
    if (!ctx.onGround) {
        return vdata.m_flRecoveryTimeCrouch * 4.0f;
    }

    if (ctx.onLadder) {
        return vdata.m_flRecoveryTimeStand;
    }

    float baseRecovery = ctx.ducking ? vdata.m_flRecoveryTimeCrouch : vdata.m_flRecoveryTimeStand;
    float finalRecovery = ctx.ducking ? vdata.m_flRecoveryTimeCrouchFinal : vdata.m_flRecoveryTimeStandFinal;

    if (finalRecovery != -1.0f && vdata.m_nRecoveryTransitionEndBullet > vdata.m_nRecoveryTransitionStartBullet) {
        float frac = static_cast<float>(recoilIndex - vdata.m_nRecoveryTransitionStartBullet) /
                     static_cast<float>(vdata.m_nRecoveryTransitionEndBullet - vdata.m_nRecoveryTransitionStartBullet);
        frac = ClampFloat(frac, 0.0f, 1.0f);
        return baseRecovery + frac * (finalRecovery - baseRecovery);
    }

    return baseRecovery;
}

// FUN_18080d4c0 @ RVA 0x0080d4c0
float CWeaponAccuracyPipeline::CalculateStrafingInaccuracy(
    const PlayerPawnAccuracyContext& ctx,
    const AccuracyConVars& cvars
) {
    if (!cvars.sv_strafing_inaccuracy_enabled) {
        return 0.0f;
    }

    float speedSqr = ctx.velocity.x * ctx.velocity.x +
                     ctx.velocity.y * ctx.velocity.y +
                     ctx.velocity.z * ctx.velocity.z;

    if (speedSqr < 0.001f) {
        return 0.0f;
    }

    float speed = std::sqrt(speedSqr);
    Vector3 velDir = {ctx.velocity.x / speed, ctx.velocity.y / speed, ctx.velocity.z / speed};

    Vector3 eyeForward;
    CViewRender::AngleVectors(ctx.eyeAngles, &eyeForward, nullptr, nullptr);

    float dot = DotProduct(eyeForward, velDir);
    float perpFrac = 1.0f - std::abs(dot);

    // Bias transformation function matching FUN_18173d3d0
    float biasedPerp = std::pow(ClampFloat(perpFrac, 0.0f, 1.0f), cvars.sv_strafing_inaccuracy_bias);

    return (speed / 250.0f) * biasedPerp * cvars.sv_strafing_inaccuracy_scale;
}

// FUN_180828d90 @ RVA 0x00828d90
void CWeaponAccuracyPipeline::UpdateTurningInaccuracy(
    WeaponAccuracyState& state,
    const QAngle& eyeAngles,
    [[maybe_unused]] float dt,
    const AccuracyConVars& cvars
) {
    if (!cvars.sv_turning_inaccuracy_enabled) {
        state.m_flTurningInaccuracyDelta = 0.0f;
        state.m_flTurningInaccuracy = 0.0f;
        return;
    }

    Vector3 currentDir;
    CViewRender::AngleVectors(eyeAngles, &currentDir, nullptr, nullptr);

    if (state.m_vecTurningInaccuracyEyeDirLast.x == 0.0f &&
        state.m_vecTurningInaccuracyEyeDirLast.y == 0.0f &&
        state.m_vecTurningInaccuracyEyeDirLast.z == 0.0f) {
        state.m_vecTurningInaccuracyEyeDirLast = currentDir;
        return;
    }

    float deltaDeg = 0.0f;
    float dot = DotProduct(currentDir, state.m_vecTurningInaccuracyEyeDirLast);
    dot = ClampFloat(dot, -1.0f, 1.0f);

    float angleRad = std::acos(dot);
    deltaDeg = std::abs(angleRad) * RAD2DEG;
    if (deltaDeg > 180.0f) deltaDeg = 180.0f;

    // Decay prior turning delta
    state.m_flTurningInaccuracyDelta *= cvars.sv_turning_inaccuracy_decay;
    if (state.m_flTurningInaccuracyDelta < 1e-5f) {
        state.m_flTurningInaccuracyDelta = 0.0f;
    }

    // Accumulate normalized delta beyond sv_turning_inaccuracy_angle_min
    float minAngle = cvars.sv_turning_inaccuracy_angle_min;
    if (minAngle < 180.0f && deltaDeg > minAngle) {
        float angleFrac = (deltaDeg - minAngle) / (180.0f - minAngle);
        angleFrac = ClampFloat(angleFrac, 0.0f, 1.0f);
        state.m_flTurningInaccuracyDelta += angleFrac;
    }

    state.m_flTurningInaccuracyDelta = ClampFloat(state.m_flTurningInaccuracyDelta, 0.0f, 1.0f);
    state.m_flTurningInaccuracy = state.m_flTurningInaccuracyDelta;
    state.m_vecTurningInaccuracyEyeDirLast = currentDir;
}

// FUN_180827be0 @ RVA 0x00827be0 / 0x00828a10
void CWeaponAccuracyPipeline::UpdateAccuracyPenalty(
    WeaponAccuracyState& state,
    const CCSWeaponBaseVData_Accuracy& vdata,
    const PlayerPawnAccuracyContext& ctx,
    float curTime,
    float dt,
    const AccuracyConVars& cvars
) {
    // 1. Determine target baseline inaccuracy for current posture
    float targetInaccuracy = 0.0f;

    if (ctx.onLadder) {
        targetInaccuracy = vdata.m_flInaccuracyLadder;
    } else if (!ctx.onGround) {
        targetInaccuracy = vdata.m_flInaccuracyStand + vdata.m_flInaccuracyJump;
    } else if (ctx.boostStackCount >= cvars.weapon_accuracy_stack_boost_limit && cvars.weapon_accuracy_stack_boost_limit > 0) {
        // Player stacking / pyramid boost penalty
        targetInaccuracy = vdata.m_flInaccuracyLadder * 2.0f;
    } else if (ctx.ducking) {
        targetInaccuracy = vdata.m_flInaccuracyCrouch;
    } else {
        targetInaccuracy = vdata.m_flInaccuracyStand;
    }

    if (state.m_bInReload) {
        targetInaccuracy += vdata.m_flInaccuracyReload;
    }

    // 2. Exponential recovery decay towards target
    if (state.m_fAccuracyPenalty >= targetInaccuracy) {
        float recoveryTime = GetRecoveryTime(vdata, ctx, state.m_iRecoilIndex);
        if (recoveryTime > 0.0001f) {
            // Valve formula: exp(-(ln(10.0f) / recoveryTime) * dt)
            float decayFactor = std::exp(-(std::log(10.0f) / recoveryTime) * dt);
            state.m_fAccuracyPenalty = targetInaccuracy + (state.m_fAccuracyPenalty - targetInaccuracy) * decayFactor;
        } else {
            state.m_fAccuracyPenalty = targetInaccuracy;
        }
    } else {
        state.m_fAccuracyPenalty = targetInaccuracy;
    }

    // 3. Update view turning inaccuracy
    UpdateTurningInaccuracy(state, ctx.eyeAngles, dt, cvars);

    // 4. Continuous rapid-fire recoil index decay (FUN_180827be0 lines 88-120)
    if (state.m_flRecoilIndex > 0.0f) {
        float timeSinceShot = curTime - state.m_fLastShotTime;
        if (timeSinceShot > (vdata.m_flCycleTime + dt)) {
            // Valve formula: exp(-2.0f * ln(10.0f) * dt)
            float recoilDecay = std::exp(-2.0f * std::log(10.0f) * dt);
            state.m_flRecoilIndex *= recoilDecay;
            if (state.m_flRecoilIndex <= 0.1f) {
                state.m_flRecoilIndex = 0.0f;
                state.m_iRecoilIndex = 0;
            }
        }
    }

    state.m_flLastAccuracyUpdateTime = curTime;
}

// FUN_18080c1a0 @ RVA 0x0080c1a0 / 0x0080cfd0
float CWeaponAccuracyPipeline::GetInaccuracy(
    const WeaponAccuracyState& state,
    const CCSWeaponBaseVData_Accuracy& vdata,
    const PlayerPawnAccuracyContext& ctx,
    float* outSpread,
    float* outJumpInacc,
    const AccuracyConVars& cvars
) {
    if (cvars.weapon_accuracy_forcespread > 0.0f) {
        if (outSpread) *outSpread = cvars.weapon_accuracy_forcespread;
        if (outJumpInacc) *outJumpInacc = 0.0f;
        return cvars.weapon_accuracy_forcespread;
    }

    if (cvars.weapon_accuracy_nospread) {
        if (outSpread) *outSpread = 0.0f;
        if (outJumpInacc) *outJumpInacc = 0.0f;
        return 0.0f;
    }

    float totalInaccuracy = state.m_fAccuracyPenalty;

    // Movement speed modulation: RemapValClamped(speed, maxSpeed * 0.34, maxSpeed * 0.95, 0.0, 1.0)
    float speed2D = std::sqrt(ctx.velocity.x * ctx.velocity.x + ctx.velocity.y * ctx.velocity.y);
    float lowSpeed = vdata.m_flMaxSpeed * 0.34f;
    float highSpeed = vdata.m_flMaxSpeed * 0.95f;

    float moveFactor = 0.0f;
    if (speed2D > lowSpeed) {
        moveFactor = (speed2D - lowSpeed) / (highSpeed - lowSpeed);
        moveFactor = ClampFloat(moveFactor, 0.0f, 1.0f);
        if (ctx.ducking) {
            // Valve pow(f, 0.25f) when crouched
            moveFactor = std::pow(moveFactor, 0.25f);
        }
    }

    float moveInaccuracy = moveFactor * vdata.m_flInaccuracyMove;
    if (outSpread) *outSpread = moveInaccuracy;
    totalInaccuracy += moveInaccuracy;

    // Jumping / Airborne Inaccuracy
    float jumpInacc = 0.0f;
    if (!ctx.onGround) {
        float vz = std::abs(ctx.velocity.z);
        float apexThreshold = 200.0f; // Approx apex vertical speed band
        float jumpApex = vdata.m_flInaccuracyJumpApex;
        float jumpInitial = vdata.m_flInaccuracyJumpInitial;

        if (vz <= apexThreshold) {
            jumpInacc = jumpApex;
        } else {
            float frac = (vz - apexThreshold) / (800.0f - apexThreshold);
            frac = ClampFloat(frac, 0.0f, 1.0f);
            jumpInacc = jumpApex + frac * (jumpInitial - jumpApex);
        }
        totalInaccuracy += jumpInacc;
    }
    if (outJumpInacc) *outJumpInacc = jumpInacc;

    // Strafing component
    totalInaccuracy += CalculateStrafingInaccuracy(ctx, cvars);

    // Turning component
    totalInaccuracy += state.m_flTurningInaccuracy;

    return ClampFloat(totalInaccuracy, 0.0f, 1.0f);
}

float CWeaponAccuracyPipeline::GetSpread(
    const CCSWeaponBaseVData_Accuracy& vdata,
    int32_t /*weaponMode*/
) {
    return vdata.m_flSpread;
}

// FUN_180d23fe0 @ RVA 0x00d23fe0 / 0x00d25630 (FX_CalculateSpread)
SpreadVector2D CWeaponAccuracyPipeline::CalculateSpreadVector(
    uint16_t itemDefIndex,
    int32_t /*bulletIndex*/,
    int32_t /*numBullets*/,
    int32_t weaponMode,
    int32_t randomSeed,
    float inaccuracy,
    float spread,
    float recoilIndex,
    const AccuracyConVars& cvars
) {
    if (cvars.weapon_accuracy_nospread) {
        return {0.0f, 0.0f};
    }

    CUniformRandomStream rng;
    // Valve PRNG seed initialization matching RandomSeed(param_4)
    rng.SetSeed(randomSeed);

    // Dual uniform disk sampling
    // -----------------------------------------------------------------------------------
    // Disk 1: Inaccuracy Disk (r1 in [0, 1], theta1 in [0, 2*PI])
    // -----------------------------------------------------------------------------------
    float r1 = rng.RandomFloat(0.0f, 1.0f);
    float theta1 = rng.RandomFloat(0.0f, 2.0f * PI);

    // Revolver secondary fire (fan-the-hammer: item def 64, weaponMode 1)
    constexpr uint16_t WEAPON_REVOLVER = 64;
    constexpr uint16_t WEAPON_NEGEV    = 28;

    if (itemDefIndex == WEAPON_REVOLVER && weaponMode == 1) {
        // Power distribution pushes shots outwards towards cone perimeter
        r1 = 1.0f - r1 * r1;
    } else if (itemDefIndex == WEAPON_NEGEV && recoilIndex < 3.0f) {
        // Negev initial spin-up inaccuracy power curve: collapses towards laser precision
        for (int32_t i = 3; i > static_cast<int32_t>(recoilIndex); --i) {
            r1 = r1 * r1;
        }
        r1 = 1.0f - r1;
    }

    if (cvars.weapon_accuracy_forcespread > 0.0f) {
        r1 = 1.0f;
    }

    float R_inacc = r1 * inaccuracy;

    // -----------------------------------------------------------------------------------
    // Disk 2: Intrinsic Spread Disk (r2 in [0, 1], theta2 in [0, 2*PI])
    // -----------------------------------------------------------------------------------
    float r2 = rng.RandomFloat(0.0f, 1.0f);
    float theta2 = rng.RandomFloat(0.0f, 2.0f * PI);

    if (itemDefIndex == WEAPON_REVOLVER && weaponMode == 1) {
        r2 = 1.0f - r2 * r2;
    } else if (itemDefIndex == WEAPON_NEGEV && recoilIndex < 3.0f) {
        for (int32_t i = 3; i > static_cast<int32_t>(recoilIndex); --i) {
            r2 = r2 * r2;
        }
        r2 = 1.0f - r2;
    }

    if (cvars.weapon_accuracy_forcespread > 0.0f) {
        r2 = 1.0f;
    }

    float R_spread = r2 * spread;

    // -----------------------------------------------------------------------------------
    // Vectorial combination matching FUN_180d23fe0 lines 128-137:
    // dx = cos(theta1) * R_inacc + cos(theta2) * R_spread
    // dy = sin(theta1) * R_inacc + sin(theta2) * R_spread
    // -----------------------------------------------------------------------------------
    SpreadVector2D result{};
    result.x = std::cos(theta1) * R_inacc + std::cos(theta2) * R_spread;
    result.y = std::sin(theta1) * R_inacc + std::sin(theta2) * R_spread;

    return result;
}

void CWeaponAccuracyPipeline::OnWeaponFired(
    WeaponAccuracyState& state,
    const CCSWeaponBaseVData_Accuracy& vdata,
    AimPunchState& punchState,
    float curTime
) {
    // 1. Fire Inaccuracy impulse
    state.m_fAccuracyPenalty += vdata.m_flInaccuracyFire;
    state.m_flRecoilIndex += 1.0f;
    state.m_iRecoilIndex += 1;
    state.m_fLastShotTime = curTime;

    // 2. Recoil punch kick impulse (pitch kicks upward, yaw has directional spread)
    float punchPitch = vdata.m_flRecoilMagnitude * 0.055f;
    punchState.m_aimPunchAngle.pitch -= punchPitch;
    punchState.m_aimPunchAngleVel.pitch -= punchPitch * 20.0f;
}

// FUN_18085dc60 @ RVA 0x0085dc60 & FUN_18084d8b0 @ RVA 0x0084d8b0
void CWeaponAccuracyPipeline::SimulateAimPunchDecay(
    AimPunchState& punchState,
    float dt,
    float dampingRate
) {
    // Second-order spring integration with exponential velocity damping (-4.5f rate)
    float velDamping = std::exp(-dampingRate * dt);

    punchState.m_aimPunchAngleVel.pitch *= velDamping;
    punchState.m_aimPunchAngleVel.yaw   *= velDamping;
    punchState.m_aimPunchAngleVel.roll  *= velDamping;

    // Integrate angle from velocity
    punchState.m_aimPunchAngle.pitch += punchState.m_aimPunchAngleVel.pitch * dt;
    punchState.m_aimPunchAngle.yaw   += punchState.m_aimPunchAngleVel.yaw * dt;
    punchState.m_aimPunchAngle.roll  += punchState.m_aimPunchAngleVel.roll * dt;

    // Deadzone filter: 0.140625 threshold (0.375^2)
    float magSqr = punchState.m_aimPunchAngle.pitch * punchState.m_aimPunchAngle.pitch +
                   punchState.m_aimPunchAngle.yaw   * punchState.m_aimPunchAngle.yaw +
                   punchState.m_aimPunchAngle.roll  * punchState.m_aimPunchAngle.roll;

    if (magSqr <= 0.140625f) {
        punchState.m_aimPunchAngle = {0.0f, 0.0f, 0.0f};
        punchState.m_aimPunchAngleVel = {0.0f, 0.0f, 0.0f};
    } else {
        float scale = 1.0f - (0.140625f / magSqr);
        punchState.m_aimPunchAngle.pitch *= scale;
        punchState.m_aimPunchAngle.yaw   *= scale;
        punchState.m_aimPunchAngle.roll  *= scale;
    }

    // Clamp pitch to [-89.0f, +89.0f]
    punchState.m_aimPunchAngle.pitch = ClampFloat(punchState.m_aimPunchAngle.pitch, -89.0f, 89.0f);
}

Vector3 CWeaponAccuracyPipeline::CalculateSpreadConeRay(
    const SpreadVector2D& spreadVec,
    const Vector3& forward,
    const Vector3& right,
    const Vector3& up
) {
    Vector3 ray = {
        forward.x + right.x * spreadVec.x + up.x * spreadVec.y,
        forward.y + right.y * spreadVec.x + up.y * spreadVec.y,
        forward.z + right.z * spreadVec.x + up.z * spreadVec.y
    };

    return NormalizeVector(ray);
}

Vector3 CWeaponAccuracyPipeline::CalculateSpreadConeRay(
    float inaccuracy,
    float spread,
    float randU1,
    float randU2,
    const Vector3& forward,
    const Vector3& right,
    const Vector3& up
) {
    float r = (inaccuracy + spread) * std::sqrt(randU1);
    float theta = randU2 * 2.0f * PI;

    float spreadX = std::cos(theta) * r;
    float spreadY = std::sin(theta) * r;

    Vector3 ray = {
        forward.x + right.x * spreadX + up.x * spreadY,
        forward.y + right.y * spreadX + up.y * spreadY,
        forward.z + right.z * spreadX + up.z * spreadY
    };

    return NormalizeVector(ray);
}

} // namespace cs2::client
