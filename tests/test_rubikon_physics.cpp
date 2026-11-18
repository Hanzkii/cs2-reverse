/**
 * ============================================================================
 * Unit Test Verification: Source 2 Rubikon Physics Engine
 * GJK, EPA, and Continuous Collision Detection (CCD) Solvers
 * 
 * Verifies clean-room C++20 headers:
 *   - sdk/include/physics/rubikon_gjk.hpp
 *   - sdk/include/physics/rubikon_ccd.hpp
 * ============================================================================
 */

#include "sdk/include/physics/rubikon_gjk.hpp"
#include "sdk/include/physics/rubikon_ccd.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

using namespace cs2::physics;

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
            g_testsFailed++; \
            return; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "[RUN ] " #fn << "..." << std::endl; \
        int prevFailed = g_testsFailed; \
        fn(); \
        if (g_testsFailed == prevFailed) { \
            std::cout << "[PASS] " #fn << std::endl; \
            g_testsPassed++; \
        } \
    } while (0)

// ============================================================================
// Test 1: Linear Algebra & Matrix Transformations
// ============================================================================
void Test_LinearAlgebra() {
    Vector3 v1{1.0f, 2.0f, 3.0f};
    Vector3 v2{4.0f, 5.0f, 6.0f};

    Vector3 sum = v1 + v2;
    TEST_ASSERT(std::abs(sum.x - 5.0f) < 1e-6f && std::abs(sum.y - 7.0f) < 1e-6f && std::abs(sum.z - 9.0f) < 1e-6f,
        "Vector addition mismatch");

    float dot = v1.Dot(v2);
    TEST_ASSERT(std::abs(dot - 32.0f) < 1e-6f, "Vector dot product mismatch (1*4 + 2*5 + 3*6 = 32)");

    Vector3 cross = v1.Cross(v2);
    // (2*6 - 3*5) = -3, (3*4 - 1*6) = 6, (1*5 - 2*4) = -3
    TEST_ASSERT(std::abs(cross.x - (-3.0f)) < 1e-6f && std::abs(cross.y - 6.0f) < 1e-6f && std::abs(cross.z - (-3.0f)) < 1e-6f,
        "Vector cross product mismatch");

    Vector3 unitX{10.0f, 0.0f, 0.0f};
    Vector3 normX = unitX.Normalized();
    TEST_ASSERT(std::abs(normX.x - 1.0f) < 1e-6f && normX.Length() - 1.0f < 1e-6f, "Vector normalization mismatch");

    // Transform rotation and translation
    Matrix3x3 rotZ = Matrix3x3::FromAxisAngle({0.0f, 0.0f, 1.0f}, 1.57079632679f); // 90 degrees around Z
    Transform_t xform{{10.0f, 20.0f, 30.0f}, rotZ};

    Vector3 pt{1.0f, 0.0f, 0.0f};
    Vector3 transformed = xform.TransformPoint(pt);
    // Rotated (1, 0, 0) around Z by 90 deg -> (0, 1, 0) + (10, 20, 30) = (10, 21, 30)
    TEST_ASSERT(std::abs(transformed.x - 10.0f) < 1e-4f && std::abs(transformed.y - 21.0f) < 1e-4f && std::abs(transformed.z - 30.0f) < 1e-4f,
        "TransformPoint mismatch");

    Vector3 invPt = xform.InverseTransformPoint(transformed);
    TEST_ASSERT(std::abs(invPt.x - 1.0f) < 1e-4f && std::abs(invPt.y - 0.0f) < 1e-4f && std::abs(invPt.z - 0.0f) < 1e-4f,
        "InverseTransformPoint mismatch");
}

