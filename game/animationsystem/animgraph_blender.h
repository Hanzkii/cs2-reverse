#ifndef CS2_ANIMGRAPH_BLENDER_H
#define CS2_ANIMGRAPH_BLENDER_H

#include "two_bone_ik.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace cs2::animationsystem {

/**
 * @brief 4D Unit Quaternion for Source 2 rotation transforms.
 * Matches packed 16-byte representation in animationsystem.dll.
 */
struct Quaternion {
    float x{0.0f}, y{0.0f}, z{0.0f}, w{1.0f};

    constexpr Quaternion() noexcept = default;
    constexpr Quaternion(float _x, float _y, float _z, float _w) noexcept
        : x(_x), y(_y), z(_z), w(_w) {}

    [[nodiscard]] float Dot(const Quaternion& o) const noexcept {
        return x * o.x + y * o.y + z * o.z + w * o.w;
    }

    [[nodiscard]] float LengthSqr() const noexcept {
        return Dot(*this);
    }

    [[nodiscard]] float Length() const noexcept {
        return std::sqrt(LengthSqr());
    }

    [[nodiscard]] Quaternion Normalized() const noexcept {
        float len = Length();
        if (len > 1e-6f) {
            float inv = 1.0f / len;
            return {x * inv, y * inv, z * inv, w * inv};
        }
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }

    // Unary negation for shortest-arc quaternion alignment
    [[nodiscard]] Quaternion operator-() const noexcept {
        return {-x, -y, -z, -w};
    }

    [[nodiscard]] Quaternion operator+(const Quaternion& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z, w + o.w};
    }

    [[nodiscard]] Quaternion operator*(float s) const noexcept {
        return {x * s, y * s, z * s, w * s};
    }

    // Hamilton quaternion product
    [[nodiscard]] Quaternion operator*(const Quaternion& o) const noexcept {
        return {
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
            w * o.w - x * o.x - y * o.y - z * o.z
        };
    }

    /**
     * @brief Rotate a 3D vector by this quaternion.
     * Uses Rodrigues formula optimized for multiply-adds without trigonometric calls:
     *   t = 2 * (q.xyz cross v)
     *   v' = v + q.w * t + (q.xyz cross t)
     */
    [[nodiscard]] Vector3 RotateVector(const Vector3& v) const noexcept {
        Vector3 qv{x, y, z};
        Vector3 t = qv.Cross(v) * 2.0f;
        return v + (t * w) + qv.Cross(t);
    }

    /**
     * @brief Reconstructed from animationsystem.dll @ FUN_18038dcc0 (Quaternion_FromAngleAxis).
     * Computes unit quaternion from axis and angle: q = (axis * sin(theta/2), cos(theta/2)).
     */
    [[nodiscard]] static Quaternion FromAngleAxis(const Vector3& axis, float angleRad) noexcept;

    /**
     * @brief Normalized Linear Interpolation (Nlerp) with shortest-arc check.
     * If dot(q1, q2) < 0, q2 is negated to take the shortest geodesic arc.
     */
    [[nodiscard]] static Quaternion Nlerp(
        const Quaternion& q1,
        const Quaternion& q2,
        float t,
        bool shortestArc = true) noexcept;

    /**
     * @brief Spherical Linear Interpolation (Slerp) with shortest-arc check.
     * Reconstructed from animationsystem.dll sequence blend engine (0x1804721a0).
     */
    [[nodiscard]] static Quaternion Slerp(
        const Quaternion& q1,
        const Quaternion& q2,
        float t,
        bool shortestArc = true) noexcept;
};

/**
 * @brief Packed bone transform in parent or world coordinate space.
 * Size: 32 bytes (16 bytes rotation + 12 bytes translation + 4 bytes scale).
 */
struct alignas(16) BoneTransform {
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f}; // +0x00 (16 bytes)
    Vector3 translation{0.0f, 0.0f, 0.0f};        // +0x10 (12 bytes)
    float scale{1.0f};                            // +0x1C (4 bytes)

    /**
     * @brief Linearly interpolates bone translation & scale, and performs
     * shortest-arc Slerp/Nlerp on rotation.
     */
    [[nodiscard]] static BoneTransform Lerp(
        const BoneTransform& a,
        const BoneTransform& b,
        float t,
        bool useSlerp = false) noexcept;
};

