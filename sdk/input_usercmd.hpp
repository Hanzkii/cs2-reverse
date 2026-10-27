#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include "interfaces_registry.hpp"
#include "entity_system.hpp"
#include "client_classes.hpp"
#include "render_scene_material.hpp"

// ============================================================================
// Source 2 Reconstructed SDK - Phase 3: Input, Commands & Subtick Timing
// Target Build: 2026-10-25_26144732 (CS2 Update Archive)
// Architecture: x86-64 (MSVC ABI)
// Reconstructed & Verified via Static RTTI, Protobuf Descriptors & Disassembly
// ============================================================================

namespace cs2::sdk {

#define PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

// ============================================================================
// 1. Source 2 Button Bitmasks (64-bit unsigned integers)
// ============================================================================
enum EInButtonState : std::uint64_t {
    IN_ATTACK       = (1ULL << 0),
    IN_JUMP         = (1ULL << 1),
    IN_DUCK         = (1ULL << 2),
    IN_FORWARD      = (1ULL << 3),
    IN_BACK         = (1ULL << 4),
    IN_USE          = (1ULL << 5),
    IN_CANCEL       = (1ULL << 6),
    IN_LEFT         = (1ULL << 7),
    IN_RIGHT        = (1ULL << 8),
    IN_MOVELEFT     = (1ULL << 9),
    IN_MOVERIGHT    = (1ULL << 10),
    IN_ATTACK2      = (1ULL << 11),
    IN_RUN          = (1ULL << 12),
    IN_RELOAD       = (1ULL << 13),
    IN_ALT1         = (1ULL << 14),
    IN_ALT2         = (1ULL << 15),
    IN_SPEED        = (1ULL << 16),
    IN_WALK         = (1ULL << 17),
    IN_ZOOM         = (1ULL << 19),
    IN_FIRST_MOD    = (1ULL << 20),
    IN_SECOND_MOD   = (1ULL << 21),
    IN_BULLRUSH     = (1ULL << 22),
    IN_GRENADE1     = (1ULL << 23),
    IN_GRENADE2     = (1ULL << 24),
    IN_ATTACK3      = (1ULL << 25),
};

// ============================================================================
// 2. Google Protobuf Internal Wire & Arena Structures
// ============================================================================
#ifndef CS2_SDK_PROTOBUF_DEFINED
#define CS2_SDK_PROTOBUF_DEFINED
namespace protobuf {
    class Arena;

    namespace internal {
        struct InternalMetadata {
            void* ptr; // Low-order tag bits store allocation flags; upper bits store Arena*
        };

        struct Rep {
            std::int32_t m_nAllocatedSize; // +0x00
            std::int32_t m_nPad04;         // +0x04
            void*        m_pElements[1];   // +0x08
        };

        struct RepeatedPtrFieldBase {
            Arena*       m_pArena;           // +0x00
            std::int32_t m_nCurrentElements; // +0x08
            std::int32_t m_nTotalElements;   // +0x0C
            Rep*         m_pRep;             // +0x10
        };
    }

    template <typename T>
    struct RepeatedPtrField : public internal::RepeatedPtrFieldBase {
        T* operator[](int index) const noexcept {
            if (!this->m_pRep || index < 0 || index >= this->m_nCurrentElements) return nullptr;
            return reinterpret_cast<T*>(this->m_pRep->m_pElements[index]);
        }
        int Count() const noexcept { return this->m_nCurrentElements; }
    };
    static_assert(sizeof(RepeatedPtrField<void*>) == 0x18, "RepeatedPtrField size must be 0x18 (24 bytes)");

    class MessageLite {
    public:
        virtual ~MessageLite() = default;
    };

