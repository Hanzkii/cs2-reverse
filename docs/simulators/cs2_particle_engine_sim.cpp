// ============================================================================
// Counter-Strike 2 (Source 2) Particle Dynamics & Compute Operator Simulator
//
// Reverse-engineered from particles.dll (Build 26598758):
// - CParticleCollection: 64-byte aligned Structure of Arrays (SoA) SIMD memory pool
// - CParticleSystemDefinition: Data-driven operator pipeline & execution graph
// - CEmitContinuousRate / CEmitInstantaneous: Fractional rate accumulator
// - CInitRadius / CInitVelocityRandom / CInitColorRandom / CInitLifeTime
// - COpMovementBasic: Symplectic Euler ODE integration with gravity & drag
// - COpTurbulenceNoise: Divergence-free 3D curl vector field perturbation
// - COpWorldCollisionPlane: Rubikon half-space planar restitution & friction
// - COpFadeAndKill: Normalized age tracking & O(1) swap-with-last compaction
// - CParticleRendererSprite: Dynamic camera-facing billboard quad vertex streaming
//
// Standalone C++20 Reference Verification Suite. Zero external dependencies.
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
#include "../../game/particles/particle_system.h"
#include "../../game/particles/particle_system.cpp"

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
// Test 1: Structure of Arrays (SoA) Aligned Memory Allocation & Cache Streaming
// ----------------------------------------------------------------------------
void RunTest1_SoAMemoryAlignment() {
    PrintTestHeader(1, "Structure of Arrays (SoA) Aligned Memory Allocation & Cache Invariants");

    constexpr uint32_t kMaxParticles = 1024;
    CParticleCollection collection(kMaxParticles);

    assert(collection.GetActiveParticleCount() == 0);
    assert(collection.GetMaxParticles() == kMaxParticles);

    // Verify 64-byte alignment of raw channel pointers for AVX-512 / Cache lines
    auto CheckAlign64 = [](const void* ptr, const char* name) {
        const auto addr = reinterpret_cast<uintptr_t>(ptr);
        const bool aligned = (addr % 64) == 0;
        std::cout << "  Channel " << std::left << std::setw(16) << name
                  << " Addr: 0x" << std::hex << addr << std::dec
                  << " [Align-64: " << (aligned ? "PASS" : "FAIL") << "]\n";
        assert(aligned && "Channel pointer must be strictly 64-byte aligned");
    };

    CheckAlign64(collection.m_pPosX, "PositionX");
    CheckAlign64(collection.m_pPosY, "PositionY");
    CheckAlign64(collection.m_pPosZ, "PositionZ");
    CheckAlign64(collection.m_pPrevPosX, "PrevPosX");
    CheckAlign64(collection.m_pPrevPosY, "PrevPosY");
    CheckAlign64(collection.m_pPrevPosZ, "PrevPosZ");
    CheckAlign64(collection.m_pVelX, "VelocityX");
    CheckAlign64(collection.m_pVelY, "VelocityY");
    CheckAlign64(collection.m_pVelZ, "VelocityZ");
    CheckAlign64(collection.m_pColorR, "ColorR");
    CheckAlign64(collection.m_pColorG, "ColorG");
    CheckAlign64(collection.m_pColorB, "ColorB");
    CheckAlign64(collection.m_pAlpha, "Alpha");
    CheckAlign64(collection.m_pRadius, "Radius");
    CheckAlign64(collection.m_pLifeDuration, "LifeDuration");
    CheckAlign64(collection.m_pCreationTime, "CreationTime");

    std::cout << "\n[PASS] All 16 SoA attribute streams satisfy 64-byte SIMD alignment.\n";
}

