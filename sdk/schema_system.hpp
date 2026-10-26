#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <type_traits>

// ============================================================================
// Source 2 Reverse Engineered SDK - Runtime SchemaSystem Reflection Subsystem
// Target Binary: schemasystem.dll (Build: 2026-10-25_26144732)
// Architecture: x86-64 (MSVC ABI)
// Reconstructed & Authoritatively Verified via Static RTTI & VTable Analysis
// Provenance:
//   CSchemaSystem VTable:          schemasystem.dll + 0x58598 (40 bounded methods / 222 contiguous block)
//   FindTypeScopeForModule (str):  schemasystem.dll + 0x11350 (Slot 13)
//   FindTypeScopeForModule (hash): schemasystem.dll + 0x11510 (Slot 14)
//   CSchemaSystemTypeScope VTable: schemasystem.dll + 0x586F0 (35 bounded methods / 179 contiguous block)
//   FindDeclaredClass (binding):   schemasystem.dll + 0x162B0 (Slot 2)
//   FindDeclaredClass (hash):      schemasystem.dll + 0x10460 (Slot 24)
//   FindDeclaredClass (str):       schemasystem.dll + 0x10380 (Slot 25)
//   FindDeclaredEnum (binding):    schemasystem.dll + 0x162F0 (Slot 3)
//   FindDeclaredEnum (hash):       schemasystem.dll + 0x105A0 (Slot 26)
//   FindDeclaredEnum (str):        schemasystem.dll + 0x104C0 (Slot 27)
//   CSchemaType VTable:            schemasystem.dll + 0x58930 (17 methods)
// ============================================================================

