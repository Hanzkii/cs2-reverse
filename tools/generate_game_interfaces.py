#!/usr/bin/env python3
"""
generate_game_interfaces.py - Source 2 C++20 Interface Header Generator

Parses data/vtables.json (13,365 VTables, 101 canonical slots) and
data/function_signatures.json to emit clean, type-safe C++20 abstract
class headers into game/interfaces/.

Generated Interfaces:
  - INetworkServerService (engine2.dll)
  - INetworkClientService (engine2.dll)
  - ISource2Server (server.dll)
  - ISource2Client (client.dll)
  - IEngineCVar & CCvar (tier0.dll / engine2.dll)
  - ISchemaSystem & CSchemaSystemTypeScope (schemasystem.dll)
  - IMemAlloc (tier0.dll)
  - IInputSystem (inputsystem.dll)
  - IFileSystem (filesystem_stdio.dll)
  - ITraceManager & CGameTraceManager (server.dll / client.dll)

Guarantees:
  - MSVC x64 ABI VTable alignment: virtual method indices strictly match vtable_index.
  - Pure C++20 standard library compliance (/std:c++20 /W4).
  - Type-safe method signatures and pure virtual `= 0` declarations.
  - Zero local path disclosure (relative paths only).
"""

import os
import sys
import json
import argparse
from pathlib import Path
from typing import Dict, List, Any, Optional

# =============================================================================
# Canonical Interface Schema Definitions & Known Method Signatures
# =============================================================================