// ============================================================================
// Test 2: CGJKProxy Bit-Exact Support Extraction (RVA 0x002dcf10)
// ============================================================================
void Test_CGJKProxy() {
    CGJKProxy boxProxy;
    boxProxy.isBox = 1;
    boxProxy.boxCoords[0] = -5.0f; // minX
    boxProxy.boxCoords[1] =  5.0f; // maxX
    boxProxy.boxCoords[2] = -10.0f; // minY
    boxProxy.boxCoords[3] =  10.0f; // maxY
    boxProxy.boxCoords[4] = -15.0f; // minZ
    boxProxy.boxCoords[5] =  15.0f; // maxZ

    // Test all 8 vertex permutations
    Vector3 v0, v7;
    boxProxy.GetVertex(0, &v0); // ix=0, iy=0, iz=0 -> (-5, -10, -15)
    boxProxy.GetVertex(7, &v7); // ix=1, iy=1, iz=1 -> (5, 10, 15)

    TEST_ASSERT(std::abs(v0.x - (-5.0f)) < 1e-6f && std::abs(v0.y - (-10.0f)) < 1e-6f && std::abs(v0.z - (-15.0f)) < 1e-6f,
        "CGJKProxy box vertex 0 mismatch");
    TEST_ASSERT(std::abs(v7.x - 5.0f) < 1e-6f && std::abs(v7.y - 10.0f) < 1e-6f && std::abs(v7.z - 15.0f) < 1e-6f,
        "CGJKProxy box vertex 7 mismatch");

    // Test mesh array with scale
    std::vector<Vector3> meshVertices = {
        {1.0f, 2.0f, 3.0f},
        {4.0f, 5.0f, 6.0f}
    };
    CGJKProxy meshProxy;
    meshProxy.isBox = 0;
    meshProxy.vertices = meshVertices.data();
    meshProxy.vertexCount = 2;
    meshProxy.scale = 2.0f;

    Vector3 mv1;
    meshProxy.GetVertex(1, &mv1);
    TEST_ASSERT(std::abs(mv1.x - 8.0f) < 1e-6f && std::abs(mv1.y - 10.0f) < 1e-6f && std::abs(mv1.z - 12.0f) < 1e-6f,
        "CGJKProxy scaled mesh vertex mismatch");
}

// ============================================================================
// Test 3: Gilbert-Johnson-Keerthi (GJK) Distance & Disjoint Separation
// ============================================================================
void Test_GJK_DisjointAndIntersection() {
    // 1. Disjoint Spheres: Sphere A @ (0,0,0) r=5, Sphere B @ (20,0,0) r=5
    // Distance should be 20 - (5 + 5) = 10.0f
    SupportSphere sphereA({0.0f, 0.0f, 0.0f}, 5.0f);
    SupportSphere sphereB({20.0f, 0.0f, 0.0f}, 5.0f);

    GjkResult_t resDisjoint = RubikonGjkEngine::EvaluateGJK(sphereA, sphereB);
    TEST_ASSERT(!resDisjoint.intersect, "Disjoint spheres reported as intersecting");
    TEST_ASSERT(std::abs(resDisjoint.distance - 10.0f) < 1e-3f, "Disjoint sphere distance mismatch (expected 10.0)");
    TEST_ASSERT(std::abs(resDisjoint.closestPointA.x - 5.0f) < 1e-3f, "Closest point A on sphere mismatch");
    TEST_ASSERT(std::abs(resDisjoint.closestPointB.x - 15.0f) < 1e-3f, "Closest point B on sphere mismatch");

    // 2. Intersecting Spheres: Sphere A @ (0,0,0) r=5, Sphere C @ (6,0,0) r=5
    // Overlapping by 4.0 units
    SupportSphere sphereC({6.0f, 0.0f, 0.0f}, 5.0f);
    GjkResult_t resIntersect = RubikonGjkEngine::EvaluateGJK(sphereA, sphereC);
    TEST_ASSERT(resIntersect.intersect, "Overlapping spheres reported as non-intersecting");
    TEST_ASSERT(resIntersect.distance < 1e-4f, "Overlapping spheres distance should be 0");

    // 3. Disjoint OBBs: Box A [-5, 5]^3 @ (0,0,0), Box B [-5, 5]^3 @ (20, 0, 0)
    // Distance should be 20 - (5 + 5) = 10.0f
    SupportOBB boxA({0.0f, 0.0f, 0.0f}, {5.0f, 5.0f, 5.0f});
    SupportOBB boxB({20.0f, 0.0f, 0.0f}, {5.0f, 5.0f, 5.0f});

    GjkResult_t resBox = RubikonGjkEngine::EvaluateGJK(boxA, boxB);
    TEST_ASSERT(!resBox.intersect, "Disjoint boxes reported as intersecting");
    TEST_ASSERT(std::abs(resBox.distance - 10.0f) < 1e-3f, "Disjoint box distance mismatch (expected 10.0)");
    TEST_ASSERT(resBox.separatingNormal.x < -0.99f, "Separating normal should point from B to A (-X)");
}