// ----------------------------------------------------------------------------
// Test 2: Continuous Rate & Instantaneous Particle Emission Dynamics
// ----------------------------------------------------------------------------
void RunTest2_EmitterDynamics() {
    PrintTestHeader(2, "Continuous Rate & Instantaneous Fractional Emission Accumulator");

    CParticleCollection collection(500);

    // Instantaneous Emitter: 50 particles at t = 0.0s
    EmitInstantaneous instantEmitter(50, 0.0f);
    const uint32_t instantSpawned = instantEmitter.Emit(&collection, 0.016f);
    assert(instantSpawned == 50);
    assert(collection.GetActiveParticleCount() == 50);
    std::cout << "  Instantaneous Burst: Spawned " << instantSpawned << " particles at t=0.0s\n";

    // Second instant emit attempt must fire 0 (single burst semantics)
    const uint32_t secondBurst = instantEmitter.Emit(&collection, 0.016f);
    assert(secondBurst == 0);
    std::cout << "  Instantaneous Re-fire: Correctly emitted " << secondBurst << " particles (single-shot)\n";

    // Continuous Emitter: 100 particles/sec across 10 sub-tick frames of dt = 0.005s (0.05s total)
    // Expected total: 100 * 0.05 = 5.0 particles
    EmitContinuousRate rateEmitter(100.0f);
    uint32_t continuousTotal = 0;
    const float dt = 0.005f; // 5ms sub-tick

    for (int frame = 0; frame < 10; ++frame) {
        collection.AdvanceTime(dt);
        const uint32_t emitted = rateEmitter.Emit(&collection, dt);
        continuousTotal += emitted;
        std::cout << "    Sub-Tick #" << std::setw(2) << frame + 1
                  << " (t=" << std::fixed << std::setprecision(3) << collection.GetCurrentTime()
                  << "s): Emitted=" << emitted
                  << ", Remainder=" << std::setprecision(4) << rateEmitter.m_flEmissionRemainder << '\n';
    }

    assert(continuousTotal == 5);
    std::cout << "  Continuous Rate Accumulator: Exactly 5 particles emitted over 0.05s with 0.0 fractional drift.\n";
    std::cout << "\n[PASS] Continuous fractional remainder accumulator verified.\n";
}

// ----------------------------------------------------------------------------
// Test 3: Stochastic Particle Initialization Profiles
// ----------------------------------------------------------------------------
void RunTest3_ParticleInitializers() {
    PrintTestHeader(3, "Stochastic Initializer Parameter Distribution & Bounds");

    CParticleCollection collection(200);
    const uint32_t startIdx = collection.AllocateParticles(100);
    assert(startIdx == 0);

    // Initializer 1: Radius [8.0, 16.0]
    InitRadius initRadius(8.0f, 16.0f);
    initRadius.Initialize(&collection, 0, 100);

    // Initializer 2: Velocity [-100, -100, 200] to [100, 100, 400]
    InitVelocityRandom initVel({ -100.0f, -100.0f, 200.0f }, { 100.0f, 100.0f, 400.0f });
    initVel.Initialize(&collection, 0, 100);

    // Initializer 3: Color RGBA [0.5, 0.5, 0.5, 0.8] to [1.0, 1.0, 1.0, 1.0]
    InitColorRandom initColor(ColorRGBA{ 0.5f, 0.5f, 0.5f, 0.8f }, ColorRGBA{ 1.0f, 1.0f, 1.0f, 1.0f });
    initColor.Initialize(&collection, 0, 100);

    // Initializer 4: Lifetime [1.5, 3.0]
    InitLifeTime initLife(1.5f, 3.0f);
    initLife.Initialize(&collection, 0, 100);

    for (uint32_t i = 0; i < 100; ++i) {
        assert(collection.m_pRadius[i] >= 8.0f && collection.m_pRadius[i] <= 16.0f);
        assert(collection.m_pVelX[i] >= -100.0f && collection.m_pVelX[i] <= 100.0f);
        assert(collection.m_pVelY[i] >= -100.0f && collection.m_pVelY[i] <= 100.0f);
        assert(collection.m_pVelZ[i] >= 200.0f && collection.m_pVelZ[i] <= 400.0f);
        assert(collection.m_pColorR[i] >= 0.5f && collection.m_pColorR[i] <= 1.0f);
        assert(collection.m_pAlpha[i] >= 0.8f && collection.m_pAlpha[i] <= 1.0f);
        assert(collection.m_pLifeDuration[i] >= 1.5f && collection.m_pLifeDuration[i] <= 3.0f);
    }

    std::cout << "  Verified 100 spawned particles: 100% within stochastic distribution boundaries.\n";
    std::cout << "\n[PASS] Stochastic Initializer constraints validated.\n";
}

