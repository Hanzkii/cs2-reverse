// ============================================================================
// Counter-Strike 2 (Source 2) Particle Dynamics & 3D Compute Operator Test Suite
//
// Standalone C++20 Automated Unit & Verification Suite for:
// - CParticleCollection: 64-Byte Aligned Structure of Arrays (SoA) SIMD Pool
// - 3D Force Field Operators:
//   * COpMovementBasic: Symplectic Euler ODE with 3D Gravity Vector & Drag
//   * COpTurbulenceNoise: Divergence-Free 3D Curl Turbulence Vector Field
//   * COpVortexRing: Toroidal Vortex Ring Angular Momentum & Circulation
//   * COpWorldCollisionPlane: Rubikon Planar Half-Space Restitution & Friction
//   * COpWorldCollisionAABB: 3D Bounding Box Multi-Planar Confinement & Bounce
//   * COpFadeAndKill: Piecewise Hermite Lifecycle & O(1) Compaction
// - CParticleRendererSprite: Hardware Dynamic Billboard Quad Vertex Streaming
//
// MSVC x64 compliant (/O2 /std:c++20 /EHsc /W4 /WX). Zero external dependencies.
// ============================================================================

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <string>

// Include authoritative reconstructed Source 2 particle system implementation
#include "../game/particles/particle_system.h"
#include "../game/particles/particle_system.cpp"

using namespace cs2::particles;

