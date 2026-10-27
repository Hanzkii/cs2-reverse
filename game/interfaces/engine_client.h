#ifndef CS2_INTERFACES_ENGINE_CLIENT_H
#define CS2_INTERFACES_ENGINE_CLIENT_H

#include <cstdint>
#include <cstddef>
#include "../shared/math_types.h"

namespace cs2::interfaces {

/**
 * @brief Source 2 Engine to Client Interface (ISource2EngineToClient)
 * Token: "Source2EngineToClient001" in engine2.dll
 * VTable RVA: +0x5d5c38 (185 slots)
 */
class ISource2EngineToClient {
public:
    virtual ~ISource2EngineToClient() = default;

    // Slot 1..26: Standard IAppSystem methods
    virtual bool Connect(void* /*pFactory*/) = 0;
    virtual void Disconnect() = 0;
    virtual void* QueryInterface(const char* /*pInterfaceName*/) = 0;
    virtual int  Init() = 0;
    virtual void Shutdown() = 0;

    // Slot 27: IsInGame (RVA: +0x1bf4b0)
    virtual bool IsInGame() = 0;

    // Slot 28: IsConnected
    virtual bool IsConnected() = 0;

    // Slot 29: IsPaused
    virtual bool IsPaused() = 0;

    // Slot 30: GetMaxClients
    virtual int  GetMaxClients() = 0;

    // Slot 31: GetLevelName
    virtual const char* GetLevelName() = 0;

    // Slot 32: GetLevelNameShort
    virtual const char* GetLevelNameShort() = 0;

    // Slot 33: GetScreenSize
    virtual void GetScreenSize(int& width, int& height) = 0;

    // Slot 35: GetLocalPlayerSlot (0-based client index)
    virtual int  GetLocalPlayerSlot() = 0;
};

/**
 * @brief Network Game Client Service (CNetworkGameClient)
 * Token: "NetworkClientService_001" in engine2.dll
 * VTable RVA: +0x613ac8 (64 slots)
 */
class CNetworkGameClient {
public:
    virtual ~CNetworkGameClient() = default;

    // Slot 23: IsConnected
    virtual bool IsConnected() = 0;

    // Slot 24: GetDeltaFrame
    virtual int  GetDeltaFrame() = 0;

    // Slot 25: GetServerTick
    virtual int  GetServerTick() = 0;

    // Slot 26: GetClientTick
    virtual int  GetClientTick() = 0;

    // Slot 27: GetLatency (round-trip time in seconds)
    virtual float GetLatency() = 0;
};

/**
 * @brief Game Resource Service Client (CGameResourceService)
 * Token: "GameResourceServiceClientV001" in engine2.dll
 * VTable RVA: +0x60e7c8 (182 slots)
 */
class CGameResourceService {
public:
    virtual ~CGameResourceService() = default;

    // Slot 58: GetGameEntitySystem (client.dll + 0x271c8b8)
    virtual void* GetGameEntitySystem() = 0;
};

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_ENGINE_CLIENT_H