// ============================================================================
// Test 4: Expanding Polytope Algorithm (EPA) Penetration Depth & Contact Normal
// ============================================================================
void Test_EPA_PenetrationManifolds() {
    // Two overlapping spheres: A @ (0,0,0) r=10, B @ (12,0,0) r=10
    // Overlap depth = 10 + 10 - 12 = 8.0 units
    // Normal pointing from B to A should be (-1, 0, 0)
    SupportSphere sA({0.0f, 0.0f, 0.0f}, 10.0f);
    SupportSphere sB({12.0f, 0.0f, 0.0f}, 10.0f);

    EpaResult_t epa = RubikonGjkEngine::EvaluateEPA(sA, sB);
    TEST_ASSERT(epa.success, "EPA evaluation failed on overlapping spheres");
    TEST_ASSERT(std::abs(epa.penetrationDepth - 8.0f) < 0.2f, "EPA penetration depth mismatch (expected ~8.0)");
    TEST_ASSERT(epa.contactNormal.x < -0.95f, "EPA contact normal should point from B to A (-X)");

    // Overlapping Box and Capsule
    // Box [-10, 10]^3 @ (0,0,0), Capsule from (5,0,0) to (5,0,20) r=2
    SupportOBB box({0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f});
    SupportCapsule cap({5.0f, 0.0f, 0.0f}, {5.0f, 0.0f, 20.0f}, 2.0f);

    EpaResult_t epaBoxCap = RubikonGjkEngine::EvaluateEPA(box, cap);
    TEST_ASSERT(epaBoxCap.success, "EPA evaluation failed on overlapping box and capsule");
    TEST_ASSERT(epaBoxCap.penetrationDepth > 0.0f, "EPA penetration depth must be positive for overlapping shapes");
}

