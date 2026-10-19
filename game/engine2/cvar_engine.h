#ifndef CS2_ENGINE2_CVAR_ENGINE_H
#define CS2_ENGINE2_CVAR_ENGINE_H

#include <cstdint>
#include <string>
#include <unordered_map>

namespace cs2::engine2 {

// ConVar Bit Flags verified from engine2.dll @ 0x1800825e0
constexpr uint32_t FCVAR_NONE               = 0;
constexpr uint32_t FCVAR_UNREGISTERED       = (1 << 0);
constexpr uint32_t FCVAR_DEVELOPMENTONLY    = (1 << 1);
constexpr uint32_t FCVAR_GAMEDLL            = (1 << 2);
constexpr uint32_t FCVAR_CLIENTDLL          = (1 << 3);
constexpr uint32_t FCVAR_HIDDEN             = (1 << 4);
constexpr uint32_t FCVAR_PROTECTED          = (1 << 5);
constexpr uint32_t FCVAR_NOTIFY             = (1 << 8);
constexpr uint32_t FCVAR_REPLICATED         = (1 << 13); // Bit 13 / 0x2000: Server authoritative
constexpr uint32_t FCVAR_CHEAT              = (1 << 14); // Bit 14 / 0x4000: Protected by sv_cheats

struct ConVarRecord {
    std::string name;
    std::string value;
    uint32_t flags = 0;
    std::string helpString;
};

/**
 * @brief Reconstructed CCvar Engine Subsystem from engine2.dll
 * Implements FCVAR security gates, caseless string hashing, and authoritative synchronization.
 */
class CCvarEngine {
public:
    static CCvarEngine& Get();

    void RegisterConVar(const std::string& name, const std::string& defaultValue, uint32_t flags, const std::string& help);
    [[nodiscard]] const ConVarRecord* FindConVar(const std::string& name) const;

    bool SetClientConVar(const std::string& name, const std::string& newValue, bool isServerCall);
    [[nodiscard]] bool IsCheatProtected(const std::string& name) const;
    [[nodiscard]] bool IsReplicated(const std::string& name) const;

    static uint32_t HashConVarName(const std::string& name) noexcept;

private:
    CCvarEngine();
    std::unordered_map<std::string, ConVarRecord> m_ConVars;
};

} // namespace cs2::engine2

#endif // CS2_ENGINE2_CVAR_ENGINE_H
