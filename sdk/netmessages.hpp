#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

// ============================================================================
// Source 2 Reconstructed SDK - NetMessages & Entity State Serialization
// Target Build: 2026-10-25_26144732 (CS2 Update Archive)
// Architecture: x86-64 (MSVC ABI)
// Binary: networksystem.dll, engine2.dll, client.dll
// Reconstructed via Ghidra Static Disassembly & Binary RTTI Analysis
// ============================================================================

namespace cs2::sdk {

// Include guard for shared Google Protobuf wire descriptors
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
// 1. NetPacketHeader_t (Wire Framing & Transport Packet Header)
// ============================================================================
// Size: 0x18 (24 bytes), 8-byte aligned
struct NetPacketHeader_t {
    std::uint32_t m_nMessageId{0};       // +0x00: NetMessage ID (e.g., 0x04 for Tick, 0x37 for PacketEntities)
    std::uint32_t m_nWireSize{0};        // +0x04: Encoded payload size in bytes (LEB128 VarInt32)
    std::uint32_t m_nHeaderBits{0};      // +0x08: Bit length of variable-bit header (6, 10, 14, or 34 bits)
    std::uint32_t m_nSequenceNumber{0};  // +0x0C: Outgoing transport packet sequence number
    std::uint32_t m_nSequenceAck{0};     // +0x10: Last received sequence acknowledgement
    std::uint32_t m_nFlags{0};           // +0x14: Packet status flags (reliable, choked, etc.)
};
static_assert(sizeof(NetPacketHeader_t) == 0x18, "NetPacketHeader_t size must be 24 bytes (0x18)");

// ============================================================================
// 2. CNetworkVar<T> & Property Dirty Bitmasks
// ============================================================================

/**
 * @brief Property dirty tracking bitmask for entity network delta synchronization.
 * Supports arbitrary bit capacities packed into 64-bit words.
 */
template <std::size_t NumBits>
class CNetworkPropertyDirtyBitmask {
public:
    static constexpr std::size_t WordCount = (NumBits + 63) / 64;
    std::uint64_t m_Words[WordCount]{0};

    constexpr void Set(std::size_t bit) noexcept {
        if (bit < NumBits) {
            m_Words[bit / 64] |= (1ULL << (bit % 64));
        }
    }

    constexpr void Clear(std::size_t bit) noexcept {
        if (bit < NumBits) {
            m_Words[bit / 64] &= ~(1ULL << (bit % 64));
        }
    }

    [[nodiscard]] constexpr bool IsSet(std::size_t bit) const noexcept {
        if (bit >= NumBits) return false;
        return (m_Words[bit / 64] & (1ULL << (bit % 64))) != 0;
    }

    constexpr void ClearAll() noexcept {
        for (std::size_t i = 0; i < WordCount; ++i) {
            m_Words[i] = 0;
        }
    }

    [[nodiscard]] constexpr bool Any() const noexcept {
        for (std::size_t i = 0; i < WordCount; ++i) {
            if (m_Words[i] != 0) return true;
        }
        return false;
    }
};

/**
 * @brief Source 2 networked property wrapper tracking modification ticks and dirty flags.
 * Bit 31 of m_nTickAndFlags encodes the dirty bit; Bits 0..30 encode the change tick.
 */
template <typename T>
class CNetworkVar {
public:
    using value_type = T;

    static constexpr std::uint32_t DIRTY_BIT_MASK = 0x80000000;
    static constexpr std::uint32_t TICK_VALUE_MASK = 0x7FFFFFFF;

    constexpr CNetworkVar() = default;
    constexpr explicit CNetworkVar(const T& val) noexcept
        : m_Value(val), m_nTickAndFlags(0) {}

    // Implicit conversion & dereference accessors
    constexpr operator const T&() const noexcept { return m_Value; }
    constexpr const T& Get() const noexcept { return m_Value; }
    constexpr const T* operator->() const noexcept { return &m_Value; }

    // Assignment updates value and increments change tick with dirty bit
    CNetworkVar& operator=(const T& val) noexcept {
        if (m_Value != val) {
            m_Value = val;
            const std::uint32_t curTick = (m_nTickAndFlags & TICK_VALUE_MASK) + 1;
            m_nTickAndFlags = (curTick & TICK_VALUE_MASK) | DIRTY_BIT_MASK;
        }
        return *this;
    }

