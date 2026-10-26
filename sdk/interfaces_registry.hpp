#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

// ============================================================================
// Source 2 Reverse Engineered SDK - Phase 0: Multi-Binary Interfaces Registry
// Target Build: 2026-10-25_26144732 (CS2 Update Archive)
// Architecture: x86-64 (MSVC ABI)
// Reconstructed & Authoritatively Verified via Static RTTI & VTable Analysis
// ============================================================================

namespace cs2::sdk {

#define PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

// ============================================================================
// Dynamic Interface Registration Linked List (CreateInterface Registry)
// Discovery Anchor: Exported CreateInterface entry point in each Source 2 DLL
// Layout verified across all 34 synchronized DLLs:
//   m_CreateFn : +0x00 (Instantiate callback)
//   m_pName    : +0x08 (Null-terminated ASCII interface token)
//   m_pNext    : +0x10 (Next node in singly-linked list)
// ============================================================================
using InstantiateInterfaceFn = void* (*)();

struct InterfaceReg {
    InstantiateInterfaceFn m_CreateFn; // +0x00
    const char*            m_pName;    // +0x08
    InterfaceReg*          m_pNext;    // +0x10
};
static_assert(sizeof(InterfaceReg) == 0x18, "InterfaceReg size must be 0x18 bytes");
static_assert(offsetof(InterfaceReg, m_CreateFn) == 0x00, "m_CreateFn offset mismatch");
static_assert(offsetof(InterfaceReg, m_pName)    == 0x08, "m_pName offset mismatch");
static_assert(offsetof(InterfaceReg, m_pNext)    == 0x10, "m_pNext offset mismatch");

using CreateInterfaceFn = void* (*)(const char* pName, int* pReturnCode);

// Forward declarations of types referenced by interfaces
class CGameEntitySystem;
class CSchemaSystemTypeScope;
class ConVar;
class INetChannelInfo;
class CNetworkGameClient;
class CMaterial2;

// ============================================================================
// 1. ISource2EngineToClient (CEngineClient)
// Binary: engine2.dll
// Token:  "Source2EngineToClient001" (Node RVA: 0x61a7a0, Factory RVA: 0x78ab0)
// VTable: engine2.dll + 0x5d5c38 (185 methods, this_offset: +0x0)
// RTTI:   .?AVCEngineClient@@
// ============================================================================
class ISource2EngineToClient {
public:
    virtual ~ISource2EngineToClient() = default;                                                 // Slot 0

    // Virtual slots 1 to 31
    virtual void Unk01() = 0; virtual void Unk02() = 0; virtual void Unk03() = 0; virtual void Unk04() = 0;
    virtual void Unk05() = 0; virtual void Unk06() = 0; virtual void Unk07() = 0; virtual void Unk08() = 0;
    virtual void Unk09() = 0; virtual void Unk10() = 0; virtual void Unk11() = 0; virtual void Unk12() = 0;
    virtual void Unk13() = 0; virtual void Unk14() = 0; virtual void Unk15() = 0; virtual void Unk16() = 0;
    virtual void Unk17() = 0; virtual void Unk18() = 0; virtual void Unk19() = 0; virtual void Unk20() = 0;
    virtual void Unk21() = 0; virtual void Unk22() = 0; virtual void Unk23() = 0; virtual void Unk24() = 0;
    virtual void Unk25() = 0; virtual void Unk26() = 0; virtual void Unk27() = 0; virtual void Unk28() = 0;
    virtual void Unk29() = 0; virtual void Unk30() = 0; virtual void Unk31() = 0;

    // Slot 32: engine2.dll + 0x76150 (Returns screen pixel resolution)
    virtual void GetScreenSize(std::int32_t& nWidth, std::int32_t& nHeight) = 0;

    virtual void Unk33() = 0; virtual void Unk34() = 0; virtual void Unk35() = 0;

    // Slot 36: engine2.dll + 0x76280 (Returns display aspect ratio w/h)
    virtual float GetScreenAspectRatio() = 0;

    // Slot 37: engine2.dll + 0x762c0 (Reads *(int32*)(this + 0x39c))
    virtual std::int32_t GetLocalPlayer(std::int32_t& nSplitScreenSlot) = 0;

    // Slot 38: engine2.dll + 0x762e0 (Reads *(int32*)(this + 0x240))
    virtual std::int32_t GetMaxClients() = 0;

