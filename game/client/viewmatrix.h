#ifndef CS2_CLIENT_VIEWMATRIX_H
#define CS2_CLIENT_VIEWMATRIX_H

#include <cmath>

namespace cs2::client {

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vector3() noexcept = default;
    constexpr Vector3(float fx, float fy, float fz) noexcept : x(fx), y(fy), z(fz) {}

    Vector3 operator+(const Vector3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    Vector3 operator/(float s) const noexcept { return {x / s, y / s, z / s}; }

    [[nodiscard]] float Dot(const Vector3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] float LengthSqr() const noexcept { return Dot(*this); }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(LengthSqr()); }
    [[nodiscard]] Vector3 Normalized() const noexcept {
        float l = Length();
        return l > 1e-6f ? (*this) * (1.0f / l) : Vector3{0, 0, 0};
    }
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