    class Message : public MessageLite {};
}
#endif // CS2_SDK_PROTOBUF_DEFINED

// ============================================================================
// 3. Protobuf Message Types for Angles, Vectors, and Subtick Events
// ============================================================================

// Size: 0x28 (40 bytes), VTable RVA: client.dll + 0x01b72908
struct CMsgQAngle : public protobuf::Message {
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_;                       // +0x14
    float pitch;                                              // +0x18 (X)
    float yaw;                                                // +0x1C (Y)
    float roll;                                               // +0x20 (Z)
    std::uint32_t _pad24;                                     // +0x24
};
static_assert(sizeof(CMsgQAngle) == 0x28, "CMsgQAngle layout mismatch");

// Size: 0x28 (40 bytes)
struct CMsgVector : public protobuf::Message {
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_;                       // +0x14
    float x;                                                  // +0x18
    float y;                                                  // +0x1C
    float z;                                                  // +0x20
    float w;                                                  // +0x24
};
static_assert(sizeof(CMsgVector) == 0x28, "CMsgVector layout mismatch");

// Size: 0x30 (48 bytes), VTable RVA: client.dll + 0x01b2f3e8
struct CInButtonStatePB : public protobuf::Message {
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_;                       // +0x14
    std::uint64_t buttonstate1;                               // +0x18 (Held buttons)
    std::uint64_t buttonstate2;                               // +0x20 (Pressed edges)
    std::uint64_t buttonstate3;                               // +0x28 (Released edges)
};
static_assert(sizeof(CInButtonStatePB) == 0x30, "CInButtonStatePB layout mismatch");

// Embedded button state structure (Size: 0x20 / 32 bytes)
// VTable RVA: client.dll + 0x01b2b5b0
struct CInButtonState {
    void*         m_pVTable;                                  // +0x00
    std::uint64_t m_nHeldButtons;                             // +0x08 (buttonstate1)
    std::uint64_t m_nPressedButtons;                          // +0x10 (buttonstate2)
    std::uint64_t m_nReleasedButtons;                         // +0x18 (buttonstate3)
};
static_assert(sizeof(CInButtonState) == 0x20, "CInButtonState layout mismatch");

// Client Subtick Move Step in Protobuf Wire Format (Size: 0x38 / 56 bytes)
// VTable RVA: client.dll + 0x01b2f488
struct CSubtickMoveStep : public protobuf::Message {
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_;                       // +0x14
    std::uint64_t m_nButton;                                  // +0x18 (Button bitmask, 0 for analog)
    bool          m_bPressed;                                 // +0x20 (Keydown edge vs keyup)
    std::uint8_t  _pad21[3];                                  // +0x21
    float         m_flWhen;                                   // +0x24 (Sub-tick fraction in [0.0, 1.0])
    float         m_flAnalogForwardDelta;                     // +0x28
    float         m_flAnalogLeftDelta;                        // +0x2C
    float         m_flPitchDelta;                             // +0x30
    float         m_flYawDelta;                               // +0x34
};
static_assert(sizeof(CSubtickMoveStep) == 0x38, "CSubtickMoveStep layout mismatch");

// Hardware/Input Ring Buffer Step (Internal 32-byte representation within CCSGOInput)
struct CSubtickInputStep {
    float         m_flWhen;               // +0x00 (Normalized fraction in [0.0, 1.0])
    std::uint32_t _pad04;                 // +0x04
    std::uint64_t m_nButton;              // +0x08 (Button bitmask)
    bool          m_bPressed;             // +0x10
    std::uint8_t  _pad11[3];              // +0x11
    float         m_flAnalogLeftDelta;    // +0x14
    float         m_flPitchDelta;         // +0x18
    float         m_flYawDelta;           // +0x1C
};
static_assert(sizeof(CSubtickInputStep) == 0x20, "CSubtickInputStep must be exactly 32 bytes");

// Interpolation descriptors
struct CSGOInterpolationInfoPB : public protobuf::Message {
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_;                       // +0x14
    std::int32_t src_tick;                                    // +0x18
    std::int32_t dst_tick;                                    // +0x1C
    float        frac;                                        // +0x20
    std::uint32_t _pad;                                       // +0x24
};
static_assert(sizeof(CSGOInterpolationInfoPB) == 0x28, "CSGOInterpolationInfoPB layout mismatch");

struct CSGOInterpolationInfoPB_CL : public protobuf::Message {
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_;                       // +0x14
    float        frac;                                        // +0x18
    std::uint32_t _pad;                                       // +0x1C
};
static_assert(sizeof(CSGOInterpolationInfoPB_CL) == 0x20, "CSGOInterpolationInfoPB_CL layout mismatch");

// ============================================================================
// 4. Subtick Input History Entry (Size: 0x78 / 120 bytes)
// VTable RVA: client.dll + 0x01bcecb0
// ============================================================================
class CSGOInputHistoryEntryPB : public protobuf::Message {
public:
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_;                       // +0x14
    CMsgQAngle*                 m_pViewAngles;                // +0x18
    CSGOInterpolationInfoPB_CL* m_pClInterp;                  // +0x20
    CSGOInterpolationInfoPB*    m_pSvInterp0;                 // +0x28
    CSGOInterpolationInfoPB*    m_pSvInterp1;                 // +0x30
    CSGOInterpolationInfoPB*    m_pPlayerInterp;              // +0x38
    CMsgVector*                 m_pShootPosition;             // +0x40
    CMsgVector*                 m_pTargetHeadPosCheck;        // +0x48
    CMsgVector*                 m_pTargetAbsPosCheck;         // +0x50
    CMsgQAngle*                 m_pTargetAbsAngCheck;         // +0x58
    std::int32_t                m_nRenderTickCount;           // +0x60
    float                       m_flRenderTickFraction;       // +0x64 [0.0, 1.0]
    std::int32_t                m_nPlayerTickCount;           // +0x68
    float                       m_flPlayerTickFraction;       // +0x6C [0.0, 1.0]
    std::int32_t                m_nFrameNumber;               // +0x70
    std::int32_t                m_nTargetEntIndex;            // +0x74
};
static_assert(sizeof(CSGOInputHistoryEntryPB) == 0x78, "CSGOInputHistoryEntryPB size must be 0x78 (120 bytes)");

class CBaseUserCmdExecutionNotes : public protobuf::Message {
public:
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_;                       // +0x14
    void* m_pIgnoredReason;                                   // +0x18
};
static_assert(sizeof(CBaseUserCmdExecutionNotes) == 0x20, "CBaseUserCmdExecutionNotes layout mismatch");

// ============================================================================
// 5. Base User Command Protobuf Message (Size: 0x88 / 136 bytes)
// VTable RVA: client.dll + 0x01b2f5c8
// ============================================================================
class CBaseUserCmdPB : public protobuf::Message {
public:
    protobuf::internal::InternalMetadata _internal_metadata_;                 // +0x08
    std::uint32_t _has_bits_[1];                                              // +0x10
    mutable std::int32_t _cached_size_;                                       // +0x14