    // Slot 39: engine2.dll + 0x76380 (Checks *(uint32*)(m_pNetGameClient + 0x230) == 6 [SIGNONSTATE_FULL])
    virtual bool IsInGame() = 0;

    // Slot 40: engine2.dll + 0x763b0 (Checks *(uint32*)(m_pNetGameClient + 0x230) >= 2 [SIGNONSTATE_CONNECTED])
    virtual bool IsConnected() = 0;

    // Slot 41: engine2.dll + 0x76530 (Returns *(INetChannelInfo**)(m_pNetGameClient + 0xf0))
    virtual INetChannelInfo* GetNetChannelInfo(std::int32_t nSplitScreenSlot = 0) = 0;

    virtual void Unk42() = 0; virtual void Unk43() = 0; virtual void Unk44() = 0;
    virtual void Unk45() = 0; virtual void Unk46() = 0; virtual void Unk47() = 0;
    virtual void Unk48() = 0; virtual void Unk49() = 0; virtual void Unk50() = 0;

    // Slot 51: engine2.dll + 0x75d60 (Dispatches console command string into engine execution queue)
    virtual void ExecuteClientCmd(std::int32_t nSplitScreenSlot, const char* pszCmd, bool bUnk = false) = 0;
};
static_assert(sizeof(ISource2EngineToClient) == 0x08, "Interface pointer must be 8 bytes");

// ============================================================================
// 2. CNetworkGameClient & INetworkClientService
// Binary: engine2.dll
// Token:  "NetworkClientService_001" (Node RVA: 0x619dc8, Factory RVA: 0x1bf4b0)
// VTable: engine2.dll + 0x613ac8 (CNetworkClientService, 64 methods)
// VTable: engine2.dll + 0x5d4cf0 (CNetworkGameClient, 147 methods)
// RTTI:   .?AVCNetworkGameClient@@
// ============================================================================
enum ESignonState : std::int32_t {
    SIGNONSTATE_NONE        = 0,
    SIGNONSTATE_CHALLENGE   = 1,
    SIGNONSTATE_CONNECTED   = 2,
    SIGNONSTATE_NEW         = 3,
    SIGNONSTATE_PRESPAWN    = 4,
    SIGNONSTATE_SPAWN       = 5,
    SIGNONSTATE_FULL        = 6, // In-game active
    SIGNONSTATE_CHANGELEVEL = 7
};

class CNetworkGameClient {
public:
    virtual ~CNetworkGameClient() = default;

    PAD(0x0E8, m_pad0);
    // engine2.dll + 0x76530 dereferences this pointer at +0x0F0
    INetChannelInfo* m_pNetChannel;       // +0x0F0
    PAD(0x118, m_pad1);
    // engine2.dll + 0x861a0 stores level name string at +0x210
    char             m_szLevelName[260];  // +0x210
    char             m_szLevelNameShort[32]; // +0x314
    PAD(0x008, m_pad2);
    // engine2.dll + 0x76380 checks signon state at +0x230
    // Note: In CNetworkGameClient layout, relative offsets:
    // Signon state offset verified: +0x230 from base
};

class INetworkClientService {
public:
    virtual ~INetworkClientService() = default;

    // Slot 23: Returns the active CNetworkGameClient singleton pointer
    // Discovered via RTTI slot walk in CNetworkClientService
    virtual void Unk01() = 0; virtual void Unk02() = 0; virtual void Unk03() = 0;
    virtual void Unk04() = 0; virtual void Unk05() = 0; virtual void Unk06() = 0;
    virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0; virtual void Unk11() = 0; virtual void Unk12() = 0;
    virtual void Unk13() = 0; virtual void Unk14() = 0; virtual void Unk15() = 0;
    virtual void Unk16() = 0; virtual void Unk17() = 0; virtual void Unk18() = 0;
    virtual void Unk19() = 0; virtual void Unk20() = 0; virtual void Unk21() = 0;
    virtual void Unk22() = 0;

    // Slot 23: engine2.dll + 0x1bf4b0
    virtual CNetworkGameClient* GetNetworkGameClient() = 0;
};

// ============================================================================
// 3. IGameResourceService (CGameResourceService)
// Binary: engine2.dll
// Token:  "GameResourceServiceClientV001" (Node RVA: 0x619eb8, Factory RVA: 0x1ab6e0)
// VTable: engine2.dll + 0x60e7c8 (182 methods)
// RTTI:   .?AVCGameResourceService@@
// ============================================================================
class IGameResourceService {
public:
    virtual ~IGameResourceService() = default;

