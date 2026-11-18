#pragma once

// ============================================================================
// Source 2 Reverse Engineered SDK - Panorama V8 JavaScript & Web Overlay Bridge
// Target Binaries: panorama.dll, client.dll (Build: 2026-10-25_26144732)
// Architecture: x86-64 (MSVC ABI)
// Language Standard: C++20 (/std:c++20 /W4 /WX)
//
// Host Objects & APIs:
//   - $ (Global DOM & scheduler)
//   - DemoAPI (Sub-tick playback controls & scrubbing)
//   - GameEvents (Authoritative game event subscription)
//   - GameInterfaceAPI (Console & ConVar execution)
//   - Web Overlay Bridge (Real-time telemetry streaming to external web browsers)
// ============================================================================

#include "cuipanel.hpp"
#include "event_pipeline.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <sstream>
#include <memory>
#include <mutex>

namespace cs2::panorama {

// ============================================================================
// 1. V8 Script Context & Host Object Bindings
// ============================================================================
class CDemoAPIHost {
public:
    virtual ~CDemoAPIHost() = default;

    [[nodiscard]] virtual std::int32_t GetPlaybackTick() const noexcept = 0;
    [[nodiscard]] virtual std::int32_t GetTotalTicks() const noexcept = 0;
    virtual void SetPlaybackTick(std::int32_t nTick) = 0;
    virtual void GotoNextRound() = 0;
    virtual void GotoPreviousRound() = 0;
    [[nodiscard]] virtual bool IsPlaybackPaused() const noexcept = 0;
    virtual void PausePlayback() = 0;
    virtual void ResumePlayback() = 0;
    [[nodiscard]] virtual float GetTimescale() const noexcept = 0;
    virtual void SetTimescale(float flScale) = 0;
    [[nodiscard]] virtual std::string GetDemoFilename() const = 0;
    [[nodiscard]] virtual std::string GetMapName() const = 0;
    [[nodiscard]] virtual std::int32_t GetDemoProtocol() const noexcept = 0;
};

class CGameInterfaceAPIHost {
public:
    virtual ~CGameInterfaceAPIHost() = default;

    virtual void ConsoleExec(std::string_view svCommand) = 0;
    [[nodiscard]] virtual std::string GetSettingString(std::string_view svConVar) const = 0;
    [[nodiscard]] virtual float GetSettingFloat(std::string_view svConVar) const = 0;
};

class CMyPersonaAPIHost {
public:
    virtual ~CMyPersonaAPIHost() = default;

    [[nodiscard]] virtual std::uint64_t GetXuid() const noexcept = 0;
    [[nodiscard]] virtual std::string GetName() const = 0;
};

class CMatchStatsAPIHost {
public:
    virtual ~CMatchStatsAPIHost() = default;

    [[nodiscard]] virtual std::int32_t GetRoundScore(std::int32_t nTeam) const noexcept = 0;
};

// ============================================================================
// 2. Real-Time Web Overlay Bridge (JSON Telemetry Streamer)
// Connects native CS2 engine state to external HTML5/CSS3/JS overlays
// ============================================================================
struct SubTickTelemetryFrame_t {
    std::int32_t   nTick{0};
    float          flSubTickFraction{0.0f}; // 0.0 .. 1.0 (15.625ms slice)
    float          flLatencyMs{0.0f};
    std::int32_t   nHealth{100};
    std::int32_t   nArmor{100};
    std::string    strActiveWeapon{"weapon_ak47"};
    std::int32_t   nClipAmmo{30};
    std::int32_t   nReserveAmmo{90};
    std::int32_t   nCTScore{0};
    std::int32_t   nTScore{0};
    float          flRoundTimeRemaining{115.0f};
    bool           bBombPlanted{false};
    float          flBombTimeRemaining{40.0f};
};

class CWebOverlayBridge {
public:
    using MessageOutHandler_t = std::function<void(std::string_view jsonPayload)>;

    CWebOverlayBridge() = default;
    ~CWebOverlayBridge() = default;

    void SetBroadcastHandler(MessageOutHandler_t handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_broadcastHandler = std::move(handler);
    }

    /**
     * @brief Formats a Sub-Tick telemetry frame as standard JSON and broadcasts it.
     */
    void BroadcastTelemetry(const SubTickTelemetryFrame_t& frame) {
        std::string json = SerializeTelemetryJSON(frame);
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_broadcastHandler) {
            m_broadcastHandler(json);
        }
    }

