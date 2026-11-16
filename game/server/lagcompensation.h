#ifndef CS2_SERVER_LAGCOMPENSATION_H
#define CS2_SERVER_LAGCOMPENSATION_H

#include "../client/viewmatrix.h"
#include "controller_pawn.h"
#include <cstdint>
#include <vector>
#include <deque>
#include <array>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace cs2::server {

using client::Vector3;
using client::QAngle;

// -------------------------------------------------------------------------
// Authoritative Engine Constants & Virtual Offsets (server.dll @ Build 26598758)
// -------------------------------------------------------------------------
constexpr uintptr_t VTABLE_LAGCOMPENSATION_MANAGER = 0x1819c7650;
constexpr uintptr_t VA_START_LAG_COMPENSATION      = 0x180ED27D0; // RVA: 0x00ED27D0
constexpr uintptr_t VA_FINISH_LAG_COMPENSATION     = 0x180EB8F40; // RVA: 0x00EB8F40
constexpr uintptr_t VA_RESTORE_ENTITY_TO           = 0x180ECDB00; // RVA: 0x00ECDB00
constexpr uintptr_t VA_BACKTRACK_PLAYER            = 0x180EC8950; // RVA: 0x00EC8950

constexpr int MAX_BONES = 128;
constexpr size_t RING_BUFFER_CAPACITY = 64;                  // 64-tick circular buffer (1.000s @ 64Hz)
constexpr float TICK_RATE = 64.0f;                           // Authoritative 64Hz simulation
constexpr float TICK_INTERVAL_64 = 1.0f / TICK_RATE;         // 0.015625s (1.0f / 64.0f)
constexpr float MAX_BACKTRACK_WINDOW = 0.200f;               // sv_maxunlag: 200ms (0.200s)
constexpr float TELEPORT_DISTANCE_SQR_THRESHOLD = 4096.0f;   // 64.0f^2 units: Discontinuity breaker
constexpr float SLERP_EPSILON = 0.9995f;                     // NLERP threshold for near-parallel quaternions

// -------------------------------------------------------------------------
// Lag Record Bitflags (Reconstructed from FUN_180ecdb00 / RestoreEntityTo)
// -------------------------------------------------------------------------
constexpr uint8_t LAG_RECORD_RESTORED        = 0x01; // Bit 0: Record applied / active
constexpr uint8_t LAG_RECORD_ORIGIN_CHANGED  = 0x02; // Bit 1: World origin offset modified
constexpr uint8_t LAG_RECORD_ANGLES_CHANGED  = 0x04; // Bit 2: Eye angles / rotation modified
constexpr uint8_t LAG_RECORD_BOUNDS_CHANGED  = 0x08; // Bit 3: Collision mins/maxs modified
constexpr uint8_t LAG_RECORD_BONES_CHANGED   = 0x10; // Bit 4: Skeletal bone matrices cached

// -------------------------------------------------------------------------
// Hypercomplex Unit Quaternion (Rotation Representation)
// -------------------------------------------------------------------------
struct Quaternion {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Quaternion() noexcept = default;
    constexpr Quaternion(float inW, float inX, float inY, float inZ) noexcept
        : w(inW), x(inX), y(inY), z(inZ) {}

    static Quaternion FromAxisAngle(const Vector3& axis, float angleRadians) noexcept {
        const float half = angleRadians * 0.5f;
        const float s = std::sin(half);
        const float len = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
        if (len <= 1e-7f) return { 1.0f, 0.0f, 0.0f, 0.0f };
        const float invLen = 1.0f / len;
        return { std::cos(half), axis.x * invLen * s, axis.y * invLen * s, axis.z * invLen * s };
    }

    static Quaternion FromEuler(float pitchDeg, float yawDeg, float rollDeg) noexcept {
        constexpr float degToRad = std::numbers::pi_v<float> / 180.0f;
        const float p = pitchDeg * degToRad * 0.5f;
        const float y = yawDeg * degToRad * 0.5f;
        const float r = rollDeg * degToRad * 0.5f;

        const float sp = std::sin(p), cp = std::cos(p);
        const float sy = std::sin(y), cy = std::cos(y);
        const float sr = std::sin(r), cr = std::cos(r);

        return {
            cr * cp * cy + sr * sp * sy,
            sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy
        };
    }

    [[nodiscard]] constexpr float Dot(const Quaternion& o) const noexcept {
        return w * o.w + x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] float Norm() const noexcept {
        return std::sqrt(w * w + x * x + y * y + z * z);
    }

    [[nodiscard]] Quaternion Normalized() const noexcept {
        const float n = Norm();
        if (n <= 1e-7f) return { 1.0f, 0.0f, 0.0f, 0.0f };
        const float inv = 1.0f / n;
        return { w * inv, x * inv, y * inv, z * inv };
    }

