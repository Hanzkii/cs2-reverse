#ifndef CS2_CLIENT_SUBTICK_PREDICTION_H
#define CS2_CLIENT_SUBTICK_PREDICTION_H

#include "viewmatrix.h"
#include "movement.h"
#include <cstdint>
#include <vector>

namespace cs2::client {

using InButtonBits = MoveButtonBits;

struct SubTickMoveStep {
    float when;        // 0.0 to 1.0 (sub-tick fractional offset)
    uint64_t button;   // InButtonBits
    bool pressed;      // true if pressed, false if released
};

struct UserCmd {
    uint32_t commandNumber;
    uint32_t clientTick;
    float forwardMove;
    float sideMove;
    QAngle viewAngles;
    uint64_t buttons;
    std::vector<SubTickMoveStep> subTickMoves;
};

struct PlayerMovementState {
    Vector3 origin;
    Vector3 velocity;
    bool onGround;
    float stamina;
};

/**
 * @brief Reconstructed Sub-Tick Movement Prediction from client.dll
 * Multi-slice continuous physics solver matching CS2 continuous analog jump timing.
 */
class CSubtickPrediction {
public:
    static constexpr float TICK_INTERVAL = 1.0f / 64.0f; // 0.015625s
    static constexpr float GRAVITY = 800.0f;             // u/s^2
    static constexpr float JUMP_IMPULSE = 301.993378f;   // sqrt(2 * 800 * 57)
    static constexpr float FRICTION = 5.2f;

    static void PredictUserCmd(PlayerMovementState& state, const UserCmd& cmd);
    static void SimulateGroundFriction(PlayerMovementState& state, float dt);
    static void SimulateAirborne(PlayerMovementState& state, float dt);
};

} // namespace cs2::client

#endif // CS2_CLIENT_SUBTICK_PREDICTION_H