    void Set(const T& val, std::uint32_t currentTick = 0) noexcept {
        if (m_Value != val) {
            m_Value = val;
            m_nTickAndFlags = (currentTick & TICK_VALUE_MASK) | DIRTY_BIT_MASK;
        }
    }

    void MarkChanged(std::uint32_t currentTick = 0) noexcept {
        m_nTickAndFlags = (currentTick & TICK_VALUE_MASK) | DIRTY_BIT_MASK;
    }

    [[nodiscard]] constexpr std::uint32_t GetChangeTick() const noexcept {
        return m_nTickAndFlags & TICK_VALUE_MASK;
    }

    void SetChangeTick(std::uint32_t tick) noexcept {
        m_nTickAndFlags = (tick & TICK_VALUE_MASK) | (m_nTickAndFlags & DIRTY_BIT_MASK);
    }

    [[nodiscard]] constexpr bool IsDirty() const noexcept {
        return (m_nTickAndFlags & DIRTY_BIT_MASK) != 0;
    }

    void ClearDirty() noexcept {
        m_nTickAndFlags &= TICK_VALUE_MASK;
    }

    T& GetForModify(std::uint32_t currentTick = 0) noexcept {
        m_nTickAndFlags = (currentTick & TICK_VALUE_MASK) | DIRTY_BIT_MASK;
        return m_Value;
    }

