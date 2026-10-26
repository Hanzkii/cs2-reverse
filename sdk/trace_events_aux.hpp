#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include "interfaces_registry.hpp"
#include "entity_system.hpp"
#include "client_classes.hpp"

// ============================================================================
// Source 2 Reconstructed SDK - Phase 4: World Tracing, Events & Auxiliary Data
// Target Build: 2026-10-25_26144732 (CS2 Update Archive)
// Architecture: x86-64 (MSVC ABI)
// Language Standard: C++20 (/std:c++20 /W4 /WX)
//
// Verified Components:
//   1. Spatial Query Manager (CGameTraceManager, Ray_t, CTraceFilter, GameTrace_t)
//   2. Volumetric Smoke Optical Occlusion (LineOfSightThroughSmoke @ 0x1807ce470)
//   3. Game Events Subsystem (IGameEventManager2, IGameEvent, IGameEventListener2)
//   4. Audio Subsystem (ISoundOpSystem, StartSoundEventInfo_t)
//   5. Network Telemetry Interface (INetChannelInfo, FlowDirection)
//   6. Weapon Configuration Metadata (CCSWeaponBaseVData, CSWeaponType)
// ============================================================================

namespace cs2::sdk {

#define PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

// ============================================================================
// SECTION 1: SPATIAL QUERY & RAY TRACING DATA STRUCTURES
// Discovered in client.dll & vphysics2.dll:
// - CGameTraceManager Global Pointer: client.dll + 0x02229020 (VA: 0x182229020)
// - Static BSS Instance:              client.dll + 0x0254cf30 (VA: 0x18254cf30)
// - TraceShape Function RVA:          client.dll + 0x00a1ae30 (VA: 0x180a1ae30)
// - ClipRayToEntity Function RVA:     client.dll + 0x00a1b8a0 (VA: 0x180a1b8a0)
// ============================================================================

/**
 * @brief 64-byte Geometric Query Ray & Swept Extent Volume.
 * Provenance: client.dll @ 0x180a1ae30 / vphysics2.dll @ 0x1802544a0.
 */
struct alignas(16) Ray_t {
    Vector3       m_vStart;         // +0x00: Ray segment trajectory origin
    Vector3       m_vEnd;           // +0x0C: Ray segment trajectory terminus
    Vector3       m_vOffset;        // +0x18: Swept centroid displacement vector
    Vector3       m_vExtent;        // +0x24: Half-extent bounding box dimensions
    bool          m_bIsRay;         // +0x30: True if line ray (extent == 0), false if swept volume
    bool          m_bHasExtents;    // +0x31: True if swept volume possesses non-zero bounds
    PAD(0x06, m_pad32);             // +0x32: Alignment padding
    std::uint64_t m_nMask;          // +0x38: 64-bit spatial collision mask (e.g. MASK_SHOT = 0x46004003)

    Ray_t() noexcept
        : m_vStart{}, m_vEnd{}, m_vOffset{}, m_vExtent{}
        , m_bIsRay{ true }, m_bHasExtents{ false }, m_pad32{}
        , m_nMask{ 0x46004003ULL } {}

    Ray_t(const Vector3& start, const Vector3& end, std::uint64_t mask = 0x46004003ULL) noexcept
        : m_vStart{ start }, m_vEnd{ end }, m_vOffset{}, m_vExtent{}
        , m_bIsRay{ true }, m_bHasExtents{ false }, m_pad32{}
        , m_nMask{ mask } {}

    Ray_t(const Vector3& start, const Vector3& end, const Vector3& mins, const Vector3& maxs, std::uint64_t mask = 0x46004003ULL) noexcept
        : m_vStart{ start }, m_vEnd{ end }
        , m_vOffset{ (mins.x + maxs.x) * 0.5f, (mins.y + maxs.y) * 0.5f, (mins.z + maxs.z) * 0.5f }
        , m_vExtent{ (maxs.x - mins.x) * 0.5f, (maxs.y - mins.y) * 0.5f, (maxs.z - mins.z) * 0.5f }
        , m_bIsRay{ false }, m_bHasExtents{ true }, m_pad32{}
        , m_nMask{ mask } {}
};
static_assert(sizeof(Ray_t) == 0x40, "Ray_t size must be exactly 0x40 (64 bytes)");
static_assert(offsetof(Ray_t, m_vStart)      == 0x00, "Ray_t::m_vStart offset mismatch");
static_assert(offsetof(Ray_t, m_vEnd)        == 0x0C, "Ray_t::m_vEnd offset mismatch");
static_assert(offsetof(Ray_t, m_vOffset)     == 0x18, "Ray_t::m_vOffset offset mismatch");
static_assert(offsetof(Ray_t, m_vExtent)     == 0x24, "Ray_t::m_vExtent offset mismatch");
static_assert(offsetof(Ray_t, m_bIsRay)      == 0x30, "Ray_t::m_bIsRay offset mismatch");
static_assert(offsetof(Ray_t, m_bHasExtents) == 0x31, "Ray_t::m_bHasExtents offset mismatch");
static_assert(offsetof(Ray_t, m_nMask)       == 0x38, "Ray_t::m_nMask offset mismatch");

/**
 * @brief 72-byte Entity & Collision Layer Query Filter.
 * Provenance: client.dll @ 0x180205cb0 (alloc size 0x48) / VTable: 0x181ad4838.
 */
class CTraceFilter {
public:
    virtual ~CTraceFilter() = default;                                                 // Slot 0 (0x180205cb0)
    virtual bool ShouldHitEntity(void* pEntity, std::uint64_t nCollisionMask) = 0;   // Slot 1 (0x18022d900)

