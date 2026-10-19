#ifndef CS2_CLIENT_VIEWMATRIX_H
#define CS2_CLIENT_VIEWMATRIX_H

namespace cs2::client {

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct QAngle {
    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;
};

struct ViewMatrix {
    float m[4][4];
};

/**
 * @brief Reconstructed Camera & View Matrix Pipeline from client.dll
 * Reverse engineered from AngleVectors @ FUN_1817402a0 and WorldToScreen projection.
 */
class CViewRender {
public:
    static constexpr float AIM_PUNCH_MULTIPLIER = 2.0f; // CS2 recoil scalar

    // FUN_1817402a0
    static void AngleVectors(const QAngle& angles, Vector3* forward, Vector3* right = nullptr, Vector3* up = nullptr);

    static QAngle ApplyRecoilPunch(const QAngle& viewAngles, const QAngle& aimPunch);

    static bool WorldToScreen(const Vector3& world, float& screenX, float& screenY,
                             const ViewMatrix& matrix, int screenWidth, int screenHeight);
};

} // namespace cs2::client

#endif // CS2_CLIENT_VIEWMATRIX_H
