#ifndef CS2_SHARED_HITBOX_H
#define CS2_SHARED_HITBOX_H

#include <cstdint>
#include "math_types.h"

namespace cs2::shared {

enum HitGroup_t : std::int32_t {
    HITGROUP_GENERIC   = 0,
    HITGROUP_HEAD      = 1,
    HITGROUP_CHEST     = 2,
    HITGROUP_STOMACH   = 3,
    HITGROUP_LEFTARM   = 4,
    HITGROUP_RIGHTARM  = 5,
    HITGROUP_LEFTLEG   = 6,
    HITGROUP_RIGHTLEG  = 7,
    HITGROUP_NECK      = 8,
    HITGROUP_UNUSED    = 9,
    HITGROUP_GEAR      = 10
};

/**
 * @brief Returns the authoritative CS2 damage multiplier for a target hitgroup.
 * Head: 4.0x, Stomach: 1.25x, Chest: 1.0x, Limbs: 0.75x.
 */
inline constexpr float GetHitGroupDamageMultiplier(HitGroup_t group) noexcept {
    switch (group) {
        case HITGROUP_HEAD:     return 4.00f;
        case HITGROUP_STOMACH:  return 1.25f;
        case HITGROUP_CHEST:    return 1.00f;
        case HITGROUP_LEFTARM:
        case HITGROUP_RIGHTARM:
        case HITGROUP_LEFTLEG:
        case HITGROUP_RIGHTLEG: return 0.75f;
        default:                return 1.00f;
    }
}

/**
 * @brief Studio bone hitbox bounding capsule / OBB (CHitBox)
 * Size: exactly 112 bytes (0x70)
 */
struct CHitBox {
    const char*   m_pName{nullptr};           // +0x00: Hitbox bone identifier string
    const char*   m_pSurfaceProperty{nullptr};// +0x08: Material physics surface
    const char*   m_pBoneName{nullptr};       // +0x10: Target skeleton bone
    Vector3D      m_vMinBounds{};             // +0x18: Local minimum bounding coordinates (12 bytes)
    Vector3D      m_vMaxBounds{};             // +0x24: Local maximum bounding coordinates (12 bytes)
    float         m_flShapeRadius{0.0f};      // +0x30: Capsule mantle radius
    std::uint32_t m_nBoneId{0};               // +0x34: Skeletal bone index
    std::int32_t  m_nHitGroup{0};             // +0x38: Authoritative HitGroup_t
    std::uint8_t  m_nPad[52]{};               // +0x3C: Studio model runtime padding
};
static_assert(sizeof(CHitBox) == 0x70, "CHitBox must be exactly 0x70 (112 bytes)");

/**
 * @brief Studio hitbox set array header (CHitBoxSet)
 * Size: exactly 48 bytes (0x30)
 */
struct CHitBoxSet {
    const char*   m_pName{nullptr};           // +0x00: Set name (e.g. "default")
    std::uint32_t m_nNameHash{0};             // +0x08: MurmurHash2 token
    std::uint32_t m_nPad0{0};                 // +0x0C
    std::uint64_t m_nPad1{0};                 // +0x10
    std::int32_t  m_nHitBoxCount{0};          // +0x18: Number of child hitboxes
    std::uint32_t m_nPad2{0};                 // +0x1C
    CHitBox*      m_pHitBoxes{nullptr};       // +0x20: Pointer to CHitBox array
    std::uint64_t m_nPad3{0};                 // +0x28
};
static_assert(sizeof(CHitBoxSet) == 0x30, "CHitBoxSet must be exactly 0x30 (48 bytes)");

} // namespace cs2::shared

#endif // CS2_SHARED_HITBOX_H
