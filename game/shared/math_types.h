#ifndef CS2_SHARED_MATH_TYPES_H
#define CS2_SHARED_MATH_TYPES_H

#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace cs2::shared {

// ----------------------------------------------------------------------------
// Vector2D
// ----------------------------------------------------------------------------
struct Vector2D {
    float x{0.0f}, y{0.0f};

    constexpr Vector2D() = default;
    constexpr Vector2D(float fx, float fy) : x(fx), y(fy) {}

    [[nodiscard]] constexpr Vector2D operator+(const Vector2D& o) const noexcept { return {x + o.x, y + o.y}; }
    [[nodiscard]] constexpr Vector2D operator-(const Vector2D& o) const noexcept { return {x - o.x, y - o.y}; }
    [[nodiscard]] constexpr Vector2D operator*(float s) const noexcept { return {x * s, y * s}; }
    [[nodiscard]] constexpr Vector2D operator/(float s) const noexcept { return {x / s, y / s}; }

    [[nodiscard]] constexpr float Dot(const Vector2D& o) const noexcept { return x * o.x + y * o.y; }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(x * x + y * y); }
};
static_assert(sizeof(Vector2D) == 8, "Vector2D must be 8 bytes");

// ----------------------------------------------------------------------------
// Vector3D (12 bytes, matching Source 2 core memory layout)
// ----------------------------------------------------------------------------
struct Vector3D {
    float x{0.0f}, y{0.0f}, z{0.0f};

    constexpr Vector3D() = default;
    constexpr Vector3D(float fx, float fy, float fz) : x(fx), y(fy), z(fz) {}

    [[nodiscard]] constexpr Vector3D operator+(const Vector3D& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    [[nodiscard]] constexpr Vector3D operator-(const Vector3D& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    [[nodiscard]] constexpr Vector3D operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    [[nodiscard]] constexpr Vector3D operator/(float s) const noexcept { return {x / s, y / s, z / s}; }

    constexpr Vector3D& operator+=(const Vector3D& o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vector3D& operator-=(const Vector3D& o) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr Vector3D& operator*=(float s) noexcept { x *= s; y *= s; z *= s; return *this; }

    [[nodiscard]] constexpr float Dot(const Vector3D& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(Dot(*this)); }
    [[nodiscard]] float LengthSq() const noexcept { return Dot(*this); }
    [[nodiscard]] float Length2D() const noexcept { return std::sqrt(x * x + y * y); }

    [[nodiscard]] Vector3D Cross(const Vector3D& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] Vector3D Normalized() const noexcept {
        const float len = Length();
        return (len > 1e-6f) ? (*this / len) : Vector3D{};
    }

    [[nodiscard]] bool IsZero(float tol = 1e-6f) const noexcept {
        return std::abs(x) < tol && std::abs(y) < tol && std::abs(z) < tol;
    }
};
static_assert(sizeof(Vector3D) == 12, "Vector3D must be 12 bytes");

// ----------------------------------------------------------------------------
// Vector4D (16 bytes)
// ----------------------------------------------------------------------------
struct alignas(16) Vector4D {
    float x{0.0f}, y{0.0f}, z{0.0f}, w{0.0f};

    constexpr Vector4D() = default;
    constexpr Vector4D(float fx, float fy, float fz, float fw = 0.0f) : x(fx), y(fy), z(fz), w(fw) {}
};
static_assert(sizeof(Vector4D) == 16, "Vector4D must be 16 bytes");

// ----------------------------------------------------------------------------
// QAngle (Euler angles in degrees: pitch, yaw, roll)
// ----------------------------------------------------------------------------
struct QAngle {
    float pitch{0.0f}; // X
    float yaw{0.0f};   // Y
    float roll{0.0f};  // Z

    constexpr QAngle() = default;
    constexpr QAngle(float p, float y, float r = 0.0f) : pitch(p), yaw(y), roll(r) {}

    [[nodiscard]] constexpr QAngle operator+(const QAngle& o) const noexcept { return {pitch + o.pitch, yaw + o.yaw, roll + o.roll}; }
    [[nodiscard]] constexpr QAngle operator-(const QAngle& o) const noexcept { return {pitch - o.pitch, yaw - o.yaw, roll - o.roll}; }
    [[nodiscard]] constexpr QAngle operator*(float s) const noexcept { return {pitch * s, yaw * s, roll * s}; }

    void Clamp() noexcept {
        pitch = std::clamp(pitch, -89.0f, 89.0f);
        while (yaw > 180.0f) yaw -= 360.0f;
        while (yaw < -180.0f) yaw += 360.0f;
        roll = 0.0f;
    }

    [[nodiscard]] Vector3D Forward() const noexcept {
        const float sp = std::sin(pitch * (3.14159265f / 180.0f));
        const float cp = std::cos(pitch * (3.14159265f / 180.0f));
        const float sy = std::sin(yaw * (3.14159265f / 180.0f));
        const float cy = std::cos(yaw * (3.14159265f / 180.0f));
        return {cp * cy, cp * sy, -sp};
    }
};
static_assert(sizeof(QAngle) == 12, "QAngle must be 12 bytes");

// ----------------------------------------------------------------------------
// VMatrix (64 bytes, alignas(16), 4x4 row-major transformation matrix)
// client.dll active view matrix global @ client.dll + 0x256cfe0
// ----------------------------------------------------------------------------
struct alignas(16) VMatrix {
    float m[4][4]{};

    constexpr VMatrix() = default;

    static VMatrix Identity() noexcept {
        VMatrix res;
        res.m[0][0] = 1.0f; res.m[1][1] = 1.0f; res.m[2][2] = 1.0f; res.m[3][3] = 1.0f;
        return res;
    }

    [[nodiscard]] const float* operator[](int i) const noexcept { return m[i]; }
    [[nodiscard]] float* operator[](int i) noexcept { return m[i]; }

    /**
     * @brief Transforms 3D world coordinate into 2D normalized screen space.
     * Homogeneous clipping: rejects vertices behind the camera (w < 0.001f).
     */
    [[nodiscard]] bool WorldToScreen(const Vector3D& world, Vector2D& screen, int screenWidth, int screenHeight) const noexcept {
        const float w = m[3][0] * world.x + m[3][1] * world.y + m[3][2] * world.z + m[3][3];
        if (w < 0.001f) return false;

        const float invW = 1.0f / w;
        const float ndcX = (m[0][0] * world.x + m[0][1] * world.y + m[0][2] * world.z + m[0][3]) * invW;
        const float ndcY = (m[1][0] * world.x + m[1][1] * world.y + m[1][2] * world.z + m[1][3]) * invW;

        screen.x = (screenWidth * 0.5f) + (ndcX * screenWidth * 0.5f);
        screen.y = (screenHeight * 0.5f) - (ndcY * screenHeight * 0.5f);
        return true;
    }
};
static_assert(sizeof(VMatrix) == 64, "VMatrix must be 64 bytes");

} // namespace cs2::shared

#endif // CS2_SHARED_MATH_TYPES_H