    /**
     * @brief Dispatches a game event (e.g. player_hurt, killfeed) to connected web overlays.
     */
    void BroadcastGameEvent(std::string_view svEventName, const GameEventPayload_t& payload) {
        std::ostringstream ss;
        ss << "{\"type\":\"game_event\",\"event\":\"" << svEventName << "\",\"data\":{";
        bool first = true;
        for (const auto& [k, v] : payload) {
            if (!first) ss << ",";
            first = false;
            ss << "\"" << k << "\":";
            switch (v.m_type) {
            case GameEventKeyValue_t::EType::String:
                ss << "\"" << v.m_strVal << "\"";
                break;
            case GameEventKeyValue_t::EType::Float:
                ss << v.m_flVal;
                break;
            case GameEventKeyValue_t::EType::Int32:
                ss << v.m_nIntVal;
                break;
            case GameEventKeyValue_t::EType::UInt64:
                ss << v.m_nUInt64Val;
                break;
            case GameEventKeyValue_t::EType::Bool:
                ss << (v.m_bVal ? "true" : "false");
                break;
            }
        }
        ss << "}}";

        std::string json = ss.str();
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_broadcastHandler) {
            m_broadcastHandler(json);
        }
    }

    /**
     * @brief Ingests incoming commands from web overlays (e.g. scrubber clicks).
     */
    void HandleIncomingWebMessage(std::string_view svMessage, CDemoAPIHost* pDemoAPI, CGameInterfaceAPIHost* pGameInterface) {
        if (!pDemoAPI || svMessage.empty()) return;

        // Minimal zero-dependency JSON parser for standard command payloads
        if (svMessage.find("\"action\":\"goto_tick\"") != std::string_view::npos) {
            size_t tickPos = svMessage.find("\"tick\":");
            if (tickPos != std::string_view::npos) {
                int tick = std::atoi(svMessage.data() + tickPos + 7);
                pDemoAPI->SetPlaybackTick(tick);
            }
        } else if (svMessage.find("\"action\":\"pause\"") != std::string_view::npos) {
            pDemoAPI->PausePlayback();
        } else if (svMessage.find("\"action\":\"resume\"") != std::string_view::npos) {
            pDemoAPI->ResumePlayback();
        } else if (svMessage.find("\"action\":\"timescale\"") != std::string_view::npos) {
            size_t scalePos = svMessage.find("\"scale\":");
            if (scalePos != std::string_view::npos) {
                float scale = static_cast<float>(std::atof(svMessage.data() + scalePos + 8));
                pDemoAPI->SetTimescale(scale);
            }
        } else if (svMessage.find("\"action\":\"con_command\"") != std::string_view::npos && pGameInterface) {
            size_t cmdPos = svMessage.find("\"cmd\":\"");
            if (cmdPos != std::string_view::npos) {
                size_t start = cmdPos + 7;
                size_t end = svMessage.find("\"", start);
                if (end != std::string_view::npos) {
                    std::string cmd(svMessage.substr(start, end - start));
                    pGameInterface->ConsoleExec(cmd);
                }
            }
        }
    }

    [[nodiscard]] static std::string SerializeTelemetryJSON(const SubTickTelemetryFrame_t& f) {
        std::ostringstream ss;
        ss << "{"
           << "\"type\":\"telemetry\","
           << "\"tick\":" << f.nTick << ","
           << "\"subtick_fraction\":" << f.flSubTickFraction << ","
           << "\"latency_ms\":" << f.flLatencyMs << ","
           << "\"health\":" << f.nHealth << ","
           << "\"armor\":" << f.nArmor << ","
           << "\"weapon\":\"" << f.strActiveWeapon << "\","
           << "\"clip\":" << f.nClipAmmo << ","
           << "\"reserve\":" << f.nReserveAmmo << ","
           << "\"score_ct\":" << f.nCTScore << ","
           << "\"score_t\":" << f.nTScore << ","
           << "\"round_time\":" << f.flRoundTimeRemaining << ","
           << "\"bomb_planted\":" << (f.bBombPlanted ? "true" : "false") << ","
           << "\"bomb_time\":" << f.flBombTimeRemaining
           << "}";
        return ss.str();
    }

private:
    std::mutex          m_mutex;
    MessageOutHandler_t m_broadcastHandler;
};

} // namespace cs2::panorama