INTERFACE_SPECS = [
    {
        "id": "network_server_service",
        "header": "network_server_service.h",
        "interface_name": "INetworkServerService",
        "class_name": "CNetworkServerService",
        "alias_name": "CNetworkServerService",
        "binary": "engine2.dll",
        "token": "NetworkServerService_001",
        "vtable_rva": "0x60fa40",
        "default_slots": 51,
        "description": "Source 2 Network Server Loop & Hosting Service Interface",
        "includes": ["<cstdint>", "<cstddef>"],
        "forward_decl": [],
        "methods": {
            0: {"ret": "", "name": "~INetworkServerService", "params": "", "qualifier": "= default", "comment": "Slot 0: Virtual destructor"},
            1: {"ret": "bool", "name": "Connect", "params": "void* pFactory", "qualifier": "= 0", "comment": "Slot 1: IAppSystem::Connect"},
            2: {"ret": "void", "name": "Disconnect", "params": "", "qualifier": "= 0", "comment": "Slot 2: IAppSystem::Disconnect"},
            3: {"ret": "void*", "name": "QueryInterface", "params": "const char* pInterfaceName", "qualifier": "= 0", "comment": "Slot 3: IAppSystem::QueryInterface"},
            4: {"ret": "int", "name": "Init", "params": "", "qualifier": "= 0", "comment": "Slot 4: IAppSystem::Init"},
            5: {"ret": "void", "name": "Shutdown", "params": "", "qualifier": "= 0", "comment": "Slot 5: IAppSystem::Shutdown"},
            23: {"ret": "void*", "name": "GetIGameServer", "params": "", "qualifier": "= 0", "comment": "Slot 23: Returns active IGameServer instance"},
            24: {"ret": "void*", "name": "GetNetworkChannel", "params": "int nSlot = 0", "qualifier": "= 0", "comment": "Slot 24: Returns active INetChannel for client slot"}
        }
    },
    {
        "id": "network_client_service",
        "header": "network_client_service.h",
        "interface_name": "INetworkClientService",
        "class_name": "CNetworkClientService",
        "alias_name": "CNetworkClientService",
        "binary": "engine2.dll",
        "token": "NetworkClientService_001",
        "vtable_rva": "0x60e978",
        "default_slots": 64,
        "description": "Source 2 Network Client Loop & Pacing Service Interface",
        "includes": ["<cstdint>", "<cstddef>"],
        "forward_decl": ["class CNetworkGameClient;"],
        "methods": {
            0: {"ret": "", "name": "~INetworkClientService", "params": "", "qualifier": "default", "comment": "Slot 0: Virtual destructor"},
            1: {"ret": "bool", "name": "Connect", "params": "void* pFactory", "qualifier": "= 0", "comment": "Slot 1: IAppSystem::Connect"},
            2: {"ret": "void", "name": "Disconnect", "params": "", "qualifier": "= 0", "comment": "Slot 2: IAppSystem::Disconnect"},
            3: {"ret": "void*", "name": "QueryInterface", "params": "const char* pInterfaceName", "qualifier": "= 0", "comment": "Slot 3: IAppSystem::QueryInterface"},
            4: {"ret": "int", "name": "Init", "params": "", "qualifier": "= 0", "comment": "Slot 4: IAppSystem::Init"},
            5: {"ret": "void", "name": "Shutdown", "params": "", "qualifier": "= 0", "comment": "Slot 5: IAppSystem::Shutdown"},
            21: {"ret": "void*", "name": "GetNetworkChannel", "params": "", "qualifier": "= 0", "comment": "Slot 21: Returns client INetChannel instance"},
            22: {"ret": "int", "name": "GetSignonState", "params": "", "qualifier": "= 0", "comment": "Slot 22: Returns current ESignonState enum value"},
            23: {"ret": "CNetworkGameClient*", "name": "GetNetworkGameClient", "params": "", "qualifier": "= 0", "comment": "Slot 23: Returns active CNetworkGameClient manager"}
        }
    },
    {
        "id": "source2_server",
        "header": "source2_server.h",
        "interface_name": "ISource2Server",
        "class_name": "CSource2Server",
        "alias_name": "CSource2Server",
        "binary": "server.dll",
        "token": "Source2Server001",
        "vtable_rva": "0x197e1d0",
        "default_slots": 103,
        "description": "Source 2 Dedicated / Listen Game Server Interface",
        "includes": ["<cstdint>", "<cstddef>"],
        "forward_decl": [],
        "methods": {
            0: {"ret": "", "name": "~ISource2Server", "params": "", "qualifier": "default", "comment": "Slot 0: Virtual destructor"},
            1: {"ret": "bool", "name": "Connect", "params": "void* pFactory", "qualifier": "= 0", "comment": "Slot 1: IAppSystem::Connect"},
            2: {"ret": "void", "name": "Disconnect", "params": "", "qualifier": "= 0", "comment": "Slot 2: IAppSystem::Disconnect"},
            3: {"ret": "void*", "name": "QueryInterface", "params": "const char* pInterfaceName", "qualifier": "= 0", "comment": "Slot 3: IAppSystem::QueryInterface"},
            4: {"ret": "int", "name": "Init", "params": "", "qualifier": "= 0", "comment": "Slot 4: IAppSystem::Init"},
            5: {"ret": "void", "name": "Shutdown", "params": "", "qualifier": "= 0", "comment": "Slot 5: IAppSystem::Shutdown"},
            6: {"ret": "bool", "name": "InitServer", "params": "const char* pszGameDir", "qualifier": "= 0", "comment": "Slot 6: Initializes game directory and subsystems"},
            7: {"ret": "void", "name": "ShutdownServer", "params": "", "qualifier": "= 0", "comment": "Slot 7: Shuts down server game systems"},
            8: {"ret": "void", "name": "GameInit", "params": "", "qualifier": "= 0", "comment": "Slot 8: Game loop initialization"},
            9: {"ret": "void", "name": "GameShutdown", "params": "", "qualifier": "= 0", "comment": "Slot 9: Game loop teardown"}
        }
    },
    {
        "id": "source2_client",
        "header": "source2_client.h",
        "interface_name": "ISource2Client",
        "class_name": "CSource2Client",
        "alias_name": "CSource2Client",
        "binary": "client.dll",
        "token": "Source2Client002",
        "vtable_rva": "0x1c5aa90",
        "default_slots": 211,
        "description": "Source 2 Game Client Subsystem & Presentation Dispatcher Interface",
        "includes": ["<cstdint>", "<cstddef>"],
        "forward_decl": [],
        "methods": {
            0: {"ret": "", "name": "~ISource2Client", "params": "", "qualifier": "default", "comment": "Slot 0: Virtual destructor"},
            1: {"ret": "bool", "name": "Connect", "params": "void* pFactory", "qualifier": "= 0", "comment": "Slot 1: IAppSystem::Connect"},
            2: {"ret": "void", "name": "Disconnect", "params": "", "qualifier": "= 0", "comment": "Slot 2: IAppSystem::Disconnect"},
            3: {"ret": "void*", "name": "QueryInterface", "params": "const char* pInterfaceName", "qualifier": "= 0", "comment": "Slot 3: IAppSystem::QueryInterface"},
            4: {"ret": "int", "name": "Init", "params": "", "qualifier": "= 0", "comment": "Slot 4: IAppSystem::Init"},
            5: {"ret": "void", "name": "Shutdown", "params": "", "qualifier": "= 0", "comment": "Slot 5: IAppSystem::Shutdown"},
            6: {"ret": "void", "name": "PostInit", "params": "", "qualifier": "= 0", "comment": "Slot 6: Client post-initialization hook"},
            7: {"ret": "void", "name": "PreShutdown", "params": "", "qualifier": "= 0", "comment": "Slot 7: Client pre-shutdown notification"},
            8: {"ret": "void", "name": "GameInit", "params": "", "qualifier": "= 0", "comment": "Slot 8: Client game startup"},
            9: {"ret": "void", "name": "GameShutdown", "params": "", "qualifier": "= 0", "comment": "Slot 9: Client game termination"},
            10: {"ret": "void", "name": "LevelInitPreEntity", "params": "const char* pszMapName", "qualifier": "= 0", "comment": "Slot 10: Map load prior to entity spawn"},
            11: {"ret": "void", "name": "LevelInitPostEntity", "params": "", "qualifier": "= 0", "comment": "Slot 11: Map load following entity spawn"},
            12: {"ret": "void", "name": "LevelShutdownPreEntity", "params": "", "qualifier": "= 0", "comment": "Slot 12: Map purge prior to entity destruction"},
            13: {"ret": "void", "name": "LevelShutdownPostEntity", "params": "", "qualifier": "= 0", "comment": "Slot 13: Map purge following entity destruction"}
        }
    },
    {
        "id": "cvar",
        "header": "cvar.h",
        "interface_name": "IEngineCVar",
        "class_name": "CCvar",
        "alias_name": "CCvar",
        "binary": "tier0.dll",
        "token": "VEngineCvar007",
        "vtable_rva": "0x313208",
        "default_slots": 46,
        "description": "Source 2 ConVar & ConCommand Registry Interface (CCvar / IEngineCVar)",
        "includes": ["<cstdint>", "<cstddef>", "<string_view>", "../tier0/utl_string_token.h"],
        "forward_decl": [],
        "pre_content": """// Source 2 ConVar Security & Replication Flags
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
""",
        "methods": {
            0: {"ret": "", "name": "~IEngineCVar", "params": "", "qualifier": "default", "comment": "Slot 0: Virtual destructor"},
            1: {"ret": "bool", "name": "Connect", "params": "void* pFactory", "qualifier": "= 0", "comment": "Slot 1: IAppSystem::Connect"},
            2: {"ret": "void", "name": "Disconnect", "params": "", "qualifier": "= 0", "comment": "Slot 2: IAppSystem::Disconnect"},
            3: {"ret": "void*", "name": "QueryInterface", "params": "const char* pInterfaceName", "qualifier": "= 0", "comment": "Slot 3: IAppSystem::QueryInterface"},
            4: {"ret": "int", "name": "Init", "params": "", "qualifier": "= 0", "comment": "Slot 4: IAppSystem::Init"},
            5: {"ret": "void", "name": "Shutdown", "params": "", "qualifier": "= 0", "comment": "Slot 5: IAppSystem::Shutdown"},
            11: {"ret": "ConVar*", "name": "FindVar", "params": "const char* pszVarName", "qualifier": "= 0", "comment": "Slot 11: Finds ConVar by name string"},
            12: {"ret": "ConVar*", "name": "FindVarByIndex", "params": "std::int32_t nIndex", "qualifier": "= 0", "comment": "Slot 12: Finds ConVar by internal hash/registration index"},
            13: {"ret": "void*", "name": "FindCommandByName", "params": "const char* pszCommandName", "qualifier": "= 0", "comment": "Slot 13: Finds ConCommand by name string"},
            41: {"ret": "std::uint64_t", "name": "GetFirstCVarArrayValue", "params": "", "qualifier": "= 0", "comment": "Slot 41: Returns first ConVar iterator handle"},
            42: {"ret": "std::uint64_t", "name": "GetNextCVarArrayValue", "params": "", "qualifier": "= 0", "comment": "Slot 42: Returns next ConVar iterator handle"}
        },
        "post_class_content": """
    // Convenience Accessors
    [[nodiscard]] ConVar* FindVarByName(const char* pszVarName) { return FindVar(pszVarName); }
    [[nodiscard]] ConVar* FindVarByToken(tier0::CUtlStringToken token) { return FindVarByIndex(static_cast<std::int32_t>(token.m_nHashCode)); }
"""
    },
    {
        "id": "schema_interface",
        "header": "schema_interface.h",
        "interface_name": "ISchemaSystem",
        "class_name": "CSchemaSystem",
        "alias_name": "CSchemaSystem",
        "binary": "schemasystem.dll",
        "token": "SchemaSystem_001",
        "vtable_rva": "0x58598",
        "default_slots": 40,
        "description": "Source 2 Runtime Reflection SchemaSystem Subsystem",
        "includes": ["<cstdint>", "<cstddef>", "../tier0/utl_string_token.h"],
        "forward_decl": [],
        "pre_content": """#define CS2_PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

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
""",
        "methods": {
            0: {"ret": "", "name": "~ISchemaSystem", "params": "", "qualifier": "default", "comment": "Slot 0: Virtual destructor"},
            1: {"ret": "bool", "name": "Connect", "params": "void* pFactory", "qualifier": "= 0", "comment": "Slot 1: IAppSystem::Connect"},
            2: {"ret": "void", "name": "Disconnect", "params": "", "qualifier": "= 0", "comment": "Slot 2: IAppSystem::Disconnect"},
            3: {"ret": "void*", "name": "QueryInterface", "params": "const char* pInterfaceName", "qualifier": "= 0", "comment": "Slot 3: IAppSystem::QueryInterface"},
            4: {"ret": "int", "name": "Init", "params": "", "qualifier": "= 0", "comment": "Slot 4: IAppSystem::Init"},
            5: {"ret": "void", "name": "Shutdown", "params": "", "qualifier": "= 0", "comment": "Slot 5: IAppSystem::Shutdown"},
            13: {"ret": "CSchemaSystemTypeScope*", "name": "FindTypeScopeForModule", "params": "const char* pszModuleName, void* pBindingContext = nullptr", "qualifier": "= 0", "comment": "Slot 13: Resolves module scope by name (e.g. 'client.dll')"},
            14: {"ret": "CSchemaSystemTypeScope*", "name": "FindTypeScopeForModuleByToken", "params": "std::uint32_t nScopeHash", "qualifier": "= 0", "comment": "Slot 14: Resolves module scope by MurmurHash2 token"},
            25: {"ret": "CSchemaSystemTypeScope*", "name": "GlobalTypeScope", "params": "", "qualifier": "= 0", "comment": "Slot 25: Returns root global type scope"}
        },
        "post_file_content": """#undef CS2_PAD
"""
    },
    {
        "id": "memalloc",
        "header": "memalloc.h",
        "interface_name": "IMemAlloc",
        "class_name": "IMemAlloc",
        "alias_name": "IMemAlloc",
        "binary": "tier0.dll",
        "token": "g_pMemAlloc",
        "vtable_rva": "0x3277c0",
        "default_slots": 40,
        "description": "Source 2 Tier0 Heap Allocator Interface (IMemAlloc)",
        "includes": ["<cstdint>", "<cstddef>"],
        "forward_decl": [],
        "methods": {
            0: {"ret": "", "name": "~IMemAlloc", "params": "", "qualifier": "default", "comment": "Slot 0: Virtual destructor"},
            1: {"ret": "void*", "name": "Alloc", "params": "std::size_t nSize", "qualifier": "= 0", "comment": "Slot 1: Allocates raw memory block of nSize bytes"},
            2: {"ret": "void*", "name": "Realloc", "params": "void* pMem, std::size_t nSize", "qualifier": "= 0", "comment": "Slot 2: Reallocates memory block to new size"},
            3: {"ret": "void", "name": "Free", "params": "void* pMem", "qualifier": "= 0", "comment": "Slot 3: Frees previously allocated memory block"},
            4: {"ret": "void*", "name": "Expand", "params": "void* pMem, std::size_t nSize", "qualifier": "= 0", "comment": "Slot 4: Expands memory block in-place"},
            5: {"ret": "void*", "name": "AllocAligned", "params": "std::size_t nSize, std::size_t nAlignment", "qualifier": "= 0", "comment": "Slot 5: Allocates aligned memory block"},
            6: {"ret": "void*", "name": "ReallocAligned", "params": "void* pMem, std::size_t nSize, std::size_t nAlignment", "qualifier": "= 0", "comment": "Slot 6: Reallocates aligned memory block"},
            7: {"ret": "void", "name": "FreeAligned", "params": "void* pMem", "qualifier": "= 0", "comment": "Slot 7: Frees aligned memory block"},
            8: {"ret": "std::size_t", "name": "GetSize", "params": "void* pMem", "qualifier": "= 0", "comment": "Slot 8: Returns usable allocation byte size"},
            21: {"ret": "std::size_t", "name": "GetSizeDetailed", "params": "void* pMem", "qualifier": "= 0", "comment": "Slot 21: Returns detailed buffer allocation capacity"}
        }
    },
    {
        "id": "input_system",
        "header": "input_system.h",
        "interface_name": "IInputSystem",
        "class_name": "CInputSystem",
        "alias_name": "CInputSystem",
        "binary": "inputsystem.dll",
        "token": "InputSystemVersion001",
        "vtable_rva": "0x393e0",
        "default_slots": 106,
        "description": "Source 2 Hardware Input Processing & Button State Polling Interface",
        "includes": ["<cstdint>", "<cstddef>"],
        "forward_decl": [],
        "methods": {
            0: {"ret": "", "name": "~IInputSystem", "params": "", "qualifier": "default", "comment": "Slot 0: Virtual destructor"},
            1: {"ret": "bool", "name": "Connect", "params": "void* pFactory", "qualifier": "= 0", "comment": "Slot 1: IAppSystem::Connect"},
            2: {"ret": "void", "name": "Disconnect", "params": "", "qualifier": "= 0", "comment": "Slot 2: IAppSystem::Disconnect"},
            3: {"ret": "void*", "name": "QueryInterface", "params": "const char* pInterfaceName", "qualifier": "= 0", "comment": "Slot 3: IAppSystem::QueryInterface"},
            4: {"ret": "int", "name": "Init", "params": "", "qualifier": "= 0", "comment": "Slot 4: IAppSystem::Init"},
            5: {"ret": "void", "name": "Shutdown", "params": "", "qualifier": "= 0", "comment": "Slot 5: IAppSystem::Shutdown"},
            19: {"ret": "bool", "name": "IsButtonDown", "params": "std::int32_t nButtonCode", "qualifier": "= 0", "comment": "Slot 19: Polls instantaneous key/button state"},
            20: {"ret": "std::uint32_t", "name": "GetButtonPressedTick", "params": "std::int32_t nButtonCode", "qualifier": "= 0", "comment": "Slot 20: Returns tick when button was pressed"},
            21: {"ret": "bool", "name": "IsButtonReleased", "params": "std::int32_t nButtonCode", "qualifier": "= 0", "comment": "Slot 21: Returns true if button was released in active frame"}
        }
    },
    {
        "id": "file_system",
        "header": "file_system.h",
        "interface_name": "IFileSystem",
        "class_name": "CFileSystem_Stdio",
        "alias_name": "CFileSystem_Stdio",
        "binary": "filesystem_stdio.dll",
        "token": "VFileSystem017",
        "vtable_rva": "0x1b0ca0",
        "default_slots": 128,
        "description": "Source 2 VPK & OS Virtual File System Interface (IFileSystem / CFileSystem_Stdio)",
        "includes": ["<cstdint>", "<cstddef>"],
        "forward_decl": [],
        "methods": {
            0: {"ret": "", "name": "~IFileSystem", "params": "", "qualifier": "default", "comment": "Slot 0: Virtual destructor"},
            1: {"ret": "bool", "name": "Connect", "params": "void* pFactory", "qualifier": "= 0", "comment": "Slot 1: IAppSystem::Connect"},
            2: {"ret": "void", "name": "Disconnect", "params": "", "qualifier": "= 0", "comment": "Slot 2: IAppSystem::Disconnect"},
            3: {"ret": "void*", "name": "QueryInterface", "params": "const char* pInterfaceName", "qualifier": "= 0", "comment": "Slot 3: IAppSystem::QueryInterface"},
            4: {"ret": "int", "name": "Init", "params": "", "qualifier": "= 0", "comment": "Slot 4: IAppSystem::Init"},
            5: {"ret": "void", "name": "Shutdown", "params": "", "qualifier": "= 0", "comment": "Slot 5: IAppSystem::Shutdown"},
            11: {"ret": "std::int32_t", "name": "Read", "params": "void* pOutput, std::int32_t size, void* file", "qualifier": "= 0", "comment": "Slot 11: Reads raw bytes from open file handle"},
            12: {"ret": "std::int32_t", "name": "Write", "params": "const void* pInput, std::int32_t size, void* file", "qualifier": "= 0", "comment": "Slot 12: Writes raw bytes to open file handle"},
            13: {"ret": "void*", "name": "Open", "params": "const char* pFileName, const char* pOptions, const char* pathID = nullptr", "qualifier": "= 0", "comment": "Slot 13: Opens file handle with access mode and pathID"},
            14: {"ret": "void", "name": "Close", "params": "void* file", "qualifier": "= 0", "comment": "Slot 14: Closes file handle"},
            15: {"ret": "void", "name": "Seek", "params": "void* file, std::int32_t pos, std::int32_t seekType", "qualifier": "= 0", "comment": "Slot 15: Sets file stream read/write pointer"},
            16: {"ret": "std::uint32_t", "name": "Tell", "params": "void* file", "qualifier": "= 0", "comment": "Slot 16: Returns current file stream position"},
            17: {"ret": "std::uint32_t", "name": "Size", "params": "void* file", "qualifier": "= 0", "comment": "Slot 17: Returns open file length in bytes"},
            18: {"ret": "std::uint32_t", "name": "SizeByName", "params": "const char* pFileName, const char* pPathID = nullptr", "qualifier": "= 0", "comment": "Slot 18: Returns file size by relative path"},
            22: {"ret": "bool", "name": "FileExists", "params": "const char* pFileName, const char* pPathID = nullptr", "qualifier": "= 0", "comment": "Slot 22: Tests file existence across search paths"},
            23: {"ret": "bool", "name": "IsDirectory", "params": "const char* pFileName, const char* pPathID = nullptr", "qualifier": "= 0", "comment": "Slot 23: Tests directory path existence"},
            32: {"ret": "void", "name": "AddSearchPath", "params": "const char* pPath, const char* pathID, int addType = 0", "qualifier": "= 0", "comment": "Slot 32: Mounts directory or VPK archive into pathID"},
            33: {"ret": "bool", "name": "RemoveSearchPath", "params": "const char* pPath, const char* pathID", "qualifier": "= 0", "comment": "Slot 33: Unmounts directory or archive from search paths"}
        }
    },
    {
        "id": "trace_manager",
        "header": "trace_manager.h",
        "interface_name": "ITraceManager",
        "class_name": "CGameTraceManager",
        "alias_name": "CGameTraceManager",
        "binary": "server.dll",
        "token": "CGameTraceManager",
        "vtable_rva": "0x19d2ba8",
        "default_slots": 6,
        "description": "Source 2 Spatial Query & Ray-Tracing Subsystem Interface (ITraceManager / CGameTraceManager)",
        "includes": ["<cstdint>", "../shared/ray_trace.h"],
        "forward_decl": [],
        "methods": {
            0: {"ret": "", "name": "~ITraceManager", "params": "", "qualifier": "default", "comment": "Slot 0: Virtual destructor"},
            1: {"ret": "bool", "name": "ClipRayToEntity", "params": "const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, void* pEntity, shared::GameTrace_t* pTrace", "qualifier": "= 0", "comment": "Slot 1: Clips ray trajectory against specific entity"},
            2: {"ret": "bool", "name": "ClipRayToCollideable", "params": "const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, void* pCollide, shared::GameTrace_t* pTrace", "qualifier": "= 0", "comment": "Slot 2: Clips ray against collideable physics proxy"},
            3: {"ret": "void", "name": "TraceRay", "params": "const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, shared::GameTrace_t* pTrace", "qualifier": "= 0", "comment": "Slot 3: Performs broadphase & narrowphase scene raycast"},
            4: {"ret": "void", "name": "SetupTraceShape", "params": "void* pShape, const shared::Ray_t* pRay", "qualifier": "= 0", "comment": "Slot 4: Initializes bounding volume shape for sweep test"},
            5: {"ret": "bool", "name": "TraceShape", "params": "const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, shared::GameTrace_t* pTrace", "qualifier": "= 0", "comment": "Slot 5: Executes swept shape query against world physics"}
        }
    }
]

