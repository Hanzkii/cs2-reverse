#pragma once

/**
 * ============================================================================
 * Source 2 Rubikon Physics Engine - Gilbert-Johnson-Keerthi (GJK) & EPA Solver
 * Clean-room C++20 Reconstruction
 * 
 * Target: vphysics2.dll (Build 26598758, 2026-11-11)
 * Reversed RVAs:
 *   - RnGJKDistanceSolver:     0x002d85c0 (VA: 0x1802d85c0)
 *   - CGJKProxy::GetVertex:    0x002dcf10 (VA: 0x1802dcf10)
 * ============================================================================
 */

#include <cmath>
#include <cstdint>
#include <vector>
#include <array>
#include <limits>
#include <algorithm>
#include <optional>

namespace cs2::physics {

// ============================================================================
// 1. Bit-Exact 3D Linear Algebra Primitives
// ============================================================================

struct Vector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr Vector3() noexcept = default;
    constexpr Vector3(float _x, float _y, float _z) noexcept : x(_x), y(_y), z(_z) {}

    [[nodiscard]] static constexpr float Abs(float v) noexcept {
        return v < 0.0f ? -v : v;
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

    [[nodiscard]] Vector3 Normalized(float eps = 1e-7f) const noexcept {
        float l = Length();
        return l > eps ? (*this) * (1.0f / l) : Vector3{0.0f, 0.0f, 0.0f};
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

    [[nodiscard]] static Matrix3x3 Identity() noexcept { return Matrix3x3{}; }

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

// ============================================================================
// 2. Convex Support Mapping Interfaces
// ============================================================================

class ISupportShape {
public:
    virtual ~ISupportShape() = default;
    [[nodiscard]] virtual Vector3 GetSupport(const Vector3& dir) const = 0;
    [[nodiscard]] virtual Vector3 GetCenter() const = 0;
    [[nodiscard]] virtual float GetMargin() const noexcept { return 0.0f; }
};

class SupportSphere final : public ISupportShape {
    Vector3 m_center{0.0f, 0.0f, 0.0f};
    float m_radius{0.0f};
public:
    SupportSphere(const Vector3& center, float radius) noexcept
        : m_center(center), m_radius(radius) {}

    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept override {
        return m_center + dir.Normalized() * m_radius;
    }
    [[nodiscard]] Vector3 GetCenter() const noexcept override { return m_center; }
    [[nodiscard]] float GetMargin() const noexcept override { return m_radius; }
};

class SupportCapsule final : public ISupportShape {
    Vector3 m_p0{0.0f, 0.0f, 0.0f};
    Vector3 m_p1{0.0f, 0.0f, 0.0f};
    float m_radius{0.0f};
public:
    SupportCapsule(const Vector3& p0, const Vector3& p1, float radius) noexcept
        : m_p0(p0), m_p1(p1), m_radius(radius) {}

    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept override {
        float d0 = m_p0.Dot(dir);
        float d1 = m_p1.Dot(dir);
        Vector3 base = (d0 >= d1) ? m_p0 : m_p1;
        return base + dir.Normalized() * m_radius;
    }
    [[nodiscard]] Vector3 GetCenter() const noexcept override { return (m_p0 + m_p1) * 0.5f; }
    [[nodiscard]] float GetMargin() const noexcept override { return m_radius; }
};

class SupportOBB final : public ISupportShape {
    Vector3 m_center{0.0f, 0.0f, 0.0f};
    Vector3 m_halfExtents{0.0f, 0.0f, 0.0f};
    Matrix3x3 m_rotation{};
public:
    SupportOBB(const Vector3& center, const Vector3& halfExtents, const Matrix3x3& rot = Matrix3x3::Identity()) noexcept
        : m_center(center), m_halfExtents(halfExtents), m_rotation(rot) {}

    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept override {
        Vector3 localDir = m_rotation.TransposeMultiply(dir);
        Vector3 localSupport{
            (localDir.x >= 0.0f) ? m_halfExtents.x : -m_halfExtents.x,
            (localDir.y >= 0.0f) ? m_halfExtents.y : -m_halfExtents.y,
            (localDir.z >= 0.0f) ? m_halfExtents.z : -m_halfExtents.z
        };
        return m_center + m_rotation.Multiply(localSupport);
    }
    [[nodiscard]] Vector3 GetCenter() const noexcept override { return m_center; }
};

class SupportConvexHull final : public ISupportShape {
    std::vector<Vector3> m_vertices;
    Transform_t m_transform{};
public:
    SupportConvexHull(std::vector<Vector3> vertices, const Transform_t& xform = Transform_t{})
        : m_vertices(std::move(vertices)), m_transform(xform) {}

    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept override {
        if (m_vertices.empty()) return m_transform.position;

        Vector3 localDir = m_transform.InverseTransformVector(dir);
        float maxDot = -std::numeric_limits<float>::infinity();
        size_t bestIdx = 0;

        for (size_t i = 0; i < m_vertices.size(); ++i) {
            float d = m_vertices[i].Dot(localDir);
            if (d > maxDot) {
                maxDot = d;
                bestIdx = i;
            }
        }
        return m_transform.TransformPoint(m_vertices[bestIdx]);
    }
    [[nodiscard]] Vector3 GetCenter() const noexcept override { return m_transform.position; }
};

/**
 * @brief Rubikon internal CGJKProxy layout reconstructed from vphysics2.dll.
 * Reconstructed from RVA 0x002dcf10:
 *   [rcx + 0x08] = vertex array pointer
 *   [rcx + 0x10] = scale multiplier
 *   [rcx + 0x14] = uint8_t isBox
 *   [rcx + 0x18] = min/max box coordinates for 3 axes
 */
struct CGJKProxy {
    const Vector3* vertices{nullptr};
    uint32_t vertexCount{0};
    float scale{1.0f};
    uint8_t isBox{0};
    uint8_t pad[3]{0, 0, 0};
    float boxCoords[6]{-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f}; // {minX, maxX, minY, maxY, minZ, maxZ}

    /**
     * @brief Bit-exact implementation of CGJKProxy::GetVertex @ RVA 0x002dcf10.
     * Uses bitwise shifts to resolve 8 box vertices or indexed vertex array multiplication.
     */
    void GetVertex(int vertexIndex, Vector3* outVertex) const noexcept {
        if (!outVertex) return;
        if (isBox) {
            int ix = vertexIndex & 1;
            int iy = (vertexIndex >> 1) & 1;
            int iz = (vertexIndex >> 2) & 1;
            outVertex->x = boxCoords[ix];
            outVertex->y = boxCoords[2 + iy];
            outVertex->z = boxCoords[4 + iz];
        } else if (vertices && vertexIndex >= 0 && static_cast<uint32_t>(vertexIndex) < vertexCount) {
            outVertex->x = vertices[vertexIndex].x * scale;
            outVertex->y = vertices[vertexIndex].y * scale;
            outVertex->z = vertices[vertexIndex].z * scale;
        } else {
            *outVertex = Vector3{0.0f, 0.0f, 0.0f};
        }
    }
};

// ============================================================================
// 3. Minkowski Difference & Simplex Solver
// ============================================================================

struct MinkowskiPoint_t {
    Vector3 v{0.0f, 0.0f, 0.0f}; // Difference vector v = a - b
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
    std::array<MinkowskiPoint_t, 4> points{};
    int count{0};

    void Clear() noexcept { count = 0; }

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

struct EpaResult_t {
    bool success{false};
    float penetrationDepth{0.0f};
    Vector3 contactNormal{0.0f, 0.0f, 0.0f}; // Outward contact normal pointing from B to A
    Vector3 contactPointA{0.0f, 0.0f, 0.0f};
    Vector3 contactPointB{0.0f, 0.0f, 0.0f};
    int iterations{0};
};

// ============================================================================
// 4. Rubikon Simplex Voronoi Solver & GJK Engine
// ============================================================================

class RubikonGjkEngine {
public:
    static void ComputeBarycentric(
        const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& p,
        float& u, float& v, float& w) noexcept
    {
        Vector3 v0 = b - a;
        Vector3 v1 = c - a;
        Vector3 v2 = p - a;

        float d00 = v0.Dot(v0);
        float d01 = v0.Dot(v1);
        float d11 = v1.Dot(v1);
        float d20 = v2.Dot(v0);
        float d21 = v2.Dot(v1);

        float denom = d00 * d11 - d01 * d01;
        if (std::abs(denom) > 1e-8f) {
            v = (d11 * d20 - d01 * d21) / denom;
            w = (d00 * d21 - d01 * d20) / denom;
            u = 1.0f - v - w;

            u = std::clamp(u, 0.0f, 1.0f);
            v = std::clamp(v, 0.0f, 1.0f);
            w = std::clamp(w, 0.0f, 1.0f);
            float sum = u + v + w;
            if (sum > 1e-6f) {
                u /= sum;
                v /= sum;
                w /= sum;
            } else {
                u = 1.0f / 3.0f; v = 1.0f / 3.0f; w = 1.0f / 3.0f;
            }
        } else {
            u = 1.0f / 3.0f; v = 1.0f / 3.0f; w = 1.0f / 3.0f;
        }
    }

    /**
     * @brief Computes closest point to origin on 1-simplex, 2-simplex, or 3-simplex
     * using exact Voronoi feature region decomposition (Christer Ericson RTCD).
     */
    static void SolveSimplex(
        Simplex_t& simplex, Vector3& outClosest,
        Vector3& outClosestA, Vector3& outClosestB, bool& outEnclosesOrigin) noexcept
    {
        outEnclosesOrigin = false;

        if (simplex.count == 1) {
            outClosest = simplex.points[0].v;
            outClosestA = simplex.points[0].a;
            outClosestB = simplex.points[0].b;
            return;
        }

        if (simplex.count == 2) {
            const MinkowskiPoint_t a = simplex.points[0];
            const MinkowskiPoint_t b = simplex.points[1];
            Vector3 ab = b.v - a.v;
            float d1 = -a.v.Dot(ab);
            float d2 = -b.v.Dot(ab);

            if (d1 <= 0.0f) {
                outClosest = a.v;
                outClosestA = a.a;
                outClosestB = a.b;
                simplex.Set1(a);
                return;
            }
            if (d2 >= 0.0f) {
                outClosest = b.v;
                outClosestA = b.a;
                outClosestB = b.b;
                simplex.Set1(b);
                return;
            }

            float t = d1 / (d1 - d2);
            outClosest = a.v + ab * t;
            outClosestA = a.a + (b.a - a.a) * t;
            outClosestB = a.b + (b.b - a.b) * t;
            return;
        }

        if (simplex.count == 3) {
            const MinkowskiPoint_t a = simplex.points[0];
            const MinkowskiPoint_t b = simplex.points[1];
            const MinkowskiPoint_t c = simplex.points[2];

            Vector3 ab = b.v - a.v;
            Vector3 ac = c.v - a.v;

            // Check vertex region A
            float d1 = -a.v.Dot(ab);
            float d2 = -a.v.Dot(ac);
            if (d1 <= 0.0f && d2 <= 0.0f) {
                outClosest = a.v;
                outClosestA = a.a;
                outClosestB = a.b;
                simplex.Set1(a);
                return;
            }

            // Check vertex region B
            float d3 = -b.v.Dot(ab);
            float d4 = -b.v.Dot(ac);
            if (d3 >= 0.0f && d4 <= d3) {
                outClosest = b.v;
                outClosestA = b.a;
                outClosestB = b.b;
                simplex.Set1(b);
                return;
            }

            // Check edge region AB
            float vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
                float v = d1 / (d1 - d3);
                outClosest = a.v + ab * v;
                outClosestA = a.a + (b.a - a.a) * v;
                outClosestB = a.b + (b.b - a.b) * v;
                simplex.Set2(a, b);
                return;
            }

            // Check vertex region C
            float d5 = -c.v.Dot(ab);
            float d6 = -c.v.Dot(ac);
            if (d6 >= 0.0f && d5 <= d6) {
                outClosest = c.v;
                outClosestA = c.a;
                outClosestB = c.b;
                simplex.Set1(c);
                return;
            }

            // Check edge region AC
            float vb = d5 * d2 - d1 * d6;
            if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
                float w = d2 / (d2 - d6);
                outClosest = a.v + ac * w;
                outClosestA = a.a + (c.a - a.a) * w;
                outClosestB = a.b + (c.b - a.b) * w;
                simplex.Set2(a, c);
                return;
            }

            // Check edge region BC
            float va = d3 * d6 - d5 * d4;
            if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
                float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                Vector3 bc = c.v - b.v;
                outClosest = b.v + bc * w;
                outClosestA = b.a + (c.a - b.a) * w;
                outClosestB = b.b + (c.b - b.b) * w;
                simplex.Set2(b, c);
                return;
            }

            // Face region ABC
            float denom = 1.0f / (va + vb + vc);
            float v = vb * denom;
            float w = vc * denom;
            float u = 1.0f - v - w;

            outClosest = a.v * u + b.v * v + c.v * w;
            outClosestA = a.a * u + b.a * v + c.a * w;
            outClosestB = a.b * u + b.b * v + c.b * w;
            return;
        }

        if (simplex.count == 4) {
            const MinkowskiPoint_t a = simplex.points[0];
            const MinkowskiPoint_t b = simplex.points[1];
            const MinkowskiPoint_t c = simplex.points[2];
            const MinkowskiPoint_t d = simplex.points[3];

            Vector3 ab = b.v - a.v;
            Vector3 ac = c.v - a.v;
            Vector3 ad = d.v - a.v;

            Vector3 nABC = ab.Cross(ac);
            if (nABC.Dot(ad) > 0.0f) nABC = -nABC;

            Vector3 nACD = ac.Cross(ad);
            if (nACD.Dot(ab) > 0.0f) nACD = -nACD;

            Vector3 nADB = ad.Cross(ab);
            if (nADB.Dot(ac) > 0.0f) nADB = -nADB;

            Vector3 nBDC = (d.v - b.v).Cross(c.v - b.v);
            if (nBDC.Dot(a.v - b.v) > 0.0f) nBDC = -nBDC;

            Vector3 ao = -a.v;
            Vector3 bo = -b.v;

            bool outABC = nABC.Dot(ao) > 1e-5f;
            bool outACD = nACD.Dot(ao) > 1e-5f;
            bool outADB = nADB.Dot(ao) > 1e-5f;
            bool outBDC = nBDC.Dot(bo) > 1e-5f;

            if (!outABC && !outACD && !outADB && !outBDC) {
                outEnclosesOrigin = true;
                outClosest = Vector3{0.0f, 0.0f, 0.0f};
                outClosestA = (a.a + b.a + c.a + d.a) * 0.25f;
                outClosestB = (a.b + b.b + c.b + d.b) * 0.25f;
                return;
            }

            if (outABC) simplex.Set3(a, b, c);
            else if (outACD) simplex.Set3(a, c, d);
            else if (outADB) simplex.Set3(a, d, b);
            else simplex.Set3(b, d, c);

            SolveSimplex(simplex, outClosest, outClosestA, outClosestB, outEnclosesOrigin);
        }
    }

    /**
     * @brief Gilbert-Johnson-Keerthi (GJK) distance & intersection solver.
     */
    static GjkResult_t EvaluateGJK(
        const ISupportShape& shapeA, const ISupportShape& shapeB,
        int maxIterations = 32, float tolerance = 1e-4f)
    {
        GjkResult_t result;
        Vector3 initialDir = shapeA.GetCenter() - shapeB.GetCenter();
        if (initialDir.LengthSqr() < 1e-6f) {
            initialDir = Vector3{1.0f, 0.0f, 0.0f};
        }

        MinkowskiPoint_t p0 = MinkowskiSupport(shapeA, shapeB, initialDir);
        result.simplex.Set1(p0);

        Vector3 closestPt = p0.v;
        Vector3 closestA = p0.a;
        Vector3 closestB = p0.b;
        bool encloses = false;

        for (int iter = 0; iter < maxIterations; ++iter) {
            result.iterations = iter + 1;
            SolveSimplex(result.simplex, closestPt, closestA, closestB, encloses);

            if (encloses || closestPt.LengthSqr() < (tolerance * tolerance)) {
                result.intersect = true;
                result.distance = 0.0f;
                result.closestPointA = closestA;
                result.closestPointB = closestB;
                result.separatingNormal = initialDir.Normalized() * -1.0f;
                return result;
            }

            float currentDist = closestPt.Length();
            Vector3 searchDir = closestPt * (-1.0f / currentDist);
            MinkowskiPoint_t newPt = MinkowskiSupport(shapeA, shapeB, searchDir);

            float delta = currentDist + newPt.v.Dot(searchDir);
            if (delta <= tolerance || (newPt.v - closestPt).LengthSqr() < (tolerance * tolerance)) {
                result.intersect = false;
                result.distance = currentDist;
                result.closestPointA = closestA;
                result.closestPointB = closestB;
                result.separatingNormal = searchDir * -1.0f;
                return result;
            }

            result.simplex.Push(newPt);
        }

        result.intersect = encloses;
        result.distance = closestPt.Length();
        result.closestPointA = closestA;
        result.closestPointB = closestB;
        result.separatingNormal = (result.distance > 1e-6f) ? (closestPt * (1.0f / result.distance)) : Vector3{-1.0f, 0.0f, 0.0f};
        return result;
    }

    /**
     * @brief Expanding Polytope Algorithm (EPA) for contact manifold penetration depth & normal.
     */
    static EpaResult_t EvaluateEPA(
        const ISupportShape& shapeA, const ISupportShape& shapeB,
        int maxIterations = 48, float tolerance = 1e-4f)
    {
        EpaResult_t result;

        struct EpaFace_t {
            uint32_t i0, i1, i2;
            Vector3 normal;
            float dist;
            bool obsolete;
        };

        struct EpaEdge_t {
            uint32_t i0, i1;
        };

        // 6 cardinal directions to initialize octahedron
        const Vector3 cardinalDirs[6] = {
            { 1.0f,  0.0f,  0.0f},
            {-1.0f,  0.0f,  0.0f},
            { 0.0f,  1.0f,  0.0f},
            { 0.0f, -1.0f,  0.0f},
            { 0.0f,  0.0f,  1.0f},
            { 0.0f,  0.0f, -1.0f}
        };

        std::vector<MinkowskiPoint_t> polytope;
        polytope.reserve(96);

        for (const auto& dir : cardinalDirs) {
            polytope.push_back(MinkowskiSupport(shapeA, shapeB, dir));
        }

        const uint32_t initialTriangles[8][3] = {
            {0, 2, 4}, {0, 5, 2}, {0, 4, 3}, {0, 3, 5},
            {1, 4, 2}, {1, 2, 5}, {1, 3, 4}, {1, 5, 3}
        };

        std::vector<EpaFace_t> faces;
        faces.reserve(128);

        for (const auto& tri : initialTriangles) {
            uint32_t a = tri[0], b = tri[1], c = tri[2];
            Vector3 e1 = polytope[b].v - polytope[a].v;
            Vector3 e2 = polytope[c].v - polytope[a].v;
            Vector3 n = e1.Cross(e2);
            float len = n.Length();
            if (len < 1e-6f) continue;
            n = n * (1.0f / len);
            float d = n.Dot(polytope[a].v);
            if (d < 0.0f) {
                n = -n;
                d = -d;
                std::swap(b, c);
            }
            faces.push_back({a, b, c, n, d, false});
        }

        for (int iter = 0; iter < maxIterations; ++iter) {
            result.iterations = iter + 1;

            float minDist = std::numeric_limits<float>::infinity();
            int bestFaceIdx = -1;

            for (size_t f = 0; f < faces.size(); ++f) {
                if (!faces[f].obsolete && faces[f].dist < minDist) {
                    minDist = faces[f].dist;
                    bestFaceIdx = static_cast<int>(f);
                }
            }

            if (bestFaceIdx == -1) break;

            const auto& bestFace = faces[bestFaceIdx];
            MinkowskiPoint_t newPt = MinkowskiSupport(shapeA, shapeB, bestFace.normal);
            float d = newPt.v.Dot(bestFace.normal);

            if ((d - bestFace.dist) < tolerance) {
                result.success = true;
                result.penetrationDepth = bestFace.dist;
                result.contactNormal = bestFace.normal * -1.0f; // Points from B to A

                Vector3 proj = bestFace.normal * bestFace.dist;
                float u = 0.0f, v = 0.0f, w = 0.0f;
                ComputeBarycentric(polytope[bestFace.i0].v, polytope[bestFace.i1].v, polytope[bestFace.i2].v, proj, u, v, w);

                result.contactPointA = polytope[bestFace.i0].a * u + polytope[bestFace.i1].a * v + polytope[bestFace.i2].a * w;
                result.contactPointB = polytope[bestFace.i0].b * u + polytope[bestFace.i1].b * v + polytope[bestFace.i2].b * w;
                return result;
            }

            uint32_t newVertexIdx = static_cast<uint32_t>(polytope.size());
            polytope.push_back(newPt);

            std::vector<EpaEdge_t> horizon;
            auto AddHorizonEdge = [&](uint32_t ea, uint32_t eb) {
                for (auto it = horizon.begin(); it != horizon.end(); ++it) {
                    if (it->i0 == eb && it->i1 == ea) {
                        horizon.erase(it);
                        return;
                    }
                }
                horizon.push_back({ea, eb});
            };

            for (size_t f = 0; f < faces.size(); ++f) {
                if (faces[f].obsolete) continue;
                if (faces[f].normal.Dot(newPt.v - polytope[faces[f].i0].v) > 1e-5f) {
                    faces[f].obsolete = true;
                    AddHorizonEdge(faces[f].i0, faces[f].i1);
                    AddHorizonEdge(faces[f].i1, faces[f].i2);
                    AddHorizonEdge(faces[f].i2, faces[f].i0);
                }
            }

            if (horizon.empty()) {
                result.success = true;
                result.penetrationDepth = bestFace.dist;
                result.contactNormal = bestFace.normal * -1.0f;
                Vector3 proj = bestFace.normal * bestFace.dist;
                float u = 0.0f, v = 0.0f, w = 0.0f;
                ComputeBarycentric(polytope[bestFace.i0].v, polytope[bestFace.i1].v, polytope[bestFace.i2].v, proj, u, v, w);
                result.contactPointA = polytope[bestFace.i0].a * u + polytope[bestFace.i1].a * v + polytope[bestFace.i2].a * w;
                result.contactPointB = polytope[bestFace.i0].b * u + polytope[bestFace.i1].b * v + polytope[bestFace.i2].b * w;
                return result;
            }

            for (const auto& edge : horizon) {
                Vector3 e1 = polytope[edge.i1].v - polytope[edge.i0].v;
                Vector3 e2 = newPt.v - polytope[edge.i0].v;
                Vector3 n = e1.Cross(e2);
                float len = n.Length();
                if (len < 1e-6f) continue;
                n = n * (1.0f / len);
                float dist = n.Dot(polytope[edge.i0].v);
                if (dist < 0.0f) {
                    n = -n;
                    dist = -dist;
                    faces.push_back({edge.i1, edge.i0, newVertexIdx, n, dist, false});
                } else {
                    faces.push_back({edge.i0, edge.i1, newVertexIdx, n, dist, false});
                }
            }
        }

        // Return best face if max iterations reached
        float minDist = std::numeric_limits<float>::infinity();
        int bestFaceIdx = -1;
        for (size_t f = 0; f < faces.size(); ++f) {
            if (!faces[f].obsolete && faces[f].dist < minDist) {
                minDist = faces[f].dist;
                bestFaceIdx = static_cast<int>(f);
            }
        }
        if (bestFaceIdx >= 0) {
            const auto& bestFace = faces[bestFaceIdx];
            result.success = true;
            result.penetrationDepth = bestFace.dist;
            result.contactNormal = bestFace.normal * -1.0f;
            Vector3 proj = bestFace.normal * bestFace.dist;
            float u = 0.0f, v = 0.0f, w = 0.0f;
            ComputeBarycentric(polytope[bestFace.i0].v, polytope[bestFace.i1].v, polytope[bestFace.i2].v, proj, u, v, w);
            result.contactPointA = polytope[bestFace.i0].a * u + polytope[bestFace.i1].a * v + polytope[bestFace.i2].a * w;
            result.contactPointB = polytope[bestFace.i0].b * u + polytope[bestFace.i1].b * v + polytope[bestFace.i2].b * w;
        }

        return result;
    }
};

} // namespace cs2::physics