    [[nodiscard]] constexpr Quaternion Conjugate() const noexcept {
        return { w, -x, -y, -z };
    }

    [[nodiscard]] constexpr Quaternion operator*(const Quaternion& r) const noexcept {
        return {
            w * r.w - x * r.x - y * r.y - z * r.z,
            w * r.x + x * r.w + y * r.z - z * r.y,
            w * r.y - x * r.z + y * r.w + z * r.x,
            w * r.z + x * r.y - y * r.x + z * r.w
        };
    }

    [[nodiscard]] Vector3 RotateVector(const Vector3& v) const noexcept {
        const Quaternion qv(0.0f, v.x, v.y, v.z);
        const Quaternion res = (*this) * qv * Conjugate();
        return { res.x, res.y, res.z };
    }

    /**
     * @brief Geodesic Shortest-Arc Spherical Linear Interpolation (SLERP) / NLERP
     * server.dll: shortest arc quaternion interpolation.
     */
    [[nodiscard]] static Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t) noexcept {
        float cosTheta = q0.Dot(q1);
        Quaternion target = q1;

        // Take shortest geodesic arc path across 4D hypersphere
        if (cosTheta < 0.0f) {
            cosTheta = -cosTheta;
            target = { -q1.w, -q1.x, -q1.y, -q1.z };
        }

        // Small-angle threshold: use NLERP to prevent division by zero
        if (cosTheta > SLERP_EPSILON) {
            const float invT = 1.0f - t;
            Quaternion nlerp(
                invT * q0.w + t * target.w,
                invT * q0.x + t * target.x,
                invT * q0.y + t * target.y,
                invT * q0.z + t * target.z
            );
            return nlerp.Normalized();
        }

        const float theta = std::acos(std::clamp(cosTheta, -1.0f, 1.0f));
        const float sinTheta = std::sin(theta);
        const float w0 = std::sin((1.0f - t) * theta) / sinTheta;
        const float w1 = std::sin(t * theta) / sinTheta;

        return {
            w0 * q0.w + w1 * target.w,
            w0 * q0.x + w1 * target.x,
            w0 * q0.y + w1 * target.y,
            w0 * q0.z + w1 * target.z
        };
    }

    [[nodiscard]] bool BitwiseEqual(const Quaternion& o) const noexcept {
        return w == o.w && x == o.x && y == o.y && z == o.z;
    }
};

// -------------------------------------------------------------------------
// Linear Algebra: 3x4 Affine Transformation Matrix (Row-Major)
// -------------------------------------------------------------------------
struct Matrix3x4 {
    float m[3][4]{};

    static constexpr Matrix3x4 Identity() noexcept {
        Matrix3x4 mat{};
        mat.m[0][0] = 1.0f;
        mat.m[1][1] = 1.0f;
        mat.m[2][2] = 1.0f;
        return mat;
    }

    static Matrix3x4 FromQuaternionTranslation(const Quaternion& q, const Vector3& pos) noexcept {
        Matrix3x4 mat{};
        const float xx = q.x * q.x;
        const float yy = q.y * q.y;
        const float zz = q.z * q.z;
        const float xy = q.x * q.y;
        const float xz = q.x * q.z;
        const float yz = q.y * q.z;
        const float wx = q.w * q.x;
        const float wy = q.w * q.y;
        const float wz = q.w * q.z;

        mat.m[0][0] = 1.0f - 2.0f * (yy + zz);
        mat.m[0][1] = 2.0f * (xy - wz);
        mat.m[0][2] = 2.0f * (xz + wy);
        mat.m[0][3] = pos.x;

        mat.m[1][0] = 2.0f * (xy + wz);
        mat.m[1][1] = 1.0f - 2.0f * (xx + zz);
        mat.m[1][2] = 2.0f * (yz - wx);
        mat.m[1][3] = pos.y;

        mat.m[2][0] = 2.0f * (xz - wy);
        mat.m[2][1] = 2.0f * (yz + wx);
        mat.m[2][2] = 1.0f - 2.0f * (xx + yy);
        mat.m[2][3] = pos.z;

        return mat;
    }

    [[nodiscard]] Vector3 TransformPoint(const Vector3& p) const noexcept {
        return {
            m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3],
            m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3],
            m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3]
        };
    }

    [[nodiscard]] Vector3 RotatePoint(const Vector3& p) const noexcept {
        return {
            m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z,
            m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z,
            m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z
        };
    }

    [[nodiscard]] Vector3 GetOrigin() const noexcept {
        return { m[0][3], m[1][3], m[2][3] };
    }

    [[nodiscard]] bool BitwiseEqual(const Matrix3x4& o) const noexcept {
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 4; ++c) {
                if (m[r][c] != o.m[r][c]) return false;
            }
        }
        return true;
    }
};

