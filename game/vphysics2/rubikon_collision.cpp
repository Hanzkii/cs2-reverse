#include "rubikon_collision.h"
#include <algorithm>
#include <limits>
#include <vector>

namespace cs2::vphysics2 {

// ============================================================================
// Internal Helpers & Linear Algebra Utilities
// ============================================================================

namespace {

inline Vector3 VectorMin(const Vector3& a, const Vector3& b) noexcept {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

inline Vector3 VectorMax(const Vector3& a, const Vector3& b) noexcept {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

inline void ComputeTriangleBarycentric(
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

inline Vector3 FindOrthogonal(const Vector3& v) noexcept {
    if (std::abs(v.x) >= 0.57735027f) {
        return Vector3{v.y, -v.x, 0.0f}.Normalized();
    }
    return Vector3{0.0f, v.z, -v.y}.Normalized();
}

} // anonymous namespace

// ============================================================================
// 1. Ray & AABB Bounding Volume Intersections
// ============================================================================

RayCastResult CRubikonCollision::IntersectRayAABB(const Ray_t& ray, const AABB_t& box, float skin) {
    RayCastResult res;

    const Vector3 boxMin = {box.mins.x - skin, box.mins.y - skin, box.mins.z - skin};
    const Vector3 boxMax = {box.maxs.x + skin, box.maxs.y + skin, box.maxs.z + skin};

    float tmin = 0.0f;
    float tmax = 1.0f;
    Vector3 hitNormal{0.0f, 0.0f, 0.0f};

    // Polyhedral slab test across 3 primary axes
    for (int i = 0; i < 3; ++i) {
        const float rayStart = (i == 0) ? ray.start.x : (i == 1 ? ray.start.y : ray.start.z);
        const float rayDelta = (i == 0) ? ray.delta.x : (i == 1 ? ray.delta.y : ray.delta.z);
        const float bMin = (i == 0) ? boxMin.x : (i == 1 ? boxMin.y : boxMin.z);
        const float bMax = (i == 0) ? boxMax.x : (i == 1 ? boxMax.y : boxMax.z);

        if (std::abs(rayDelta) < 1e-8f) {
            if (rayStart < bMin || rayStart > bMax) {
                return res; // Ray is parallel and strictly outside the slab
            }
        } else {
            const float ood = 1.0f / rayDelta;
            float t1 = (bMin - rayStart) * ood;
            float t2 = (bMax - rayStart) * ood;
            float sign = -1.0f;

            if (t1 > t2) {
                std::swap(t1, t2);
                sign = 1.0f;
            }

            if (t1 > tmin) {
                tmin = t1;
                hitNormal = {0.0f, 0.0f, 0.0f};
                if (i == 0) hitNormal.x = sign;
                else if (i == 1) hitNormal.y = sign;
                else hitNormal.z = sign;
            }
            tmax = std::min(tmax, t2);

            if (tmin > tmax) {
                return res;
            }
        }
    }

    if (tmin >= 0.0f && tmin <= 1.0f) {
        res.hit = true;
        res.fraction = tmin;
        res.normal = hitNormal;
        res.hitPoint = ray.start + (ray.delta * tmin);
    }

    return res;
}

RayCastResult CRubikonCollision::SweepSphereAgainstCapsule(
    const Vector3& sphereStart, const Vector3& sphereEnd, float sphereRadius,
    const Vector3& capP0, const Vector3& capP1, float capRadius)
{
    RayCastResult res;
    const float combinedRadius = sphereRadius + capRadius;
    const Vector3 rayDelta = sphereEnd - sphereStart;

    Vector3 capDelta = capP1 - capP0;
    float capLenSqr = capDelta.LengthSqr();

    constexpr int kSteps = 32;
    for (int i = 0; i <= kSteps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSteps);
        Vector3 spherePos = sphereStart + (rayDelta * t);

        float u = 0.0f;
        if (capLenSqr > 1e-6f) {
            u = std::clamp((spherePos - capP0).Dot(capDelta) / capLenSqr, 0.0f, 1.0f);
        }
        Vector3 closestOnCap = capP0 + (capDelta * u);
        Vector3 diff = spherePos - closestOnCap;

        if (diff.LengthSqr() <= combinedRadius * combinedRadius) {
            res.hit = true;
            res.fraction = t;
            res.normal = diff.Normalized();
            res.hitPoint = closestOnCap + (res.normal * capRadius);
            return res;
        }
    }

    return res;
}

std::optional<uint32_t> CRubikonCollision::TraceRayBvh(
    const Ray_t& ray, const std::vector<RubikonBvhNode_t>& nodes)
{
    if (nodes.empty()) return std::nullopt;

    std::vector<uint32_t> stack;
    stack.reserve(32);
    stack.push_back(0);

    float closestFraction = 1.0f;
    std::optional<uint32_t> bestPrimitive = std::nullopt;

    while (!stack.empty()) {
        uint32_t nodeIdx = stack.back();
        stack.pop_back();

        if (nodeIdx >= nodes.size()) continue;
        const auto& node = nodes[nodeIdx];

        AABB_t box{node.minBounds, node.maxBounds};
        RayCastResult hit = IntersectRayAABB(ray, box, 0.0f);

        if (hit.hit && hit.fraction <= closestFraction) {
            if (node.leftChild == 0xFFFFFFFF && node.rightChild == 0xFFFFFFFF) {
                closestFraction = hit.fraction;
                bestPrimitive = node.primitiveIndex;
            } else {
                if (node.rightChild != 0xFFFFFFFF) stack.push_back(node.rightChild);
                if (node.leftChild != 0xFFFFFFFF) stack.push_back(node.leftChild);
            }
        }
    }

    return bestPrimitive;
}

// ============================================================================
// 2. Gilbert-Johnson-Keerthi (GJK) Narrowphase Distance & Intersection Solver
// ============================================================================

namespace {

/**
 * @brief Computes closest point to origin on 0-simplex (point), 1-simplex (segment),
 * 2-simplex (triangle), or 3-simplex (tetrahedron), and extracts exact witness points.
 */
void SolveSimplexClosest(
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
        float abLenSqr = ab.LengthSqr();

        if (abLenSqr < 1e-8f) {
            outClosest = a.v;
            outClosestA = a.a;
            outClosestB = a.b;
            simplex.Set1(a);
            return;
        }

        float t = std::clamp(-a.v.Dot(ab) / abLenSqr, 0.0f, 1.0f);
        outClosest = a.v + ab * t;
        outClosestA = a.a + (b.a - a.a) * t;
        outClosestB = a.b + (b.b - a.b) * t;

        if (t <= 0.0f) {
            simplex.Set1(a);
        } else if (t >= 1.0f) {
            simplex.Set1(b);
        }
        return;
    }

    if (simplex.count == 3) {
        const MinkowskiPoint_t a = simplex.points[0];
        const MinkowskiPoint_t b = simplex.points[1];
        const MinkowskiPoint_t c = simplex.points[2];

        Vector3 ab = b.v - a.v;
        Vector3 ac = c.v - a.v;
        Vector3 n = ab.Cross(ac);
        float nLenSqr = n.LengthSqr();

        if (nLenSqr < 1e-8f) {
            // Degenerate triangle: fallback to segment ab
            simplex.Set2(a, b);
            SolveSimplexClosest(simplex, outClosest, outClosestA, outClosestB, outEnclosesOrigin);
            return;
        }

        float u = 0.0f, v = 0.0f, w = 0.0f;
        Vector3 nUnit = n.Normalized();
        Vector3 proj = nUnit * nUnit.Dot(a.v);
        ComputeTriangleBarycentric(a.v, b.v, c.v, proj, u, v, w);

        outClosest = a.v * u + b.v * v + c.v * w;
        outClosestA = a.a * u + b.a * v + c.a * w;
        outClosestB = a.b * u + b.b * v + c.b * w;

        // Simplify simplex feature
        if (u >= 0.999f) simplex.Set1(a);
        else if (v >= 0.999f) simplex.Set1(b);
        else if (w >= 0.999f) simplex.Set1(c);
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
            // Origin is strictly enclosed within tetrahedron
            outEnclosesOrigin = true;
            outClosest = Vector3{0.0f, 0.0f, 0.0f};
            outClosestA = (a.a + b.a + c.a + d.a) * 0.25f;
            outClosestB = (a.b + b.b + c.b + d.b) * 0.25f;
            return;
        }

        // Project onto visible faces
        if (outABC) {
            simplex.Set3(a, b, c);
        } else if (outACD) {
            simplex.Set3(a, c, d);
        } else if (outADB) {
            simplex.Set3(a, d, b);
        } else {
            simplex.Set3(b, d, c);
        }
        SolveSimplexClosest(simplex, outClosest, outClosestA, outClosestB, outEnclosesOrigin);
    }
}

} // anonymous namespace

GjkResult_t CRubikonCollision::GJK_Evaluate(
    const ISupportShape& shapeA, const ISupportShape& shapeB,
    int maxIterations, float tolerance)
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

