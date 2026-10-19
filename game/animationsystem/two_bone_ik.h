#ifndef CS2_TWO_BONE_IK_H
#define CS2_TWO_BONE_IK_H

#include <cmath>
#include <tuple>

namespace cs2::animationsystem {

struct Vector3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    Vector3 operator+(const Vector3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    [[nodiscard]] float Dot(const Vector3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] float LengthSqr() const noexcept { return Dot(*this); }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(LengthSqr()); }
    [[nodiscard]] Vector3 Cross(const Vector3& o) const noexcept {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    [[nodiscard]] Vector3 Normalized() const noexcept {
        float l = Length();
        return l > 1e-6f ? (*this) * (1.0f / l) : Vector3{0, 0, 0};
    }
};

struct IKSolveResult {
    Vector3 rootPos;
    Vector3 midPos;
    Vector3 effectorPos;
    bool reachedTarget{true};
};

/**
 * @brief Reconstructed Two-Bone IK solver from animationsystem.dll @ FUN_1802f9750.
 * Applied for player leg IK foot-planting and weapon procedural hand placement.
 */
class CTwoBoneIKSolver {
public:
    /**
     * @brief Solves analytical two-bone IK using the Law of Cosines and pole vector plane projection.
     * @param root Starting joint position (e.g. hip or shoulder)
     * @param target Desired end effector target (e.g. foot ground contact or weapon grip)
     * @param poleTarget Direction vector for the bending joint (knee or elbow forward)
     * @param len1 Upper bone length
     * @param len2 Lower bone length
     */
    static IKSolveResult Solve(
        const Vector3& root,
        const Vector3& target,
        const Vector3& poleTarget,
        float len1,
        float len2);
};

} // namespace cs2::animationsystem

#endif // CS2_TWO_BONE_IK_H
