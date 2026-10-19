#include "subtick_prediction.h"
#include <cmath>
#include <algorithm>

namespace cs2::client {

void CSubtickPrediction::SimulateGroundFriction(PlayerMovementState& state, float dt) {
    const float speed = std::sqrt(state.velocity.x * state.velocity.x + state.velocity.y * state.velocity.y);
    if (speed < 0.1f) {
        state.velocity.x = 0;
        state.velocity.y = 0;
        return;
    }

    const float drop = speed * FRICTION * dt;
    const float newspeed = std::max(0.0f, speed - drop);
    const float frac = newspeed / speed;

    state.velocity.x *= frac;
    state.velocity.y *= frac;
}

void CSubtickPrediction::SimulateAirborne(PlayerMovementState& state, float dt) {
    state.velocity.z -= GRAVITY * dt;
}

void CSubtickPrediction::PredictUserCmd(PlayerMovementState& state, const UserCmd& cmd) {
    float currentFraction = 0.0f;

    auto events = cmd.subTickMoves;
    std::sort(events.begin(), events.end(), [](const SubTickMoveStep& a, const SubTickMoveStep& b) {
        return a.when < b.when;
    });

    for (const auto& ev : events) {
        const float sliceDt = (ev.when - currentFraction) * TICK_INTERVAL;
        if (sliceDt > 0.0f) {
            if (state.onGround) {
                SimulateGroundFriction(state, sliceDt);
            } else {
                SimulateAirborne(state, sliceDt);
            }
            state.origin.x += state.velocity.x * sliceDt;
            state.origin.y += state.velocity.y * sliceDt;
            state.origin.z += state.velocity.z * sliceDt;
        }

        // Sub-tick instantaneous action impulse
        if (ev.pressed && (ev.button & IN_JUMP) && state.onGround) {
            state.velocity.z = JUMP_IMPULSE;
            state.onGround = false;
        }

        currentFraction = ev.when;
    }

    const float remainderDt = (1.0f - currentFraction) * TICK_INTERVAL;
    if (remainderDt > 0.0f) {
        if (state.onGround) {
            SimulateGroundFriction(state, remainderDt);
        } else {
            SimulateAirborne(state, remainderDt);
        }
        state.origin.x += state.velocity.x * remainderDt;
        state.origin.y += state.velocity.y * remainderDt;
        state.origin.z += state.velocity.z * remainderDt;
    }
}

} // namespace cs2::client
