#ifndef CS2_PARTICLES_SMOKE_VOXELS_H
#define CS2_PARTICLES_SMOKE_VOXELS_H

#include <cstdint>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace cs2::particles {

// ----------------------------------------------------------------------------
// Authoritative Engine Constants & Virtual Offsets (server.dll @ Build 26598758)
// ----------------------------------------------------------------------------
constexpr uintptr_t VA_SMOKE_SPAWN              = 0x1809cba30; // RVA: 0x009cba30
constexpr uintptr_t VA_SMOKE_TICK_UPDATE        = 0x1809cec20; // RVA: 0x009cec20
constexpr uintptr_t VA_VOXEL_EXPAND_CARVE       = 0x1809c9070; // RVA: 0x009c9070
constexpr uintptr_t VA_LINE_OF_SIGHT_THROUGH_SMOKE = 0x1809b82f0; // RVA: 0x009b82f0
constexpr uintptr_t VA_VOXEL_RAYMARCH           = 0x1809cd690; // RVA: 0x009cd690
constexpr uintptr_t VA_SERIALIZE_SPARSE_DELTA   = 0x1809b2230; // RVA: 0x009b2230

constexpr float SMOKE_MAX_EXTENT               = 640.0f;  // Total diameter (units)
constexpr float SMOKE_HALF_EXTENT              = 320.0f;  // Bounding radius (units)
constexpr float VOXEL_SIZE                     = 20.0f;   // Size of each voxel cube (units)
constexpr float VOXEL_INV_SIZE                 = 0.05f;   // 1.0f / VOXEL_SIZE
constexpr int   GRID_DIM                       = 32;      // 32 voxels per axis (320 / 20 * 2)
constexpr int   GRID_TOTAL_VOXELS              = 32768;   // 32^3 = 32,768 voxels (0x8000)
constexpr int   BITMASK_WORDS                  = 512;     // 32768 / 64 = 512 uint64 words (0x200)
constexpr float GRID_CENTER_VOXEL              = 16.0f;   // Voxel index origin
constexpr float FULL_OCCLUSION_DENSITY         = 0.20f;   // server.dll: 0x1809b82f0 optical density threshold
constexpr float SHOCKWAVE_DISPLACEMENT_UNITS   = 30.0f;   // HE grenade displacement outward (+30.0 units)
constexpr int32_t SHOCKWAVE_COOLDOWN_TICKS     = 29;      // 29-tick hold cooldown for shockwave dissipation

// ----------------------------------------------------------------------------
// 3D Geometric Vector
// ----------------------------------------------------------------------------
struct Vector3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    constexpr Vector3() noexcept = default;
    constexpr Vector3(float _x, float _y, float _z) noexcept : x(_x), y(_y), z(_z) {}

    [[nodiscard]] constexpr Vector3 operator+(const Vector3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    [[nodiscard]] constexpr Vector3 operator-(const Vector3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    [[nodiscard]] constexpr Vector3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    [[nodiscard]] constexpr Vector3 operator/(float s) const noexcept { return {x / s, y / s, z / s}; }

    [[nodiscard]] constexpr float Dot(const Vector3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] float LengthSqr() const noexcept { return Dot(*this); }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(LengthSqr()); }

    [[nodiscard]] Vector3 Normalized() const noexcept {
        const float len = Length();
        return (len > 1e-6f) ? (*this / len) : Vector3{0, 0, 0};
    }
};

// ----------------------------------------------------------------------------
// 10-Byte Sparse Wire Serialization Record (Protobuf / Net-Channel Delta)
// server.dll FUN_1809b1bd0 / VA 0x1809b2230
// [uint16_t word_index (0..511)] + [uint64_t mask (64 occupancy bits)]
// ----------------------------------------------------------------------------
#pragma pack(push, 1)
struct SparseVoxelWord_t {
    uint16_t word_index; // 2 bytes (0..511)
    uint64_t mask;       // 8 bytes (64-bit occupancy mask)
};
#pragma pack(pop)

static_assert(sizeof(SparseVoxelWord_t) == 10, "SparseVoxelWord_t must be exactly 10 bytes");
using SparseVoxelWordPB = SparseVoxelWord_t;

/**
 * @brief Authoritative Volumetric Smoke Voxel Grid Reconstructed from server.dll
 * Build 26598758:
 * - 32x32x32 = 32,768 voxel bounding box centered at m_vSmokeDetonationPos.
 * - 3D Morton space-filling curve bit interleaving (Morton3D / Part1By2).
 * - Double-buffered 512-word uint64 bitmask array (512 * 64 = 32,768 bits).
 * - Authoritative Line-of-Sight DDA raymarching with 0.20f optical density occlusion threshold.
 * - Cylindrical bullet tunnel carve-out & spherical HE shockwave displacement (+30.0 units, 29-tick cooldown).
 * - 10-byte sparse wire protocol delta packing.
 */
class CSmokeVoxelGrid {
public:
    static constexpr int kGridDim = GRID_DIM;
    static constexpr int kTotalVoxels = GRID_TOTAL_VOXELS;
    static constexpr int kBitmaskWords = BITMASK_WORDS;