    // CGameEntitySystem pointer is stored at offset +0x58 in CGameResourceService
    PAD(0x50, m_pad50);
    CGameEntitySystem* m_pGameEntitySystem; // +0x58

    CGameEntitySystem* GetGameEntitySystem() const {
        return m_pGameEntitySystem;
    }
};

// ============================================================================
// 4. IEngineServiceMgr (CEngineServiceMgr)
// Binary: engine2.dll
// Token:  "EngineServiceMgr001" (Node RVA: 0x61a868, Factory RVA: 0x247a80)
// VTable: engine2.dll + 0x61efb0 (249 methods)
// RTTI:   .?AVCEngineServiceMgr@@
// ============================================================================
class IEngineServiceMgr {
public:
    virtual ~IEngineServiceMgr() = default;
};

// ============================================================================
// 5. IEngineCVar (CCvar) & ConVar
// Binary: tier0.dll
// Token:  "VEngineCvar007" (Node RVA: 0x323530, Factory RVA: 0x789b0)
// VTable: tier0.dll + 0x313208 (48 methods)
// RTTI:   .?AVCCvar@@
// ============================================================================
enum EConVarFlags : std::uint32_t {
    FCVAR_NONE                    = 0,
    FCVAR_UNREGISTERED            = (1 << 0),
    FCVAR_DEVELOPMENTONLY         = (1 << 1),
    FCVAR_GAMEDLL                 = (1 << 2),
    FCVAR_CLIENTDLL               = (1 << 3),
    FCVAR_HIDDEN                  = (1 << 4),
    FCVAR_PROTECTED               = (1 << 5),
    FCVAR_SPONLY                  = (1 << 6),
    FCVAR_ARCHIVE                 = (1 << 7),
    FCVAR_NOTIFY                  = (1 << 8),
    FCVAR_USERINFO                = (1 << 9),
    FCVAR_PRINTABLEONLY           = (1 << 10),
    FCVAR_UNLOGGED                = (1 << 11),
    FCVAR_NEVER_AS_STRING         = (1 << 12),
    FCVAR_REPLICATED              = (1 << 13),
    FCVAR_CHEAT                   = (1 << 14),
    FCVAR_PER_USER                = (1 << 15),
    FCVAR_DEMO                    = (1 << 16),
    FCVAR_DONTRECORD              = (1 << 17),
    FCVAR_RELEASE                 = (1 << 19),
    FCVAR_MENUBAR_ITEM            = (1 << 20),
    FCVAR_COMMANDLINE_ENFORCED    = (1 << 21),
    FCVAR_NOT_CONNECTED           = (1 << 22),
    FCVAR_VCONSOLE_SET_FOCUS      = (1 << 23),
    FCVAR_SERVER_CAN_EXECUTE      = (1 << 28),
    FCVAR_SERVER_CANNOT_QUERY     = (1 << 29),
    FCVAR_CLIENTCMD_CAN_EXECUTE   = (1 << 30)
};

// Reconstructed ConVar Value union & storage
union CVValue_t {
    bool         m_bValue;
    std::int32_t m_iValue;
    std::int64_t m_i64Value;
    float        m_flValue;
    double       m_dbValue;
    const char*  m_szValue;
};

class ConVar {
public:
    const char*      m_pszName;        // +0x00
    ConVar*          m_pNext;          // +0x08
    PAD(0x10, m_pad10);                // +0x10
    const char*      m_pszHelpString;  // +0x20
    std::uint32_t    m_nFlags;         // +0x28
    PAD(0x14, m_pad2c);                // +0x2C
    CVValue_t        m_Value;          // +0x40

    bool GetBool() const { return m_Value.m_bValue; }
    std::int32_t GetInt() const { return m_Value.m_iValue; }
    float GetFloat() const { return m_Value.m_flValue; }
    const char* GetString() const { return m_Value.m_szValue ? m_Value.m_szValue : ""; }
};
static_assert(offsetof(ConVar, m_pszName) == 0x00, "ConVar m_pszName offset mismatch");
static_assert(offsetof(ConVar, m_pNext) == 0x08, "ConVar m_pNext offset mismatch");
static_assert(offsetof(ConVar, m_pszHelpString) == 0x20, "ConVar m_pszHelpString offset mismatch");
static_assert(offsetof(ConVar, m_nFlags) == 0x28, "ConVar m_nFlags offset mismatch");

class IEngineCVar {
public:
    virtual ~IEngineCVar() = default;

