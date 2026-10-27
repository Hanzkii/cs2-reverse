#ifndef CS2_SHARED_RAY_TRACE_H
#define CS2_SHARED_RAY_TRACE_H

#include <cstdint>
#include <cstddef>
#include "math_types.h"

namespace cs2::shared {

#define CS2_PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

#ifdef OPAQUE
#undef OPAQUE
#endif
#ifdef TRANSPARENT
#undef TRANSPARENT
#endif

namespace Contents {
    constexpr std::uint32_t EMPTY        = 0x00000000;
    constexpr std::uint32_t SOLID        = 0x00000001;
    constexpr std::uint32_t WINDOW       = 0x00000002;
    constexpr std::uint32_t MOVEABLE     = 0x00004000;
    constexpr std::uint32_t PLAYERCLIP   = 0x00010000;
    constexpr std::uint32_t MONSTERCLIP  = 0x00020000;
    constexpr std::uint32_t OPAQUE       = 0x00000080;
    constexpr std::uint64_t MASK_SHOT    = 0x46004003;
    constexpr std::uint64_t MASK_VISIBLE = 0x00000081;
}

/**
 * @brief 64-byte Geometric Query Ray & Swept Extent Volume.
 * Reconstructed from client.dll @ 0x180a1ae30 / vphysics2.dll @ 0x1802544a0.
 */
struct alignas(16) Ray_t {
    Vector3D      m_vStart;         // +0x00: Ray segment trajectory origin (12 bytes)
    Vector3D      m_vEnd;           // +0x0C: Ray segment trajectory terminus (12 bytes)
    Vector3D      m_vOffset;        // +0x18: Swept centroid displacement vector (12 bytes)
    Vector3D      m_vExtent;        // +0x24: Half-extent bounding box dimensions (12 bytes)
    bool          m_bIsRay;         // +0x30: True if line ray (extent == 0), false if swept volume (1 byte)
    bool          m_bHasExtents;    // +0x31: True if swept volume possesses non-zero bounds (1 byte)
    CS2_PAD(0x06, m_pad32);         // +0x32: Alignment padding
    std::uint64_t m_nMask;          // +0x38: 64-bit spatial collision mask (8 bytes)

    void Init(const Vector3D& start, const Vector3D& end, std::uint64_t mask = Contents::MASK_SHOT) noexcept {
        m_vStart = start;
        m_vEnd = end;
        m_vOffset = Vector3D{0.0f, 0.0f, 0.0f};
        m_vExtent = Vector3D{0.0f, 0.0f, 0.0f};
        m_bIsRay = true;
        m_bHasExtents = false;
        m_nMask = mask;
    }
};
static_assert(sizeof(Ray_t) == 0x40, "Ray_t must be exactly 64 bytes (0x40)");

/**
 * @brief 72-byte Spatial Query Filter (CTraceFilter).
 * Reconstructed from client.dll VTable @ 0x181ad4838.
 */
struct CTraceFilter {
    std::uintptr_t m_pVTable{ 0x181ad4838 };   // +0x00: Virtual method table pointer
    std::uint64_t  m_nTraceMask{ Contents::MASK_SHOT }; // +0x08: 64-bit spatial filtering bitmask
    std::uint64_t  m_v1[2]{ 0, 0 };            // +0x10: Internal collision flags
    std::uint32_t  m_nSkipHandles[2]{ 0xFFFFFFFF, 0xFFFFFFFF }; // +0x20: Entity handle exclusion pair
    std::uint16_t  m_nCollisions[2]{ 0, 0 };   // +0x28: Collision class indicators
    std::uint8_t   m_bV3{ 0 };                 // +0x2C: Internal predicate byte
    CS2_PAD(0x03, m_pad2d);                    // +0x2D
    std::uint64_t  m_uV4{ 0x7 };               // +0x30: Traversal flags
    std::uint8_t   m_bV5{ 0 };                 // +0x38: Dynamic pass indicator
    CS2_PAD(0x07, m_pad39);                    // +0x39

    CTraceFilter() = default;
    explicit CTraceFilter(std::uint64_t mask, std::uint32_t skipHandle = 0xFFFFFFFF)
        : m_nTraceMask(mask) {
        m_nSkipHandles[0] = skipHandle;
    }

    [[nodiscard]] virtual bool ShouldHitEntity(void* /*pEntity*/, std::uint64_t /*contentsMask*/) const {
        return true;
    }
};
static_assert(sizeof(CTraceFilter) == 0x48, "CTraceFilter must be exactly 72 bytes (0x48)");

/**
 * @brief 192-byte Authoritative Ray Intersection & Physics Query Result (GameTrace_t).
 * Reconstructed from client.dll. Contains the 40-byte internal Rubikon physics sweep buffer.
 */
struct alignas(16) GameTrace_t {
    void*         m_pSurface{ nullptr };       // +0x00: Surface physics descriptor pointer
    void*         m_pHitEntity{ nullptr };     // +0x08: Hit C_BaseEntity entity pointer
    void*         m_pHitboxData{ nullptr };    // +0x10: Target hitbox runtime descriptor pointer
    std::uint64_t m_nContents{ 0 };            // +0x18: Point contents flags at hit position
    std::uint64_t m_nHitBoxIndex{ 0 };         // +0x20: Hitbox bone index
    CS2_PAD(0x08, m_pad28);                    // +0x28
    Vector3D      m_vStartPos{};               // +0x30: Trajectory origin position
    CS2_PAD(0x04, m_pad3c);                    // +0x3C
    Vector3D      m_vEndPos{};                 // +0x40: Trajectory terminus position
    CS2_PAD(0x04, m_pad4c);                    // +0x4C
    std::uint8_t  m_rubikonBuffer[40]{};       // +0x50: Internal Rubikon sweep buffer (0x28 bytes)
    Vector3D      m_vRayStart{};               // +0x78: Exact evaluated ray entry vector
    Vector3D      m_vRayEnd{};                 // +0x84: Exact evaluated ray exit vector
    CS2_PAD(0x08, m_pad90);                    // +0x90
    CS2_PAD(0x04, m_pad98);                    // +0x98
    Vector3D      m_vHitNormal{};              // +0x9C: Surface unit normal vector at intersection
    std::uint32_t m_nSpatialVolumeIndex{ 0 };  // +0xA8: Rubikon spatial subdivision cell / hitgroup ID
    float         m_flFraction{ 1.0f };        // +0xAC: Intersection fraction [0.0..1.0] along ray (1.0 = miss)
    std::uint64_t m_nHitFlags{ 0 };            // +0xB0: Intersection status flags
    std::uint16_t m_nSurfaceFlags{ 0 };        // +0xB8: Physical material surface descriptor flags
    std::uint8_t  m_bStartSolid{ 0 };          // +0xBA: True if ray started inside a solid geometry volume
    CS2_PAD(0x05, m_padbb);                    // +0xBB

    [[nodiscard]] constexpr bool DidHit() const noexcept {
        return m_flFraction < 1.0f || m_bStartSolid;
    }

    [[nodiscard]] constexpr bool IsVisible() const noexcept {
        return m_flFraction > 0.97f && !m_bStartSolid;
    }
};
static_assert(sizeof(GameTrace_t) == 0xC0, "GameTrace_t must be exactly 192 bytes (0xC0)");

#undef CS2_PAD

} // namespace cs2::shared

#endif // CS2_SHARED_RAY_TRACE_H
