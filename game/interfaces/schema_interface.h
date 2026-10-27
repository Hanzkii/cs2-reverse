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

class CSchemaSystemTypeScope {
public:
    virtual ~CSchemaSystemTypeScope() = default;

    // Slot 2: FindDeclaredClass (RVA: +0x00010380)
    virtual CSchemaClassInfo* FindDeclaredClass(const char* pszClassName) = 0;
};

/**
 * @brief SchemaSystem Master Interface (CSchemaSystem)
 * Token: "SchemaSystem_001" in schemasystem.dll
 * VTable: 0x180058598 (222 slots)
 */
class CSchemaSystem {
public:
    virtual ~CSchemaSystem() = default;

    // Slot 13: FindTypeScopeForModule (RVA: +0x00013b80)
    virtual CSchemaSystemTypeScope* FindTypeScopeForModule(const char* pszModuleName) = 0;
};

#undef CS2_PAD

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_SCHEMA_INTERFACE_H