namespace cs2::sdk {

#ifndef PAD
#define PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:
#endif

// Forward declarations
class CSchemaType;
class CSchemaClassInfo;
class CSchemaEnumInfo;
class CSchemaSystemTypeScope;
class ISchemaSystem;

// ============================================================================
// Schema Type Category Enums
// Extracted from CSchemaType RTTI hierarchy in schemasystem.dll
// ============================================================================
enum class ESchemaTypeCategory : std::uint8_t {
    Builtin          = 0,
    Ptr              = 1,
    FixedArray       = 2,
    Atomic           = 3,
    DeclaredClass    = 4,
    DeclaredEnum     = 5,
    Bitfield         = 6
};

enum class ESchemaAtomicCategory : std::uint8_t {
    Basic            = 0,
    T                = 1,
    CollectionOfT    = 2,
    TT               = 3,
    I                = 4
};

// ============================================================================
// CSchemaType
// VTable RVA: schemasystem.dll + 0x58930 (17 methods)
// RTTI: .?AVCSchemaType@@
// Represents runtime type descriptor for class fields, return types, etc.
// ============================================================================
class CSchemaType {
public:
    virtual ~CSchemaType() = default;                                                            // Slot 0: 0xcdb0
    virtual void GetTypeName(char* pOutBuf, std::size_t nBufSize) const = 0;                     // Slot 1: 0x2b8f0
    virtual std::size_t GetSize() const = 0;                                                     // Slot 2: 0x2c5b0
    virtual std::size_t GetAlignment() const = 0;                                                // Slot 3: 0x3a2ac

public:
    const char*             m_pszName;          // +0x08: Null-terminated type name string (e.g., "int32", "C_BaseEntity*")
    CSchemaSystemTypeScope* m_pTypeScope;       // +0x10: Owning type scope
    ESchemaTypeCategory     m_nTypeCategory;    // +0x18: Schema type category enum
    ESchemaAtomicCategory   m_nAtomicCategory;  // +0x19: Atomic subcategory enum
    PAD(0x6, _pad1A);                           // +0x1A: Alignment padding to 0x20 bytes
};
static_assert(sizeof(CSchemaType) == 0x20, "CSchemaType size mismatch");
static_assert(offsetof(CSchemaType, m_pszName) == 0x08, "CSchemaType::m_pszName offset mismatch");
static_assert(offsetof(CSchemaType, m_pTypeScope) == 0x10, "CSchemaType::m_pTypeScope offset mismatch");
static_assert(offsetof(CSchemaType, m_nTypeCategory) == 0x18, "CSchemaType::m_nTypeCategory offset mismatch");
static_assert(offsetof(CSchemaType, m_nAtomicCategory) == 0x19, "CSchemaType::m_nAtomicCategory offset mismatch");

// ============================================================================
// CSchemaType_DeclaredClass
// RTTI: .?AVCSchemaType_DeclaredClass@@
// Type descriptor representing an engine class declaration reference.
// Verified in schemasystem.dll @ 0x18002C4C0 (m_pClassInfo stored at +0x20).
// ============================================================================
class CSchemaType_DeclaredClass : public CSchemaType {
public:
    CSchemaClassInfo* m_pClassInfo; // +0x20: Pointer to resolved CSchemaClassInfo
};
static_assert(sizeof(CSchemaType_DeclaredClass) == 0x28, "CSchemaType_DeclaredClass size mismatch");
static_assert(offsetof(CSchemaType_DeclaredClass, m_pClassInfo) == 0x20, "CSchemaType_DeclaredClass::m_pClassInfo offset mismatch");

// ============================================================================
// SchemaMetadataEntry_t (SchemaMetadataEntryData_t)
// Layout verified in schemasystem.dll @ 0x180029C8E (stride 0x10 bytes)
// and 0x18000C5E0-0x18000C5EC (iteration loop over metadata).
//   +0x00: m_pszName (ASCII identifier, e.g., "MNetworkEnable", "MNetworkChangeAccessorField")
//   +0x08: m_pValue   (Pointer to metadata network descriptor / value / type)
// ============================================================================
struct SchemaMetadataEntry_t {
    const char* m_pszName; // +0x00: Null-terminated metadata token
    void*       m_pValue;  // +0x08: Value pointer or type descriptor
};
using SchemaMetadataEntryData_t = SchemaMetadataEntry_t;
static_assert(sizeof(SchemaMetadataEntry_t) == 0x10, "SchemaMetadataEntry_t size must be 0x10 bytes");
static_assert(offsetof(SchemaMetadataEntry_t, m_pszName) == 0x00, "SchemaMetadataEntry_t::m_pszName offset mismatch");
static_assert(offsetof(SchemaMetadataEntry_t, m_pValue)   == 0x08, "SchemaMetadataEntry_t::m_pValue offset mismatch");

// ============================================================================
// SchemaClassFieldData_t
// Layout verified in schemasystem.dll:
//   - Field iteration & stride (0x20 bytes): schemasystem.dll + 0x180029C96
//   - Offset comparison & usage:             schemasystem.dll + 0x18000F1C6
//   - Metadata array dereference:            schemasystem.dll + 0x180029C77
//
// Field Members:
//   m_pszName                   : +0x00 (ASCII field name string, e.g., "m_iHealth")
//   m_pType                     : +0x08 (Descriptor pointer to CSchemaType)
//   m_nSingleInheritanceOffset  : +0x10 (Byte offset within class instance memory)
//   m_nMetadataSize             : +0x14 (Count of metadata elements bound to field)
//   m_pMetadata                 : +0x18 (Pointer to SchemaMetadataEntry_t array)
// ============================================================================
struct SchemaClassFieldData_t {
    const char*            m_pszName;                  // +0x00: Field identifier string
    CSchemaType*           m_pType;                    // +0x08: Runtime field type descriptor
    std::int32_t           m_nSingleInheritanceOffset; // +0x10: Relative byte offset within entity class
    std::int32_t           m_nMetadataSize;            // +0x14: Metadata entry count
    SchemaMetadataEntry_t* m_pMetadata;                // +0x18: Array of field metadata attributes
};
static_assert(sizeof(SchemaClassFieldData_t) == 0x20, "SchemaClassFieldData_t size must be 0x20 bytes");
static_assert(offsetof(SchemaClassFieldData_t, m_pszName)                  == 0x00, "SchemaClassFieldData_t::m_pszName offset mismatch");
static_assert(offsetof(SchemaClassFieldData_t, m_pType)                     == 0x08, "SchemaClassFieldData_t::m_pType offset mismatch");
static_assert(offsetof(SchemaClassFieldData_t, m_nSingleInheritanceOffset)  == 0x10, "SchemaClassFieldData_t::m_nSingleInheritanceOffset offset mismatch");
static_assert(offsetof(SchemaClassFieldData_t, m_nMetadataSize)             == 0x14, "SchemaClassFieldData_t::m_nMetadataSize offset mismatch");
static_assert(offsetof(SchemaClassFieldData_t, m_pMetadata)                 == 0x18, "SchemaClassFieldData_t::m_pMetadata offset mismatch");

// ============================================================================
// SchemaBaseClassInfoData_t
// Layout verified in schemasystem.dll @ 0x18000FFD0-0x18000FFDE (stride 0x10)
//   +0x00: m_nOffset (Offset of the base class within the derived class)
//   +0x08: m_pClass  (Resolved pointer to base CSchemaClassInfo)
// ============================================================================
struct SchemaBaseClassInfoData_t {
    std::uint32_t     m_nOffset; // +0x00: Base class sub-object byte offset
    PAD(0x4, _pad04);            // +0x04: Alignment padding
    CSchemaClassInfo* m_pClass;  // +0x08: Target base class descriptor
};
static_assert(sizeof(SchemaBaseClassInfoData_t) == 0x10, "SchemaBaseClassInfoData_t size must be 0x10 bytes");
static_assert(offsetof(SchemaBaseClassInfoData_t, m_nOffset) == 0x00, "SchemaBaseClassInfoData_t::m_nOffset offset mismatch");
static_assert(offsetof(SchemaBaseClassInfoData_t, m_pClass)  == 0x08, "SchemaBaseClassInfoData_t::m_pClass offset mismatch");

// ============================================================================
// SchemaStaticFieldData_t
// Static field definition inside a schema class descriptor.
// Verified in schemasystem.dll @ 0x180029C37 (m_pStaticFields array at +0x48).
// ============================================================================
struct SchemaStaticFieldData_t {
    const char*  m_pszName;  // +0x00: Static field identifier
    CSchemaType* m_pType;    // +0x08: Type descriptor
    void*        m_pInstance;// +0x10: Memory location of static member
    PAD(0x8, _pad18);        // +0x18: Padding to 0x20 bytes
};
static_assert(sizeof(SchemaStaticFieldData_t) == 0x20, "SchemaStaticFieldData_t size must be 0x20 bytes");

// ============================================================================
// CSchemaClassInfo (SchemaClassInfoData_t)
// Layout verified in schemasystem.dll (Build: 2026-10-25_26144732):
//   - Logging inspection:    schemasystem.dll @ 0x18000B3D8-0x18000B3E3:
//                            "Class: %7d bytes, %4d fields, %4d nonderived fields"
//                            m_nSize read from [rbx+0x20], m_nFieldCount from [rbx+0x24]
//   - Field count & array:   schemasystem.dll @ 0x180029C60 (field count at +0x24, fields array at +0x30)
//   - Base class traversal:  schemasystem.dll @ 0x18000FFC2 (+0x29 count, +0x38 array)
//   - MurmurHash2 token:     schemasystem.dll @ 0x180029FD4 (64-bit token stored at +0x18)
//   - Static fields array:   schemasystem.dll @ 0x180029C37 (static fields at +0x48, count at +0x26)
// ============================================================================
class CSchemaClassInfo {
public:
    CSchemaClassInfo*          m_pSelf;            // +0x00: Registration node / self-pointer
    const char*                m_pszName;          // +0x08: Null-terminated class identifier (e.g. "C_CSPlayerPawn")
    const char*                m_pszModuleName;    // +0x10: Owning module token (e.g. "client.dll")
    union {
        std::uint64_t          m_nClassHash;       // +0x18: MurmurHash2 token calculated @ 0x180029FC5
        void*                  m_pTypeScope;       // +0x18: Pointer to owning CSchemaSystemTypeScope
    };
    std::int32_t               m_nSize;            // +0x20: Instance byte size (Verified schemasystem.dll @ 0x18000B3E3)
    std::int16_t               m_nFieldCount;      // +0x24: Declared field count (Verified schemasystem.dll @ 0x18000B3D8, 0x180029C60)
    std::int16_t               m_nStaticCount;     // +0x26: Static field count (Verified schemasystem.dll @ 0x180029CB9)
    std::uint8_t               m_nMetadataCount;   // +0x28: Metadata entry count (Verified schemasystem.dll @ 0x18000BC70, 0x18002A2AF)
    std::uint8_t               m_nBaseClassCount;  // +0x29: Base classes count (Verified schemasystem.dll @ 0x18000FFC2, 0x180029CA3)
    PAD(0x6, _pad2A);                              // +0x2A: Alignment padding to 0x30 boundary
    SchemaClassFieldData_t*    m_pFields;          // +0x30: Array of declared fields (Verified schemasystem.dll @ 0x180029C73)
    SchemaBaseClassInfoData_t* m_pBaseClasses;     // +0x38: Array of base class descriptors (Verified schemasystem.dll @ 0x18000FFDA)
    PAD(0x8, _pad40);                              // +0x40: Internal reservation
    SchemaStaticFieldData_t*   m_pStaticFields;    // +0x48: Array of static fields (Verified schemasystem.dll @ 0x180029C37)
    PAD(0x10, _pad50);                             // +0x50: Internal reservation
    std::uint32_t              m_nFlags;           // +0x60: Schema class flags (SCHEMA_CF1_*)
    PAD(0x4, _pad64);                              // +0x64: Tail padding to 0x68 bytes

public:
    [[nodiscard]] const char* GetName() const noexcept { return m_pszName; }
    [[nodiscard]] const char* GetModuleName() const noexcept { return m_pszModuleName; }
    [[nodiscard]] std::int32_t GetSize() const noexcept { return m_nSize; }
    [[nodiscard]] std::int16_t GetFieldCount() const noexcept { return m_nFieldCount; }
    [[nodiscard]] SchemaClassFieldData_t* GetFields() const noexcept { return m_pFields; }