    std::uint64_t m_nCollisionMask{ 0xFFFFFFFFFFFFFFFFULL }; // +0x08: 64-bit collision layer mask
    std::uint64_t m_v1{ 0 };                                 // +0x10: Filter evaluation flags
    std::uint64_t m_v2{ 0 };                                 // +0x18: Extended attribute mask
    std::uint32_t m_hEntityToIgnore[2]{ 0xFFFFFFFF, 0 };     // +0x20: Primary entity handle to bypass
    std::uint32_t m_hOwnerToIgnore[2]{ 0xFFFFFFFF, 0 };      // +0x28: Hierarchy owner handle to bypass
    std::uint16_t m_nCollisionGroup[2]{ 0, 0 };              // +0x30: Collision group IDs
    std::uint32_t m_nHitGroup{ 0 };                          // +0x34: Hitgroup filter
    std::uint32_t m_nLayerMask{ 0 };                         // +0x38: Secondary collision layer mask
    std::uint32_t m_nFilterLayer{ 0x100FFFF };               // +0x3C: Layer discriminator (0x100FFFF default)
    std::uint8_t  m_nFilterMode{ 3 };                        // +0x40: Filter execution mode
    std::uint8_t  m_nFlags{ 0x49 };                          // +0x41: Internal status bitfield
    PAD(0x06, m_pad42);                                      // +0x42: Tail alignment padding
};
static_assert(sizeof(CTraceFilter) == 0x48, "CTraceFilter size must be exactly 0x48 (72 bytes)");
static_assert(offsetof(CTraceFilter, m_nCollisionMask)  == 0x08, "CTraceFilter::m_nCollisionMask offset mismatch");
static_assert(offsetof(CTraceFilter, m_v1)             == 0x10, "CTraceFilter::m_v1 offset mismatch");
static_assert(offsetof(CTraceFilter, m_v2)             == 0x18, "CTraceFilter::m_v2 offset mismatch");
static_assert(offsetof(CTraceFilter, m_hEntityToIgnore) == 0x20, "CTraceFilter::m_hEntityToIgnore offset mismatch");
static_assert(offsetof(CTraceFilter, m_hOwnerToIgnore)  == 0x28, "CTraceFilter::m_hOwnerToIgnore offset mismatch");
static_assert(offsetof(CTraceFilter, m_nCollisionGroup) == 0x30, "CTraceFilter::m_nCollisionGroup offset mismatch");
static_assert(offsetof(CTraceFilter, m_nHitGroup)       == 0x34, "CTraceFilter::m_nHitGroup offset mismatch");
static_assert(offsetof(CTraceFilter, m_nLayerMask)      == 0x38, "CTraceFilter::m_nLayerMask offset mismatch");
static_assert(offsetof(CTraceFilter, m_nFilterLayer)    == 0x3C, "CTraceFilter::m_nFilterLayer offset mismatch");
static_assert(offsetof(CTraceFilter, m_nFilterMode)     == 0x40, "CTraceFilter::m_nFilterMode offset mismatch");
static_assert(offsetof(CTraceFilter, m_nFlags)          == 0x41, "CTraceFilter::m_nFlags offset mismatch");

/**
 * @brief 192-byte Authoritative Ray Intersection Result Block.
 * Provenance: client.dll @ 0x18177b560 (Init function) / 0x180a1ae30.
 */
struct alignas(16) GameTrace_t {
    void*         m_pSurface{ nullptr };       // +0x00: Pointer to physical SurfaceData_t descriptor
    C_BaseEntity* m_pHitEntity{ nullptr };     // +0x08: Intersected C_BaseEntity pointer (nullptr if world / air)
    void*         m_pHitboxData{ nullptr };    // +0x10: Target hitbox runtime descriptor pointer
    std::uint64_t m_nContents{ 0 };            // +0x18: Point contents flags at hit position
    std::uint64_t m_nHitBoxIndex{ 0 };         // +0x20: Hitbox bone index
    PAD(0x08, m_pad28);                        // +0x28
    Vector3       m_vStartPos{};               // +0x30: Trajectory origin position
    PAD(0x04, m_pad3c);                        // +0x3C
    Vector3       m_vEndPos{};                 // +0x40: Trajectory terminus position
    PAD(0x04, m_pad4c);                        // +0x4C
    std::uint8_t  m_rubikonBuffer[40]{};       // +0x50: Internal Rubikon sweep buffer (0x28 bytes)
    Vector3       m_vRayStart{};               // +0x78: Exact evaluated ray entry vector
    Vector3       m_vRayEnd{};                 // +0x84: Exact evaluated ray exit vector
    PAD(0x08, m_pad90);                        // +0x90
    PAD(0x04, m_pad98);                        // +0x98
    Vector3       m_vHitNormal{};              // +0x9C: Surface unit normal vector at intersection
    std::uint32_t m_nSpatialVolumeIndex{ 0 };  // +0xA8: Rubikon spatial subdivision cell / hitgroup ID
    float         m_flFraction{ 1.0f };        // +0xAC: Intersection fraction [0.0..1.0] along ray (1.0 = miss)
    std::uint64_t m_nHitFlags{ 0 };            // +0xB0: Intersection status flags
    std::uint16_t m_nSurfaceFlags{ 0 };        // +0xB8: Physical material surface descriptor flags
    std::uint8_t  m_bStartSolid{ 0 };          // +0xBA: True if ray started inside a solid geometry volume
    PAD(0x05, m_padbb);                        // +0xBB

