/**
 * ============================================================================
 * Source 2 CS2 Authoritative Temporal Rollback & Lag Compensation Simulator
 * File: cs2_lag_compensation_sim.cpp
 * Standard: ISO C++20 (/std:c++20 /EHsc /O2 /W4)
 * Dependencies: C++ Standard Library Only (Zero 3rd-Party Dependencies)
 *
 * Description:
 *   High-fidelity simulation test harness validating the mathematical
 *   mechanics of server-side lag compensation (CLagCompensationManager) in
 *   Source 2 (server.dll Build 26598758).
 *
 * Capabilities Demonstrated:
 *   1. 64-tick circular history ring buffer per tracked entity (1.0s retention).
 *   2. Continuous sub-tick queries: T_target = (tick + w) * dt.
 *   3. Geodesic shortest-arc unit quaternion SLERP and small-angle NLERP.
 *   4. Linear origin Lerp and 3x4 affine bone matrix transformation blending.
 *   5. Ray-box (AABB / OBB) directional spatial intersection testing.
 *   6. 100% bit-exact pre-rewind snapshot caching and post-trace state restoration.
 *   7. Authoritative latency clamping (sv_maxunlag = 0.200s) and teleport breaker.
 * ============================================================================
 */

#include <iostream>
#include <vector>
#include <deque>
#include <array>
#include <cmath>
#include <numbers>
#include <iomanip>
#include <string>
#include <string_view>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>

namespace cs2::sim {

// ---------------------------------------------------------------------------
// Engine Constants (Reconstructed from server.dll Build 26598758)
// ---------------------------------------------------------------------------
constexpr int MAX_BONES = 128;
constexpr size_t RING_BUFFER_CAPACITY = 64;           // 64 discrete simulation ticks
constexpr float TICK_RATE = 64.0f;                    // Authoritative 64Hz simulation
constexpr float TICK_INTERVAL = 1.0f / TICK_RATE;     // 0.015625s per tick
constexpr float MAX_UNLAG_DEFAULT = 0.200f;           // sv_maxunlag: 200ms latency window
constexpr float TELEPORT_BREAKER_SQR = 4096.0f;       // 64.0^2 units: Discontinuity breaker
constexpr float SLERP_EPSILON = 0.9995f;              // NLERP threshold for near-parallel quaternions

// Bitflags for LagRecord_t component modification
constexpr uint8_t LAG_RECORD_RESTORED       = 0x01;
constexpr uint8_t LAG_RECORD_ORIGIN_CHANGED = 0x02;
constexpr uint8_t LAG_RECORD_ANGLES_CHANGED = 0x04;
constexpr uint8_t LAG_RECORD_BOUNDS_CHANGED = 0x08;
constexpr uint8_t LAG_RECORD_BONES_CHANGED  = 0x10;

// ---------------------------------------------------------------------------
// Linear Algebra: 3D Vector
// ---------------------------------------------------------------------------
struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vector3() noexcept = default;
    constexpr Vector3(float inX, float inY, float inZ) noexcept : x(inX), y(inY), z(inZ) {}

    [[nodiscard]] constexpr Vector3 operator+(const Vector3& o) const noexcept {
        return { x + o.x, y + o.y, z + o.z };
    }
    [[nodiscard]] constexpr Vector3 operator-(const Vector3& o) const noexcept {
        return { x - o.x, y - o.y, z - o.z };
    }
    [[nodiscard]] constexpr Vector3 operator*(float s) const noexcept {
        return { x * s, y * s, z * s };
    }
    [[nodiscard]] constexpr Vector3 operator/(float s) const noexcept {
        const float inv = 1.0f / s;
        return { x * inv, y * inv, z * inv };
    }

    [[nodiscard]] constexpr float Dot(const Vector3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }
    [[nodiscard]] constexpr Vector3 Cross(const Vector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
    [[nodiscard]] float LengthSqr() const noexcept {
        return x * x + y * y + z * z;
    }
    [[nodiscard]] float Length() const noexcept {
        return std::sqrt(LengthSqr());
    }
    [[nodiscard]] Vector3 Normalized() const noexcept {
        const float len = Length();
        if (len <= 1e-7f) return { 0.0f, 0.0f, 0.0f };
        return *this / len;
    }

    [[nodiscard]] bool BitwiseEqual(const Vector3& o) const noexcept {
        return x == o.x && y == o.y && z == o.z;
    }
};

[[nodiscard]] constexpr Vector3 Lerp(const Vector3& a, const Vector3& b, float t) noexcept {
    return a + (b - a) * t;
}

// ---------------------------------------------------------------------------
// Linear Algebra: Unit Quaternion (Hypercomplex Rotation)
// ---------------------------------------------------------------------------
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
        const Vector3 normAxis = axis.Normalized();
        return { std::cos(half), normAxis.x * s, normAxis.y * s, normAxis.z * s };
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

    [[nodiscard]] Quaternion Conjugate() const noexcept {
        return { w, -x, -y, -z };
    }

