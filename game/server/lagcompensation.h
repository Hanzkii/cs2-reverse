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

namespace cs2::server {

using client::Vector3;
using client::QAngle;

// -------------------------------------------------------------------------
// Authoritative Engine Constants (server.dll @ build 26144732)
// -------------------------------------------------------------------------
constexpr int MAX_BONES = 128;
constexpr float MAX_BACKTRACK_WINDOW = 0.200f;              // sv_maxunlag: 200ms (0.200s)
constexpr float TICK_INTERVAL_64 = 0.015625f;               // 1.0f / 64.0f (64 tick simulation)
constexpr float TELEPORT_DISTANCE_SQR_THRESHOLD = 4096.0f;  // 64.0f^2 units: Discontinuity breaker

// -------------------------------------------------------------------------
// Lag Record Bitflags (Reversed from FUN_180ed3f10 @ 0x180ed3f10)
// -------------------------------------------------------------------------
constexpr uint8_t LAG_RECORD_RESTORED        = 0x01; // Bit 0: Record applied / active
constexpr uint8_t LAG_RECORD_ORIGIN_CHANGED  = 0x02; // Bit 1: World origin offset modified
constexpr uint8_t LAG_RECORD_ANGLES_CHANGED  = 0x04; // Bit 2: Eye angles / rotation modified
constexpr uint8_t LAG_RECORD_BOUNDS_CHANGED  = 0x08; // Bit 3: Collision mins/maxs modified
constexpr uint8_t LAG_RECORD_BONES_CHANGED   = 0x10; // Bit 4: Skeletal bone matrices cached

struct Matrix3x4 {
    float m[3][4];

    static Matrix3x4 Identity() noexcept {
        Matrix3x4 out{};
        out.m[0][0] = 1.0f; out.m[1][1] = 1.0f; out.m[2][2] = 1.0f;
        return out;
    }
};

/**
 * @brief Authoritative Lag Compensation History Record (LagRecord_t)
 * Reconstructed from server.dll player_lagcompensation.cpp & FUN_180eced70
 */
struct LagRecord_t {
    uint32_t tickCount = 0;                                  // Server simulation tick index
    float simulationTime = 0.0f;                             // Authoritative simulation timestamp (seconds)
    float subTickFraction = 0.0f;                            // Continuous analog sub-tick offset [0.0, 1.0)
    Vector3 origin{};                                        // World-space pawn origin coordinates
    QAngle eyeAngles{};                                      // Eye/head orientation angles
    Vector3 mins{-16.0f, -16.0f, 0.0f};                      // Axis-aligned bounding box minimum
    Vector3 maxs{16.0f, 16.0f, 72.0f};                       // Axis-aligned bounding box maximum
    std::array<Matrix3x4, MAX_BONES> boneMatrices{};         // Resolved world-space bone transform cache
    size_t boneCount = 0;                                    // Active bone count (typically <= 128)
    uint8_t flags = 0;                                       // Bitmask of modified transforms
    Vector3 preTeleportOrigin{};                             // Spatial anchor for 4096.0 sqr distance check
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
 * @brief Ring buffer tracking history for a single player pawn
 */
struct PawnTrackHistory_t {
    uint32_t pawnIndex = 0;                                  // Pawn entity index in entity list
    EntityHandle handle{};                                   // 32-bit CEntityHandle (Index + Serial)
    std::deque<LagRecord_t> records;                         // Circular history buffer (<= 200ms)
    RestoreSnapshot_t restoreSnapshot;                       // Original state prior to rewind
    bool isBacktracked = false;                              // True if currently rewound for trace
};

using PawnHistory = PawnTrackHistory_t;

/**
 * @brief Reconstructed Server-Side Lag Compensation Engine from server.dll
 * Reverse-engineered from CLagCompensationManager:
 * - StartLagCompensation (0x180ed8c00 / 0x180ed6f10)
 * - FinishLagCompensation (0x180ebf350)
 * - BacktrackPlayer / BacktrackEntity (0x180eced70)
 * - RestoreEntityTo (0x180ed3f10)
 */
class CLagCompensationServer {
public:
    static CLagCompensationServer& Get();

    /**
     * @brief Records authoritative pawn state into ring buffer track.
     */
    void RecordDataIntoTrack(uint32_t pawnIndex, EntityHandle handle, float curTime, uint32_t tick,
                             float subTickFraction, const Vector3& origin, const QAngle& angles,
                             const Vector3& mins, const Vector3& maxs,
                             const Matrix3x4* bones, size_t boneCount, uint8_t flags = 0x1E);

    // Legacy compatibility overload
    void RecordPawn(uint32_t pawnIndex, float curTime, uint32_t tick,
                    const Vector3& origin, const QAngle& angles,
                    const Vector3& mins, const Vector3& maxs,
                    const Matrix3x4* bones, size_t boneCount);

    /**
     * @brief Rewinds all active target pawns to the client's continuous target timestamp.
     * Clamped to [currentTime - maxUnlag, currentTime].
     */
    bool StartLagCompensation(uint32_t attackerIndex, float targetTime,
                              float maxUnlag = MAX_BACKTRACK_WINDOW);

    /**
     * @brief Sub-tick aware StartLagCompensation converting tick + subTickFraction into continuous time.
     */
    bool StartLagCompensationSubTick(uint32_t attackerIndex, uint32_t targetTick,
                                     float subTickFraction, float maxUnlag = MAX_BACKTRACK_WINDOW);

    /**
     * @brief Restores all backtracked pawns back to their authoritative present states.
     * Reconstructed from CLagCompensationManager::FinishLagCompensation @ 0x180ebf350.
     */
    void FinishLagCompensation();

    // Legacy overload for single pawn index
    void FinishLagCompensation(uint32_t pawnIndex);

    /**
     * @brief Backtracks an individual entity by sampling/interpolating records.
     * Enforces the 4096.0 unit^2 teleportation breaker check (FUN_180ed3f10).
     */
    bool BacktrackEntity(PawnTrackHistory_t& pawn, float targetTime, LagRecord_t& outRecord);

    /**
     * @brief Restores an entity's physical state from snapshot (FUN_180ed3f10).
     */
    bool RestoreEntityTo(PawnTrackHistory_t& pawn, const LagRecord_t& record);

    /**
     * @brief Interpolates between two history records at continuous fraction [0.0, 1.0].
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
     * @brief Converts discrete tick base and analog sub-tick fraction into seconds.
     */
    [[nodiscard]] static constexpr float CalculateSubTickSimulationTime(
        uint32_t tick, float subTickFraction, float tickInterval = TICK_INTERVAL_64) noexcept {
        return (static_cast<float>(tick) + std::clamp(subTickFraction, 0.0f, 1.0f)) * tickInterval;
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
