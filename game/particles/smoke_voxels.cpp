#include "smoke_voxels.h"
#include <algorithm>

namespace cs2::particles {

static inline uint32_t Part1By2(uint32_t x) noexcept {
    x &= 0x000003ff;
    x = (x ^ (x << 16)) & 0xff0000ff;
    x = (x ^ (x <<  8)) & 0x0300f00f;
    x = (x ^ (x <<  4)) & 0x030c30c3;
    x = (x ^ (x <<  2)) & 0x09249249;
    return x;
}

uint32_t CSmokeVoxelGrid::Morton3D(uint32_t x, uint32_t y, uint32_t z) noexcept {
    return (Part1By2(z) << 2) | (Part1By2(y) << 1) | Part1By2(x);
}

CSmokeVoxelGrid::CSmokeVoxelGrid(const Vector3& origin, float voxelSize)
    : m_Origin(origin), m_VoxelSize(voxelSize), m_Voxels(kTotalVoxels, 0) {}

uint8_t CSmokeVoxelGrid::GetVoxel(int x, int y, int z) const {
    if (x < 0 || x >= kGridDim || y < 0 || y >= kGridDim || z < 0 || z >= kGridDim) {
        return 0;
    }
    uint32_t idx = Morton3D(static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(z));
    return idx < m_Voxels.size() ? m_Voxels[idx] : 0;
}

void CSmokeVoxelGrid::SetVoxel(int x, int y, int z, uint8_t val) {
    if (x < 0 || x >= kGridDim || y < 0 || y >= kGridDim || z < 0 || z >= kGridDim) {
        return;
    }
    uint32_t idx = Morton3D(static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(z));
    if (idx < m_Voxels.size()) {
        m_Voxels[idx] = val;
    }
}

void CSmokeVoxelGrid::InitializeSphere(float radius, uint8_t density) {
    std::fill(m_Voxels.begin(), m_Voxels.end(), uint8_t{0});
    const float radiusSqr = radius * radius;
    const float halfDim = (kGridDim * m_VoxelSize) * 0.5f;

    for (int z = 0; z < kGridDim; ++z) {
        float pz = (z + 0.5f) * m_VoxelSize - halfDim;
        for (int y = 0; y < kGridDim; ++y) {
            float py = (y + 0.5f) * m_VoxelSize - halfDim;
            for (int x = 0; x < kGridDim; ++x) {
                float px = (x + 0.5f) * m_VoxelSize - halfDim;
                float d2 = px * px + py * py + pz * pz;
                if (d2 <= radiusSqr) {
                    float falloff = 1.0f - (d2 / radiusSqr);
                    SetVoxel(x, y, z, static_cast<uint8_t>(density * falloff));
                }
            }
        }
    }
}

bool CSmokeVoxelGrid::LineOfSight(const Vector3& start, const Vector3& end, float maxAccumulatedDensity) const {
    Vector3 dir = end - start;
    float dist = dir.Length();
    if (dist < 1e-4f) return true;

    // Step size equal to half a voxel
    float stepDist = m_VoxelSize * 0.5f;
    int numSteps = static_cast<int>(dist / stepDist) + 1;
    Vector3 stepVector = dir * (stepDist / dist);

    float accumulatedDensity = 0.0f;
    Vector3 current = start;
    const float halfDim = (kGridDim * m_VoxelSize) * 0.5f;

    for (int i = 0; i < numSteps; ++i) {
        Vector3 local = current - m_Origin;
        int vx = static_cast<int>((local.x + halfDim) / m_VoxelSize);
        int vy = static_cast<int>((local.y + halfDim) / m_VoxelSize);
        int vz = static_cast<int>((local.z + halfDim) / m_VoxelSize);

        uint8_t voxelVal = GetVoxel(vx, vy, vz);
        accumulatedDensity += static_cast<float>(voxelVal) * (stepDist / m_VoxelSize);

        if (accumulatedDensity >= maxAccumulatedDensity) {
            return false; // Vision fully blocked
        }

        current = current + stepVector;
    }

    return true; // Vision clear
}

void CSmokeVoxelGrid::CarveBulletTunnel(const Vector3& start, const Vector3& end, float tunnelRadius) {
    Vector3 dir = end - start;
    float dist = dir.Length();
    if (dist < 1e-4f) return;

    Vector3 dirNorm = dir * (1.0f / dist);
    const float halfDim = (kGridDim * m_VoxelSize) * 0.5f;
    const float tunnelRadiusSqr = tunnelRadius * tunnelRadius;

    for (int z = 0; z < kGridDim; ++z) {
        float pz = (z + 0.5f) * m_VoxelSize - halfDim + m_Origin.z;
        for (int y = 0; y < kGridDim; ++y) {
            float py = (y + 0.5f) * m_VoxelSize - halfDim + m_Origin.y;
            for (int x = 0; x < kGridDim; ++x) {
                float px = (x + 0.5f) * m_VoxelSize - halfDim + m_Origin.x;
                Vector3 pt{px, py, pz};
                
                // Distance from pt to line segment [start, end]
                Vector3 v = pt - start;
                float proj = v.Dot(dirNorm);
                if (proj >= 0.0f && proj <= dist) {
                    Vector3 closest = start + (dirNorm * proj);
                    if ((pt - closest).Dot(pt - closest) <= tunnelRadiusSqr) {
                        SetVoxel(x, y, z, 0);
                    }
                }
            }
        }
    }
}

void CSmokeVoxelGrid::DetonateHE(const Vector3& center, float blastRadius) {
    const float halfDim = (kGridDim * m_VoxelSize) * 0.5f;
    const float blastRadiusSqr = blastRadius * blastRadius;

    for (int z = 0; z < kGridDim; ++z) {
        float pz = (z + 0.5f) * m_VoxelSize - halfDim + m_Origin.z;
        for (int y = 0; y < kGridDim; ++y) {
            float py = (y + 0.5f) * m_VoxelSize - halfDim + m_Origin.y;
            for (int x = 0; x < kGridDim; ++x) {
                float px = (x + 0.5f) * m_VoxelSize - halfDim + m_Origin.x;
                Vector3 pt{px, py, pz};
                Vector3 delta = pt - center;
                if (delta.Dot(delta) <= blastRadiusSqr) {
                    SetVoxel(x, y, z, 0);
                }
            }
        }
    }
}

} // namespace cs2::particles
