#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "interfaces_registry.hpp"
#include "entity_system.hpp"
#include "schema_system.hpp"

// ============================================================================
// Source 2 Reverse Engineered SDK - Phase 2: Client Classes Hierarchy
// Target Build: 2026-10-25_26144732 (CS2 Update Archive)
// Architecture: x86-64 (MSVC ABI)
// Binary: client.dll (Base: 0x180000000)
// Reconstructed & Authoritatively Verified via Static Schema Reflection & RTTI
// ============================================================================

namespace cs2::sdk {

#ifndef PAD
#define PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:
#endif

// ============================================================================
// Fundamental Geometric & Math Structures
// ============================================================================
#ifndef CS2_SDK_MATH_TYPES_DEFINED
#define CS2_SDK_MATH_TYPES_DEFINED

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vector3() noexcept = default;
    constexpr Vector3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}
};
static_assert(sizeof(Vector3) == 0x0C, "Vector3 size must be 12 bytes");

struct QAngle {
    float pitch = 0.0f;
    float yaw   = 0.0f;
    float roll  = 0.0f;

    constexpr QAngle() noexcept = default;
    constexpr QAngle(float p, float y, float r) noexcept : pitch(p), yaw(y), roll(r) {}
};
static_assert(sizeof(QAngle) == 0x0C, "QAngle size must be 12 bytes");

struct CTransform {
    float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f; // +0x00: Rotation Quaternion
    float px = 0.0f, py = 0.0f, pz = 0.0f;             // +0x10: Translation Vector
    float scale = 1.0f;                                // +0x1C: Uniform Scale Factor
};
static_assert(sizeof(CTransform) == 0x20, "CTransform size must be 32 bytes");

template <typename T>
struct CUtlVector {
    std::int32_t m_size      = 0;
    std::int32_t m_capacity  = 0;
    T*           m_pElements = nullptr;

    [[nodiscard]] constexpr std::int32_t Count() const noexcept { return m_size; }
    [[nodiscard]] constexpr T& Element(std::int32_t i) noexcept { return m_pElements[i]; }
    [[nodiscard]] constexpr const T& Element(std::int32_t i) const noexcept { return m_pElements[i]; }
    [[nodiscard]] constexpr T& operator[](std::int32_t i) noexcept { return m_pElements[i]; }
    [[nodiscard]] constexpr const T& operator[](std::int32_t i) const noexcept { return m_pElements[i]; }
};
static_assert(sizeof(CUtlVector<void*>) == 0x10, "CUtlVector size must be 16 bytes");

#endif // CS2_SDK_MATH_TYPES_DEFINED

// Forward declarations
class C_BaseEntity;
class C_BaseModelEntity;
class CBaseAnimGraph;
class C_BaseCombatCharacter;
class C_BasePlayerPawn;
class C_CSPlayerPawnBase;
class C_CSPlayerPawn;
class CBasePlayerController;
class CCSPlayerController;
class CGameSceneNode;
class CSkeletonInstance;
class CModelState;
class CCollisionProperty;
class CGlowProperty;
class C_BasePlayerWeapon;
class C_CSWeaponBase;
class CPlayerPawnComponent;
class CPlayer_WeaponServices;
class CCSPlayer_WeaponServices;
class CPlayer_ItemServices;
class CCSPlayer_ItemServices;
class CPlayer_ObserverServices;
class CCSObserver_ObserverServices;
class CPlayer_CameraServices;
class CCSPlayerBase_CameraServices;
class CCSPlayer_CameraServices;
class CCSPlayer_AimPunchServices;
class C_PlantedC4;

// ============================================================================
// 1. CCollisionProperty
// Binary: client.dll (Schema: "CCollisionProperty", Size: 0xB8)
// Verified Field Offsets:
//   m_vecMins:          +0x0040
//   m_vecMaxs:          +0x004C
//   m_usSolidFlags:     +0x005A
//   m_flBoundingRadius: +0x0060
// ============================================================================
class CCollisionProperty {
public:
    virtual ~CCollisionProperty() = default;         // +0x00
    PAD(0x08, m_pad08);                              // +0x08
    PAD(0x30, m_collisionAttribute);                 // +0x10: VPhysics collision attribute
    Vector3 m_vecMins;                               // +0x40: AABB bounding box minimum
    Vector3 m_vecMaxs;                               // +0x4C: AABB bounding box maximum
    PAD(0x02, m_pad58);                              // +0x58
    std::uint8_t m_usSolidFlags;                     // +0x5A: Solid flags bitmask
    std::uint8_t m_nSolidType;                       // +0x5B: SolidType_t
    std::uint8_t m_triggerBloat;                     // +0x5C: Trigger bloat
    std::uint8_t m_nSurroundType;                    // +0x5D: SurroundType_t
    std::uint8_t m_CollisionGroup;                   // +0x5E: CollisionGroup_t
    std::uint8_t m_nEnablePhysics;                   // +0x5F: Physics enable state
    float m_flBoundingRadius;                        // +0x60: Bounding sphere radius
    Vector3 m_vecSpecifiedSurroundingMins;           // +0x64
    Vector3 m_vecSpecifiedSurroundingMaxs;           // +0x70
    Vector3 m_vecSurroundingMaxs;                    // +0x7C
    Vector3 m_vecSurroundingMins;                    // +0x88
    Vector3 m_vCapsuleCenter1;                       // +0x94
    Vector3 m_vCapsuleCenter2;                       // +0xA0
    float m_flCapsuleRadius;                         // +0xAC
    PAD(0x08, m_padB0);                              // +0xB0..+0xB8
};
static_assert(sizeof(CCollisionProperty) == 0xB8, "CCollisionProperty size must be 0xB8");
static_assert(offsetof(CCollisionProperty, m_vecMins) == 0x40, "m_vecMins offset mismatch");
static_assert(offsetof(CCollisionProperty, m_vecMaxs) == 0x4C, "m_vecMaxs offset mismatch");
static_assert(offsetof(CCollisionProperty, m_usSolidFlags) == 0x5A, "m_usSolidFlags offset mismatch");
static_assert(offsetof(CCollisionProperty, m_flBoundingRadius) == 0x60, "m_flBoundingRadius offset mismatch");