    [[nodiscard]] constexpr bool DidHit() const noexcept {
        return m_flFraction < 1.0f || m_bStartSolid;
    }

    [[nodiscard]] constexpr bool IsVisible() const noexcept {
        return m_flFraction > 0.97f && !m_bStartSolid;
    }
};
static_assert(sizeof(GameTrace_t) == 0xC0, "GameTrace_t size must be exactly 0xC0 (192 bytes)");
static_assert(offsetof(GameTrace_t, m_pSurface)            == 0x00, "GameTrace_t::m_pSurface offset mismatch");
static_assert(offsetof(GameTrace_t, m_pHitEntity)          == 0x08, "GameTrace_t::m_pHitEntity offset mismatch");
static_assert(offsetof(GameTrace_t, m_pHitboxData)         == 0x10, "GameTrace_t::m_pHitboxData offset mismatch");
static_assert(offsetof(GameTrace_t, m_nContents)           == 0x18, "GameTrace_t::m_nContents offset mismatch");
static_assert(offsetof(GameTrace_t, m_nHitBoxIndex)        == 0x20, "GameTrace_t::m_nHitBoxIndex offset mismatch");
static_assert(offsetof(GameTrace_t, m_vStartPos)           == 0x30, "GameTrace_t::m_vStartPos offset mismatch");
static_assert(offsetof(GameTrace_t, m_vEndPos)             == 0x40, "GameTrace_t::m_vEndPos offset mismatch");
static_assert(offsetof(GameTrace_t, m_vRayStart)           == 0x78, "GameTrace_t::m_vRayStart offset mismatch");
static_assert(offsetof(GameTrace_t, m_vRayEnd)             == 0x84, "GameTrace_t::m_vRayEnd offset mismatch");
static_assert(offsetof(GameTrace_t, m_vHitNormal)          == 0x9C, "GameTrace_t::m_vHitNormal offset mismatch");
static_assert(offsetof(GameTrace_t, m_nSpatialVolumeIndex) == 0xA8, "GameTrace_t::m_nSpatialVolumeIndex offset mismatch");
static_assert(offsetof(GameTrace_t, m_flFraction)          == 0xAC, "GameTrace_t::m_flFraction offset mismatch");
static_assert(offsetof(GameTrace_t, m_nHitFlags)           == 0xB0, "GameTrace_t::m_nHitFlags offset mismatch");
static_assert(offsetof(GameTrace_t, m_nSurfaceFlags)       == 0xB8, "GameTrace_t::m_nSurfaceFlags offset mismatch");
static_assert(offsetof(GameTrace_t, m_bStartSolid)         == 0xBA, "GameTrace_t::m_bStartSolid offset mismatch");

/**
 * @brief Spatial Query Master Interface (CGameTraceManager / IEngineTrace).
 * Global Instance in client.dll: +0x02229020 (VA: 0x182229020).
 */
class CGameTraceManager {
public:
    virtual ~CGameTraceManager() = default;