    protobuf::RepeatedPtrField<CSubtickMoveStep> m_subtick_moves;             // +0x18
    void*                        m_pMoveCRC;                                  // +0x30 (std::string*)
    CInButtonStatePB*            m_pButtonsPB;                                // +0x38
    CMsgQAngle*                  m_pViewAngles;                               // +0x40
    CBaseUserCmdExecutionNotes*  m_pExecutionNotes;                           // +0x48
    std::int32_t                 m_nLegacyCommandNumber;                      // +0x50
    std::int32_t                 m_nClientTick;                               // +0x54
    float                        m_flForwardMove;                             // +0x58
    float                        m_flLeftMove;                                // +0x5C
    float                        m_flUpMove;                                  // +0x60
    std::int32_t                 m_nImpulse;                                  // +0x64
    std::int32_t                 m_nWeaponSelect;                             // +0x68
    std::int32_t                 m_nRandomSeed;                               // +0x6C
    std::int32_t                 m_nMousedX;                                  // +0x70
    std::int32_t                 m_nMousedY;                                  // +0x74
    std::uint32_t                m_nPredictionOffsetTicksX256;                // +0x78
    std::uint32_t                m_nConsumedServerAngleChanges;               // +0x7C
    std::int32_t                 m_nCmdFlags;                                 // +0x80
    std::uint32_t                m_nPawnEntityHandle;                         // +0x84
};
static_assert(sizeof(CBaseUserCmdPB) == 0x88, "CBaseUserCmdPB size must be 0x88 (136 bytes)");

// ============================================================================
// 6. CSGO Specific User Command Protobuf Message (Size: 0x48 / 72 bytes)
// VTable RVA: client.dll + 0x01bced50
// ============================================================================
class CSGOUserCmdPB : public protobuf::Message {
public:
    protobuf::internal::InternalMetadata _internal_metadata_;                 // +0x08
    std::uint32_t _has_bits_[1];                                              // +0x10
    mutable std::int32_t _cached_size_;                                       // +0x14
    protobuf::RepeatedPtrField<CSGOInputHistoryEntryPB> m_input_history;       // +0x18
    CBaseUserCmdPB* m_pBaseCmd;                                               // +0x30
    bool            m_bHasBeenPredicted;                                      // +0x38
    bool            m_bIsPredictingBodyShotFx;                                // +0x39
    bool            m_bIsPredictingHeadShotFx;                                // +0x3A
    bool            m_bIsPredictingKillRagdolls;                              // +0x3B
    std::int32_t    m_nAttack1StartHistoryIndex;                              // +0x3C
    std::int32_t    m_nAttack2StartHistoryIndex;                              // +0x40
    std::uint32_t   m_nPad54;                                                 // +0x44
};
static_assert(sizeof(CSGOUserCmdPB) == 0x48, "CSGOUserCmdPB size must be 0x48 (72 bytes)");

// ============================================================================
// 7. Full CUserCmd Container (Size: 0x98 / 152 bytes)
// VTable RVA: client.dll + 0x01c1cf68 (VA: 0x181c1cf68)
// Scalar Destructor: client.dll + 0x0092c9d0
// ============================================================================
class CUserCmdBase {
public:
    virtual ~CUserCmdBase() = default;                                        // +0x00
};

template <typename T>
class CUserCmdBaseHost : public CUserCmdBase {
public:
    std::int32_t  m_nSequenceNumber;                                          // +0x08
    std::uint32_t m_nPad0C;                                                   // +0x0C
};

class CCSGOUserCmd : public CUserCmdBaseHost<CSGOUserCmdPB> {};

class CUserCmd : public CCSGOUserCmd, public CSGOUserCmdPB {
public:
    // +0x00: CCSGOUserCmd (disp = 0, size 0x10)
    // +0x10: CSGOUserCmdPB (disp = 16, size 0x48, ends at 0x58)
    CInButtonState m_buttons;                                                 // +0x58..+0x78 (size 0x20)
    std::int32_t   m_nImpulse;                                                // +0x78
    std::int32_t   m_nWeaponSelect;                                           // +0x7C
    std::int64_t   m_nRandomSeed;                                             // +0x80
    std::int32_t   m_nMousedX;                                                // +0x88
    bool           m_bHasPredicted;                                           // +0x8C
    std::uint8_t   m_nPad8D[3];                                               // +0x8D
    std::int32_t   m_nPad90;                                                  // +0x90
    std::int32_t   m_nCommandStatus;                                          // +0x94 (2 = ready / executed)
};
static_assert(sizeof(CUserCmd) == 0x98, "CUserCmd size must be 0x98 (152 bytes)");
static_assert(offsetof(CUserCmd, m_nSequenceNumber) == 0x08, "m_nSequenceNumber offset mismatch");
static_assert(offsetof(CUserCmd, m_pBaseCmd) == 0x40, "m_pBaseCmd offset mismatch in CUserCmd");
static_assert(offsetof(CUserCmd, m_buttons) == 0x58, "m_buttons offset mismatch in CUserCmd");
static_assert(offsetof(CUserCmd, m_nImpulse) == 0x78, "m_nImpulse offset mismatch in CUserCmd");
static_assert(offsetof(CUserCmd, m_nCommandStatus) == 0x94, "m_nCommandStatus offset mismatch in CUserCmd");

// ============================================================================
// 8. CCSGOInput Subsystem Layout (Size: 0xc20 / 3,104 bytes)
// Global Pointer: client.dll + 0x0223dc00 (VA: 0x18223dc00)
// Static Instance: client.dll + 0x0257d780 (VA: 0x18257d780)
// Getter Function: client.dll + 0x00ce37a0 (GetCSGOInput)
// VTable RVA: client.dll + 0x01c9fa58 (VA: 0x181c9fa58)
// ============================================================================
class CCSGOInput {
public:
    // Slot 0: Scalar deleting destructor (client.dll + 0x00cde340)
    virtual ~CCSGOInput() = default;

