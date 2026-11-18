//===========================================================================//
// Valve Source 2 Engine - Clean-Room SDK
// Module: server.dll (CS2 Build 2026-11-11_26598758)
// Subsystem: CBaseEntity & CEntityInstance 1:1 Binary Layout
//===========================================================================//

#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace cs2::server {

struct Vector3 {
    float x{ 0.0f }, y{ 0.0f }, z{ 0.0f };
};
static_assert(sizeof(Vector3) == 0x0C);

struct QAngle {
    float pitch{ 0.0f }, yaw{ 0.0f }, roll{ 0.0f };
};
static_assert(sizeof(QAngle) == 0x0C);

struct CTransform {
    Vector3 vecPosition;
    float   flPad0;
    float   rotQuat[4];
    float   flScale;
    float   flPad1[3];
};
static_assert(sizeof(CTransform) == 0x30);

class CEntityHandle {
public:
    union {
        uint32_t m_Value;
        struct {
            uint32_t m_Index  : 15;
            uint32_t m_Serial : 17;
        };
    };

    constexpr CEntityHandle() noexcept : m_Value(0xFFFFFFFF) {}
    constexpr explicit CEntityHandle(uint32_t val) noexcept : m_Value(val) {}
    constexpr bool IsValid() const noexcept { return m_Value != 0xFFFFFFFF && (m_Value & 0x7FFF) != 0x7FFF; }
    constexpr uint32_t GetEntryIndex() const noexcept { return m_Value & 0x7FFF; }
    constexpr uint32_t GetSerialNumber() const noexcept { return m_Value >> 15; }
    constexpr bool operator==(const CEntityHandle& other) const noexcept = default;
};
static_assert(sizeof(CEntityHandle) == 0x04);

class CEntityIdentity;
class CGameSceneNode;
class CCollisionProperty;
class CBodyComponent;

// 1. CEntityInstance (Base for all Source 2 entities, size 0x30)
class CEntityInstance {
public:
    virtual ~CEntityInstance() = default;              // +0x00: Slot 0 (dtor)
    int64_t          m_iszPrivateVScripts;             // +0x08
    CEntityIdentity* m_pEntityIdentity;                // +0x10: Link to descriptor
    uint8_t          m_pad18[0x18];                    // +0x18

    [[nodiscard]] inline CEntityIdentity* GetIdentity() const noexcept {
        return m_pEntityIdentity;
    }
};
static_assert(sizeof(CEntityInstance) == 0x30);
static_assert(offsetof(CEntityInstance, m_pEntityIdentity) == 0x10);

// 2. Component Structure Anchors
class CGameSceneNode {
public:
    virtual ~CGameSceneNode() = default;               // +0x00
    uint8_t          m_pad08[0x08];                    // +0x08
    CTransform       m_nodeToWorld;                    // +0x10: Spatial Transform Matrix
    CEntityInstance* m_pOwner;                         // +0x30
    CGameSceneNode*  m_pParent;                        // +0x38
    CGameSceneNode*  m_pChild;                         // +0x40
    CGameSceneNode*  m_pNextSibling;                   // +0x48
    uint8_t          m_pad50[0x20];                    // +0x50
    CEntityHandle    m_hParent;                        // +0x70
    uint8_t          m_pad74[0x0C];                    // +0x74
    Vector3          m_vecOrigin;                      // +0x80: Local Origin
    uint8_t          m_pad8C[0x2C];                    // +0x8C
    QAngle           m_angRotation;                    // +0xB8: Local Angles
    float            m_flScale;                        // +0xC4: Local Scale
    Vector3          m_vecAbsOrigin;                   // +0xC8: World Origin
    QAngle           m_angAbsRotation;                 // +0xD4: World Angles
    float            m_flAbsScale;                     // +0xE0: World Scale
};
static_assert(offsetof(CGameSceneNode, m_nodeToWorld)    == 0x10);
static_assert(offsetof(CGameSceneNode, m_vecOrigin)      == 0x80);
static_assert(offsetof(CGameSceneNode, m_vecAbsOrigin)   == 0xC8);
static_assert(offsetof(CGameSceneNode, m_angAbsRotation) == 0xD4);

class CBodyComponent {
public:
    virtual ~CBodyComponent() = default;               // +0x00
    CGameSceneNode*  m_pSceneNode;                     // +0x08: Scene graph node
    uint8_t          m_pad10[0x38];                    // +0x10
    CEntityInstance* __m_pChainEntity;                 // +0x48
};
static_assert(offsetof(CBodyComponent, m_pSceneNode) == 0x08);

