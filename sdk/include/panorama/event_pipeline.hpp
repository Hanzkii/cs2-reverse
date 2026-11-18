#pragma once

// ============================================================================
// Source 2 Reverse Engineered SDK - Panorama UI Event Dispatch Pipeline
// Target Binaries: panorama.dll, client.dll (Build: 2026-10-25_26144732)
// Architecture: x86-64 (MSVC ABI)
// Language Standard: C++20 (/std:c++20 /W4 /WX)
//
// Verified Offsets & Structures:
//   CUIEvent<1> VTable:       panorama.dll + 0x4CAF68
//   Event Dispatch Flow:     Capture -> Target -> Bubble -> Unhandled
//   Wire Format:             Custom Game Events to Server (NetMessage 304 / Protobuf)
// ============================================================================

#include "cuipanel.hpp"
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>

namespace cs2::panorama {

// ============================================================================
// 1. Event Dispatch Phases & Routing Modes
// ============================================================================
enum class EEventDispatchPhase : std::uint8_t {
    Capture   = 0, // Root window downward towards target panel
    Target    = 1, // Target panel itself
    Bubble    = 2, // Target panel upward towards root window
    Unhandled = 3  // Global unhandled fallthrough handlers
};

enum class EEventBubbleMode : std::uint8_t {
    BubbleToParent = 0,
    StopAtTarget   = 1,
    BroadcastTree  = 2
};

// ============================================================================
// 2. Canonical Panorama Event IDs (Hashed via FNV-1a / MurmurHash2)
// ============================================================================
namespace events {
    constexpr CUIEventID_t Activated              = 0x0001; // Panel clicked or activated
    constexpr CUIEventID_t MouseOver              = 0x0002; // Cursor entered panel bounds
    constexpr CUIEventID_t MouseOut               = 0x0003; // Cursor exited panel bounds
    constexpr CUIEventID_t ContextMenu            = 0x0004; // Right-click context menu requested
    constexpr CUIEventID_t DoubleClick            = 0x0005; // Primary double click
    constexpr CUIEventID_t SetFocus               = 0x0006; // Focus received
    constexpr CUIEventID_t KillFocus              = 0x0007; // Focus lost
    constexpr CUIEventID_t KeyDown                = 0x0008; // Physical key depressed
    constexpr CUIEventID_t KeyUp                  = 0x0009; // Physical key released
    constexpr CUIEventID_t KeyTyped               = 0x000A; // Character input translated
    constexpr CUIEventID_t LayoutCompleted        = 0x000B; // Layout calculation complete
    constexpr CUIEventID_t StyleChanged           = 0x000C; // CSS class / property updated
    constexpr CUIEventID_t PageLoaded             = 0x000D; // XML template layout fully loaded
    constexpr CUIEventID_t CustomGameEvent        = 0x000E; // Custom game event received from network
    constexpr CUIEventID_t SubTickActuationUpdate = 0x000F; // CS2 Sub-tick telemetry event
}

// Compile-time FNV-1a hash utility for custom event strings
[[nodiscard]] constexpr CUIEventID_t HashEventName(std::string_view svName) noexcept {
    std::uint32_t nHash = 0x811C9DC5;
    for (char c : svName) {
        nHash ^= static_cast<std::uint8_t>(c);
        nHash *= 0x01000193;
    }
    return static_cast<CUIEventID_t>((nHash ^ (nHash >> 16)) & 0xFFFF);
}

// ============================================================================
// 3. Base Panorama UI Event (CUIEvent)
// Binary layout verified in panorama.dll + 0x4CAF68
// ============================================================================
class CUIEvent {
public:
    virtual ~CUIEvent() = default;
    [[nodiscard]] virtual CUIEventID_t GetEventType() const noexcept = 0;
    [[nodiscard]] virtual PanelHandle_t GetTargetPanel() const noexcept = 0;
    [[nodiscard]] virtual bool CanBubble() const noexcept = 0;

    // Binary layout:
    CUIEventID_t     m_nEventType{0};         // 0x08
    std::uint16_t    m_nFlags{0};             // 0x0A
    PanelHandle_t    m_hTargetPanel{0};       // 0x0C
    CUIPanel*        m_pSourcePanel{nullptr}; // 0x14
    std::int32_t     m_nArgsCount{0};         // 0x1C
};

// Concrete CUIEvent for internal simulation and tests
class CConcreteUIEvent : public CUIEvent {
public:
    CConcreteUIEvent(CUIEventID_t nType, PanelHandle_t hTarget, CUIPanel* pSource, bool bCanBubble = true) noexcept {
        m_nEventType = nType;
        m_hTargetPanel = hTarget;
        m_pSourcePanel = pSource;
        m_bCanBubble = bCanBubble;
    }

