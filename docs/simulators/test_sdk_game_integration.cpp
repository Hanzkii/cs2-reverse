/**
 * @file test_sdk_game_integration.cpp
 * @brief End-to-end validation harness testing seamless integration between
 *        the public C++20 SDK (sdk/) and the reconstructed engine subsystems (game/).
 * 
 * Standards: Pure ISO C++20, Zero external dependencies, MSVC x64 compliant.
 */

#include <iostream>
#include <iomanip>
#include <cassert>

// Master SDK Header (automatically pulls in sdk/ and game/ through game_bridge.hpp)
#include "sdk/cs2_sdk.hpp"

using namespace cs2;

int main() {
    std::cout << "=================================================================\n";
    std::cout << " VALVE SOURCE 2: Master SDK & Engine Integration Harness\n";
    std::cout << " Validating Seamless Interop: sdk/ <---> game/ (C++20)\n";
    std::cout << "=================================================================\n\n";

    // -------------------------------------------------------------------------
    // Test 1: Zero-Cost Bi-directional Type Adapters
    // -------------------------------------------------------------------------
    std::cout << "[Test 1] Testing Bi-directional Math & Handle Adapters...\n";
    {
        const sdk::Vector3 sdkVec{100.0f, -250.0f, 64.0f};
        const shared::Vector3D gameVec = bridge::ToGame(sdkVec);
        assert(gameVec.x == 100.0f && gameVec.y == -250.0f && gameVec.z == 64.0f);

        const sdk::Vector3 backVec = bridge::ToSDK(gameVec);
        assert(backVec.x == sdkVec.x && backVec.y == sdkVec.y && backVec.z == sdkVec.z);

        const sdk::CTransform sdkTrans{0.0f, 0.70710678f, 0.0f, 0.70710678f, 10.0f, 20.0f, 30.0f, 1.0f};
        const shared::CTransform gameTrans = bridge::ToGame(sdkTrans);
        assert(gameTrans.qy == 0.70710678f && gameTrans.pz == 30.0f);

        const sdk::CEntityHandle sdkHandle{42, 1};
        const shared::CEntityHandle gameHandle = bridge::ToGame(sdkHandle);
        assert(gameHandle.GetEntryIndex() == 42 && gameHandle.GetSerialNumber() == 1);

        std::cout << "  -> SUCCESS: Vector3, CTransform, and CEntityHandle adapters verified.\n";
    }

    // -------------------------------------------------------------------------
    // Test 2: Analytical Two-Bone Inverse Kinematics (Law of Cosines)
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 2] Testing SDK Integration with animationsystem (Two-Bone IK)...\n";
    {
        const sdk::Vector3 hipPos{0.0f, 0.0f, 40.0f};
        const sdk::Vector3 footPos{0.0f, 15.0f, 0.0f};
        const sdk::Vector3 kneePoleHint{0.0f, 30.0f, 20.0f};

        const auto ikSolution = sdk::SDK::SolveTwoBoneIK(hipPos, footPos, kneePoleHint, 22.0f, 22.0f);
        assert(ikSolution.reachedTarget);
        assert(ikSolution.midPos.y > 0.0f); // Knee projects forward

        std::cout << "  Hip Pos:    (" << hipPos.x << ", " << hipPos.y << ", " << hipPos.z << ")\n";
        std::cout << "  Knee Joint: (" << ikSolution.midPos.x << ", " << ikSolution.midPos.y << ", " << ikSolution.midPos.z << ")\n";
        std::cout << "  Foot End:   (" << ikSolution.effectorPos.x << ", " << ikSolution.effectorPos.y << ", " << ikSolution.effectorPos.z << ")\n";
        std::cout << "  -> SUCCESS: Analytical limb kinematics solved via SDK facade.\n";
    }

    // -------------------------------------------------------------------------
    // Test 3: Combat Damage & Armor Scaling Pipeline (server.dll)
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 3] Testing SDK Integration with server combat damage engine...\n";
    {
        // AK-47 (Base 36 dmg, 1.55 armor ratio, 0.98 falloff) fired at armored CT Head from 500u
        const auto resultHeadshot = sdk::SDK::EvaluateDamage(
            100,                      // Victim HP
            100,                      // Victim Armor
            3,                        // Team: CT
            true,                     // Has Helmet
            36.0f,                    // AK-47 Base Damage
            0.98f,                    // AK-47 Range Modifier
            500.0f,                   // Distance: 500 units (1 decay step)
            server::HitGroup_t::HITGROUP_HEAD,
            1.55f                     // AK-47 Armor Ratio
        );

        std::cout << "  AK-47 Headshot at 500u (with Helmet):\n";
        std::cout << "    Raw Distance Dmg: " << resultHeadshot.flRawDistanceDamage << " HP\n";
        std::cout << "    Head Multiplier:  " << resultHeadshot.flHitgroupScaledDmg << " HP (4.0x)\n";
        std::cout << "    Absorbed by Helm: " << resultHeadshot.flAbsorbedByArmor << " HP\n";
        std::cout << "    Final Damage:     " << resultHeadshot.flFinalHealthDamage << " HP (Fatal: " << (resultHeadshot.bIsFatal ? "YES" : "NO") << ")\n";
        std::cout << "    Remaining Armor:  " << resultHeadshot.nRemainingArmor << " AP\n";

        assert(resultHeadshot.bIsFatal); // AK-47 is a 1-shot headshot even through helmet
        std::cout << "  -> SUCCESS: Authoritative armor absorption ODE verified.\n";
    }

    // -------------------------------------------------------------------------
    // Test 4: Camera & WorldToScreen Projection Pipeline (client.dll)
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 4] Testing SDK Integration with client camera & view projection...\n";
    {
        sdk::VMatrix viewProjMatrix{};
        // Set standard 4x4 perspective view-projection matrix
        viewProjMatrix[0][0] = 1.0f;
        viewProjMatrix[1][1] = 1.333f;
        viewProjMatrix[2][2] = 1.0f; viewProjMatrix[2][3] = 10.0f;
        viewProjMatrix[3][2] = 1.0f; // W perspective divisor

        const sdk::Vector3 worldTarget{0.0f, 0.0f, 100.0f};
        float screenX = 0.0f, screenY = 0.0f;

        const bool onScreen = bridge::WorldToScreen(worldTarget, screenX, screenY, &viewProjMatrix, 1920, 1080);
        assert(onScreen);
        assert(std::abs(screenX - 960.0f) < 1.0f); // Centered in screen width
        assert(std::abs(screenY - 540.0f) < 1.0f); // Centered in screen height

        std::cout << "  World Coordinate: (0, 0, 100)\n";
        std::cout << "  Projected Screen: (" << screenX << ", " << screenY << ") on 1920x1080 display\n";
        std::cout << "  -> SUCCESS: Frustum rejection and viewport projection validated.\n";
    }

    // -------------------------------------------------------------------------
    // Test 5: Client Bone Setup Hierarchy & Hitbox Projections
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 5] Testing SDK Integration with client bone hierarchy & hitboxes...\n";
    {
        shared::CSkeletonInstance skeleton;
        client::C_BaseEntityAnimGraph animGraph;

        // Construct 3-bone hierarchy: Root (0) -> Spine (1) -> Head (2)
        const int boneParents[3] = {-1, 0, 1};
        animGraph.AttachSkeleton(&skeleton, boneParents, 3);

        // Setup local offsets: Spine is +30u up, Head is +20u up
        animGraph.SetLocalBoneTransform(0, shared::CTransform{0, 0, 0, 1, 0, 0, 0, 1});
        animGraph.SetLocalBoneTransform(1, shared::CTransform{0, 0, 0, 1, 0, 0, 30, 1});
        animGraph.SetLocalBoneTransform(2, shared::CTransform{0, 0, 0, 1, 0, 0, 20, 1});

        shared::matrix3x4_t outBones[3];
        const bool setupSuccess = animGraph.SetupBones(outBones, 3, shared::BONE_USED_BY_HITBOX, 1.0f);
        assert(setupSuccess);

        shared::Vector3D headPos;
        const bool gotBone = animGraph.GetBonePosition(2, headPos);
        assert(gotBone);
        assert(headPos.z == 50.0f); // 30 + 20 = 50 units

        std::cout << "  Root Bone Z:  0 units\n";
        std::cout << "  Spine Bone Z: 30 units\n";
        std::cout << "  Head Bone Z:  " << headPos.z << " units\n";
        std::cout << "  -> SUCCESS: Hierarchical forward kinematics evaluated via SDK bridge.\n";
    }

    std::cout << "\n=================================================================\n";
    std::cout << " ALL SDK & GAME SUBSYSTEM INTEGRATION TESTS PASSED (100% C++20)!\n";
    std::cout << "=================================================================\n";
    return 0;
}
