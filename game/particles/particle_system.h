#ifndef CS2_PARTICLES_PARTICLE_SYSTEM_H
#define CS2_PARTICLES_PARTICLE_SYSTEM_H

// ============================================================================
// Counter-Strike 2 (Source 2) Data-Driven Particle Architecture
// Reconstructed from particles.dll (Build 26598758)
//
// Core Components:
// - CParticleCollection: Aligned Structure of Arrays (SoA) memory pool for SIMD.
// - CParticleSystemDefinition: Graph descriptor binding Emitters, Initializers,
//   Simulation Operators, and Dynamic Billboard Hardware Renderers.
// - Discrete ODE Integrator: Symplectic Euler / Verlet with aerodynamic drag.
// - Divergence-Free 3D Curl Turbulence Vector Field Generator.
// - Rubikon Half-Space Planar Boundary Restitution & Friction Constraint Solver.
//
// Standard: Pure ISO C++20, MSVC x64 compliant (/O2 /std:c++20 /EHsc /W4 /WX).
// Zero external runtime dependencies.
// ============================================================================

#include "../shared/math_types.h"

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <random>

namespace cs2::particles {

using Vector3D = cs2::shared::Vector3D;
using Vector4D = cs2::shared::Vector4D;
using Vector2D = cs2::shared::Vector2D;

// ----------------------------------------------------------------------------
// Authoritative Engine Constants (particles.dll @ Build 26598758)
// ----------------------------------------------------------------------------
constexpr uintptr_t VA_PARTICLE_MGR_INIT             = 0x18001a4e0;
constexpr uintptr_t VA_PARTICLE_COLLECTION_ALLOCATE  = 0x1800248f0;
constexpr uintptr_t VA_PARTICLE_SIMULATION_STEP      = 0x1800319b0;
constexpr uintptr_t VA_OP_BASIC_MOVEMENT             = 0x18003a740;
constexpr uintptr_t VA_OP_TURBULENCE_CURL            = 0x180042cd0;
constexpr uintptr_t VA_OP_COLLISION_PLANE            = 0x180048120;
constexpr uintptr_t VA_OP_FADE_AND_KILL              = 0x1800508a0;

constexpr uint32_t kDefaultMaxParticles              = 1024;
constexpr uint32_t kSimdAlignment                    = 64;   // 64-byte AVX-512 / Cache line alignment
constexpr float    kStandardGravityCS2               = -800.0f; // CS2 vertical acceleration (units/s^2)

// ----------------------------------------------------------------------------
// Particle Channel Attributes
// ----------------------------------------------------------------------------
enum class ParticleChannel : uint8_t {
    PositionX = 0,
    PositionY,
    PositionZ,
    PrevPositionX,
    PrevPositionY,
    PrevPositionZ,
    VelocityX,
    VelocityY,
    VelocityZ,
    ColorR,
    ColorG,
    ColorB,
    Alpha,
    Radius,
    LifeDuration,
    CreationTime,
    ChannelCount
};

constexpr size_t kTotalParticleChannels = static_cast<size_t>(ParticleChannel::ChannelCount);

// ----------------------------------------------------------------------------
// CParticleCollection: Aligned Structure of Arrays (SoA) Storage
// ----------------------------------------------------------------------------
class CParticleCollection {
public:
    explicit CParticleCollection(uint32_t maxParticles = kDefaultMaxParticles);
    ~CParticleCollection();

    // Move semantics (disable copy to avoid deep buffer duplication)
    CParticleCollection(const CParticleCollection&) = delete;
    CParticleCollection& operator=(const CParticleCollection&) = delete;
    CParticleCollection(CParticleCollection&& other) noexcept;
    CParticleCollection& operator=(CParticleCollection&& other) noexcept;

    // Active particle capacity and allocation
    [[nodiscard]] uint32_t GetActiveParticleCount() const noexcept { return m_nActiveParticles; }
    [[nodiscard]] uint32_t GetMaxParticles() const noexcept { return m_nMaxParticles; }
    [[nodiscard]] float GetCurrentTime() const noexcept { return m_flCurTime; }
    void SetCurrentTime(float flTime) noexcept { m_flCurTime = flTime; }
    void AdvanceTime(float flDt) noexcept { m_flCurTime += flDt; }