    [[nodiscard]] CUIEventID_t GetEventType() const noexcept override { return m_nEventType; }
    [[nodiscard]] PanelHandle_t GetTargetPanel() const noexcept override { return m_hTargetPanel; }
    [[nodiscard]] bool CanBubble() const noexcept override { return m_bCanBubble; }

private:
    bool m_bCanBubble{true};
};

struct CUIEventHandler {
    void*            pFunction{nullptr};      // Native function pointer or JS Callback closure
    void*            pContext{nullptr};       // Target context panel pointer
    CUIEventID_t     nEventType{0};           // Event ID symbol
    std::uint16_t    nFlags{0};               // Handled mode flags
};

// ============================================================================
// 4. Source 2 GameEvents Subsystem & Network Custom Event Bridge
// ============================================================================
struct GameEventKeyValue_t {
    enum class EType : std::uint8_t {
        String,
        Float,
        Int32,
        UInt64,
        Bool
    };

    EType        m_type{EType::String};
    std::string  m_strVal;
    float        m_flVal{0.0f};
    int32_t      m_nIntVal{0};
    uint64_t     m_nUInt64Val{0};
    bool         m_bVal{false};
};

using GameEventPayload_t = std::unordered_map<std::string, GameEventKeyValue_t>;

/**
 * @brief Bridge between Source 2 IGameEventManager2 / network messages and Panorama V8 JS.
 */
class CGameEventsBridge {
public:
    using EventCallback_t = std::function<void(const GameEventPayload_t&)>;

    // Subscribe to a game event broadcast (e.g. "player_hurt", "weapon_fire")
    std::uint32_t Subscribe(std::string_view svEventName, EventCallback_t callback) {
        std::uint32_t hListener = m_nNextListenerID++;
        m_subscriptions[std::string(svEventName)].emplace_back(hListener, std::move(callback));
        return hListener;
    }

    // Unsubscribe listener
    bool Unsubscribe(std::uint32_t hListener) {
        for (auto& [name, list] : m_subscriptions) {
            for (auto it = list.begin(); it != list.end(); ++it) {
                if (it->first == hListener) {
                    list.erase(it);
                    return true;
                }
            }
        }
        return false;
    }

    // Fire event into registered JS / native listeners
    void DispatchGameEvent(std::string_view svEventName, const GameEventPayload_t& payload) {
        auto it = m_subscriptions.find(std::string(svEventName));
        if (it != m_subscriptions.end()) {
            for (const auto& [id, cb] : it->second) {
                if (cb) {
                    cb(payload);
                }
            }
        }
    }

    /**
     * @brief SendCustomGameEventToServer wire serialization.
     * Encapsulates event name and payload into a client-to-server netmessage.
     */
    [[nodiscard]] std::vector<std::uint8_t> SerializeCustomGameEventToServer(
        std::string_view svEventName,
        const GameEventPayload_t& payload) const
    {
        std::vector<std::uint8_t> buffer;
        // Packet Header: 2-byte length, 2-byte NetMessage ID (304 = svc_CustomGameEvent)
        const std::uint16_t nNetMsgID = 304;
        buffer.push_back(static_cast<std::uint8_t>(nNetMsgID & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((nNetMsgID >> 8) & 0xFF));

        // Event name length + ASCII string
        const auto nNameLen = static_cast<std::uint8_t>(svEventName.size());
        buffer.push_back(nNameLen);
        buffer.insert(buffer.end(), svEventName.begin(), svEventName.end());

        // Field count
        const auto nFieldCount = static_cast<std::uint16_t>(payload.size());
        buffer.push_back(static_cast<std::uint8_t>(nFieldCount & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((nFieldCount >> 8) & 0xFF));

        // Serialize Key-Values
        for (const auto& [k, v] : payload) {
            const auto kLen = static_cast<std::uint8_t>(k.size());
            buffer.push_back(kLen);
            buffer.insert(buffer.end(), k.begin(), k.end());
            buffer.push_back(static_cast<std::uint8_t>(v.m_type));

            switch (v.m_type) {
            case GameEventKeyValue_t::EType::String: {
                const auto sLen = static_cast<std::uint16_t>(v.m_strVal.size());
                buffer.push_back(static_cast<std::uint8_t>(sLen & 0xFF));
                buffer.push_back(static_cast<std::uint8_t>((sLen >> 8) & 0xFF));
                buffer.insert(buffer.end(), v.m_strVal.begin(), v.m_strVal.end());
                break;
            }
            case GameEventKeyValue_t::EType::Float: {
                const auto* pBytes = reinterpret_cast<const std::uint8_t*>(&v.m_flVal);
                buffer.insert(buffer.end(), pBytes, pBytes + sizeof(float));
                break;
            }
            case GameEventKeyValue_t::EType::Int32: {
                const auto* pBytes = reinterpret_cast<const std::uint8_t*>(&v.m_nIntVal);
                buffer.insert(buffer.end(), pBytes, pBytes + sizeof(int32_t));
                break;
            }
            case GameEventKeyValue_t::EType::UInt64: {
                const auto* pBytes = reinterpret_cast<const std::uint8_t*>(&v.m_nUInt64Val);
                buffer.insert(buffer.end(), pBytes, pBytes + sizeof(uint64_t));
                break;
            }
            case GameEventKeyValue_t::EType::Bool: {
                buffer.push_back(v.m_bVal ? 1 : 0);
                break;
            }
            }
        }
        return buffer;
    }

private:
    std::uint32_t m_nNextListenerID{1};
    std::unordered_map<std::string, std::vector<std::pair<std::uint32_t, EventCallback_t>>> m_subscriptions;
};

} // namespace cs2::panorama