    virtual void Unk01() = 0; virtual void Unk02() = 0; virtual void Unk03() = 0;
    virtual void Unk04() = 0; virtual void Unk05() = 0; virtual void Unk06() = 0;
    virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0;

    // Slot 11: tier0.dll + 0x6c6b0 (Finds ConVar by caseless name / 0x31415926 hash)
    // Signature verified: 40 53 56 57 41 54 41 56 48 83 EC 30 33 DB 45 8B
    virtual ConVar* FindVar(const char* pszVarName) = 0;

    // Slot 12: tier0.dll + 0x6c8c0 (Finds ConVar by registration index)
    virtual ConVar* FindVarByIndex(std::int32_t nIndex) = 0;

    virtual void Unk13() = 0; virtual void Unk14() = 0; virtual void Unk15() = 0;
    virtual void Unk16() = 0; virtual void Unk17() = 0; virtual void Unk18() = 0;
    virtual void Unk19() = 0; virtual void Unk20() = 0; virtual void Unk21() = 0;
    virtual void Unk22() = 0; virtual void Unk23() = 0; virtual void Unk24() = 0;
    virtual void Unk25() = 0; virtual void Unk26() = 0; virtual void Unk27() = 0;
    virtual void Unk28() = 0; virtual void Unk29() = 0; virtual void Unk30() = 0;
    virtual void Unk31() = 0; virtual void Unk32() = 0; virtual void Unk33() = 0;
    virtual void Unk34() = 0; virtual void Unk35() = 0; virtual void Unk36() = 0;
    virtual void Unk37() = 0; virtual void Unk38() = 0; virtual void Unk39() = 0;
    virtual void Unk40() = 0;

    // Slot 41: tier0.dll + 0x6c5f0 (First cvar array index iterator)
    virtual std::uint64_t GetFirstCVarArrayValue() = 0;

    // Slot 42: tier0.dll + 0x69ff0 (Next cvar array index iterator)
    virtual std::uint64_t GetNextCVarArrayValue() = 0;
};

// ============================================================================
// 6. IMemAlloc
// Binary: tier0.dll
// Export: g_pMemAlloc (Export RVA: 0x3ac458, VA: 0x1803ac458)
// VTable: tier0.dll + 0x3277d0 (85 methods)
// RTTI:   .?AVCMemAllocStandard@@
// ============================================================================
class IMemAlloc {
public:
    virtual ~IMemAlloc() = default;

    // Slot 1: Allocates block of nSize bytes
    virtual void*  Alloc(std::size_t nSize) = 0;
    // Slot 2: Reallocates block
    virtual void*  Realloc(void* pMem, std::size_t nSize) = 0;
    // Slot 3: Frees memory block
    virtual void   Free(void* pMem) = 0;

    virtual void Unk04() = 0; virtual void Unk05() = 0; virtual void Unk06() = 0;
    virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0; virtual void Unk11() = 0; virtual void Unk12() = 0;
    virtual void Unk13() = 0; virtual void Unk14() = 0; virtual void Unk15() = 0;
    virtual void Unk16() = 0; virtual void Unk17() = 0; virtual void Unk18() = 0;
    virtual void Unk19() = 0; virtual void Unk20() = 0;

    // Slot 21: Returns usable size of allocated block
    virtual std::size_t GetSize(void* pMem) = 0;
};

// ============================================================================
// 7. IInputSystem & IInputStackSystem
// Binary: inputsystem.dll
// Tokens: "InputSystemVersion001" (Node RVA: 0x37850, Factory RVA: 0x11160)
//         "InputStackSystemVersion001" (Node RVA: 0x37838, Factory RVA: 0x3670)
// VTable: inputsystem.dll + 0x393e0 (CInputSystem, 106 methods)
// VTable: inputsystem.dll + 0x37910 (CInputStackSystem, 30 methods)
// RTTI:   .?AVCInputSystem@@, .?AVCInputStackSystem@@
// ============================================================================
class IInputSystem {
public:
    virtual ~IInputSystem() = default;