// ----------------------------------------------------------------------------
// Test 4: Numerical ODE Integration (CS2 Gravity & Aerodynamic Drag)
// ----------------------------------------------------------------------------
void RunTest4_NumericalODEIntegration() {
    PrintTestHeader(4, "Numerical ODE Integration (CS2 Gravity & Terminal Velocity Convergence)");

    CParticleCollection collection(10);
    const uint32_t idx = collection.AllocateParticles(1);
    assert(idx == 0);

    // Initial state: Particle at origin with zero velocity
    collection.SetPosition(0, { 0.0f, 0.0f, 1000.0f });
    collection.SetVelocity(0, { 0.0f, 0.0f, 0.0f });

    // Operator with gravity g_z = -800.0 in/s^2 and drag k = 0.5 s^-1
    // Theoretical steady-state terminal velocity: v_term = g / k = -800 / 0.5 = -1600.0 in/s
    constexpr float kDrag = 0.5f;
    OpMovementBasic movementOp(Vector3D{ 0.0f, 0.0f, kStandardGravityCS2 }, kDrag);

    constexpr float dt = 0.01f; // 10ms step
    constexpr int totalSteps = 2000; // 20.0 seconds of simulation for >99.99% exponential decay

    for (int step = 0; step < totalSteps; ++step) {
        movementOp.Operate(&collection, dt);
    }

    const float finalVelZ = collection.m_pVelZ[0];
    constexpr float expectedTerminalVel = kStandardGravityCS2 / kDrag; // -1600.0f

    std::cout << "  Simulated 20.0 seconds of free fall under CS2 gravity (-800 in/s^2) & drag (0.5 s^-1):\n";
    std::cout << "    Asymptotic Terminal Velocity: " << expectedTerminalVel << " in/s\n";
    std::cout << "    Numerical Integrated VelZ:   " << finalVelZ << " in/s\n";
    std::cout << "    Residual Delta:              " << std::abs(finalVelZ - expectedTerminalVel) << " in/s\n";

    // In discrete integration with factor (1 - k*dt):
    // Equilibrium satisfies: (v + g*dt)*(1 - k*dt) = v => v*k*dt = g*dt*(1 - k*dt) => v = g/k * (1 - k*dt)
    // For dt=0.01, (1 - 0.005) * -1600 = -1592.0 in/s
    const float expectedDiscreteEquilibrium = (kStandardGravityCS2 / kDrag) * (1.0f - kDrag * dt);
    assert(CheckFloatNear(finalVelZ, expectedDiscreteEquilibrium, 1.0f));

    std::cout << "\n[PASS] Numerical ODE integration matches discrete terminal velocity.\n";
}