    [[nodiscard]] const SchemaClassFieldData_t* FindField(std::string_view fieldName) const noexcept {
        if (!m_pFields || m_nFieldCount <= 0) return nullptr;
        for (std::int16_t i = 0; i < m_nFieldCount; ++i) {
            if (m_pFields[i].m_pszName && fieldName == m_pFields[i].m_pszName) {
                return &m_pFields[i];
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::int32_t FindFieldOffset(std::string_view fieldName) const noexcept {
        const auto* pField = FindField(fieldName);
        return pField ? pField->m_nSingleInheritanceOffset : -1;
    }
};
using SchemaClassInfoData_t = CSchemaClassInfo;

static_assert(sizeof(CSchemaClassInfo) == 0x68, "CSchemaClassInfo size must be 0x68 bytes");
static_assert(offsetof(CSchemaClassInfo, m_pSelf)           == 0x00, "CSchemaClassInfo::m_pSelf offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_pszName)         == 0x08, "CSchemaClassInfo::m_pszName offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_pszModuleName)   == 0x10, "CSchemaClassInfo::m_pszModuleName offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_nClassHash)      == 0x18, "CSchemaClassInfo::m_nClassHash offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_nSize)           == 0x20, "CSchemaClassInfo::m_nSize offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_nFieldCount)     == 0x24, "CSchemaClassInfo::m_nFieldCount offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_nStaticCount)    == 0x26, "CSchemaClassInfo::m_nStaticCount offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_nMetadataCount)  == 0x28, "CSchemaClassInfo::m_nMetadataCount offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_nBaseClassCount) == 0x29, "CSchemaClassInfo::m_nBaseClassCount offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_pFields)         == 0x30, "CSchemaClassInfo::m_pFields offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_pBaseClasses)    == 0x38, "CSchemaClassInfo::m_pBaseClasses offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_pStaticFields)   == 0x48, "CSchemaClassInfo::m_pStaticFields offset mismatch");
static_assert(offsetof(CSchemaClassInfo, m_nFlags)          == 0x60, "CSchemaClassInfo::m_nFlags offset mismatch");

