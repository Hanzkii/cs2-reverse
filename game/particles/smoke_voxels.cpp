#include "smoke_voxels.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace cs2::particles {

// ----------------------------------------------------------------------------
// 3D Morton Order (Z-Order Curve) Bit Interleaving
// Extracted directly from server.dll FUN_1809c8e50:
// Interleaves 5 bits of X, Y, Z into a 15-bit Morton index (0..32767).
// ----------------------------------------------------------------------------
uint32_t CSmokeVoxelGrid::Part1By2(uint32_t x) noexcept {
    x &= 0x0000001f;                  // 5 bits: x_4 x_3 x_2 x_1 x_0
    x = (x | (x << 8)) & 0x0300f00f;  // Interleave staging
    x = (x ^ (x << 4)) & 0x030c30c3;
    x = (x ^ (x << 2)) & 0x09249249;
    return x;
}

uint32_t CSmokeVoxelGrid::Morton3D(uint32_t x, uint32_t y, uint32_t z) noexcept {
    return Part1By2(x) | (Part1By2(y) << 1) | (Part1By2(z) << 2);
}

void CSmokeVoxelGrid::MortonDecode3D(uint32_t code, uint32_t& x, uint32_t& y, uint32_t& z) noexcept {
    x = y = z = 0;
    for (int i = 0; i < 5; ++i) {
        x |= ((code >> (3 * i + 0)) & 1) << i;
        y |= ((code >> (3 * i + 1)) & 1) << i;
        z |= ((code >> (3 * i + 2)) & 1) << i;
    }
}

// ----------------------------------------------------------------------------
// Constructor & Lifecycle
// ----------------------------------------------------------------------------
CSmokeVoxelGrid::CSmokeVoxelGrid(const Vector3& origin, float voxelSize)
    : m_vSmokeDetonationPos(origin), m_VoxelSize(voxelSize)
{
    std::memset(m_OccupancyMask, 0, sizeof(m_OccupancyMask));
    std::memset(m_VoxelDensities, 0, sizeof(m_VoxelDensities));
}

void CSmokeVoxelGrid::Detonate(const Vector3& origin, int32_t tick) {
    m_vSmokeDetonationPos = origin;
    m_nStartTick = tick;
    m_nCurrentTick = tick;
    m_bActive = true;
    m_nActiveBuffer = 0;
    m_nVoxelUpdateCounter = 0;
    m_nShockwaveCooldownTicks = 0;

    std::memset(m_OccupancyMask, 0, sizeof(m_OccupancyMask));
    std::memset(m_VoxelDensities, 0, sizeof(m_VoxelDensities));

    // Initial spherical expansion seed (center voxel 16, 16, 16)
    ExpandInitialVolume(80.0f);
}

void CSmokeVoxelGrid::OnTick(int32_t currentTick) {
    m_nCurrentTick = currentTick;
    if (m_nShockwaveCooldownTicks > 0) {
        m_nShockwaveCooldownTicks--;
    }
}

// ----------------------------------------------------------------------------
// Coordinate Transformations
// ----------------------------------------------------------------------------
bool CSmokeVoxelGrid::WorldToVoxel(const Vector3& worldPos, int& vx, int& vy, int& vz) const noexcept {
    vx = static_cast<int>(std::floor((worldPos.x - m_vSmokeDetonationPos.x) * VOXEL_INV_SIZE + GRID_CENTER_VOXEL));
    vy = static_cast<int>(std::floor((worldPos.y - m_vSmokeDetonationPos.y) * VOXEL_INV_SIZE + GRID_CENTER_VOXEL));
    vz = static_cast<int>(std::floor((worldPos.z - m_vSmokeDetonationPos.z) * VOXEL_INV_SIZE + GRID_CENTER_VOXEL));

    return (vx >= 0 && vx < GRID_DIM &&
            vy >= 0 && vy < GRID_DIM &&
            vz >= 0 && vz < GRID_DIM);
}