    // Slot 1: ClipRayToEntity narrowphase test (RVA client.dll + 0x00a1b8a0)
    virtual bool ClipRayToEntity(
        const Ray_t* pRay,
        const Vector3* pStart,
        const Vector3* pEnd,
        C_BaseEntity* pEntity,
        CTraceFilter* pFilter,
        GameTrace_t* pTrace,
        std::int32_t nTraceFlags = 0
    ) = 0;

    PAD(0x10, m_padVirtuals);

    // Slot 5: TraceShape primary scene sweep (RVA client.dll + 0x00a1ae30)
    virtual bool TraceShape(
        const Ray_t* pRay,
        const Vector3* pStart,
        const Vector3* pEnd,
        CTraceFilter* pFilter,
        GameTrace_t* pTrace
    ) = 0;
};

// ============================================================================
// SECTION 2: VOLUMETRIC SMOKE OPTICAL OCCLUSION
// Discovered in client.dll: RVA 0x007ce470 (VA: 0x1807ce470)
// Prologue: 48 8B C4 55 53 56 41 55 41 57 48 8D 68 A8 48 81 EC 30 01 00 00
// Evaluates 32x32x32 Morton Z-Order voxel density with 0.20f density gate threshold.
// ============================================================================
using FnLineOfSightThroughSmoke = float (__fastcall*)(
    const Vector3& vStart,            // RCX: Ray trajectory origin
    const Vector3& vEnd,              // RDX: Ray trajectory terminus
    Vector3* pOutIntersection         // R8:  Optional blocking point receiver
);

// ============================================================================
// SECTION 3: GAME EVENT SUBSYSTEM (IGameEventManager2, IGameEvent)
// Interface Token: "GAMEEVENTSMANAGER002"
// - Global Instance Pointer: client.dll + 0x2228E40 (VA: 0x182228E40)
// - Singleton Instance:      client.dll + 0x22292C0 (VA: 0x1822292C0)
// - CGameEventManager VTable: client.dll + 0x1C337A8 (VA: 0x181C337A8)
// - IGameEvent VTable:        client.dll + 0x1C334F0 (VA: 0x181C334F0)
// ============================================================================

class CSVCMsg_GameEvent;

/**
 * @brief Key-Value Game Event Deserialization Interface.
 * VTable: client.dll + 0x1C334F0 (18 virtual slots).
 */
class IGameEvent {
public:
    virtual ~IGameEvent() = 0;                                                                           // Slot 0  (0x1809C8BD0)
    [[nodiscard]] virtual const char* GetName() const = 0;                                               // Slot 1  (0x1809E8C80)
    [[nodiscard]] virtual int GetID() const = 0;                                                         // Slot 2  (0x1809E7F40)
    [[nodiscard]] virtual bool IsReliable() const = 0;                                                   // Slot 3  (0x1809F2730)
    [[nodiscard]] virtual bool IsLocal() const = 0;                                                      // Slot 4  (0x1809F2640)
    [[nodiscard]] virtual bool IsEmpty(const char* pszKeyName = nullptr) = 0;                            // Slot 5  (0x1809F25A0)
    [[nodiscard]] virtual bool GetBool(const char* pszKeyName = nullptr, bool bDefaultValue = false) = 0;// Slot 6  (0x1809E4C90)
    [[nodiscard]] virtual int GetInt(const char* pszKeyName = nullptr, int nDefaultValue = 0) = 0;      // Slot 7  (0x1809E7F50)
    [[nodiscard]] virtual uint64_t GetUint64(const char* pszKeyName = nullptr, uint64_t nDefault = 0) = 0; // Slot 8  (0x1809EA5F0)
    [[nodiscard]] virtual float GetFloat(const char* pszKeyName = nullptr, float flDefaultValue = 0.0f) = 0; // Slot 9  (0x1809E7B70)
    [[nodiscard]] virtual const char* GetString(const char* pszKeyName = nullptr, const char* pszDefault = "") = 0; // Slot 10 (0x1809EA380)
    [[nodiscard]] virtual const void* GetPtr(const char* pszKeyName = nullptr) = 0;                      // Slot 11 (0x1809E9DB0)
    virtual void SetBool(const char* pszKeyName, bool bValue) = 0;                                       // Slot 12 (0x1809F1C70)
    virtual void SetInt(const char* pszKeyName, int nValue) = 0;                                         // Slot 13 (0x1809E5500)
    virtual void SetUint64(const char* pszKeyName, uint64_t nValue) = 0;                                 // Slot 14 (0x1809F2800)
    virtual void SetFloat(const char* pszKeyName, float flValue) = 0;                                    // Slot 15 (0x1809F2000)
    virtual void SetString(const char* pszKeyName, const char* pszValue) = 0;                            // Slot 16 (0x1809E9090)
    virtual void SetPtr(const char* pszKeyName, const void* pValue) = 0;                                 // Slot 17 (0x1809F24F0)
};

/**
 * @brief Game Event Listener Callback Base Interface.
 */
class IGameEventListener2 {
public:
    virtual ~IGameEventListener2() = default;
    virtual void FireGameEvent(IGameEvent* pEvent) = 0;
    [[nodiscard]] virtual int GetEventDebugID() { return 42; }
};

/**
 * @brief Game Events Master Manager Interface ("GAMEEVENTSMANAGER002").
 * VTable: client.dll + 0x1C337A8 (16 virtual slots).
 */
class IGameEventManager2 {
public:
    virtual ~IGameEventManager2() = 0;                                                                   // Slot 0  (0x1809C8C20)
    virtual int LoadEventsFromFile(const char* pszFilename, bool bSearchPaths = true) = 0;               // Slot 1  (0x1809F68B0)
    virtual void Reset() = 0;                                                                            // Slot 2  (0x180A055E0)
    virtual bool AddListener(IGameEventListener2* pListener, const char* pszName, bool bServer = false) = 0; // Slot 3  (0x1809CAB60)
    virtual bool FindListener(IGameEventListener2* pListener, const char* pszName) = 0;                  // Slot 4  (0x1809E2A20)
    virtual void RemoveListener(IGameEventListener2* pListener) = 0;                                     // Slot 5  (0x180A02370)
    virtual IGameEvent* CreateEvent(const char* pszName, bool bForce = false, int* pCookie = nullptr) = 0;// Slot 6  (0x1809D5440)
    virtual bool FireEvent(IGameEvent* pEvent, bool bDontBroadcast = false) = 0;                         // Slot 7  (0x1809E3500)
    virtual bool FireEventClientSide(IGameEvent* pEvent) = 0;                                            // Slot 8  (0x1809E3790)
    virtual IGameEvent* DuplicateEvent(IGameEvent* pEvent) = 0;                                          // Slot 9  (0x1809D9990)
    virtual void FreeEvent(IGameEvent* pEvent) = 0;                                                      // Slot 10 (0x1809E3FE0)
    virtual bool SerializeEvent(IGameEvent* pEvent, CSVCMsg_GameEvent* pMsg) = 0;                        // Slot 11 (0x180A0F840)
    virtual IGameEvent* UnserializeEvent(const CSVCMsg_GameEvent* pMsg) = 0;                             // Slot 12 (0x180A1F290)
    virtual int LookupEventId(const char* pszName) = 0;                                                  // Slot 13 (0x1809F79C0)
    virtual bool SerializeKeyValues(IGameEvent* pEvent, void* pBuffer) = 0;                              // Slot 14 (0x1809FC580)
    virtual bool HasEvent(const char* pszName) = 0;                                                      // Slot 15 (0x1809CF2E0)
};

// ============================================================================
// SECTION 4: AUDIO SUBSYSTEM (SoundOpSystem001)
// Interface Token: "SoundOpSystem001" (soundsystem.dll + 0x4AB140)
// Dispatcher: CSoundServices::DispatchSoundEvent (client.dll + 0xB4FB20)
// ============================================================================

using SoundEventGuid_t = std::uint64_t;

/**
 * @brief Sound Emission Dispatcher Payload (80 bytes / 0x80).
 */
struct StartSoundEventInfo_t {
    float         m_vecOrigin[3];       // +0x00: Spatial emission origin
    float         m_angAngles[3];       // +0x0C: Directional orientation angles
    float         m_flVolume;           // +0x18: Emission acoustic gain [0.0..1.0]
    float         m_flPitch;            // +0x1C: Frequency pitch scalar
    CEntityHandle m_hEntity;            // +0x20: Associated emitter entity handle
    std::int32_t  m_nSoundSource;       // +0x24: Acoustic source channel
    std::uint32_t m_nFlags;             // +0x28: Emission bitflags
    PAD(0x04, m_pad2C);                 // +0x2C
    float         m_matSpatial[12];     // +0x30: 3x4 affine spatial transformation matrix
    PAD(0x20, m_pad60);                 // +0x60
};
static_assert(sizeof(StartSoundEventInfo_t) == 0x80, "StartSoundEventInfo_t size must be 0x80 bytes");

// ============================================================================
// SECTION 5: NETWORK TELEMETRY (INetChannelInfo)
// Implementation: CNetChan (networksystem.dll VTable: 0x180235890)
// ============================================================================

enum FlowDirection : int {
    FLOW_OUTGOING = 0,
    FLOW_INCOMING = 1,
    MAX_FLOWS     = 2
};

/**
 * @brief Source 2 Network Performance Telemetry Interface.
 * VTable: networksystem.dll + 0x235890 (32 slots).
 */
class INetChannelInfo {
public:
    virtual ~INetChannelInfo() = default;