// ============================================================================
// 2. CGlowProperty
// Binary: client.dll (Schema: "CGlowProperty", Size: 0x58)
// Verified Field Offsets:
//   m_fGlowColor:        +0x0008
//   m_iGlowType:         +0x0030
//   m_iGlowTeam:         +0x0034
//   m_glowColorOverride: +0x0040
//   m_bGlowing:          +0x0051
// ============================================================================
class CGlowProperty {
public:
    virtual ~CGlowProperty() = default;              // +0x00
    Vector3 m_fGlowColor;                            // +0x08: Vector RGB glow tint
    PAD(0x1C, m_pad14);                              // +0x14
    std::int32_t m_iGlowType;                        // +0x30: Glow style
    std::int32_t m_iGlowTeam;                        // +0x34: Team isolation index
    std::int32_t m_nGlowRange;                       // +0x38: Max range
    std::int32_t m_nGlowRangeMin;                    // +0x3C: Min range
    std::uint32_t m_glowColorOverride;               // +0x40: RGBA packed color override
    bool m_bFlashing;                                // +0x44: Pulsing strobe flag
    PAD(0x03, m_pad45);                              // +0x45
    float m_flGlowTime;                              // +0x48
    float m_flGlowStartTime;                         // +0x4C
    bool m_bEligibleForScreenHighlight;              // +0x50
    bool m_bGlowing;                                 // +0x51: Active glow state flag
    PAD(0x06, m_pad52);                              // +0x52..+0x58
};
static_assert(sizeof(CGlowProperty) == 0x58, "CGlowProperty size must be 0x58");
static_assert(offsetof(CGlowProperty, m_fGlowColor) == 0x08, "m_fGlowColor offset mismatch");
static_assert(offsetof(CGlowProperty, m_iGlowType) == 0x30, "m_iGlowType offset mismatch");
static_assert(offsetof(CGlowProperty, m_glowColorOverride) == 0x40, "m_glowColorOverride offset mismatch");
static_assert(offsetof(CGlowProperty, m_bGlowing) == 0x51, "m_bGlowing offset mismatch");

// ============================================================================
// 3. C_BaseEntity
// Base Class: CEntityInstance (Size: 0x30)
// Binary: client.dll (Schema: "C_BaseEntity", Size: 0x600)
// Verified Field Offsets:
//   m_pGameSceneNode:   +0x0330 (CGameSceneNode*)
//   m_iMaxHealth:       +0x0348 (int32_t)
//   m_iHealth:          +0x034C (int32_t)
//   m_lifeState:        +0x0354 (uint8_t: 0=Alive, 1=Dying, 2=Dead)
//   m_flSimulationTime: +0x03B8 (float)
//   m_iTeamNum:         +0x03E7 (uint8_t: 2=T, 3=CT)
//   m_fFlags:           +0x03F4 (uint32_t bitmask: FL_ONGROUND=1, FL_DUCKING=2)
//   m_vecAbsVelocity:   +0x03F8 (Vector3)
// ============================================================================
class C_BaseEntity : public CEntityInstance {
public:
    void* m_CBodyComponent;                          // +0x030
    void* m_NetworkTransmitComponent;                // +0x038
    PAD(0x02F0, m_pad40);                            // +0x040..+0x330
    CGameSceneNode* m_pGameSceneNode;                // +0x330: Spatial scene node pointer
    void* m_pRenderComponent;                        // +0x338
    CCollisionProperty* m_pCollision;                // +0x340: Collision property pointer
    std::int32_t m_iMaxHealth;                       // +0x348: Maximum health
    std::int32_t m_iHealth;                          // +0x34C: Current health points
    float m_flDamageAccumulator;                     // +0x350
    std::uint8_t m_lifeState;                        // +0x354: LifeState_t (0=Alive, 1=Dying, 2=Dead)
    bool m_bTakesDamage;                             // +0x355
    PAD(0x0062, m_pad356);                           // +0x356..+0x3B8
    float m_flSimulationTime;                        // +0x3B8: Simulation timestamp in seconds
    PAD(0x002B, m_pad3BC);                           // +0x3BC..+0x3E7
    std::uint8_t m_iTeamNum;                         // +0x3E7: Team index (2=Terrorist, 3=Counter-Terrorist)
    std::uint32_t m_spawnflags;                      // +0x3E8
    PAD(0x0008, m_pad3EC);                           // +0x3EC..+0x3F4
    std::uint32_t m_fFlags;                          // +0x3F4: Entity flags (FL_ONGROUND, FL_DUCKING)
    Vector3 m_vecAbsVelocity;                        // +0x3F8: World-space absolute velocity
    PAD(0x010C, m_pad404);                           // +0x404..+0x510
    Vector3 m_vecBaseVelocity;                       // +0x510: Inherited conveyor/push velocity
    PAD(0x0004, m_pad51C);                           // +0x51C..+0x520
    CHandle<C_BaseEntity> m_hOwnerEntity;            // +0x520: Owner entity handle
    std::uint8_t m_MoveCollide;                      // +0x524
    std::uint8_t m_MoveType;                         // +0x525: MoveType_t (MOVETYPE_WALK, etc.)
    std::uint8_t m_nActualMoveType;                  // +0x526
    PAD(0x0001, m_pad527);                           // +0x527
    float m_flWaterLevel;                            // +0x528
    std::uint32_t m_fEffects;                        // +0x52C
    CHandle<C_BaseEntity> m_hGroundEntity;           // +0x530: Entity stood on
    PAD(0x00CC, m_pad534);                           // +0x534..+0x600

