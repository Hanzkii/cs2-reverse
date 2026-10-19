#ifndef CS2_RUBIKON_COLLISION_H
#define CS2_RUBIKON_COLLISION_H

#include <cstdint>
#include <cmath>
#include <optional>
#include <vector>

namespace cs2::vphysics2 {

struct Vector3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    Vector3 operator+(const Vector3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    [[nodiscard]] float Dot(const Vector3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] float LengthSqr() const noexcept { return Dot(*this); }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(LengthSqr()); }
    [[nodiscard]] Vector3 Normalized() const noexcept {
        float l = Length();
        return l > 1e-6f ? (*this) * (1.0f / l) : Vector3{0, 0, 0};
    }
};

struct Ray_t {
    Vector3 start;
    Vector3 delta;
};

struct AABB_t {
    Vector3 mins;
    Vector3 maxs;

    [[nodiscard]] bool Contains(const Vector3& pt) const noexcept {
        return pt.x >= mins.x && pt.x <= maxs.x &&
               pt.y >= mins.y && pt.y <= maxs.y &&
               pt.z >= mins.z && pt.z <= maxs.z;
    }
};

struct RayCastResult {
    bool hit{false};
    float fraction{1.0f};
    Vector3 normal{0.0f, 0.0f, 0.0f};
    Vector3 hitPoint{0.0f, 0.0f, 0.0f};
};

/**
 * @brief Rubikon 48-byte BVH Node matching vphysics2.dll SIMD accelerator @ 0x18000f4e0.
 */
struct alignas(16) RubikonBvhNode_t {
    Vector3 minBounds;
    uint32_t leftChild;
    Vector3 maxBounds;
    uint32_t rightChild;
    uint32_t flags;
    uint32_t primitiveIndex;
    uint32_t pad[2];
};

/**
 * @brief Reconstructed Rubikon Physics collision solver from vphysics2.dll.
 */
class CRubikonCollision {
public:
    // Valve Rubikon standard skin margin is 1/32th of an inch (0.03125)
    static constexpr float kSkinMargin = 0.03125f;

    /**
     * @brief Ray vs Box intersection with Rubikon skin expansion.
     */
    static RayCastResult IntersectRayAABB(const Ray_t& ray, const AABB_t& box, float skin = kSkinMargin);

    /**
     * @brief Swept sphere / capsule query against stationary capsule.
     */
    static RayCastResult SweepSphereAgainstCapsule(
        const Vector3& sphereStart, const Vector3& sphereEnd, float sphereRadius,
        const Vector3& capP0, const Vector3& capP1, float capRadius);

    /**
     * @brief Traverses a Rubikon BVH tree node array against a ray.
     */
    static std::optional<uint32_t> TraceRayBvh(const Ray_t& ray, const std::vector<RubikonBvhNode_t>& nodes);
};

} // namespace cs2::vphysics2

#endif // CS2_RUBIKON_COLLISION_H