// 3. CBaseEntity (Source 2 server.dll Ground-Truth Reconstruction)
// Size: Exactly 0x4A8 bytes (1,192 bytes)
class CBaseEntity : public CEntityInstance {
public:
    // Virtual Method Table (Slots 0..4, 11, 81)
    virtual void PostConstructor() {}                  // Slot 1 (RVA 0x1540d0)
    virtual void Spawn() {}                            // Slot 2 (RVA 0x153ff0)
    virtual void Precache() {}                         // Slot 3 (RVA 0x614920)
    virtual void Activate() {}                         // Slot 4 (RVA 0xcd1c70)

    // Member Variables
    CBodyComponent*            m_CBodyComponent;            // +0x0030
    void*                      m_NetworkTransmitComponent;  // +0x0038
    uint8_t                    m_pad40[0x208];              // +0x0040..+0x0248
    uintptr_t                  m_aThinkFunctions;           // +0x0248
    uint8_t                    m_pad250[0x10];              // +0x0250
    int32_t                    m_iCurrentThinkContext;      // +0x0260
    int32_t                    m_nLastThinkTick;            // +0x0264
    bool                       m_bDisabledContextThinks;    // +0x0268
    uint8_t                    m_pad269[0x0F];              // +0x0269
    int32_t                    m_isSteadyState;             // +0x0278
    uint8_t                    m_pad27C[0x04];              // +0x027C
    float                      m_lastNetworkChange;         // +0x0280
    uint8_t                    m_pad284[0x04];              // +0x0284
    void*                      m_think;                     // +0x0288: Delegate callback pointer
    uint8_t                    m_pad290[0x40];              // +0x0290
    int32_t                    m_iHealth;                   // +0x02D0
    int32_t                    m_iMaxHealth;                // +0x02D4
    uint8_t                    m_lifeState;                 // +0x02D8: 0=ALIVE, 1=DYING, 2=DEAD
    uint8_t                    m_pad2D9[0x03];              // +0x02D9
    float                      m_flDamageAccumulator;       // +0x02DC
    bool                       m_bTakesDamage;              // +0x02E0
    uint8_t                    m_pad2E1[0x07];              // +0x02E1
    int32_t                    m_nTakeDamageFlags;          // +0x02E8
    uint8_t                    m_pad2EC[0x04];              // +0x02EC
    int32_t                    m_nPlatformType;             // +0x02F0
    uint8_t                    m_MoveCollide;               // +0x02F2
    uint8_t                    m_MoveType;                  // +0x02F3
    uint8_t                    m_nPreviouslySetMoveType;    // +0x02F4
    uint8_t                    m_nActualMoveType;           // +0x02F5
    uint8_t                    m_nWaterTouch;               // +0x02F6
    uint8_t                    m_nSlimeTouch;               // +0x02F7
    bool                       m_bRestoreInHierarchy;       // +0x02F8
    uint8_t                    m_pad2F9[0x07];              // +0x02F9
    uintptr_t                  m_target;                    // +0x0300
    CEntityHandle              m_hDamageFilter;             // +0x0308
    uint8_t                    m_pad30C[0x04];              // +0x030C
    uintptr_t                  m_iszDamageFilterName;       // +0x0310
    float                      m_flMoveDoneTime;            // +0x0318
    uint32_t                   m_nSubclassID;               // +0x031C: CUtlStringToken
    uint8_t                    m_pad320[0x08];              // +0x0320
    float                      m_flAnimTime;                // +0x0328
    float                      m_flSimulationTime;          // +0x032C
    float                      m_flCreateTime;              // +0x0330
    bool                       m_bClientSideRagdoll;        // +0x0334
    uint8_t                    m_ubInterpolationFrame;      // +0x0335
    uint8_t                    m_pad336[0x02];              // +0x0336
    Vector3                    m_vPrevVPhysicsUpdatePos;    // +0x0338
    int32_t                    m_iTeamNum;                  // +0x0344
    uintptr_t                  m_iGlobalname;               // +0x0348
    int32_t                    m_iSentToClients;            // +0x0350
    uint8_t                    m_pad354[0x04];              // +0x0354
    uintptr_t                  m_sUniqueHammerID;           // +0x0358
    uint32_t                   m_spawnflags;                // +0x0360
    int32_t                    m_nNextThinkTick;            // +0x0364
    int32_t                    m_nSimulationTick;           // +0x0368
    uint8_t                    m_pad36C[0x04];              // +0x036C
    uint8_t                    m_OnKilled[0x18];            // +0x0370: CEntityIOOutput
    uint32_t                   m_fFlags;                    // +0x0388: FL_ONGROUND, FL_DUCKING, etc.
    Vector3                    m_vecAbsVelocity;            // +0x038C
    Vector3                    m_vecVelocity;               // +0x0398
    uint8_t                    m_pad3A4[0x24];              // +0x03A4
    Vector3                    m_vecBaseVelocity;           // +0x03C8
    int32_t                    m_nPushEnumCount;            // +0x03D4
    CCollisionProperty*        m_pCollision;                // +0x03D8
    CEntityHandle              m_hEffectEntity;             // +0x03E0
    CEntityHandle              m_hOwnerEntity;              // +0x03E4
    uint32_t                   m_fEffects;                  // +0x03E8
    CEntityHandle              m_hGroundEntity;             // +0x03EC
    int32_t                    m_nGroundBodyIndex;          // +0x03F0
    float                      m_flFriction;                // +0x03F4
    float                      m_flElasticity;              // +0x03F8
    float                      m_flGravityScale;            // +0x03FC
    float                      m_flTimeScale;               // +0x0400
    float                      m_flWaterLevel;              // +0x0404
    bool                       m_bGravityDisabled;          // +0x0408
    bool                       m_bAnimatedEveryTick;        // +0x0409
    uint8_t                    m_pad40A[0x02];              // +0x040A
    float                      m_flActualGravityScale;      // +0x040C
    bool                       m_bGravityActuallyDisabled;  // +0x0410
    bool                       m_bDisableLowViolence;       // +0x0411
    uint8_t                    m_nWaterType;                // +0x0412
    uint8_t                    m_pad413[0x01];              // +0x0413
    int32_t                    m_iEFlags;                   // +0x0414
    uint8_t                    m_OnUser1[0x18];             // +0x0418
    uint8_t                    m_OnUser2[0x18];             // +0x0430
    uint8_t                    m_OnUser3[0x18];             // +0x0448
    uint8_t                    m_OnUser4[0x18];             // +0x0460
    int32_t                    m_iInitialTeamNum;           // +0x0478
    float                      m_flNavIgnoreUntilTime;      // +0x047C
    Vector3                    m_vecAngVelocity;            // +0x0480
    bool                       m_bNetworkQuantizeOrigin;    // +0x048C
    bool                       m_bLagCompensate;            // +0x048D
    uint8_t                    m_pad48E[0x02];              // +0x048E
    void*                      m_pBlocker;                  // +0x0490
    float                      m_flLocalTime;               // +0x0494
    float                      m_flVPhysicsUpdateLocalTime; // +0x0498
    int32_t                    m_nBloodType;                // +0x049C
    void*                      m_pPulseGraphInstance;       // +0x04A0