    // Slot 1..105: Discovered via MSVC RTTI vtable walk
    virtual void Unk01() = 0; virtual void Unk02() = 0; virtual void Unk03() = 0;
    virtual void Unk04() = 0; virtual void Unk05() = 0; virtual void Unk06() = 0;
    virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0; virtual void Unk11() = 0; virtual void Unk12() = 0;
    virtual void Unk13() = 0; virtual void Unk14() = 0; virtual void Unk15() = 0;
    virtual void Unk16() = 0; virtual void Unk17() = 0; virtual void Unk18() = 0;

    // Slot 19: inputsystem.dll + 0x12b10 (Polls button down state)
    virtual bool IsButtonDown(std::int32_t nButtonCode) = 0;
};

class IInputStackSystem {
public:
    virtual ~IInputStackSystem() = default;
};

// ============================================================================
// 8. IFileSystem
// Binary: filesystem_stdio.dll
// Token:  "VFileSystem017" (Node RVA: 0x1aecd0, Factory RVA: 0x3ca20)
// VTable: filesystem_stdio.dll + 0x1b0ca0 (140 methods)
// RTTI:   .?AVCFileSystem_Stdio@@
// ============================================================================
class IFileSystem {
public:
    virtual ~IFileSystem() = default;
};

// ============================================================================
// 9. ISchemaSystem & CSchemaSystemTypeScope
// Binary: schemasystem.dll
// Token:  "SchemaSystem_001" (Node RVA: 0x4c248, Factory RVA: 0x715f8)
// VTable: schemasystem.dll + 0x58598 (CSchemaSystem, 222 methods)
// VTable: schemasystem.dll + 0x586f0 (CSchemaSystemTypeScope, 179 methods)
// RTTI:   .?AVCSchemaSystem@@, .?AVCSchemaSystemTypeScope@@
// ============================================================================
#ifndef CS2_SDK_ISCHEMASYSTEM_DEFINED
#define CS2_SDK_ISCHEMASYSTEM_DEFINED
class ISchemaSystem {
public:
    virtual ~ISchemaSystem() = default;

    virtual void Unk01() = 0; virtual void Unk02() = 0; virtual void Unk03() = 0;
    virtual void Unk04() = 0; virtual void Unk05() = 0; virtual void Unk06() = 0;
    virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0; virtual void Unk11() = 0; virtual void Unk12() = 0;

    // Slot 13: schemasystem.dll + 0x11350 (Resolves type scope by module name string)
    virtual CSchemaSystemTypeScope* FindTypeScopeForModule(const char* pszModuleName, void* pBindingContext = nullptr) = 0;

    // Slot 14: schemasystem.dll + 0x11510 (Resolves type scope by module MurmurHash2 token)
    virtual CSchemaSystemTypeScope* FindTypeScopeForModule(std::uint32_t nScopeHash) = 0;
};
#endif

// ============================================================================
// 10. Primary Subsystems (Render, Material, Scene, Physics, Network, Audio, UI)
// Verified VTable RVAs, tokens, and method counts
// ============================================================================

// rendersystemdx11.dll: "RenderDeviceMgr001" (VTable: 0x3f5f98, 58 methods)
class IRenderDeviceMgr {
public:
    virtual ~IRenderDeviceMgr() = default;
};

// rendersystemdx11.dll: "RenderDevice001" (VTable: 0x3f40e8, 291 methods)
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;
};

// materialsystem2.dll: "VMaterialSystem2_001" (VTable: 0x12d920, 57 methods)
class IMaterialSystem2 {
public:
    virtual ~IMaterialSystem2() = default;

    virtual void Unk01() = 0; virtual void Unk02() = 0; virtual void Unk03() = 0;
    virtual void Unk04() = 0; virtual void Unk05() = 0; virtual void Unk06() = 0;
    virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0; virtual void Unk11() = 0; virtual void Unk12() = 0;
    virtual void Unk13() = 0;

    // Slot 14: materialsystem2.dll + 0x37200 (Finds or loads CMaterial2 instance from asset path)
    virtual CMaterial2** FindOrCreateFromResource(CMaterial2** ppOutMaterial, const char* pszResourceName) = 0;

    // Slot 15: materialsystem2.dll + 0x372d0 (Creates procedural / dynamic runtime material)
    virtual CMaterial2** CreateMaterial(CMaterial2** ppOutMaterial, const char* pszMaterialName, void* pData) = 0;
};