Vector3 CSmokeVoxelGrid::VoxelToWorld(int vx, int vy, int vz) const noexcept {
    return {
        (static_cast<float>(vx) - GRID_CENTER_VOXEL + 0.5f) * m_VoxelSize + m_vSmokeDetonationPos.x,
        (static_cast<float>(vy) - GRID_CENTER_VOXEL + 0.5f) * m_VoxelSize + m_vSmokeDetonationPos.y,
        (static_cast<float>(vz) - GRID_CENTER_VOXEL + 0.5f) * m_VoxelSize + m_vSmokeDetonationPos.z
    };
}

// ----------------------------------------------------------------------------
// Voxel State & Bitmask Accessors
// ----------------------------------------------------------------------------
bool CSmokeVoxelGrid::IsVoxelActive(int vx, int vy, int vz) const noexcept {
    if (vx < 0 || vx >= GRID_DIM || vy < 0 || vy >= GRID_DIM || vz < 0 || vz >= GRID_DIM) {
        return false;
    }
    const uint32_t morton = Morton3D(static_cast<uint32_t>(vx), static_cast<uint32_t>(vy), static_cast<uint32_t>(vz));
    const uint32_t wordIdx = morton >> 6;
    const uint32_t bitIdx = morton & 0x3f;
    return (m_OccupancyMask[m_nActiveBuffer][wordIdx] & (1ULL << bitIdx)) != 0;
}

void CSmokeVoxelGrid::SetVoxelActive(int vx, int vy, int vz, float density) {
    if (vx < 0 || vx >= GRID_DIM || vy < 0 || vy >= GRID_DIM || vz < 0 || vz >= GRID_DIM) {
        return;
    }
    const uint32_t morton = Morton3D(static_cast<uint32_t>(vx), static_cast<uint32_t>(vy), static_cast<uint32_t>(vz));
    const uint32_t wordIdx = morton >> 6;
    const uint32_t bitIdx = morton & 0x3f;

    m_OccupancyMask[m_nActiveBuffer][wordIdx] |= (1ULL << bitIdx);
    m_VoxelDensities[morton] = std::clamp(density, 0.0f, 1.0f);
}

void CSmokeVoxelGrid::ClearVoxel(int vx, int vy, int vz) {
    if (vx < 0 || vx >= GRID_DIM || vy < 0 || vy >= GRID_DIM || vz < 0 || vz >= GRID_DIM) {
        return;
    }
    const uint32_t morton = Morton3D(static_cast<uint32_t>(vx), static_cast<uint32_t>(vy), static_cast<uint32_t>(vz));
    const uint32_t wordIdx = morton >> 6;
    const uint32_t bitIdx = morton & 0x3f;

    m_OccupancyMask[m_nActiveBuffer][wordIdx] &= ~(1ULL << bitIdx);
    m_VoxelDensities[morton] = 0.0f;
}

uint8_t CSmokeVoxelGrid::GetVoxel(int x, int y, int z) const {
    if (!IsVoxelActive(x, y, z)) return 0;
    const uint32_t morton = Morton3D(static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(z));
    return static_cast<uint8_t>(std::clamp(m_VoxelDensities[morton] * 255.0f, 0.0f, 255.0f));
}

void CSmokeVoxelGrid::SetVoxel(int x, int y, int z, uint8_t val) {
    if (val == 0) {
        ClearVoxel(x, y, z);
    } else {
        SetVoxelActive(x, y, z, static_cast<float>(val) / 255.0f);
    }
}

// ----------------------------------------------------------------------------
// Volume Expansion
// ----------------------------------------------------------------------------
void CSmokeVoxelGrid::ExpandInitialVolume(float radiusUnits) {
    const float rSqr = radiusUnits * radiusUnits;
    for (int z = 0; z < GRID_DIM; ++z) {
        for (int y = 0; y < GRID_DIM; ++y) {
            for (int x = 0; x < GRID_DIM; ++x) {
                const Vector3 world = VoxelToWorld(x, y, z);
                const float dSqr = (world - m_vSmokeDetonationPos).LengthSqr();
                if (dSqr <= rSqr) {
                    const float falloff = 1.0f - std::sqrt(dSqr) / radiusUnits;
                    SetVoxelActive(x, y, z, 0.5f + falloff * 0.5f);
                }
            }
        }
    }
}

