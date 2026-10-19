#ifndef CS2_SMOKE_VOXELS_H
#define CS2_SMOKE_VOXELS_H

#include <cstdint>
#include <vector>
#include <cmath>

namespace cs2::particles {

struct Vector3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    Vector3 operator+(const Vector3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    [[nodiscard]] float Dot(const Vector3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(Dot(*this)); }
};

/**
 * @brief Reconstructed Volumetric Smoke Grid from particles.dll.
 * Analyzed in FUN_1809b7f50 (LineOfSightThroughSmoke).
 */
class CSmokeVoxelGrid {
public:
    static constexpr int kGridDim = 32;
    static constexpr int kTotalVoxels = kGridDim * kGridDim * kGridDim; // 32,768

    CSmokeVoxelGrid(const Vector3& origin, float voxelSize);

    /**
     * @brief Encodes 3D integer coordinates into Morton Z-order index.
     */
    static uint32_t Morton3D(uint32_t x, uint32_t y, uint32_t z) noexcept;

    /**
     * @brief Initializes a spherical smoke plume of radius R.
     */
    void InitializeSphere(float radius, uint8_t density = 255);

    /**
     * @brief Performs 3D DDA raymarch to check if line of sight is obstructed by smoke.
     * Reconstructed from particles.dll @ FUN_1809b7f50.
     * @return true if visible (not blocked), false if smoke blocks vision.
     */
    [[nodiscard]] bool LineOfSight(const Vector3& start, const Vector3& end, float maxAccumulatedDensity = 120.0f) const;

    /**
     * @brief Carves a cylindrical or conical bullet tunnel through the smoke voxels.
     */
    void CarveBulletTunnel(const Vector3& start, const Vector3& end, float tunnelRadius);

    /**
     * @brief Carves a spherical cavity produced by an HE grenade detonation.
     */
    void DetonateHE(const Vector3& center, float blastRadius);

    [[nodiscard]] uint8_t GetVoxel(int x, int y, int z) const;
    void SetVoxel(int x, int y, int z, uint8_t val);

    [[nodiscard]] const Vector3& GetOrigin() const noexcept { return m_Origin; }
    [[nodiscard]] float GetVoxelSize() const noexcept { return m_VoxelSize; }

private:
    Vector3 m_Origin;
    float m_VoxelSize;
    std::vector<uint8_t> m_Voxels;
};

} // namespace cs2::particles

#endif // CS2_SMOKE_VOXELS_H