    T m_Value{};
    std::uint32_t m_nTickAndFlags{0};
};

static_assert(sizeof(CNetworkVar<std::uint32_t>) == 0x08, "CNetworkVar<uint32_t> must be 8 bytes");
static_assert(sizeof(CNetworkVar<std::int32_t>) == 0x08, "CNetworkVar<int32_t> must be 8 bytes");
static_assert(sizeof(CNetworkVar<float>) == 0x08, "CNetworkVar<float> must be 8 bytes");
static_assert(sizeof(CNetworkVar<std::uint64_t>) == 0x10, "CNetworkVar<uint64_t> must be 16 bytes");

// ============================================================================
// 3. CNETMsg_Tick Protobuf Definition
// ============================================================================
// NetMessage ID: 0x04 (4)
// VTable RVA: networksystem.dll + 0x22c8d8
// Size: 0x48 (72 bytes), MSVC x64 ABI verified
struct CNETMsg_Tick : public protobuf::Message {
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_{0};                    // +0x14
    void*         expected_long_tick_reason{nullptr};         // +0x18: TaggedStringPtr / ArenaStringPtr (field 11)
    std::uint32_t tick{0};                                    // +0x20: field 1
    std::uint32_t host_computationtime{0};                    // +0x24: field 4
    std::uint32_t host_computationtime_std_deviation{0};      // +0x28: field 5
    std::uint32_t legacy_host_loss{0};                        // +0x2C: field 7
    std::uint32_t host_unfiltered_frametime{0};               // +0x30: field 8
    std::uint32_t hltv_replay_flags{0};                       // +0x34: field 9
    std::uint32_t expected_long_tick{0};                      // +0x38: field 10
    std::uint32_t host_frame_dropped_pct_x10{0};              // +0x3C: field 12
    std::uint32_t host_frame_irregular_arrival_pct_x10{0};    // +0x40: field 13
    std::uint32_t _pad44{0};                                  // +0x44: alignment padding
};
static_assert(sizeof(CNETMsg_Tick) == 0x48, "CNETMsg_Tick size must be 0x48 (72 bytes)");

// ============================================================================
// 4. CSVCMsg_PacketEntities & Sub-Messages
// ============================================================================
// Sub-Message: alternate_baseline_t
struct CSVCMsg_PacketEntities_alternate_baseline_t : public protobuf::Message {
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_{0};                    // +0x14
    std::int32_t  entity_index{0};                            // +0x18: field 1
    std::int32_t  baseline_index{0};                          // +0x1C: field 2
};
static_assert(sizeof(CSVCMsg_PacketEntities_alternate_baseline_t) == 0x20, "alternate_baseline_t size must be 0x20 (32 bytes)");

// Sub-Message: non_transmitted_entities_t
struct CSVCMsg_PacketEntities_non_transmitted_entities_t : public protobuf::Message {
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_{0};                    // +0x14
    void*         data{nullptr};                              // +0x18: ArenaStringPtr (field 2)
    std::int32_t  header_count{0};                           // +0x20: field 1
    std::uint32_t _pad24{0};                                  // +0x24: alignment padding
};
static_assert(sizeof(CSVCMsg_PacketEntities_non_transmitted_entities_t) == 0x28, "non_transmitted_entities_t size must be 0x28 (40 bytes)");

// Sub-Message: outofpvs_entity_updates_t
struct CSVCMsg_PacketEntities_outofpvs_entity_updates_t : public protobuf::Message {
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_{0};                    // +0x14
    void*         data{nullptr};                              // +0x18: ArenaStringPtr (field 2)
    std::int32_t  count{0};                                  // +0x20: field 1
    std::uint32_t _pad24{0};                                  // +0x24: alignment padding
};
static_assert(sizeof(CSVCMsg_PacketEntities_outofpvs_entity_updates_t) == 0x28, "outofpvs_entity_updates_t size must be 0x28 (40 bytes)");

// NetMessage ID: 0x37 (55)
// VTable RVA: networksystem.dll + 0x2267b8
// Size: 0xA8 (168 bytes), MSVC x64 ABI verified
struct CSVCMsg_PacketEntities : public protobuf::Message {
    protobuf::internal::InternalMetadata _internal_metadata_; // +0x08
    std::uint32_t _has_bits_[1];                              // +0x10
    mutable std::int32_t _cached_size_{0};                    // +0x14
    void* entity_data{nullptr};                               // +0x18: ArenaStringPtr (field 7)
    void* serialized_entities{nullptr};                       // +0x20: ArenaStringPtr (field 13)
    void* dev_padding{nullptr};                               // +0x28: ArenaStringPtr (field 1000)
    protobuf::RepeatedPtrField<CSVCMsg_PacketEntities_alternate_baseline_t> alternate_baselines; // +0x30..0x48 (field 15)
    void* cmd_recv_status_rep{nullptr};                       // +0x48: RepeatedField internal rep (field 22)
    std::int32_t cmd_recv_status_current{0};                  // +0x50
    std::int32_t cmd_recv_status_total{0};                    // +0x54
    void* cmd_recv_status_arena{nullptr};                     // +0x58
    CSVCMsg_PacketEntities_non_transmitted_entities_t* non_transmitted_entities{nullptr}; // +0x60 (field 19)
    CSVCMsg_PacketEntities_outofpvs_entity_updates_t*  outofpvs_entity_updates{nullptr};   // +0x68 (field 23)
    std::int32_t  max_entries{0};                             // +0x70 (field 1)
    std::int32_t  updated_entries{0};                         // +0x74 (field 2)
    std::int32_t  baseline{0};                                // +0x78 (field 5)
    bool          legacy_is_delta{false};                     // +0x7C (field 3)
    bool          update_baseline{false};                     // +0x7D (field 4)
    bool          pending_full_frame{false};                  // +0x7E (field 8)
    std::uint8_t  _pad7F{0};                                  // +0x7F
    std::int32_t  delta_from{0};                              // +0x80 (field 6)
    std::uint32_t active_spawngroup_handle{0};                // +0x84 (field 9)
    std::uint32_t max_spawngroup_creationsequence{0};         // +0x88 (field 10)
    std::uint32_t last_cmd_number_executed{0};                // +0x8C (field 11)
    std::uint32_t server_tick{0};                             // +0x90 (field 12)
    std::uint32_t has_pvs_vis_bits_deprecated{0};             // +0x94 (field 16)
    std::int32_t  last_cmd_number_recv_delta{0};              // +0x98 (field 17)
    std::uint32_t cq_starved_command_ticks{0};                // +0x9C (field 20)
    std::uint32_t cq_discarded_command_ticks{0};              // +0xA0 (field 21)
    std::uint32_t _padA4{0};                                  // +0xA4: alignment padding
};
static_assert(sizeof(CSVCMsg_PacketEntities) == 0xA8, "CSVCMsg_PacketEntities size must be 0xA8 (168 bytes)");

} // namespace cs2::sdk