// ============================================================================
// CSchemaClassBinding
// Wrapper structure returned by CSchemaSystemTypeScope::FindDeclaredClass (Slot 2)
// Disassembly: schemasystem.dll @ 0x1800162B0
// ============================================================================
struct CSchemaClassBinding {
    CSchemaClassInfo* m_pClassInfo; // +0x00: Resolved schema class info pointer
};
static_assert(sizeof(CSchemaClassBinding) == 0x08, "CSchemaClassBinding size must be 0x08 bytes");
static_assert(offsetof(CSchemaClassBinding, m_pClassInfo) == 0x00, "CSchemaClassBinding::m_pClassInfo offset mismatch");

// ============================================================================
// SchemaEnumFieldData_t
// Enumerator field descriptor inside a schema enum info.
// ============================================================================
struct SchemaEnumFieldData_t {
    const char*   m_pszName;  // +0x00: Enum key identifier
    std::uint64_t m_nValue;   // +0x08: Enum constant value
    PAD(0x10, _pad10);        // +0x10: Metadata and padding to 0x20 bytes
};
static_assert(sizeof(SchemaEnumFieldData_t) == 0x20, "SchemaEnumFieldData_t size mismatch");

// ============================================================================
// CSchemaEnumInfo (SchemaEnumInfoData_t)
// Verified in schemasystem.dll @ 0x1800104C0 (Slot 27 enum resolution)
// ============================================================================
class CSchemaEnumInfo {
public:
    CSchemaEnumInfo*       m_pSelf;          // +0x00
    const char*            m_pszName;        // +0x08: Enum identifier
    const char*            m_pszModuleName;  // +0x10: Owning module
    std::uint8_t           m_nSize;          // +0x18: Storage byte size (1, 2, 4, 8)
    std::uint8_t           m_nAlign;         // +0x19: Alignment
    std::int16_t           m_nFieldCount;    // +0x1A: Enumerator count
    std::int16_t           m_nMetadataCount; // +0x1C: Metadata count
    PAD(0x2, _pad1E);                        // +0x1E: Alignment padding
    SchemaEnumFieldData_t* m_pFields;        // +0x20: Enumerator elements array
};
using SchemaEnumInfoData_t = CSchemaEnumInfo;
static_assert(offsetof(CSchemaEnumInfo, m_pszName)     == 0x08, "CSchemaEnumInfo::m_pszName offset mismatch");
static_assert(offsetof(CSchemaEnumInfo, m_nFieldCount) == 0x1A, "CSchemaEnumInfo::m_nFieldCount offset mismatch");
static_assert(offsetof(CSchemaEnumInfo, m_pFields)     == 0x20, "CSchemaEnumInfo::m_pFields offset mismatch");

