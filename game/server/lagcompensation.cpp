#include "lagcompensation.h"
#include <algorithm>

namespace cs2::server {

CLagCompensationServer& CLagCompensationServer::Get() {
    static CLagCompensationServer s_Instance;
    return s_Instance;
}

PawnHistory* CLagCompensationServer::GetOrCreatePawn(uint32_t pawnIndex) {
    for (auto& p : m_Pawns) {
        if (p.pawnIndex == pawnIndex) {
            return &p;
        }
    }
    m_Pawns.push_back({ pawnIndex, {}, {}, false });
    return &m_Pawns.back();
}

void CLagCompensationServer::RecordPawn(uint32_t pawnIndex, float curTime, uint32_t tick,
                                        const Vector3& origin, const QAngle& angles,
                                        const Vector3& mins, const Vector3& maxs,
                                        const Matrix3x4* bones, size_t boneCount) {
    PawnHistory* pawn = GetOrCreatePawn(pawnIndex);
    if (!pawn) return;

    LagRecord record;
    record.tickCount = tick;
    record.simulationTime = curTime;
    record.origin = origin;
    record.eyeAngles = angles;
    record.mins = mins;
    record.maxs = maxs;

    const size_t count = std::min(boneCount, static_cast<size_t>(MAX_BONES));
    for (size_t i = 0; i < count; ++i) {
        record.boneMatrices[i] = bones[i];
    }

    pawn->records.push_front(record);

    while (!pawn->records.empty() && 
           (curTime - pawn->records.back().simulationTime) > MAX_BACKTRACK_WINDOW) {
        pawn->records.pop_back();
    }
}

bool CLagCompensationServer::StartLagCompensation(uint32_t pawnIndex, float targetTime) {
    PawnHistory* pawn = GetOrCreatePawn(pawnIndex);
    if (!pawn || pawn->records.empty() || pawn->isBacktracked) {
        return false;
    }

    pawn->restoreSnapshot = pawn->records.front();

    const auto& history = pawn->records;
    const LagRecord* targetRecord = nullptr;

    for (const auto& rec : history) {
        if (rec.simulationTime <= targetTime) {
            targetRecord = &rec;
            break;
        }
    }

    if (!targetRecord) {
        targetRecord = &history.back();
    }

    pawn->isBacktracked = true;
    return true;
}

void CLagCompensationServer::FinishLagCompensation(uint32_t pawnIndex) {
    PawnHistory* pawn = GetOrCreatePawn(pawnIndex);
    if (!pawn || !pawn->isBacktracked) {
        return;
    }

    pawn->isBacktracked = false;
}

bool CLagCompensationServer::InterpolateBones(const LagRecord& from, const LagRecord& to,
                                             float fraction, std::array<Matrix3x4, MAX_BONES>& outBones) {
    fraction = std::clamp(fraction, 0.0f, 1.0f);

    for (int b = 0; b < MAX_BONES; ++b) {
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 4; ++c) {
                outBones[b].m[r][c] = from.boneMatrices[b].m[r][c] +
                                     (to.boneMatrices[b].m[r][c] - from.boneMatrices[b].m[r][c]) * fraction;
            }
        }
    }
    return true;
}

} // namespace cs2::server