    [[nodiscard]] constexpr bool IsAlive() const noexcept {
        return m_lifeState == 0 && m_iHealth > 0;
    }

    [[nodiscard]] constexpr bool IsOnGround() const noexcept {
        return (m_fFlags & (1 << 0)) != 0;
    }
};
static_assert(sizeof(C_BaseEntity) == 0x600, "C_BaseEntity size must be 0x600");
static_assert(offsetof(C_BaseEntity, m_pGameSceneNode) == 0x330, "m_pGameSceneNode offset mismatch");
static_assert(offsetof(C_BaseEntity, m_iMaxHealth) == 0x348, "m_iMaxHealth offset mismatch");
static_assert(offsetof(C_BaseEntity, m_iHealth) == 0x34C, "m_iHealth offset mismatch");
static_assert(offsetof(C_BaseEntity, m_lifeState) == 0x354, "m_lifeState offset mismatch");
static_assert(offsetof(C_BaseEntity, m_flSimulationTime) == 0x3B8, "m_flSimulationTime offset mismatch");
static_assert(offsetof(C_BaseEntity, m_iTeamNum) == 0x3E7, "m_iTeamNum offset mismatch");
static_assert(offsetof(C_BaseEntity, m_fFlags) == 0x3F4, "m_fFlags offset mismatch");
static_assert(offsetof(C_BaseEntity, m_vecAbsVelocity) == 0x3F8, "m_vecAbsVelocity offset mismatch");

// ============================================================================
// 4. C_BaseModelEntity
// Base Class: C_BaseEntity (Size: 0x600)
// Binary: client.dll (Schema: "C_BaseModelEntity", Size: 0x1098)
// Verified Field Offsets:
//   m_Collision:     +0x0D30 (CCollisionProperty, Size: 0xB8)
//   m_Glow:          +0x0DE8 (CGlowProperty, Size: 0x58)
//   m_vecViewOffset: +0x0F60 (Vector3, eye height relative to origin)
// ============================================================================
class C_BaseModelEntity : public C_BaseEntity {
public:
    PAD(0x0730, m_pad600);                           // +0x600..+0x0D30
    CCollisionProperty m_Collision;                  // +0x0D30: Embedded collision property
    CGlowProperty m_Glow;                            // +0x0DE8: Embedded glow property
    PAD(0x0120, m_padE40);                           // +0x0E40..+0x0F60
    Vector3 m_vecViewOffset;                         // +0x0F60: Camera offset from origin (z=64 standing, z=46 crouching)
    PAD(0x012C, m_padF6C);                           // +0x0F6C..+0x1098
};
static_assert(sizeof(C_BaseModelEntity) == 0x1098, "C_BaseModelEntity size must be 0x1098");
static_assert(offsetof(C_BaseModelEntity, m_Collision) == 0x0D30, "m_Collision offset mismatch");
static_assert(offsetof(C_BaseModelEntity, m_Glow) == 0x0DE8, "m_Glow offset mismatch");
static_assert(offsetof(C_BaseModelEntity, m_vecViewOffset) == 0x0F60, "m_vecViewOffset offset mismatch");

// ============================================================================
// 5. CBasePlayerController
// Base Class: C_BaseEntity (Size: 0x600)
// Binary: client.dll (Schema: "CBasePlayerController", Size: 0x800)
// Verified Field Offsets:
//   m_nTickBase:                +0x06B8
//   m_hPawn:                    +0x06BC (CHandle<C_BasePlayerPawn>)
//   m_bIsLocalPlayerController: +0x0790 (bool)
// ============================================================================
class CBasePlayerController : public C_BaseEntity {
public:
    PAD(0x00B8, m_pad600);                           // +0x600..+0x6B8
    std::uint32_t m_nTickBase;                       // +0x6B8: Current simulated tickbase
    CHandle<C_BasePlayerPawn> m_hPawn;               // +0x6BC: Associated base pawn handle
    PAD(0x00D0, m_pad6C0);                           // +0x6C0..+0x790
    bool m_bIsLocalPlayerController;                 // +0x790: True if controller belongs to local client
    PAD(0x006F, m_pad791);                           // +0x791..+0x800
};
static_assert(sizeof(CBasePlayerController) == 0x800, "CBasePlayerController size must be 0x800");
static_assert(offsetof(CBasePlayerController, m_nTickBase) == 0x6B8, "m_nTickBase offset mismatch");
static_assert(offsetof(CBasePlayerController, m_hPawn) == 0x6BC, "m_hPawn offset mismatch");
static_assert(offsetof(CBasePlayerController, m_bIsLocalPlayerController) == 0x790, "m_bIsLocalPlayerController offset mismatch");

