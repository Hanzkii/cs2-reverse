#ifndef CS2_INTERFACES_CVAR_H
#define CS2_INTERFACES_CVAR_H

#include <cstdint>
#include <cstddef>
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

    [[nodiscard]] const char* GetName() const noexcept { return m_pszName; }
    [[nodiscard]] const char* GetHelpText() const noexcept { return m_pszDescription; }
    [[nodiscard]] bool HasFlag(std::uint32_t flag) const noexcept { return (m_nFlags & flag) != 0; }
};

/**
 * @brief Source 2 ConVar & ConCommand Registry Interface (CCvar / IEngineCVar)
 * Token: "VEngineCvar007" in tier0.dll
 * VTable: tier0.dll + 0x313208 (46 virtual slots)
 */
class IEngineCVar {
public:
    // Slot 0: Virtual destructor
    virtual ~IEngineCVar() = default;

    // Slot 1: IAppSystem::Connect
    virtual bool Connect(void* pFactory) = 0;

    // Slot 2: IAppSystem::Disconnect
    virtual void Disconnect() = 0;

    // Slot 3: IAppSystem::QueryInterface
    virtual void* QueryInterface(const char* pInterfaceName) = 0;

    // Slot 4: IAppSystem::Init
    virtual int Init() = 0;

    // Slot 5: IAppSystem::Shutdown
    virtual void Shutdown() = 0;

    // Virtual slots 6 to 10 (Internal / Unmapped)
    virtual void Unk06() = 0; virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0;

    // Slot 11: Finds ConVar by name string
    virtual ConVar* FindVar(const char* pszVarName) = 0;

    // Slot 12: Finds ConVar by internal hash/registration index
    virtual ConVar* FindVarByIndex(std::int32_t nIndex) = 0;

    // Slot 13: Finds ConCommand by name string
    virtual void* FindCommandByName(const char* pszCommandName) = 0;

    // Virtual slots 14 to 40 (Internal / Unmapped)
    virtual void Unk14() = 0; virtual void Unk15() = 0; virtual void Unk16() = 0; virtual void Unk17() = 0;
    virtual void Unk18() = 0; virtual void Unk19() = 0; virtual void Unk20() = 0; virtual void Unk21() = 0;
    virtual void Unk22() = 0; virtual void Unk23() = 0; virtual void Unk24() = 0; virtual void Unk25() = 0;
    virtual void Unk26() = 0; virtual void Unk27() = 0; virtual void Unk28() = 0; virtual void Unk29() = 0;
    virtual void Unk30() = 0; virtual void Unk31() = 0; virtual void Unk32() = 0; virtual void Unk33() = 0;
    virtual void Unk34() = 0; virtual void Unk35() = 0; virtual void Unk36() = 0; virtual void Unk37() = 0;
    virtual void Unk38() = 0; virtual void Unk39() = 0; virtual void Unk40() = 0;

    // Slot 41: Returns first ConVar iterator handle
    virtual std::uint64_t GetFirstCVarArrayValue() = 0;

    // Slot 42: Returns next ConVar iterator handle
    virtual std::uint64_t GetNextCVarArrayValue() = 0;

    virtual void Unk43() = 0; // Slot 43
    virtual void Unk44() = 0; // Slot 44
    virtual void Unk45() = 0; // Slot 45


    // Convenience Accessors
    [[nodiscard]] ConVar* FindVarByName(const char* pszVarName) { return FindVar(pszVarName); }
    [[nodiscard]] ConVar* FindVarByToken(tier0::CUtlStringToken token) { return FindVarByIndex(static_cast<std::int32_t>(token.m_nHashCode)); }

};
static_assert(sizeof(IEngineCVar) == 0x08, "IEngineCVar interface pointer must be 8 bytes");

using CCvar = IEngineCVar;

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_CVAR_H