        SolveSimplexClosest(result.simplex, closestPt, closestA, closestB, encloses);

        if (encloses || closestPt.LengthSqr() < (tolerance * tolerance)) {
            result.intersect = true;
            result.distance = 0.0f;
            result.closestPointA = closestA;
            result.closestPointB = closestB;
            result.separatingNormal = (initialDir.Normalized() * -1.0f);
            return result;
        }

        float currentDist = closestPt.Length();
        Vector3 searchDir = closestPt * (-1.0f / currentDist);

        MinkowskiPoint_t newPt = MinkowskiSupport(shapeA, shapeB, searchDir);

        // Disjoint convergence check: supporting plane distance bound
        float delta = currentDist + newPt.v.Dot(searchDir);
        if (delta <= tolerance || (newPt.v - closestPt).LengthSqr() < (tolerance * tolerance)) {
            // Reached closest distance between disjoint convex sets
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

// ============================================================================
// 3. Expanding Polytope Algorithm (EPA) Penetration Depth Solver
// ============================================================================

namespace {

struct EpaFace_t {
    uint32_t i0{0};
    uint32_t i1{0};
    uint32_t i2{0};
    Vector3 normal{0.0f, 0.0f, 0.0f};
    float dist{0.0f};
    bool obsolete{false};
};

struct EpaEdge_t {
    uint32_t i0{0};
    uint32_t i1{0};
};

} // anonymous namespace

EpaResult_t CRubikonCollision::EPA_Evaluate(
    const ISupportShape& shapeA, const ISupportShape& shapeB,
    const Simplex_t& /*terminalSimplex*/, int maxIterations, float tolerance)
{
    EpaResult_t result;

    // Use 6 cardinal directions (+-X, +-Y, +-Z) to build a robust initial octahedron
    // whose 8 triangular faces strictly enclose the origin in 3D without degenerate edges.
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

    // 8 triangular faces of the octahedron
    const uint32_t initialTriangles[8][3] = {
        {0, 2, 4},
        {0, 5, 2},
        {0, 4, 3},
        {0, 3, 5},
        {1, 4, 2},
        {1, 2, 5},
        {1, 3, 4},
        {1, 5, 3}
    };

    std::vector<EpaFace_t> faces;
    faces.reserve(128);

    for (const auto& tri : initialTriangles) {
        uint32_t a = tri[0];
        uint32_t b = tri[1];
        uint32_t c = tri[2];

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
            // Converged to boundary of Minkowski difference
            result.success = true;
            result.penetrationDepth = bestFace.dist;
            result.contactNormal = bestFace.normal * -1.0f; // Authoritative contact normal pointing from B to A

            Vector3 proj = bestFace.normal * bestFace.dist;
            float u = 0.0f, v = 0.0f, w = 0.0f;
            ComputeTriangleBarycentric(
                polytope[bestFace.i0].v, polytope[bestFace.i1].v, polytope[bestFace.i2].v,
                proj, u, v, w);

            result.contactPointA = polytope[bestFace.i0].a * u + polytope[bestFace.i1].a * v + polytope[bestFace.i2].a * w;
            result.contactPointB = polytope[bestFace.i0].b * u + polytope[bestFace.i1].b * v + polytope[bestFace.i2].b * w;
            return result;
        }

        uint32_t newVertexIdx = static_cast<uint32_t>(polytope.size());
        polytope.push_back(newPt);

        // Horizon edge detection across all faces visible from newPt
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
            ComputeTriangleBarycentric(
                polytope[bestFace.i0].v, polytope[bestFace.i1].v, polytope[bestFace.i2].v,
                proj, u, v, w);
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

    // Return best approximation upon reaching max iterations
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
        ComputeTriangleBarycentric(
            polytope[bestFace.i0].v, polytope[bestFace.i1].v, polytope[bestFace.i2].v,
            proj, u, v, w);
        result.contactPointA = polytope[bestFace.i0].a * u + polytope[bestFace.i1].a * v + polytope[bestFace.i2].a * w;
        result.contactPointB = polytope[bestFace.i0].b * u + polytope[bestFace.i1].b * v + polytope[bestFace.i2].b * w;
    }

    return result;
}

// ============================================================================
// 4. Continuous Collision Detection (CCD) & Conservative Advancement
// ============================================================================

namespace {

class TranslatedSupportShape final : public ISupportShape {
    const ISupportShape* m_base;
    Vector3 m_offset;
public:
    TranslatedSupportShape(const ISupportShape* base, const Vector3& offset) noexcept
        : m_base(base), m_offset(offset) {}