// ============================================================================
// 6. CCSPlayerController
// Base Class: CBasePlayerController (Size: 0x800)
// Binary: client.dll (Schema: "CCSPlayerController", Size: 0x980)
// Verified Field Offsets:
//   m_bIsLocalPlayerController: +0x0790 (Inherited from CBasePlayerController)
//   m_iPing:                    +0x0838 (uint32_t)
//   m_sSanitizedPlayerName:     +0x0878 (const char*)
//   m_hPlayerPawn:              +0x092C (CHandle<C_CSPlayerPawn>)
//   m_bPawnIsAlive:             +0x0934 (bool)
//   m_iPawnHealth:              +0x0938 (int32_t)
//   m_iPawnArmor:               +0x093C (int32_t)
//   m_bPawnHasDefuser:          +0x0940 (bool)
//   m_bPawnHasHelmet:           +0x0941 (bool)
// ============================================================================
class CCSPlayerController : public CBasePlayerController {
public:
    PAD(0x0038, m_pad800);                           // +0x800..+0x838
    std::uint32_t m_iPing;                           // +0x838: Network ping in milliseconds
    PAD(0x003C, m_pad83C);                           // +0x83C..+0x878
    const char* m_sSanitizedPlayerName;              // +0x878: Null-terminated sanitized player nickname
    PAD(0x00AC, m_pad880);                           // +0x880..+0x92C
    CHandle<C_CSPlayerPawn> m_hPlayerPawn;           // +0x92C: Currently possessed player pawn handle
    CHandle<C_CSPlayerPawn> m_hObserverPawn;         // +0x930: Spectated target pawn handle
    bool m_bPawnIsAlive;                             // +0x934: Cached live pawn alive flag
    PAD(0x0003, m_pad935);                           // +0x935..+0x938
    std::int32_t m_iPawnHealth;                      // +0x938: Replicated scoreboard health points
    std::int32_t m_iPawnArmor;                       // +0x93C: Replicated scoreboard armor value
    bool m_bPawnHasDefuser;                          // +0x940: Replicated defusal kit possession flag
    bool m_bPawnHasHelmet;                           // +0x941: Replicated Kevlar helmet possession flag
    PAD(0x003E, m_pad942);                           // +0x942..+0x980
};
static_assert(sizeof(CCSPlayerController) == 0x980, "CCSPlayerController size must be 0x980");
static_assert(offsetof(CCSPlayerController, m_bIsLocalPlayerController) == 0x790, "m_bIsLocalPlayerController offset mismatch");
static_assert(offsetof(CCSPlayerController, m_iPing) == 0x838, "m_iPing offset mismatch");
static_assert(offsetof(CCSPlayerController, m_sSanitizedPlayerName) == 0x878, "m_sSanitizedPlayerName offset mismatch");
static_assert(offsetof(CCSPlayerController, m_hPlayerPawn) == 0x92C, "m_hPlayerPawn offset mismatch");
static_assert(offsetof(CCSPlayerController, m_bPawnIsAlive) == 0x934, "m_bPawnIsAlive offset mismatch");
static_assert(offsetof(CCSPlayerController, m_iPawnHealth) == 0x938, "m_iPawnHealth offset mismatch");
static_assert(offsetof(CCSPlayerController, m_iPawnArmor) == 0x93C, "m_iPawnArmor offset mismatch");
static_assert(offsetof(CCSPlayerController, m_bPawnHasDefuser) == 0x940, "m_bPawnHasDefuser offset mismatch");
static_assert(offsetof(CCSPlayerController, m_bPawnHasHelmet) == 0x941, "m_bPawnHasHelmet offset mismatch");

// ============================================================================
// 7. Component Services (Weapon, Item, Observer, Camera, AimPunch)
// ============================================================================

// --- Weapon Services ---
class CPlayer_WeaponServices {
public:
    virtual ~CPlayer_WeaponServices() = default;     // +0x00
    PAD(0x0040, m_pad08);                            // +0x08..+0x48
    CUtlVector<CHandle<C_BasePlayerWeapon>> m_hMyWeapons; // +0x48: Carried inventory weapons
    PAD(0x0008, m_pad58);                            // +0x58..+0x60
    CHandle<C_BasePlayerWeapon> m_hActiveWeapon;     // +0x60: Currently wielded active weapon
    PAD(0x0044, m_pad64);                            // +0x64..+0xA8
};
static_assert(sizeof(CPlayer_WeaponServices) == 0xA8, "CPlayer_WeaponServices size must be 0xA8");
static_assert(offsetof(CPlayer_WeaponServices, m_hMyWeapons) == 0x48, "m_hMyWeapons offset mismatch");
static_assert(offsetof(CPlayer_WeaponServices, m_hActiveWeapon) == 0x60, "m_hActiveWeapon offset mismatch");

class CCSPlayer_WeaponServices : public CPlayer_WeaponServices {
public:
    PAD(0x1560, m_padA8);                            // +0xA8..+0x1608
};
static_assert(sizeof(CCSPlayer_WeaponServices) == 0x1608, "CCSPlayer_WeaponServices size must be 0x1608");
static_assert(offsetof(CCSPlayer_WeaponServices, m_hMyWeapons) == 0x48, "m_hMyWeapons offset mismatch");
static_assert(offsetof(CCSPlayer_WeaponServices, m_hActiveWeapon) == 0x60, "m_hActiveWeapon offset mismatch");

// --- Item Services ---
class CPlayer_ItemServices {
public:
    virtual ~CPlayer_ItemServices() = default;       // +0x00
    PAD(0x0040, m_pad08);                            // +0x08..+0x48
};
static_assert(sizeof(CPlayer_ItemServices) == 0x48, "CPlayer_ItemServices size must be 0x48");

class CCSPlayer_ItemServices : public CPlayer_ItemServices {
public:
    bool m_bHasDefuser;                              // +0x48: Possession of defusal kit
    bool m_bHasHelmet;                               // +0x49: Possession of Kevlar helmet
    bool m_bHasHeavyArmor;                           // +0x4A: Heavy assault suit protection
    PAD(0x0005, m_pad4B);                            // +0x4B..+0x50
};
static_assert(sizeof(CCSPlayer_ItemServices) == 0x50, "CCSPlayer_ItemServices size must be 0x50");
static_assert(offsetof(CCSPlayer_ItemServices, m_bHasDefuser) == 0x48, "m_bHasDefuser offset mismatch");
static_assert(offsetof(CCSPlayer_ItemServices, m_bHasHelmet) == 0x49, "m_bHasHelmet offset mismatch");
static_assert(offsetof(CCSPlayer_ItemServices, m_bHasHeavyArmor) == 0x4A, "m_bHasHeavyArmor offset mismatch");

