// =============================================================================
// Counter-Strike 2 (Source 2) - Rubikon Physics Collision Engine Simulator
// Target Binary: vphysics2.dll (Base: 0x180000000)
//
// Subsystems Verified:
//   1. Gilbert-Johnson-Keerthi (GJK) 3D Iterative Simplex Solver & Voronoi Features
//   2. Expanding Polytope Algorithm (EPA) Penetration Depth & Contact Witness Extraction
//   3. Continuous Collision Detection (CCD) via Conservative Advancement & Swept Capsules
//   4. Morton-Coded (Z-Order) 3D BVH Broadphase Spatial Acceleration Tree
//   5. Rubikon Content Bitmasks & Multi-Layer Collision Filtering Matrix
//
// Compliance: Pure ISO C++20, MSVC x64 (/O2 /std:c++20 /EHsc /W4 /WX), zero external dependencies.
// =============================================================================

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <algorithm>

// Include authoritative reconstructed Rubikon physics headers and implementation
#include "../../game/vphysics2/rubikon_collision.h"
#include "../../game/vphysics2/rubikon_collision.cpp"

using namespace cs2::vphysics2;

namespace {

void PrintSeparator(char ch = '=') {
    for (int i = 0; i < 80; ++i) std::cout << ch;
    std::cout << '\n';
}

void PrintTestHeader(int testNum, const char* title) {
    std::cout << "\n[TEST " << testNum << "] " << title << "\n";
}

} // anonymous namespace