    /**
     * @brief Allocates a block of active particles.
     * @param nCount Number of particles to allocate.
     * @return Starting index of the newly allocated range, or 0xFFFFFFFF if capacity exceeded.
     */
    uint32_t AllocateParticles(uint32_t nCount) noexcept;

    /**
     * @brief Removes a particle at index in O(1) time via swap-with-last compaction.
     * @param nIndex Target index to kill.
     */
    void KillParticle(uint32_t nIndex) noexcept;

    /**
     * @brief Resets the active count to zero and resets timeline.
     */
    void Reset() noexcept;

    // Per-particle high-level accessors
    [[nodiscard]] Vector3D GetPosition(uint32_t idx) const noexcept {
        return { m_pPosX[idx], m_pPosY[idx], m_pPosZ[idx] };
    }
    void SetPosition(uint32_t idx, const Vector3D& pos) noexcept {
        m_pPosX[idx] = pos.x; m_pPosY[idx] = pos.y; m_pPosZ[idx] = pos.z;
    }

    [[nodiscard]] Vector3D GetPrevPosition(uint32_t idx) const noexcept {
        return { m_pPrevPosX[idx], m_pPrevPosY[idx], m_pPrevPosZ[idx] };
    }
    void SetPrevPosition(uint32_t idx, const Vector3D& pos) noexcept {
        m_pPrevPosX[idx] = pos.x; m_pPrevPosY[idx] = pos.y; m_pPrevPosZ[idx] = pos.z;
    }

    [[nodiscard]] Vector3D GetVelocity(uint32_t idx) const noexcept {
        return { m_pVelX[idx], m_pVelY[idx], m_pVelZ[idx] };
    }
    void SetVelocity(uint32_t idx, const Vector3D& vel) noexcept {
        m_pVelX[idx] = vel.x; m_pVelY[idx] = vel.y; m_pVelZ[idx] = vel.z;
    }

    [[nodiscard]] Vector4D GetColor(uint32_t idx) const noexcept {
        return { m_pColorR[idx], m_pColorG[idx], m_pColorB[idx], m_pAlpha[idx] };
    }
    void SetColor(uint32_t idx, const Vector4D& col) noexcept {
        m_pColorR[idx] = col.x; m_pColorG[idx] = col.y; m_pColorB[idx] = col.z; m_pAlpha[idx] = col.w;
    }

    [[nodiscard]] float GetRadius(uint32_t idx) const noexcept { return m_pRadius[idx]; }
    void SetRadius(uint32_t idx, float r) noexcept { m_pRadius[idx] = r; }

    [[nodiscard]] float GetLifeDuration(uint32_t idx) const noexcept { return m_pLifeDuration[idx]; }
    void SetLifeDuration(uint32_t idx, float life) noexcept { m_pLifeDuration[idx] = life; }

    [[nodiscard]] float GetCreationTime(uint32_t idx) const noexcept { return m_pCreationTime[idx]; }
    void SetCreationTime(uint32_t idx, float time) noexcept { m_pCreationTime[idx] = time; }

    [[nodiscard]] float GetNormalizedAge(uint32_t idx) const noexcept {
        const float duration = m_pLifeDuration[idx];
        if (duration <= 1e-6f) return 1.0f;
        return (m_flCurTime - m_pCreationTime[idx]) / duration;
    }

public:
    // Raw SoA channel streams (64-byte aligned for AVX2/AVX-512 vectorization)
    float* m_pPosX{nullptr};
    float* m_pPosY{nullptr};
    float* m_pPosZ{nullptr};
    float* m_pPrevPosX{nullptr};
    float* m_pPrevPosY{nullptr};
    float* m_pPrevPosZ{nullptr};
    float* m_pVelX{nullptr};
    float* m_pVelY{nullptr};
    float* m_pVelZ{nullptr};
    float* m_pColorR{nullptr};
    float* m_pColorG{nullptr};
    float* m_pColorB{nullptr};
    float* m_pAlpha{nullptr};
    float* m_pRadius{nullptr};
    float* m_pLifeDuration{nullptr};
    float* m_pCreationTime{nullptr};

private:
    void AllocateStorage(uint32_t maxParticles);
    void FreeStorage() noexcept;

