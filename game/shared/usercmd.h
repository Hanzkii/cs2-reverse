#ifndef CS2_SHARED_USERCMD_H
#define CS2_SHARED_USERCMD_H

#include <cstdint>
#include <cstddef>
#include "math_types.h"

namespace cs2::shared {

#define CS2_PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

/**
 * @brief Sub-tick input history entry protobuf container (CSgoInputHistoryEntryPB)
 * Size: 120 bytes (0x78).
 */
struct CSgoInputHistoryEntryPB {
    CS2_PAD(0x18, m_pad0);
    QAngle        m_viewAngles{};            // +0x18: Sub-tick view angles
    Vector3D      m_shootPosition{};         // +0x24: Player eye position
    Vector3D      m_targetHeadPositionCheck{}; // +0x30: Lag compensation verification point
    float         m_flRenderTickFraction{0.0f}; // +0x3C: Sub-tick fraction within render frame
    float         m_flPlayerTickFraction{0.0f}; // +0x40: Sub-tick fraction within tick interval [0.0..1.0)
    CS2_PAD(0x34, m_pad44);
};
static_assert(sizeof(CSgoInputHistoryEntryPB) == 0x78, "CSgoInputHistoryEntryPB must be 120 bytes");

/**
 * @brief Base UserCmd Protobuf container (CBaseUserCmdPB)
 * Size: 136 bytes (0x88).
 */
struct CBaseUserCmdPB {
    CS2_PAD(0x18, m_pad0);
    std::uint32_t m_nCommandNumber{0};       // +0x18: Monotonically increasing command sequence
    std::int32_t  m_nTickCount{0};           // +0x1C: Synchronized server tick
    CS2_PAD(0x24, m_pad20);
    QAngle        m_viewAngles{};            // +0x44: Command aim angles
    float         m_flForwardMove{0.0f};     // +0x50: Analog forward/back motion
    float         m_flSideMove{0.0f};        // +0x54: Analog left/right motion
    float         m_flUpMove{0.0f};          // +0x58: Analog vertical motion
    CS2_PAD(0x2C, m_pad5c);
};
static_assert(sizeof(CBaseUserCmdPB) == 0x88, "CBaseUserCmdPB must be 136 bytes");

/**
 * @brief Reconstructed Source 2 Client User Command (CUserCmd)
 * Size: 152 bytes (0x98).
 */
struct CUserCmd {
    CS2_PAD(0x40, m_pad0);
    CBaseUserCmdPB*          m_pBaseCmd{nullptr};                // +0x40: Protobuf base command
    CS2_PAD(0x20, m_pad48);
    CSgoInputHistoryEntryPB* m_pInputHistoryEntries{nullptr};     // +0x68: Dynamic array of sub-tick entries
    std::int32_t             m_nInputHistoryEntriesCount{0};     // +0x70: Sub-tick history count
    std::int32_t             m_nInputHistoryEntriesAllocated{0}; // +0x74: Allocated capacity
    std::uint8_t             m_nAttack1StartHistoryIndex{0xFF};  // +0x78: Attack 1 transition index
    std::uint8_t             m_nAttack2StartHistoryIndex{0xFF};  // +0x79: Attack 2 transition index
    std::uint8_t             m_nAttack3StartHistoryIndex{0xFF};  // +0x7A: Attack 3 transition index
    CS2_PAD(0x1D, m_pad7b);
};
static_assert(sizeof(CUserCmd) == 0x98, "CUserCmd must be 152 bytes");

#undef CS2_PAD

} // namespace cs2::shared

#endif // CS2_SHARED_USERCMD_H
