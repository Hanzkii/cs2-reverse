#include "movement.h"

namespace cs2::client {

void CPlayerMovement::ClipVelocity(const Vector3& in, const Vector3& normal, Vector3& out, float overbounce) {
    const float backoff = (in.x * normal.x + in.y * normal.y + in.z * normal.z) * overbounce;

    out.x = in.x - (normal.x * backoff);
    out.y = in.y - (normal.y * backoff);
    out.z = in.z - (normal.z * backoff);

    // Friction / rubbing deadzone
    constexpr float STOP_EPSILON = 0.001f;
    if (std::abs(out.x) < STOP_EPSILON) out.x = 0.0f;
    if (std::abs(out.y) < STOP_EPSILON) out.y = 0.0f;
    if (std::abs(out.z) < STOP_EPSILON) out.z = 0.0f;
}

MoveTrace_t CPlayerMovement::TracePlayerBBox(const Vector3& start, const Vector3& end,
                                            const PlayerMovementServices& services) {
    MoveTrace_t tr;
    tr.startPos = start;
    tr.endPos = end;
    tr.planeNormal = Vector3{0.0f, 0.0f, 1.0f};

    // Increments m_nTraceCount at offset +0x0648
    const_cast<PlayerMovementServices&>(services).m_nTraceCount++;

    // Ground plane analytical collision test (Z <= 0.0f is default solid floor)
    if (end.z <= 0.0f) {
        if (start.z <= 0.0f) {
            tr.fraction = 0.0f;
            tr.allSolid = true;
            tr.startSolid = true;
            tr.endPos = start;
        } else {
            const float deltaZ = start.z - end.z;
            tr.fraction = (deltaZ > 0.0001f) ? (start.z / deltaZ) : 0.0f;
            tr.fraction = std::clamp(tr.fraction, 0.0f, 1.0f);
            tr.endPos.x = start.x + (end.x - start.x) * tr.fraction;
            tr.endPos.y = start.y + (end.y - start.y) * tr.fraction;
            tr.endPos.z = 0.0f;
        }
        tr.planeNormal = Vector3{0.0f, 0.0f, 1.0f};
    } else {
        tr.fraction = 1.0f;
        tr.endPos = end;
    }

    return tr;
}

void CPlayerMovement::Friction(PlayerMovementServices& services, PlayerMoveData& mv, float dt) {
    const float speed = std::sqrt(mv.velocity.x * mv.velocity.x + mv.velocity.y * mv.velocity.y);
    if (speed < 0.1f) {
        mv.velocity.x = 0.0f;
        mv.velocity.y = 0.0f;
        return;
    }

    // sv_stopspeed threshold clamping
    const float control = (speed < SV_STOPSPEED) ? SV_STOPSPEED : speed;

    // drop = control * sv_friction * m_flSurfaceFriction * dt
    const float drop = control * SV_FRICTION * services.m_flSurfaceFriction * dt;
    const float newspeed = std::max(0.0f, speed - drop);

    if (newspeed != speed) {
        const float fraction = newspeed / speed;
        mv.velocity.x *= fraction;
        mv.velocity.y *= fraction;
    }
}

void CPlayerMovement::Accelerate(PlayerMovementServices& services, PlayerMoveData& mv,
                                 const Vector3& wishdir, float wishspeed, float accel, float dt) {
    // Current projected speed along wish direction
    const float currentspeed = mv.velocity.x * wishdir.x + mv.velocity.y * wishdir.y;
    const float addspeed = wishspeed - currentspeed;

    if (addspeed <= 0.0f) {
        return;
    }

    // Stance multipliers: crouch (0.34) and shift-walk (0.52)
    float stanceMultiplier = 1.0f;
    if (services.m_bDucked) {
        stanceMultiplier = CS_DUCK_SPEED_MULTIPLIER;
    } else if (mv.buttons & (IN_SPEED | IN_WALK)) {
        stanceMultiplier = CS_WALK_SPEED_MULTIPLIER;
    }

    // Weapon max speed scalar (sv_accelerate_use_weapon_speed)
    float weaponSpeedRatio = 1.0f;
    if (mv.clientMaxSpeed > 0.0f && mv.clientMaxSpeed < 250.0f) {
        weaponSpeedRatio = mv.clientMaxSpeed / 250.0f;
    }

    // accelspeed = accel * dt * wishspeed * m_flSurfaceFriction * stanceMultiplier * weaponSpeedRatio
    float accelspeed = accel * dt * wishspeed * services.m_flSurfaceFriction * stanceMultiplier * weaponSpeedRatio;
    if (accelspeed > addspeed) {
        accelspeed = addspeed;
    }

    mv.velocity.x += accelspeed * wishdir.x;
    mv.velocity.y += accelspeed * wishdir.y;
}

void CPlayerMovement::AirAccelerate(PlayerMovementServices& services, PlayerMoveData& mv,
                                    const Vector3& wishdir, float wishspeed, float accel, float dt) {
    // Air wishspeed is strictly clamped to sv_air_max_wishspeed (30.0f)
    const float wishspd = std::min(wishspeed, SV_AIR_MAX_WISHSPEED);

    const float currentspeed = mv.velocity.x * wishdir.x + mv.velocity.y * wishdir.y + mv.velocity.z * wishdir.z;
    const float addspeed = wishspd - currentspeed;

    if (addspeed <= 0.0f) {
        return;
    }

    float accelspeed = accel * wishspeed * dt * services.m_flSurfaceFriction;
    if (accelspeed > addspeed) {
        accelspeed = addspeed;
    }

    mv.velocity.x += accelspeed * wishdir.x;
    mv.velocity.y += accelspeed * wishdir.y;
    mv.velocity.z += accelspeed * wishdir.z;
}

void CPlayerMovement::CategorizePosition(PlayerMovementServices& services, PlayerMoveData& mv, float /*dt*/) {
    // Trace 2 units downward to detect floor contact
    Vector3 point = mv.origin;
    point.z -= 2.0f;

    MoveTrace_t tr = TracePlayerBBox(mv.origin, point, services);

    // Check if normal is steep or walkable (z >= 0.7071 for <= 45.57 deg)
    if (tr.fraction < 1.0f && tr.planeNormal.z >= SV_STANDABLE_NORMAL_Z) {
        mv.onGround = true;
        services.m_groundNormal = tr.planeNormal;

        // Snap vertical position to contact surface and kill downward velocity
        if (mv.velocity.z <= 0.0f) {
            mv.origin.z = tr.endPos.z;
            mv.velocity.z = 0.0f;
        }
    } else {
        mv.onGround = false;
    }
}

void CPlayerMovement::StepMove(PlayerMovementServices& services, PlayerMoveData& mv, float dt) {
    const Vector3 originalPos = mv.origin;
    const Vector3 originalVel = mv.velocity;

    // 1. First attempt direct horizontal move
    Vector3 dest;
    dest.x = originalPos.x + originalVel.x * dt;
    dest.y = originalPos.y + originalVel.y * dt;
    dest.z = originalPos.z;

    MoveTrace_t tr = TracePlayerBBox(originalPos, dest, services);
    if (tr.fraction == 1.0f) {
        mv.origin = tr.endPos;
        return;
    }

    // 2. Obstacle encountered: attempt obstacle step-up (up to sv_stepsize = 18.0f)
    Vector3 stepUpPos = originalPos;
    stepUpPos.z += SV_STEPSIZE;

    Vector3 stepDest = stepUpPos;
    stepDest.x += originalVel.x * dt;
    stepDest.y += originalVel.y * dt;

    MoveTrace_t trStep = TracePlayerBBox(stepUpPos, stepDest, services);

    // 3. Step downward from elevated position to locate new ground surface
    Vector3 stepDownEnd = trStep.endPos;
    stepDownEnd.z -= SV_STEPSIZE;

    MoveTrace_t trDown = TracePlayerBBox(trStep.endPos, stepDownEnd, services);

    // 4. Verify if stepped move made valid horizontal progress
    const float directDistSqr = (tr.endPos.x - originalPos.x) * (tr.endPos.x - originalPos.x) +
                                (tr.endPos.y - originalPos.y) * (tr.endPos.y - originalPos.y);
    const float stepDistSqr   = (trDown.endPos.x - originalPos.x) * (trDown.endPos.x - originalPos.x) +
                                (trDown.endPos.y - originalPos.y) * (trDown.endPos.y - originalPos.y);

    if (stepDistSqr > directDistSqr && trDown.planeNormal.z >= SV_STANDABLE_NORMAL_Z) {
        mv.origin = trDown.endPos;
    } else {
        // Fall back to sliding along collision normal
        ClipVelocity(originalVel, tr.planeNormal, mv.velocity);
        mv.origin = tr.endPos;
    }
}

float CPlayerMovement::CalculateSubtickJumpVelocity(const PlayerMovementServices& services,
                                                   float subtickFraction, float horizontalSpeed) {
    // Reverse engineered from client.dll FUN_1808c0460:
    // fVar2 = clamp(1.0 + speed * 0.0005, 0.2, 1.0)
    const float speedRatio = std::clamp(1.0f + horizontalSpeed * 0.0005f, 0.2f, 1.0f);

    // Sub-tick continuous interpolation: earlier keypress within tick grants greater initial push
    const float subtickLead = (1.0f - std::clamp(subtickFraction, 0.0f, 1.0f)) * 0.6f;
    float subtickMultiplier = std::min(1.0f, speedRatio + subtickLead);

    // Stamina penalty scaling: clamp(1.0 - stamina / 100.0, 0.0, 1.0)
    if (services.m_flStamina > 0.0f) {
        const float staminaRatio = std::clamp(1.0f - (services.m_flStamina / 100.0f), 0.0f, 1.0f);
        subtickMultiplier *= staminaRatio;
    }

    // Half-gravity compensation during jump tick: sv_jump_impulse - (sv_gravity * 0.5 * 0.0078125)
    // 301.993378 - (800.0 * 0.5 * 0.0078125) = 298.868378
    constexpr float HALF_GRAVITY_JUMP_COMP = SV_GRAVITY * 0.5f * (TICK_INTERVAL * 0.5f);
    const float baseJumpImpulse = SV_JUMP_IMPULSE - HALF_GRAVITY_JUMP_COMP;

    return baseJumpImpulse * subtickMultiplier;
}

bool CPlayerMovement::CheckJumpButton(PlayerMovementServices& services, PlayerMoveData& mv, float /*dt*/) {
    if (!(mv.buttons & IN_JUMP)) {
        return false;
    }

    if (!mv.onGround) {
        return false;
    }

    // Extract exact sub-tick fraction from subtick input events if available
    float subtickFraction = 0.0f;
    for (const auto& step : mv.subtickSteps) {
        if (step.pressed && (step.button & IN_JUMP)) {
            subtickFraction = step.when;
            break;
        }
    }

    const float horizontalSpeed = std::sqrt(mv.velocity.x * mv.velocity.x + mv.velocity.y * mv.velocity.y);
    const float jumpVelZ = CalculateSubtickJumpVelocity(services, subtickFraction, horizontalSpeed);

    mv.velocity.z = jumpVelZ;
    mv.onGround = false;

    // Update internal movement service tracking fields
    services.m_bHasWalkMovedSinceLastJump = false;
    services.m_flLastJumpFrac = subtickFraction;
    services.m_flLastJumpVelocityZ = jumpVelZ;
    services.m_flHeightAtJumpStart = mv.origin.z;
    services.m_flStaminaAtJumpStart = services.m_flStamina;
    services.m_flVelMulAtJumpStart = jumpVelZ / SV_JUMP_IMPULSE;
    services.m_bJumpApexPending = true;

    // Apply CS2 jump stamina cost (recovers exponentially over subsequent ticks)
    services.m_flStamina = std::min(100.0f, services.m_flStamina + 40.0f);

    return true;
}

void CPlayerMovement::WalkMove(PlayerMovementServices& services, PlayerMoveData& mv, float dt) {
    // Sets m_bHasWalkMovedSinceLastJump = true (+0x0439)
    services.m_bHasWalkMovedSinceLastJump = true;

    // 1. Calculate view angles basis vectors (pitch zeroed for 2D ground movement)
    QAngle moveAngles;
    moveAngles.pitch = 0.0f;
    moveAngles.yaw   = mv.viewAngles.y;
    moveAngles.roll  = 0.0f;

    Vector3 forward, right, up;
    CViewRender::AngleVectors(moveAngles, &forward, &right, &up);

    // 2. Compute wish velocity vector
    Vector3 wishvel;
    wishvel.x = forward.x * mv.forwardMove + right.x * mv.sideMove;
    wishvel.y = forward.y * mv.forwardMove + right.y * mv.sideMove;
    wishvel.z = 0.0f;

    services.m_vecWalkWishVel = wishvel;

    float wishspeed = std::sqrt(wishvel.x * wishvel.x + wishvel.y * wishvel.y);
    Vector3 wishdir{0.0f, 0.0f, 0.0f};

    if (wishspeed > 0.0001f) {
        wishdir.x = wishvel.x / wishspeed;
        wishdir.y = wishvel.y / wishspeed;
    }

    // Clamp wishspeed to player maximum speed
    const float maxspeed = (mv.clientMaxSpeed > 0.0f) ? std::min(mv.maxSpeed, mv.clientMaxSpeed) : mv.maxSpeed;
    if (wishspeed > maxspeed) {
        wishspeed = maxspeed;
    }

    mv.wishSpeed = wishspeed;
    mv.wishDirection = wishdir;

    // 3. Apply ground friction
    Friction(services, mv, dt);

    // 4. Directional ground acceleration
    Accelerate(services, mv, wishdir, wishspeed, SV_ACCELERATE, dt);

    // Clamp vertical velocity to zero on ground
    mv.velocity.z = 0.0f;

    // 5. Add base velocity (conveyor belts, push triggers)
    mv.velocity.x += mv.baseVelocity.x;
    mv.velocity.y += mv.baseVelocity.y;

    // 6. Execute step move and obstacle traversal
    StepMove(services, mv, dt);

    // 7. Remove base velocity
    mv.velocity.x -= mv.baseVelocity.x;
    mv.velocity.y -= mv.baseVelocity.y;

    // 8. Re-categorize position and snap to floor
    CategorizePosition(services, mv, dt);
}

void CPlayerMovement::AirMove(PlayerMovementServices& services, PlayerMoveData& mv, float dt) {
    QAngle moveAngles;
    moveAngles.pitch = 0.0f;
    moveAngles.yaw   = mv.viewAngles.y;
    moveAngles.roll  = 0.0f;

    Vector3 forward, right, up;
    CViewRender::AngleVectors(moveAngles, &forward, &right, &up);

    Vector3 wishvel;
    wishvel.x = forward.x * mv.forwardMove + right.x * mv.sideMove;
    wishvel.y = forward.y * mv.forwardMove + right.y * mv.sideMove;
    wishvel.z = 0.0f;

    float wishspeed = std::sqrt(wishvel.x * wishvel.x + wishvel.y * wishvel.y);
    Vector3 wishdir{0.0f, 0.0f, 0.0f};

    if (wishspeed > 0.0001f) {
        wishdir.x = wishvel.x / wishspeed;
        wishdir.y = wishvel.y / wishspeed;
    }

    const float maxspeed = (mv.clientMaxSpeed > 0.0f) ? std::min(mv.maxSpeed, mv.clientMaxSpeed) : mv.maxSpeed;
    if (wishspeed > maxspeed) {
        wishspeed = maxspeed;
    }

    mv.wishSpeed = wishspeed;
    mv.wishDirection = wishdir;

    // 1. Airborne strafe acceleration
    AirAccelerate(services, mv, wishdir, wishspeed, SV_AIRACCELERATE, dt);

    // 2. Continuous sub-tick half-gravity integration (Verlet leapfrog)
    mv.velocity.z -= SV_GRAVITY * 0.5f * dt;

    // 3. Position integration
    mv.origin.x += mv.velocity.x * dt;
    mv.origin.y += mv.velocity.y * dt;
    mv.origin.z += mv.velocity.z * dt;

    // 4. Second half-gravity integration
    mv.velocity.z -= SV_GRAVITY * 0.5f * dt;

    // 5. Categorize position to test for landing
    CategorizePosition(services, mv, dt);
}

void CPlayerMovement::ProcessMovement(PlayerMovementServices& services, PlayerMoveData& mv, float dt) {
    // Stamina recovery
    if (services.m_flStamina > 0.0f) {
        services.m_flStamina = std::max(0.0f, services.m_flStamina - 20.0f * dt);
    }

    // Check if player is dead or immobilized
    if (mv.moveType == MOVETYPE_NONE) {
        mv.velocity = Vector3{0.0f, 0.0f, 0.0f};
        return;
    }

    // Noclip movement
    if (mv.moveType == MOVETYPE_NOCLIP) {
        QAngle moveAngles{mv.viewAngles.x, mv.viewAngles.y, mv.viewAngles.z};
        Vector3 forward, right, up;
        CViewRender::AngleVectors(moveAngles, &forward, &right, &up);

        mv.velocity.x = forward.x * mv.forwardMove + right.x * mv.sideMove;
        mv.velocity.y = forward.y * mv.forwardMove + right.y * mv.sideMove;
        mv.velocity.z = forward.z * mv.forwardMove + right.z * mv.sideMove + up.z * mv.upMove;

        mv.origin.x += mv.velocity.x * dt;
        mv.origin.y += mv.velocity.y * dt;
        mv.origin.z += mv.velocity.z * dt;
        return;
    }

    // Handle sub-tick slices if sub-tick input steps exist
    if (!mv.subtickSteps.empty()) {
        auto sortedSteps = mv.subtickSteps;
        std::sort(sortedSteps.begin(), sortedSteps.end(), [](const SubtickInputStep& a, const SubtickInputStep& b) {
            return a.when < b.when;
        });

        float currentFrac = 0.0f;
        for (const auto& step : sortedSteps) {
            const float sliceDt = (step.when - currentFrac) * dt;
            if (sliceDt > 0.0f) {
                if (mv.onGround) {
                    WalkMove(services, mv, sliceDt);
                } else {
                    AirMove(services, mv, sliceDt);
                }
            }

            // Apply instantaneous sub-tick button actions (e.g. analog jump)
            if (step.pressed && (step.button & IN_JUMP)) {
                mv.buttons |= IN_JUMP;
                CheckJumpButton(services, mv, dt);
            }

            currentFrac = step.when;
        }

        const float remainderDt = (1.0f - currentFrac) * dt;
        if (remainderDt > 0.0f) {
            if (mv.onGround) {
                WalkMove(services, mv, remainderDt);
            } else {
                AirMove(services, mv, remainderDt);
            }
        }
        return;
    }

    // Standard discrete tick execution:
    // 1. Initial jump check
    CheckJumpButton(services, mv, dt);

    // 2. Ground vs Airborne locomotion dispatch
    if (mv.onGround) {
        WalkMove(services, mv, dt);
    } else {
        AirMove(services, mv, dt);
    }
}

} // namespace cs2::client
