#ifndef CS2_INTERFACES_NETWORK_SERVER_SERVICE_H
#define CS2_INTERFACES_NETWORK_SERVER_SERVICE_H

#include <cstdint>
#include <cstddef>

namespace cs2::interfaces {

/**
 * @brief Source 2 Network Server Loop & Hosting Service Interface
 * Token: "NetworkServerService_001" in engine2.dll
 * VTable: engine2.dll + 0x60fa40 (51 virtual slots)
 */
class INetworkServerService {
public:
    // Slot 0: Virtual destructor
    virtual ~INetworkServerService() = default;

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

    // Virtual slots 6 to 22 (Internal / Unmapped)
    virtual void Unk06() = 0; virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0; virtual void Unk11() = 0; virtual void Unk12() = 0; virtual void Unk13() = 0;
    virtual void Unk14() = 0; virtual void Unk15() = 0; virtual void Unk16() = 0; virtual void Unk17() = 0;
    virtual void Unk18() = 0; virtual void Unk19() = 0; virtual void Unk20() = 0; virtual void Unk21() = 0;
    virtual void Unk22() = 0;

    // Slot 23: Returns active IGameServer instance
    virtual void* GetIGameServer() = 0;

    // Slot 24: Returns active INetChannel for client slot
    virtual void* GetNetworkChannel(int nSlot = 0) = 0;

    // Virtual slots 25 to 50 (Internal / Unmapped)
    virtual void Unk25() = 0; virtual void Unk26() = 0; virtual void Unk27() = 0; virtual void Unk28() = 0;
    virtual void Unk29() = 0; virtual void Unk30() = 0; virtual void Unk31() = 0; virtual void Unk32() = 0;
    virtual void Unk33() = 0; virtual void Unk34() = 0; virtual void Unk35() = 0; virtual void Unk36() = 0;
    virtual void Unk37() = 0; virtual void Unk38() = 0; virtual void Unk39() = 0; virtual void Unk40() = 0;
    virtual void Unk41() = 0; virtual void Unk42() = 0; virtual void Unk43() = 0; virtual void Unk44() = 0;
    virtual void Unk45() = 0; virtual void Unk46() = 0; virtual void Unk47() = 0; virtual void Unk48() = 0;
    virtual void Unk49() = 0; virtual void Unk50() = 0;

};
static_assert(sizeof(INetworkServerService) == 0x08, "INetworkServerService interface pointer must be 8 bytes");

using CNetworkServerService = INetworkServerService;

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_NETWORK_SERVER_SERVICE_H
