#include "cvar_engine.h"
#include <cctype>

namespace cs2::engine2 {

CCvarEngine::CCvarEngine() {
    RegisterConVar("sv_cheats", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Allows cheats on server.");
    RegisterConVar("sv_friction", "5.2", FCVAR_REPLICATED, "World ground friction scalar.");
    RegisterConVar("cl_interp", "0.015625", FCVAR_CLIENTDLL, "Client interpolation duration.");
}

CCvarEngine& CCvarEngine::Get() {
    static CCvarEngine s_Instance;
    return s_Instance;
}

uint32_t CCvarEngine::HashConVarName(const std::string& name) noexcept {
    // Caseless 32-bit FNV-1a hash matching engine2.dll seed 0x31415926
    uint32_t hash = 0x31415926;
    for (char c : name) {
        hash ^= static_cast<uint8_t>(std::tolower(static_cast<unsigned char>(c)));
        hash *= 0x01000193;
    }
    return hash;
}

void CCvarEngine::RegisterConVar(const std::string& name, const std::string& defaultValue, uint32_t flags, const std::string& help) {
    m_ConVars[name] = { name, defaultValue, flags, help };
}

const ConVarRecord* CCvarEngine::FindConVar(const std::string& name) const {
    auto it = m_ConVars.find(name);
    return it != m_ConVars.end() ? &it->second : nullptr;
}

bool CCvarEngine::IsCheatProtected(const std::string& name) const {
    const auto* cvar = FindConVar(name);
    return cvar && (cvar->flags & FCVAR_CHEAT) != 0;
}

bool CCvarEngine::IsReplicated(const std::string& name) const {
    const auto* cvar = FindConVar(name);
    return cvar && (cvar->flags & FCVAR_REPLICATED) != 0;
}

bool CCvarEngine::SetClientConVar(const std::string& name, const std::string& newValue, bool isServerCall) {
    auto it = m_ConVars.find(name);
    if (it == m_ConVars.end()) {
        return false;
    }

    // Gatekeeper 1: FCVAR_REPLICATED (Bit 13) cannot be modified by client unless pushed by server
    if ((it->second.flags & FCVAR_REPLICATED) && !isServerCall) {
        return false; // Unauthorized client overwrite rejected
    }

    // Gatekeeper 2: FCVAR_CHEAT (Bit 14) requires sv_cheats == "1"
    if ((it->second.flags & FCVAR_CHEAT)) {
        const auto* svCheats = FindConVar("sv_cheats");
        if (!svCheats || svCheats->value != "1") {
            return false; // Cheats are not enabled
        }
    }

    it->second.value = newValue;
    return true;
}

} // namespace cs2::engine2