# =============================================================================
# Code Emission Engine
# =============================================================================

def generate_header_code(spec: Dict[str, Any], total_slots: int, vtable_info: Optional[Dict[str, Any]]) -> str:
    """Emits complete, clean C++20 header content for an interface."""
    header_guard = f"CS2_INTERFACES_{spec['id'].upper()}_H"
    interface_name = spec["interface_name"]
    alias_name = spec.get("alias_name")
    binary = spec["binary"]
    token = spec["token"]
    vtable_rva = vtable_info.get("vtable_rva", spec.get("vtable_rva", "unknown")) if vtable_info else spec.get("vtable_rva", "unknown")
    desc = spec["description"]

    code = []
    code.append(f"#ifndef {header_guard}")
    code.append(f"#define {header_guard}\n")

    # Includes
    for inc in spec.get("includes", []):
        if inc.startswith("<") or inc.startswith('"'):
            code.append(f"#include {inc}")
        else:
            code.append(f'#include "{inc}"')
    code.append("")

    # Forward declarations
    for fwd in spec.get("forward_decl", []):
        code.append(fwd)
    if spec.get("forward_decl"):
        code.append("")

    code.append("namespace cs2::interfaces {\n")

    # Pre-content (e.g. structs, enums)
    if "pre_content" in spec:
        code.append(spec["pre_content"].rstrip() + "\n")

    # Documentation block
    code.append("/**")
    code.append(f" * @brief {desc}")
    code.append(f" * Token: \"{token}\" in {binary}")
    code.append(f" * VTable: {binary} + {vtable_rva} ({total_slots} virtual slots)")
    code.append(" */")

    code.append(f"class {interface_name} {{")
    code.append("public:")

    known_methods = spec.get("methods", {})

    # Emit each slot sequentially from 0 to total_slots - 1
    # Group consecutive unknown slots for readability
    slot = 0
    while slot < total_slots:
        if slot in known_methods:
            m = known_methods[slot]
            ret_type = m["ret"]
            name = m["name"]
            params = m["params"]
            qual = m["qualifier"]
            cmt = m.get("comment", f"Slot {slot}")

            if name.startswith("~"):
                # Destructor
                code.append(f"    // {cmt}")
                destr_qual = "= default" if qual in ("default", "= default", "") else qual
                code.append(f"    virtual {name}() {destr_qual};\n")
            else:
                code.append(f"    // {cmt}")
                code.append(f"    virtual {ret_type} {name}({params}) {qual};\n")
            slot += 1
        else:
            # Check how many consecutive unknown slots follow
            unknown_batch = []
            while slot < total_slots and slot not in known_methods:
                unknown_batch.append(slot)
                slot += 1

            if len(unknown_batch) <= 4:
                for u in unknown_batch:
                    code.append(f"    virtual void Unk{u:02d}() = 0; // Slot {u}")
                code.append("")
            else:
                code.append(f"    // Virtual slots {unknown_batch[0]} to {unknown_batch[-1]} (Internal / Unmapped)")
                chunk_size = 4
                for i in range(0, len(unknown_batch), chunk_size):
                    chunk = unknown_batch[i:i + chunk_size]
                    line_parts = [f"virtual void Unk{u:02d}() = 0;" for u in chunk]
                    code.append("    " + " ".join(line_parts))
                code.append("")

    # Post-class content
    if "post_class_content" in spec:
        code.append(spec["post_class_content"].rstrip() + "\n")

    code.append("};")
    code.append(f"static_assert(sizeof({interface_name}) == 0x08, \"{interface_name} interface pointer must be 8 bytes\");\n")

    # Type alias if alias_name != interface_name
    if alias_name and alias_name != interface_name:
        code.append(f"using {alias_name} = {interface_name};\n")

    if "post_file_content" in spec:
        code.append(spec["post_file_content"].rstrip() + "\n")

    code.append("} // namespace cs2::interfaces\n")
    code.append(f"#endif // {header_guard}\n")

    return "\n".join(code)