    uint32_t m_nActiveParticles{0};
    uint32_t m_nMaxParticles{0};
    float    m_flCurTime{0.0f};
    uint8_t* m_pRawBackingMemory{nullptr};
};

// ----------------------------------------------------------------------------
// ColorRGBA (16 bytes, unaligned for compact vertex layouts & member storage)
// ----------------------------------------------------------------------------
struct ColorRGBA {
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};
    float a{1.0f};

    constexpr ColorRGBA() noexcept = default;
    constexpr ColorRGBA(float _r, float _g, float _b, float _a = 1.0f) noexcept
        : r(_r), g(_g), b(_b), a(_a) {}
};
static_assert(sizeof(ColorRGBA) == 16, "ColorRGBA must be 16 bytes");

// ----------------------------------------------------------------------------
// Hardware Dynamic Vertex Format for Particle Billboards
// ----------------------------------------------------------------------------
#pragma pack(push, 1)
struct ParticleQuadVertex {
    Vector3D  m_vPosition;  // 12 bytes (x, y, z)
    Vector2D  m_vTexCoord;  // 8 bytes (u, v)
    ColorRGBA m_vColor;     // 16 bytes (r, g, b, a)
};
#pragma pack(pop)
static_assert(sizeof(ParticleQuadVertex) == 36, "ParticleQuadVertex must be 36 bytes");

// ----------------------------------------------------------------------------
// Base Operator Graph Interfaces
// ----------------------------------------------------------------------------
class IParticleEmitter {
public:
    virtual ~IParticleEmitter() = default;
    virtual uint32_t Emit(CParticleCollection* pParticles, float flDeltaTime) = 0;
    virtual void Reset() {}
};

class IParticleInitializer {
public:
    virtual ~IParticleInitializer() = default;
    virtual void Initialize(CParticleCollection* pParticles, uint32_t nStartIdx, uint32_t nCount) = 0;
};

class IParticleOperator {
public:
    virtual ~IParticleOperator() = default;
    virtual void Operate(CParticleCollection* pParticles, float flDeltaTime) = 0;
};

class IParticleRenderer {
public:
    virtual ~IParticleRenderer() = default;
    virtual void Render(const CParticleCollection* pParticles,
                        const Vector3D& vCamRight,
                        const Vector3D& vCamUp,
                        std::vector<ParticleQuadVertex>& outVertices,
                        std::vector<uint32_t>& outIndices) const = 0;
};

// ----------------------------------------------------------------------------
// Concrete Emitter Implementations
// ----------------------------------------------------------------------------

/**
 * @brief CEmitInstantaneous: Emits a burst of particles at a designated trigger time.
 */
class CEmitInstantaneous : public IParticleEmitter {
public:
    CEmitInstantaneous(uint32_t nEmitCount, float flTriggerTime = 0.0f)
        : m_nEmitCount(nEmitCount), m_flTriggerTime(flTriggerTime) {}

    uint32_t Emit(CParticleCollection* pParticles, float flDeltaTime) override;
    void Reset() override { m_bFired = false; }

    uint32_t m_nEmitCount{100};
    float    m_flTriggerTime{0.0f};
    bool     m_bFired{false};
};
using EmitInstantaneous = CEmitInstantaneous;

/**
 * @brief CEmitContinuousRate: Accumulates continuous fractional particle emission.
 */
class CEmitContinuousRate : public IParticleEmitter {
public:
    explicit CEmitContinuousRate(float flRateParticlesPerSec)
        : m_flRate(flRateParticlesPerSec) {}

    uint32_t Emit(CParticleCollection* pParticles, float flDeltaTime) override;
    void Reset() override { m_flEmissionRemainder = 0.0f; }

    float m_flRate{50.0f};
    float m_flEmissionRemainder{0.0f};
};
using EmitContinuousRate = CEmitContinuousRate;

// ----------------------------------------------------------------------------
// Concrete Initializer Implementations
// ----------------------------------------------------------------------------

/**
 * @brief CInitRadius: Sets initial particle radius within bounds [flMin, flMax].
 */
class CInitRadius : public IParticleInitializer {
public:
    CInitRadius(float flMin, float flMax) : m_flMin(flMin), m_flMax(flMax) {}
    void Initialize(CParticleCollection* pParticles, uint32_t nStartIdx, uint32_t nCount) override;