void CSmokeVoxelGrid::InitializeSphere(float radius, uint8_t density) {
    m_bActive = true;
    std::memset(m_OccupancyMask, 0, sizeof(m_OccupancyMask));
    std::memset(m_VoxelDensities, 0, sizeof(m_VoxelDensities));
    const float radiusSqr = radius * radius;
    const float maxDensity = static_cast<float>(density) / 255.0f;

    for (int z = 0; z < GRID_DIM; ++z) {
        for (int y = 0; y < GRID_DIM; ++y) {
            for (int x = 0; x < GRID_DIM; ++x) {
                const Vector3 world = VoxelToWorld(x, y, z);
                const float dSqr = (world - m_vSmokeDetonationPos).LengthSqr();
                if (dSqr <= radiusSqr) {
                    const float falloff = 1.0f - (dSqr / radiusSqr);
                    SetVoxelActive(x, y, z, maxDensity * falloff);
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Feature 1: Bullet Carve-out Algorithm
// Clears cylindrical tunnel of radius tunnelRadius along bullet travel segment
// ----------------------------------------------------------------------------
int CSmokeVoxelGrid::CarveBulletTunnel(const Vector3& start, const Vector3& end, float tunnelRadius) {
    int clearedCount = 0;
    const Vector3 dir = end - start;
    const float segLenSqr = dir.LengthSqr();
    const float radiusSqr = tunnelRadius * tunnelRadius;

    for (int z = 0; z < GRID_DIM; ++z) {
        for (int y = 0; y < GRID_DIM; ++y) {
            for (int x = 0; x < GRID_DIM; ++x) {
                if (!IsVoxelActive(x, y, z)) continue;

                const Vector3 pt = VoxelToWorld(x, y, z);
                const float t = (segLenSqr > 1e-6f) ? std::clamp((pt - start).Dot(dir) / segLenSqr, 0.0f, 1.0f) : 0.0f;
                const Vector3 closestPt = start + dir * t;
                const float distSqr = (pt - closestPt).LengthSqr();

                if (distSqr <= radiusSqr) {
                    ClearVoxel(x, y, z);
                    clearedCount++;
                }
            }
        }
    }
    m_nVoxelUpdateCounter++;
    return clearedCount;
}

// ----------------------------------------------------------------------------
// Feature 2: HE Grenade Explosion Shockwave Carve & Displacement
// Clears spherical core and pushes adjacent voxels outward (+30.0 units) with 29-tick cooldown
// ----------------------------------------------------------------------------
int CSmokeVoxelGrid::CarveExplosionShockwave(const Vector3& explosionCenter,
                                            float clearRadius,
                                            float pushRadius,
                                            int32_t cooldownTicks)
{
    int affectedCount = 0;
    const float clearRadiusSqr = clearRadius * clearRadius;
    const float pushRadiusSqr  = pushRadius * pushRadius;

    m_nShockwaveCooldownTicks = cooldownTicks;

    for (int z = 0; z < GRID_DIM; ++z) {
        for (int y = 0; y < GRID_DIM; ++y) {
            for (int x = 0; x < GRID_DIM; ++x) {
                if (!IsVoxelActive(x, y, z)) continue;

                const Vector3 pt = VoxelToWorld(x, y, z);
                const float dSqr = (pt - explosionCenter).LengthSqr();

                if (dSqr <= clearRadiusSqr) {
                    ClearVoxel(x, y, z);
                    affectedCount++;
                }
                else if (dSqr <= pushRadiusSqr) {
                    // server.dll: Push voxels outward along explosion normal by +30.0 units
                    const float d = std::sqrt(dSqr);
                    const Vector3 pushDir = (d > 1e-6f) ? (pt - explosionCenter) / d : Vector3{0, 0, 1};
                    const Vector3 pushedPt = pt + pushDir * SHOCKWAVE_DISPLACEMENT_UNITS;

                    int nx, ny, nz;
                    if (WorldToVoxel(pushedPt, nx, ny, nz)) {
                        const uint32_t oldMorton = Morton3D(static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(z));
                        const float oldDensity = m_VoxelDensities[oldMorton];
                        ClearVoxel(x, y, z);
                        SetVoxelActive(nx, ny, nz, oldDensity * 0.85f);
                        affectedCount++;
                    }
                }
            }
        }
    }
    m_nVoxelUpdateCounter++;
    return affectedCount;
}

void CSmokeVoxelGrid::DetonateHE(const Vector3& center, float blastRadius) {
    CarveExplosionShockwave(center, blastRadius * 0.7f, blastRadius, SHOCKWAVE_COOLDOWN_TICKS);
}

// ----------------------------------------------------------------------------
// Feature 3: Line-of-Sight 3D DDA Raymarcher (server.dll @ 0x1809b82f0 / 0x1809cd690)
// Exact optical density occlusion threshold: 0.20f
// ----------------------------------------------------------------------------
float CSmokeVoxelGrid::LineOfSightRaymarch(const Vector3& start, const Vector3& end) const {
    const Vector3 ray = end - start;
    const float rayLen = ray.Length();
    if (rayLen < 1e-4f) return 0.0f;

    const Vector3 rayDir = ray / rayLen;
    float accumulatedDensity = 0.0f;

    const float stepSize = m_VoxelSize * 0.5f;
    int maxSteps = static_cast<int>(rayLen / stepSize) + 1;
    maxSteps = std::min(maxSteps, 128); // Cap iterations at 128

    for (int i = 0; i <= maxSteps; ++i) {
        const float dist = static_cast<float>(i) * stepSize;
        if (dist > rayLen) break;

        const Vector3 currentPt = start + rayDir * dist;
        int vx, vy, vz;
        if (WorldToVoxel(currentPt, vx, vy, vz)) {
            if (IsVoxelActive(vx, vy, vz)) {
                const uint32_t morton = Morton3D(static_cast<uint32_t>(vx), static_cast<uint32_t>(vy), static_cast<uint32_t>(vz));
                const float density = m_VoxelDensities[morton];
                accumulatedDensity += density * (stepSize / m_VoxelSize) * 0.12f;

                if (accumulatedDensity >= FULL_OCCLUSION_DENSITY) {
                    return 1.0f; // Vision completely blocked
                }
            }
        }
    }

    return std::clamp(accumulatedDensity / FULL_OCCLUSION_DENSITY, 0.0f, 1.0f);
}

bool CSmokeVoxelGrid::LineOfSight(const Vector3& start, const Vector3& end, float occlusionThreshold) const {
    const float opacity = LineOfSightRaymarch(start, end);
    const float thresholdNormalized = (occlusionThreshold >= 1.0f) ? 1.0f : (occlusionThreshold / FULL_OCCLUSION_DENSITY);
    return opacity < thresholdNormalized;
}

// ----------------------------------------------------------------------------
// Feature 4: Sparse Wire Serialization (server.dll @ 0x1809b2230)
// Encodes active bitmask words into compact delta list (2-byte index + 8-byte mask = 10 bytes)
// ----------------------------------------------------------------------------
std::vector<SparseVoxelWord_t> CSmokeVoxelGrid::SerializeSparseDelta() const {
    std::vector<SparseVoxelWord_t> packet;
    for (uint16_t w = 0; w < BITMASK_WORDS; ++w) {
        const uint64_t mask = m_OccupancyMask[m_nActiveBuffer][w];
        if (mask != 0) {
            packet.push_back({w, mask});
        }
    }
    return packet;
}

void CSmokeVoxelGrid::DeserializeSparseDelta(const std::vector<SparseVoxelWord_t>& packet) {
    std::memset(m_OccupancyMask[m_nActiveBuffer], 0, sizeof(m_OccupancyMask[m_nActiveBuffer]));
    for (const auto& record : packet) {
        if (record.word_index < BITMASK_WORDS) {
            m_OccupancyMask[m_nActiveBuffer][record.word_index] = record.mask;
        }
    }
}

} // namespace cs2::particles