    [[nodiscard]] Vector3 GetSupport(const Vector3& dir) const noexcept override {
        return m_base->GetSupport(dir) + m_offset;
    }

    [[nodiscard]] Vector3 GetCenter() const noexcept override {
        return m_base->GetCenter() + m_offset;
    }
};

} // anonymous namespace

CcdResult_t CRubikonCollision::ConservativeAdvancement(
    const ISupportShape& shapeA, const Vector3& linearVelA,
    const ISupportShape& shapeB, const Vector3& linearVelB,
    float dt, float contactTolerance, int maxIterations)
{
    CcdResult_t result;
    const Vector3 relVel = linearVelA - linearVelB;
    float currentT = 0.0f;
    Vector3 lastNormal{-1.0f, 0.0f, 0.0f};

    for (int iter = 0; iter < maxIterations; ++iter) {
        result.iterations = iter + 1;

        TranslatedSupportShape currentA(&shapeA, linearVelA * (currentT * dt));
        TranslatedSupportShape currentB(&shapeB, linearVelB * (currentT * dt));

        GjkResult_t gjk = GJK_Evaluate(currentA, currentB, 32, contactTolerance * 0.1f);

        if (gjk.intersect || gjk.distance <= contactTolerance) {
            result.hit = true;
            result.toi = currentT;
            result.contactNormal = (gjk.separatingNormal.LengthSqr() > 1e-4f) ? gjk.separatingNormal : lastNormal;
            result.contactPoint = (gjk.closestPointA + gjk.closestPointB) * 0.5f;
            return result;
        }

        lastNormal = gjk.separatingNormal;

        // Project relative velocity onto separating normal (points from B to A)
        float approachSpeed = -relVel.Dot(gjk.separatingNormal);
        if (approachSpeed <= 1e-6f) {
            // Shapes are separating or moving parallel to the separating axis
            break;
        }

        float deltaT = (gjk.distance - contactTolerance) / (approachSpeed * dt);
        if (deltaT < 1e-5f) {
            deltaT = 1e-5f; // Prevent stagnation
        }

        currentT += deltaT;
        if (currentT >= 1.0f) {
            break; // No collision within time interval [0, dt]
        }
    }

    result.toi = std::min(currentT, 1.0f);
    return result;
}

CcdResult_t CRubikonCollision::SweepCapsuleAgainstCapsule(
    const RnCapsule_t& capAStart, const Vector3& velA,
    const RnCapsule_t& capBStart, const Vector3& velB,
    float dt)
{
    SupportCapsule supA(capAStart);
    SupportCapsule supB(capBStart);
    return ConservativeAdvancement(supA, velA, supB, velB, dt, 1e-3f, 32);
}

// ============================================================================
// 5. Rubikon Collision Filtering Manager
// ============================================================================

bool CRubikonCollision::ShouldCollide(
    uint64_t maskA, uint64_t maskB,
    CollisionGroup_t groupA, CollisionGroup_t groupB) noexcept
{
    // Layer 1: Bitwise content mask compatibility
    if ((maskA & maskB) == 0) {
        return false;
    }

    // Layer 2: Rubikon collision group interaction rules
    if (groupA == CollisionGroup_t::DEBRIS && groupB == CollisionGroup_t::DEBRIS) {
        return false;
    }

    if ((groupA == CollisionGroup_t::PLAYER && groupB == CollisionGroup_t::DEBRIS) ||
        (groupA == CollisionGroup_t::DEBRIS && groupB == CollisionGroup_t::PLAYER)) {
        return false;
    }

    if (groupA == CollisionGroup_t::DOOR_BLOCKER && groupB != CollisionGroup_t::PLAYER) {
        return false;
    }

    return true;
}

// ============================================================================
// 6. Morton 3D BVH Accelerator Implementation
// ============================================================================

namespace {

struct MortonItem_t {
    uint32_t code{0};
    uint32_t index{0};
    AABB_t bounds{};
};

uint32_t BuildBvhSubtree(
    std::vector<RubikonBvhNode_t>& nodes,
    const std::vector<MortonItem_t>& items,
    size_t start, size_t end)
{
    if (end - start == 1) {
        // Leaf node
        RubikonBvhNode_t leaf;
        leaf.minBounds = items[start].bounds.mins;
        leaf.maxBounds = items[start].bounds.maxs;
        leaf.leftChild = 0xFFFFFFFF;
        leaf.rightChild = 0xFFFFFFFF;
        leaf.flags = 1; // Leaf flag
        leaf.primitiveIndex = items[start].index;
        leaf.pad[0] = 0;
        leaf.pad[1] = 0;

        uint32_t leafIdx = static_cast<uint32_t>(nodes.size());
        nodes.push_back(leaf);
        return leafIdx;
    }

    // Internal node: divide sorted Morton range at midpoint
    size_t mid = start + (end - start) / 2;
    uint32_t internalIdx = static_cast<uint32_t>(nodes.size());
    nodes.push_back({}); // Reserve slot

    uint32_t leftChild = BuildBvhSubtree(nodes, items, start, mid);
    uint32_t rightChild = BuildBvhSubtree(nodes, items, mid, end);

    RubikonBvhNode_t& internalNode = nodes[internalIdx];
    internalNode.leftChild = leftChild;
    internalNode.rightChild = rightChild;
    internalNode.flags = 0; // Internal node
    internalNode.primitiveIndex = 0xFFFFFFFF;
    internalNode.pad[0] = 0;
    internalNode.pad[1] = 0;

    internalNode.minBounds = VectorMin(nodes[leftChild].minBounds, nodes[rightChild].minBounds);
    internalNode.maxBounds = VectorMax(nodes[leftChild].maxBounds, nodes[rightChild].maxBounds);

    return internalIdx;
}

} // anonymous namespace

void MortonBVH::Build(const std::vector<AABB_t>& primitiveBounds) {
    m_nodes.clear();
    if (primitiveBounds.empty()) return;

    // Compute global bounding box
    m_worldBounds = primitiveBounds[0];
    for (size_t i = 1; i < primitiveBounds.size(); ++i) {
        m_worldBounds.mins = VectorMin(m_worldBounds.mins, primitiveBounds[i].mins);
        m_worldBounds.maxs = VectorMax(m_worldBounds.maxs, primitiveBounds[i].maxs);
    }

    std::vector<MortonItem_t> items;
    items.reserve(primitiveBounds.size());

    for (size_t i = 0; i < primitiveBounds.size(); ++i) {
        Vector3 center = primitiveBounds[i].GetCenter();
        uint32_t code = ComputeMorton3D(center, m_worldBounds);
        items.push_back({code, static_cast<uint32_t>(i), primitiveBounds[i]});
    }

    // Sort along Z-order space filling curve
    std::sort(items.begin(), items.end(), [](const MortonItem_t& a, const MortonItem_t& b) {
        return a.code < b.code;
    });

    m_nodes.reserve(primitiveBounds.size() * 2);
    BuildBvhSubtree(m_nodes, items, 0, items.size());
}

void MortonBVH::QueryAABB(const AABB_t& queryBox, std::vector<uint32_t>& outPrimitiveIndices) const {
    if (m_nodes.empty()) return;

    std::vector<uint32_t> stack;
    stack.reserve(32);
    stack.push_back(0);

    while (!stack.empty()) {
        uint32_t nodeIdx = stack.back();
        stack.pop_back();

        if (nodeIdx >= m_nodes.size()) continue;
        const auto& node = m_nodes[nodeIdx];

        AABB_t nodeBox{node.minBounds, node.maxBounds};
        if (!nodeBox.Overlaps(queryBox)) continue;

        if (node.leftChild == 0xFFFFFFFF && node.rightChild == 0xFFFFFFFF) {
            outPrimitiveIndices.push_back(node.primitiveIndex);
        } else {
            if (node.rightChild != 0xFFFFFFFF) stack.push_back(node.rightChild);
            if (node.leftChild != 0xFFFFFFFF) stack.push_back(node.leftChild);
        }
    }
}

RayCastResult MortonBVH::TraceRay(const Ray_t& ray) const {
    RayCastResult result;
    if (m_nodes.empty()) return result;

    std::vector<uint32_t> stack;
    stack.reserve(32);
    stack.push_back(0);

    float closestFraction = 1.0f;

    while (!stack.empty()) {
        uint32_t nodeIdx = stack.back();
        stack.pop_back();

        if (nodeIdx >= m_nodes.size()) continue;
        const auto& node = m_nodes[nodeIdx];

        AABB_t nodeBox{node.minBounds, node.maxBounds};
        RayCastResult hit = CRubikonCollision::IntersectRayAABB(ray, nodeBox, 0.0f);

        if (hit.hit && hit.fraction <= closestFraction) {
            if (node.leftChild == 0xFFFFFFFF && node.rightChild == 0xFFFFFFFF) {
                closestFraction = hit.fraction;
                result = hit;
            } else {
                if (node.rightChild != 0xFFFFFFFF) stack.push_back(node.rightChild);
                if (node.leftChild != 0xFFFFFFFF) stack.push_back(node.leftChild);
            }
        }
    }

    return result;
}

} // namespace cs2::vphysics2
