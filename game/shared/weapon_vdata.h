#ifndef CS2_SHARED_WEAPON_VDATA_H
#define CS2_SHARED_WEAPON_VDATA_H

#include <cstdint>
#include <cstddef>

namespace cs2::shared {

#define CS2_PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

enum class CSWeaponType : std::int32_t {
    Knife          = 0,
    Pistol         = 1,
    Submachinegun  = 2,
    Rifle          = 3,
    Shotgun        = 4,
    SniperRifle    = 5,
    Machinegun     = 6,
    C4             = 7,
    Taser          = 8,
    Grenade        = 9,
    Equipment      = 10,
    StackableItem  = 11,
    Unknown        = 12
};

/**
 * @brief Weapon Configuration Metadata (CCSWeaponBaseVData).
 * Total Struct Size: exactly 2,208 bytes (0x8A0).
 * Verified against client.dll SchemaSystem class reflection.
 */
struct CCSWeaponBaseVData {
    CS2_PAD(0x520, m_pad0);
    CSWeaponType m_WeaponType{ CSWeaponType::Unknown }; // +0x0520: Subsystem classification
    CS2_PAD(0x1FC, m_pad524);
    const char*  m_szName{ nullptr };                   // +0x0720: Internal technical name (e.g. "weapon_ak47")
    CS2_PAD(0x10, m_pad728);
    float        m_flCycleTime{ 0.1f };                 // +0x0738: Time interval between successive rounds
    CS2_PAD(0x14, m_pad73c);
    float        m_flSpread{ 0.0f };                    // +0x0750: Intrinsic cone base spread
    CS2_PAD(0x04, m_pad754);
    float        m_flInaccuracyCrouch{ 0.0f };          // +0x0758: Inaccuracy penalty while crouching
    CS2_PAD(0x04, m_pad75c);
    float        m_flInaccuracyStand{ 0.0f };           // +0x0760: Inaccuracy penalty while standing
    CS2_PAD(0x04, m_pad764);
    float        m_flInaccuracyJump{ 0.0f };            // +0x0768: Inaccuracy penalty airborne
    CS2_PAD(0x1C, m_pad76c);
    float        m_flInaccuracyMove{ 0.0f };            // +0x0788: Inaccuracy penalty while traversing
    CS2_PAD(0x94, m_pad78c);
    std::int32_t m_nDamage{ 0 };                        // +0x0820: Base raw physical kinetic impact damage
    CS2_PAD(0x04, m_pad824);
    float        m_flArmorRatio{ 1.0f };                // +0x0828: Kinetic armor penetration scaling coefficient
    float        m_flPenetration{ 1.0f };               // +0x082C: Material barrier penetration capability
    float        m_flRange{ 8192.0f };                  // +0x0830: Maximum ballistic raycast range in units
    float        m_flRangeModifier{ 0.98f };            // +0x0834: Distance attenuation loss factor (500-unit decay)
    CS2_PAD(0x68, m_pad838);
};
static_assert(sizeof(CCSWeaponBaseVData) == 0x8A0, "CCSWeaponBaseVData must be 2,208 bytes (0x8A0)");

#undef CS2_PAD

} // namespace cs2::shared

#endif // CS2_SHARED_WEAPON_VDATA_H