    [[nodiscard]] Quaternion operator*(const Quaternion& r) const noexcept {
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
     * @brief Geodesic Shortest-Arc Spherical Linear Interpolation (SLERP)
     * Matches the quaternion interpolation logic in animationsystem.dll & server.dll.
     */
    [[nodiscard]] static Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t) noexcept {
        float cosTheta = q0.Dot(q1);
        Quaternion target = q1;

        // Take shortest geodesic path across 4D hypersphere
        if (cosTheta < 0.0f) {
            cosTheta = -cosTheta;
            target = { -q1.w, -q1.x, -q1.y, -q1.z };
        }

        // Small-angle threshold: use NLERP to prevent numerical division by zero
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

// ---------------------------------------------------------------------------
// Linear Algebra: 3x4 Affine Transformation Matrix (Row-Major)
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Computational Geometry: Ray & Bounding Volumes (AABB / OBB)
// ---------------------------------------------------------------------------
struct Ray {
    Vector3 origin;
    Vector3 direction; // Normalized direction vector

    Ray(const Vector3& o, const Vector3& d) noexcept
        : origin(o), direction(d.Normalized()) {}

    [[nodiscard]] Vector3 PointAt(float t) const noexcept {
        return origin + direction * t;
    }
};

struct AABB {
    Vector3 mins;
    Vector3 maxs;

    constexpr AABB() noexcept
        : mins(-16.0f, -16.0f, 0.0f), maxs(16.0f, 16.0f, 72.0f) {}
    constexpr AABB(const Vector3& inMins, const Vector3& inMaxs) noexcept
        : mins(inMins), maxs(inMaxs) {}

    /**
     * @brief Kay-Kajiya / Smits Fast Slab Intersection Test
     * Returns true if ray intersects volume, computing tNear and tFar.
     */
    [[nodiscard]] bool IntersectRay(const Ray& ray, float& outNear, float& outFar) const noexcept {
        float t0 = -1e20f;
        float t1 = 1e20f;

        const float rayDir[3] = { ray.direction.x, ray.direction.y, ray.direction.z };
        const float rayOrig[3] = { ray.origin.x, ray.origin.y, ray.origin.z };
        const float boxMin[3] = { mins.x, mins.y, mins.z };
        const float boxMax[3] = { maxs.x, maxs.y, maxs.z };

        for (int i = 0; i < 3; ++i) {
            if (std::abs(rayDir[i]) < 1e-7f) {
                // Ray parallel to slab; check if origin is outside
                if (rayOrig[i] < boxMin[i] || rayOrig[i] > boxMax[i]) {
                    return false;
                }
            } else {
                const float invDir = 1.0f / rayDir[i];
                float nearI = (boxMin[i] - rayOrig[i]) * invDir;
                float farI = (boxMax[i] - rayOrig[i]) * invDir;
                if (nearI > farI) std::swap(nearI, farI);

                t0 = std::max(t0, nearI);
                t1 = std::min(t1, farI);
                if (t0 > t1) return false;
            }
        }

        if (t1 < 0.0f) return false; // Box is behind ray
        outNear = (t0 < 0.0f) ? 0.0f : t0;
        outFar = t1;
        return true;
    }
};

struct OBB {
    AABB localBox;
    Matrix3x4 worldTransform; // World-space coordinate frame

    OBB(const AABB& box, const Matrix3x4& transform) noexcept
        : localBox(box), worldTransform(transform) {}

    /**
     * @brief Ray-OBB Intersection via Inverse Rigid Transform
     */
    [[nodiscard]] bool IntersectRay(const Ray& ray, float& outNear, float& outFar) const noexcept {
        // Compute inverse rotation and translation (rigid body: R^T)
        const Vector3 p = ray.origin - worldTransform.GetOrigin();
        const Vector3 localRayOrigin(
            worldTransform.m[0][0] * p.x + worldTransform.m[1][0] * p.y + worldTransform.m[2][0] * p.z,
            worldTransform.m[0][1] * p.x + worldTransform.m[1][1] * p.y + worldTransform.m[2][1] * p.z,
            worldTransform.m[0][2] * p.x + worldTransform.m[1][2] * p.y + worldTransform.m[2][2] * p.z
        );

        const Vector3 localRayDir(
            worldTransform.m[0][0] * ray.direction.x + worldTransform.m[1][0] * ray.direction.y + worldTransform.m[2][0] * ray.direction.z,
            worldTransform.m[0][1] * ray.direction.x + worldTransform.m[1][1] * ray.direction.y + worldTransform.m[2][1] * ray.direction.z,
            worldTransform.m[0][2] * ray.direction.x + worldTransform.m[1][2] * ray.direction.y + worldTransform.m[2][2] * ray.direction.z
        );

        const Ray localRay(localRayOrigin, localRayDir);
        return localBox.IntersectRay(localRay, outNear, outFar);
    }
};

// ---------------------------------------------------------------------------
// Authoritative History Record (LagRecord_t)
// ---------------------------------------------------------------------------
struct LagRecord_t {
    uint32_t tickCount = 0;                                  // Discrete simulation tick
    float simulationTime = 0.0f;                             // Authoritative simulation seconds
    float subTickFraction = 0.0f;                            // Continuous fraction w in [0, 1)
    uint8_t flags = 0;                                       // Bitmask of modified transforms
    Vector3 origin{};                                        // World spatial origin
    Quaternion orientation{};                                // Hyperspherical rotation
    AABB bounds{};                                           // Axis-aligned collision bounds
    std::array<Matrix3x4, MAX_BONES> boneMatrices{};         // Resolved skeletal hierarchy
    size_t boneCount = 0;                                    // Active joint count (<= 128)
    Vector3 preTeleportOrigin{};                             // Reference anchor for breaker check

    [[nodiscard]] bool BitwiseEqual(const LagRecord_t& o) const noexcept {
        if (tickCount != o.tickCount || simulationTime != o.simulationTime ||
            subTickFraction != o.subTickFraction || flags != o.flags ||
            boneCount != o.boneCount) return false;

        if (!origin.BitwiseEqual(o.origin) || !orientation.BitwiseEqual(o.orientation)) return false;
        if (!bounds.mins.BitwiseEqual(o.bounds.mins) || !bounds.maxs.BitwiseEqual(o.bounds.maxs)) return false;

        for (size_t b = 0; b < boneCount; ++b) {
            if (!boneMatrices[b].BitwiseEqual(o.boneMatrices[b])) return false;
        }
        return true;
    }
};

struct RestoreSnapshot_t {
    LagRecord_t snapshot{};
    bool isBacktracked = false;
};

// ---------------------------------------------------------------------------
// Tracked Entity State Representation
// ---------------------------------------------------------------------------
struct TrackedPawn_t {
    uint32_t pawnIndex = 0;
    uint32_t entityHandle = 0; // 32-bit handle: Index (handle & 0x7FFF), Serial (handle >> 15)

    // Current Authoritative Active Simulation State
    Vector3 activeOrigin{};
    Quaternion activeOrientation{};
    AABB activeBounds{};
    std::array<Matrix3x4, MAX_BONES> activeBones{};
    size_t activeBoneCount = 0;

    // Temporal History Ring Buffer (Capacity: 64 ticks = 1.0s)
    std::deque<LagRecord_t> ringBuffer;
    RestoreSnapshot_t restoreSnapshot;
    bool isBacktracked = false;
};

// ---------------------------------------------------------------------------
// Authoritative Temporal Rollback Engine (CLagCompensationManager)
// ---------------------------------------------------------------------------
class CLagCompensationManager {
public:
    CLagCompensationManager() = default;

    /**
     * @brief Ingests authoritative simulation state into 64-tick ring buffer.
     */
    void RecordPawnState(uint32_t pawnIndex, uint32_t handle, uint32_t tick,
                         float subTickFraction, const Vector3& origin,
                         const Quaternion& orientation, const AABB& bounds,
                         const Matrix3x4* bones, size_t boneCount)
    {
        TrackedPawn_t* pawn = GetOrCreatePawn(pawnIndex, handle);
        if (!pawn) return;

        // Update active current state
        pawn->activeOrigin = origin;
        pawn->activeOrientation = orientation;
        pawn->activeBounds = bounds;
        pawn->activeBoneCount = std::min(boneCount, static_cast<size_t>(MAX_BONES));
        for (size_t i = 0; i < pawn->activeBoneCount; ++i) {
            pawn->activeBones[i] = bones[i];
        }

        LagRecord_t record;
        record.tickCount = tick;
        record.simulationTime = (static_cast<float>(tick) + subTickFraction) * TICK_INTERVAL;
        record.subTickFraction = std::clamp(subTickFraction, 0.0f, 1.0f);
        record.flags = LAG_RECORD_ORIGIN_CHANGED | LAG_RECORD_ANGLES_CHANGED |
                       LAG_RECORD_BOUNDS_CHANGED | LAG_RECORD_BONES_CHANGED;
        record.origin = origin;
        record.orientation = orientation;
        record.bounds = bounds;
        record.boneCount = pawn->activeBoneCount;
        for (size_t i = 0; i < pawn->activeBoneCount; ++i) {
            record.boneMatrices[i] = bones[i];
        }

        // Anchor previous origin for teleport breaker check (FUN_180ecdb00)
        if (!pawn->ringBuffer.empty()) {
            record.preTeleportOrigin = pawn->ringBuffer.front().origin;
        } else {
            record.preTeleportOrigin = origin;
        }

        pawn->ringBuffer.push_front(record);

        // Maintain fixed 64-tick circular capacity
        while (pawn->ringBuffer.size() > RING_BUFFER_CAPACITY) {
            pawn->ringBuffer.pop_back();
        }
    }

    /**
     * @brief StartLagCompensation: Rewinds candidate entities to continuous target timestamp.
     * Enforces sv_maxunlag (200ms) clamping and caches RestoreSnapshot_t.
     */
    bool StartLagCompensation(uint32_t attackerIndex, float targetTime,
                              float maxUnlag = MAX_UNLAG_DEFAULT)
    {
        if (m_Pawns.empty()) return false;

        // Baseline reference time: newest record across all entities
        float latestTime = 0.0f;
        for (const auto& p : m_Pawns) {
            if (!p.ringBuffer.empty() && p.ringBuffer.front().simulationTime > latestTime) {
                latestTime = p.ringBuffer.front().simulationTime;
            }
        }

        // Anti-exploitation latency clamp: [latestTime - maxUnlag, latestTime]
        const float minAllowedTime = latestTime - maxUnlag;
        const float clampedTarget = std::clamp(targetTime, minAllowedTime, latestTime);

        bool anyBacktracked = false;

        for (auto& pawn : m_Pawns) {
            if (pawn.pawnIndex == attackerIndex || pawn.ringBuffer.empty()) continue;
            if (pawn.isBacktracked) continue;

            // 1. Snapshot Authoritative Current State for 100% Bit-Exact Restoration
            pawn.restoreSnapshot.snapshot.tickCount = pawn.ringBuffer.front().tickCount;
            pawn.restoreSnapshot.snapshot.simulationTime = pawn.ringBuffer.front().simulationTime;
            pawn.restoreSnapshot.snapshot.subTickFraction = pawn.ringBuffer.front().subTickFraction;
            pawn.restoreSnapshot.snapshot.flags = 0x1E;
            pawn.restoreSnapshot.snapshot.origin = pawn.activeOrigin;
            pawn.restoreSnapshot.snapshot.orientation = pawn.activeOrientation;
            pawn.restoreSnapshot.snapshot.bounds = pawn.activeBounds;
            pawn.restoreSnapshot.snapshot.boneCount = pawn.activeBoneCount;
            pawn.restoreSnapshot.snapshot.boneMatrices = pawn.activeBones;
            pawn.restoreSnapshot.isBacktracked = true;

            // 2. Perform Sub-Tick Fractional History Rewind
            LagRecord_t backtrackedRecord;
            if (BacktrackEntity(pawn, clampedTarget, backtrackedRecord)) {
                // Apply historic backtracked transforms to active entity components
                pawn.activeOrigin = backtrackedRecord.origin;
                pawn.activeOrientation = backtrackedRecord.orientation;
                pawn.activeBounds = backtrackedRecord.bounds;
                pawn.activeBones = backtrackedRecord.boneMatrices;
                pawn.activeBoneCount = backtrackedRecord.boneCount;

                pawn.isBacktracked = true;
                anyBacktracked = true;
            }
        }

        m_bLagCompensationActive = anyBacktracked;
        return anyBacktracked;
    }

    /**
     * @brief Sub-tick query entrypoint: T_target = (tick + w) * dt.
     */
    bool StartLagCompensationSubTick(uint32_t attackerIndex, uint32_t targetTick,
                                     float subTickFraction, float maxUnlag = MAX_UNLAG_DEFAULT)
    {
        const float targetTime = (static_cast<float>(targetTick) +
                                  std::clamp(subTickFraction, 0.0f, 1.0f)) * TICK_INTERVAL;
        return StartLagCompensation(attackerIndex, targetTime, maxUnlag);
    }

    /**
     * @brief FinishLagCompensation: Restores all backtracked entities to pre-rewind state.
     */
    void FinishLagCompensation() {
        for (auto& pawn : m_Pawns) {
            if (pawn.isBacktracked && pawn.restoreSnapshot.isBacktracked) {
                RestoreEntityTo(pawn, pawn.restoreSnapshot.snapshot);
                pawn.isBacktracked = false;
                pawn.restoreSnapshot.isBacktracked = false;
            }
        }
        m_bLagCompensationActive = false;
    }

    /**
     * @brief RestoreEntityTo: Reapplies cached snapshot components.
     */
    bool RestoreEntityTo(TrackedPawn_t& pawn, const LagRecord_t& record) {
        if (record.flags & LAG_RECORD_BOUNDS_CHANGED) {
            pawn.activeBounds = record.bounds;
        }
        if (record.flags & LAG_RECORD_ANGLES_CHANGED) {
            pawn.activeOrientation = record.orientation;
        }
        if (record.flags & LAG_RECORD_ORIGIN_CHANGED) {
            pawn.activeOrigin = record.origin;
        }
        if (record.flags & LAG_RECORD_BONES_CHANGED) {
            pawn.activeBoneCount = record.boneCount;
            pawn.activeBones = record.boneMatrices;
        }
        return true;
    }

    /**
     * @brief BacktrackEntity: Samples and interpolates historical ring buffer records.
     */
    bool BacktrackEntity(const TrackedPawn_t& pawn, float targetTime, LagRecord_t& outRecord) const {
        if (pawn.ringBuffer.empty()) return false;
        const auto& history = pawn.ringBuffer;

        // Extrapolate bounds clamps
        if (targetTime >= history.front().simulationTime) {
            outRecord = history.front();
            return true;
        }
        if (targetTime <= history.back().simulationTime) {
            outRecord = history.back();
            return true;
        }

        // Locate bracketing pair [older, newer] where older.time <= targetTime <= newer.time
        const LagRecord_t* newerRecord = nullptr;
        const LagRecord_t* olderRecord = nullptr;

        for (size_t i = 0; i < history.size() - 1; ++i) {
            if (history[i].simulationTime >= targetTime && history[i + 1].simulationTime <= targetTime) {
                newerRecord = &history[i];
                olderRecord = &history[i + 1];
                break;
            }
        }

        if (!newerRecord || !olderRecord) {
            outRecord = history.back();
            return true;
        }

        // Teleport Discontinuity Breaker (4096.0 sqr units = 64.0 units threshold)
        const float distSqr = (newerRecord->origin - olderRecord->origin).LengthSqr();
        if (distSqr >= TELEPORT_BREAKER_SQR) {
            // Discontinuity: snap to nearest discrete frame
            const float timeSpan = newerRecord->simulationTime - olderRecord->simulationTime;
            const float frac = (timeSpan > 1e-6f) ? (targetTime - olderRecord->simulationTime) / timeSpan : 0.0f;
            outRecord = (frac >= 0.5f) ? *newerRecord : *olderRecord;
            return true;
        }

        // Continuous fractional interpolation
        const float timeDelta = newerRecord->simulationTime - olderRecord->simulationTime;
        const float fraction = (timeDelta > 1e-6f)
            ? std::clamp((targetTime - olderRecord->simulationTime) / timeDelta, 0.0f, 1.0f)
            : 0.0f;

        return InterpolateRecords(*olderRecord, *newerRecord, fraction, outRecord);
    }

    /**
     * @brief InterpolateRecords: Synthesizes sub-tick continuous intermediate state.
     */
    static bool InterpolateRecords(const LagRecord_t& older, const LagRecord_t& newer,
                                   float fraction, LagRecord_t& outRecord)
    {
        fraction = std::clamp(fraction, 0.0f, 1.0f);

        outRecord.tickCount = (fraction >= 0.5f) ? newer.tickCount : older.tickCount;
        outRecord.simulationTime = older.simulationTime + (newer.simulationTime - older.simulationTime) * fraction;
        outRecord.subTickFraction = fraction;

        // Kinematic Origin Lerp: P(u) = P0 + (P1 - P0) * u
        outRecord.origin = Lerp(older.origin, newer.origin, fraction);

        // Rotation Geodesic Shortest-Arc SLERP
        outRecord.orientation = Quaternion::Slerp(older.orientation, newer.orientation, fraction);

        // AABB Collision Extents Interpolation
        outRecord.bounds.mins = Lerp(older.bounds.mins, newer.bounds.mins, fraction);
        outRecord.bounds.maxs = Lerp(older.bounds.maxs, newer.bounds.maxs, fraction);

        // Skeletal Hierarchy Linear Transformation Blending
        outRecord.boneCount = std::max(older.boneCount, newer.boneCount);
        for (size_t b = 0; b < outRecord.boneCount; ++b) {
            outRecord.boneMatrices[b] = LerpMatrix(older.boneMatrices[b], newer.boneMatrices[b], fraction);
        }

        outRecord.flags = older.flags | newer.flags;
        return true;
    }

    [[nodiscard]] TrackedPawn_t* GetPawn(uint32_t pawnIndex) noexcept {
        for (auto& p : m_Pawns) {
            if (p.pawnIndex == pawnIndex) return &p;
        }
        return nullptr;
    }

    [[nodiscard]] const std::vector<TrackedPawn_t>& GetPawns() const noexcept { return m_Pawns; }
    void ClearHistory() noexcept {
        m_Pawns.clear();
        m_bLagCompensationActive = false;
    }

private:
    std::vector<TrackedPawn_t> m_Pawns;
    bool m_bLagCompensationActive = false;

    TrackedPawn_t* GetOrCreatePawn(uint32_t pawnIndex, uint32_t handle) {
        for (auto& p : m_Pawns) {
            if (p.pawnIndex == pawnIndex) {
                p.entityHandle = handle;
                return &p;
            }
        }
        m_Pawns.push_back({ pawnIndex, handle, {}, {}, {}, {}, 0, {}, {}, false });
        return &m_Pawns.back();
    }
};

} // namespace cs2::sim

// ============================================================================
// VERIFICATION TEST SUITE
// ============================================================================
int main() {
    using namespace cs2::sim;

    std::cout << "=================================================================================\n";
    std::cout << "  SOURCE 2 AUTHORITATIVE TEMPORAL ROLLBACK & LAG COMPENSATION VERIFICATION SUITE \n";
    std::cout << "  Target: server.dll Build 26598758 | Pure C++20 Clean-Room Simulation           \n";
    std::cout << "=================================================================================\n\n";

    CLagCompensationManager manager;
    constexpr uint32_t ATTACKER_INDEX = 1;
    constexpr uint32_t TARGET_INDEX = 2;
    constexpr uint32_t TARGET_HANDLE = (TARGET_INDEX & 0x7FFF) | (1 << 15);

    // ---------------------------------------------------------------------------
    // TEST 1: Sub-Tick Fractional Interpolation Accuracy
    // ---------------------------------------------------------------------------
    std::cout << "[TEST 1] Verifying Sub-Tick Fractional Interpolation Accuracy...\n";
    {
        // Setup two bracketing states:
        // Tick 100 (t = 1.5625s): Origin (100, 0, 0), Angle 0 deg
        // Tick 101 (t = 1.578125s): Origin (200, 50, 20), Angle 90 deg around Z
        const uint32_t tick0 = 100;
        const uint32_t tick1 = 101;
        const Vector3 p0(100.0f, 0.0f, 0.0f);
        const Vector3 p1(110.0f, 5.0f, 2.0f);
        const Quaternion q0 = Quaternion::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, 0.0f);
        const Quaternion q1 = Quaternion::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, std::numbers::pi_v<float> * 0.5f);
        const AABB box({ -16.0f, -16.0f, 0.0f }, { 16.0f, 16.0f, 72.0f });

