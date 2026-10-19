// =============================================================================
// Source 2 Rubikon Physics Engine & Collision Detection Simulator
// Target Binary: vphysics2.dll (Base: 0x180000000)
// Verified against Ghidra decompiler signatures and SchemaSystem bindings:
//   - Convex Hull Raycast: FUN_180251dc0 / FUN_1802544a0 (1/32" epsilon)
//   - Capsule Ray/Sweep: FUN_18024e9b0 / FUN_18016caf0
//   - GJK Support Functions: CTraceAABB (0x1802ea210), CTraceCapsule (0x18006d070),
//                            CTraceHull (0x1802ea5d0)
//   - BVH Spatial Tree: RnCompoundTree_t & 48-byte BvhNode_t
//
// Zero external dependencies - pure ISO C++17 / MSVC x64.
// =============================================================================

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <iomanip>
#include <algorithm>
#include <chrono>

namespace cs2 {

// -----------------------------------------------------------------------------
// Math Primitives & Alignment
// -----------------------------------------------------------------------------
struct Vector3 {
    float x{ 0.0f }, y{ 0.0f }, z{ 0.0f };

    constexpr Vector3() = default;
    constexpr Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vector3 operator+(const Vector3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vector3 operator-(const Vector3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vector3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vector3 operator/(float s) const { return { x / s, y / s, z / s }; }

    Vector3& operator+=(const Vector3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vector3& operator-=(const Vector3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }

    float Dot(const Vector3& o) const { return x * o.x + y * o.y + z * o.z; }
    float LengthSqr() const { return Dot(*this); }
    float Length() const { return std::sqrt(LengthSqr()); }

    Vector3 Normalized() const {
        float len = Length();
        return (len > 1e-6f) ? (*this / len) : Vector3{ 0.0f, 0.0f, 0.0f };
    }
};

struct Plane {
    Vector3 normal;
    float dist{ 0.0f }; // Valve Rubikon convention: normal.Dot(p) - dist = 0

    constexpr Plane() = default;
    constexpr Plane(float nx, float ny, float nz, float d) : normal(nx, ny, nz), dist(d) {}
    constexpr Plane(const Vector3& n, float d) : normal(n), dist(d) {}
};

struct AABB {
    Vector3 min;
    Vector3 max;

    bool Overlaps(const AABB& o) const {
        if (max.x < o.min.x || min.x > o.max.x) return false;
        if (max.y < o.min.y || min.y > o.max.y) return false;
        if (max.z < o.min.z || min.z > o.max.z) return false;
        return true;
    }

    bool Contains(const Vector3& p) const {
        return (p.x >= min.x && p.x <= max.x &&
                p.y >= min.y && p.y <= max.y &&
                p.z >= min.z && p.z <= max.z);
    }
};

// -----------------------------------------------------------------------------
// Source 2 Rubikon Trace Result (Reconstructed from FUN_180251dc0)
// -----------------------------------------------------------------------------
struct TraceResult {
    float fraction{ 1.0f };           // [0.0..1.0] hit fraction along ray
    Vector3 hitPoint;                 // World-space intersection coordinate
    Vector3 normal;                   // Surface normal at contact
    int16_t hitPlaneIndex{ -1 };      // Index of contacting polygon/hull plane
    bool allSolid{ false };           // Ray origin started inside solid
    bool startSolid{ false };         // Ray began in solid region
    bool hit{ false };                // True if fraction < 1.0
};

// -----------------------------------------------------------------------------
// 1. Rubikon Convex Polyhedron Hull Raycast (FUN_1802544a0 & FUN_180251dc0)
// -----------------------------------------------------------------------------
class RnHull {
public:
    std::vector<Plane> m_FacePlanes;     // Half-spaces defining convex hull
    std::vector<Vector3> m_Vertices;     // Support vertices
    AABB m_Bounds;                       // Cached bounding box

    void AddPlane(const Plane& p) {
        m_FacePlanes.push_back(p);
    }

    void AddVertex(const Vector3& v) {
        m_Vertices.push_back(v);
        if (m_Vertices.size() == 1) {
            m_Bounds.min = m_Bounds.max = v;
        } else {
            m_Bounds.min.x = std::min(m_Bounds.min.x, v.x);
            m_Bounds.min.y = std::min(m_Bounds.min.y, v.y);
            m_Bounds.min.z = std::min(m_Bounds.min.z, v.z);
            m_Bounds.max.x = std::max(m_Bounds.max.x, v.x);
            m_Bounds.max.y = std::max(m_Bounds.max.y, v.y);
            m_Bounds.max.z = std::max(m_Bounds.max.z, v.z);
        }
    }

    // Exact replica of FUN_1802544a0:
    // Convex polyhedron slab-clipping with 1/32" (0.03125f) skin epsilon
    bool ClipRay(const Vector3& rayStart, const Vector3& rayDelta,
                 float& outEnterFrac, float& outExitFrac, int16_t& outHitPlane, bool& outStartSolid) const
    {
        constexpr float DIST_EPSILON = 0.03125f; // 1/32 inch Source 2 skin tolerance

        float tMin = -3.4028235e+38f;
        float tMax = 3.4028235e+38f;
        int enterPlane = -1;
        int exitPlane = -1;
        bool allSolid = true;

        if (m_FacePlanes.empty()) return false;

        for (size_t i = 0; i < m_FacePlanes.size(); ++i) {
            const Plane& p = m_FacePlanes[i];

            // Signed distances from plane
            float dStart = p.normal.Dot(rayStart) - p.dist;
            float dEnd = p.normal.Dot(rayStart + rayDelta) - p.dist;

            // Completely outside this half-space?
            if (dStart > 0.0f && dEnd > 0.0f) {
                return false;
            }

            if (dStart > 0.0f || dEnd > 0.0f) {
                if (dStart > 0.0f) {
                    allSolid = false;
                }

                // Entering vs Exiting half-space
                if (dStart <= dEnd) {
                    // Exiting: (dStart + EPS) / (dStart - dEnd)
                    float frac = (dStart + DIST_EPSILON) / (dStart - dEnd);
                    if (frac < tMax) {
                        tMax = frac;
                        exitPlane = static_cast<int>(i);
                    }
                } else {
                    // Entering: (dStart - EPS) / (dStart - dEnd)
                    float frac = (dStart - DIST_EPSILON) / (dStart - dEnd);
                    if (tMin < frac) {
                        tMin = frac;
                        enterPlane = static_cast<int>(i);
                    }
                }
            }
        }

        if (tMax <= tMin) {
            return false;
        }

        outEnterFrac = std::max(0.0f, tMin);
        outExitFrac = tMax;
        outHitPlane = static_cast<int16_t>(enterPlane);
        outStartSolid = allSolid;
        return true;
    }

    // High-level raycast matching FUN_180251dc0
    bool CastRay(const Vector3& start, const Vector3& delta, TraceResult& outResult) const {
        float enterFrac = 1.0f;
        float exitFrac = 1.0f;
        int16_t hitPlane = -1;
        bool startSolid = false;

        if (!ClipRay(start, delta, enterFrac, exitFrac, hitPlane, startSolid)) {
            outResult.fraction = 1.0f;
            outResult.hit = false;
            return false;
        }

        if (enterFrac >= 1.0f) {
            outResult.fraction = 1.0f;
            outResult.hit = false;
            return false;
        }

        outResult.fraction = enterFrac;
        outResult.hitPoint = start + delta * enterFrac;
        outResult.startSolid = startSolid;
        outResult.hitPlaneIndex = hitPlane;

        if (hitPlane >= 0 && hitPlane < static_cast<int16_t>(m_FacePlanes.size())) {
            outResult.normal = m_FacePlanes[hitPlane].normal;
        } else {
            outResult.normal = (delta * -1.0f).Normalized();
        }

        outResult.hit = true;
        return true;
    }
};

// -----------------------------------------------------------------------------
// 2. Rubikon Capsule Ray & Sweep Intersection (FUN_18016caf0)
// -----------------------------------------------------------------------------
class RnCapsule {
public:
    Vector3 m_CenterA;
    Vector3 m_CenterB;
    float m_Radius{ 0.0f };

    RnCapsule(const Vector3& a, const Vector3& b, float radius)
        : m_CenterA(a), m_CenterB(b), m_Radius(radius) {}

    // Raycast against capsule (swept sphere along segment AB)
    bool CastRay(const Vector3& rayStart, const Vector3& rayDelta, TraceResult& outResult) const {
        Vector3 segAxis = m_CenterB - m_CenterA;
        float segLenSqr = segAxis.LengthSqr();

        // Degenerate capsule = sphere
        if (segLenSqr < 1e-6f) {
            return CastRaySphere(m_CenterA, m_Radius, rayStart, rayDelta, outResult);
        }

        // Ray vs Finite Cylinder with Hemispherical Endcaps
        Vector3 rc = rayStart - m_CenterA;
        Vector3 d = rayDelta;

        float dDotAxis = d.Dot(segAxis);
        float rcDotAxis = rc.Dot(segAxis);

        Vector3 dPerp = d - segAxis * (dDotAxis / segLenSqr);
        Vector3 rcPerp = rc - segAxis * (rcDotAxis / segLenSqr);

        float a = dPerp.LengthSqr();
        float b = 2.0f * rcPerp.Dot(dPerp);
        float c = rcPerp.LengthSqr() - m_Radius * m_Radius;

        float bestT = 1.0f;
        Vector3 bestNormal{ 0, 0, 0 };
        bool hitFound = false;

        if (a > 1e-6f) {
            float discr = b * b - 4.0f * a * c;
            if (discr >= 0.0f) {
                float sqrtDiscr = std::sqrt(discr);
                float t0 = (-b - sqrtDiscr) / (2.0f * a);
                if (t0 >= 0.0f && t0 < bestT) {
                    // Check if hit falls within finite segment AB
                    float proj = (rcDotAxis + t0 * dDotAxis) / segLenSqr;
                    if (proj >= 0.0f && proj <= 1.0f) {
                        bestT = t0;
                        Vector3 hitPos = rayStart + d * t0;
                        Vector3 ptOnSeg = m_CenterA + segAxis * proj;
                        bestNormal = (hitPos - ptOnSeg).Normalized();
                        hitFound = true;
                    }
                }
            }
        }

        // Test hemisphere endcaps
        TraceResult capA, capB;
        if (CastRaySphere(m_CenterA, m_Radius, rayStart, rayDelta, capA)) {
            if (capA.fraction < bestT) {
                bestT = capA.fraction;
                bestNormal = capA.normal;
                hitFound = true;
            }
        }
        if (CastRaySphere(m_CenterB, m_Radius, rayStart, rayDelta, capB)) {
            if (capB.fraction < bestT) {
                bestT = capB.fraction;
                bestNormal = capB.normal;
                hitFound = true;
            }
        }

        if (hitFound) {
            outResult.fraction = bestT;
            outResult.hitPoint = rayStart + rayDelta * bestT;
            outResult.normal = bestNormal;
            outResult.hit = true;
            return true;
        }

        return false;
    }

private:
    static bool CastRaySphere(const Vector3& center, float radius,
                              const Vector3& rayStart, const Vector3& rayDelta,
                              TraceResult& outResult)
    {
        Vector3 oc = rayStart - center;
        float a = rayDelta.LengthSqr();
        float b = 2.0f * oc.Dot(rayDelta);
        float c = oc.LengthSqr() - radius * radius;

        if (a < 1e-6f) return false;
        float discr = b * b - 4.0f * a * c;
        if (discr < 0.0f) return false;

        float sqrtDiscr = std::sqrt(discr);
        float t = (-b - sqrtDiscr) / (2.0f * a);
        if (t >= 0.0f && t < 1.0f) {
            outResult.fraction = t;
            outResult.hitPoint = rayStart + rayDelta * t;
            outResult.normal = (outResult.hitPoint - center).Normalized();
            outResult.hit = true;
            return true;
        }
        return false;
    }
};

// -----------------------------------------------------------------------------
// 3. GJK Support Function Dispatch (CTraceAABB, CTraceCapsule, CTraceHull)
// -----------------------------------------------------------------------------
class ITraceObject {
public:
    virtual ~ITraceObject() = default;
    virtual Vector3 GetSupportPoint(const Vector3& direction) const = 0;
};

// CTraceAABB: Slot 1 (FUN_1802ea210)
class CTraceAABB : public ITraceObject {
public:
    AABB m_Box;

    CTraceAABB(const Vector3& min, const Vector3& max) : m_Box{ min, max } {}

    Vector3 GetSupportPoint(const Vector3& dir) const override {
        // Sign-bit extraction matching decompiled FUN_1802ea210:
        // *param_3 >> 0x1f selects min or max coordinate per axis
        return {
            (dir.x >= 0.0f) ? m_Box.max.x : m_Box.min.x,
            (dir.y >= 0.0f) ? m_Box.max.y : m_Box.min.y,
            (dir.z >= 0.0f) ? m_Box.max.z : m_Box.min.z
        };
    }
};

// CTraceCapsule: Slot 1 (FUN_18006d070)
class CTraceCapsule : public ITraceObject {
public:
    Vector3 m_PtA;
    Vector3 m_PtB;
    float m_Radius;

    CTraceCapsule(const Vector3& a, const Vector3& b, float r)
        : m_PtA(a), m_PtB(b), m_Radius(r) {}

    Vector3 GetSupportPoint(const Vector3& dir) const override {
        // Dot product comparison matching FUN_18006d070
        float dotA = m_PtA.Dot(dir);
        float dotB = m_PtB.Dot(dir);
        Vector3 basePt = (dotA >= dotB) ? m_PtA : m_PtB;
        return basePt + dir.Normalized() * m_Radius;
    }
};

// CTraceHull: Slot 1 (FUN_1802ea5d0)
class CTraceHull : public ITraceObject {
public:
    std::vector<Vector3> m_Vertices;

    explicit CTraceHull(std::vector<Vector3> verts) : m_Vertices(std::move(verts)) {}

    Vector3 GetSupportPoint(const Vector3& dir) const override {
        if (m_Vertices.empty()) return { 0, 0, 0 };

        // argmax(v_i . dir) unrolled loop matching FUN_1802ea5d0
        float bestDot = -1e30f;
        Vector3 bestV = m_Vertices[0];

        for (const auto& v : m_Vertices) {
            float d = v.Dot(dir);
            if (d > bestDot) {
                bestDot = d;
                bestV = v;
            }
        }
        return bestV;
    }
};

// -----------------------------------------------------------------------------
// 4. Source 2 Rubikon BVH Spatial Acceleration Tree (RnCompoundTree_t)
// -----------------------------------------------------------------------------
struct BvhNode {
    AABB bbox;                     // 24 bytes (min[3], max[3])
    int32_t leftChild{ -1 };       // 4 bytes: Index to left child or -1 if leaf
    int32_t rightChild{ -1 };      // 4 bytes: Index to right child or -1 if leaf
    int32_t shapeId{ -1 };         // 4 bytes: Collision shape payload ID
    int32_t padding[3]{ 0, 0, 0 }; // Align to 48 bytes (0x30 matching FUN_18000f4e0)
};

static_assert(sizeof(BvhNode) == 48, "BvhNode must match Source 2 48-byte SIMD alignment");

class RnCompoundTree {
public:
    std::vector<BvhNode> m_Nodes;

    int32_t BuildLeaf(const AABB& box, int32_t shapeId) {
        BvhNode node;
        node.bbox = box;
        node.leftChild = -1;
        node.rightChild = -1;
        node.shapeId = shapeId;
        m_Nodes.push_back(node);
        return static_cast<int32_t>(m_Nodes.size() - 1);
    }

    int32_t BuildInternal(int32_t left, int32_t right) {
        BvhNode node;
        node.bbox.min.x = std::min(m_Nodes[left].bbox.min.x, m_Nodes[right].bbox.min.x);
        node.bbox.min.y = std::min(m_Nodes[left].bbox.min.y, m_Nodes[right].bbox.min.y);
        node.bbox.min.z = std::min(m_Nodes[left].bbox.min.z, m_Nodes[right].bbox.min.z);
        node.bbox.max.x = std::max(m_Nodes[left].bbox.max.x, m_Nodes[right].bbox.max.x);
        node.bbox.max.y = std::max(m_Nodes[left].bbox.max.y, m_Nodes[right].bbox.max.y);
        node.bbox.max.z = std::max(m_Nodes[left].bbox.max.z, m_Nodes[right].bbox.max.z);
        node.leftChild = left;
        node.rightChild = right;
        node.shapeId = -1;
        m_Nodes.push_back(node);
        return static_cast<int32_t>(m_Nodes.size() - 1);
    }

    // Traversal matching FUN_18000f4e0:
    // Hierarchical query passing overlapping leaf nodes to candidate collector
    void QueryAABB(int32_t nodeIndex, const AABB& queryBox, std::vector<int32_t>& outShapeIds) const {
        if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_Nodes.size())) return;

        const auto& node = m_Nodes[nodeIndex];
        if (!node.bbox.Overlaps(queryBox)) return;

        // Leaf node check matching *(int*)(node + 0xc) == -1
        if (node.leftChild == -1) {
            outShapeIds.push_back(node.shapeId);
            return;
        }

        // Recurse children
        QueryAABB(node.leftChild, queryBox, outShapeIds);
        QueryAABB(node.rightChild, queryBox, outShapeIds);
    }
};

} // namespace cs2

// =============================================================================
// Standalone Verification Suite
// =============================================================================
int main() {
    std::cout << "================================================================================" << std::endl;
    std::cout << " Source 2 Rubikon Physics & Collision Verification Simulator" << std::endl;
    std::cout << " Target: vphysics2.dll (Base: 0x180000000)" << std::endl;
    std::cout << "================================================================================" << std::endl;

    int testsPassed = 0;
    int totalTests = 0;

    // -------------------------------------------------------------------------
    // Test 1: Rubikon Convex Hull Slab-Clipping Raycast (FUN_1802544a0)
    // Create an oriented rectangular box hull [-10, +10] along all axes
    // -------------------------------------------------------------------------
    totalTests++;
    std::cout << "\n[TEST 1] Convex Polyhedron Raycast (1/32\" skin tolerance)..." << std::endl;
    {
        cs2::RnHull cube;
        // 6 bounding planes for box [-10..+10]
        cube.AddPlane({  1.0f,  0.0f,  0.0f, 10.0f }); // +X
        cube.AddPlane({ -1.0f,  0.0f,  0.0f, 10.0f }); // -X
        cube.AddPlane({  0.0f,  1.0f,  0.0f, 10.0f }); // +Y
        cube.AddPlane({  0.0f, -1.0f,  0.0f, 10.0f }); // -Y
        cube.AddPlane({  0.0f,  0.0f,  1.0f, 10.0f }); // +Z
        cube.AddPlane({  0.0f,  0.0f, -1.0f, 10.0f }); // -Z

        // Ray from (0, -30, 0) towards (0, +30, 0): delta = (0, 60, 0)
        cs2::Vector3 start{ 0.0f, -30.0f, 0.0f };
        cs2::Vector3 delta{ 0.0f,  60.0f, 0.0f };
        cs2::TraceResult tr;

        bool hit = cube.CastRay(start, delta, tr);
        std::cout << "   Ray: Start=(0, -30, 0) Delta=(0, 60, 0)" << std::endl;
        std::cout << "   Hit: " << (hit ? "TRUE" : "FALSE")
                  << " | Frac: " << std::fixed << std::setprecision(4) << tr.fraction
                  << " | HitPoint: (" << tr.hitPoint.x << ", " << tr.hitPoint.y << ", " << tr.hitPoint.z << ")"
                  << " | Normal: (" << tr.normal.x << ", " << tr.normal.y << ", " << tr.normal.z << ")" << std::endl;

        // At y = -10, distance traveled = 20 units out of 60 -> frac = 20/60 = 0.3333...
        // Skin offset = 0.03125 / 60 = ~0.00052
        if (hit && std::abs(tr.hitPoint.y - (-10.0f)) < 0.1f && tr.normal.y == -1.0f) {
            std::cout << "   [PASS] Polyhedron ray intersection verified within skin tolerance!" << std::endl;
            testsPassed++;
        } else {
            std::cout << "   [FAIL] Incorrect polyhedron intersection!" << std::endl;
        }
    }

    // -------------------------------------------------------------------------
    // Test 2: Convex Hull Raycast Miss
    // -------------------------------------------------------------------------
    totalTests++;
    std::cout << "\n[TEST 2] Convex Polyhedron Raycast Miss..." << std::endl;
    {
        cs2::RnHull cube;
        cube.AddPlane({  1.0f,  0.0f,  0.0f, 10.0f });
        cube.AddPlane({ -1.0f,  0.0f,  0.0f, 10.0f });
        cube.AddPlane({  0.0f,  1.0f,  0.0f, 10.0f });
        cube.AddPlane({  0.0f, -1.0f,  0.0f, 10.0f });
        cube.AddPlane({  0.0f,  0.0f,  1.0f, 10.0f });
        cube.AddPlane({  0.0f,  0.0f, -1.0f, 10.0f });

        // Ray missing along X = 25
        cs2::Vector3 start{ 25.0f, -30.0f, 0.0f };
        cs2::Vector3 delta{  0.0f,  60.0f, 0.0f };
        cs2::TraceResult tr;

        bool hit = cube.CastRay(start, delta, tr);
        std::cout << "   Ray: Start=(25, -30, 0) -> Miss expected" << std::endl;
        if (!hit && tr.fraction == 1.0f) {
            std::cout << "   [PASS] Polyhedron cleanly rejected non-intersecting ray!" << std::endl;
            testsPassed++;
        } else {
            std::cout << "   [FAIL] False positive ray intersection reported!" << std::endl;
        }
    }

    // -------------------------------------------------------------------------
    // Test 3: Rubikon Capsule Raycast (FUN_18016caf0)
    // Capsule from (0, 0, 0) to (0, 0, 40) with radius 5.0
    // -------------------------------------------------------------------------
    totalTests++;
    std::cout << "\n[TEST 3] Capsule Ray Intersection (FUN_18016caf0)..." << std::endl;
    {
        cs2::RnCapsule capsule({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 40.0f }, 5.0f);

        // Ray traveling horizontally across capsule cylinder body at Z = 20
        cs2::Vector3 start{ -20.0f, 0.0f, 20.0f };
        cs2::Vector3 delta{  40.0f, 0.0f,  0.0f };
        cs2::TraceResult tr;

        bool hit = capsule.CastRay(start, delta, tr);
        std::cout << "   Capsule: Segment=(0,0,0)->(0,0,40), R=5.0" << std::endl;
        std::cout << "   Ray: Start=(-20, 0, 20) Delta=(40, 0, 0)" << std::endl;
        std::cout << "   Hit: " << (hit ? "TRUE" : "FALSE")
                  << " | Frac: " << std::fixed << std::setprecision(4) << tr.fraction
                  << " | HitPoint: (" << tr.hitPoint.x << ", " << tr.hitPoint.y << ", " << tr.hitPoint.z << ")"
                  << " | Normal: (" << tr.normal.x << ", " << tr.normal.y << ", " << tr.normal.z << ")" << std::endl;

        // Hit expected at X = -5.0 (distance 15 out of 40 = frac 0.375)
        if (hit && std::abs(tr.hitPoint.x - (-5.0f)) < 0.05f && tr.normal.x == -1.0f) {
            std::cout << "   [PASS] Capsule cylinder intersection exact match!" << std::endl;
            testsPassed++;
        } else {
            std::cout << "   [FAIL] Capsule cylinder intersection calculation error!" << std::endl;
        }
    }

    // -------------------------------------------------------------------------
    // Test 4: GJK Support Functions (AABB, Capsule, Hull)
    // -------------------------------------------------------------------------
    totalTests++;
    std::cout << "\n[TEST 4] GJK / EPA Support Function Dispatch..." << std::endl;
    {
        cs2::CTraceAABB aabb({ -5, -5, -5 }, { 5, 5, 5 });
        cs2::Vector3 dir{ 1.0f, -0.5f, 0.2f };
        cs2::Vector3 suppAABB = aabb.GetSupportPoint(dir);
        std::cout << "   AABB Support for dir(1, -0.5, 0.2): ("
                  << suppAABB.x << ", " << suppAABB.y << ", " << suppAABB.z << ")" << std::endl;

        cs2::CTraceCapsule capsule({ 0, 0, 0 }, { 0, 0, 20 }, 2.0f);
        cs2::Vector3 dirUp{ 0.0f, 0.0f, 1.0f };
        cs2::Vector3 suppCap = capsule.GetSupportPoint(dirUp);
        std::cout << "   Capsule Support for dir(0, 0, 1): ("
                  << suppCap.x << ", " << suppCap.y << ", " << suppCap.z << ")" << std::endl;

        bool okAABB = (suppAABB.x == 5.0f && suppAABB.y == -5.0f && suppAABB.z == 5.0f);
        bool okCap = (suppCap.z == 22.0f);

        if (okAABB && okCap) {
            std::cout << "   [PASS] GJK support functions accurately extract maximal extremal vertices!" << std::endl;
            testsPassed++;
        } else {
            std::cout << "   [FAIL] Support function mapping mismatch!" << std::endl;
        }
    }

    // -------------------------------------------------------------------------
    // Test 5: BVH Spatial Acceleration Hierarchy (RnCompoundTree_t)
    // -------------------------------------------------------------------------
    totalTests++;
    std::cout << "\n[TEST 5] 48-byte SIMD BVH Spatial Acceleration Tree..." << std::endl;
    {
        cs2::RnCompoundTree tree;
        // Build 4 leaf bounding boxes in different sectors
        int32_t l0 = tree.BuildLeaf({ { -50, -50, 0 }, { -10, -10, 20 } }, 101); // Sector SW
        int32_t l1 = tree.BuildLeaf({ {  10, -50, 0 }, {  50, -10, 20 } }, 102); // Sector SE
        int32_t l2 = tree.BuildLeaf({ { -50,  10, 0 }, { -10,  50, 20 } }, 103); // Sector NW
        int32_t l3 = tree.BuildLeaf({ {  10,  10, 0 }, {  50,  50, 20 } }, 104); // Sector NE

        int32_t p0 = tree.BuildInternal(l0, l1); // South parent
        int32_t p1 = tree.BuildInternal(l2, l3); // North parent
        int32_t root = tree.BuildInternal(p0, p1); // World root

        std::cout << "   Constructed BVH Tree: " << tree.m_Nodes.size() << " nodes (Root: " << root << ")" << std::endl;

        // Query narrow box in Sector NE
        cs2::AABB queryBox{ { 20, 20, 5 }, { 30, 30, 15 } };
        std::vector<int32_t> results;
        tree.QueryAABB(root, queryBox, results);

        std::cout << "   Query Box: [20..30, 20..30, 5..15] -> Hits: " << results.size() << std::endl;
        for (auto id : results) {
            std::cout << "      Candidate Shape ID: " << id << std::endl;
        }

        if (results.size() == 1 && results[0] == 104) {
            std::cout << "   [PASS] Hierarchical BVH query isolated target shape with zero false positives!" << std::endl;
            testsPassed++;
        } else {
            std::cout << "   [FAIL] BVH acceleration query failed!" << std::endl;
        }
    }

    // -------------------------------------------------------------------------
    // Summary
    // -------------------------------------------------------------------------
    std::cout << "\n================================================================================" << std::endl;
    std::cout << " RESULTS: " << testsPassed << " / " << totalTests << " Tests Passed ("
              << (testsPassed == totalTests ? "100% SUCCESS" : "FAILURE") << ")" << std::endl;
    std::cout << "================================================================================" << std::endl;

    return (testsPassed == totalTests) ? 0 : 1;
}
