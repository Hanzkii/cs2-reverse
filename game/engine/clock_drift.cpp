#include "game/engine/clock_drift.h"
#include <iostream>
#include <iomanip>

namespace cs2::engine {

// Verification runner for unit and integration testing
void VerifyClockDriftSync() {
    CClockDriftSynchronizer sync(0.5f, 64.0f); // 0.5s @ 64Hz = 32 ticks max drift

    ClockDriftTelemetry telem{};

    // Scenario 1: Minor drift within tolerance (Client = 1005, Server = 1000, Window = 4)
    // Threshold = ceil(0.015*64 + 0.5*64) + 4 = ceil(0.96 + 32) + 4 = 33 + 4 = 37 ticks
    // Delta = 5 < 37 -> No slam
    int tick1 = sync.ApplyClockDrift(1005, 1000, 4, false, &telem);
    (void)tick1;

    // Scenario 2: Severe packet delay / clock desync (Client = 1050, Server = 1000, Window = 4)
    // Delta = 50 > 37 -> SLAM!
    int tick2 = sync.ApplyClockDrift(1050, 1000, 4, false, &telem);
    (void)tick2;
}

} // namespace cs2::engine
