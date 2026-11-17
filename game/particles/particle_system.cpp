// ============================================================================
// Counter-Strike 2 (Source 2) Data-Driven Particle Architecture
// Reconstructed from particles.dll (Build 26598758)
//
// Standalone C++20 Implementation. Zero external dependencies.
// ============================================================================

#include "particle_system.h"

#include <cstdlib>
#include <new>
#include <limits>

namespace cs2::particles {

// ----------------------------------------------------------------------------
// Deterministic 3D Improved Perlin Noise Permutation Table (Ken Perlin 2002)
// ----------------------------------------------------------------------------
namespace {

constexpr uint8_t kPermutationTable[512] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
    134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
    55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
    250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
    189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
    172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
    228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
    107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
    // Repeat to avoid wrapping
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
    134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
    55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
    250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
    189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
    172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
    228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
    107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

inline float Fade(float t) noexcept {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float Lerp(float t, float a, float b) noexcept {
    return a + t * (b - a);
}

inline float Grad(int hash, float x, float y, float z) noexcept {
    const int h = hash & 15;
    const float u = (h < 8) ? x : y;
    const float v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// CParticleCollection Implementation
// ----------------------------------------------------------------------------
CParticleCollection::CParticleCollection(uint32_t maxParticles) {
    AllocateStorage(maxParticles);
}

CParticleCollection::~CParticleCollection() {
    FreeStorage();
}

CParticleCollection::CParticleCollection(CParticleCollection&& other) noexcept {
    *this = std::move(other);
}

CParticleCollection& CParticleCollection::operator=(CParticleCollection&& other) noexcept {
    if (this != &other) {
        FreeStorage();

        m_pPosX = other.m_pPosX;
        m_pPosY = other.m_pPosY;
        m_pPosZ = other.m_pPosZ;
        m_pPrevPosX = other.m_pPrevPosX;
        m_pPrevPosY = other.m_pPrevPosY;
        m_pPrevPosZ = other.m_pPrevPosZ;
        m_pVelX = other.m_pVelX;
        m_pVelY = other.m_pVelY;
        m_pVelZ = other.m_pVelZ;
        m_pColorR = other.m_pColorR;
        m_pColorG = other.m_pColorG;
        m_pColorB = other.m_pColorB;
        m_pAlpha = other.m_pAlpha;
        m_pRadius = other.m_pRadius;
        m_pLifeDuration = other.m_pLifeDuration;
        m_pCreationTime = other.m_pCreationTime;

        m_nActiveParticles = other.m_nActiveParticles;
        m_nMaxParticles = other.m_nMaxParticles;
        m_flCurTime = other.m_flCurTime;
        m_pRawBackingMemory = other.m_pRawBackingMemory;

        other.m_pRawBackingMemory = nullptr;
        other.m_nActiveParticles = 0;
        other.m_nMaxParticles = 0;
    }
    return *this;
}

void CParticleCollection::AllocateStorage(uint32_t maxParticles) {
    FreeStorage();

    m_nMaxParticles = std::max(1u, maxParticles);
    m_nActiveParticles = 0;
    m_flCurTime = 0.0f;

    // Align each attribute channel to 64 bytes
    const size_t rawChannelBytes = m_nMaxParticles * sizeof(float);
    const size_t alignedChannelBytes = ((rawChannelBytes + (kSimdAlignment - 1)) / kSimdAlignment) * kSimdAlignment;
    const size_t totalAllocationBytes = alignedChannelBytes * kTotalParticleChannels;

#if defined(_MSC_VER)
    m_pRawBackingMemory = static_cast<uint8_t*>(_aligned_malloc(totalAllocationBytes, kSimdAlignment));
#else
    m_pRawBackingMemory = static_cast<uint8_t*>(std::aligned_alloc(kSimdAlignment, totalAllocationBytes));
#endif

    if (!m_pRawBackingMemory) {
        return;
    }

    std::memset(m_pRawBackingMemory, 0, totalAllocationBytes);

    uint8_t* pPtr = m_pRawBackingMemory;
    m_pPosX          = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pPosY          = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pPosZ          = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pPrevPosX      = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pPrevPosY      = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pPrevPosZ      = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pVelX          = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pVelY          = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pVelZ          = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pColorR        = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pColorG        = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pColorB        = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pAlpha         = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pRadius        = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pLifeDuration  = reinterpret_cast<float*>(pPtr); pPtr += alignedChannelBytes;
    m_pCreationTime  = reinterpret_cast<float*>(pPtr);
}

void CParticleCollection::FreeStorage() noexcept {
    if (m_pRawBackingMemory) {
#if defined(_MSC_VER)
        _aligned_free(m_pRawBackingMemory);
#else
        std::free(m_pRawBackingMemory);
#endif
        m_pRawBackingMemory = nullptr;
    }
}

uint32_t CParticleCollection::AllocateParticles(uint32_t nCount) noexcept {
    if (nCount == 0 || m_nActiveParticles >= m_nMaxParticles) {
        return 0xFFFFFFFF;
    }

    const uint32_t available = m_nMaxParticles - m_nActiveParticles;
    const uint32_t actualCount = std::min(nCount, available);

    const uint32_t startIdx = m_nActiveParticles;
    m_nActiveParticles += actualCount;

    for (uint32_t i = startIdx; i < m_nActiveParticles; ++i) {
        m_pCreationTime[i] = m_flCurTime;
    }

    return startIdx;
}

void CParticleCollection::KillParticle(uint32_t nIndex) noexcept {
    if (nIndex >= m_nActiveParticles || m_nActiveParticles == 0) {
        return;
    }

    const uint32_t last = m_nActiveParticles - 1;
    if (nIndex != last) {
        // Swap-with-last O(1) compaction across all SoA streams
        m_pPosX[nIndex]         = m_pPosX[last];
        m_pPosY[nIndex]         = m_pPosY[last];
        m_pPosZ[nIndex]         = m_pPosZ[last];
        m_pPrevPosX[nIndex]     = m_pPrevPosX[last];
        m_pPrevPosY[nIndex]     = m_pPrevPosY[last];
        m_pPrevPosZ[nIndex]     = m_pPrevPosZ[last];
        m_pVelX[nIndex]         = m_pVelX[last];
        m_pVelY[nIndex]         = m_pVelY[last];
        m_pVelZ[nIndex]         = m_pVelZ[last];
        m_pColorR[nIndex]       = m_pColorR[last];
        m_pColorG[nIndex]       = m_pColorG[last];
        m_pColorB[nIndex]       = m_pColorB[last];
        m_pAlpha[nIndex]        = m_pAlpha[last];
        m_pRadius[nIndex]       = m_pRadius[last];
        m_pLifeDuration[nIndex] = m_pLifeDuration[last];
        m_pCreationTime[nIndex] = m_pCreationTime[last];
    }

    --m_nActiveParticles;
}

void CParticleCollection::Reset() noexcept {
    m_nActiveParticles = 0;
    m_flCurTime = 0.0f;
}

// ----------------------------------------------------------------------------
// Emitter Implementations
// ----------------------------------------------------------------------------
uint32_t CEmitInstantaneous::Emit(CParticleCollection* pParticles, float flDeltaTime) {
    (void)flDeltaTime;
    if (!m_bFired && pParticles->GetCurrentTime() >= m_flTriggerTime) {
        m_bFired = true;
        const uint32_t startIdx = pParticles->AllocateParticles(m_nEmitCount);
        return (startIdx != 0xFFFFFFFF) ? m_nEmitCount : 0;
    }
    return 0;
}

uint32_t CEmitContinuousRate::Emit(CParticleCollection* pParticles, float flDeltaTime) {
    if (m_flRate <= 0.0f) {
        return 0;
    }

    m_flEmissionRemainder += m_flRate * flDeltaTime;
    const auto count = static_cast<uint32_t>(m_flEmissionRemainder);

    if (count > 0) {
        m_flEmissionRemainder -= static_cast<float>(count);
        const uint32_t startIdx = pParticles->AllocateParticles(count);
        return (startIdx != 0xFFFFFFFF) ? count : 0;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Initializer Implementations
// ----------------------------------------------------------------------------
namespace {
    static thread_local std::mt19937 s_Prng(1337);
}

void CInitRadius::Initialize(CParticleCollection* pParticles, uint32_t nStartIdx, uint32_t nCount) {
    std::uniform_real_distribution<float> dist(m_flMin, m_flMax);
    for (uint32_t i = nStartIdx; i < nStartIdx + nCount; ++i) {
        pParticles->m_pRadius[i] = dist(s_Prng);
    }
}

void CInitVelocityRandom::Initialize(CParticleCollection* pParticles, uint32_t nStartIdx, uint32_t nCount) {
    std::uniform_real_distribution<float> distX(m_vMin.x, m_vMax.x);
    std::uniform_real_distribution<float> distY(m_vMin.y, m_vMax.y);
    std::uniform_real_distribution<float> distZ(m_vMin.z, m_vMax.z);

    for (uint32_t i = nStartIdx; i < nStartIdx + nCount; ++i) {
        pParticles->m_pVelX[i] = distX(s_Prng);
        pParticles->m_pVelY[i] = distY(s_Prng);
        pParticles->m_pVelZ[i] = distZ(s_Prng);
    }
}

void CInitColorRandom::Initialize(CParticleCollection* pParticles, uint32_t nStartIdx, uint32_t nCount) {
    std::uniform_real_distribution<float> distR(m_minColor.r, m_maxColor.r);
    std::uniform_real_distribution<float> distG(m_minColor.g, m_maxColor.g);
    std::uniform_real_distribution<float> distB(m_minColor.b, m_maxColor.b);
    std::uniform_real_distribution<float> distA(m_minColor.a, m_maxColor.a);

    for (uint32_t i = nStartIdx; i < nStartIdx + nCount; ++i) {
        pParticles->m_pColorR[i] = distR(s_Prng);
        pParticles->m_pColorG[i] = distG(s_Prng);
        pParticles->m_pColorB[i] = distB(s_Prng);
        pParticles->m_pAlpha[i]  = distA(s_Prng);
    }
}

void CInitLifeTime::Initialize(CParticleCollection* pParticles, uint32_t nStartIdx, uint32_t nCount) {
    std::uniform_real_distribution<float> dist(m_flMin, m_flMax);
    for (uint32_t i = nStartIdx; i < nStartIdx + nCount; ++i) {
        pParticles->m_pLifeDuration[i] = dist(s_Prng);
    }
}

// ----------------------------------------------------------------------------
// Operator Implementations
// ----------------------------------------------------------------------------
void COpMovementBasic::Operate(CParticleCollection* pParticles, float flDeltaTime) {
    const uint32_t count = pParticles->GetActiveParticleCount();
    if (count == 0 || flDeltaTime <= 0.0f) {
        return;
    }

    const float kDrag = std::max(0.0f, 1.0f - m_flDrag * flDeltaTime);
    const float gx = m_vGravity.x * flDeltaTime;
    const float gy = m_vGravity.y * flDeltaTime;
    const float gz = m_vGravity.z * flDeltaTime;

    float* const px = pParticles->m_pPosX;
    float* const py = pParticles->m_pPosY;
    float* const pz = pParticles->m_pPosZ;
    float* const prevX = pParticles->m_pPrevPosX;
    float* const prevY = pParticles->m_pPrevPosY;
    float* const prevZ = pParticles->m_pPrevPosZ;
    float* const vx = pParticles->m_pVelX;
    float* const vy = pParticles->m_pVelY;
    float* const vz = pParticles->m_pVelZ;

    for (uint32_t i = 0; i < count; ++i) {
        // Record prior integration state
        prevX[i] = px[i];
        prevY[i] = py[i];
        prevZ[i] = pz[i];

        // Accelerate with gravity and decelerate with aerodynamic friction
        vx[i] = (vx[i] + gx) * kDrag;
        vy[i] = (vy[i] + gy) * kDrag;
        vz[i] = (vz[i] + gz) * kDrag;

        // Position displacement
        px[i] += vx[i] * flDeltaTime;
        py[i] += vy[i] * flDeltaTime;
        pz[i] += vz[i] * flDeltaTime;
    }
}

float COpTurbulenceNoise::EvaluatePerlin3D(float x, float y, float z) noexcept {
    const int X = static_cast<int>(std::floor(x)) & 255;
    const int Y = static_cast<int>(std::floor(y)) & 255;
    const int Z = static_cast<int>(std::floor(z)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    const float u = Fade(x);
    const float v = Fade(y);
    const float w = Fade(z);

    const int A  = kPermutationTable[X] + Y;
    const int AA = kPermutationTable[A] + Z;
    const int AB = kPermutationTable[A + 1] + Z;
    const int B  = kPermutationTable[X + 1] + Y;
    const int BA = kPermutationTable[B] + Z;
    const int BB = kPermutationTable[B + 1] + Z;

    return Lerp(w,
        Lerp(v,
            Lerp(u, Grad(kPermutationTable[AA],     x,     y,     z),
                    Grad(kPermutationTable[BA],     x - 1, y,     z)),
            Lerp(u, Grad(kPermutationTable[AB],     x,     y - 1, z),
                    Grad(kPermutationTable[BB],     x - 1, y - 1, z))),
        Lerp(v,
            Lerp(u, Grad(kPermutationTable[AA + 1], x,     y,     z - 1),
                    Grad(kPermutationTable[BA + 1], x - 1, y,     z - 1)),
            Lerp(u, Grad(kPermutationTable[AB + 1], x,     y - 1, z - 1),
                    Grad(kPermutationTable[BB + 1], x - 1, y - 1, z - 1))));
}

Vector3D COpTurbulenceNoise::EvaluateCurlNoise(const Vector3D& pos, float time, float scale) noexcept {
    const float eps = 0.05f;
    const float invEps2 = 1.0f / (2.0f * eps);

    const float sx = pos.x * scale + time * 0.1f;
    const float sy = pos.y * scale + time * 0.1f;
    const float sz = pos.z * scale + time * 0.1f;

    // Vector potential fields Psi_x, Psi_y, Psi_z evaluated using decorrelated 3D noise offsets
    // v_curl = del x Psi = (dPsi_z/dy - dPsi_y/dz, dPsi_x/dz - dPsi_z/dx, dPsi_y/dx - dPsi_x/dy)
    const float dPz_dy = (EvaluatePerlin3D(sx + 31.41f, sy + eps, sz) - EvaluatePerlin3D(sx + 31.41f, sy - eps, sz)) * invEps2;
    const float dPy_dz = (EvaluatePerlin3D(sx + 17.13f, sy, sz + eps) - EvaluatePerlin3D(sx + 17.13f, sy, sz - eps)) * invEps2;

    const float dPx_dz = (EvaluatePerlin3D(sx, sy, sz + eps) - EvaluatePerlin3D(sx, sy, sz - eps)) * invEps2;
    const float dPz_dx = (EvaluatePerlin3D(sx + 31.41f + eps, sy, sz) - EvaluatePerlin3D(sx + 31.41f - eps, sy, sz)) * invEps2;

    const float dPy_dx = (EvaluatePerlin3D(sx + 17.13f + eps, sy, sz) - EvaluatePerlin3D(sx + 17.13f - eps, sy, sz)) * invEps2;
    const float dPx_dy = (EvaluatePerlin3D(sx, sy + eps, sz) - EvaluatePerlin3D(sx, sy - eps, sz)) * invEps2;

    return Vector3D{ dPz_dy - dPy_dz, dPx_dz - dPz_dx, dPy_dx - dPx_dy };
}

void COpTurbulenceNoise::Operate(CParticleCollection* pParticles, float flDeltaTime) {
    const uint32_t count = pParticles->GetActiveParticleCount();
    if (count == 0 || flDeltaTime <= 0.0f || m_flAmplitude <= 0.0f) {
        return;
    }

    const float curTime = pParticles->GetCurrentTime();
    const float ampDt = m_flAmplitude * flDeltaTime;

    float* const px = pParticles->m_pPosX;
    float* const py = pParticles->m_pPosY;
    float* const pz = pParticles->m_pPosZ;
    float* const vx = pParticles->m_pVelX;
    float* const vy = pParticles->m_pVelY;
    float* const vz = pParticles->m_pVelZ;

    for (uint32_t i = 0; i < count; ++i) {
        const Vector3D curl = EvaluateCurlNoise(Vector3D{ px[i], py[i], pz[i] }, curTime, m_flNoiseScale);
        vx[i] += curl.x * ampDt;
        vy[i] += curl.y * ampDt;
        vz[i] += curl.z * ampDt;
    }
}

void COpFadeAndKill::Operate(CParticleCollection* pParticles, float flDeltaTime) {
    (void)flDeltaTime;
    uint32_t i = 0;
    while (i < pParticles->GetActiveParticleCount()) {
        const float u = pParticles->GetNormalizedAge(i);
        if (u >= 1.0f) {
            // Expiration: Kill and compact SoA array in O(1)
            pParticles->KillParticle(i);
            // Notice: do NOT increment i; the last particle was moved to slot i
        } else {
            // Smooth piecewise fade envelope
            float alpha = 1.0f;
            if (m_flFadeInFraction > 1e-5f && u < m_flFadeInFraction) {
                alpha = u / m_flFadeInFraction;
            } else if (m_flFadeOutFraction < 1.0f && u > m_flFadeOutFraction) {
                alpha = (1.0f - u) / (1.0f - m_flFadeOutFraction);
            }

            pParticles->m_pAlpha[i] = std::clamp(alpha, 0.0f, 1.0f) * m_flBaseAlpha;
            ++i;
        }
    }
}

void COpWorldCollisionPlane::Operate(CParticleCollection* pParticles, float flDeltaTime) {
    (void)flDeltaTime;
    const uint32_t count = pParticles->GetActiveParticleCount();
    if (count == 0) {
        return;
    }

    const float nx = m_vNormal.x;
    const float ny = m_vNormal.y;
    const float nz = m_vNormal.z;
    const float planeD = m_flPlaneD;
    const float rest = m_flRestitution;
    const float frictionScalar = std::clamp(1.0f - m_flFriction, 0.0f, 1.0f);

    float* const px = pParticles->m_pPosX;
    float* const py = pParticles->m_pPosY;
    float* const pz = pParticles->m_pPosZ;
    float* const vx = pParticles->m_pVelX;
    float* const vy = pParticles->m_pVelY;
    float* const vz = pParticles->m_pVelZ;
    const float* const rad = pParticles->m_pRadius;

    for (uint32_t i = 0; i < count; ++i) {
        const float dist = (px[i] * nx + py[i] * ny + pz[i] * nz) - planeD;
        const float r = rad[i];

        if (dist < r) {
            // Resolve positional overlap / boundary penetration
            const float penetration = r - dist;
            px[i] += nx * penetration;
            py[i] += ny * penetration;
            pz[i] += nz * penetration;

            // Decompose velocity into normal and tangential components
            const float vNormal = vx[i] * nx + vy[i] * ny + vz[i] * nz;
            if (vNormal < 0.0f) { // Moving towards plane
                const float vnx = vNormal * nx;
                const float vny = vNormal * ny;
                const float vnz = vNormal * nz;

                const float vtx = vx[i] - vnx;
                const float vty = vy[i] - vny;
                const float vtz = vz[i] - vnz;

                // Restitution along normal, friction attenuation along tangent
                vx[i] = -vnx * rest + vtx * frictionScalar;
                vy[i] = -vny * rest + vty * frictionScalar;
                vz[i] = -vnz * rest + vtz * frictionScalar;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Renderer Implementation
// ----------------------------------------------------------------------------
void CParticleRendererSprite::Render(const CParticleCollection* pParticles,
                                     const Vector3D& vCamRight,
                                     const Vector3D& vCamUp,
                                     std::vector<ParticleQuadVertex>& outVertices,
                                     std::vector<uint32_t>& outIndices) const {
    const uint32_t count = pParticles->GetActiveParticleCount();
    if (count == 0) {
        return;
    }

    outVertices.reserve(outVertices.size() + count * 4);
    outIndices.reserve(outIndices.size() + count * 6);

    const float* const px = pParticles->m_pPosX;
    const float* const py = pParticles->m_pPosY;
    const float* const pz = pParticles->m_pPosZ;
    const float* const cr = pParticles->m_pColorR;
    const float* const cg = pParticles->m_pColorG;
    const float* const cb = pParticles->m_pColorB;
    const float* const ca = pParticles->m_pAlpha;
    const float* const rad = pParticles->m_pRadius;

    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t baseVertex = static_cast<uint32_t>(outVertices.size());
        const float r = rad[i];
        const Vector3D center{ px[i], py[i], pz[i] };
        const ColorRGBA color{ cr[i], cg[i], cb[i], ca[i] };

        const Vector3D right = vCamRight * r;
        const Vector3D up    = vCamUp * r;

        // Quad 4 vertices (camera-aligned billboard)
        // 0: Bottom-Left
        outVertices.push_back({ center - right - up, Vector2D{0.0f, 0.0f}, color });
        // 1: Bottom-Right
        outVertices.push_back({ center + right - up, Vector2D{1.0f, 0.0f}, color });
        // 2: Top-Right
        outVertices.push_back({ center + right + up, Vector2D{1.0f, 1.0f}, color });
        // 3: Top-Left
        outVertices.push_back({ center - right + up, Vector2D{0.0f, 1.0f}, color });

        // 2 triangles per quad
        outIndices.push_back(baseVertex + 0);
        outIndices.push_back(baseVertex + 1);
        outIndices.push_back(baseVertex + 2);
        outIndices.push_back(baseVertex + 0);
        outIndices.push_back(baseVertex + 2);
        outIndices.push_back(baseVertex + 3);
    }
}

// ----------------------------------------------------------------------------
// Master Particle System Definition Execution Step
// ----------------------------------------------------------------------------
void CParticleSystemDefinition::Step(CParticleCollection* pParticles, float flDeltaTime) {
    if (!pParticles || flDeltaTime <= 0.0f) {
        return;
    }

    pParticles->AdvanceTime(flDeltaTime);

    // 1. Evaluate Emitters and initialize newly spawned particles
    for (const auto& emitter : m_Emitters) {
        const uint32_t prevCount = pParticles->GetActiveParticleCount();
        const uint32_t spawned = emitter->Emit(pParticles, flDeltaTime);
        if (spawned > 0) {
            for (const auto& initializer : m_Initializers) {
                initializer->Initialize(pParticles, prevCount, spawned);
            }
        }
    }

    // 2. Evaluate Operators in topological graph order
    for (const auto& op : m_Operators) {
        op->Operate(pParticles, flDeltaTime);
    }
}

} // namespace cs2::particles