    CSmokeVoxelGrid(const Vector3& origin = {}, float voxelSize = VOXEL_SIZE);

    /**
     * @brief Encodes 3D coordinates into Morton Z-order index (0..32767).
     * server.dll: Part1By2 bit interleaving.
     */
    static uint32_t Part1By2(uint32_t x) noexcept;
    static uint32_t Morton3D(uint32_t x, uint32_t y, uint32_t z) noexcept;
    static void MortonDecode3D(uint32_t code, uint32_t& x, uint32_t& y, uint32_t& z) noexcept;

    /**
     * @brief Initializes a volumetric smoke cloud at world origin.
     */
    void Detonate(const Vector3& origin, int32_t tick = 0);
    void InitializeSphere(float radius, uint8_t density = 255);
    void ExpandInitialVolume(float radiusUnits = 80.0f);

    /**
     * @brief Simulation tick update managing shockwave cooldown and dissipation.
     */
    void OnTick(int32_t currentTick);

    // Coordinate conversions
    [[nodiscard]] bool WorldToVoxel(const Vector3& worldPos, int& vx, int& vy, int& vz) const noexcept;
    [[nodiscard]] Vector3 VoxelToWorld(int vx, int vy, int vz) const noexcept;

    // Voxel bitmask & density state
    [[nodiscard]] bool IsVoxelActive(int vx, int vy, int vz) const noexcept;
    void SetVoxelActive(int vx, int vy, int vz, float density = 1.0f);
    void ClearVoxel(int vx, int vy, int vz);

    /**
     * @brief Authoritative Line-of-Sight DDA Raymarching
     * Reconstructed from LineOfSightThroughSmoke @ RVA 0x009b82f0 / VA 0x1809b82f0.
     * Evaluates optical density along ray; returns true if visible, false if obstructed.
     * Occlusion occurs when accumulated optical density >= 0.20f.
     */
    [[nodiscard]] bool LineOfSight(const Vector3& start, const Vector3& end,
                                  float occlusionThreshold = FULL_OCCLUSION_DENSITY) const;

    /**
     * @brief Detailed raymarch returning normalized opacity in range [0.0, 1.0].
     */
    [[nodiscard]] float LineOfSightRaymarch(const Vector3& start, const Vector3& end) const;

    /**
     * @brief Cylindrical bullet tunnel carve-out algorithm.
     * Carves a cylinder of radius tunnelRadius along bullet travel segment.
     */
    int CarveBulletTunnel(const Vector3& start, const Vector3& end, float tunnelRadius = 30.0f);

    /**
     * @brief HE Grenade Explosion Shockwave Carve & Displacement
     * Clears spherical core and pushes adjacent voxels outward (+30.0 units) with 29-tick cooldown.
     */
    int CarveExplosionShockwave(const Vector3& explosionCenter,
                                float clearRadius = 180.0f,
                                float pushRadius = 260.0f,
                                int32_t cooldownTicks = SHOCKWAVE_COOLDOWN_TICKS);

    // Legacy overload
    void DetonateHE(const Vector3& center, float blastRadius);

    /**
     * @brief 10-Byte Sparse Wire Protocol Serialization
     * Reconstructed from SerializeSparseDelta @ RVA 0x009b2230 / VA 0x1809b2230.
     */
    [[nodiscard]] std::vector<SparseVoxelWord_t> SerializeSparseDelta() const;
    void DeserializeSparseDelta(const std::vector<SparseVoxelWord_t>& packet);

    // Double-buffering helpers
    void SwapBuffer() noexcept { m_nActiveBuffer ^= 1; }
    [[nodiscard]] int GetActiveBufferIndex() const noexcept { return m_nActiveBuffer; }

    // Legacy accessors
    [[nodiscard]] uint8_t GetVoxel(int x, int y, int z) const;
    void SetVoxel(int x, int y, int z, uint8_t val);

    [[nodiscard]] const Vector3& GetOrigin() const noexcept { return m_vSmokeDetonationPos; }
    [[nodiscard]] float GetVoxelSize() const noexcept { return m_VoxelSize; }
    [[nodiscard]] bool IsActive() const noexcept { return m_bActive; }
    [[nodiscard]] int32_t GetShockwaveCooldown() const noexcept { return m_nShockwaveCooldownTicks; }

private:
    Vector3 m_vSmokeDetonationPos{0.0f, 0.0f, 0.0f};
    float   m_VoxelSize{VOXEL_SIZE};
    int32_t m_nStartTick{0};
    int32_t m_nCurrentTick{0};
    bool    m_bActive{false};

    // Double-buffered 512-word uint64 bitmask array (512 * 64 = 32,768 bits)
    uint64_t m_OccupancyMask[2][BITMASK_WORDS]{};
    int      m_nActiveBuffer{0};

    // Dense floating-point optical densities for local physics simulation
    float    m_VoxelDensities[GRID_TOTAL_VOXELS]{};

    // State tracking
    int32_t  m_nVoxelUpdateCounter{0};
    int32_t  m_nShockwaveCooldownTicks{0};
};

} // namespace cs2::particles

#endif // CS2_PARTICLES_SMOKE_VOXELS_H
