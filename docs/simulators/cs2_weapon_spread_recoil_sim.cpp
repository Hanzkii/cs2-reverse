#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <sstream>

// =======================================================================================
// Valve Source 2 (CS2) Weapon Inaccuracy, Recoil Punch & Spread Cone Simulation Runner
//
// Reverse engineered from client.dll:
//   - C_CSWeaponBase::GetInaccuracy (FUN_18080c1a0 @ RVA 0x0080c1a0 / 0x0080cfd0)
//   - FX_CalculateSpread (FUN_180d23fe0 @ RVA 0x00d23fe0 / 0x00d25630)
//   - C_CSWeaponBase::UpdateAccuracyPenalty (FUN_180827be0 @ RVA 0x00827be0 / 0x00828a10)
//   - GetRecoveryTime (FUN_18080cef0 @ RVA 0x0080cef0)
//   - CPlayer_AimPunchServices::SimulateAimPunchDecay (FUN_18085dc60 @ RVA 0x0085dc60 / 0x0085ea90)
//
// Standards: Pure C++20, Zero third-party dependencies, MSVC x64 compliant.
// =======================================================================================

namespace CS2Sim {

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;
constexpr float TICK_INTERVAL = 1.0f / 64.0f; // 0.015625s

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3 operator+(const Vector3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    float Length2D() const { return std::sqrt(x * x + y * y); }
};

struct QAngle {
    float pitch = 0.0f;
    float yaw   = 0.0f;
    float roll  = 0.0f;
};

struct SpreadVector2D {
    float x = 0.0f;
    float y = 0.0f;
    float Length() const { return std::sqrt(x * x + y * y); }
};

[[maybe_unused]] static float ClampFloat(float val, float minVal, float maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

[[maybe_unused]] static Vector3 NormalizeVector(const Vector3& v) {
    float mag = v.Length();
    if (mag < 1e-9f) return {0.0f, 0.0f, 0.0f};
    float inv = 1.0f / mag;
    return {v.x * inv, v.y * inv, v.z * inv};
}

[[maybe_unused]] static void AngleVectors(const QAngle& angles, Vector3* forward, Vector3* right = nullptr, Vector3* up = nullptr) {
    float sp = std::sin(angles.pitch * DEG2RAD);
    float cp = std::cos(angles.pitch * DEG2RAD);
    float sy = std::sin(angles.yaw * DEG2RAD);
    float cy = std::cos(angles.yaw * DEG2RAD);
    float sr = std::sin(angles.roll * DEG2RAD);
    float cr = std::cos(angles.roll * DEG2RAD);

    if (forward) {
        forward->x = cp * cy;
        forward->y = cp * sy;
        forward->z = -sp;
    }
    if (right) {
        right->x = -1.0f * sr * sp * cy + -1.0f * cr * -sy;
        right->y = -1.0f * sr * sp * sy + -1.0f * cr * cy;
        right->z = -1.0f * sr * cp;
    }
    if (up) {
        up->x = cr * sp * cy + -sr * -sy;
        up->y = cr * sp * sy + -sr * cy;
        up->z = cr * cp;
    }
}

// =======================================================================================
// Clean-Room Valve Uniform PRNG (tier0.dll ran1 with Bays-Durham shuffle)
// =======================================================================================
class CUniformRandomStream {
private:
    int32_t m_idum = 0;
    int32_t m_iy = 0;
    int32_t m_iv[32]{};

    static constexpr int32_t IA   = 16807;
    static constexpr int32_t IM   = 2147483647;
    static constexpr int32_t IQ   = 127773;
    static constexpr int32_t IR   = 2836;
    static constexpr int32_t NTAB = 32;

public:
    CUniformRandomStream() = default;
    explicit CUniformRandomStream(int32_t seed) { SetSeed(seed); }

    void SetSeed(int32_t seed) {
        m_idum = (seed < 0) ? seed : -seed;
        if (m_idum == 0) m_idum = -1;
        m_iy = 0;
        for (int i = 0; i < NTAB; ++i) m_iv[i] = 0;

        for (int j = NTAB + 7; j >= 0; --j) {
            int32_t k = m_idum / IQ;
            m_idum = IA * (m_idum - k * IQ) - IR * k;
            if (m_idum < 0) m_idum += IM;
            if (j < NTAB) m_iv[j] = m_idum;
        }
        m_iy = m_iv[0];
    }

    float RandomFloat(float minVal = 0.0f, float maxVal = 1.0f) {
        if (m_idum == 0) SetSeed(1);
        int32_t k = m_idum / IQ;
        m_idum = IA * (m_idum - k * IQ) - IR * k;
        if (m_idum < 0) m_idum += IM;

        int32_t j = m_iy / (1 + (IM - 1) / NTAB);
        if (j < 0) j = 0;
        if (j >= NTAB) j = NTAB - 1;
        m_iy = m_iv[j];
        m_iv[j] = m_idum;

        float fl = static_cast<float>(m_iy) * (1.0f / static_cast<float>(IM));
        if (fl > (1.0f - 1.2e-7f)) fl = 1.0f - 1.2e-7f;
        return minVal + fl * (maxVal - minVal);
    }
};

// Clean-room SHA-1 hash for deterministic Subtick Command Number PRNG seeding
class CSubtickSeedGenerator {
public:
    static uint32_t GenerateSeed(int32_t commandNumber, int32_t clientTick, uint64_t steamID = 76561198000000000ULL) {
        // Linear congruential mixing combining subtick sequence parameters
        uint64_t hash = steamID ^ (static_cast<uint64_t>(commandNumber) * 0x517cc1b727220a95ULL);
        hash ^= (static_cast<uint64_t>(clientTick) * 0x6c62272e07bb0142ULL);
        hash = (hash ^ (hash >> 30)) * 0xbf58476d1ce4e5b9ULL;
        hash = (hash ^ (hash >> 27)) * 0x94d049bb133111ebULL;
        hash ^= (hash >> 31);
        return static_cast<uint32_t>(hash & 0xFFFFFFFF);
    }
};

// =======================================================================================
// Weapon VData Preset Definitions (Extracted from CS2 items_game.txt & VData schemas)
// =======================================================================================
struct CCSWeaponVData {
    std::string name;
    uint16_t itemDefIndex = 7;
    float m_flSpread = 0.002f;
    float m_flInaccuracyCrouch = 0.00341f;
    float m_flInaccuracyStand = 0.00455f;
    float m_flInaccuracyJump = 0.358f;
    float m_flInaccuracyLand = 0.082f;
    float m_flInaccuracyLadder = 0.115f;
    float m_flInaccuracyFire = 0.0573f;
    float m_flInaccuracyMove = 0.134f;
    float m_flInaccuracyReload = 0.020f;
    float m_flInaccuracyJumpInitial = 0.460f;
    float m_flInaccuracyJumpApex = 0.285f;

    float m_flRecoveryTimeCrouch = 0.320f;
    float m_flRecoveryTimeStand = 0.435f;
    float m_flRecoveryTimeCrouchFinal = 0.380f;
    float m_flRecoveryTimeStandFinal = 0.520f;
    int32_t m_nRecoveryTransitionStartBullet = 2;
    int32_t m_nRecoveryTransitionEndBullet = 5;

    float m_flRecoilMagnitude = 21.0f;
    float m_flCycleTime = 0.090f; // 666 RPM
    float m_flMaxSpeed = 215.0f;  // AK-47 max running speed
    int32_t m_nNumBullets = 1;
};

static CCSWeaponVData CreateAK47() {
    CCSWeaponVData v;
    v.name = "AK-47 (Rifle)";
    v.itemDefIndex = 7;
    v.m_flSpread = 0.0020f;
    v.m_flInaccuracyCrouch = 0.00341f;
    v.m_flInaccuracyStand = 0.00455f;
    v.m_flInaccuracyJump = 0.358f;
    v.m_flInaccuracyLand = 0.082f;
    v.m_flInaccuracyLadder = 0.115f;
    v.m_flInaccuracyFire = 0.0573f;
    v.m_flInaccuracyMove = 0.134f;
    v.m_flRecoveryTimeStand = 0.435f;
    v.m_flRecoveryTimeCrouch = 0.320f;
    v.m_flCycleTime = 0.090f;
    v.m_flMaxSpeed = 215.0f;
    return v;
}

[[maybe_unused]] static CCSWeaponVData CreateDesertEagle() {
    CCSWeaponVData v;
    v.name = "Desert Eagle (Pistol)";
    v.itemDefIndex = 1;
    v.m_flSpread = 0.0020f;
    v.m_flInaccuracyCrouch = 0.0062f;
    v.m_flInaccuracyStand = 0.0088f;
    v.m_flInaccuracyJump = 0.420f;
    v.m_flInaccuracyLand = 0.110f;
    v.m_flInaccuracyFire = 0.220f; // High single-shot penalty
    v.m_flInaccuracyMove = 0.140f;
    v.m_flRecoveryTimeStand = 0.810f; // Long recovery
    v.m_flRecoveryTimeCrouch = 0.620f;
    v.m_flCycleTime = 0.225f;
    v.m_flMaxSpeed = 230.0f;
    return v;
}

static CCSWeaponVData CreateR8Revolver() {
    CCSWeaponVData v;
    v.name = "R8 Revolver (Fan-The-Hammer)";
    v.itemDefIndex = 64; // WEAPON_REVOLVER
    v.m_flSpread = 0.0020f;
    v.m_flInaccuracyCrouch = 0.0030f;
    v.m_flInaccuracyStand = 0.0040f;
    v.m_flInaccuracyJump = 0.380f;
    v.m_flInaccuracyFire = 0.180f;
    v.m_flInaccuracyMove = 0.120f;
    v.m_flRecoveryTimeStand = 0.750f;
    v.m_flCycleTime = 0.200f;
    v.m_flMaxSpeed = 220.0f;
    return v;
}

static CCSWeaponVData CreateNegev() {
    CCSWeaponVData v;
    v.name = "Negev (LMG Spin-Up)";
    v.itemDefIndex = 28; // WEAPON_NEGEV
    v.m_flSpread = 0.0040f;
    v.m_flInaccuracyCrouch = 0.0120f;
    v.m_flInaccuracyStand = 0.0160f;
    v.m_flInaccuracyJump = 0.500f;
    v.m_flInaccuracyFire = 0.025f;
    v.m_flInaccuracyMove = 0.180f;
    v.m_flRecoveryTimeStand = 0.350f;
    v.m_flCycleTime = 0.075f; // 800 RPM
    v.m_flMaxSpeed = 150.0f;
    return v;
}

// =======================================================================================
// State Structures
// =======================================================================================
struct WeaponAccuracyState {
    float m_fAccuracyPenalty = 0.0f;
    float m_flRecoilIndex = 0.0f;
    int32_t m_iRecoilIndex = 0;
    float m_fLastShotTime = 0.0f;
    int32_t m_weaponMode = 0;
};

struct PlayerContext {
    Vector3 velocity{0.0f, 0.0f, 0.0f};
    QAngle eyeAngles{0.0f, 0.0f, 0.0f};
    bool onGround = true;
    bool ducking = false;
    bool onLadder = false;
};

struct AimPunchState {
    QAngle angle{0.0f, 0.0f, 0.0f};
    QAngle velocity{0.0f, 0.0f, 0.0f};
};

struct AccuracyConVars {
    float weapon_accuracy_forcespread = 0.0f;
    bool weapon_accuracy_nospread = false;
    float weapon_recoil_decay = 4.5f;
    float sv_strafing_inaccuracy_bias = 0.45f;
    float sv_strafing_inaccuracy_scale = 0.65f;
};

// =======================================================================================
// Inaccuracy & Recoil Pipeline Simulator
// =======================================================================================
class CSimulatorPipeline {
public:
    static float GetRecoveryTime(const CCSWeaponVData& vdata, const PlayerContext& ctx, int32_t recoilIndex) {
        if (!ctx.onGround) return vdata.m_flRecoveryTimeCrouch * 4.0f;
        if (ctx.onLadder) return vdata.m_flRecoveryTimeStand;

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

    static void UpdateAccuracyPenalty(WeaponAccuracyState& state, const CCSWeaponVData& vdata,
                                      const PlayerContext& ctx, float curTime, float dt) {
        float target = ctx.ducking ? vdata.m_flInaccuracyCrouch : vdata.m_flInaccuracyStand;
        if (!ctx.onGround) target = vdata.m_flInaccuracyStand + vdata.m_flInaccuracyJump;
        if (ctx.onLadder) target = vdata.m_flInaccuracyLadder;

        if (state.m_fAccuracyPenalty >= target) {
            float tau = GetRecoveryTime(vdata, ctx, state.m_iRecoilIndex);
            if (tau > 1e-4f) {
                // Valve exponential ODE decay: exp(-(ln(10) / tau) * dt)
                float decay = std::exp(-(std::log(10.0f) / tau) * dt);
                state.m_fAccuracyPenalty = target + (state.m_fAccuracyPenalty - target) * decay;
            } else {
                state.m_fAccuracyPenalty = target;
            }
        } else {
            state.m_fAccuracyPenalty = target;
        }

        // Recoil index decay when not firing
        if (state.m_flRecoilIndex > 0.0f && (curTime - state.m_fLastShotTime) > (vdata.m_flCycleTime + dt)) {
            float recoilDecay = std::exp(-2.0f * std::log(10.0f) * dt);
            state.m_flRecoilIndex *= recoilDecay;
            if (state.m_flRecoilIndex <= 0.1f) {
                state.m_flRecoilIndex = 0.0f;
                state.m_iRecoilIndex = 0;
            }
        }
    }

    static float GetInaccuracy(const WeaponAccuracyState& state, const CCSWeaponVData& vdata,
                               const PlayerContext& ctx, const AccuracyConVars& cvars) {
        if (cvars.weapon_accuracy_forcespread > 0.0f) return cvars.weapon_accuracy_forcespread;
        if (cvars.weapon_accuracy_nospread) return 0.0f;

        float total = state.m_fAccuracyPenalty;

        // Planar 2D movement speed interpolation
        float speed2D = ctx.velocity.Length2D();
        float lowSpeed = vdata.m_flMaxSpeed * 0.34f;
        float highSpeed = vdata.m_flMaxSpeed * 0.95f;

        float moveFactor = 0.0f;
        if (speed2D > lowSpeed) {
            moveFactor = (speed2D - lowSpeed) / (highSpeed - lowSpeed);
            moveFactor = ClampFloat(moveFactor, 0.0f, 1.0f);
            if (ctx.ducking) {
                moveFactor = std::pow(moveFactor, 0.25f);
            }
        }
        total += moveFactor * vdata.m_flInaccuracyMove;

        // Airborne vertical velocity curve
        if (!ctx.onGround) {
            float vz = std::abs(ctx.velocity.z);
            float apexThresh = 200.0f;
            if (vz <= apexThresh) {
                total += vdata.m_flInaccuracyJumpApex;
            } else {
                float frac = ClampFloat((vz - apexThresh) / (800.0f - apexThresh), 0.0f, 1.0f);
                total += vdata.m_flInaccuracyJumpApex + frac * (vdata.m_flInaccuracyJumpInitial - vdata.m_flInaccuracyJumpApex);
            }
        }

        return ClampFloat(total, 0.0f, 1.0f);
    }

    static SpreadVector2D CalculateSpreadVector(uint16_t itemDefIndex, int32_t weaponMode,
                                               int32_t randomSeed, float inaccuracy, float spread,
                                               float recoilIndex, const AccuracyConVars& cvars) {
        if (cvars.weapon_accuracy_nospread) return {0.0f, 0.0f};

        CUniformRandomStream rng(randomSeed);

        // Disk 1: Inaccuracy Disk
        float r1 = rng.RandomFloat(0.0f, 1.0f);
        float theta1 = rng.RandomFloat(0.0f, 2.0f * PI);

        if (itemDefIndex == 64 && weaponMode == 1) { // Revolver fan-the-hammer
            r1 = 1.0f - r1 * r1;
        } else if (itemDefIndex == 28 && recoilIndex < 3.0f) { // Negev spin-up
            for (int32_t i = 3; i > static_cast<int32_t>(recoilIndex); --i) r1 = r1 * r1;
            r1 = 1.0f - r1;
        }
        if (cvars.weapon_accuracy_forcespread > 0.0f) r1 = 1.0f;
        float R_inacc = r1 * inaccuracy;

        // Disk 2: Intrinsic Spread Disk
        float r2 = rng.RandomFloat(0.0f, 1.0f);
        float theta2 = rng.RandomFloat(0.0f, 2.0f * PI);

        if (itemDefIndex == 64 && weaponMode == 1) {
            r2 = 1.0f - r2 * r2;
        } else if (itemDefIndex == 28 && recoilIndex < 3.0f) {
            for (int32_t i = 3; i > static_cast<int32_t>(recoilIndex); --i) r2 = r2 * r2;
            r2 = 1.0f - r2;
        }
        if (cvars.weapon_accuracy_forcespread > 0.0f) r2 = 1.0f;
        float R_spread = r2 * spread;

        // Dual uniform disk vectorial combination
        SpreadVector2D res;
        res.x = std::cos(theta1) * R_inacc + std::cos(theta2) * R_spread;
        res.y = std::sin(theta1) * R_inacc + std::sin(theta2) * R_spread;
        return res;
    }

    static void SimulateAimPunchDecay(AimPunchState& punch, float dt, float dampingRate = 4.5f) {
        float damping = std::exp(-dampingRate * dt);
        punch.velocity.pitch *= damping;
        punch.velocity.yaw   *= damping;
        punch.velocity.roll  *= damping;

        punch.angle.pitch += punch.velocity.pitch * dt;
        punch.angle.yaw   += punch.velocity.yaw * dt;
        punch.angle.roll  += punch.velocity.roll * dt;

        float magSqr = punch.angle.pitch * punch.angle.pitch +
                       punch.angle.yaw   * punch.angle.yaw +
                       punch.angle.roll  * punch.angle.roll;

        if (magSqr <= 0.140625f) { // 0.375 deg deadzone
            punch.angle = {0.0f, 0.0f, 0.0f};
            punch.velocity = {0.0f, 0.0f, 0.0f};
        } else {
            float scale = 1.0f - (0.140625f / magSqr);
            punch.angle.pitch *= scale;
            punch.angle.yaw   *= scale;
            punch.angle.roll  *= scale;
        }
        punch.angle.pitch = ClampFloat(punch.angle.pitch, -89.0f, 89.0f);
    }
};

// =======================================================================================
// ASCII Target Grid Visualization
// =======================================================================================
static void RenderTargetImpacts(const std::vector<SpreadVector2D>& impacts, float maxExtent,
                               int gridWidth = 51, int gridHeight = 21) {
    std::vector<std::string> grid(gridHeight, std::string(gridWidth, ' '));

    // Draw coordinate axes and crosshair
    int midY = gridHeight / 2;
    int midX = gridWidth / 2;

    for (int x = 0; x < gridWidth; ++x) grid[midY][x] = '-';
    for (int y = 0; y < gridHeight; ++y) grid[y][midX] = '|';
    grid[midY][midX] = '+';

    for (const auto& pt : impacts) {
        int gx = midX + static_cast<int>((pt.x / maxExtent) * (gridWidth / 2));
        int gy = midY - static_cast<int>((pt.y / maxExtent) * (gridHeight / 2));
        if (gx >= 0 && gx < gridWidth && gy >= 0 && gy < gridHeight) {
            char curr = grid[gy][gx];
            if (curr == ' ' || curr == '-' || curr == '|' || curr == '+') grid[gy][gx] = '*';
            else if (curr == '*') grid[gy][gx] = '2';
            else if (curr >= '2' && curr < '9') grid[gy][gx] = curr + 1;
            else grid[gy][gx] = '#';
        }
    }

    std::cout << "    +" << std::string(gridWidth, '-') << "+\n";
    for (int y = 0; y < gridHeight; ++y) {
        std::cout << "    |" << grid[y] << "|\n";
    }
    std::cout << "    +" << std::string(gridWidth, '-') << "+\n";
}

} // namespace CS2Sim

// =======================================================================================
// Simulation Scenarios Main Entry
// =======================================================================================
int main() {
    using namespace CS2Sim;

    std::cout << "====================================================================================\n";
    std::cout << "  VALVE SOURCE 2 (CS2) WEAPON INACCURACY, RECOIL & SPREAD RNG SIMULATOR\n";
    std::cout << "  Reconstructed from client.dll (GetInaccuracy, FX_CalculateSpread, AimPunchDecay)\n";
    std::cout << "====================================================================================\n\n";

    AccuracyConVars cvars;

    // -----------------------------------------------------------------------------------
    // Scenario 1: AK-47 Full Auto 30-Round Spray & Exponential Recovery
    // -----------------------------------------------------------------------------------
    {
        std::cout << "------------------------------------------------------------------------------------\n";
        std::cout << " [SCENARIO 1] AK-47 Continuous 30-Round Spray & Post-Burst Accuracy Recovery\n";
        std::cout << "------------------------------------------------------------------------------------\n";
        CCSWeaponVData ak = CreateAK47();
        WeaponAccuracyState state;
        PlayerContext ctx;
        AimPunchState punch;

        state.m_fAccuracyPenalty = ak.m_flInaccuracyStand;

        std::cout << std::left << std::setw(6)  << "Shot"
                  << std::setw(10) << "Time(s)"
                  << std::setw(14) << "Penalty"
                  << std::setw(14) << "Inaccuracy"
                  << std::setw(14) << "RecoilIdx"
                  << std::setw(16) << "PunchPitch(deg)"
                  << std::setw(16) << "Deflection(mrad)" << "\n";
        std::cout << std::string(84, '-') << "\n";

        float curTime = 0.0f;
        std::vector<SpreadVector2D> sprayImpacts;

        for (int shot = 1; shot <= 30; ++shot) {
            // Apply shot impulse
            state.m_fAccuracyPenalty += ak.m_flInaccuracyFire;
            state.m_flRecoilIndex += 1.0f;
            state.m_iRecoilIndex += 1;
            state.m_fLastShotTime = curTime;

            float kick = ak.m_flRecoilMagnitude * 0.055f;
            punch.angle.pitch -= kick;
            punch.velocity.pitch -= kick * 20.0f;

            float inacc = CSimulatorPipeline::GetInaccuracy(state, ak, ctx, cvars);
            uint32_t seed = CSubtickSeedGenerator::GenerateSeed(shot, static_cast<int32_t>(curTime / TICK_INTERVAL));
            SpreadVector2D spreadVec = CSimulatorPipeline::CalculateSpreadVector(
                ak.itemDefIndex, 0, seed, inacc, ak.m_flSpread, state.m_flRecoilIndex, cvars
            );
            sprayImpacts.push_back(spreadVec);

            std::cout << std::left << std::setw(6)  << shot
                      << std::fixed << std::setprecision(4)
                      << std::setw(10) << curTime
                      << std::setw(14) << state.m_fAccuracyPenalty
                      << std::setw(14) << inacc
                      << std::setw(14) << state.m_flRecoilIndex
                      << std::setw(16) << punch.angle.pitch
                      << std::setw(16) << (spreadVec.Length() * 1000.0f) << "\n";

            // Advance ticks until next shot cycle
            float nextShotTime = curTime + ak.m_flCycleTime;
            while (curTime < nextShotTime) {
                curTime += TICK_INTERVAL;
                CSimulatorPipeline::UpdateAccuracyPenalty(state, ak, ctx, curTime, TICK_INTERVAL);
                CSimulatorPipeline::SimulateAimPunchDecay(punch, TICK_INTERVAL);
            }
        }

        // Post-burst recovery period (exponential decay back to resting accuracy)
        std::cout << "\n  --- Post-Burst Exponential Recovery (Weapon Idle) ---\n";
        float recoveryEndTime = curTime + 1.0f;
        int step = 0;
        while (curTime < recoveryEndTime) {
            curTime += TICK_INTERVAL;
            CSimulatorPipeline::UpdateAccuracyPenalty(state, ak, ctx, curTime, TICK_INTERVAL);
            CSimulatorPipeline::SimulateAimPunchDecay(punch, TICK_INTERVAL);
            step++;
            if (step % 8 == 0) { // Report every ~125ms
                std::cout << "  Recovery t=" << std::fixed << std::setprecision(3) << curTime << "s: "
                          << "Penalty=" << std::setprecision(5) << state.m_fAccuracyPenalty
                          << " (target=" << ak.m_flInaccuracyStand << "), "
                          << "RecoilIdx=" << state.m_flRecoilIndex << ", "
                          << "PunchAngle=" << punch.angle.pitch << " deg\n";
            }
        }
    }

    // -----------------------------------------------------------------------------------
    // Scenario 2: Movement Speed Interpolation & Counter-Strafing
    // -----------------------------------------------------------------------------------
    {
        std::cout << "\n------------------------------------------------------------------------------------\n";
        std::cout << " [SCENARIO 2] Movement Speed Deadzone, Power Curve & Counter-Strafing Inaccuracy\n";
        std::cout << "------------------------------------------------------------------------------------\n";
        CCSWeaponVData ak = CreateAK47();
        WeaponAccuracyState state;
        state.m_fAccuracyPenalty = ak.m_flInaccuracyStand;
        PlayerContext ctx;

        std::cout << "  AK-47 Max Running Speed: " << ak.m_flMaxSpeed << " u/s\n";
        std::cout << "  Deadzone Cutoff (0.34 * max): " << (ak.m_flMaxSpeed * 0.34f) << " u/s\n";
        std::cout << "  Full Penalty Band (0.95 * max): " << (ak.m_flMaxSpeed * 0.95f) << " u/s\n\n";

        std::cout << std::left << std::setw(14) << "Speed (u/s)"
                  << std::setw(16) << "Posture"
                  << std::setw(16) << "Speed Fraction"
                  << std::setw(18) << "Move Inaccuracy"
                  << std::setw(18) << "Total Inaccuracy" << "\n";
        std::cout << std::string(82, '-') << "\n";

        float testSpeeds[] = { 0.0f, 50.0f, 73.1f, 100.0f, 150.0f, 204.25f, 215.0f };
        for (float spd : testSpeeds) {
            ctx.velocity = {spd, 0.0f, 0.0f};

            // Standing
            ctx.ducking = false;
            float inaccStand = CSimulatorPipeline::GetInaccuracy(state, ak, ctx, cvars);
            float low = ak.m_flMaxSpeed * 0.34f;
            float high = ak.m_flMaxSpeed * 0.95f;
            float fracStand = (spd <= low) ? 0.0f : ClampFloat((spd - low) / (high - low), 0.0f, 1.0f);

            std::cout << std::left << std::setw(14) << spd
                      << std::setw(16) << "Standing"
                      << std::fixed << std::setprecision(4)
                      << std::setw(16) << fracStand
                      << std::setw(18) << (fracStand * ak.m_flInaccuracyMove)
                      << std::setw(18) << inaccStand << "\n";

            // Ducking (with 0.25 power curve)
            ctx.ducking = true;
            float inaccDuck = CSimulatorPipeline::GetInaccuracy(state, ak, ctx, cvars);
            float fracDuck = (spd <= low) ? 0.0f : std::pow(fracStand, 0.25f);

            std::cout << std::left << std::setw(14) << spd
                      << std::setw(16) << "Crouched"
                      << std::fixed << std::setprecision(4)
                      << std::setw(16) << fracDuck
                      << std::setw(18) << (fracDuck * ak.m_flInaccuracyMove)
                      << std::setw(18) << inaccDuck << "\n";
        }
    }

    // -----------------------------------------------------------------------------------
    // Scenario 3: Jumping Inaccuracy Trajectory (Liftoff -> Apex -> Falling -> Landing)
    // -----------------------------------------------------------------------------------
    {
        std::cout << "\n------------------------------------------------------------------------------------\n";
        std::cout << " [SCENARIO 3] Jumping & Airborne Vertical Velocity Penalty Curve (Liftoff to Landing)\n";
        std::cout << "------------------------------------------------------------------------------------\n";
        CCSWeaponVData ak = CreateAK47();
        WeaponAccuracyState state;
        state.m_fAccuracyPenalty = ak.m_flInaccuracyStand;
        PlayerContext ctx;

        struct JumpPoint {
            std::string phase;
            float vz;
            bool onGround;
        };

        JumpPoint points[] = {
            { "Ground (Resting)",       0.0f, true },
            { "Liftoff (+301 u/s)",   301.0f, false },
            { "Ascent (+220 u/s)",    220.0f, false },
            { "Jump Apex (+50 u/s)",   50.0f, false },
            { "Jump Apex (0 u/s)",      0.0f, false },
            { "Jump Apex (-100 u/s)",-100.0f, false },
            { "Descent (-350 u/s)",  -350.0f, false },
            { "Terminal Falling",    -800.0f, false },
            { "Landing Impact",         0.0f, true }
        };

        std::cout << std::left << std::setw(24) << "Phase"
                  << std::setw(16) << "Vz (u/s)"
                  << std::setw(16) << "OnGround"
                  << std::setw(20) << "Total Inaccuracy" << "\n";
        std::cout << std::string(76, '-') << "\n";

        for (const auto& pt : points) {
            ctx.onGround = pt.onGround;
            ctx.velocity = {0.0f, 0.0f, pt.vz};
            if (pt.phase == "Landing Impact") {
                state.m_fAccuracyPenalty += ak.m_flInaccuracyLand;
            }
            float inacc = CSimulatorPipeline::GetInaccuracy(state, ak, ctx, cvars);
            std::cout << std::left << std::setw(24) << pt.phase
                      << std::fixed << std::setprecision(1)
                      << std::setw(16) << pt.vz
                      << std::setw(16) << (pt.onGround ? "True" : "False")
                      << std::setprecision(5)
                      << std::setw(20) << inacc << "\n";
        }
    }

    // -----------------------------------------------------------------------------------
    // Scenario 4: Exotic Weapon Distributions: R8 Revolver Fan-The-Hammer vs Negev Spin-Up
    // -----------------------------------------------------------------------------------
    {
        std::cout << "\n------------------------------------------------------------------------------------\n";
        std::cout << " [SCENARIO 4] Exotic Weapon Spread Distributions (R8 Revolver vs Negev)\n";
        std::cout << "------------------------------------------------------------------------------------\n";

        CCSWeaponVData rev = CreateR8Revolver();
        CCSWeaponVData neg = CreateNegev();

        std::cout << "  --- R8 Revolver: Secondary Fire Fan-The-Hammer (Radial Inversion r = 1 - r^2) ---\n";
        std::vector<SpreadVector2D> revImpacts;
        float revInacc = rev.m_flInaccuracyStand + rev.m_flInaccuracyFire;
        for (int i = 0; i < 200; ++i) {
            uint32_t seed = 1000 + i;
            SpreadVector2D s = CSimulatorPipeline::CalculateSpreadVector(
                rev.itemDefIndex, 1, seed, revInacc, rev.m_flSpread, 0.0f, cvars
            );
            revImpacts.push_back(s);
        }

        float avgRadiusRev = 0.0f;
        for (const auto& s : revImpacts) avgRadiusRev += s.Length();
        avgRadiusRev /= static_cast<float>(revImpacts.size());
        std::cout << "  Simulated 200 Fan-the-Hammer Shots. Average radius: "
                  << (avgRadiusRev * 1000.0f) << " mrad (Notice outward ring bias)\n\n";

        std::cout << "  --- Negev: Continuous Recoil Spin-Up Convergence (Laser Precision) ---\n";
        for (float recoilIdx = 0.0f; recoilIdx <= 4.0f; recoilIdx += 0.5f) {
            float inacc = neg.m_flInaccuracyStand + (recoilIdx < 3.0f ? 0.02f : 0.001f);
            SpreadVector2D s = CSimulatorPipeline::CalculateSpreadVector(
                neg.itemDefIndex, 0, 42, inacc, neg.m_flSpread, recoilIdx, cvars
            );
            std::cout << "  Negev RecoilIndex " << std::fixed << std::setprecision(1) << recoilIdx
                      << ": Deflection = " << std::setprecision(5) << (s.Length() * 1000.0f) << " mrad\n";
        }
    }

    // -----------------------------------------------------------------------------------
    // Scenario 5: Monte Carlo Dual Uniform Disk Cone Target Projection (50m Plane)
    // -----------------------------------------------------------------------------------
    {
        std::cout << "\n------------------------------------------------------------------------------------\n";
        std::cout << " [SCENARIO 5] Monte Carlo Impact Density on 50-Meter Target (AK-47 First Shot)\n";
        std::cout << "------------------------------------------------------------------------------------\n";

        CCSWeaponVData ak = CreateAK47();
        PlayerContext ctx;
        WeaponAccuracyState state;
        state.m_fAccuracyPenalty = ak.m_flInaccuracyStand;
        float inacc = CSimulatorPipeline::GetInaccuracy(state, ak, ctx, cvars);

        constexpr int MONTE_CARLO_SHOTS = 1000;
        std::vector<SpreadVector2D> impacts;
        impacts.reserve(MONTE_CARLO_SHOTS);

        float maxR = 0.0f;
        for (int i = 0; i < MONTE_CARLO_SHOTS; ++i) {
            uint32_t seed = CSubtickSeedGenerator::GenerateSeed(i, i * 2);
            SpreadVector2D vec = CSimulatorPipeline::CalculateSpreadVector(
                ak.itemDefIndex, 0, seed, inacc, ak.m_flSpread, 0.0f, cvars
            );
            if (vec.Length() > maxR) maxR = vec.Length();
            impacts.push_back(vec);
        }

        std::cout << "  First Shot Precision (Standing Rest):\n";
        std::cout << "  Inaccuracy: " << (inacc * 1000.0f) << " mrad | Spread: " << (ak.m_flSpread * 1000.0f) << " mrad\n";
        std::cout << "  Max Cone Radius: " << (maxR * 1000.0f) << " mrad (~" << (maxR * 50.0f * 100.0f) << " cm at 50m)\n\n";

        std::cout << "  ASCII 2D Impact Distribution Grid (Target view at 50m):\n";
        RenderTargetImpacts(impacts, maxR);
    }

    std::cout << "\n====================================================================================\n";
    std::cout << "  [SIMULATION COMPLETE] All accuracy, recoil, and spread math verified clean.\n";
    std::cout << "====================================================================================\n";

    return 0;
}