    virtual const char*   GetName() const = 0;                                          // Slot 0  (0x0bd020)
    virtual const char*   GetAddress() const = 0;                                       // Slot 1  (0x0bd040)
    virtual const void*   GetRemoteAddress() const = 0;                                 // Slot 2  (0x0c3e10)
    virtual float         GetTime() const = 0;                                          // Slot 3  (0x0c5ed0)
    virtual float         GetConnectionTime() const = 0;                                // Slot 4  (0x0c3df0)
    virtual int32_t       GetBufferSize() const = 0;                                    // Slot 5  (0x0c3d50)
    virtual bool          IsLoopback() const = 0;                                       // Slot 6  (0x0bb550)
    virtual bool          IsTimingOut() const = 0;                                      // Slot 7  (0x0bb540)
    virtual bool          IsPlayback() const = 0;                                       // Slot 8  (0x0c3e60)
    virtual bool          IsSplitScreenUser() const = 0;                                // Slot 9  (0x0c6ef0)
    virtual float         GetLatency(int flow) const = 0;                               // Slot 10 (0x0c5c10)
    virtual float         GetAvgLatency(int flow) const = 0;                            // Slot 11 (0x0c5cd0)
    virtual float         GetAvgLoss(int flow) const = 0;                               // Slot 12 (0x0c5e70)
    virtual float         GetAvgChoke(int flow) const = 0;                              // Slot 13 (0x0c5e90)
    virtual float         GetAvgData(int flow) const = 0;                               // Slot 14 (0x0c5eb0)
    virtual float         GetAvgPackets(int flow) const = 0;                            // Slot 15 (0x0c5a40)
    virtual float         GetAvgPacketsDropped(int flow) const = 0;                     // Slot 16 (0x0c5ae0)
    virtual uint64_t      GetTotalData(int flow) const = 0;                             // Slot 17 (0x0c5b80)
    virtual int32_t       GetTotalPackets(int flow) const = 0;                          // Slot 18 (0x0c5ba0)
    virtual int32_t       GetSequenceNr(int flow) const = 0;                            // Slot 19 (0x0c5bc0)
    virtual float         GetTimeoutSeconds() const = 0;                                // Slot 20 (0x0c5a10)
    virtual void          GetStreamProgress(int* r, int* s, int* t) const = 0;          // Slot 21 (0x0c5be0)
    virtual float         GetRemoteFramerate() const = 0;                               // Slot 22 (0x0c5a00)
    virtual float         GetRemoteFramerateStdDev() const = 0;                         // Slot 23 (0x0c3ea0)
    virtual float         GetRemoteFramerateCompatibility() const = 0;                  // Slot 24 (0x0bd010)
    virtual void*         GetNetStatsOut(int flow) = 0;                                 // Slot 25 (0x0c5d10)
    virtual const void*   GetNetStatsIn(int flow) const = 0;                            // Slot 26 (0x0c5d40)
    virtual void          CopyNetStats(void* dest) const = 0;                           // Slot 27 (0x0c5d60)
    virtual void          SetInterpolationAmount(float fl1, float fl2) = 0;             // Slot 28 (0x0c65c0)
    virtual void          SetCommandInterpolationAmount(int frame, int flow) = 0;       // Slot 29 (0x0c6240)
    virtual void          SetTrace(bool bEnable) = 0;                                   // Slot 30 (0x0c3f20)
    virtual int32_t       IsValidPacket(int flow, int frame, float* pflLatency = nullptr) const = 0; // Slot 31 (0x0c4180)
};

// ============================================================================
// SECTION 6: WEAPON CONFIGURATION METADATA (CCSWeaponBaseVData)
// Schema Class in client.dll: 2,208 bytes (0x8A0)
// Base: CBasePlayerWeaponVData (1,312 bytes / 0x520)
// ============================================================================

enum class CSWeaponType : std::int32_t {
    WEAPONTYPE_KNIFE         = 0,
    WEAPONTYPE_PISTOL        = 1,
    WEAPONTYPE_SUBMACHINEGUN = 2,
    WEAPONTYPE_RIFLE         = 3,
    WEAPONTYPE_SHOTGUN       = 4,
    WEAPONTYPE_SNIPER_RIFLE  = 5,
    WEAPONTYPE_MACHINEGUN    = 6,
    WEAPONTYPE_C4            = 7,
    WEAPONTYPE_TASER         = 8,
    WEAPONTYPE_GRENADE       = 9,
    WEAPONTYPE_EQUIPMENT     = 10,
    WEAPONTYPE_STACKABLEITEM = 11,
    WEAPONTYPE_UNKNOWN       = 12
};

struct CBasePlayerWeaponVData {
    PAD(0x520, m_padBaseWeaponVData);
};
static_assert(sizeof(CBasePlayerWeaponVData) == 0x520, "CBasePlayerWeaponVData size mismatch");

struct CCSWeaponBaseVData : public CBasePlayerWeaponVData {
    CSWeaponType m_WeaponType;                           // +0x0520 (1312)
    std::uint32_t m_WeaponCategory;                      // +0x0524 (1316)
    char m_szAnimSkeleton[224];                          // +0x0528 (1320)
    Vector3 m_vecMuzzlePos0;                             // +0x0608 (1544)
    Vector3 m_vecMuzzlePos1;                             // +0x0614 (1556)
    char m_szTracerParticle[224];                        // +0x0620 (1568)
    std::uint32_t m_GearSlot;                            // +0x0700 (1792)
    std::uint32_t m_GearSlotPosition;                    // +0x0704 (1796)
    std::uint32_t m_DefaultLoadoutSlot;                  // +0x0708 (1800)
    std::int32_t m_nPrice;                               // +0x070c (1804)
    std::int32_t m_nKillAward;                           // +0x0710 (1808)
    std::int32_t m_nPrimaryReserveAmmoMax;               // +0x0714 (1812)
    std::int32_t m_nSecondaryReserveAmmoMax;             // +0x0718 (1816)
    bool m_bMeleeWeapon;                                 // +0x071c (1820)
    bool m_bHasBurstMode;                                // +0x071d (1821)
    bool m_bIsRevolver;                                  // +0x071e (1822)
    bool m_bCannotShootUnderwater;                       // +0x071f (1823)
    const char* m_szName;                                // +0x0720 (1824)
    std::uint32_t m_eSilencerType;                       // +0x0728 (1832)
    bool m_bShowCrosshair;                               // +0x072c (1836)
    bool m_bIsFullAuto;                                  // +0x072d (1837)
    PAD(0x02, m_pad72e);                                 // +0x072e (1838)
    std::int32_t m_nNumBullets;                          // +0x0730 (1840)
    bool m_bReloadsSingleShells;                         // +0x0734 (1844)
    PAD(0x03, m_pad735);                                 // +0x0735 (1845)
    float m_flCycleTime;                                 // +0x0738 (1848)
    PAD(0x04, m_pad73c);                                 // +0x073c (1852)
    float m_flCycleTimeWhenInBurstMode;                  // +0x0740 (1856)
    float m_flTimeBetweenBurstShots;                     // +0x0744 (1860)
    float m_flMaxSpeed;                                  // +0x0748 (1864)
    PAD(0x04, m_pad74c);                                 // +0x074c (1868)
    float m_flSpread;                                    // +0x0750 (1872)
    PAD(0x04, m_pad754);                                 // +0x0754 (1876)
    float m_flInaccuracyCrouch;                          // +0x0758 (1880)
    PAD(0x04, m_pad75c);                                 // +0x075c (1884)
    float m_flInaccuracyStand;                           // +0x0760 (1888)
    PAD(0x04, m_pad764);                                 // +0x0764 (1892)
    float m_flInaccuracyJump;                            // +0x0768 (1896)
    PAD(0x04, m_pad76c);                                 // +0x076c (1900)
    float m_flInaccuracyLand;                            // +0x0770 (1904)
    PAD(0x04, m_pad774);                                 // +0x0774 (1908)
    float m_flInaccuracyLadder;                          // +0x0778 (1912)
    PAD(0x04, m_pad77c);                                 // +0x077c (1916)
    float m_flInaccuracyFire;                            // +0x0780 (1920)
    PAD(0x04, m_pad784);                                 // +0x0784 (1924)
    float m_flInaccuracyMove;                            // +0x0788 (1928)
    PAD(0x04, m_pad78c);                                 // +0x078c (1932)
    float m_flRecoilAngle;                               // +0x0790 (1936)
    PAD(0x04, m_pad794);                                 // +0x0794 (1940)
    float m_flRecoilAngleVariance;                       // +0x0798 (1944)
    PAD(0x04, m_pad79c);                                 // +0x079c (1948)
    float m_flRecoilMagnitude;                           // +0x07a0 (1952)
    PAD(0x04, m_pad7a4);                                 // +0x07a4 (1956)
    float m_flRecoilMagnitudeVariance;                   // +0x07a8 (1960)
    PAD(0x04, m_pad7ac);                                 // +0x07ac (1964)
    std::int64_t m_nTracerFrequency;                     // +0x07b0 (1968)
    float m_flInaccuracyJumpInitial;                     // +0x07b8 (1976)
    float m_flInaccuracyJumpApex;                        // +0x07bc (1980)
    float m_flInaccuracyReload;                          // +0x07c0 (1984)
    float m_flDeployDuration;                            // +0x07c4 (1988)
    float m_flDisallowAttackAfterReloadStartDuration;    // +0x07c8 (1992)
    std::int32_t m_nBurstShotCount;                      // +0x07cc (1996)
    bool m_bAllowBurstHolster;                           // +0x07d0 (2000)
    PAD(0x03, m_pad7d1);                                 // +0x07d1 (2001)
    std::int32_t m_nRecoilSeed;                          // +0x07d4 (2004)
    std::int32_t m_nSpreadSeed;                          // +0x07d8 (2008)
    float m_flAttackMovespeedFactor;                     // +0x07dc (2012)
    float m_flInaccuracyPitchShift;                      // +0x07e0 (2016)
    float m_flInaccuracyAltSoundThreshold;               // +0x07e4 (2020)
    const char* m_szUseRadioSubtitle;                    // +0x07e8 (2024)
    bool m_bUnzoomsAfterShot;                            // +0x07f0 (2032)
    bool m_bHideViewModelWhenZoomed;                     // +0x07f1 (2033)
    PAD(0x02, m_pad7f2);                                 // +0x07f2 (2034)
    std::int32_t m_nZoomLevels;                          // +0x07f4 (2036)
    std::int32_t m_nZoomFOV1;                            // +0x07f8 (2040)
    std::int32_t m_nZoomFOV2;                            // +0x07fc (2044)
    float m_flZoomTime0;                                 // +0x0800 (2048)
    float m_flZoomTime1;                                 // +0x0804 (2052)
    float m_flZoomTime2;                                 // +0x0808 (2056)
    float m_flIronSightPullUpSpeed;                      // +0x080c (2060)
    float m_flIronSightPutDownSpeed;                     // +0x0810 (2064)
    float m_flIronSightFOV;                              // +0x0814 (2068)
    float m_flIronSightPivotForward;                     // +0x0818 (2072)
    float m_flIronSightLooseness;                        // +0x081c (2076)
    std::int32_t m_nDamage;                              // +0x0820 (2080) [Base ballistic power / m_iDamage]
    float m_flHeadshotMultiplier;                        // +0x0824 (2084)
    float m_flArmorRatio;                                // +0x0828 (2088) [Armor penetration efficiency]
    float m_flPenetration;                               // +0x082c (2092) [Penetration depth]
    float m_flRange;                                     // +0x0830 (2096) [Effective range]
    float m_flRangeModifier;                             // +0x0834 (2100) [Distance attenuation]
    float m_flFlinchVelocityModifierLarge;               // +0x0838 (2104)
    float m_flFlinchVelocityModifierSmall;               // +0x083c (2108)
    float m_flRecoveryTimeCrouch;                        // +0x0840 (2112)
    float m_flRecoveryTimeStand;                         // +0x0844 (2116)
    float m_flRecoveryTimeCrouchFinal;                   // +0x0848 (2120)
    float m_flRecoveryTimeStandFinal;                    // +0x084c (2124)
    std::int32_t m_nRecoveryTransitionStartBullet;       // +0x0850 (2128)
    std::int32_t m_nRecoveryTransitionEndBullet;         // +0x0854 (2132)
    float m_flThrowVelocity;                             // +0x0858 (2136)
    Vector3 m_vSmokeColor;                               // +0x085c (2140)
    char m_szAnimClass[56];                              // +0x0868 (2152)
};

static_assert(sizeof(CCSWeaponBaseVData) == 0x8A0, "CCSWeaponBaseVData size must be 2208 bytes (0x8A0)");
static_assert(offsetof(CCSWeaponBaseVData, m_WeaponType) == 0x520, "m_WeaponType offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_szName) == 0x720, "m_szName offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_flCycleTime) == 0x738, "m_flCycleTime offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_flSpread) == 0x750, "m_flSpread offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_flInaccuracyCrouch) == 0x758, "m_flInaccuracyCrouch offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_flInaccuracyStand) == 0x760, "m_flInaccuracyStand offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_flInaccuracyJump) == 0x768, "m_flInaccuracyJump offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_flInaccuracyMove) == 0x788, "m_flInaccuracyMove offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_nDamage) == 0x820, "m_nDamage offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_flArmorRatio) == 0x828, "m_flArmorRatio offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_flPenetration) == 0x82c, "m_flPenetration offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_flRange) == 0x830, "m_flRange offset mismatch");
static_assert(offsetof(CCSWeaponBaseVData, m_flRangeModifier) == 0x834, "m_flRangeModifier offset mismatch");

#undef PAD

} // namespace cs2::sdk