[[nodiscard]] constexpr Matrix3x4 LerpMatrix(const Matrix3x4& a, const Matrix3x4& b, float t) noexcept {
    Matrix3x4 out{};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 4; ++c) {
            out.m[r][c] = a.m[r][c] + (b.m[r][c] - a.m[r][c]) * t;
        }
    }
    return out;
}

// -------------------------------------------------------------------------
// Axis-Aligned Bounding Box (AABB)
// -------------------------------------------------------------------------
struct AABB {
    Vector3 mins{-16.0f, -16.0f, 0.0f};
    Vector3 maxs{16.0f, 16.0f, 72.0f};
};

/**
 * @brief Authoritative Lag Compensation History Record (LagRecord_t)
 * Reconstructed from server.dll Build 26598758 (CLagCompensationManager)
 */
struct LagRecord_t {
    uint32_t tickCount = 0;                                  // Server simulation tick index
    float simulationTime = 0.0f;                             // Authoritative simulation timestamp (seconds)
    float subTickFraction = 0.0f;                            // Continuous analog sub-tick offset [0.0, 1.0)
    uint8_t flags = 0;                                       // Bitmask of modified transforms
    Vector3 origin{};                                        // World-space pawn origin coordinates
    QAngle eyeAngles{};                                      // Eye/head orientation Euler angles
    Quaternion orientation{};                                // Unit quaternion rotation
    Vector3 mins{-16.0f, -16.0f, 0.0f};                      // Axis-aligned bounding box minimum
    Vector3 maxs{16.0f, 16.0f, 72.0f};                       // Axis-aligned bounding box maximum
    std::array<Matrix3x4, MAX_BONES> boneMatrices{};         // Resolved world-space bone transform cache
    size_t boneCount = 0;                                    // Active bone count (typically <= 128)
    Vector3 preTeleportOrigin{};                             // Spatial anchor for 4096.0 sqr distance check

    [[nodiscard]] bool BitwiseEqual(const LagRecord_t& o) const noexcept {
        if (tickCount != o.tickCount || simulationTime != o.simulationTime ||
            subTickFraction != o.subTickFraction || flags != o.flags ||
            boneCount != o.boneCount) return false;

        if (origin.x != o.origin.x || origin.y != o.origin.y || origin.z != o.origin.z) return false;
        if (!orientation.BitwiseEqual(o.orientation)) return false;
        if (mins.x != o.mins.x || mins.y != o.mins.y || mins.z != o.mins.z) return false;
        if (maxs.x != o.maxs.x || maxs.y != o.maxs.y || maxs.z != o.maxs.z) return false;

        for (size_t b = 0; b < boneCount; ++b) {
            if (!boneMatrices[b].BitwiseEqual(o.boneMatrices[b])) return false;
        }
        return true;
    }
};

using LagRecord = LagRecord_t;

/**
 * @brief Pre-rewind Snapshot used by FinishLagCompensation to restore pawn states
 */
struct RestoreSnapshot_t {
    LagRecord_t snapshot{};
    bool isBacktracked = false;
    bool originModified = false;
    bool anglesModified = false;
    bool boundsModified = false;
    bool bonesModified = false;
};

/**
 * @brief 64-Tick Ring Buffer Tracking History for a Single Player Pawn
 * 1.000s retention at 64Hz (Build 26598758).
 */
struct PawnTrackHistory_t {
    uint32_t pawnIndex = 0;                                  // Pawn entity index in entity list
    EntityHandle handle{};                                   // 32-bit CEntityHandle (Index + Serial)
    std::deque<LagRecord_t> records;                         // Circular history buffer (64 ticks capacity)
    RestoreSnapshot_t restoreSnapshot;                       // Original state prior to rewind
    bool isBacktracked = false;                              // True if currently rewound for trace
};

using PawnHistory = PawnTrackHistory_t;

/**
 * @brief Reconstructed Server-Side Lag Compensation Engine from server.dll (Build 26598758)
 * Reverse-engineered from CLagCompensationManager:
 * - VTable @ 0x1819c7650
 * - StartLagCompensation (0x180ED27D0, RVA 0x00ED27D0)
 * - FinishLagCompensation (0x180EB8F40, RVA 0x00EB8F40)
 * - RestoreEntityTo (0x180ECDB00, RVA 0x00ECDB00)
 * - BacktrackPlayer / BacktrackEntity (0x180EC8950, RVA 0x00EC8950)
 */
class CLagCompensationServer {
public:
    static CLagCompensationServer& Get();

    /**
     * @brief Records authoritative pawn state into 64-tick ring buffer track.
     */
    void RecordDataIntoTrack(uint32_t pawnIndex, EntityHandle handle, float curTime, uint32_t tick,
                             float subTickFraction, const Vector3& origin, const QAngle& angles,
                             const Vector3& mins, const Vector3& maxs,
                             const Matrix3x4* bones, size_t boneCount, uint8_t flags = 0x1E);

