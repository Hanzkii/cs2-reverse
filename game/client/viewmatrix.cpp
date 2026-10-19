#include "viewmatrix.h"
#include <cmath>

namespace cs2::client {

constexpr float DEG2RAD = 3.14159265358979323846f / 180.0f;

void CViewRender::AngleVectors(const QAngle& angles, Vector3* forward, Vector3* right, Vector3* up) {
    const float sp = std::sin(angles.pitch * DEG2RAD);
    const float cp = std::cos(angles.pitch * DEG2RAD);
    const float sy = std::sin(angles.yaw * DEG2RAD);
    const float cy = std::cos(angles.yaw * DEG2RAD);
    const float sr = std::sin(angles.roll * DEG2RAD);
    const float cr = std::cos(angles.roll * DEG2RAD);

    if (forward) {
        forward->x = cp * cy;
        forward->y = cp * sy;
        forward->z = -sp;
    }

    if (right) {
        right->x = -1.0f * sr * sp * cy + -1.0f * cr * -sy;
        right->y = -1.0f * sr * sp * sy + -1.0f * cr * cy;
        right->z = -1.0f * sr * cp;
    }

    if (up) {
        up->x = cr * sp * cy + -sr * -sy;
        up->y = cr * sp * sy + -sr * cy;
        up->z = cr * cp;
    }
}

QAngle CViewRender::ApplyRecoilPunch(const QAngle& viewAngles, const QAngle& aimPunch) {
    QAngle eff;
    eff.pitch = viewAngles.pitch + aimPunch.pitch * AIM_PUNCH_MULTIPLIER;
    eff.yaw = viewAngles.yaw + aimPunch.yaw * AIM_PUNCH_MULTIPLIER;
    eff.roll = viewAngles.roll + aimPunch.roll * AIM_PUNCH_MULTIPLIER;
    return eff;
}

bool CViewRender::WorldToScreen(const Vector3& world, float& screenX, float& screenY,
                               const ViewMatrix& matrix, int screenWidth, int screenHeight) {
    const float w = matrix.m[3][0] * world.x + matrix.m[3][1] * world.y + matrix.m[3][2] * world.z + matrix.m[3][3];

    // Homogeneous frustum rejection
    if (w < 0.001f) {
        return false;
    }

    const float invw = 1.0f / w;
    const float x = (matrix.m[0][0] * world.x + matrix.m[0][1] * world.y + matrix.m[0][2] * world.z + matrix.m[0][3]) * invw;
    const float y = (matrix.m[1][0] * world.x + matrix.m[1][1] * world.y + matrix.m[1][2] * world.z + matrix.m[1][3]) * invw;

    screenX = (screenWidth * 0.5f) + (0.5f * x * screenWidth + 0.5f);
    screenY = (screenHeight * 0.5f) - (0.5f * y * screenHeight + 0.5f);
    return true;
}

} // namespace cs2::client
