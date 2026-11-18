#pragma once

#include "game/server/player_controller.h"

namespace cs2::sdk::server {
    using CEntityHandle = cs2::server::CEntityHandle;
    template <typename T>
    using CHandle = cs2::server::CHandle<T>;
    using CEntityIdentity = cs2::server::CEntityIdentity;
    using CEntityInstance = cs2::server::CEntityInstance;
    using CBaseEntity = cs2::server::CBaseEntity;
    using CBasePlayerController = cs2::server::CBasePlayerController;
    using CCSPlayerController = cs2::server::CCSPlayerController;
    using CBasePlayerPawn = cs2::server::CBasePlayerPawn;
    using CCSPlayerPawnBase = cs2::server::CCSPlayerPawnBase;
    using CCSPlayerPawn = cs2::server::CCSPlayerPawn;
} // namespace cs2::sdk::server