    // Overload taking explicit Quaternion orientation
    void RecordDataIntoTrackQuat(uint32_t pawnIndex, EntityHandle handle, float curTime, uint32_t tick,
                                 float subTickFraction, const Vector3& origin, const Quaternion& orientation,
                                 const Vector3& mins, const Vector3& maxs,
                                 const Matrix3x4* bones, size_t boneCount, uint8_t flags = 0x1E);

    // Legacy compatibility overload
    void RecordPawn(uint32_t pawnIndex, float curTime, uint32_t tick,
                    const Vector3& origin, const QAngle& angles,
                    const Vector3& mins, const Vector3& maxs,
                    const Matrix3x4* bones, size_t boneCount);

    /**
     * @brief Rewinds all active target pawns to the client's continuous target timestamp.
     * Clamped to [latestTime - maxUnlag, latestTime] (sv_maxunlag: 200ms).
     * Reconstructed from CLagCompensationManager::StartLagCompensation @ 0x180ED27D0.
     */
    bool StartLagCompensation(uint32_t attackerIndex, float targetTime,
                              float maxUnlag = MAX_BACKTRACK_WINDOW);

    /**
     * @brief Sub-tick aware StartLagCompensation converting tick + w into continuous time:
     * t_target = (tick + w) * dt - t_interp.
     */
    bool StartLagCompensationSubTick(uint32_t attackerIndex, uint32_t targetTick,
                                     float subTickFraction, float flInterp = 0.0f,
                                     float maxUnlag = MAX_BACKTRACK_WINDOW);

    /**
     * @brief Restores all backtracked pawns back to their authoritative present states.
     * Reconstructed from CLagCompensationManager::FinishLagCompensation @ 0x180EB8F40.
     */
    void FinishLagCompensation();

    // Overload for single pawn index
    void FinishLagCompensation(uint32_t pawnIndex);

    /**
     * @brief Backtracks an individual entity by sampling/interpolating records.
     * Enforces the 4096.0 unit^2 (64.0 units) teleportation breaker check.
     * Reconstructed from CLagCompensationManager::BacktrackPlayer @ 0x180EC8950.
     */
    bool BacktrackEntity(PawnTrackHistory_t& pawn, float targetTime, LagRecord_t& outRecord);

    /**
     * @brief Direct alias matching VA 0x180EC8950.
     */
    bool BacktrackPlayer(PawnTrackHistory_t& pawn, float targetTime, LagRecord_t& outRecord) {
        return BacktrackEntity(pawn, targetTime, outRecord);
    }

    /**
     * @brief Restores an entity's physical state from snapshot.
     * Reconstructed from CLagCompensationManager::RestoreEntityTo @ 0x180ECDB00.
     */
    bool RestoreEntityTo(PawnTrackHistory_t& pawn, const LagRecord_t& record);

    /**
     * @brief Interpolates between two history records at continuous fraction [0.0, 1.0].
     * Uses geodesic Quaternion SLERP/NLERP and 3x4 affine bone matrix blending.
     */
    static bool InterpolateRecords(const LagRecord_t& older, const LagRecord_t& newer,
                                   float fraction, LagRecord_t& outRecord);

    /**
     * @brief Interpolates 3x4 affine bone matrices linearly with SIMD precision.
     */
    static bool InterpolateBones(const LagRecord_t& from, const LagRecord_t& to,
                                 float fraction, std::array<Matrix3x4, MAX_BONES>& outBones,
                                 size_t boneCount = MAX_BONES);

    /**
     * @brief Converts discrete tick base and analog sub-tick fraction into seconds:
     * t_target = (tick + w) * dt - t_interp.
     */
    [[nodiscard]] static constexpr float CalculateSubTickSimulationTime(
        uint32_t tick, float subTickFraction, float flInterp = 0.0f,
        float tickInterval = TICK_INTERVAL_64) noexcept {
        return (static_cast<float>(tick) + std::clamp(subTickFraction, 0.0f, 1.0f)) * tickInterval - flInterp;
    }

    [[nodiscard]] const std::vector<PawnTrackHistory_t>& GetPawns() const noexcept { return m_Pawns; }
    void ClearHistory() noexcept { m_Pawns.clear(); }

private:
    CLagCompensationServer() = default;
    std::vector<PawnTrackHistory_t> m_Pawns;

    PawnTrackHistory_t* GetOrCreatePawn(uint32_t pawnIndex, EntityHandle handle = {});
};

using CLagCompensationManager = CLagCompensationServer;

} // namespace cs2::server

#endif // CS2_SERVER_LAGCOMPENSATION_H