struct CSchemaEnumBinding {
    CSchemaEnumInfo* m_pEnumInfo; // +0x00: Resolved schema enum pointer
};
static_assert(sizeof(CSchemaEnumBinding) == 0x08, "CSchemaEnumBinding size mismatch");

// ============================================================================
// CSchemaSystemTypeScope
// VTable RVA: schemasystem.dll + 0x586F0 (35 bounded methods / 179 contiguous block)
// RTTI: .?AVCSchemaSystemTypeScope@@
// Instance Allocation: 0x3640 bytes (schemasystem.dll @ 0x180011420)
//
// Verified Virtual Slots:
//   - Slot  0: ~CSchemaSystemTypeScope()            (RVA: 0x0FCD0)
//   - Slot  1: Unk01                                (RVA: 0x10060)
//   - Slot  2: FindDeclaredClass (via out binding)  (RVA: 0x162B0)
//   - Slot  3: FindDeclaredEnum (via out binding)   (RVA: 0x162F0)
//   - Slot 24: FindDeclaredClass (by MurmurHash2)   (RVA: 0x10460)
//   - Slot 25: FindDeclaredClass (by class name str)(RVA: 0x10380)
//   - Slot 26: FindDeclaredEnum (by MurmurHash2)    (RVA: 0x105A0)
//   - Slot 27: FindDeclaredEnum (by enum name str)  (RVA: 0x104C0)
// ============================================================================
class CSchemaSystemTypeScope {
public:
    virtual ~CSchemaSystemTypeScope() = default;                                                 // Slot 0: 0x0fcd0
    virtual void Unk01() = 0;                                                                    // Slot 1: 0x10060