    virtual void Unk01() = 0;
    virtual void Unk02() = 0;
    virtual void Unk03() = 0;
    virtual void Unk04() = 0;

    // Slot 5: CreateMove (client.dll + 0x00b66c20, VA: 0x180b66c20)
    // Parameter Signature:
    //   rcx: this (CCSGOInput*)
    //   edx: nSlot (split-screen player slot, 0 for primary)
    //   r8b: bActive (input activation flag)
    virtual bool CreateMove(std::int32_t nSlot, bool bActive) = 0;

    // Memory members
    PAD(0x258, m_pad08);
    // Sub-tick move storage array (capacity 32)
    std::int32_t      m_nFixedSubtickAllocCount; // +0x260
    PAD(0x00C, m_pad264);
    void*             m_pSubtickMemory;          // +0x270 (&this->m_SubtickMoveSteps)
    std::int32_t      m_nSubtickCapacity;        // +0x278 (32)
    PAD(0x008, m_pad27C);
    std::int32_t      m_nSubtickMoveStepCount;   // +0x284 (Number of sub-tick steps recorded this frame)
    CSubtickInputStep m_SubtickMoveSteps[32];    // +0x288..+0x688 (1,024 bytes)
    QAngle            m_angViewAngles;           // +0x688
    PAD(0x284, m_pad694);
    float             m_flSubtickAccumulator;    // +0x918
    PAD(0x234, m_pad91C);
    std::int32_t      m_nMouseHistoryCount;      // +0xb50
    PAD(0x004, m_padB54);
    void*             m_pMouseHistory;           // +0xb58
    PAD(0x020, m_padB60);
    void*             m_pSecondaryVTable;        // +0xb80 (client.dll + 0x01c9fb68)
    void*             m_pTertiaryVTable;         // +0xb88 (client.dll + 0x01c9fb78)
    bool              m_bIsSwappingToPredictableController; // +0xb90
    PAD(0x02B, m_padB91);
    bool              m_bCameraInThirdPerson;    // +0xbbc
    PAD(0x01B, m_padBBD);
    std::int32_t      m_nFrameSequence;          // +0xbd8
    PAD(0x020, m_padBDC);
    std::int32_t      m_nInputLockCount;         // +0xbfc
    PAD(0x008, m_padC00);
    QAngle            m_angThirdPersonAngle;     // +0xc08
    PAD(0x00C, m_padC14);
};
static_assert(sizeof(CCSGOInput) == 0xc20, "CCSGOInput size must be 0xc20 (3,104 bytes)");
static_assert(offsetof(CCSGOInput, m_nSubtickMoveStepCount) == 0x284, "m_nSubtickMoveStepCount offset");
static_assert(offsetof(CCSGOInput, m_SubtickMoveSteps) == 0x288, "m_SubtickMoveSteps offset");
static_assert(offsetof(CCSGOInput, m_angViewAngles) == 0x688, "m_angViewAngles offset");
static_assert(offsetof(CCSGOInput, m_flSubtickAccumulator) == 0x918, "m_flSubtickAccumulator offset");

#undef PAD

} // namespace cs2::sdk