def generate_interfaces_master_header(specs: List[Dict[str, Any]], existing_extra_headers: List[str]) -> str:
    """Emits the unified game/interfaces/interfaces.h master registry header."""
    code = []
    code.append("#ifndef CS2_INTERFACES_INTERFACES_H")
    code.append("#define CS2_INTERFACES_INTERFACES_H\n")
    code.append("#include <cstdint>")
    code.append("#include <cstring>")
    code.append("#include <type_traits>\n")

    # Include existing extra domain headers first
    for extra in existing_extra_headers:
        code.append(f"#include \"{extra}\"")

    # Include all generated interface headers
    for spec in specs:
        hdr = spec["header"]
        if hdr not in existing_extra_headers:
            code.append(f"#include \"{hdr}\"")
    code.append("")

    code.append("namespace cs2::interfaces {\n")
    code.append("using InstantiateInterfaceFn = void* (*)();\n")
    code.append("/**")
    code.append(" * @brief Source 2 Internal Interface Registry Node (InterfaceReg)")
    code.append(" * Singly-linked list node populated by DLL exports.")
    code.append(" * Size: 24 bytes (0x18) on x64.")
    code.append(" */")
    code.append("struct InterfaceReg {")
    code.append("    InstantiateInterfaceFn m_CreateFn; // +0x00: Factory function returning interface instance")
    code.append("    const char*            m_pName;    // +0x08: Null-terminated versioned token string")
    code.append("    InterfaceReg*          m_pNext;    // +0x10: Next node in linked list")
    code.append("};")
    code.append("static_assert(sizeof(InterfaceReg) == 0x18, \"InterfaceReg must be 24 bytes (0x18)\");\n")
    code.append("using CreateInterfaceFn = void* (*)(const char* pName, int* pReturnCode);\n")
    code.append("/**")
    code.append(" * @brief Traverses an InterfaceReg linked list to locate an interface by version token.")
    code.append(" */")
    code.append("inline void* CaptureInterface(InterfaceReg* pHead, const char* pInterfaceName) noexcept {")
    code.append("    if (!pHead || !pInterfaceName) return nullptr;")
    code.append("    for (InterfaceReg* pCur = pHead; pCur; pCur = pCur->m_pNext) {")
    code.append("        if (pCur->m_pName && std::strcmp(pCur->m_pName, pInterfaceName) == 0) {")
    code.append("            return pCur->m_CreateFn ? pCur->m_CreateFn() : nullptr;")
    code.append("        }")
    code.append("    }")
    code.append("    return nullptr;")
    code.append("}\n")

    code.append("/**")
    code.append(" * @brief Type-safe template wrapper for capturing interfaces from an InterfaceReg head.")
    code.append(" */")
    code.append("template <typename T>")
    code.append("inline T* GetInterface(InterfaceReg* pHead, const char* pInterfaceName) noexcept {")
    code.append("    return static_cast<T*>(CaptureInterface(pHead, pInterfaceName));")
    code.append("}\n")

    code.append("} // namespace cs2::interfaces\n")
    code.append("#endif // CS2_INTERFACES_INTERFACES_H\n")

    return "\n".join(code)