// ============================================================================
// Test 5: Continuous Collision Detection (CCD) & Conservative Advancement
// ============================================================================
void Test_CCD_ConservativeAdvancement() {
    // High-speed swept sphere moving from (0,0,0) to (100,0,0) with radius 5.
    // Stationary box obstacle at (50, 0, 0) with extents (10, 10, 10) (bounds [40, 60]).
    // Expected contact at X = 40 - 5 = 35.
    // Normalized TOI should be 35 / 100 = 0.35.
    SupportSphere movingSphere({0.0f, 0.0f, 0.0f}, 5.0f);
    RnSweep_t sweepSphere;
    sweepSphere.transform0 = Transform_t{{0.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepSphere.transform1 = Transform_t{{100.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepSphere.linearVelocity = Vector3{100.0f, 0.0f, 0.0f};

    SupportOBB wallBox({0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f});
    RnSweep_t sweepWall;
    sweepWall.transform0 = Transform_t{{50.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepWall.transform1 = Transform_t{{50.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepWall.linearVelocity = Vector3{0.0f, 0.0f, 0.0f};

    CcdResult_t ccd = RubikonCcdSolver::SolveConservativeAdvancement(movingSphere, sweepSphere, wallBox, sweepWall);
    TEST_ASSERT(ccd.hit, "Continuous collision detection failed to detect obstacle");
    TEST_ASSERT(std::abs(ccd.toi - 0.35f) < 0.05f, "CCD TOI mismatch (expected ~0.35)");
    TEST_ASSERT(ccd.contactNormal.x < -0.9f, "CCD contact normal should oppose velocity (-X)");

    // Test tunneling prevention with fast moving capsule
    SupportCapsule fastCap({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}, 2.0f);
    RnSweep_t sweepCap;
    sweepCap.transform0 = Transform_t{{0.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepCap.transform1 = Transform_t{{200.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepCap.linearVelocity = Vector3{200.0f, 0.0f, 0.0f};

    SupportCapsule targetCap({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}, 5.0f);
    RnSweep_t sweepTarget;
    sweepTarget.transform0 = Transform_t{{80.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepTarget.transform1 = Transform_t{{80.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepTarget.linearVelocity = Vector3{0.0f, 0.0f, 0.0f};

    CcdResult_t capCcd = RubikonCcdSolver::SweepCapsuleCapsule(fastCap, sweepCap, targetCap, sweepTarget);
    TEST_ASSERT(capCcd.hit, "Swept capsule tunneling occurred");
    TEST_ASSERT(capCcd.toi > 0.0f && capCcd.toi < 1.0f, "Swept capsule TOI out of bounds");
}

// ============================================================================
// Test 6: Reconstructed Contact VTables (CRnConvexContact & CRnMeshContact)
// ============================================================================
void Test_ContactVTables() {
    CRnConvexContact convexContact;
    TEST_ASSERT(convexContact.GetFlags() == 0, "CRnConvexContact GetFlags mismatch (slot 1 returned 0)");

    CRnMeshContact meshContact;
    TEST_ASSERT(meshContact.GetFlags() == 1, "CRnMeshContact GetFlags mismatch (slot 1 returned 1)");

    RnSweep_t sweepA;
    sweepA.transform0 = Transform_t{{0.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepA.transform1 = Transform_t{{50.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepA.linearVelocity = Vector3{50.0f, 0.0f, 0.0f};

    RnSweep_t sweepB;
    sweepB.transform0 = Transform_t{{100.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepB.transform1 = Transform_t{{100.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
    sweepB.linearVelocity = Vector3{0.0f, 0.0f, 0.0f};

    float toiConvex = convexContact.FindTOI(sweepA, sweepB, 1.0f);
    TEST_ASSERT(toiConvex >= 0.0f && toiConvex <= 1.0f, "CRnConvexContact FindTOI returned invalid fraction");

    float toiMesh = meshContact.FindTOI(sweepA, sweepB, 1.0f);
    TEST_ASSERT(toiMesh >= 0.0f && toiMesh <= 1.0f, "CRnMeshContact FindTOI returned invalid fraction");

    convexContact.Reset();
    meshContact.Reset();
}

// ============================================================================
// Main Execution Runner
// ============================================================================
int main() {
    std::cout << "==================================================================" << std::endl;
    std::cout << " Source 2 Rubikon Physics Engine (GJK, EPA, CCD) Unit Tests" << std::endl;
    std::cout << " Target: vphysics2.dll (Build 26598758)" << std::endl;
    std::cout << "==================================================================" << std::endl;

    RUN_TEST(Test_LinearAlgebra);
    RUN_TEST(Test_CGJKProxy);
    RUN_TEST(Test_GJK_DisjointAndIntersection);
    RUN_TEST(Test_EPA_PenetrationManifolds);
    RUN_TEST(Test_CCD_ConservativeAdvancement);
    RUN_TEST(Test_ContactVTables);

    std::cout << "==================================================================" << std::endl;
    std::cout << " Results: Passed: " << g_testsPassed << ", Failed: " << g_testsFailed << std::endl;
    std::cout << "==================================================================" << std::endl;

    return g_testsFailed == 0 ? 0 : 1;
}