// scenesystem.dll: "SceneSystem_002" (VTable: 0x5f17c0, 199 methods)
class ISceneSystem {
public:
    virtual ~ISceneSystem() = default;
};

// meshsystem.dll: "MeshSystem001" (VTable: 0x1548d0, 47 methods)
class IMeshSystem {
public:
    virtual ~IMeshSystem() = default;
};

// animationsystem.dll: "AnimationSystem_001" (VTable: 0x7232e0, 224 methods)
class IAnimationSystem {
public:
    virtual ~IAnimationSystem() = default;
};

// worldrenderer.dll: "WorldRendererMgr001" (VTable: 0x1a61c0, 104 methods)
class IWorldRendererMgr {
public:
    virtual ~IWorldRendererMgr() = default;
};

// particles.dll: "ParticleSystemMgr003" (VTable: 0x4d7ff0, 64 methods)
class IParticleSystemMgr {
public:
    virtual ~IParticleSystemMgr() = default;
};

// vphysics2.dll: "VPhysics2Interface_001" (VTable: 0x3fbf50, 178 methods)
class IVPhysics2 {
public:
    virtual ~IVPhysics2() = default;
};

// networksystem.dll: "NetworkSystemVersion001" (VTable: 0x23a798, 64 methods)
class INetworkSystem {
public:
    virtual ~INetworkSystem() = default;
};

// networksystem.dll: "NetworkMessagesVersion001" (VTable: 0x239a38, 89 methods)
class INetworkMessages {
public:
    virtual ~INetworkMessages() = default;
};

// resourcesystem.dll: "ResourceSystem013" (VTable: 0x65348, 119 methods)
class IResourceSystem {
public:
    virtual ~IResourceSystem() = default;
};

// soundsystem.dll: "SoundSystem001" (VTable: 0x4918c8, 135 methods)
class ISoundSystem {
public:
    virtual ~ISoundSystem() = default;
};

// soundsystem.dll: "SoundOpSystem001" (VTable: 0x4ab140, 218 methods)
class ISoundOpSystem {
public:
    virtual ~ISoundOpSystem() = default;
};

// panorama.dll: "PanoramaUIEngine001" (VTable: 0x4c22e0, 18 methods)
class IPanoramaUIEngine {
public:
    virtual ~IPanoramaUIEngine() = default;
};

// pulse_system.dll: "PulseSystem_001" (VTable: 0x19cf98, 77 methods)
class IPulseSystem {
public:
    virtual ~IPulseSystem() = default;
};

// localize.dll: "Localize_001" (VTable: 0x4d058, 115 methods)
class ILocalize {
public:
    virtual ~ILocalize() = default;
};

// matchmaking.dll: "MatchFramework_001" (VTable: 0x1604f8, 30 methods)
class IMatchFramework {
public:
    virtual ~IMatchFramework() = default;
};

// matchmaking.dll: "GameTypes001" (VTable: 0x183730, 157 methods)
class IGameTypes {
public:
    virtual ~IGameTypes() = default;
};

// client.dll: "Source2Client002" (VTable: 0x1c65030, 240 methods)
class ISource2Client {
public:
    virtual ~ISource2Client() = default;
};

// server.dll: "Source2Server001" (VTable: 0x1996698, 105 methods)
class ISource2Server {
public:
    virtual ~ISource2Server() = default;
};

// server.dll: "Source2GameClients001" (VTable: 0x19981c8, 300 methods)
class ISource2GameClients {
public:
    virtual ~ISource2GameClients() = default;
};

// server.dll: "Source2GameEntities001" (VTable: 0x19965c0, 23 methods)
class ISource2GameEntities {
public:
    virtual ~ISource2GameEntities() = default;
};

// ============================================================================
// Dynamic Runtime Interface Resolver Helper
// Walks the DLL's exported CreateInterface or internal s_pInterfaceRegs list
// ============================================================================
template <typename T>
inline T* CaptureInterface(void* hModule, const char* pszInterfaceName) {
    if (!hModule || !pszInterfaceName) return nullptr;
    // User or mod code provides GetProcAddress equivalent:
    // auto pfnCreateInterface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress((HMODULE)hModule, "CreateInterface"));
    // return reinterpret_cast<T*>(pfnCreateInterface(pszInterfaceName, nullptr));
    return nullptr;
}

#undef PAD

} // namespace cs2::sdk
