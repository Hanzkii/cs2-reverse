#ifndef CS2_GAME_ENGINE_CLOCK_DRIFT_H
#define CS2_GAME_ENGINE_CLOCK_DRIFT_H

#include <cstdint>
#include <cmath>
#include <string_view>

// ============================================================================
// Source 2 Engine Sub-Tick Clock Drift Synchronizer & Authoritative Slammer
// Binary: engine2.dll | Build: 2026-11-11_26598758
// Verified VA: 0x180067AA0 | RVA: 0x00067AA0 | VTable Slot: 73 (CNetworkGameClient)
//
// Reverse Engineered & Reconstructed from:
//   - CNetworkGameClient::ApplyClockDrift (0x180067AA0)
//   - ConVar cl_clockdrift_max_secs (0x1807307d8, default 0.5s)
//   - Format: "Slamming client tick to server tick. abs( server tick %d - client tick %d = %d ) > ( cl_clockdrift_max_ticks %.1f + sim ticks %d + jitter margin %.1f = %d )\n"
//   - Format: "[s%5d c%5d t%6.3f] ApplyClockDrift correcting %d -> %d\n"
// ============================================================================

namespace cs2::engine {

struct ClockDriftTelemetry {
    int m_nClientTick;
    int m_nServerTick;
    int m_nTickDelta;
    float m_flClockDriftMaxTicks;
    int m_nSimTicksWindow;
    float m_flJitterMarginTicks;
    int m_nTotalThresholdTicks;
    bool m_bSlammed;
    int m_nCorrectedTick;
};

class CClockDriftSynchronizer {
public:
    CClockDriftSynchronizer(float flMaxSecs = 0.5f, float flTickRate = 64.0f) noexcept
        : m_flClockDriftMaxSecs(flMaxSecs),
          m_flTickRate(flTickRate),
          m_flJitterMarginSecs(0.015f),
          m_flAverageDriftSecs(0.0f),
          m_nSlamCount(0),
          m_nTargetTickClamped(0) {}

    // Bit-exact implementation of engine2.dll FUN_180067aa0 (CNetworkGameClient::ApplyClockDrift)
    [[nodiscard]] int ApplyClockDrift(
        int nClientTick,
        int nServerTick,
        int nSimulationTicksWindow,
        bool bDemoOrPaused = false,
        ClockDriftTelemetry* pOutTelemetry = nullptr) noexcept
    {
        if (nServerTick == 0 || bDemoOrPaused) {
            return nServerTick;
        }

        const float flMaxTicks = m_flClockDriftMaxSecs * m_flTickRate;
        const float flJitterTicks = m_flJitterMarginSecs * m_flTickRate;
        const int nThresholdBase = static_cast<int>(std::ceil(flJitterTicks + flMaxTicks));
        const int nTotalThreshold = nThresholdBase + nSimulationTicksWindow;

        const int nDelta = nClientTick - nServerTick;
        const int nAbsDelta = std::abs(nDelta);

        bool bSlam = false;
        int nResultTick = nServerTick;

        if (nTotalThreshold < nAbsDelta) {
            // Hard snap to authoritative server tick when client clock drifts outside allowable window
            bSlam = true;
            m_nSlamCount++;
        }

        if (bSlam) {
            // Slamming branch: Snaps client tick directly to server window
            nResultTick = (nClientTick - nSimulationTicksWindow) + 1;
        } else {
            // Proportional continuous adjustment
            nResultTick = nServerTick;
        }

        // Clamp against local tick floor
        if (nResultTick < m_nTargetTickClamped) {
            m_nTargetTickClamped = nResultTick;
        }

        if (pOutTelemetry) {
            pOutTelemetry->m_nClientTick = nClientTick;
            pOutTelemetry->m_nServerTick = nServerTick;
            pOutTelemetry->m_nTickDelta = nDelta;
            pOutTelemetry->m_flClockDriftMaxTicks = flMaxTicks;
            pOutTelemetry->m_nSimTicksWindow = nSimulationTicksWindow;
            pOutTelemetry->m_flJitterMarginTicks = flJitterTicks;
            pOutTelemetry->m_nTotalThresholdTicks = nTotalThreshold;
            pOutTelemetry->m_bSlammed = bSlam;
            pOutTelemetry->m_nCorrectedTick = nResultTick;
        }

        return nResultTick;
    }

    void SetClockDriftMaxSecs(float flSecs) noexcept { m_flClockDriftMaxSecs = flSecs; }
    [[nodiscard]] float GetClockDriftMaxSecs() const noexcept { return m_flClockDriftMaxSecs; }

    void SetJitterMarginSecs(float flSecs) noexcept { m_flJitterMarginSecs = flSecs; }
    [[nodiscard]] float GetJitterMarginSecs() const noexcept { return m_flJitterMarginSecs; }

    [[nodiscard]] int GetSlamCount() const noexcept { return m_nSlamCount; }
    void ResetSlamCount() noexcept { m_nSlamCount = 0; }

private:
    float m_flClockDriftMaxSecs;
    float m_flTickRate;
    float m_flJitterMarginSecs;
    float m_flAverageDriftSecs;
    int m_nSlamCount;
    int m_nTargetTickClamped;
};

} // namespace cs2::engine

#endif // CS2_GAME_ENGINE_CLOCK_DRIFT_H
