#pragma once

// ============================================================================
// Source 2 Reconstructed Master SDK (Counter-Strike 2)
// Target Build: 2026-10-25_26144732 (CS2 Update Archive)
// Architecture: x86-64 (MSVC ABI)
// Language Standard: C++20 (/std:c++20 /W4 /WX)
// 
// Complete clean-room reconstructed SDK reverse-engineered from synchronized
// CS2 Ghidra project and verified against native PE binaries.
//
// Modular Components:
//   - Phase 0: interfaces_registry.hpp   (Multi-binary CreateInterface registry & core engine VTables)
//   - Phase 1: schema_system.hpp         (CSchemaSystem reflection & dynamic field traversal)
//   - Phase 1: entity_system.hpp         (CGameEntitySystem 2D chunk table & handle arithmetic)
//   - Phase 1: client_classes.hpp        (C_BaseEntity, C_CSPlayerPawn, Controller, PlantedC4)
//   - Phase 2: render_scene_material.hpp (VMatrix, CViewRender, studio hitboxes, scene objects)
//   - Phase 3: input_usercmd.hpp         (CCSGOInput, CUserCmd, Protobuf wire serialization, subtick)
//   - Phase 4: trace_events_aux.hpp      (CGameTraceManager, IGameEventManager2, SoundOpSystem, VData)
// ============================================================================

#include "interfaces_registry.hpp"
#include "schema_system.hpp"
#include "entity_system.hpp"
#include "client_classes.hpp"
#include "render_scene_material.hpp"
#include "input_usercmd.hpp"
#include "trace_events_aux.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace cs2::sdk {

/**
 * @brief High-level helper accessors for reversed Source 2 singletons.
 */
class SDK {
public:
    // Engine Client Interface ("Source2EngineToClient001")
    [[nodiscard]] static inline ISource2EngineToClient* GetEngineClient() noexcept {
#ifdef _WIN32
        const auto modBase = reinterpret_cast<std::uintptr_t>(::GetModuleHandleA("engine2.dll"));
        if (!modBase) return nullptr;
        return *reinterpret_cast<ISource2EngineToClient**>(modBase + 0x5d5c38);
#else
        return nullptr;
#endif
    }

    // Schema System Interface ("SchemaSystem_001")
    [[nodiscard]] static inline ISchemaSystem* GetSchemaSystem() noexcept {
#ifdef _WIN32
        const auto modBase = reinterpret_cast<std::uintptr_t>(::GetModuleHandleA("schemasystem.dll"));
        if (!modBase) return nullptr;
        return *reinterpret_cast<ISchemaSystem**>(modBase + 0x3e1bc0);
#else
        return nullptr;
#endif
    }

    // Client Entity System Singleton (client.dll + 0x271c8b8)
    [[nodiscard]] static inline CGameEntitySystem* GetEntitySystem() noexcept {
#ifdef _WIN32
        const auto modBase = reinterpret_cast<std::uintptr_t>(::GetModuleHandleA("client.dll"));
        if (!modBase) return nullptr;
        return *reinterpret_cast<CGameEntitySystem**>(modBase + 0x271c8b8);
#else
        return nullptr;
#endif
    }

    // CSGO Input Subsystem Singleton (client.dll + 0x223dc00)
    [[nodiscard]] static inline CCSGOInput* GetCSGOInput() noexcept {
#ifdef _WIN32
        const auto modBase = reinterpret_cast<std::uintptr_t>(::GetModuleHandleA("client.dll"));
        if (!modBase) return nullptr;
        return *reinterpret_cast<CCSGOInput**>(modBase + 0x0223dc00);
#else
        return nullptr;
#endif
    }

    // Global View-Projection Matrix (client.dll + 0x256cfe0)
    [[nodiscard]] static inline const VMatrix* GetViewMatrix(int nViewIndex = 0) noexcept {
#ifdef _WIN32
        const auto modBase = reinterpret_cast<std::uintptr_t>(::GetModuleHandleA("client.dll"));
        if (!modBase) return nullptr;
        const auto* pMatrices = reinterpret_cast<const VMatrix*>(modBase + 0x256cfe0);
        return &pMatrices[nViewIndex];
#else
        (void)nViewIndex;
        return nullptr;
#endif
    }

    // Spatial Query Manager (client.dll + 0x02229020)
    [[nodiscard]] static inline CGameTraceManager* GetGameTraceManager() noexcept {
#ifdef _WIN32
        const auto modBase = reinterpret_cast<std::uintptr_t>(::GetModuleHandleA("client.dll"));
        if (!modBase) return nullptr;
        return *reinterpret_cast<CGameTraceManager**>(modBase + 0x02229020);
#else
        return nullptr;
#endif
    }

    // Game Event Manager Interface ("GAMEEVENTSMANAGER002", client.dll + 0x02228E40)
    [[nodiscard]] static inline IGameEventManager2* GetGameEventManager() noexcept {
#ifdef _WIN32
        const auto modBase = reinterpret_cast<std::uintptr_t>(::GetModuleHandleA("client.dll"));
        if (!modBase) return nullptr;
        return *reinterpret_cast<IGameEventManager2**>(modBase + 0x02228E40);
#else
        return nullptr;
#endif
    }

    // Audio Subsystem Interface ("SoundOpSystem001", client.dll + 0x0279EE90)
    [[nodiscard]] static inline ISoundOpSystem* GetSoundOpSystem() noexcept {
#ifdef _WIN32
        const auto modBase = reinterpret_cast<std::uintptr_t>(::GetModuleHandleA("client.dll"));
        if (!modBase) return nullptr;
        return *reinterpret_cast<ISoundOpSystem**>(modBase + 0x0279EE90);
#else
        return nullptr;
#endif
    }
};

} // namespace cs2::sdk
