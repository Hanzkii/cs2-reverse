#ifndef CS2_SERVER_LAGCOMPENSATION_H
#define CS2_SERVER_LAGCOMPENSATION_H

#include "../client/viewmatrix.h"
#include <cstdint>
#include <vector>
#include <deque>
#include <array>

namespace cs2::server {

using client::Vector3;
using client::QAngle;

constexpr int MAX_BONES = 128;
constexpr float MAX_BACKTRACK_WINDOW = 0.200f; // 200ms

struct Matrix3x4 {
    float m[3][4];
};

struct LagRecord {
    uint32_t tickCount;
    float simulationTime;
    Vector3 origin;
    QAngle eyeAngles;
    Vector3 mins;
    Vector3 maxs;
    std::array<Matrix3x4, MAX_BONES> boneMatrices;
};

struct PawnHistory {
    uint32_t pawnIndex;
    std::deque<LagRecord> records;
    LagRecord restoreSnapshot;
    bool isBacktracked = false;
};

/**
 * @brief Reconstructed Server-Side Lag Compensation from server.dll
 * Disassembled from server lag compensation pipeline.
 */
class CLagCompensationServer {
public:
    static CLagCompensationServer& Get();

    void RecordPawn(uint32_t pawnIndex, float curTime, uint32_t tick,
                    const Vector3& origin, const QAngle& angles,
                    const Vector3& mins, const Vector3& maxs,
                    const Matrix3x4* bones, size_t boneCount);

    bool StartLagCompensation(uint32_t pawnIndex, float targetTime);
    void FinishLagCompensation(uint32_t pawnIndex);

    static bool InterpolateBones(const LagRecord& from, const LagRecord& to,
                                 float fraction, std::array<Matrix3x4, MAX_BONES>& outBones);

private:
    CLagCompensationServer() = default;
    std::vector<PawnHistory> m_Pawns;

    PawnHistory* GetOrCreatePawn(uint32_t pawnIndex);
};

} // namespace cs2::server

#endif // CS2_SERVER_LAGCOMPENSATION_H