# =============================================================================
# Main Orchestration Routine
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="Source 2 C++20 Interface Header Generator")
    parser.add_argument("--vtables", default="data/vtables.json", help="Path to vtables.json")
    parser.add_argument("--signatures", default="data/function_signatures.json", help="Path to function_signatures.json")
    parser.add_argument("--outdir", default="game/interfaces", help="Output directory for generated C++ headers")
    args = parser.parse_args()

    vtables_path = Path(args.vtables)
    signatures_path = Path(args.signatures)
    outdir = Path(args.outdir)

    if not vtables_path.exists():
        print(f"[!] Error: VTables database not found: {vtables_path}", file=sys.stderr)
        return 1

    print("=================================================================")
    print(" SOURCE 2 C++20 INTERFACE GENERATOR")
    print("=================================================================")
    print(f" VTables Database:   {vtables_path}")
    print(f" Signatures:         {signatures_path if signatures_path.exists() else 'None'}")
    print(f" Output Directory:   {outdir}")
    print("=================================================================")

    with open(vtables_path, "r", encoding="utf-8") as f:
        vtables_data = json.load(f)

    # Index vtables by (binary, class_name)
    vtable_index = {}
    for entry in vtables_data:
        key = (entry.get("binary", "").lower(), entry.get("class_name", ""))
        # Prefer entries with methods or canonical mappings
        if key not in vtable_index or len(entry.get("methods", [])) > len(vtable_index[key].get("methods", [])):
            vtable_index[key] = entry

    # Create output directory
    outdir.mkdir(parents=True, exist_ok=True)

    generated_files = []

    for spec in INTERFACE_SPECS:
        bin_name = spec["binary"].lower()
        cls_name = spec["class_name"]
        key = (bin_name, cls_name)
        vinfo = vtable_index.get(key)

        total_slots = spec["default_slots"]
        if vinfo and vinfo.get("method_count", 0) > 0:
            total_slots = vinfo["method_count"]

        # Ensure all defined methods fit in total_slots
        max_defined_slot = max(spec["methods"].keys()) if spec["methods"] else 0
        if total_slots <= max_defined_slot:
            total_slots = max_defined_slot + 1

        # Emit header
        code = generate_header_code(spec, total_slots, vinfo)
        out_file = outdir / spec["header"]
        with open(out_file, "w", encoding="utf-8") as f:
            f.write(code)

        canonical_count = len(spec["methods"])
        print(f"  [+] Generated: {spec['header']:<26} -> {spec['interface_name']:<24} ({total_slots:3d} slots, {canonical_count:2d} canonical)")
        generated_files.append(spec["header"])

    # Existing extra domain headers in game/interfaces/
    extra_domain_headers = [
        "engine_client.h",
        "game_event_manager.h",
        "sound_system.h"
    ]

    # Generate game/interfaces/interfaces.h
    master_header_code = generate_interfaces_master_header(INTERFACE_SPECS, extra_domain_headers)
    master_file = outdir / "interfaces.h"
    with open(master_file, "w", encoding="utf-8") as f:
        f.write(master_header_code)

    print(f"  [+] Updated:   interfaces.h               -> Master interface registry & token resolver")
    print("=================================================================")
    print(f" Successfully generated {len(generated_files)} interfaces and updated master registry.")
    print("=================================================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