    [[nodiscard]] inline CGameSceneNode* GetSceneNode() const noexcept {
        return m_CBodyComponent ? m_CBodyComponent->m_pSceneNode : nullptr;
    }

    [[nodiscard]] inline const Vector3& GetAbsOrigin() const noexcept {
        static constexpr Vector3 kZeroOrigin{ 0.0f, 0.0f, 0.0f };
        CGameSceneNode* pNode = GetSceneNode();
        return pNode ? pNode->m_vecAbsOrigin : kZeroOrigin;
    }

    [[nodiscard]] inline const QAngle& GetAbsAngles() const noexcept {
        static constexpr QAngle kZeroAngle{ 0.0f, 0.0f, 0.0f };
        CGameSceneNode* pNode = GetSceneNode();
        return pNode ? pNode->m_angAbsRotation : kZeroAngle;
    }

    [[nodiscard]] inline const CTransform* GetTransform() const noexcept {
        CGameSceneNode* pNode = GetSceneNode();
        return pNode ? &pNode->m_nodeToWorld : nullptr;
    }
};

static_assert(sizeof(CBaseEntity) == 0x4A8, "CBaseEntity total footprint must be exactly 0x4a8 (1192 bytes)");
static_assert(offsetof(CBaseEntity, m_CBodyComponent)    == 0x030);
static_assert(offsetof(CBaseEntity, m_aThinkFunctions)   == 0x248);
static_assert(offsetof(CBaseEntity, m_think)             == 0x288);
static_assert(offsetof(CBaseEntity, m_iHealth)           == 0x2D0);
static_assert(offsetof(CBaseEntity, m_lifeState)         == 0x2D8);
static_assert(offsetof(CBaseEntity, m_nSubclassID)       == 0x31C);
static_assert(offsetof(CBaseEntity, m_nNextThinkTick)    == 0x364);
static_assert(offsetof(CBaseEntity, m_fFlags)            == 0x388);
static_assert(offsetof(CBaseEntity, m_pCollision)        == 0x3D8);
static_assert(offsetof(CBaseEntity, m_pPulseGraphInstance) == 0x4A0);

} // namespace cs2::server