// --- Observer Services ---
class CPlayer_ObserverServices {
public:
    virtual ~CPlayer_ObserverServices() = default;   // +0x00
    PAD(0x0040, m_pad08);                            // +0x08..+0x48
    std::uint8_t m_iObserverMode;                    // +0x48: Spectator mode (OBS_MODE_IN_EYE, OBS_MODE_CHASE, OBS_MODE_ROAMING)
    PAD(0x0003, m_pad49);                            // +0x49..+0x4C
    CHandle<C_BaseEntity> m_hObserverTarget;         // +0x4C: Spectated target entity
    PAD(0x0010, m_pad50);                            // +0x50..+0x60
};
static_assert(sizeof(CPlayer_ObserverServices) == 0x60, "CPlayer_ObserverServices size must be 0x60");
static_assert(offsetof(CPlayer_ObserverServices, m_iObserverMode) == 0x48, "m_iObserverMode offset mismatch");
static_assert(offsetof(CPlayer_ObserverServices, m_hObserverTarget) == 0x4C, "m_hObserverTarget offset mismatch");

class CCSObserver_ObserverServices : public CPlayer_ObserverServices {
public:
    PAD(0x00A0, m_pad60);                            // +0x60..+0x100
};
static_assert(sizeof(CCSObserver_ObserverServices) == 0x100, "CCSObserver_ObserverServices size must be 0x100");
static_assert(offsetof(CCSObserver_ObserverServices, m_iObserverMode) == 0x48, "m_iObserverMode offset mismatch");
static_assert(offsetof(CCSObserver_ObserverServices, m_hObserverTarget) == 0x4C, "m_hObserverTarget offset mismatch");

using CCSPlayer_ObserverServices = CCSObserver_ObserverServices;

// --- Camera Services ---
class CPlayer_CameraServices {
public:
    virtual ~CPlayer_CameraServices() = default;     // +0x00
    PAD(0x0040, m_pad08);                            // +0x08..+0x48
    QAngle m_vecCsViewPunchAngle;                    // +0x48: View punch kick angles
    PAD(0x0244, m_pad54);                            // +0x54..+0x298
};
static_assert(sizeof(CPlayer_CameraServices) == 0x298, "CPlayer_CameraServices size must be 0x298");
static_assert(offsetof(CPlayer_CameraServices, m_vecCsViewPunchAngle) == 0x48, "m_vecCsViewPunchAngle offset mismatch");

class CCSPlayerBase_CameraServices : public CPlayer_CameraServices {
public:
    std::uint32_t m_iFOV;                            // +0x298: Current Field of View in degrees
    PAD(0x0014, m_pad29C);                           // +0x29C..+0x2B0
};
static_assert(sizeof(CCSPlayerBase_CameraServices) == 0x2B0, "CCSPlayerBase_CameraServices size must be 0x2B0");
static_assert(offsetof(CCSPlayerBase_CameraServices, m_iFOV) == 0x298, "m_iFOV offset mismatch");

class CCSPlayer_CameraServices : public CCSPlayerBase_CameraServices {
public:
    float m_flDeathCamTilt;                          // +0x2B0: Camera roll angle during death cam
    PAD(0x009C, m_pad2B4);                           // +0x2B4..+0x350
};
static_assert(sizeof(CCSPlayer_CameraServices) == 0x350, "CCSPlayer_CameraServices size must be 0x350");
static_assert(offsetof(CCSPlayer_CameraServices, m_flDeathCamTilt) == 0x2B0, "m_flDeathCamTilt offset mismatch");

// --- Aim Punch Services (Source 2 Recoil ODE Pipeline) ---
// Note: In CS2 Source 2, legacy m_aimPunchAngle & m_aimPunchCache were refactored from C_CSPlayerPawn
// into CCSPlayer_AimPunchServices (FUN_18085dc60 / CPlayer_AimPunchServices::SimulateAimPunchDecay).
class CCSPlayer_AimPunchServices {
public:
    virtual ~CCSPlayer_AimPunchServices() = default; // +0x00
    PAD(0x0040, m_pad08);                            // +0x08..+0x48
    std::uint32_t m_predictableBaseTick;             // +0x48
    float m_predictableBaseTickInterpAmount;         // +0x4C
    QAngle m_predictableBaseAngle;                   // +0x50: Primary recoil aim punch angle (m_aimPunchAngle)
    QAngle m_predictableBaseAngleVel;                // +0x5C: Recoil aim punch angular velocity spring state
    PAD(0x0038, m_pad68);                            // +0x68..+0xA0
    std::uint32_t m_unpredictableBaseTick;           // +0xA0
    QAngle m_unpredictableBaseAngle;                 // +0xA4
    PAD(0x0038, m_padB0);                            // +0xB0..+0xE8
};
static_assert(sizeof(CCSPlayer_AimPunchServices) == 0xE8, "CCSPlayer_AimPunchServices size must be 0xE8");
static_assert(offsetof(CCSPlayer_AimPunchServices, m_predictableBaseAngle) == 0x50, "m_predictableBaseAngle offset mismatch");
static_assert(offsetof(CCSPlayer_AimPunchServices, m_predictableBaseAngleVel) == 0x5C, "m_predictableBaseAngleVel offset mismatch");
static_assert(offsetof(CCSPlayer_AimPunchServices, m_unpredictableBaseAngle) == 0xA4, "m_unpredictableBaseAngle offset mismatch");

// ============================================================================
// 8. Player Pawn Hierarchy
// ============================================================================

// Base Anim Graph (Size: 0x1268)
class CBaseAnimGraph : public C_BaseModelEntity {
public:
    PAD(0x01D0, m_pad1098_anim);                     // +0x1098 (C_BaseModelEntity) .. +0x1268
};
static_assert(sizeof(CBaseAnimGraph) == 0x1268, "CBaseAnimGraph size must be 0x1268");