namespace {

void PrintSeparator(char ch = '=') {
    for (int i = 0; i < 80; ++i) std::cout << ch;
    std::cout << '\n';
}

void PrintTestHeader(int testNum, const char* title) {
    std::cout << "\n[TEST " << testNum << "] " << title << "\n";
    PrintSeparator('-');
}

bool CheckFloatNear(float a, float b, float eps = 1e-3f) {
    return std::abs(a - b) <= eps;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Test 1: 4096-Particle SoA 64-Byte SIMD Alignment & Contiguity
// ----------------------------------------------------------------------------
void Test1_SoA4096MemoryAlignment() {
    PrintTestHeader(1, "4096-Particle SoA 64-Byte SIMD Alignment & Contiguity");

    constexpr uint32_t kParticleCount = 4096;
    CParticleCollection collection(kParticleCount);

    assert(collection.GetActiveParticleCount() == 0);
    assert(collection.GetMaxParticles() == kParticleCount);

    auto CheckAlign = [](const void* ptr, const char* name) {
        const auto addr = reinterpret_cast<uintptr_t>(ptr);
        const bool aligned = (addr % 64) == 0;
        assert(aligned && "Stream must be strictly 64-byte aligned for AVX-512");
        std::cout << "  Channel " << std::left << std::setw(16) << name
                  << " 0x" << std::hex << addr << std::dec << " [64B-Aligned: PASS]\n";
    };

    CheckAlign(collection.m_pPosX, "PosX");
    CheckAlign(collection.m_pPosY, "PosY");
    CheckAlign(collection.m_pPosZ, "PosZ");
    CheckAlign(collection.m_pPrevPosX, "PrevPosX");
    CheckAlign(collection.m_pPrevPosY, "PrevPosY");
    CheckAlign(collection.m_pPrevPosZ, "PrevPosZ");
    CheckAlign(collection.m_pVelX, "VelX");
    CheckAlign(collection.m_pVelY, "VelY");
    CheckAlign(collection.m_pVelZ, "VelZ");
    CheckAlign(collection.m_pColorR, "ColorR");
    CheckAlign(collection.m_pColorG, "ColorG");
    CheckAlign(collection.m_pColorB, "ColorB");
    CheckAlign(collection.m_pAlpha, "Alpha");
    CheckAlign(collection.m_pRadius, "Radius");
    CheckAlign(collection.m_pLifeDuration, "LifeDuration");
    CheckAlign(collection.m_pCreationTime, "CreationTime");

    std::cout << "\n[PASS] All 16 SoA channel streams 64-byte aligned for 4096 particles.\n";
}

// ----------------------------------------------------------------------------
// Test 2: Continuous & Burst Emitter Precision
// ----------------------------------------------------------------------------
void Test2_EmitterDynamics() {
    PrintTestHeader(2, "Fractional Rate Accumulator & Burst Emitter Precision");

    CParticleCollection collection(500);

    EmitInstantaneous instant(100, 0.0f);
    assert(instant.Emit(&collection, 0.016f) == 100);
    assert(instant.Emit(&collection, 0.016f) == 0); // Single-burst invariant
    assert(collection.GetActiveParticleCount() == 100);

    collection.Reset();
    assert(collection.GetActiveParticleCount() == 0);

    EmitContinuousRate continuous(200.0f); // 200/sec
    uint32_t totalEmitted = 0;
    constexpr float dt = 0.010f; // 10ms -> 2 particles/step

    for (int i = 0; i < 50; ++i) { // 500ms total -> 100 particles
        totalEmitted += continuous.Emit(&collection, dt);
    }

    assert(totalEmitted == 100);
    assert(collection.GetActiveParticleCount() == 100);
    std::cout << "  Emitted 100 particles over 0.5s at 200 Hz with zero fractional drift.\n";
    std::cout << "\n[PASS] Continuous fractional remainder accumulator verified.\n";
}

// ----------------------------------------------------------------------------
// Test 3: Stochastic Attribute Initializer Distributions
// ----------------------------------------------------------------------------
void Test3_Initializers() {
    PrintTestHeader(3, "Stochastic Initializer Bounding Volume Distribution");

    CParticleCollection collection(100);
    collection.AllocateParticles(100);

    InitRadius initR(2.0f, 6.0f);
    InitVelocityRandom initV(Vector3D{-50,-50,100}, Vector3D{50,50,300});
    InitColorRandom initC(ColorRGBA{0.2f,0.3f,0.4f,0.5f}, ColorRGBA{0.8f,0.9f,1.0f,1.0f});
    InitLifeTime initL(0.5f, 1.5f);

    initR.Initialize(&collection, 0, 100);
    initV.Initialize(&collection, 0, 100);
    initC.Initialize(&collection, 0, 100);
    initL.Initialize(&collection, 0, 100);

    for (uint32_t i = 0; i < 100; ++i) {
        assert(collection.m_pRadius[i] >= 2.0f && collection.m_pRadius[i] <= 6.0f);
        assert(collection.m_pVelX[i] >= -50.0f && collection.m_pVelX[i] <= 50.0f);
        assert(collection.m_pVelY[i] >= -50.0f && collection.m_pVelY[i] <= 50.0f);
        assert(collection.m_pVelZ[i] >= 100.0f && collection.m_pVelZ[i] <= 300.0f);
        assert(collection.m_pColorR[i] >= 0.2f && collection.m_pColorR[i] <= 0.8f);
        assert(collection.m_pAlpha[i] >= 0.5f && collection.m_pAlpha[i] <= 1.0f);
        assert(collection.m_pLifeDuration[i] >= 0.5f && collection.m_pLifeDuration[i] <= 1.5f);
    }

    std::cout << "  100 / 100 stochastic particles confirmed within multidimensional bounds.\n";
    std::cout << "\n[PASS] Stochastic initializer bounds verified.\n";
}

// ----------------------------------------------------------------------------
// Test 4: Symplectic Euler ODE Integration with 3D Gravity Vector & Drag
// ----------------------------------------------------------------------------
void Test4_SymplecticEulerODE() {
    PrintTestHeader(4, "Symplectic Euler ODE Integration with 3D Gravity Vector & Drag");

    CParticleCollection collection(10);
    collection.AllocateParticles(1);
    collection.SetPosition(0, { 0.0f, 0.0f, 500.0f });
    collection.SetVelocity(0, { 100.0f, -50.0f, 0.0f });

    // 3D Gravity: [gx=0, gy=0, gz=-800] with drag k=0.1
    OpMovementBasic movement(Vector3D{ 0.0f, 0.0f, -800.0f }, 0.1f);

    constexpr float dt = 0.016f;
    for (int i = 0; i < 60; ++i) { // ~1.0 second
        movement.Operate(&collection, dt);
    }

    const Vector3D pos = collection.GetPosition(0);
    const Vector3D vel = collection.GetVelocity(0);

    std::cout << "  After 60 steps (~1s) under 3D Gravity (gz=-800) and Drag (0.1):\n";
    std::cout << "    Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
    std::cout << "    Velocity: (" << vel.x << ", " << vel.y << ", " << vel.z << ")\n";

    assert(pos.z < 500.0f);
    assert(vel.z < -600.0f);
    assert(vel.x > 0.0f && vel.x < 100.0f); // Attenuated by drag
    assert(vel.y < 0.0f && vel.y > -50.0f); // Attenuated by drag

    std::cout << "\n[PASS] Symplectic Euler ODE integration verified.\n";
}

// ----------------------------------------------------------------------------
// Test 5: Divergence-Free 3D Curl Turbulence Vector Field Incompressibility
// ----------------------------------------------------------------------------
void Test5_CurlTurbulenceIncompressibility() {
    PrintTestHeader(5, "Divergence-Free 3D Curl Turbulence Field (del . v_curl == 0)");

    const Vector3D probes[] = {
        { 0.0f, 0.0f, 0.0f },
        { 64.0f, -32.0f, 16.0f },
        { -128.0f, 256.0f, 48.0f }
    };

    constexpr float eps = 0.01f;
    constexpr float inv2Eps = 1.0f / (2.0f * eps);
    constexpr float scale = 0.02f;

    for (size_t i = 0; i < std::size(probes); ++i) {
        const Vector3D p = probes[i];

        const Vector3D vxP = OpTurbulenceNoise::EvaluateCurlNoise({ p.x + eps, p.y, p.z }, 0.0f, scale);
        const Vector3D vxM = OpTurbulenceNoise::EvaluateCurlNoise({ p.x - eps, p.y, p.z }, 0.0f, scale);
        const float dvx_dx = (vxP.x - vxM.x) * inv2Eps;

        const Vector3D vyP = OpTurbulenceNoise::EvaluateCurlNoise({ p.x, p.y + eps, p.z }, 0.0f, scale);
        const Vector3D vyM = OpTurbulenceNoise::EvaluateCurlNoise({ p.x, p.y - eps, p.z }, 0.0f, scale);
        const float dvy_dy = (vyP.y - vyM.y) * inv2Eps;

        const Vector3D vzP = OpTurbulenceNoise::EvaluateCurlNoise({ p.x, p.y, p.z + eps }, 0.0f, scale);
        const Vector3D vzM = OpTurbulenceNoise::EvaluateCurlNoise({ p.x, p.y, p.z - eps }, 0.0f, scale);
        const float dvz_dz = (vzP.z - vzM.z) * inv2Eps;

        const float div = dvx_dx + dvy_dy + dvz_dz;
        std::cout << "  Probe [" << i << "] del . v = " << std::scientific << div << std::defaultfloat << '\n';
        assert(std::abs(div) < 1e-2f && "Curl noise must be divergence-free");
    }

    std::cout << "\n[PASS] Mathematical incompressibility del . (del x Psi) == 0 validated.\n";
}

// ----------------------------------------------------------------------------
// Test 6: Toroidal Vortex Ring Angular Momentum & Circulation Field
// ----------------------------------------------------------------------------
void Test6_ToroidalVortexRing() {
    PrintTestHeader(6, "Toroidal Vortex Ring Angular Momentum & Circulation Field");

    CParticleCollection collection(10);
    collection.AllocateParticles(2);

    // Particle 0 on central axis: expect upward axial jet
    collection.SetPosition(0, { 0.0f, 0.0f, 40.0f });
    collection.SetVelocity(0, { 0.0f, 0.0f, 0.0f });

    // Particle 1 on filament ring at X = 30
    collection.SetPosition(1, { 30.0f, 0.0f, 40.0f });
    collection.SetVelocity(1, { 0.0f, 0.0f, 0.0f });

    OpVortexRing vortex(Vector3D{0,0,40}, Vector3D{0,0,1}, 30.0f, 8.0f, 150.0f, 50.0f);
    vortex.Operate(&collection, 0.016f);

    const Vector3D v0 = collection.GetVelocity(0);
    const Vector3D v1 = collection.GetVelocity(1);

    std::cout << "  Central Axis Jet VelZ: " << v0.z << " in/s (Expected > 0)\n";
    std::cout << "  Filament Swirl VelY:   " << v1.y << " in/s (Expected > 0)\n";

    assert(v0.z > 0.0f);
    assert(v1.y > 0.0f);

    std::cout << "\n[PASS] Toroidal vortex ring dynamics validated.\n";
}

// ----------------------------------------------------------------------------
// Test 7: Rubikon Half-Space Ground Plane Collision & Restitution
// ----------------------------------------------------------------------------
void Test7_PlanarCollision() {
    PrintTestHeader(7, "Rubikon Half-Space Ground Plane Collision & Restitution");

    CParticleCollection collection(10);
    collection.AllocateParticles(1);
    collection.SetRadius(0, 5.0f);
    collection.SetPosition(0, { 0.0f, 0.0f, 2.0f }); // Penetrating z=0 by 3u
    collection.SetVelocity(0, { 50.0f, 0.0f, -100.0f });

    OpWorldCollisionPlane plane(Vector3D{0,0,1}, 0.0f, 0.5f, 0.2f);
    plane.Operate(&collection, 0.016f);

    const Vector3D pos = collection.GetPosition(0);
    const Vector3D vel = collection.GetVelocity(0);

    assert(CheckFloatNear(pos.z, 5.0f));
    assert(CheckFloatNear(vel.z, 50.0f)); // -(-100) * 0.5
    assert(CheckFloatNear(vel.x, 40.0f)); // 50 * (1 - 0.2)

    std::cout << "  Penetration resolved to z=5.0, reflected velZ=+50.0, velX=40.0\n";
    std::cout << "\n[PASS] Planar half-space collision verified.\n";
}

// ----------------------------------------------------------------------------
// Test 8: 3D Bounding Box Multi-Planar Confinement & Bounce
// ----------------------------------------------------------------------------
void Test8_BoundingBoxConfinement() {
    PrintTestHeader(8, "3D Bounding Box Multi-Planar Confinement & Bounce");

    CParticleCollection collection(10);
    collection.AllocateParticles(1);
    collection.SetRadius(0, 5.0f);
    collection.SetPosition(0, { 118.0f, 0.0f, 50.0f }); // Penetrating maxX 120
    collection.SetVelocity(0, { 100.0f, 50.0f, 20.0f });

    OpWorldCollisionAABB box(Vector3D{-120,-120,0}, Vector3D{120,120,200}, 0.6f, 0.1f);
    box.Operate(&collection, 0.016f);

    const Vector3D pos = collection.GetPosition(0);
    const Vector3D vel = collection.GetVelocity(0);

    assert(CheckFloatNear(pos.x, 115.0f));
    assert(CheckFloatNear(vel.x, -60.0f)); // -100 * 0.6
    assert(CheckFloatNear(vel.y, 45.0f));  // 50 * 0.9

    std::cout << "  Bounding box reflected X to -60.0 with position clamped to 115.0.\n";
    std::cout << "\n[PASS] 3D bounding box multi-planar confinement verified.\n";
}

// ----------------------------------------------------------------------------
// Test 9: Lifecycle Compaction & O(1) Swap-with-Last Invariant
// ----------------------------------------------------------------------------
void Test9_LifecycleCompaction() {
    PrintTestHeader(9, "Lifecycle Compaction & O(1) Swap-with-Last Invariant");

    CParticleCollection collection(10);
    collection.AllocateParticles(3);
    collection.SetLifeDuration(0, 1.0f);
    collection.SetLifeDuration(1, 3.0f);
    collection.SetLifeDuration(2, 1.0f);

    collection.m_pPosX[0] = 10.0f;
    collection.m_pPosX[1] = 20.0f;
    collection.m_pPosX[2] = 30.0f;

    OpFadeAndKill fadeKill;
    collection.AdvanceTime(1.5f);
    fadeKill.Operate(&collection, 1.5f);

    assert(collection.GetActiveParticleCount() == 1);
    assert(collection.m_pPosX[0] == 20.0f);

    std::cout << "  Killed particles 0 & 2; survivor particle 1 (tag 20) swapped to slot 0.\n";
    std::cout << "\n[PASS] O(1) swap-with-last compaction verified.\n";
}

// ----------------------------------------------------------------------------
// Test 10: Dynamic Billboard Hardware Vertex Generation
// ----------------------------------------------------------------------------
void Test10_BillboardVertexGeneration() {
    PrintTestHeader(10, "Dynamic Billboard Hardware Vertex Generation");

    CParticleCollection collection(10);
    collection.AllocateParticles(1);
    collection.SetPosition(0, { 50.0f, 0.0f, 25.0f });
    collection.SetRadius(0, 10.0f);
    collection.SetColor(0, { 1.0f, 0.8f, 0.2f, 0.9f });

    CParticleRendererSprite renderer;
    std::vector<ParticleQuadVertex> vertices;
    std::vector<uint32_t> indices;

    renderer.Render(&collection, {1,0,0}, {0,0,1}, vertices, indices);

    assert(vertices.size() == 4);
    assert(indices.size() == 6);
    assert(CheckFloatNear(vertices[0].m_vPosition.x, 40.0f));
    assert(CheckFloatNear(vertices[1].m_vPosition.x, 60.0f));

    std::cout << "  Generated 1 camera-facing quad: 4 vertices (36B each), 6 indices.\n";
    std::cout << "\n[PASS] Dynamic hardware billboard vertex generation verified.\n";
}

// ----------------------------------------------------------------------------
// Test 11: Scaled Execution Graph at 4096 Particles (Performance Invariant)
// ----------------------------------------------------------------------------
void Test11_ScaledExecutionGraph4096() {
    PrintTestHeader(11, "Scaled Execution Graph at 4096 Particles (Performance Invariant)");

    constexpr uint32_t kMaxP = 4096;
    CParticleSystemDefinition def("vortex_smoke_composite", kMaxP);

    def.AddEmitter(std::make_unique<EmitContinuousRate>(4000.0f));
    def.AddInitializer(std::make_unique<InitRadius>(3.0f, 8.0f));
    def.AddInitializer(std::make_unique<InitVelocityRandom>(Vector3D{-40,-40,40}, Vector3D{40,40,120}));
    def.AddInitializer(std::make_unique<InitColorRandom>(ColorRGBA{0.5f,0.6f,0.7f,0.8f}, ColorRGBA{0.9f,0.9f,0.9f,1.0f}));
    def.AddInitializer(std::make_unique<InitLifeTime>(1.0f, 2.0f));

    def.AddOperator(std::make_unique<OpMovementBasic>(Vector3D{0,0,-200}, 0.05f));
    def.AddOperator(std::make_unique<OpTurbulenceNoise>(0.03f, 60.0f));
    def.AddOperator(std::make_unique<OpVortexRing>(Vector3D{0,0,35}, Vector3D{0,0,1}, 30.0f, 8.0f, 100.0f, 50.0f));
    def.AddOperator(std::make_unique<OpWorldCollisionAABB>(Vector3D{-120,-120,0}, Vector3D{120,120,200}, 0.5f, 0.2f));
    def.AddOperator(std::make_unique<OpFadeAndKill>(0.1f, 0.8f, 1.0f));
    def.SetRenderer(std::make_unique<CParticleRendererSprite>());

    CParticleCollection collection(kMaxP);

    constexpr float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < 60; ++frame) { // 1.0s simulation
        def.Step(&collection, dt);
    }

    const uint32_t active = collection.GetActiveParticleCount();
    std::cout << "  Simulated 60 frames @ 60 Hz with 4096 particle capacity.\n";
    std::cout << "  Active particles: " << active << " / " << kMaxP << '\n';
    assert(active > 1000 && active <= kMaxP);

    std::vector<ParticleQuadVertex> v;
    std::vector<uint32_t> idx;
    def.Render(&collection, {1,0,0}, {0,0,1}, v, idx);
    assert(v.size() == active * 4);
    assert(idx.size() == active * 6);

    std::cout << "  Rendered " << v.size() / 4 << " billboards (" << v.size() << " vertices) at 60Hz.\n";
    std::cout << "\n[PASS] Scaled execution graph at 4096 particles verified.\n";
}

// ----------------------------------------------------------------------------
// Main Runner
// ----------------------------------------------------------------------------
int main() {
    PrintSeparator('=');
    std::cout << "  Counter-Strike 2 (Source 2) Particle Dynamics & 3D Compute Engine\n";
    std::cout << "  Comprehensive C++20 Verification Suite (tests/test_cs2_particle_engine_sim)\n";
    PrintSeparator('=');

    Test1_SoA4096MemoryAlignment();
    Test2_EmitterDynamics();
    Test3_Initializers();
    Test4_SymplecticEulerODE();
    Test5_CurlTurbulenceIncompressibility();
    Test6_ToroidalVortexRing();
    Test7_PlanarCollision();
    Test8_BoundingBoxConfinement();
    Test9_LifecycleCompaction();
    Test10_BillboardVertexGeneration();
    Test11_ScaledExecutionGraph4096();

    PrintSeparator('=');
    std::cout << "  ALL 11 / 11 C++20 VERIFICATION TESTS PASSED SUCCESSFULLY!\n";
    PrintSeparator('=');

    return 0;
}
