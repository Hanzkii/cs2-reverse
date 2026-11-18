//===========================================================================//
// Valve Source 2 Engine - Clean-Room SDK
// Module: server.dll (CS2 Build 2026-11-11_26598758)
// Subsystem: CCSPlayerController & CCSPlayerPawn 1:1 Linkage
//===========================================================================//

#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace cs2::server {

constexpr uint32_t INVALID_EHANDLE_INDEX = 0xFFFFFFFF;
constexpr uint32_t ENT_ENTRY_MASK        = 0x7FFF;
constexpr uint32_t NUM_ENT_ENTRIES       = 512;

// 32-bit CEntityHandle bitfield
class CEntityHandle {
public:
    uint32_t m_Index{ INVALID_EHANDLE_INDEX };

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return m_Index != INVALID_EHANDLE_INDEX && m_Index != 0xFFFFFFFE;
    }

    [[nodiscard]] constexpr uint32_t GetEntryIndex() const noexcept {
        return m_Index & ENT_ENTRY_MASK;
    }

    [[nodiscard]] constexpr uint32_t GetSerialNumber() const noexcept {
        return m_Index >> 15;
    }

    constexpr bool operator==(const CEntityHandle& other) const noexcept = default;
};
static_assert(sizeof(CEntityHandle) == 0x4);

template <typename T>
class CHandle : public CEntityHandle {};
static_assert(sizeof(CHandle<void*>) == 0x4);

class CEntityInstance;

// 112-byte (0x70) Entity Identity Record
struct alignas(8) CEntityIdentity {
    CEntityInstance* m_pEntityInstance; // +0x00
    void*            m_pClass;          // +0x08
    CEntityHandle    m_handle;          // +0x10
    uint32_t         m_nameStringToken; // +0x14
    const char*      m_pszName;         // +0x18
    void*            m_pPrev;           // +0x20
    void*            m_pNext;           // +0x28
    void*            m_pPrevByClass;    // +0x30
    void*            m_pNextByClass;    // +0x38
    uint32_t         m_flags;           // +0x40
    uint8_t          pad_0x44[0x2C];    // +0x44
};
static_assert(sizeof(CEntityIdentity) == 0x70);
static_assert(offsetof(CEntityIdentity, m_handle) == 0x10);

// Base Entity Instance (+0x10 identity backpointer)
class CEntityInstance {
public:
    virtual ~CEntityInstance() = default;
    void*            m_pEntityInfo;      // +0x08
    CEntityIdentity* m_pEntityIdentity;  // +0x10
};
static_assert(offsetof(CEntityInstance, m_pEntityIdentity) == 0x10);

// CBaseEntity (+0x344 m_iTeamNum)
class CBaseEntity : public CEntityInstance {
public:
    uint8_t pad_0x18[0x32C];
    uint8_t m_iTeamNum;                 // +0x0344
    uint8_t pad_0x345[0x163];
};
static_assert(offsetof(CBaseEntity, m_iTeamNum) == 0x0344);

class CBasePlayerPawn;
class CCSPlayerPawn;

// CBasePlayerController (+0x4e0 m_hPawn, +0x514 m_iConnected, +0x51c m_iszPlayerName)
class CBasePlayerController : public CBaseEntity {
public:
    uint8_t                 pad_0x4a8[0x38];
    CHandle<CBasePlayerPawn> m_hPawn;               // +0x04e0
    uint8_t                 pad_0x4e4[0x30];
    int32_t                 m_iConnected;           // +0x0514
    int32_t                 m_iMostConnected;       // +0x0518
    char                    m_iszPlayerName[128];   // +0x051c (m_sSanitizedPlayerName)
    uint8_t                 pad_0x59c[0x23c];
};
static_assert(offsetof(CBasePlayerController, m_hPawn) == 0x04e0);
static_assert(offsetof(CBasePlayerController, m_iConnected) == 0x0514);
static_assert(offsetof(CBasePlayerController, m_iszPlayerName) == 0x051c);
static_assert(sizeof(CBasePlayerController) == 0x7d8);

// CCSPlayerController (Total Size: 0xab8)
class CCSPlayerController : public CBasePlayerController {
public:
    void*                   m_pInGameMoneyServices;      // +0x07e8
    void*                   m_pInventoryServices;        // +0x07f0
    void*                   m_pActionTrackingServices;   // +0x07f8
    void*                   m_pDamageServices;           // +0x0800
    uint8_t                 pad_0x808[0xe4];
    CHandle<CCSPlayerPawn>  m_hPlayerPawn;               // +0x08ec
    CHandle<CBaseEntity>    m_hObserverPawn;             // +0x08f0
    uint32_t                m_DesiredObserverMode;       // +0x08f4
    CHandle<CBaseEntity>    m_hDesiredObserverTarget;    // +0x08f8
    bool                    m_bPawnIsAlive;              // +0x08fc
    uint8_t                 pad_0x8fd[0x1bb];
};
static_assert(offsetof(CCSPlayerController, m_hPlayerPawn) == 0x08ec);
static_assert(offsetof(CCSPlayerController, m_hObserverPawn) == 0x08f0);
static_assert(offsetof(CCSPlayerController, m_bPawnIsAlive) == 0x08fc);
static_assert(sizeof(CCSPlayerController) == 0x0ab8);

// CBasePlayerPawn (Size: 0xcd0)
class CBasePlayerPawn : public CBaseEntity {
public:
    uint8_t                          pad_0x4a8[0x668];
    void*                            m_pWeaponServices;    // +0x0b10
    void*                            m_pItemServices;      // +0x0b18
    uint8_t                          pad_0xb20[0x28];
    void*                            m_pCameraServices;    // +0x0b48
    void*                            m_pMovementServices;  // +0x0b50
    uint8_t                          pad_0xb58[0x140];
    CHandle<CCSPlayerController>     m_hController;        // +0x0c98
    CHandle<CCSPlayerController>     m_hDefaultController; // +0x0c9c
    uint8_t                          pad_0xca0[0x30];
};
static_assert(offsetof(CBasePlayerPawn, m_pWeaponServices) == 0x0b10);
static_assert(offsetof(CBasePlayerPawn, m_pItemServices) == 0x0b18);
static_assert(offsetof(CBasePlayerPawn, m_pCameraServices) == 0x0b48);
static_assert(offsetof(CBasePlayerPawn, m_pMovementServices) == 0x0b50);
static_assert(offsetof(CBasePlayerPawn, m_hController) == 0x0c98);
static_assert(sizeof(CBasePlayerPawn) == 0x0cd0);

// CCSPlayerPawnBase (Size: 0xe20)
class CCSPlayerPawnBase : public CBasePlayerPawn {
public:
    uint8_t                          pad_0xcd0[0x144];
    CHandle<CCSPlayerController>     m_hOriginalController; // +0x0e14
    uint8_t                          pad_0xe18[0x8];
};
static_assert(offsetof(CCSPlayerPawnBase, m_hOriginalController) == 0x0e14);
static_assert(sizeof(CCSPlayerPawnBase) == 0x0e20);

// CCSPlayerPawn (Size: 0x15c0)
class CCSPlayerPawn : public CCSPlayerPawnBase {
public:
    uint8_t pad_0xe20[0x7a0];
};
static_assert(sizeof(CCSPlayerPawn) == 0x15c0);

} // namespace cs2::server
