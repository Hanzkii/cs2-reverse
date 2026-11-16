#include "lagcompensation.h"
#include <algorithm>
#include <cmath>

namespace cs2::server {

namespace {

// Helper: Normalize Euler angle to [-180, 180] degrees
float NormalizeDeg(float angle) noexcept {
    while (angle > 180.0f)  angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

// Helper: Interpolate Euler angles along shortest geodesic arc
QAngle InterpolateAngles(const QAngle& a, const QAngle& b, float t) noexcept {
    QAngle res;
    float deltaPitch = NormalizeDeg(b.pitch - a.pitch);
    float deltaYaw   = NormalizeDeg(b.yaw - a.yaw);
    float deltaRoll  = NormalizeDeg(b.roll - a.roll);

    res.pitch = NormalizeDeg(a.pitch + deltaPitch * t);
    res.yaw   = NormalizeDeg(a.yaw + deltaYaw * t);
    res.roll  = NormalizeDeg(a.roll + deltaRoll * t);
    return res;
}

} // anonymous namespace

CLagCompensationServer& CLagCompensationServer::Get() {
    static CLagCompensationServer s_Instance;
    return s_Instance;
}

PawnTrackHistory_t* CLagCompensationServer::GetOrCreatePawn(uint32_t pawnIndex, EntityHandle handle) {
    for (auto& p : m_Pawns) {
        if (p.pawnIndex == pawnIndex) {
            if (handle.IsValid()) {
                p.handle = handle;
            }
            return &p;
        }
    }
    m_Pawns.push_back({ pawnIndex, handle, {}, {}, false });
    return &m_Pawns.back();
}

void CLagCompensationServer::RecordDataIntoTrack(
    uint32_t pawnIndex, EntityHandle handle, float curTime, uint32_t tick,
    float subTickFraction, const Vector3& origin, const QAngle& angles,
    const Vector3& mins, const Vector3& maxs,
    const Matrix3x4* bones, size_t boneCount, uint8_t flags)
{
    Quaternion orientation = Quaternion::FromEuler(angles.pitch, angles.yaw, angles.roll);
    RecordDataIntoTrackQuat(pawnIndex, handle, curTime, tick, subTickFraction,
                            origin, orientation, mins, maxs, bones, boneCount, flags);
    if (!m_Pawns.empty()) {
        PawnTrackHistory_t* p = GetOrCreatePawn(pawnIndex, handle);
        if (p && !p->records.empty()) {
            p->records.front().eyeAngles = angles;
        }
    }
}

void CLagCompensationServer::RecordDataIntoTrackQuat(
    uint32_t pawnIndex, EntityHandle handle, float curTime, uint32_t tick,
    float subTickFraction, const Vector3& origin, const Quaternion& orientation,
    const Vector3& mins, const Vector3& maxs,
    const Matrix3x4* bones, size_t boneCount, uint8_t flags)
{
    PawnTrackHistory_t* pawn = GetOrCreatePawn(pawnIndex, handle);
    if (!pawn) return;

    LagRecord_t record;
    record.tickCount = tick;
    record.simulationTime = curTime;
    record.subTickFraction = std::clamp(subTickFraction, 0.0f, 1.0f);
    record.origin = origin;
    record.orientation = orientation;
    record.mins = mins;
    record.maxs = maxs;
    record.flags = flags;

    // Anchor previous origin for teleport breaker check (FUN_180ecdb00)
    if (!pawn->records.empty()) {
        record.preTeleportOrigin = pawn->records.front().origin;
    } else {
        record.preTeleportOrigin = origin;
    }

    const size_t count = std::min(boneCount, static_cast<size_t>(MAX_BONES));
    record.boneCount = count;
    for (size_t i = 0; i < count; ++i) {
        record.boneMatrices[i] = bones[i];
    }
    // Fill identity for uncomputed trailing joints
    for (size_t i = count; i < MAX_BONES; ++i) {
        record.boneMatrices[i] = Matrix3x4::Identity();
    }

    pawn->records.push_front(record);

    // Maintain 64-tick circular buffer capacity (1.000s retention @ 64Hz)
    while (pawn->records.size() > RING_BUFFER_CAPACITY) {
        pawn->records.pop_back();
    }
}

void CLagCompensationServer::RecordPawn(
    uint32_t pawnIndex, float curTime, uint32_t tick,
    const Vector3& origin, const QAngle& angles,
    const Vector3& mins, const Vector3& maxs,
    const Matrix3x4* bones, size_t boneCount)
{
    RecordDataIntoTrack(pawnIndex, EntityHandle(pawnIndex, 1), curTime, tick,
                        0.0f, origin, angles, mins, maxs, bones, boneCount, 0x1E);
}

bool CLagCompensationServer::StartLagCompensation(
    uint32_t attackerIndex, float targetTime, float maxUnlag)
{
    if (m_Pawns.empty()) {
        return false;
    }

    // Determine current simulation baseline time from newest tracked record
    float latestTime = 0.0f;
    for (const auto& p : m_Pawns) {
        if (!p.records.empty() && p.records.front().simulationTime > latestTime) {
            latestTime = p.records.front().simulationTime;
        }
    }

    // Anti-exploitation latency clamp: [latestTime - maxUnlag, latestTime]
    const float minAllowedTime = latestTime - maxUnlag;
    const float clampedTarget = std::clamp(targetTime, minAllowedTime, latestTime);

    bool anyBacktracked = false;

    // Iterate all active pawns and backtrack candidate targets (VA 0x180ED27D0)
    for (auto& pawn : m_Pawns) {
        if (pawn.pawnIndex == attackerIndex || pawn.records.empty()) {
            continue;
        }

        if (pawn.isBacktracked) {
            continue;
        }

        // Snapshot authoritative current state for 100% bit-exact restoration
        pawn.restoreSnapshot.snapshot = pawn.records.front();
        pawn.restoreSnapshot.isBacktracked = true;
        pawn.restoreSnapshot.originModified = true;
        pawn.restoreSnapshot.anglesModified = true;
        pawn.restoreSnapshot.boundsModified = true;
        pawn.restoreSnapshot.bonesModified = true;

        // Compute sub-tick interpolated backtracked record
        LagRecord_t backtrackedRecord;
        if (BacktrackEntity(pawn, clampedTarget, backtrackedRecord)) {
            // Apply historical backtracked state to head of record list
            pawn.records.front().origin = backtrackedRecord.origin;
            pawn.records.front().orientation = backtrackedRecord.orientation;
            pawn.records.front().eyeAngles = backtrackedRecord.eyeAngles;
            pawn.records.front().mins = backtrackedRecord.mins;
            pawn.records.front().maxs = backtrackedRecord.maxs;
            pawn.records.front().boneMatrices = backtrackedRecord.boneMatrices;
            pawn.records.front().boneCount = backtrackedRecord.boneCount;

            pawn.isBacktracked = true;
            anyBacktracked = true;
        }
    }

    return anyBacktracked;
}

bool CLagCompensationServer::StartLagCompensationSubTick(
    uint32_t attackerIndex, uint32_t targetTick,
    float subTickFraction, float flInterp, float maxUnlag)
{
    const float targetTime = CalculateSubTickSimulationTime(targetTick, subTickFraction, flInterp);
    return StartLagCompensation(attackerIndex, targetTime, maxUnlag);
}

void CLagCompensationServer::FinishLagCompensation() {
    // Reconstructed from CLagCompensationManager::FinishLagCompensation @ 0x180EB8F40
    for (auto& pawn : m_Pawns) {
        if (pawn.isBacktracked && pawn.restoreSnapshot.isBacktracked) {
            RestoreEntityTo(pawn, pawn.restoreSnapshot.snapshot);
            pawn.isBacktracked = false;
            pawn.restoreSnapshot.isBacktracked = false;
        }
    }
}

void CLagCompensationServer::FinishLagCompensation(uint32_t pawnIndex) {
    for (auto& pawn : m_Pawns) {
        if (pawn.pawnIndex == pawnIndex && pawn.isBacktracked) {
            RestoreEntityTo(pawn, pawn.restoreSnapshot.snapshot);
            pawn.isBacktracked = false;
            pawn.restoreSnapshot.isBacktracked = false;
            break;
        }
    }
}

bool CLagCompensationServer::BacktrackEntity(
    PawnTrackHistory_t& pawn, float targetTime, LagRecord_t& outRecord)
{
    if (pawn.records.empty()) {
        return false;
    }

    const auto& history = pawn.records;

    // Extrapolation bounds clamping
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

    // -------------------------------------------------------------------------
    // Teleport Distance Squared Breaker (4096.0 = 64.0^2 units)
    // Decompiled from FUN_180ecdb00 / RestoreEntityTo
    // -------------------------------------------------------------------------
    const float dx = newerRecord->origin.x - olderRecord->origin.x;
    const float dy = newerRecord->origin.y - olderRecord->origin.y;
    const float dz = newerRecord->origin.z - olderRecord->origin.z;
    const float distSqr = dx * dx + dy * dy + dz * dz;

    if (distSqr >= TELEPORT_DISTANCE_SQR_THRESHOLD) {
        // Discontinuity detected: clamp to nearest discrete frame to prevent visual glitching
        const float timeSpan = newerRecord->simulationTime - olderRecord->simulationTime;
        const float fraction = (timeSpan > 1e-6f) ? (targetTime - olderRecord->simulationTime) / timeSpan : 0.0f;
        outRecord = (fraction >= 0.5f) ? *newerRecord : *olderRecord;
        return true;
    }

    // Continuous fractional interpolation: fraction in [0.0, 1.0]
    const float timeDelta = newerRecord->simulationTime - olderRecord->simulationTime;
    const float fraction = (timeDelta > 1e-6f)
        ? std::clamp((targetTime - olderRecord->simulationTime) / timeDelta, 0.0f, 1.0f)
        : 0.0f;

    return InterpolateRecords(*olderRecord, *newerRecord, fraction, outRecord);
}

bool CLagCompensationServer::RestoreEntityTo(
    PawnTrackHistory_t& pawn, const LagRecord_t& record)
{
    // Reconstructed from CLagCompensationManager::RestoreEntityTo @ 0x180ECDB00
    if (pawn.records.empty()) {
        return false;
    }

    // 1. Restore collision bounds (flags & 0x08)
    if (record.flags & LAG_RECORD_BOUNDS_CHANGED) {
        pawn.records.front().mins = record.mins;
        pawn.records.front().maxs = record.maxs;
    }

    // 2. Restore eye/head orientation & quaternion (flags & 0x04)
    if (record.flags & LAG_RECORD_ANGLES_CHANGED) {
        pawn.records.front().eyeAngles = record.eyeAngles;
        pawn.records.front().orientation = record.orientation;
    }

    // 3. Restore world origin (flags & 0x02) with 4096.0 sqr distance check
    if (record.flags & LAG_RECORD_ORIGIN_CHANGED) {
        const float dx = pawn.records.front().origin.x - record.preTeleportOrigin.x;
        const float dy = pawn.records.front().origin.y - record.preTeleportOrigin.y;
        const float dz = pawn.records.front().origin.z - record.preTeleportOrigin.z;
        const float moveDistSqr = dx * dx + dy * dy + dz * dz;

        if (moveDistSqr < TELEPORT_DISTANCE_SQR_THRESHOLD) {
            pawn.records.front().origin = record.origin;
        }
    }

    // 4. Restore skeletal bone transforms (flags & 0x10)
    if (record.flags & LAG_RECORD_BONES_CHANGED) {
        const size_t count = std::min(record.boneCount, static_cast<size_t>(MAX_BONES));
        for (size_t i = 0; i < count; ++i) {
            pawn.records.front().boneMatrices[i] = record.boneMatrices[i];
        }
        pawn.records.front().boneCount = count;
    }

    pawn.isBacktracked = false;
    return true;
}

bool CLagCompensationServer::InterpolateRecords(
    const LagRecord_t& older, const LagRecord_t& newer,
    float fraction, LagRecord_t& outRecord)
{
    fraction = std::clamp(fraction, 0.0f, 1.0f);

    outRecord.tickCount = (fraction >= 0.5f) ? newer.tickCount : older.tickCount;
    outRecord.simulationTime = older.simulationTime + (newer.simulationTime - older.simulationTime) * fraction;
    outRecord.subTickFraction = fraction;

    // Linear origin lerp: P(u) = P0 + (P1 - P0) * u
    outRecord.origin.x = older.origin.x + (newer.origin.x - older.origin.x) * fraction;
    outRecord.origin.y = older.origin.y + (newer.origin.y - older.origin.y) * fraction;
    outRecord.origin.z = older.origin.z + (newer.origin.z - older.origin.z) * fraction;

    // Geodesic Shortest-Arc Quaternion SLERP / NLERP
    outRecord.orientation = Quaternion::Slerp(older.orientation, newer.orientation, fraction);

    // Angle interpolation with wrap-around
    outRecord.eyeAngles = InterpolateAngles(older.eyeAngles, newer.eyeAngles, fraction);

    // Bounding box dimensions
    outRecord.mins.x = older.mins.x + (newer.mins.x - older.mins.x) * fraction;
    outRecord.mins.y = older.mins.y + (newer.mins.y - older.mins.y) * fraction;
    outRecord.mins.z = older.mins.z + (newer.mins.z - older.mins.z) * fraction;

    outRecord.maxs.x = older.maxs.x + (newer.maxs.x - older.maxs.x) * fraction;
    outRecord.maxs.y = older.maxs.y + (newer.maxs.y - older.maxs.y) * fraction;
    outRecord.maxs.z = older.maxs.z + (newer.maxs.z - older.maxs.z) * fraction;

    outRecord.flags = older.flags | newer.flags;
    outRecord.boneCount = std::max(older.boneCount, newer.boneCount);

    return InterpolateBones(older, newer, fraction, outRecord.boneMatrices, outRecord.boneCount);
}

bool CLagCompensationServer::InterpolateBones(
    const LagRecord_t& from, const LagRecord_t& to,
    float fraction, std::array<Matrix3x4, MAX_BONES>& outBones,
    size_t boneCount)
{
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    const size_t count = std::min(boneCount, static_cast<size_t>(MAX_BONES));

    for (size_t b = 0; b < count; ++b) {
        outBones[b] = LerpMatrix(from.boneMatrices[b], to.boneMatrices[b], fraction);
    }
    for (size_t b = count; b < MAX_BONES; ++b) {
        outBones[b] = Matrix3x4::Identity();
    }
    return true;
}

} // namespace cs2::server