// Base Combat Character (Size: 0x12F0)
class C_BaseCombatCharacter : public CBaseAnimGraph {
public:
    PAD(0x0088, m_padCombat);                        // +0x1268 (CBaseAnimGraph) .. +0x12F0
};
static_assert(sizeof(C_BaseCombatCharacter) == 0x12F0, "C_BaseCombatCharacter size must be 0x12F0");

// Base Player Pawn (Size: 0x14C8)
class C_BasePlayerPawn : public C_BaseCombatCharacter {
public:
    CCSPlayer_WeaponServices* m_pWeaponServices;     // +0x12F0: Inventory & active weapon container
    CCSPlayer_ItemServices*   m_pItemServices;       // +0x12F8: Equipment (defuser, helmet, armor) container
    PAD(0x0008, m_pad1300);                          // +0x1300..+0x1308
    CPlayer_ObserverServices* m_pObserverServices;   // +0x1308: Spectator target & mode container
    PAD(0x0018, m_pad1310);                          // +0x1310..+0x1328
    CCSPlayer_CameraServices* m_pCameraServices;     // +0x1328: FOV & deathcam container
    PAD(0x0174, m_pad1330);                          // +0x1330..+0x14A4
    Vector3 m_vOldOrigin;                            // +0x14A4: Network un-interpolated base origin
    PAD(0x0018, m_pad14B0);                          // +0x14B0..+0x14C8
};
static_assert(sizeof(C_BasePlayerPawn) == 0x14C8, "C_BasePlayerPawn size must be 0x14C8");
static_assert(offsetof(C_BasePlayerPawn, m_pWeaponServices) == 0x12F0, "m_pWeaponServices offset mismatch");
static_assert(offsetof(C_BasePlayerPawn, m_pItemServices) == 0x12F8, "m_pItemServices offset mismatch");
static_assert(offsetof(C_BasePlayerPawn, m_pObserverServices) == 0x1308, "m_pObserverServices offset mismatch");
static_assert(offsetof(C_BasePlayerPawn, m_pCameraServices) == 0x1328, "m_pCameraServices offset mismatch");
static_assert(offsetof(C_BasePlayerPawn, m_vOldOrigin) == 0x14A4, "m_vOldOrigin offset mismatch");

// CS Player Pawn Base (Size: 0x1568)
class C_CSPlayerPawnBase : public C_BasePlayerPawn {
public:
    PAD(0x0044, m_pad14C8);                          // +0x14C8..+0x150C
    float m_flFlashMaxAlpha;                         // +0x150C: Flashbang max blinding alpha (255.0f)
    float m_flFlashDuration;                         // +0x1510: Flashbang effect duration timer in seconds
    PAD(0x0054, m_pad1514);                          // +0x1514..+0x1568
};
static_assert(sizeof(C_CSPlayerPawnBase) == 0x1568, "C_CSPlayerPawnBase size must be 0x1568");
static_assert(offsetof(C_CSPlayerPawnBase, m_flFlashMaxAlpha) == 0x150C, "m_flFlashMaxAlpha offset mismatch");
static_assert(offsetof(C_CSPlayerPawnBase, m_flFlashDuration) == 0x1510, "m_flFlashDuration offset mismatch");

// CS Player Pawn (The Primary Player Entity, Size: 0x3720)
class C_CSPlayerPawn : public C_CSPlayerPawnBase {
public:
    PAD(0x0030, m_pad1568);                          // +0x1568..+0x1598
    CCSPlayer_AimPunchServices* m_pAimPunchServices; // +0x1598: Recoil aim punch spring-damper component
    PAD(0x0900, m_pad15A0);                          // +0x15A0..+0x1EA0
    bool m_bIsScoped;                                // +0x1EA0: Weapon optical scope active flag
    bool m_bResumeZoom;                              // +0x1EA1: Automatic sniper zoom resumption flag
    bool m_bIsDefusing;                              // +0x1EA2: Active bomb defusal progress flag
    PAD(0x0015, m_pad1EA3);                          // +0x1EA3..+0x1EB8
    std::int32_t m_iShotsFired;                      // +0x1EB8: Consecutive continuous shots fired counter
    PAD(0x0014, m_pad1EBC);                          // +0x1EBC..+0x1ED0
    std::int32_t m_ArmorValue;                       // +0x1ED0: Current player Kevlar body armor amount
    PAD(0x172C, m_pad1ED4);                          // +0x1ED4..+0x3600
    QAngle m_angEyeAngles;                           // +0x3600: Network synchronized camera view pitch & yaw
    PAD(0x0114, m_pad360C);                          // +0x360C..+0x3720

    // Legacy AimPunch API Accessors
    [[nodiscard]] inline const QAngle& GetAimPunchAngle() const noexcept {
        if (m_pAimPunchServices)
            return m_pAimPunchServices->m_predictableBaseAngle;
        static const QAngle zero{0.0f, 0.0f, 0.0f};
        return zero;
    }

    [[nodiscard]] inline const QAngle& GetAimPunchAngleVel() const noexcept {
        if (m_pAimPunchServices)
            return m_pAimPunchServices->m_predictableBaseAngleVel;
        static const QAngle zero{0.0f, 0.0f, 0.0f};
        return zero;
    }
};
static_assert(sizeof(C_CSPlayerPawn) == 0x3720, "C_CSPlayerPawn size must be 0x3720");
static_assert(offsetof(C_CSPlayerPawn, m_pWeaponServices) == 0x12F0, "m_pWeaponServices offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_pItemServices) == 0x12F8, "m_pItemServices offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_pObserverServices) == 0x1308, "m_pObserverServices offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_pCameraServices) == 0x1328, "m_pCameraServices offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_vOldOrigin) == 0x14A4, "m_vOldOrigin offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_flFlashMaxAlpha) == 0x150C, "m_flFlashMaxAlpha offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_flFlashDuration) == 0x1510, "m_flFlashDuration offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_pAimPunchServices) == 0x1598, "m_pAimPunchServices offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_bIsScoped) == 0x1EA0, "m_bIsScoped offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_bResumeZoom) == 0x1EA1, "m_bResumeZoom offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_bIsDefusing) == 0x1EA2, "m_bIsDefusing offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_iShotsFired) == 0x1EB8, "m_iShotsFired offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_ArmorValue) == 0x1ED0, "m_ArmorValue offset mismatch");
static_assert(offsetof(C_CSPlayerPawn, m_angEyeAngles) == 0x3600, "m_angEyeAngles offset mismatch");