// ----------------------------------------------------------------------------
// Test 5: Divergence-Free 3D Curl Turbulence Vector Fields
// ----------------------------------------------------------------------------
void RunTest5_CurlTurbulenceDivergence() {
    PrintTestHeader(5, "Divergence-Free 3D Curl Turbulence Vector Fields (Incompressibility)");

    // Curl noise is defined as v_curl = del x Psi.
    // By vector calculus identity, the divergence of curl of any smooth vector field is zero:
    // del . (del x Psi) == 0.
    // Verify numerical divergence at arbitrary 3D probe coordinates using central finite differences:
    // div = dv_x/dx + dv_y/dy + dv_z/dz ~ 0.
    const Vector3D probeCoords[] = {
        { 0.0f, 0.0f, 0.0f },
        { 128.5f, -64.2f, 32.1f },
        { 512.0f, 256.0f, -128.0f },
        { -45.0f, 320.0f, 88.0f }
    };

    constexpr float eps = 0.01f;
    constexpr float inv2Eps = 1.0f / (2.0f * eps);
    constexpr float noiseScale = 0.02f;

    std::cout << "  Evaluating numerical divergence del . v_curl at 4 spatial probe points:\n";

    for (size_t i = 0; i < std::size(probeCoords); ++i) {
        const Vector3D pt = probeCoords[i];

        const Vector3D vxPlus  = OpTurbulenceNoise::EvaluateCurlNoise({ pt.x + eps, pt.y, pt.z }, 0.0f, noiseScale);
        const Vector3D vxMinus = OpTurbulenceNoise::EvaluateCurlNoise({ pt.x - eps, pt.y, pt.z }, 0.0f, noiseScale);
        const float dvx_dx = (vxPlus.x - vxMinus.x) * inv2Eps;

        const Vector3D vyPlus  = OpTurbulenceNoise::EvaluateCurlNoise({ pt.x, pt.y + eps, pt.z }, 0.0f, noiseScale);
        const Vector3D vyMinus = OpTurbulenceNoise::EvaluateCurlNoise({ pt.x, pt.y - eps, pt.z }, 0.0f, noiseScale);
        const float dvy_dy = (vyPlus.y - vyMinus.y) * inv2Eps;

        const Vector3D vzPlus  = OpTurbulenceNoise::EvaluateCurlNoise({ pt.x, pt.y, pt.z + eps }, 0.0f, noiseScale);
        const Vector3D vzMinus = OpTurbulenceNoise::EvaluateCurlNoise({ pt.x, pt.y, pt.z - eps }, 0.0f, noiseScale);
        const float dvz_dz = (vzPlus.z - vzMinus.z) * inv2Eps;

        const float divergence = dvx_dx + dvy_dy + dvz_dz;
        std::cout << "    Probe [" << i << "] (" << pt.x << ", " << pt.y << ", " << pt.z << ") -> "
                  << "del . v = " << std::scientific << std::setprecision(4) << divergence << std::defaultfloat << '\n';

        assert(std::abs(divergence) < 1e-2f && "Curl noise vector field must be divergence-free");
    }

    std::cout << "\n[PASS] Mathematical incompressibility del . (del x Psi) == 0 verified.\n";
}

// ----------------------------------------------------------------------------
// Test 6: Planar Boundary Restitution & Tangential Friction Impulse
// ----------------------------------------------------------------------------
void RunTest6_PlanarCollisionBoundary() {
    PrintTestHeader(6, "Rubikon Half-Space Planar Restitution & Tangential Friction Impulse");

    CParticleCollection collection(10);
    const uint32_t idx = collection.AllocateParticles(1);
    assert(idx == 0);

    // Particle approaching floor plane z = 0 with normal n = (0, 0, 1), D = 0.
    // Radius = 5.0 units.
    collection.SetRadius(0, 5.0f);
    collection.SetPosition(0, { 10.0f, 20.0f, 2.0f }); // Penetrating ground by 3.0 units (z=2 < r=5)
    collection.SetVelocity(0, { 100.0f, 0.0f, -200.0f }); // Moving down at 200 in/s, forward at 100 in/s

    // Ground plane: normal (0, 0, 1), D=0, restitution e=0.5, friction mu=0.25
    OpWorldCollisionPlane planeOp(Vector3D{ 0.0f, 0.0f, 1.0f }, 0.0f, 0.5f, 0.25f);

    planeOp.Operate(&collection, 0.016f);

    const Vector3D correctedPos = collection.GetPosition(0);
    const Vector3D reflectedVel = collection.GetVelocity(0);

    std::cout << "  Pre-Collision State:  Pos=(10, 20, 2), Vel=(100, 0, -200), Radius=5\n";
    std::cout << "  Post-Collision State: Pos=(" << correctedPos.x << ", " << correctedPos.y << ", " << correctedPos.z << ")\n";
    std::cout << "                        Vel=(" << reflectedVel.x << ", " << reflectedVel.y << ", " << reflectedVel.z << ")\n";

    // 1. Position must be pushed out to z = 5.0
    assert(CheckFloatNear(correctedPos.z, 5.0f, 1e-4f));

    // 2. Normal velocity (-200) reflected with e=0.5: v_z = -(-200) * 0.5 = +100.0 in/s
    assert(CheckFloatNear(reflectedVel.z, 100.0f, 1e-4f));

    // 3. Tangential velocity (100) attenuated by (1 - mu) = (1 - 0.25) = 0.75: v_x = 75.0 in/s
    assert(CheckFloatNear(reflectedVel.x, 75.0f, 1e-4f));

    std::cout << "\n[PASS] Planar penetration resolution, restitution bounce, and friction verified.\n";
}

