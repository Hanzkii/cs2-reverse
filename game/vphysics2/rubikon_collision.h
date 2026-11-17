#ifndef CS2_RUBIKON_COLLISION_H
#define CS2_RUBIKON_COLLISION_H

#include <cstdint>
#include <cmath>
#include <optional>
#include <vector>
#include <algorithm>
#include <limits>

namespace cs2::vphysics2 {

// ============================================================================
// 1. Fundamental Linear Algebra & Spatial Geometry Primitives
// ============================================================================

struct Vector3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    constexpr Vector3() noexcept = default;
    constexpr Vector3(float _x, float _y, float _z) noexcept : x(_x), y(_y), z(_z) {}

    [[nodiscard]] static constexpr float Abs(float val) noexcept {
        return val < 0.0f ? -val : val;
    }

    [[nodiscard]] constexpr Vector3 operator+(const Vector3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    [[nodiscard]] constexpr Vector3 operator-(const Vector3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    [[nodiscard]] constexpr Vector3 operator-() const noexcept { return {-x, -y, -z}; }
    [[nodiscard]] constexpr Vector3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    [[nodiscard]] constexpr Vector3 operator/(float s) const noexcept { 
        float inv = (Abs(s) > 1e-8f) ? (1.0f / s) : 0.0f;
        return {x * inv, y * inv, z * inv}; 
    }

    constexpr Vector3& operator+=(const Vector3& o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vector3& operator-=(const Vector3& o) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr Vector3& operator*=(float s) noexcept { x *= s; y *= s; z *= s; return *this; }

    [[nodiscard]] constexpr bool operator==(const Vector3& o) const noexcept {
        return Abs(x - o.x) < 1e-6f && Abs(y - o.y) < 1e-6f && Abs(z - o.z) < 1e-6f;
    }
    [[nodiscard]] constexpr bool operator!=(const Vector3& o) const noexcept { return !(*this == o); }

    [[nodiscard]] constexpr float Dot(const Vector3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    
    [[nodiscard]] constexpr Vector3 Cross(const Vector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] float LengthSqr() const noexcept { return Dot(*this); }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(LengthSqr()); }
    
    [[nodiscard]] Vector3 Normalized() const noexcept {
        float l = Length();
        return l > 1e-7f ? (*this) * (1.0f / l) : Vector3{0.0f, 0.0f, 0.0f};
    }

    [[nodiscard]] bool IsZero(float eps = 1e-6f) const noexcept {
        return LengthSqr() < (eps * eps);
    }
};

struct Matrix3x3 {
    Vector3 rows[3]{
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };

    constexpr Matrix3x3() noexcept = default;
    constexpr Matrix3x3(const Vector3& r0, const Vector3& r1, const Vector3& r2) noexcept
        : rows{r0, r1, r2} {}

    [[nodiscard]] static Matrix3x3 FromColumns(const Vector3& c0, const Vector3& c1, const Vector3& c2) noexcept {
        return Matrix3x3{
            {c0.x, c1.x, c2.x},
            {c0.y, c1.y, c2.y},
            {c0.z, c1.z, c2.z}
        };
    }

    [[nodiscard]] static Matrix3x3 Identity() noexcept {
        return Matrix3x3{};
    }

    [[nodiscard]] static Matrix3x3 FromAxisAngle(const Vector3& axis, float angleRad) noexcept {
        Vector3 a = axis.Normalized();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        float t = 1.0f - c;

        return Matrix3x3{
            {t * a.x * a.x + c,       t * a.x * a.y - s * a.z, t * a.x * a.z + s * a.y},
            {t * a.x * a.y + s * a.z, t * a.y * a.y + c,       t * a.y * a.z - s * a.x},
            {t * a.x * a.z - s * a.y, t * a.y * a.z + s * a.x, t * a.z * a.z + c}
        };
    }

    [[nodiscard]] Vector3 Multiply(const Vector3& v) const noexcept {
        return {rows[0].Dot(v), rows[1].Dot(v), rows[2].Dot(v)};
    }

    [[nodiscard]] Vector3 TransposeMultiply(const Vector3& v) const noexcept {
        return {
            rows[0].x * v.x + rows[1].x * v.y + rows[2].x * v.z,
            rows[0].y * v.x + rows[1].y * v.y + rows[2].y * v.z,
            rows[0].z * v.x + rows[1].z * v.y + rows[2].z * v.z
        };
    }
};

struct Transform_t {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Matrix3x3 rotation{};

    constexpr Transform_t() noexcept = default;
    constexpr Transform_t(const Vector3& pos, const Matrix3x3& rot) noexcept
        : position(pos), rotation(rot) {}

    [[nodiscard]] Vector3 TransformPoint(const Vector3& pt) const noexcept {
        return position + rotation.Multiply(pt);
    }

    [[nodiscard]] Vector3 InverseTransformPoint(const Vector3& pt) const noexcept {
        return rotation.TransposeMultiply(pt - position);
    }

    [[nodiscard]] Vector3 TransformVector(const Vector3& vec) const noexcept {
        return rotation.Multiply(vec);
    }

    [[nodiscard]] Vector3 InverseTransformVector(const Vector3& vec) const noexcept {
        return rotation.TransposeMultiply(vec);
    }
};

struct Ray_t {
    Vector3 start{0.0f, 0.0f, 0.0f};
    Vector3 delta{0.0f, 0.0f, 0.0f};
};

struct AABB_t {
    Vector3 mins{0.0f, 0.0f, 0.0f};
    Vector3 maxs{0.0f, 0.0f, 0.0f};

    [[nodiscard]] bool Contains(const Vector3& pt) const noexcept {
        return pt.x >= mins.x && pt.x <= maxs.x &&
               pt.y >= mins.y && pt.y <= maxs.y &&
               pt.z >= mins.z && pt.z <= maxs.z;
    }

    [[nodiscard]] bool Overlaps(const AABB_t& o) const noexcept {
        return maxs.x >= o.mins.x && mins.x <= o.maxs.x &&
               maxs.y >= o.mins.y && mins.y <= o.maxs.y &&
               maxs.z >= o.mins.z && mins.z <= o.maxs.z;
    }

    [[nodiscard]] Vector3 GetCenter() const noexcept {
        return (mins + maxs) * 0.5f;
    }

    [[nodiscard]] Vector3 GetExtents() const noexcept {
        return (maxs - mins) * 0.5f;
    }

    [[nodiscard]] AABB_t Expand(float skin) const noexcept {
        return {
            {mins.x - skin, mins.y - skin, mins.z - skin},
            {maxs.x + skin, maxs.y + skin, maxs.z + skin}
        };
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

static_assert(sizeof(RubikonBvhNode_t) == 48, "RubikonBvhNode_t must be exactly 48 bytes matching vphysics2.dll SIMD layout");

// ============================================================================
// 2. Collision Shape Representations & Support Functions
// ============================================================================

struct RnSphere_t {
    Vector3 center{0.0f, 0.0f, 0.0f};
    float radius{0.0f};

    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept {
        return center + dir.Normalized() * radius;
    }
};

struct RnCapsule_t {
    Vector3 p0{0.0f, 0.0f, 0.0f};
    Vector3 p1{0.0f, 0.0f, 0.0f};
    float radius{0.0f};

    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept {
        float d0 = p0.Dot(dir);
        float d1 = p1.Dot(dir);
        Vector3 base = (d0 >= d1) ? p0 : p1;
        return base + dir.Normalized() * radius;
    }
};

struct RnOBB_t {
    Vector3 center{0.0f, 0.0f, 0.0f};
    Vector3 halfExtents{0.0f, 0.0f, 0.0f};
    Matrix3x3 rotation{};

    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept {
        Vector3 localDir = rotation.TransposeMultiply(dir);
        Vector3 localSupport{
            (localDir.x >= 0.0f) ? halfExtents.x : -halfExtents.x,
            (localDir.y >= 0.0f) ? halfExtents.y : -halfExtents.y,
            (localDir.z >= 0.0f) ? halfExtents.z : -halfExtents.z
        };
        return center + rotation.Multiply(localSupport);
    }
};

struct RnHull_t {
    std::vector<Vector3> vertices;
    Transform_t transform{};

    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept {
        if (vertices.empty()) return transform.position;

        Vector3 localDir = transform.InverseTransformVector(dir);
        float maxDot = -std::numeric_limits<float>::infinity();
        size_t bestIdx = 0;

        for (size_t i = 0; i < vertices.size(); ++i) {
            float d = vertices[i].Dot(localDir);
            if (d > maxDot) {
                maxDot = d;
                bestIdx = i;
            }
        }
        return transform.TransformPoint(vertices[bestIdx]);
    }
};

/**
 * @brief Abstract convex support mapping interface for GJK/EPA solver.
 */
class ISupportShape {
public:
    virtual ~ISupportShape() = default;
    [[nodiscard]] virtual Vector3 GetSupport(const Vector3& dir) const = 0;
    [[nodiscard]] virtual Vector3 GetCenter() const = 0;
};

class SupportSphere final : public ISupportShape {
    RnSphere_t m_sphere;
public:
    explicit SupportSphere(const RnSphere_t& s) noexcept : m_sphere(s) {}
    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept override { return m_sphere.GetSupport(dir); }
    [[nodiscard]] Vector3 GetCenter() const noexcept override { return m_sphere.center; }
};

class SupportCapsule final : public ISupportShape {
    RnCapsule_t m_capsule;
public:
    explicit SupportCapsule(const RnCapsule_t& c) noexcept : m_capsule(c) {}
    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept override { return m_capsule.GetSupport(dir); }
    [[nodiscard]] Vector3 GetCenter() const noexcept override { return (m_capsule.p0 + m_capsule.p1) * 0.5f; }
};

class SupportOBB final : public ISupportShape {
    RnOBB_t m_obb;
public:
    explicit SupportOBB(const RnOBB_t& obb) noexcept : m_obb(obb) {}
    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept override { return m_obb.GetSupport(dir); }
    [[nodiscard]] Vector3 GetCenter() const noexcept override { return m_obb.center; }
};

class SupportHull final : public ISupportShape {
    const RnHull_t* m_hull;
public:
    explicit SupportHull(const RnHull_t& hull) noexcept : m_hull(&hull) {}
    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept override { return m_hull->GetSupport(dir); }
    [[nodiscard]] Vector3 GetCenter() const noexcept override { return m_hull->transform.position; }
};

// ============================================================================
// 3. Minkowski Difference & Gilbert-Johnson-Keerthi (GJK) Data Structures
// ============================================================================

struct MinkowskiPoint_t {
    Vector3 v{0.0f, 0.0f, 0.0f}; // v = a - b
    Vector3 a{0.0f, 0.0f, 0.0f}; // Extremal witness on Shape A
    Vector3 b{0.0f, 0.0f, 0.0f}; // Extremal witness on Shape B
};

[[nodiscard]] inline MinkowskiPoint_t MinkowskiSupport(
    const ISupportShape& shapeA, const ISupportShape& shapeB, const Vector3& dir) noexcept {
    Vector3 supA = shapeA.GetSupport(dir);
    Vector3 supB = shapeB.GetSupport(-dir);
    return {supA - supB, supA, supB};
}

struct Simplex_t {
    MinkowskiPoint_t points[4]{};
    int count{0};

    void Push(const MinkowskiPoint_t& p) noexcept {
        for (int i = std::min(count, 3); i > 0; --i) {
            points[i] = points[i - 1];
        }
        points[0] = p;
        if (count < 4) ++count;
    }

    void Set1(const MinkowskiPoint_t& a) noexcept {
        points[0] = a;
        count = 1;
    }

    void Set2(const MinkowskiPoint_t& a, const MinkowskiPoint_t& b) noexcept {
        points[0] = a;
        points[1] = b;
        count = 2;
    }

    void Set3(const MinkowskiPoint_t& a, const MinkowskiPoint_t& b, const MinkowskiPoint_t& c) noexcept {
        points[0] = a;
        points[1] = b;
        points[2] = c;
        count = 3;
    }

    void Set4(const MinkowskiPoint_t& a, const MinkowskiPoint_t& b, const MinkowskiPoint_t& c, const MinkowskiPoint_t& d) noexcept {
        points[0] = a;
        points[1] = b;
        points[2] = c;
        points[3] = d;
        count = 4;
    }
};

struct GjkResult_t {
    bool intersect{false};
    float distance{0.0f};
    Vector3 closestPointA{0.0f, 0.0f, 0.0f};
    Vector3 closestPointB{0.0f, 0.0f, 0.0f};
    Vector3 separatingNormal{0.0f, 0.0f, 0.0f}; // Points from B to A
    int iterations{0};
    Simplex_t simplex{};
};

// ============================================================================
// 4. Expanding Polytope Algorithm (EPA) Data Structures
// ============================================================================

struct EpaResult_t {
    bool success{false};
    float penetrationDepth{0.0f};
    Vector3 contactNormal{0.0f, 0.0f, 0.0f}; // Outward contact normal from B to A
    Vector3 contactPointA{0.0f, 0.0f, 0.0f};
    Vector3 contactPointB{0.0f, 0.0f, 0.0f};
    int iterations{0};
};

// ============================================================================
// 5. Continuous Collision Detection (CCD) Data Structures
// ============================================================================

struct CcdResult_t {
    bool hit{false};
    float toi{1.0f}; // Normalized time-of-impact fraction in [0, 1]
    Vector3 contactPoint{0.0f, 0.0f, 0.0f};
    Vector3 contactNormal{0.0f, 0.0f, 0.0f};
    int iterations{0};
};

// ============================================================================
// 6. Rubikon Collision Filtering Bitmasks & Groups
// ============================================================================

namespace CollisionFlags {
    constexpr uint32_t CONTENTS_EMPTY            = 0;
    constexpr uint32_t CONTENTS_SOLID            = 0x1;
    constexpr uint32_t CONTENTS_WINDOW           = 0x2;
    constexpr uint32_t CONTENTS_AUX              = 0x4;
    constexpr uint32_t CONTENTS_GRATE            = 0x8;
    constexpr uint32_t CONTENTS_SLIME            = 0x10;
    constexpr uint32_t CONTENTS_WATER            = 0x20;
    constexpr uint32_t CONTENTS_BLOCKLOS         = 0x40;
    constexpr uint32_t CONTENTS_OPAQUE           = 0x80;
    constexpr uint32_t CONTENTS_MOVEABLE         = 0x4000;
    constexpr uint32_t CONTENTS_AREAPORTAL       = 0x8000;
    constexpr uint32_t CONTENTS_PLAYERCLIP       = 0x10000;
    constexpr uint32_t CONTENTS_MONSTERCLIP      = 0x20000;
    constexpr uint32_t CONTENTS_MONSTER          = 0x2000000;
    constexpr uint32_t CONTENTS_DEBRIS           = 0x4000000;
    constexpr uint32_t CONTENTS_DETAIL           = 0x8000000;
    constexpr uint32_t CONTENTS_HITBOX           = 0x40000000;

    constexpr uint32_t MASK_ALL                  = 0xFFFFFFFF;
    constexpr uint32_t MASK_SOLID                = (CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_GRATE);
    constexpr uint32_t MASK_PLAYERSOLID          = (CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_GRATE);
    constexpr uint32_t MASK_NPCSOLID             = (CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_MONSTERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_GRATE);
    constexpr uint32_t MASK_SHOT                 = (CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_MONSTER | CONTENTS_WINDOW | CONTENTS_DEBRIS | CONTENTS_HITBOX);
    constexpr uint32_t MASK_OPAQUE               = (CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_OPAQUE);
}

enum class CollisionGroup_t : uint32_t {
    NONE = 0,
    DEBRIS = 1,
    INTERACTIVE_DEBRIS = 2,
    INTERACTIVE = 3,
    PLAYER = 4,
    BREAKABLE_GLASS = 5,
    VEHICLE = 6,
    PLAYER_MOVEMENT = 7,
    NPC = 8,
    IN_AIR = 9,
    WEAPON = 10,
    PROJECTILE = 11,
    DOOR_BLOCKER = 12
};

// ============================================================================
// 7. Morton 3D Space-Filling Curve & Spatial Indexing
// ============================================================================

[[nodiscard]] constexpr uint32_t ExpandBits10(uint32_t v) noexcept {
    v &= 0x000003ff;
    v = (v | (v << 16)) & 0x030000ff;
    v = (v | (v << 8))  & 0x0300f00f;
    v = (v | (v << 4))  & 0x030c30c3;
    v = (v | (v << 2))  & 0x09249249;
    return v;
}

[[nodiscard]] constexpr uint32_t Morton3D(uint32_t x, uint32_t y, uint32_t z) noexcept {
    return ExpandBits10(x) | (ExpandBits10(y) << 1) | (ExpandBits10(z) << 2);
}

[[nodiscard]] inline uint32_t QuantizeCoordinate(float val, float minVal, float maxVal) noexcept {
    float extent = maxVal - minVal;
    if (extent < 1e-6f) return 0;
    float norm = std::clamp((val - minVal) / extent, 0.0f, 1.0f);
    return static_cast<uint32_t>(norm * 1023.0f);
}

[[nodiscard]] inline uint32_t ComputeMorton3D(const Vector3& pt, const AABB_t& worldBounds) noexcept {
    uint32_t qx = QuantizeCoordinate(pt.x, worldBounds.mins.x, worldBounds.maxs.x);
    uint32_t qy = QuantizeCoordinate(pt.y, worldBounds.mins.y, worldBounds.maxs.y);
    uint32_t qz = QuantizeCoordinate(pt.z, worldBounds.mins.z, worldBounds.maxs.z);
    return Morton3D(qx, qy, qz);
}

// ============================================================================
// 8. CRubikonCollision: Reconstructed Collision Solver API
// ============================================================================

class CRubikonCollision {
public:
    static constexpr float kSkinMargin = 0.03125f; // 1/32th of an inch Source 2 standard skin

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

    /**
     * @brief Gilbert-Johnson-Keerthi (GJK) convex collision & distance solver.
     */
    static GjkResult_t GJK_Evaluate(
        const ISupportShape& shapeA, const ISupportShape& shapeB,
        int maxIterations = 32, float tolerance = 1e-4f);

    /**
     * @brief Expanding Polytope Algorithm (EPA) for minimum penetration depth & normal extraction.
     */
    static EpaResult_t EPA_Evaluate(
        const ISupportShape& shapeA, const ISupportShape& shapeB,
        const Simplex_t& terminalSimplex, int maxIterations = 48, float tolerance = 1e-4f);

    /**
     * @brief Continuous Collision Detection (CCD) via Conservative Advancement.
     */
    static CcdResult_t ConservativeAdvancement(
        const ISupportShape& shapeA, const Vector3& linearVelA,
        const ISupportShape& shapeB, const Vector3& linearVelB,
        float dt = 1.0f, float contactTolerance = 1e-3f, int maxIterations = 32);

    /**
     * @brief Swept capsule continuous collision query against stationary or moving capsule.
     */
    static CcdResult_t SweepCapsuleAgainstCapsule(
        const RnCapsule_t& capAStart, const Vector3& velA,
        const RnCapsule_t& capBStart, const Vector3& velB,
        float dt = 1.0f);

    /**
     * @brief Rubikon collision filtering predicate.
     */
    static bool ShouldCollide(
        uint64_t maskA, uint64_t maskB,
        CollisionGroup_t groupA, CollisionGroup_t groupB) noexcept;
};

// ============================================================================
// 9. Morton BVH Accelerator Structure
// ============================================================================

class MortonBVH {
public:
    std::vector<RubikonBvhNode_t> m_nodes;
    AABB_t m_worldBounds;

    void Build(const std::vector<AABB_t>& primitiveBounds);
    void QueryAABB(const AABB_t& queryBox, std::vector<uint32_t>& outPrimitiveIndices) const;
    [[nodiscard]] RayCastResult TraceRay(const Ray_t& ray) const;
    [[nodiscard]] const std::vector<RubikonBvhNode_t>& GetNodes() const noexcept { return m_nodes; }
};

} // namespace cs2::vphysics2

#endif // CS2_RUBIKON_COLLISION_H