// ============================================================================
// 9. Skeletal Hierarchy & Bone Matrices (CGameSceneNode, CModelState, CSkeletonInstance)
// Reference: docs/09_skeleton_bone_hierarchy_and_animgraph.md
// ============================================================================

// Base Game Scene Node (Size: 0x130)
class CGameSceneNode {
public:
    virtual ~CGameSceneNode() = default;             // +0x00
    PAD(0x08, m_pad08);                              // +0x08
    CTransform m_nodeToWorld;                        // +0x10: Root spatial coordinate transformation
    C_BaseEntity* m_pOwner;                          // +0x30: Owning entity pointer
    CGameSceneNode* m_pParent;                       // +0x38: Parent node in spatial hierarchy
    CGameSceneNode* m_pChild;                        // +0x40: Child node
    CGameSceneNode* m_pNextSibling;                  // +0x48: Sibling node
    PAD(0x20, m_pad50);                              // +0x50..+0x70
    CEntityHandle m_hParent;                         // +0x70: Parent entity handle
    PAD(0x0C, m_pad74);                              // +0x74..+0x80
    Vector3 m_vecOrigin;                             // +0x80: Local origin offset
    PAD(0x2C, m_pad8C);                              // +0x8C..+0xB8
    QAngle m_angRotation;                            // +0xB8: Local rotation angles
    float m_flScale;                                 // +0xC4: Local scalar
    Vector3 m_vecAbsOrigin;                          // +0xC8: Absolute world position
    QAngle m_angAbsRotation;                         // +0xD4: Absolute world orientation angles
    float m_flAbsScale;                              // +0xE0: Absolute world scalar
    PAD(0x1F, m_padE4);                              // +0xE4..+0x103
    bool m_bDormant;                                 // +0x103: PVS dormancy culling flag
    PAD(0x2C, m_pad104);                             // +0x104..+0x130
};
static_assert(sizeof(CGameSceneNode) == 0x130, "CGameSceneNode size must be 0x130");
static_assert(offsetof(CGameSceneNode, m_nodeToWorld) == 0x10, "m_nodeToWorld offset mismatch");
static_assert(offsetof(CGameSceneNode, m_pOwner) == 0x30, "m_pOwner offset mismatch");
static_assert(offsetof(CGameSceneNode, m_vecOrigin) == 0x80, "m_vecOrigin offset mismatch");
static_assert(offsetof(CGameSceneNode, m_angRotation) == 0xB8, "m_angRotation offset mismatch");
static_assert(offsetof(CGameSceneNode, m_vecAbsOrigin) == 0xC8, "m_vecAbsOrigin offset mismatch");
static_assert(offsetof(CGameSceneNode, m_angAbsRotation) == 0xD4, "m_angAbsRotation offset mismatch");
static_assert(offsetof(CGameSceneNode, m_bDormant) == 0x103, "m_bDormant offset mismatch");

// Model State Container (Size: 0x2B0)
class CModelState {
public:
    virtual ~CModelState() = default;                // +0x00
    PAD(0x78, m_pad08);                              // +0x08..+0x80
    CTransform* m_pBoneArray;                        // +0x80: Pointer to dynamic CTransform bone matrix array
    PAD(0x0C, m_pad88);                              // +0x88..+0x94
    std::uint16_t m_nBoneCount;                      // +0x94: Total number of active bones
    PAD(0x0A, m_pad96);                              // +0x96..+0xA0
    std::uint64_t m_hModel;                          // +0xA0: CStrongHandle to compiled model asset
    const char* m_ModelName;                         // +0xA8: ASCII model asset path string
    PAD(0x38, m_padB0);                              // +0xB0..+0xE8
    float m_flRootBoneOffset_x;                      // +0xE8
    float m_flRootBoneOffset_y;                      // +0xEC
    float m_flRootBoneOffset_z;                      // +0xF0
    PAD(0x114, m_padF4);                             // +0xF4..+0x208
    std::uint64_t m_MeshGroupMask;                   // +0x208: Visible mesh group bitmask
    PAD(0xA0, m_pad210);                             // +0x210..+0x2B0
};
static_assert(sizeof(CModelState) == 0x2B0, "CModelState size must be 0x2B0");
static_assert(offsetof(CModelState, m_pBoneArray) == 0x80, "m_pBoneArray offset mismatch");
static_assert(offsetof(CModelState, m_nBoneCount) == 0x94, "m_nBoneCount offset mismatch");
static_assert(offsetof(CModelState, m_hModel) == 0xA0, "m_hModel offset mismatch");
static_assert(offsetof(CModelState, m_ModelName) == 0xA8, "m_ModelName offset mismatch");

// Skeleton Instance (Skeletal Mesh Runtime Hierarchy, Size: 0x490)
// Composite layout: m_modelState starts at +0x140.
// Bone transforms array: CSkeletonInstance + 0x01C0 (m_modelState + 0x80).
// Bone count:            CSkeletonInstance + 0x01D4 (m_modelState + 0x94).
class CSkeletonInstance : public CGameSceneNode {
public:
    PAD(0x10, m_pad130);                             // +0x130..+0x140
    CModelState m_modelState;                        // +0x140: Embedded model state (Size: 0x2B0)
    bool m_bUseParentRenderBounds;                   // +0x3F0
    PAD(0x0B, m_pad3F1);                             // +0x3F1..+0x3FC
    std::uint8_t m_nHitboxSet;                       // +0x3FC: Active hitbox set index (0=default, 1=headshots)
    PAD(0x5B, m_pad3FD);                             // +0x3FD..+0x458
    std::uint32_t m_nCalculatedBoneFlags;            // +0x458: Forward kinematics dirty bitmask
    PAD(0x34, m_pad45C);                             // +0x45C..+0x490