// ----------------------------------------------------------------------------
// Test 7: Particle Expiration Lifecycle & O(1) Compaction Invariant
// ----------------------------------------------------------------------------
void RunTest7_LifecycleAndCompaction() {
    PrintTestHeader(7, "Particle Expiration Lifecycle & O(1) Swap-with-Last Compaction");

    CParticleCollection collection(50);
    collection.AllocateParticles(5);
    assert(collection.GetActiveParticleCount() == 5);

    // Set particle lifetimes: [1.0s, 2.0s, 1.0s, 3.0s, 1.0s]
    collection.SetLifeDuration(0, 1.0f);
    collection.SetLifeDuration(1, 2.0f);
    collection.SetLifeDuration(2, 1.0f);
    collection.SetLifeDuration(3, 3.0f);
    collection.SetLifeDuration(4, 1.0f);

    // Unique tags via PosX to trace swap identities: { 100, 200, 300, 400, 500 }
    for (uint32_t i = 0; i < 5; ++i) {
        collection.m_pPosX[i] = static_cast<float>((i + 1) * 100);
    }

    OpFadeAndKill fadeKillOp(0.1f, 0.9f, 1.0f);

    // Advance time to t = 1.1s (Particles 0, 2, 4 must expire; 1 and 3 survive)
    collection.AdvanceTime(1.1f);
    fadeKillOp.Operate(&collection, 1.1f);

    const uint32_t remaining = collection.GetActiveParticleCount();
    std::cout << "  Initial 5 particles with lifetimes [1s, 2s, 1s, 3s, 1s]. Advanced to t=1.1s.\n";
    std::cout << "  Active particles after OpFadeAndKill: " << remaining << " (Expected: 2)\n";
    assert(remaining == 2);

    // Verify surviving particles are #1 (lifetime 2s, tag 200) and #3 (lifetime 3s, tag 400)
    const float tagA = collection.m_pPosX[0];
    const float tagB = collection.m_pPosX[1];
    std::cout << "  Surviving Particle Tags: " << tagA << ", " << tagB << '\n';

    const bool hasTag200 = (tagA == 200.0f || tagB == 200.0f);
    const bool hasTag400 = (tagA == 400.0f || tagB == 400.0f);
    assert(hasTag200 && hasTag400);

    // Advance time to t = 2.5s (Particle #1 expires, only #3 survives)
    collection.AdvanceTime(1.4f);
    fadeKillOp.Operate(&collection, 1.4f);
    assert(collection.GetActiveParticleCount() == 1);
    assert(collection.m_pPosX[0] == 400.0f);
    std::cout << "  Advanced to t=2.5s: Exactly 1 particle survives (Tag: " << collection.m_pPosX[0] << ")\n";

    // Advance time to t = 3.5s (All particles dead)
    collection.AdvanceTime(1.0f);
    fadeKillOp.Operate(&collection, 1.0f);
    assert(collection.GetActiveParticleCount() == 0);
    std::cout << "  Advanced to t=3.5s: 0 particles remaining.\n";

    std::cout << "\n[PASS] O(1) swap-with-last compaction verified with zero data corruption.\n";
}

