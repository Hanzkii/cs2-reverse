#ifndef CS2_INTERFACES_CVAR_H
#define CS2_INTERFACES_CVAR_H

#include <cstdint>
#include <string_view>
#include "../tier0/utl_string_token.h"

namespace cs2::interfaces {

// Source 2 ConVar Security & Replication Flags
namespace FCVAR {
    constexpr std::uint32_t UNREGISTERED              = (1 << 0);
    constexpr std::uint32_t DEVELOPMENTONLY           = (1 << 1);
    constexpr std::uint32_t GAMEDLL                   = (1 << 2);
    constexpr std::uint32_t CLIENTDLL                 = (1 << 3);
    constexpr std::uint32_t HIDDEN                    = (1 << 4);
    constexpr std::uint32_t PROTECTED                 = (1 << 5);
    constexpr std::uint32_t SPONLY                    = (1 << 6);
    constexpr std::uint32_t ARCHIVE                   = (1 << 7);
    constexpr std::uint32_t NOTIFY                    = (1 << 8);
    constexpr std::uint32_t USERINFO                  = (1 << 9);
    constexpr std::uint32_t CHEAT                     = (1 << 14);
    constexpr std::uint32_t REPLICATED                = (1 << 29);
    constexpr std::uint32_t DEMO                      = (1 << 16);
    constexpr std::uint32_t DONTRECORD                = (1 << 17);
    constexpr std::uint32_t RELEASE                   = (1 << 19);
}

/**
 * @brief ConVar Raw Descriptor (Source 2)
 */
class ConVar {
public:
    const char*   m_pszName{nullptr};        // +0x00: Primary convar identifier
    ConVar*       m_pNext{nullptr};          // +0x08: Next convar node in tier0 hash table
    const char*   m_pszDescription{nullptr}; // +0x10: Descriptive documentation
    std::uint32_t m_nFlags{0};               // +0x18: FCVAR flags bitmask

    // Accessors
    [[nodiscard]] const char* GetName() const noexcept { return m_pszName; }
    [[nodiscard]] const char* GetHelpText() const noexcept { return m_pszDescription; }
    [[nodiscard]] bool HasFlag(std::uint32_t flag) const noexcept { return (m_nFlags & flag) != 0; }
};

/**
 * @brief Engine ConVar Master Interface (CCvar / ICvar)
 * Token: "VEngineCvar007" in tier0.dll
 * VTable RVA: +0x313208 (48 slots)
 */
class CCvar {
public:
    virtual ~CCvar() = default;

    // Slot 12: FindVarByName
    virtual ConVar* FindVarByName(const char* pszVarName) = 0;

    // Slot 13: FindVarByToken
    virtual ConVar* FindVarByToken(tier0::CUtlStringToken token) = 0;

    // Slot 14: FindCommandByName
    virtual void* FindCommandByName(const char* pszCommandName) = 0;

    // Slot 27: GetFirstConVar
    virtual ConVar* GetFirstConVar() = 0;

    // Slot 28: GetNextConVar
    virtual ConVar* GetNextConVar(ConVar* pPrev) = 0;
};

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_CVAR_H