    [[nodiscard]] inline CTransform* GetBoneArray() const noexcept {
        return m_modelState.m_pBoneArray;
    }

    [[nodiscard]] inline std::uint16_t GetBoneCount() const noexcept {
        return m_modelState.m_nBoneCount;
    }
};
static_assert(sizeof(CSkeletonInstance) == 0x490, "CSkeletonInstance size must be 0x490");
static_assert(offsetof(CSkeletonInstance, m_modelState) == 0x140, "m_modelState offset mismatch");
static_assert(offsetof(CSkeletonInstance, m_bUseParentRenderBounds) == 0x3F0, "m_bUseParentRenderBounds offset mismatch");
static_assert(offsetof(CSkeletonInstance, m_nHitboxSet) == 0x3FC, "m_nHitboxSet offset mismatch");
static_assert(offsetof(CSkeletonInstance, m_nCalculatedBoneFlags) == 0x458, "m_nCalculatedBoneFlags offset mismatch");

// ============================================================================
// 10. Planted C4 Explosive (C_PlantedC4)
// Base Class: CBaseAnimGraph (Size: 0x1268) -> C_BaseModelEntity (Size: 0x1098)
// Binary: client.dll (Schema: "C_PlantedC4", Size: 0x1958)
// Verified Field Offsets:
//   m_bBombTicking:      +0x1288 (bool)
//   m_nBombSite:         +0x128C (int32_t: 0=Site A, 1=Site B)
//   m_flC4Blow:          +0x12B8 (float: game time of explosion)
//   m_flTimerLength:     +0x12C0 (float: default 40.0s)
//   m_flDefuseLength:    +0x12D4 (float: 5.0s with kit, 10.0s without)
//   m_flDefuseCountDown: +0x12D8 (float: game time defusal completes)
//   m_bBombDefused:      +0x12DC (bool)
//   m_hBombDefuser:      +0x12E0 (CHandle<C_CSPlayerPawn>)
// ============================================================================
class C_PlantedC4 : public CBaseAnimGraph {
public:
    PAD(0x0020, m_pad1268);                          // +0x1268..+0x1288
    bool m_bBombTicking;                             // +0x1288: Armed ticking countdown active flag
    PAD(0x03, m_pad1289);                            // +0x1289..+0x128C
    std::int32_t m_nBombSite;                        // +0x128C: Designated site (0=Site A, 1=Site B)
    PAD(0x28, m_pad1290);                            // +0x1290..+0x12B8
    float m_flC4Blow;                                // +0x12B8: Absolute game timestamp when bomb detonates
    bool m_bCannotBeDefused;                         // +0x12BC
    PAD(0x03, m_pad12BD);                            // +0x12BD..+0x12C0
    float m_flTimerLength;                           // +0x12C0: Total timer duration in seconds (mp_c4timer=40.0)
    bool m_bBeingDefused;                            // +0x12C4: Defusal interaction in progress flag
    PAD(0x0F, m_pad12C5);                            // +0x12C5..+0x12D4
    float m_flDefuseLength;                          // +0x12D4: Total defuse time required (5.0s kit, 10.0s no kit)
    float m_flDefuseCountDown;                       // +0x12D8: Absolute game timestamp when defusal succeeds
    bool m_bBombDefused;                             // +0x12DC: Bomb successfully disarmed flag
    PAD(0x03, m_pad12DD);                            // +0x12DD..+0x12E0
    CHandle<C_CSPlayerPawn> m_hBombDefuser;          // +0x12E0: Player pawn currently performing defusal
    PAD(0x0674, m_pad12E4);                          // +0x12E4..+0x1958

    [[nodiscard]] constexpr float GetDefuseTimeRemaining(float curtime) const noexcept {
        if (!m_bBeingDefused || m_bBombDefused) return 0.0f;
        float remaining = m_flDefuseCountDown - curtime;
        return remaining > 0.0f ? remaining : 0.0f;
    }

    [[nodiscard]] constexpr float GetBlowTimeRemaining(float curtime) const noexcept {
        if (!m_bBombTicking || m_bBombDefused) return 0.0f;
        float remaining = m_flC4Blow - curtime;
        return remaining > 0.0f ? remaining : 0.0f;
    }
};
static_assert(sizeof(C_PlantedC4) == 0x1958, "C_PlantedC4 size must be 0x1958");
static_assert(offsetof(C_PlantedC4, m_bBombTicking) == 0x1288, "m_bBombTicking offset mismatch");
static_assert(offsetof(C_PlantedC4, m_flC4Blow) == 0x12B8, "m_flC4Blow offset mismatch");
static_assert(offsetof(C_PlantedC4, m_flTimerLength) == 0x12C0, "m_flTimerLength offset mismatch");
static_assert(offsetof(C_PlantedC4, m_flDefuseLength) == 0x12D4, "m_flDefuseLength offset mismatch");
static_assert(offsetof(C_PlantedC4, m_flDefuseCountDown) == 0x12D8, "m_flDefuseCountDown offset mismatch");
static_assert(offsetof(C_PlantedC4, m_bBombDefused) == 0x12DC, "m_bBombDefused offset mismatch");
static_assert(offsetof(C_PlantedC4, m_hBombDefuser) == 0x12E0, "m_hBombDefuser offset mismatch");

} // namespace cs2::sdk