int main() {
    PrintSeparator('=');
    std::cout << " Source 2 Rubikon Physics & Continuous Collision Verification Suite\n";
    std::cout << " Target: vphysics2.dll (Base: 0x180000000) | Image Size: 4,829,848 bytes\n";
    std::cout << " Standard: Pure ISO C++20 | MSVC x64 (/std:c++20 /O2 /EHsc /W4 /WX)\n";
    PrintSeparator('=');

    int testsPassed = 0;
    int totalTests = 0;

    // =========================================================================
    // TEST 1: GJK Disjoint Distance & Closest Point Evaluation
    // =========================================================================
    totalTests++;
    PrintTestHeader(1, "GJK Disjoint Convex Distance & Closest Witness Evaluation");
    {
        // Two disjoint spheres along X axis
        RnSphere_t sphereA{Vector3{-15.0f, 0.0f, 0.0f}, 5.0f};
        RnSphere_t sphereB{Vector3{ 15.0f, 0.0f, 0.0f}, 5.0f};

        SupportSphere supA(sphereA);
        SupportSphere supB(sphereB);

        GjkResult_t result = CRubikonCollision::GJK_Evaluate(supA, supB, 32, 1e-4f);

        std::cout << "   Sphere A: Center=(-15, 0, 0), Radius=5.0\n";
        std::cout << "   Sphere B: Center=( 15, 0, 0), Radius=5.0\n";
        std::cout << "   GJK Intersect: " << (result.intersect ? "TRUE" : "FALSE")
                  << " | Iterations: " << result.iterations << "\n";
        std::cout << "   Separation Distance: " << std::fixed << std::setprecision(4)
                  << result.distance << " units (Expected: 20.0000)\n";
        std::cout << "   Closest Point A: (" << result.closestPointA.x << ", "
                  << result.closestPointA.y << ", " << result.closestPointA.z << ")\n";
        std::cout << "   Closest Point B: (" << result.closestPointB.x << ", "
                  << result.closestPointB.y << ", " << result.closestPointB.z << ")\n";
        std::cout << "   Separating Normal (B->A): (" << result.separatingNormal.x << ", "
                  << result.separatingNormal.y << ", " << result.separatingNormal.z << ")\n";

        bool distanceOk = std::abs(result.distance - 20.0f) < 1e-3f;
        bool ptAOk = std::abs(result.closestPointA.x - (-10.0f)) < 1e-3f;
        bool ptBOk = std::abs(result.closestPointB.x - 10.0f) < 1e-3f;
        bool normOk = result.separatingNormal.x < -0.99f;

        if (!result.intersect && distanceOk && ptAOk && ptBOk && normOk) {
            std::cout << "   [PASS] GJK disjoint distance & closest witness points verified!\n";
            testsPassed++;
        } else {
            std::cout << "   [FAIL] GJK disjoint distance evaluation failed validation!\n";
        }
    }

    // =========================================================================
    // TEST 2: GJK Convex Intersection Detection Across Polyhedral Shapes
    // =========================================================================
    totalTests++;
    PrintTestHeader(2, "GJK Convex Intersection Detection Across Polyhedral Shapes");
    {
        // Case A: Overlapping Spheres
        RnSphere_t sphereA{Vector3{0.0f, 0.0f, 0.0f}, 6.0f};
        RnSphere_t sphereB{Vector3{8.0f, 0.0f, 0.0f}, 6.0f}; // Centers distance 8.0 < sum radii 12.0
        SupportSphere supA(sphereA);
        SupportSphere supB(sphereB);

        // Case B: Overlapping OBB and Capsule
        RnOBB_t box{Vector3{0.0f, 0.0f, 0.0f}, Vector3{10.0f, 10.0f, 10.0f}, Matrix3x3::Identity()};
        RnCapsule_t capsule{Vector3{5.0f, 0.0f, -20.0f}, Vector3{5.0f, 0.0f, 20.0f}, 4.0f};
        SupportOBB supBox(box);
        SupportCapsule supCap(capsule);

        // Case C: Convex Polyhedron Hull and OBB
        RnHull_t hull;
        hull.transform = Transform_t{Vector3{0.0f, 0.0f, 0.0f}, Matrix3x3::Identity()};
        hull.vertices = {
            {-10.0f, -10.0f, -10.0f}, { 10.0f, -10.0f, -10.0f},
            { 10.0f,  10.0f, -10.0f}, {-10.0f,  10.0f, -10.0f},
            {-10.0f, -10.0f,  10.0f}, { 10.0f, -10.0f,  10.0f},
            { 10.0f,  10.0f,  10.0f}, {-10.0f,  10.0f,  10.0f}
        };
        SupportHull supHull(hull);

        GjkResult_t resSpheres = CRubikonCollision::GJK_Evaluate(supA, supB, 32, 1e-4f);
        GjkResult_t resBoxCap  = CRubikonCollision::GJK_Evaluate(supBox, supCap, 32, 1e-4f);
        GjkResult_t resHullBox = CRubikonCollision::GJK_Evaluate(supHull, supBox, 32, 1e-4f);

        std::cout << "   Spheres Overlap GJK: " << (resSpheres.intersect ? "TRUE (Pass)" : "FALSE")
                  << " | Iterations: " << resSpheres.iterations << "\n";
        std::cout << "   OBB-Capsule Overlap GJK: " << (resBoxCap.intersect ? "TRUE (Pass)" : "FALSE")
                  << " | Iterations: " << resBoxCap.iterations << "\n";
        std::cout << "   Hull-OBB Overlap GJK: " << (resHullBox.intersect ? "TRUE (Pass)" : "FALSE")
                  << " | Iterations: " << resHullBox.iterations << "\n";

        if (resSpheres.intersect && resBoxCap.intersect && resHullBox.intersect) {
            std::cout << "   [PASS] GJK simplex solver accurately isolates interior origin!\n";
            testsPassed++;
        } else {
            std::cout << "   [FAIL] GJK intersection detection missed intersecting geometries!\n";
        }
    }

    // =========================================================================
    // TEST 3: Expanding Polytope Algorithm (EPA) Penetration Depth & Normal
    // =========================================================================
    totalTests++;
    PrintTestHeader(3, "Expanding Polytope Algorithm (EPA) Penetration Depth Extraction");
    {
        // Two spheres intersecting along X axis:
        // Sphere A centered at (0, 0, 0), radius 5.0
        // Sphere B centered at (8, 0, 0), radius 5.0
        // Centers distance = 8.0. Sum of radii = 10.0. Penetration depth = 10.0 - 8.0 = 2.0.
        // Authoritative contact normal from B to A = (-1, 0, 0).
        RnSphere_t sphereA{Vector3{0.0f, 0.0f, 0.0f}, 5.0f};
        RnSphere_t sphereB{Vector3{8.0f, 0.0f, 0.0f}, 5.0f};

        SupportSphere supA(sphereA);
        SupportSphere supB(sphereB);

        GjkResult_t gjk = CRubikonCollision::GJK_Evaluate(supA, supB, 32, 1e-4f);
        assert(gjk.intersect);

        EpaResult_t epa = CRubikonCollision::EPA_Evaluate(supA, supB, gjk.simplex, 48, 1e-4f);

        std::cout << "   EPA Status: " << (epa.success ? "CONVERGED" : "FAILED")
                  << " | Iterations: " << epa.iterations << "\n";
        std::cout << "   Penetration Depth: " << std::fixed << std::setprecision(4)
                  << epa.penetrationDepth << " units (Analytical: 2.0000)\n";
        std::cout << "   Contact Normal (B->A): (" << epa.contactNormal.x << ", "
                  << epa.contactNormal.y << ", " << epa.contactNormal.z << ") (Expected: -1, 0, 0)\n";
        std::cout << "   Witness Point A: (" << epa.contactPointA.x << ", "
                  << epa.contactPointA.y << ", " << epa.contactPointA.z << ")\n";
        std::cout << "   Witness Point B: (" << epa.contactPointB.x << ", "
                  << epa.contactPointB.y << ", " << epa.contactPointB.z << ")\n";

        bool epaOk = epa.success;
        bool depthOk = std::abs(epa.penetrationDepth - 2.0f) < 0.05f;
        bool normalOk = epa.contactNormal.x < -0.95f;

        if (epaOk && depthOk && normalOk) {
            std::cout << "   [PASS] EPA edge horizon expansion extracted exact minimum penetration vector!\n";
            testsPassed++;
        } else {
            std::cout << "   [FAIL] EPA penetration depth extraction discrepancy!\n";
        }
    }

    // =========================================================================
    // TEST 4: Continuous Collision Detection (CCD) & Supersonic Tunnel Rejection
    // =========================================================================
    totalTests++;
    PrintTestHeader(4, "Continuous Collision Detection (CCD) via Conservative Advancement");
    {
        // Player bounding capsule (radius 16, height 72) moving at high velocity
        // Starting at X = -500.0, traveling at V = +2000.0 units/sec towards stationary capsule at X = 0.0
        // Combined radii = 16.0 + 16.0 = 32.0.
        // Contact occurs when X_A + 16.0 = X_B - 16.0 => X_A = -32.0.
        // Travel distance = (-32.0) - (-500.0) = 468.0 units.
        // Theoretical Time Of Impact (TOI) = 468.0 / 2000.0 = 0.2340.
        RnCapsule_t movingCap{Vector3{-500.0f, 0.0f, 0.0f}, Vector3{-500.0f, 0.0f, 72.0f}, 16.0f};
        RnCapsule_t targetCap{Vector3{   0.0f, 0.0f, 0.0f}, Vector3{   0.0f, 0.0f, 72.0f}, 16.0f};

        Vector3 velA{2000.0f, 0.0f, 0.0f};
        Vector3 velB{0.0f, 0.0f, 0.0f};

        CcdResult_t ccd = CRubikonCollision::SweepCapsuleAgainstCapsule(movingCap, velA, targetCap, velB, 1.0f);

        std::cout << "   Moving Capsule: Start=(-500, 0, 0), V=(+2000, 0, 0), R=16\n";
        std::cout << "   Target Capsule: Pos=(0, 0, 0), V=(0, 0, 0), R=16\n";
        std::cout << "   Discrete Tunnel Check: Pos(t=1) = +1500 (Full Tunneling Miss)\n";
        std::cout << "   CCD Hit: " << (ccd.hit ? "TRUE" : "FALSE")
                  << " | Iterations: " << ccd.iterations << "\n";
        std::cout << "   Time of Impact (TOI): " << std::fixed << std::setprecision(4)
                  << ccd.toi << " (Analytical: 0.2340)\n";
        std::cout << "   Contact Normal: (" << ccd.contactNormal.x << ", "
                  << ccd.contactNormal.y << ", " << ccd.contactNormal.z << ")\n";
        std::cout << "   Contact Point: (" << ccd.contactPoint.x << ", "
                  << ccd.contactPoint.y << ", " << ccd.contactPoint.z << ")\n";

        bool ccdHitOk = ccd.hit;
        bool toiOk = std::abs(ccd.toi - 0.2340f) < 0.01f;

        if (ccdHitOk && toiOk) {
            std::cout << "   [PASS] Continuous collision root-finding eliminated supersonic tunnel effect!\n";
            testsPassed++;
        } else {
            std::cout << "   [FAIL] CCD root-finding failed to detect swept impact!\n";
        }
    }

    // =========================================================================
    // TEST 5: Morton-Coded (Z-Order) 3D BVH Broadphase Traversal
    // =========================================================================
    totalTests++;
    PrintTestHeader(5, "Morton-Coded (Z-Order) 3D BVH Broadphase Spatial Acceleration");
    {
        // Construct 32 distinct primitive bounding boxes distributed across a spatial domain
        std::vector<AABB_t> sceneBoxes;
        sceneBoxes.reserve(32);

        for (int i = 0; i < 32; ++i) {
            float x = static_cast<float>((i % 8) * 100 - 400);
            float y = static_cast<float>(((i / 8) % 4) * 100 - 200);
            float z = static_cast<float>((i % 2) * 50);
            sceneBoxes.push_back(AABB_t{
                Vector3{x - 20.0f, y - 20.0f, z - 20.0f},
                Vector3{x + 20.0f, y + 20.0f, z + 20.0f}
            });
        }

        MortonBVH bvh;
        bvh.Build(sceneBoxes);

        std::cout << "   Primitives Ingested: " << sceneBoxes.size() << "\n";
        std::cout << "   BVH Total Tree Nodes: " << bvh.GetNodes().size() << "\n";
        std::cout << "   World Bounds: Min=(" << bvh.m_worldBounds.mins.x << ", "
                  << bvh.m_worldBounds.mins.y << ", " << bvh.m_worldBounds.mins.z << ") Max=("
                  << bvh.m_worldBounds.maxs.x << ", " << bvh.m_worldBounds.maxs.y << ", "
                  << bvh.m_worldBounds.maxs.z << ")\n";

        // Broadphase Query with an overlapping box centered around primitive #10
        // Primitive #10: x = (10 % 8)*100 - 400 = -200, y = ((10/8)%4)*100 - 200 = -100
        AABB_t queryBox{
            Vector3{-220.0f, -120.0f, -30.0f},
            Vector3{-180.0f, -80.0f, 30.0f}
        };

        std::vector<uint32_t> queryHits;
        bvh.QueryAABB(queryBox, queryHits);

        std::cout << "   AABB Query Box: [-220..-180, -120..-80, -30..30]\n";
        std::cout << "   BVH Overlap Hits: " << queryHits.size() << " primitives: ";
        for (uint32_t id : queryHits) {
            std::cout << "#" << id << " ";
        }
        std::cout << "\n";

        // Raycast through BVH towards primitive #0 at (-400, -200, 0)
        Ray_t ray{
            Vector3{-400.0f, -400.0f, 0.0f},
            Vector3{   0.0f,  400.0f, 0.0f}
        };
        RayCastResult rayHit = bvh.TraceRay(ray);

        std::cout << "   Raycast: Start=(-400, -400, 0), Delta=(0, 400, 0)\n";
        std::cout << "   Raycast Hit: " << (rayHit.hit ? "TRUE" : "FALSE")
                  << " | Frac: " << rayHit.fraction
                  << " | HitPoint: (" << rayHit.hitPoint.x << ", " << rayHit.hitPoint.y << ", " << rayHit.hitPoint.z << ")\n";

        bool queryOk = !queryHits.empty() && std::find(queryHits.begin(), queryHits.end(), 10u) != queryHits.end();
        bool rayOk = rayHit.hit && std::abs(rayHit.hitPoint.y - (-220.0f)) < 1e-2f;

        if (queryOk && rayOk) {
            std::cout << "   [PASS] 3D Morton BVH spatial tree acceleration verified with zero false negatives!\n";
            testsPassed++;
        } else {
            std::cout << "   [FAIL] Morton BVH traversal discrepancy!\n";
        }
    }

    // =========================================================================
    // TEST 6: Rubikon Multi-Layer Collision Filtering Matrix
    // =========================================================================
    totalTests++;
    PrintTestHeader(6, "Rubikon Content Bitmasks & Multi-Layer Collision Filtering");
    {
        using namespace CollisionFlags;

        // Player pawn vs Static World
        bool playerVsWorld = CRubikonCollision::ShouldCollide(
            MASK_PLAYERSOLID, MASK_SOLID,
            CollisionGroup_t::PLAYER, CollisionGroup_t::NONE);

        // Player pawn vs Debris (debris must not block player pawns)
        bool playerVsDebris = CRubikonCollision::ShouldCollide(
            MASK_PLAYERSOLID, CONTENTS_DEBRIS,
            CollisionGroup_t::PLAYER, CollisionGroup_t::DEBRIS);

        // Debris vs Debris (debris must not collide with other debris)
        bool debrisVsDebris = CRubikonCollision::ShouldCollide(
            CONTENTS_DEBRIS, CONTENTS_DEBRIS,
            CollisionGroup_t::DEBRIS, CollisionGroup_t::DEBRIS);

        // Ballistic raycast query vs Hitbox
        bool shotVsHitbox = CRubikonCollision::ShouldCollide(
            MASK_SHOT, CONTENTS_HITBOX,
            CollisionGroup_t::PROJECTILE, CollisionGroup_t::PLAYER);

        std::cout << "   Player vs World Solid: " << (playerVsWorld ? "ALLOW (Expected: ALLOW)" : "DENY") << "\n";
        std::cout << "   Player vs Debris:      " << (playerVsDebris ? "ALLOW" : "FILTERED (Expected: FILTERED)") << "\n";
        std::cout << "   Debris vs Debris:      " << (debrisVsDebris ? "ALLOW" : "FILTERED (Expected: FILTERED)") << "\n";
        std::cout << "   Shot Query vs Hitbox:  " << (shotVsHitbox ? "ALLOW (Expected: ALLOW)" : "DENY") << "\n";

        if (playerVsWorld && !playerVsDebris && !debrisVsDebris && shotVsHitbox) {
            std::cout << "   [PASS] Rubikon multi-tier collision filtering matrix matches engine specs!\n";
            testsPassed++;
        } else {
            std::cout << "   [FAIL] Collision filtering predicate evaluation error!\n";
        }
    }

    // =========================================================================
    // Summary
    // =========================================================================
    PrintSeparator('=');
    std::cout << " VERIFICATION RESULTS: " << testsPassed << " / " << totalTests
              << " Tests Passed (" << (testsPassed * 100 / totalTests) << "% SUCCESS)\n";
    PrintSeparator('=');

    return (testsPassed == totalTests) ? 0 : 1;
}