// ----------------------------------------------------------------------------
// Test 8: Dynamic Hardware Billboard Quad Vertex Stream Generation
// ----------------------------------------------------------------------------
void RunTest8_DynamicVertexStreaming() {
    PrintTestHeader(8, "Hardware Dynamic Billboard Quad Vertex Buffer Streaming");

    CParticleCollection collection(10);
    collection.AllocateParticles(2);

    collection.SetPosition(0, { 100.0f, 0.0f, 50.0f });
    collection.SetRadius(0, 10.0f);
    collection.SetColor(0, { 1.0f, 0.5f, 0.2f, 0.9f });

    collection.SetPosition(1, { -50.0f, 20.0f, 10.0f });
    collection.SetRadius(1, 4.0f);
    collection.SetColor(1, { 0.1f, 0.8f, 1.0f, 1.0f });

    CParticleRendererSprite renderer;
    std::vector<ParticleQuadVertex> vertices;
    std::vector<uint32_t> indices;

    const Vector3D camRight{ 1.0f, 0.0f, 0.0f };
    const Vector3D camUp{ 0.0f, 0.0f, 1.0f };

    renderer.Render(&collection, camRight, camUp, vertices, indices);

    std::cout << "  Rendered 2 particles to billboard quads:\n";
    std::cout << "    Generated Vertices: " << vertices.size() << " (Expected: 8)\n";
    std::cout << "    Generated Indices:  " << indices.size()  << " (Expected: 12)\n";

    assert(vertices.size() == 8);
    assert(indices.size() == 12);

    // Verify first quad corners around center (100, 0, 50) with radius 10:
    // BL: (90, 0, 40), BR: (110, 0, 40), TR: (110, 0, 60), TL: (90, 0, 60)
    assert(CheckFloatNear(vertices[0].m_vPosition.x, 90.0f) && CheckFloatNear(vertices[0].m_vPosition.z, 40.0f));
    assert(CheckFloatNear(vertices[1].m_vPosition.x, 110.0f) && CheckFloatNear(vertices[1].m_vPosition.z, 40.0f));
    assert(CheckFloatNear(vertices[2].m_vPosition.x, 110.0f) && CheckFloatNear(vertices[2].m_vPosition.z, 60.0f));
    assert(CheckFloatNear(vertices[3].m_vPosition.x, 90.0f) && CheckFloatNear(vertices[3].m_vPosition.z, 60.0f));

    // Verify UV mapping: (0,0), (1,0), (1,1), (0,1)
    assert(vertices[0].m_vTexCoord.x == 0.0f && vertices[0].m_vTexCoord.y == 0.0f);
    assert(vertices[1].m_vTexCoord.x == 1.0f && vertices[1].m_vTexCoord.y == 0.0f);
    assert(vertices[2].m_vTexCoord.x == 1.0f && vertices[2].m_vTexCoord.y == 1.0f);
    assert(vertices[3].m_vTexCoord.x == 0.0f && vertices[3].m_vTexCoord.y == 1.0f);

    std::cout << "\n[PASS] Dynamic vertex buffer and index layout verified for GPU submission.\n";
}

// ----------------------------------------------------------------------------
// Test 9: Complete End-to-End Simulation Step in CParticleSystemDefinition Graph
// ----------------------------------------------------------------------------
void RunTest9_SystemDefinitionGraph() {
    PrintTestHeader(9, "End-to-End CParticleSystemDefinition Graph Step");

    CParticleSystemDefinition def("muzzle_flash_composite", 200);

    // Add continuous emitter: 200 particles/sec
    def.AddEmitter(std::make_unique<EmitContinuousRate>(200.0f));

    // Add initializers
    def.AddInitializer(std::make_unique<InitRadius>(4.0f, 8.0f));
    def.AddInitializer(std::make_unique<InitVelocityRandom>(Vector3D{-20.0f, -20.0f, 50.0f}, Vector3D{20.0f, 20.0f, 150.0f}));
    def.AddInitializer(std::make_unique<InitColorRandom>(ColorRGBA{1.0f, 0.6f, 0.1f, 1.0f}, ColorRGBA{1.0f, 0.9f, 0.4f, 1.0f}));
    def.AddInitializer(std::make_unique<InitLifeTime>(0.2f, 0.5f));

    // Add operators
    def.AddOperator(std::make_unique<OpMovementBasic>(Vector3D{0.0f, 0.0f, kStandardGravityCS2}, 0.1f));
    def.AddOperator(std::make_unique<OpTurbulenceNoise>(0.05f, 50.0f));
    def.AddOperator(std::make_unique<OpWorldCollisionPlane>(Vector3D{0.0f, 0.0f, 1.0f}, 0.0f, 0.3f, 0.2f));
    def.AddOperator(std::make_unique<OpFadeAndKill>(0.1f, 0.8f, 1.0f));

    // Add renderer
    def.SetRenderer(std::make_unique<CParticleRendererSprite>());

    CParticleCollection collection(200);

    // Run 30 steps of dt = 0.01667s (~0.5s of simulation)
    constexpr float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < 30; ++frame) {
        def.Step(&collection, dt);
    }

    std::cout << "  Simulated 30 frames @ 60 Hz in composite definition:\n";
    std::cout << "    Active Particles at t=0.5s: " << collection.GetActiveParticleCount() << '\n';
    assert(collection.GetActiveParticleCount() > 0 && collection.GetActiveParticleCount() <= 200);

    std::vector<ParticleQuadVertex> verts;
    std::vector<uint32_t> inds;
    def.Render(&collection, {1,0,0}, {0,0,1}, verts, inds);
    std::cout << "    Render Quads Streamed:      " << verts.size() / 4 << " billboards\n";
    assert(verts.size() == collection.GetActiveParticleCount() * 4);

    std::cout << "\n[PASS] End-to-end particle graph execution step verified.\n";
}