    // Slot 2: schemasystem.dll + 0x162B0 (Calls Slot 25 @ 0x180010380 and stores result into out pointer)
    virtual CSchemaClassBinding* FindDeclaredClass(CSchemaClassBinding* pOutBinding, const char* pszClassName) = 0;

    // Slot 3: schemasystem.dll + 0x162F0 (Calls Slot 27 @ 0x1800104C0 and stores result into out pointer)
    virtual CSchemaEnumBinding* FindDeclaredEnum(CSchemaEnumBinding* pOutBinding, const char* pszEnumName) = 0;

    // Virtual slots 4 to 23 (internal type reflection, serialization builders, callbacks)
    virtual void Unk04() = 0; virtual void Unk05() = 0; virtual void Unk06() = 0; virtual void Unk07() = 0;
    virtual void Unk08() = 0; virtual void Unk09() = 0; virtual void Unk10() = 0; virtual void Unk11() = 0;
    virtual void Unk12() = 0; virtual void Unk13() = 0; virtual void Unk14() = 0; virtual void Unk15() = 0;
    virtual void Unk16() = 0; virtual void Unk17() = 0; virtual void Unk18() = 0; virtual void Unk19() = 0;
    virtual void Unk20() = 0; virtual void Unk21() = 0; virtual void Unk22() = 0; virtual void Unk23() = 0;

    // Slot 24: schemasystem.dll + 0x10460 (Direct MurmurHash2 lookup in hash table at +0x560)
    virtual CSchemaClassInfo* FindDeclaredClassByHash(std::uint32_t nClassNameHash) = 0;

    // Slot 25: schemasystem.dll + 0x10380 (Hashes string with MurmurHash2 seed len^0xbaadfeed then calls Slot 24)
    virtual CSchemaClassInfo* FindDeclaredClass(const char* pszClassName) = 0;

    // Slot 26: schemasystem.dll + 0x105A0 (Direct MurmurHash2 lookup in enum hash table)
    virtual CSchemaEnumInfo* FindDeclaredEnumByHash(std::uint32_t nEnumNameHash) = 0;

    // Slot 27: schemasystem.dll + 0x104C0 (Hashes string and resolves enum descriptor)
    virtual CSchemaEnumInfo* FindDeclaredEnum(const char* pszEnumName) = 0;

public:
    char                    m_szScopeName[256]; // +0x08: Scope module identifier string (e.g., "client.dll")
    CSchemaSystemTypeScope* m_pParentScope;     // +0x108: Pointer to global/parent type scope
    bool                    m_bIsGlobalScope;   // +0x110: True if root global scope
    PAD(0x7, _pad111);                          // +0x111: Alignment padding
    PAD(0x448, _pad118);                        // +0x118 to +0x560: Type registry locks, allocation arenas
    // +0x560: CUtlHashtable of MurmurHash2 -> CSchemaClassInfo*
};
static_assert(offsetof(CSchemaSystemTypeScope, m_szScopeName)    == 0x08, "CSchemaSystemTypeScope::m_szScopeName offset mismatch");
static_assert(offsetof(CSchemaSystemTypeScope, m_pParentScope)   == 0x108, "CSchemaSystemTypeScope::m_pParentScope offset mismatch");
static_assert(offsetof(CSchemaSystemTypeScope, m_bIsGlobalScope) == 0x110, "CSchemaSystemTypeScope::m_bIsGlobalScope offset mismatch");

// ============================================================================
// ISchemaSystem (CSchemaSystem)
// Export Interface Token: "SchemaSystem_001" (schemasystem.dll)
// Node RVA:    0x4C248, Factory RVA: 0x715F8
// VTable RVA:  schemasystem.dll + 0x58598 (40 bounded methods / 222 contiguous block)
// RTTI:        .?AVCSchemaSystem@@
//
// Verified Virtual Slots:
//   - Slot  0: ~ISchemaSystem()                                 (RVA: 0x0E160)
//   - Slot 11: GlobalTypeScope()                                (RVA: 0x11340)
//   - Slot 12: FindOrCreateTypeScopeForModule (Allocates 0x3640)(RVA: 0x11420)
//   - Slot 13: FindTypeScopeForModule (str, outRemainder)       (RVA: 0x11350)
//   - Slot 14: FindTypeScopeForModule (typeFlag, str, outRem)   (RVA: 0x11510)
//   - Slot 25: GlobalTypeScope()                                (RVA: 0x10C70)
// ============================================================================
#ifndef CS2_SDK_ISCHEMASYSTEM_DEFINED
#define CS2_SDK_ISCHEMASYSTEM_DEFINED
class ISchemaSystem {
public:
    virtual ~ISchemaSystem() = default;                                                          // Slot 0: 0x0e160

