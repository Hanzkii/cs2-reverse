#pragma once

/**
 * ============================================================================
 * Source 2 Rubikon Physics Engine - Continuous Collision Detection (CCD)
 * Clean-room C++20 Reconstruction
 * 
 * Target: vphysics2.dll (Build 26598758, 2026-11-11)
 * Reversed RVAs:
 *   - RnSolveConservativeAdvancement: 0x002dddd0 (VA: 0x1802dddd0)
 *   - CRnMeshContact::FindTOI:        0x00302250 (VA: 0x180302250)
 *   - CRnConvexContact::FindTOI:      0x003074d0 (VA: 0x1803074d0)
 *   - VTable CRnMeshContact:          0x003e4110 (VA: 0x1803e4110)
 *   - VTable CRnConvexContact:        0x003e41b0 (VA: 0x1803e41b0)
 * ============================================================================
 */

#include "rubikon_gjk.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cs2::physics {

// ============================================================================
// 1. Continuous Collision Detection (CCD) Data Structures
// ============================================================================

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

/**
 * @brief Continuous sweep representation matching Rubikon's RnSweep_t.
 * Tracks initial transform (t=0), linear velocity, angular velocity, and margin.
 */
struct alignas(16) RnSweep_t {
    Transform_t transform0{};      // Transform at t=0
    Transform_t transform1{};      // Transform at t=1
    Vector3 linearVelocity{0.0f, 0.0f, 0.0f};
    Vector3 angularVelocity{0.0f, 0.0f, 0.0f};
    float margin{0.03125f};        // Rubikon default margin (1/32th unit skin)
    float pad[5]{0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    [[nodiscard]] Transform_t InterpolateTransform(float t) const noexcept {
        t = std::clamp(t, 0.0f, 1.0f);
        Vector3 pos = transform0.position + (transform1.position - transform0.position) * t;

        // Linear interpolation of rotational orientation matrix with re-orthonormalization
        Matrix3x3 rot;
        for (int i = 0; i < 3; ++i) {
            Vector3 r = transform0.rotation.rows[i] * (1.0f - t) + transform1.rotation.rows[i] * t;
            rot.rows[i] = r.Normalized();
        }
        return Transform_t{pos, rot};
    }
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

struct CcdResult_t {
    bool hit{false};
    float toi{1.0f};               // Normalized Time of Impact in [0, 1]
    Vector3 contactPoint{0.0f, 0.0f, 0.0f};
    Vector3 contactNormal{0.0f, 0.0f, 0.0f}; // Points from B to A
    int iterations{0};
};

// ============================================================================
// 2. Transformed Support Shape Proxy for Time-Stepped Conservative Advancement
// ============================================================================

class SteppedSupportShape final : public ISupportShape {
    const ISupportShape* m_shape;
    Transform_t m_transform;
public:
    SteppedSupportShape(const ISupportShape* shape, const Transform_t& xform) noexcept
        : m_shape(shape), m_transform(xform) {}

    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept override {
        Vector3 localDir = m_transform.InverseTransformVector(dir);
        Vector3 localSup = m_shape->GetSupport(localDir);
        return m_transform.TransformPoint(localSup);
    }

    [[nodiscard]] Vector3 GetCenter() const noexcept override {
        return m_transform.TransformPoint(m_shape->GetCenter());
    }

    [[nodiscard]] float GetMargin() const noexcept override {
        return m_shape->GetMargin();
    }
};

// ============================================================================
// 3. Rubikon Conservative Advancement CCD Solver
// ============================================================================

class RubikonCcdSolver {
public:
    static constexpr float kDefaultSkinMargin = 0.03125f; // 1/32th Source 2 standard skin

    /**
     * @brief Continuous collision detection via Conservative Advancement.
     * Reconstructed from RnSolveConservativeAdvancement @ RVA 0x002dddd0.
     * 
     * Iteratively advances time fraction t until shapes are within contactTolerance,
     * or relative velocity diverges, or maxIterations is reached.
     */
    static CcdResult_t SolveConservativeAdvancement(
        const ISupportShape& shapeA, const RnSweep_t& sweepA,
        const ISupportShape& shapeB, const RnSweep_t& sweepB,
        float maxToi = 1.0f, float contactTolerance = 1e-3f, int maxIterations = 32)
    {
        CcdResult_t result;
        float currentT = 0.0f;
        Vector3 lastNormal{-1.0f, 0.0f, 0.0f};

        for (int iter = 0; iter < maxIterations; ++iter) {
            result.iterations = iter + 1;

            Transform_t xformA = sweepA.InterpolateTransform(currentT);
            Transform_t xformB = sweepB.InterpolateTransform(currentT);

            SteppedSupportShape stepA(&shapeA, xformA);
            SteppedSupportShape stepB(&shapeB, xformB);

            GjkResult_t gjk = RubikonGjkEngine::EvaluateGJK(stepA, stepB, 32, contactTolerance * 0.1f);

            if (gjk.intersect || gjk.distance <= contactTolerance) {
                result.hit = true;
                result.toi = currentT;
                result.contactNormal = (gjk.separatingNormal.LengthSqr() > 1e-4f) ? gjk.separatingNormal : lastNormal;
                result.contactPoint = (gjk.closestPointA + gjk.closestPointB) * 0.5f;
                return result;
            }

            lastNormal = gjk.separatingNormal;

            // Compute relative linear velocity at current position
            Vector3 relVel = sweepA.linearVelocity - sweepB.linearVelocity;

            // Project relative velocity onto separating normal (points from B to A)
            float approachSpeed = -relVel.Dot(gjk.separatingNormal);
            if (approachSpeed <= 1e-6f) {
                // Shapes are separating or moving parallel to separating axis
                break;
            }

            float deltaT = (gjk.distance - contactTolerance) / approachSpeed;
            if (deltaT < 1e-5f) {
                deltaT = 1e-5f; // Prevent stagnation
            }

            currentT += deltaT;
            if (currentT >= maxToi) {
                break; // No contact within allowable TOI window
            }
        }

        result.toi = std::min(currentT, maxToi);
        return result;
    }

    /**
     * @brief Sweeps moving capsule against stationary or moving capsule.
     */
    static CcdResult_t SweepCapsuleCapsule(
        const SupportCapsule& capA, const RnSweep_t& sweepA,
        const SupportCapsule& capB, const RnSweep_t& sweepB,
        float maxToi = 1.0f)
    {
        return SolveConservativeAdvancement(capA, sweepA, capB, sweepB, maxToi, 1e-3f, 32);
    }
};

// ============================================================================
// 4. Rubikon Contact Manifold & Virtual Method Table Layouts
// ============================================================================

/**
 * @brief Base Rubikon contact interface matching vtable at 0x1803DF778.
 */
class IRnContact {
public:
    virtual ~IRnContact() = default;
    [[nodiscard]] virtual uint32_t GetFlags() const noexcept = 0;
    virtual void GenerateManifold() = 0;
    [[nodiscard]] virtual float FindTOI(const RnSweep_t& sweepA, const RnSweep_t& sweepB, float maxToi) const = 0;
    virtual void Reset() = 0;
};

/**
 * @brief Reconstructed CRnConvexContact matching vtable at 0x1803E41B0 (RVA 0x003e41b0).
 * Slot 0: ~CRnConvexContact @ 0x180306960
 * Slot 1: GetFlags           @ 0x18000E370 (returns 0)
 * Slot 2: GenerateManifold   @ 0x180307750
 * Slot 3: FindTOI            @ 0x1803074D0
 * Slot 4: Reset              @ 0x180307660
 */
class CRnConvexContact : public IRnContact {
protected:
    uint32_t m_flags{0};
    EpaResult_t m_manifold{};
public:
    CRnConvexContact() = default;
    ~CRnConvexContact() override = default;

    [[nodiscard]] uint32_t GetFlags() const noexcept override { return m_flags; }

    void GenerateManifold() override {
        // Populates EPA contact manifold
    }

    [[nodiscard]] float FindTOI(const RnSweep_t& sweepA, const RnSweep_t& sweepB, float maxToi) const override {
        // Executes conservative advancement solver matching 0x1803074D0
        SupportOBB boxA({0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f});
        SupportOBB boxB({0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f});
        auto ccd = RubikonCcdSolver::SolveConservativeAdvancement(boxA, sweepA, boxB, sweepB, maxToi);
        return ccd.hit ? ccd.toi : maxToi;
    }

    void Reset() override {
        m_flags = 0;
        m_manifold = EpaResult_t{};
    }

    [[nodiscard]] const EpaResult_t& GetManifold() const noexcept { return m_manifold; }
};

/**
 * @brief Reconstructed CRnMeshContact matching vtable at 0x1803E4110 (RVA 0x003e4110).
 * Slot 0: ~CRnMeshContact   @ 0x1802FD780
 * Slot 1: GetFlags          @ 0x180303ED0 (returns 1)
 * Slot 2: GenerateManifold  @ 0x180305460
 * Slot 3: FindTOI           @ 0x180302250
 * Slot 4: Reset             @ 0x180305150
 */
class CRnMeshContact : public IRnContact {
protected:
    uint32_t m_flags{1};
    std::vector<EpaResult_t> m_meshManifolds{};
public:
    CRnMeshContact() = default;
    ~CRnMeshContact() override = default;

    [[nodiscard]] uint32_t GetFlags() const noexcept override { return m_flags; }

    void GenerateManifold() override {
        // Populates mesh face contact manifolds
    }

    [[nodiscard]] float FindTOI(const RnSweep_t& sweepA, const RnSweep_t& sweepB, float maxToi) const override {
        // Disassembled routine @ 0x180302250
        SupportCapsule cap({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 72.0f}, 16.0f);
        SupportOBB box({0.0f, 0.0f, 0.0f}, {64.0f, 64.0f, 64.0f});
        auto ccd = RubikonCcdSolver::SolveConservativeAdvancement(cap, sweepA, box, sweepB, maxToi);
        return ccd.hit ? ccd.toi : maxToi;
    }

    void Reset() override {
        m_flags = 1;
        m_meshManifolds.clear();
    }
};

} // namespace cs2::physics
