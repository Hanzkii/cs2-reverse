#ifndef CS2_INTERFACES_SCHEMA_INTERFACE_H
#define CS2_INTERFACES_SCHEMA_INTERFACE_H

#include <cstdint>
#include <cstddef>
#include "../tier0/utl_string_token.h"

namespace cs2::interfaces {

#define CS2_PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

struct SchemaClassFieldData_t {
    const char*   m_pszName{nullptr};        // +0x00: Field identifier string
    void*         m_pType{nullptr};          // +0x08: Schema type descriptor
    std::int32_t  m_nSingleInheritanceOffset{0}; // +0x10: Exact byte offset within class
    std::int32_t  m_nMetadataSize{0};        // +0x14: Metadata attribute count
    void*         m_pMetadata{nullptr};      // +0x18: Schema metadata array pointer
};
static_assert(sizeof(SchemaClassFieldData_t) == 0x20, "SchemaClassFieldData_t must be 32 bytes (0x20)");

struct CSchemaClassInfo {
    void*                   m_pSelf{nullptr};         // +0x00
    const char*             m_pszName{nullptr};       // +0x08: Class technical name
    const char*             m_pszModuleName{nullptr}; // +0x10: Owning binary module name
    std::int32_t            m_nSize{0};               // +0x18: Total instance byte size
    std::int16_t            m_nFieldsCount{0};        // +0x1C: Field member count
    CS2_PAD(0x0A, m_pad1e);
    SchemaClassFieldData_t* m_pFields{nullptr};       // +0x28: Field descriptors array
    CS2_PAD(0x38, m_pad30);
};
static_assert(sizeof(CSchemaClassInfo) == 0x68, "CSchemaClassInfo must be 104 bytes (0x68)");

/**
 * @brief SchemaSystem Module Scope Class (CSchemaSystemTypeScope)
 * VTable: schemasystem.dll + 0x586f0 (35 slots)
 */
class CSchemaSystemTypeScope {
public:
    virtual ~CSchemaSystemTypeScope() = default;                                                 // Slot 0
    virtual const char* GetScopeName() const = 0;                                               // Slot 1
    virtual CSchemaClassInfo* FindDeclaredClass(const char* pszClassName) = 0;                  // Slot 2
    virtual void* FindDeclaredEnum(const char* pszEnumName) = 0;                                // Slot 3

    // Virtual slots 4 to 23
    virtual void Unk04() = 0; virtual void Unk05() = 0; virtual void Unk06() = 0; virtual void Unk07() = 0;
    virtual void Unk08() = 0; virtual void Unk09() = 0; virtual void Unk10() = 0; virtual void Unk11() = 0;
    virtual void Unk12() = 0; virtual void Unk13() = 0; virtual void Unk14() = 0; virtual void Unk15() = 0;
    virtual void Unk16() = 0; virtual void Unk17() = 0; virtual void Unk18() = 0; virtual void Unk19() = 0;
    virtual void Unk20() = 0; virtual void Unk21() = 0; virtual void Unk22() = 0; virtual void Unk23() = 0;

    virtual CSchemaClassInfo* FindDeclaredClassByToken(std::uint32_t nToken) = 0;               // Slot 24
    virtual CSchemaClassInfo* FindDeclaredClassByName(const char* pszClassName) = 0;            // Slot 25
    virtual void* FindDeclaredEnumByToken(std::uint32_t nToken) = 0;                            // Slot 26
    virtual void* FindDeclaredEnumByName(const char* pszEnumName) = 0;                          // Slot 27

    // Virtual slots 28 to 34
    virtual void Unk28() = 0; virtual void Unk29() = 0; virtual void Unk30() = 0; virtual void Unk31() = 0;
    virtual void Unk32() = 0; virtual void Unk33() = 0; virtual void Unk34() = 0;
};
static_assert(sizeof(CSchemaSystemTypeScope) == 0x08, "CSchemaSystemTypeScope pointer must be 8 bytes");

/**
 * @brief Source 2 Runtime Reflection SchemaSystem Subsystem
 * Token: "SchemaSystem_001" in schemasystem.dll
 * VTable: schemasystem.dll + 0x58598 (40 virtual slots)
 */
class ISchemaSystem {
public:
    // Slot 0: Virtual destructor
    virtual ~ISchemaSystem() = default;

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

    // Virtual slots 6 to 12 (Internal / Unmapped)
    virtual void Unk06() = 0; virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0; virtual void Unk11() = 0; virtual void Unk12() = 0;

    // Slot 13: Resolves module scope by name (e.g. 'client.dll')
    virtual CSchemaSystemTypeScope* FindTypeScopeForModule(const char* pszModuleName, void* pBindingContext = nullptr) = 0;

    // Slot 14: Resolves module scope by MurmurHash2 token
    virtual CSchemaSystemTypeScope* FindTypeScopeForModuleByToken(std::uint32_t nScopeHash) = 0;

    // Virtual slots 15 to 24 (Internal / Unmapped)
    virtual void Unk15() = 0; virtual void Unk16() = 0; virtual void Unk17() = 0; virtual void Unk18() = 0;
    virtual void Unk19() = 0; virtual void Unk20() = 0; virtual void Unk21() = 0; virtual void Unk22() = 0;
    virtual void Unk23() = 0; virtual void Unk24() = 0;

    // Slot 25: Returns root global type scope
    virtual CSchemaSystemTypeScope* GlobalTypeScope() = 0;

    // Virtual slots 26 to 39 (Internal / Unmapped)
    virtual void Unk26() = 0; virtual void Unk27() = 0; virtual void Unk28() = 0; virtual void Unk29() = 0;
    virtual void Unk30() = 0; virtual void Unk31() = 0; virtual void Unk32() = 0; virtual void Unk33() = 0;
    virtual void Unk34() = 0; virtual void Unk35() = 0; virtual void Unk36() = 0; virtual void Unk37() = 0;
    virtual void Unk38() = 0; virtual void Unk39() = 0;

};
static_assert(sizeof(ISchemaSystem) == 0x08, "ISchemaSystem interface pointer must be 8 bytes");

using CSchemaSystem = ISchemaSystem;

#undef CS2_PAD

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_SCHEMA_INTERFACE_H