    float m_flMin{5.0f};
    float m_flMax{10.0f};
};
using InitRadius = CInitRadius;

/**
 * @brief CInitVelocityRandom: Assigns random initial directional velocity within 3D bounding box.
 */
class CInitVelocityRandom : public IParticleInitializer {
public:
    CInitVelocityRandom(const Vector3D& vMin, const Vector3D& vMax)
        : m_vMin(vMin), m_vMax(vMax) {}
    void Initialize(CParticleCollection* pParticles, uint32_t nStartIdx, uint32_t nCount) override;

    Vector3D m_vMin{-50.0f, -50.0f, 50.0f};
    Vector3D m_vMax{ 50.0f,  50.0f, 200.0f};
};
using InitVelocityRandom = CInitVelocityRandom;

/**
 * @brief CInitColorRandom: Sets base particle color and randomized tints.
 */
class CInitColorRandom : public IParticleInitializer {
public:
    CInitColorRandom(const ColorRGBA& minCol, const ColorRGBA& maxCol)
        : m_minColor(minCol), m_maxColor(maxCol) {}
    void Initialize(CParticleCollection* pParticles, uint32_t nStartIdx, uint32_t nCount) override;

    ColorRGBA m_minColor{0.8f, 0.8f, 0.8f, 1.0f};
    ColorRGBA m_maxColor{1.0f, 1.0f, 1.0f, 1.0f};
};
using InitColorRandom = CInitColorRandom;

/**
 * @brief CInitLifeTime: Sets random lifetime duration in seconds [flMin, flMax].
 */
class CInitLifeTime : public IParticleInitializer {
public:
    CInitLifeTime(float flMin, float flMax) : m_flMin(flMin), m_flMax(flMax) {}
    void Initialize(CParticleCollection* pParticles, uint32_t nStartIdx, uint32_t nCount) override;

    float m_flMin{1.0f};
    float m_flMax{2.5f};
};
using InitLifeTime = CInitLifeTime;

// ----------------------------------------------------------------------------
// Concrete Simulation Operator Implementations
// ----------------------------------------------------------------------------

/**
 * @brief COpMovementBasic: Symplectic Euler / Verlet integration with gravity and drag.
 * Position integration: p <- p + v * dt
 * Aerodynamic drag:     v <- v * (1 - kDrag * dt)
 * Gravity acceleration: v <- v + g * dt
 */
class COpMovementBasic : public IParticleOperator {
public:
    COpMovementBasic(const Vector3D& vGravity = {0.0f, 0.0f, kStandardGravityCS2}, float flDrag = 0.05f)
        : m_vGravity(vGravity), m_flDrag(flDrag) {}

    void Operate(CParticleCollection* pParticles, float flDeltaTime) override;

    Vector3D m_vGravity{0.0f, 0.0f, kStandardGravityCS2};
    float    m_flDrag{0.05f};
};
using OpMovementBasic = COpMovementBasic;

/**
 * @brief COpTurbulenceNoise: Divergence-free 3D curl / Perlin noise velocity perturbation.
 */
class COpTurbulenceNoise : public IParticleOperator {
public:
    COpTurbulenceNoise(float flNoiseScale = 0.02f, float flAmplitude = 120.0f)
        : m_flNoiseScale(flNoiseScale), m_flAmplitude(flAmplitude) {}

    void Operate(CParticleCollection* pParticles, float flDeltaTime) override;

    static Vector3D EvaluateCurlNoise(const Vector3D& pos, float time, float scale) noexcept;
    static float EvaluatePerlin3D(float x, float y, float z) noexcept;

    float m_flNoiseScale{0.02f};
    float m_flAmplitude{120.0f};
};
using OpTurbulenceNoise = COpTurbulenceNoise;

/**
 * @brief COpFadeAndKill: Computes normalized age, applies fade curves, and compacts dead particles.
 */
class COpFadeAndKill : public IParticleOperator {
public:
    COpFadeAndKill(float flFadeInFraction = 0.1f, float flFadeOutFraction = 0.8f, float flBaseAlpha = 1.0f)
        : m_flFadeInFraction(flFadeInFraction), m_flFadeOutFraction(flFadeOutFraction), m_flBaseAlpha(flBaseAlpha) {}