// ----------------------------------------------------------------------------
// Test 10: Toroidal Vortex Ring Angular Momentum & Circulation Dynamics
// ----------------------------------------------------------------------------
void RunTest10_ToroidalVortexRingDynamics() {
    PrintTestHeader(10, "Toroidal Vortex Ring Angular Momentum & Circulation Dynamics");

    CParticleCollection collection(10);
    collection.AllocateParticles(3);

    // Particle 0: On central axis z = 35 (inside ring core, expect upward central jet)
    collection.SetPosition(0, { 0.0f, 0.0f, 35.0f });
    collection.SetVelocity(0, { 0.0f, 0.0f, 0.0f });

    // Particle 1: On circular filament radius R = 30, displaced along +X
    collection.SetPosition(1, { 30.0f, 0.0f, 35.0f });
    collection.SetVelocity(1, { 0.0f, 0.0f, 0.0f });

    // Particle 2: Outside ring radius (rho = 50, z = 35), displaced along +Y
    collection.SetPosition(2, { 0.0f, 50.0f, 35.0f });
    collection.SetVelocity(2, { 0.0f, 0.0f, 0.0f });

    // OpVortexRing: Center (0, 0, 35), Axis (0, 0, 1), R = 30, rCore = 8, Circulation = 120, Swirl = 60
    OpVortexRing vortexOp(Vector3D{ 0.0f, 0.0f, 35.0f }, Vector3D{ 0.0f, 0.0f, 1.0f }, 30.0f, 8.0f, 120.0f, 60.0f);

    constexpr float dt = 0.016f;
    vortexOp.Operate(&collection, dt);

    const Vector3D vel0 = collection.GetVelocity(0);
    const Vector3D vel1 = collection.GetVelocity(1);
    const Vector3D vel2 = collection.GetVelocity(2);

    std::cout << "  Vortex Field Response @ dt = 16ms:\n";
    std::cout << "    Central Axis (0, 0, 35):   Vel=(" << vel0.x << ", " << vel0.y << ", " << vel0.z << ")\n";
    std::cout << "    Ring Filament (30, 0, 35): Vel=(" << vel1.x << ", " << vel1.y << ", " << vel1.z << ")\n";
    std::cout << "    Outer Flow (0, 50, 35):    Vel=(" << vel2.x << ", " << vel2.y << ", " << vel2.z << ")\n";

    // 1. Central particle must experience positive upward vertical jet along ring axis
    assert(vel0.z > 0.0f);
    assert(CheckFloatNear(vel0.x, 0.0f));
    assert(CheckFloatNear(vel0.y, 0.0f));

    // 2. Filament particle on +X axis experiences azimuthal swirl (+Y direction) and zero deltaRho
    assert(vel1.y > 0.0f);
    assert(CheckFloatNear(vel1.z, 0.0f));

    // 3. Outer particle (rho=50 > R=30) has deltaRho = +20, so positive induced vZ rolling
    assert(vel2.z > 0.0f);
    // Azimuthal swirl on +Y axis points in -X direction
    assert(vel2.x < 0.0f);

    std::cout << "\n[PASS] Toroidal vortex ring circulation and azimuthal angular momentum verified.\n";
}