    // Virtual slots 1 to 10 (Connect, Disconnect, QueryInterface, Init, Shutdown, etc.)
    virtual void Unk01() = 0; virtual void Unk02() = 0; virtual void Unk03() = 0;
    virtual void Unk04() = 0; virtual void Unk05() = 0; virtual void Unk06() = 0;
    virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0;

    // Slot 11: schemasystem.dll + 0x11340 (Returns pointer to static global default type scope DAT_1800730d0)
    virtual CSchemaSystemTypeScope* GetDefaultTypeScope() = 0;

    // Slot 12: schemasystem.dll + 0x11420 (Looks up scope by module name; if absent, allocates 0x3640 bytes and constructs new scope)
    virtual CSchemaSystemTypeScope* FindOrCreateTypeScopeForModule(const char* pszModuleName, void* pBindingContext = nullptr) = 0;

    // Slot 13: schemasystem.dll + 0x11350 (Resolves existing module type scope by ASCII name string; parses '!' delimiter)
    virtual CSchemaSystemTypeScope* FindTypeScopeForModule(const char* pszModuleName, void* pBindingContext = nullptr) = 0;

    // Slot 14: schemasystem.dll + 0x11510 (Overloaded dispatch forwarding to Slot 12 or Slot 13 based on mode flag)
    virtual CSchemaSystemTypeScope* FindTypeScopeForModule(int nType, const char* pszModuleName, void* pBindingContext = nullptr) = 0;

    // Virtual slots 15 to 24 (type scope iteration, registration events, memory management)
    virtual void Unk15() = 0; virtual void Unk16() = 0; virtual void Unk17() = 0; virtual void Unk18() = 0;
    virtual void Unk19() = 0; virtual void Unk20() = 0; virtual void Unk21() = 0; virtual void Unk22() = 0;
    virtual void Unk23() = 0; virtual void Unk24() = 0;

    // Slot 25: schemasystem.dll + 0x10C70 (Resolves global type scope)
    virtual CSchemaSystemTypeScope* GlobalTypeScope() = 0;
};
#endif

// ============================================================================
// MurmurHash2 Implementation matching schemasystem.dll @ 0x180010380
// Used across all CSchemaSystemTypeScope runtime dictionary lookups.
// Algorithm:
//   seed = len ^ 0xbaadfeed
//   multiplier m = 0x5bd1e995
//   rotation r = 24
// ============================================================================
constexpr std::uint32_t MurmurHash2(std::string_view str) noexcept {
    const std::uint32_t len = static_cast<std::uint32_t>(str.length());
    if (len == 0) return 0;

    constexpr std::uint32_t m = 0x5bd1e995;
    constexpr int r = 24;

    std::uint32_t h = len ^ 0xbaadfeed;
    const char* data = str.data();
    std::uint32_t remaining = len;

    while (remaining >= 4) {
        std::uint32_t k = static_cast<std::uint8_t>(data[0]) |
                         (static_cast<std::uint8_t>(data[1]) << 8) |
                         (static_cast<std::uint8_t>(data[2]) << 16) |
                         (static_cast<std::uint8_t>(data[3]) << 24);

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        remaining -= 4;
    }

    if (remaining == 3) h ^= static_cast<std::uint8_t>(data[2]) << 16;
    if (remaining >= 2) h ^= static_cast<std::uint8_t>(data[1]) << 8;
    if (remaining >= 1) {
        h ^= static_cast<std::uint8_t>(data[0]);
        h *= m;
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

} // namespace cs2::sdk