    void Operate(CParticleCollection* pParticles, float flDeltaTime) override;

    float m_flFadeInFraction{0.1f};
    float m_flFadeOutFraction{0.8f};
    float m_flBaseAlpha{1.0f};
};
using OpFadeAndKill = COpFadeAndKill;

/**
 * @brief COpWorldCollisionPlane: Restitution & friction boundary constraint solver.
 * Normal plane equation: n . x - D = 0.
 */
class COpWorldCollisionPlane : public IParticleOperator {
public:
    COpWorldCollisionPlane(const Vector3D& vNormal = {0.0f, 0.0f, 1.0f},
                           float flPlaneD = 0.0f,
                           float flRestitution = 0.4f,
                           float flFriction = 0.2f)
        : m_vNormal(vNormal.Normalized()), m_flPlaneD(flPlaneD),
          m_flRestitution(flRestitution), m_flFriction(flFriction) {}

    void Operate(CParticleCollection* pParticles, float flDeltaTime) override;

    Vector3D m_vNormal{0.0f, 0.0f, 1.0f};
    float    m_flPlaneD{0.0f};
    float    m_flRestitution{0.4f};
    float    m_flFriction{0.2f};
};
using OpWorldCollisionPlane = COpWorldCollisionPlane;

// ----------------------------------------------------------------------------
// Concrete Renderer Implementations
// ----------------------------------------------------------------------------

/**
 * @brief CParticleRendererSprite: Emits camera-facing billboard quad vertex buffers.
 */
class CParticleRendererSprite : public IParticleRenderer {
public:
    void Render(const CParticleCollection* pParticles,
                const Vector3D& vCamRight,
                const Vector3D& vCamUp,
                std::vector<ParticleQuadVertex>& outVertices,
                std::vector<uint32_t>& outIndices) const override;
};

// ----------------------------------------------------------------------------
// CParticleSystemDefinition: Master Particle System Descriptor
// ----------------------------------------------------------------------------
class CParticleSystemDefinition {
public:
    explicit CParticleSystemDefinition(std::string strName, uint32_t maxParticles = kDefaultMaxParticles)
        : m_strName(std::move(strName)), m_nMaxParticles(maxParticles) {}

    void AddEmitter(std::unique_ptr<IParticleEmitter> emitter) {
        m_Emitters.push_back(std::move(emitter));
    }
    void AddInitializer(std::unique_ptr<IParticleInitializer> initializer) {
        m_Initializers.push_back(std::move(initializer));
    }
    void AddOperator(std::unique_ptr<IParticleOperator> op) {
        m_Operators.push_back(std::move(op));
    }
    void SetRenderer(std::unique_ptr<IParticleRenderer> renderer) {
        m_pRenderer = std::move(renderer);
    }

    /**
     * @brief Executes a single simulation step: Emitters -> Initializers -> Operators.
     * @param pParticles Target particle collection.
     * @param flDeltaTime Step elapsed time in seconds.
     */
    void Step(CParticleCollection* pParticles, float flDeltaTime);

    /**
     * @brief Generates dynamic billboard geometry.
     */
    void Render(const CParticleCollection* pParticles,
                const Vector3D& vCamRight,
                const Vector3D& vCamUp,
                std::vector<ParticleQuadVertex>& outVertices,
                std::vector<uint32_t>& outIndices) const {
        if (m_pRenderer) {
            m_pRenderer->Render(pParticles, vCamRight, vCamUp, outVertices, outIndices);
        }
    }

    [[nodiscard]] const std::string& GetName() const noexcept { return m_strName; }
    [[nodiscard]] uint32_t GetMaxParticles() const noexcept { return m_nMaxParticles; }

private:
    std::string m_strName;
    uint32_t    m_nMaxParticles{kDefaultMaxParticles};

    std::vector<std::unique_ptr<IParticleEmitter>>     m_Emitters;
    std::vector<std::unique_ptr<IParticleInitializer>> m_Initializers;
    std::vector<std::unique_ptr<IParticleOperator>>    m_Operators;
    std::unique_ptr<IParticleRenderer>                 m_pRenderer;
};

} // namespace cs2::particles

#endif // CS2_PARTICLES_PARTICLE_SYSTEM_H