        std::array<Matrix3x4, MAX_BONES> bones0{}, bones1{};
        bones0[0] = Matrix3x4::FromQuaternionTranslation(q0, p0);
        bones1[0] = Matrix3x4::FromQuaternionTranslation(q1, p1);

        manager.RecordPawnState(TARGET_INDEX, TARGET_HANDLE, tick0, 0.0f, p0, q0, box, bones0.data(), 1);
        manager.RecordPawnState(TARGET_INDEX, TARGET_HANDLE, tick1, 0.0f, p1, q1, box, bones1.data(), 1);

        // Test fractions: w in {0.0, 0.25, 0.50, 0.75, 1.0}
        const std::array<float, 5> testFractions = { 0.0f, 0.25f, 0.50f, 0.75f, 1.0f };
        for (float w : testFractions) {
            const float targetTime = (static_cast<float>(tick0) + w) * TICK_INTERVAL;
            LagRecord_t rec;
            const bool ok = manager.BacktrackEntity(*manager.GetPawn(TARGET_INDEX), targetTime, rec);
            assert(ok && "BacktrackEntity must succeed");

            const Vector3 expectedP = Lerp(p0, p1, w);
            const float pErr = (rec.origin - expectedP).Length();
            assert(pErr < 1e-4f && "Origin Lerp error exceeds tolerance");

            // Verify SLERP unit magnitude
            const float qNorm = rec.orientation.Norm();
            assert(std::abs(qNorm - 1.0f) < 1e-4f && "SLERP quaternion must be unit norm");

            std::cout << "  -> Fraction w=" << std::fixed << std::setprecision(2) << w
                      << " | Target P=(" << std::setw(6) << std::setprecision(1) << rec.origin.x << ", "
                      << std::setw(4) << rec.origin.y << ", " << std::setw(4) << rec.origin.z << ") "
                      << " | |Norm(q)|=" << std::setprecision(5) << qNorm
                      << " | Err=" << std::scientific << pErr << " [PASS]\n";
        }
    }
    std::cout << "  [SUCCESS] Sub-Tick Fractional Interpolation Accuracy Verified (100% Precision).\n\n";

    // ---------------------------------------------------------------------------
    // TEST 2: Directional Raycast Spatial Intersection (Compensated vs Uncompensated)
    // ---------------------------------------------------------------------------
    std::cout << "[TEST 2] Verifying Directional Raycast Spatial Intersection (Compensated vs Uncompensated)...\n";
    {
        manager.ClearHistory();
        // Simulate a target entity executing transverse motion across the screen:
        // Movement along X axis at 250 units/second (standard running speed)
        // Record 16 simulation ticks (250ms duration)
        const Vector3 initialPos(0.0f, 500.0f, 0.0f); // 500 units in front of viewer
        const Vector3 velocity(250.0f, 0.0f, 0.0f);
        const Quaternion facing = Quaternion::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, 0.0f);
        const AABB pawnBounds({ -16.0f, -16.0f, 0.0f }, { 16.0f, 16.0f, 72.0f });

        std::array<Matrix3x4, MAX_BONES> bones{};
        for (uint32_t t = 0; t < 16; ++t) {
            const float simTime = static_cast<float>(t) * TICK_INTERVAL;
            const Vector3 currentPos = initialPos + velocity * simTime;
            bones[0] = Matrix3x4::FromQuaternionTranslation(facing, currentPos);
            manager.RecordPawnState(TARGET_INDEX, TARGET_HANDLE, t, 0.0f, currentPos, facing, pawnBounds, bones.data(), 1);
        }

        TrackedPawn_t* pawn = manager.GetPawn(TARGET_INDEX);
        assert(pawn != nullptr);

        // At current tick (t = 15, time = 0.234375s), pawn is at:
        // X = 250 * 0.234375 = 58.59375 units
        const Vector3 presentPawnPos = pawn->activeOrigin;
        std::cout << "  Current Authoritative Target Position (Tick 15): ("
                  << std::fixed << std::setprecision(2) << presentPawnPos.x << ", "
                  << presentPawnPos.y << ", " << presentPawnPos.z << ")\n";

        // Suppose client fired a shot perceived at Tick 10 with sub-tick fraction w = 0.40
        // (Historic time = (10 + 0.40) * 0.015625s = 0.162500s)
        // At that past instant, target was at X = 250 * 0.1625 = 40.625 units
        const uint32_t clientAttackTick = 10;
        const float clientSubTickFrac = 0.40f;
        const float clientAttackTime = (static_cast<float>(clientAttackTick) + clientSubTickFrac) * TICK_INTERVAL;
        const Vector3 historicPerceivedCenter(250.0f * clientAttackTime, 500.0f, 36.0f); // Center of chest

        // Viewer fires a ray directly from origin (0, 0, 36) towards the historic perceived center:
        const Vector3 viewerEye(0.0f, 0.0f, 36.0f);
        const Ray attackRay(viewerEye, historicPerceivedCenter - viewerEye);

        float tNear = 0.0f, tFar = 0.0f;

        // Sub-test A: Raycast against UNCOMPENSATED present state
        // In the present state, the target has moved to X = 58.59.
        // Target's current bounds: X in [58.59 - 16, 58.59 + 16] = [42.59, 74.59].
        // The ray intersects Y = 500 at X = 40.625.
        // Since 40.625 < 42.59, the raycast strictly MISSES the uncompensated volume!
        const AABB presentWorldBox(
            pawn->activeOrigin + pawn->activeBounds.mins,
            pawn->activeOrigin + pawn->activeBounds.maxs
        );
        const bool uncompensatedHit = presentWorldBox.IntersectRay(attackRay, tNear, tFar);
        std::cout << "  [A] Uncompensated Present Spatial Query: "
                  << (uncompensatedHit ? "HIT (UNEXPECTED)" : "MISS (EXPECTED - Out of Bounds)")
                  << " [PASS]\n";
        assert(!uncompensatedHit && "Uncompensated raycast should miss due to target movement");

        // Sub-test B: Authoritative History Rollback via StartLagCompensation
        const bool rewindOk = manager.StartLagCompensation(ATTACKER_INDEX, clientAttackTime);
        assert(rewindOk && "StartLagCompensation must succeed");

        const Vector3 backtrackedPos = pawn->activeOrigin;
        std::cout << "  [B] Backtracked Target Position (StartLagCompensation): ("
                  << std::fixed << std::setprecision(2) << backtrackedPos.x << ", "
                  << backtrackedPos.y << ", " << backtrackedPos.z << ")\n";

        const AABB backtrackedWorldBox(
            pawn->activeOrigin + pawn->activeBounds.mins,
            pawn->activeOrigin + pawn->activeBounds.maxs
        );
        const bool compensatedHit = backtrackedWorldBox.IntersectRay(attackRay, tNear, tFar);
        assert(compensatedHit && "Compensated raycast MUST hit the backtracked bounding box");

        const Vector3 hitPoint = attackRay.PointAt(tNear);
        std::cout << "      Compensated Spatial Query: HIT at Distance="
                  << std::fixed << std::setprecision(2) << tNear << " units | HitPoint=("
                  << hitPoint.x << ", " << hitPoint.y << ", " << hitPoint.z << ") [PASS]\n";

        // Sub-test C: Authoritative State Restoration via FinishLagCompensation
        manager.FinishLagCompensation();
        assert(!pawn->isBacktracked && "Target must no longer be marked backtracked");
        assert(pawn->activeOrigin.BitwiseEqual(presentPawnPos) && "Target position must match pre-rewind state exactly");
        std::cout << "  [C] Post-Query State Restoration (FinishLagCompensation): Restored to ("
                  << pawn->activeOrigin.x << ", " << pawn->activeOrigin.y << ", " << pawn->activeOrigin.z << ") [PASS]\n";
    }
    std::cout << "  [SUCCESS] Spatial Raycast & Authoritative Rewind Protocol Fully Verified.\n\n";

    // ---------------------------------------------------------------------------
    // TEST 3: 100% Bit-Exact Authoritative State Restoration Guarantee
    // ---------------------------------------------------------------------------
    std::cout << "[TEST 3] Verifying 100% Bit-Exact State Restoration Guarantee...\n";
    {
        TrackedPawn_t* pawn = manager.GetPawn(TARGET_INDEX);
        assert(pawn != nullptr);

        // Snapshot pristine pre-rewind state
        const Vector3 origOrigin = pawn->activeOrigin;
        const Quaternion origOrientation = pawn->activeOrientation;
        const AABB origBounds = pawn->activeBounds;
        const size_t origBoneCount = pawn->activeBoneCount;
        const auto origBones = pawn->activeBones;

        // Execute multiple nested or sequential rewind passes
        for (int pass = 1; pass <= 10; ++pass) {
            const float randomTargetTime = 0.050f + static_cast<float>(pass) * 0.0125f;
            manager.StartLagCompensation(ATTACKER_INDEX, randomTargetTime);
            assert(pawn->isBacktracked);

            // Finish and restore
            manager.FinishLagCompensation();
            assert(!pawn->isBacktracked);

            // Verify bitwise exact equality across all structures
            assert(pawn->activeOrigin.BitwiseEqual(origOrigin) && "Origin drifted after restore");
            assert(pawn->activeOrientation.BitwiseEqual(origOrientation) && "Orientation drifted after restore");
            assert(pawn->activeBounds.mins.BitwiseEqual(origBounds.mins) && "Bounds mins drifted");
            assert(pawn->activeBounds.maxs.BitwiseEqual(origBounds.maxs) && "Bounds maxs drifted");
            assert(pawn->activeBoneCount == origBoneCount && "Bone count changed");

            for (size_t b = 0; b < origBoneCount; ++b) {
                assert(pawn->activeBones[b].BitwiseEqual(origBones[b]) && "Bone matrix drifted");
            }
        }
        std::cout << "  -> Evaluated 10 sequential StartLagCompensation / FinishLagCompensation cycles.\n";
        std::cout << "  -> Positional Drift: 0.000000 units | Angular Drift: 0.000000 rad | Bone Matrix Drift: 0.0\n";
    }
    std::cout << "  [SUCCESS] 100% Bit-Exact State Restoration Confirmed.\n\n";

    // ---------------------------------------------------------------------------
    // TEST 4: Anti-Exploitation Latency Clamping (sv_maxunlag Enforcement)
    // ---------------------------------------------------------------------------
    std::cout << "[TEST 4] Verifying Anti-Exploitation Latency Clamping (sv_maxunlag)...\n";
    {
        TrackedPawn_t* pawn = manager.GetPawn(TARGET_INDEX);
        assert(pawn != nullptr);

        const float latestSimTime = pawn->ringBuffer.front().simulationTime;
        // Request rewind 450ms in the past (sv_maxunlag is 200ms)
        const float exploitTargetTime = latestSimTime - 0.450f;
        const float expectedClampedTime = latestSimTime - MAX_UNLAG_DEFAULT;

        manager.StartLagCompensation(ATTACKER_INDEX, exploitTargetTime, MAX_UNLAG_DEFAULT);
        assert(pawn->isBacktracked);

        // Compute expected position at clamped target time
        LagRecord_t clampedRecord;
        manager.BacktrackEntity(*pawn, expectedClampedTime, clampedRecord);

        // Verify active backtracked position matches the clamped target time, NOT the exploited 450ms time
        const float deltaClamp = (pawn->activeOrigin - clampedRecord.origin).Length();
        assert(deltaClamp < 1e-4f && "Lag compensation must clamp target time to sv_maxunlag");

        std::cout << "  -> Requested Rewind: -" << std::fixed << std::setprecision(1)
                  << (latestSimTime - exploitTargetTime) * 1000.0f << "ms (Exploit Intent)\n";
        std::cout << "  -> Authoritative Clamped Window: -"
                  << MAX_UNLAG_DEFAULT * 1000.0f << "ms (sv_maxunlag Enforcement)\n";
        std::cout << "  -> Clamped Position Match: Delta=" << std::scientific << deltaClamp << " [PASS]\n";

        manager.FinishLagCompensation();
    }
    std::cout << "  [SUCCESS] Anti-Exploitation Latency Clamping Verified.\n\n";

    // ---------------------------------------------------------------------------
    // TEST 5: Teleport Discontinuity Breaker (4096.0 sqr units Threshold)
    // ---------------------------------------------------------------------------
    std::cout << "[TEST 5] Verifying Teleport Discontinuity Breaker (4096.0 sqr units)...\n";
    {
        // Add a teleport event between tick 50 and tick 51:
        // Tick 50: Origin (0, 0, 0)
        // Tick 51: Origin (500, 500, 0) -> Displacement = sqrt(500^2 + 500^2) = 707.1 units (>> 64 units)
        const uint32_t tpTick0 = 50;
        const uint32_t tpTick1 = 51;
        const Vector3 tpP0(0.0f, 0.0f, 0.0f);
        const Vector3 tpP1(500.0f, 500.0f, 0.0f);
        const Quaternion tpQ = Quaternion::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, 0.0f);
        const AABB tpBox({ -16.0f, -16.0f, 0.0f }, { 16.0f, 16.0f, 72.0f });

        std::array<Matrix3x4, MAX_BONES> tpBones{};
        tpBones[0] = Matrix3x4::FromQuaternionTranslation(tpQ, tpP0);

        CLagCompensationManager tpManager;
        tpManager.RecordPawnState(TARGET_INDEX, TARGET_HANDLE, tpTick0, 0.0f, tpP0, tpQ, tpBox, tpBones.data(), 1);
        tpBones[0] = Matrix3x4::FromQuaternionTranslation(tpQ, tpP1);
        tpManager.RecordPawnState(TARGET_INDEX, TARGET_HANDLE, tpTick1, 0.0f, tpP1, tpQ, tpBox, tpBones.data(), 1);

        // Query intermediate fraction w = 0.50
        const float midTime = (static_cast<float>(tpTick0) + 0.50f) * TICK_INTERVAL;
        LagRecord_t outRec;
        tpManager.BacktrackEntity(*tpManager.GetPawn(TARGET_INDEX), midTime, outRec);

        // With teleport breaker triggered, it must snap to tick 51 (frac >= 0.5), NOT interpolate to (250, 250, 0)
        const bool snappedToTick1 = outRec.origin.BitwiseEqual(tpP1);
        const bool interpolatedGhost = (outRec.origin - Vector3(250.0f, 250.0f, 0.0f)).Length() < 1.0f;

        assert(snappedToTick1 && !interpolatedGhost && "Discontinuity breaker must prevent mid-air ghost interpolation");

        std::cout << "  -> Teleport Distance: " << std::fixed << std::setprecision(1)
                  << (tpP1 - tpP0).Length() << " units (Threshold: 64.0 units / 4096.0 sqr units)\n";
        std::cout << "  -> Continuous Lerp Rejected: Snapped directly to Frame 51 [PASS]\n";
    }
    std::cout << "  [SUCCESS] Teleport Discontinuity Breaker Verified.\n\n";

    std::cout << "=================================================================================\n";
    std::cout << "  ALL 5 TEMPORAL ROLLBACK & LAG COMPENSATION TESTS COMPLETED: 100% PASS          \n";
    std::cout << "=================================================================================\n";

    return 0;
}