/**
 * @brief Execution context passed through AnimGraph update nodes.
 * Reconstructed from animationsystem.dll CAnimUpdateContext.
 */
struct CAnimUpdateContext {
    float m_flCurrentTime{0.0f};
    float m_flDeltaTime{0.0f};
    float m_flPlaybackRate{1.0f};
};

/**
 * @brief Transition curve blending formulas for state transitions.
 */
enum class TransitionBlendType : uint8_t {
    Linear = 0,
    HermiteSmoothStep = 1
};

class TransitionBlendMath {
public:
    /// Linear crossfade weight w(t) = clamp(t, 0, 1)
    [[nodiscard]] static float Linear(float t) noexcept {
        return std::clamp(t, 0.0f, 1.0f);
    }

    /// Hermite cubic smoothstep crossfade weight: w(t) = 3t^2 - 2t^3
    [[nodiscard]] static float HermiteSmoothStep(float t) noexcept {
        float c = std::clamp(t, 0.0f, 1.0f);
        return c * c * (3.0f - 2.0f * c);
    }
};

/**
 * @brief Reconstructed State Machine Update Node from animationsystem.dll.
 * Associated VTable: 0x180687838.
 * Evaluates active transitions, elapsed cross-fade weight w(t), and state switches.
 */
class CStateMachineUpdateNode {
public:
    int m_nCurrentStateID{-1};
    int m_nTargetStateID{-1};
    float m_flTransitionStartTime{0.0f};
    float m_flTransitionDuration{0.0f};
    bool m_bIsTransitioning{false};
    TransitionBlendType m_eBlendType{TransitionBlendType::HermiteSmoothStep};

    CStateMachineUpdateNode() = default;

    /**
     * @brief Initiates a transition to a target state.
     */
    void StartTransition(
        int targetStateID,
        float startTime,
        float duration,
        TransitionBlendType blendType = TransitionBlendType::HermiteSmoothStep) noexcept;

    /**
     * @brief Reconstructed from animationsystem.dll @ FUN_180361670.
     * Evaluates normalized transition progress t in [0, 1].
     */
    [[nodiscard]] float EvaluateTransitionProgress(float currentTime) const noexcept;

    /**
     * @brief Evaluates active transition cross-fade weight w(t) in [0, 1].
     * Applies linear or Hermite smoothstep (3t^2 - 2t^3) interpolation.
     */
    [[nodiscard]] float EvaluateCrossfadeWeight(float currentTime) const noexcept;

    /**
     * @brief Updates state machine transitions using evaluation context.
     * Reconstructed from animationsystem.dll @ FUN_1802c82c0.
     */
    void Update(const CAnimUpdateContext& context) noexcept;
};

/**
 * @brief Reconstructed 1D Blend Node from animationsystem.dll.
 * Associated VTable: 0x18065bb80.
 * Computes interpolation weights and blends skeletal poses.
 */
class CBlendUpdateNode {
public:
    struct ChildWeight {
        int childIndex{-1};
        float weight{0.0f};
    };

    /**
     * @brief Reconstructed from animationsystem.dll @ FUN_1802d7a80.
     * Computes adjacent child indices and normalized linear blend weights from a continuous blend parameter.
     */
    [[nodiscard]] static std::pair<ChildWeight, ChildWeight> ComputeBlendWeights(
        float blendValue,
        int numChildren) noexcept;

    /**
     * @brief Blends two skeletal poses (bone arrays) using shortest-arc quaternion Slerp/Nlerp.
     */
    static void BlendPose(
        const std::vector<BoneTransform>& poseA,
        const std::vector<BoneTransform>& poseB,
        float weight,
        std::vector<BoneTransform>& outPose,
        bool useSlerp = false) noexcept;
};

} // namespace cs2::animationsystem

#endif // CS2_ANIMGRAPH_BLENDER_H