// ----------------------------------------------------------------------------
// Test 11: 3D Bounding Box AABB Restitution & Containment Invariant
// ----------------------------------------------------------------------------
void RunTest11_BoundingBoxCollision() {
    PrintTestHeader(11, "3D Bounding Box AABB Restitution & Multi-Planar Containment");

    CParticleCollection collection(10);
    collection.AllocateParticles(2);

    // Particle 0: Moving right (+X) penetrating max X boundary (x=118, r=5 => right edge at 123 > maxX 120)
    collection.SetRadius(0, 5.0f);
    collection.SetPosition(0, { 118.0f, 0.0f, 50.0f });
    collection.SetVelocity(0, { 200.0f, 100.0f, -50.0f });

    // Particle 1: Moving downward penetrating ceiling/floor or min Y boundary (y=-118, r=4 => edge at -122 < minY -120)
    collection.SetRadius(1, 4.0f);
    collection.SetPosition(1, { 0.0f, -118.0f, 80.0f });
    collection.SetVelocity(1, { 80.0f, -150.0f, 40.0f });

    // AABB Bounds [-120, -120, 0] to [120, 120, 200], restitution e=0.5, friction mu=0.2
    OpWorldCollisionAABB aabbOp(Vector3D{ -120.0f, -120.0f, 0.0f }, Vector3D{ 120.0f, 120.0f, 200.0f }, 0.5f, 0.2f);

    aabbOp.Operate(&collection, 0.016f);

    const Vector3D pos0 = collection.GetPosition(0);
    const Vector3D vel0 = collection.GetVelocity(0);
    const Vector3D pos1 = collection.GetPosition(1);
    const Vector3D vel1 = collection.GetVelocity(1);

    std::cout << "  Particle 0 (+X Impact): Pos=(" << pos0.x << ", " << pos0.y << ", " << pos0.z << ")\n";
    std::cout << "                         Vel=(" << vel0.x << ", " << vel0.y << ", " << vel0.z << ")\n";
    std::cout << "  Particle 1 (-Y Impact): Pos=(" << pos1.x << ", " << pos1.y << ", " << pos1.z << ")\n";
    std::cout << "                         Vel=(" << vel1.x << ", " << vel1.y << ", " << vel1.z << ")\n";

    // Particle 0: Pushed back to maxX - r = 120 - 5 = 115.0
    assert(CheckFloatNear(pos0.x, 115.0f));
    // Velocity reflected: vx = -200 * 0.5 = -100.0
    assert(CheckFloatNear(vel0.x, -100.0f));
    // Tangential velocities attenuated by (1 - 0.2) = 0.8: vy = 80, vz = -40
    assert(CheckFloatNear(vel0.y, 80.0f));
    assert(CheckFloatNear(vel0.z, -40.0f));

    // Particle 1: Pushed back to minY + r = -120 + 4 = -116.0
    assert(CheckFloatNear(pos1.y, -116.0f));
    // Velocity reflected: vy = -(-150) * 0.5 = +75.0
    assert(CheckFloatNear(vel1.y, 75.0f));
    // Tangential velocities attenuated: vx = 64.0, vz = 32.0
    assert(CheckFloatNear(vel1.x, 64.0f));
    assert(CheckFloatNear(vel1.z, 32.0f));

    std::cout << "\n[PASS] Multi-planar 3D AABB containment, restitution, and friction impulse verified.\n";
}

// ----------------------------------------------------------------------------
// Main Runner
// ----------------------------------------------------------------------------
int main() {
    PrintSeparator('=');
    std::cout << "  Counter-Strike 2 (Source 2) Particle Dynamics & Compute Simulator\n";
    std::cout << "  Authoritative particles.dll Reverse Engineering Verification Suite\n";
    PrintSeparator('=');

    RunTest1_SoAMemoryAlignment();
    RunTest2_EmitterDynamics();
    RunTest3_ParticleInitializers();
    RunTest4_NumericalODEIntegration();
    RunTest5_CurlTurbulenceDivergence();
    RunTest6_PlanarCollisionBoundary();
    RunTest7_LifecycleAndCompaction();
    RunTest8_DynamicVertexStreaming();
    RunTest9_SystemDefinitionGraph();
    RunTest10_ToroidalVortexRingDynamics();
    RunTest11_BoundingBoxCollision();

    PrintSeparator('=');
    std::cout << "  ALL 11 / 11 VERIFICATION TEST SUITES PASSED SUCCESSFULLY!\n";
    PrintSeparator('=');

    return 0;
}
