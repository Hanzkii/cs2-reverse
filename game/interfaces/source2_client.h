#ifndef CS2_INTERFACES_SOURCE2_CLIENT_H
#define CS2_INTERFACES_SOURCE2_CLIENT_H

#include <cstdint>
#include <cstddef>

namespace cs2::interfaces {

/**
 * @brief Source 2 Game Client Subsystem & Presentation Dispatcher Interface
 * Token: "Source2Client002" in client.dll
 * VTable: client.dll + 0x1c5aa90 (211 virtual slots)
 */
class ISource2Client {
public:
    // Slot 0: Virtual destructor
    virtual ~ISource2Client() = default;

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

    // Slot 6: Client post-initialization hook
    virtual void PostInit() = 0;

    // Slot 7: Client pre-shutdown notification
    virtual void PreShutdown() = 0;

    // Slot 8: Client game startup
    virtual void GameInit() = 0;

    // Slot 9: Client game termination
    virtual void GameShutdown() = 0;

    // Slot 10: Map load prior to entity spawn
    virtual void LevelInitPreEntity(const char* pszMapName) = 0;

    // Slot 11: Map load following entity spawn
    virtual void LevelInitPostEntity() = 0;

    // Slot 12: Map purge prior to entity destruction
    virtual void LevelShutdownPreEntity() = 0;

    // Slot 13: Map purge following entity destruction
    virtual void LevelShutdownPostEntity() = 0;

    // Virtual slots 14 to 210 (Internal / Unmapped)
    virtual void Unk14() = 0; virtual void Unk15() = 0; virtual void Unk16() = 0; virtual void Unk17() = 0;
    virtual void Unk18() = 0; virtual void Unk19() = 0; virtual void Unk20() = 0; virtual void Unk21() = 0;
    virtual void Unk22() = 0; virtual void Unk23() = 0; virtual void Unk24() = 0; virtual void Unk25() = 0;
    virtual void Unk26() = 0; virtual void Unk27() = 0; virtual void Unk28() = 0; virtual void Unk29() = 0;
    virtual void Unk30() = 0; virtual void Unk31() = 0; virtual void Unk32() = 0; virtual void Unk33() = 0;
    virtual void Unk34() = 0; virtual void Unk35() = 0; virtual void Unk36() = 0; virtual void Unk37() = 0;
    virtual void Unk38() = 0; virtual void Unk39() = 0; virtual void Unk40() = 0; virtual void Unk41() = 0;
    virtual void Unk42() = 0; virtual void Unk43() = 0; virtual void Unk44() = 0; virtual void Unk45() = 0;
    virtual void Unk46() = 0; virtual void Unk47() = 0; virtual void Unk48() = 0; virtual void Unk49() = 0;
    virtual void Unk50() = 0; virtual void Unk51() = 0; virtual void Unk52() = 0; virtual void Unk53() = 0;
    virtual void Unk54() = 0; virtual void Unk55() = 0; virtual void Unk56() = 0; virtual void Unk57() = 0;
    virtual void Unk58() = 0; virtual void Unk59() = 0; virtual void Unk60() = 0; virtual void Unk61() = 0;
    virtual void Unk62() = 0; virtual void Unk63() = 0; virtual void Unk64() = 0; virtual void Unk65() = 0;
    virtual void Unk66() = 0; virtual void Unk67() = 0; virtual void Unk68() = 0; virtual void Unk69() = 0;
    virtual void Unk70() = 0; virtual void Unk71() = 0; virtual void Unk72() = 0; virtual void Unk73() = 0;
    virtual void Unk74() = 0; virtual void Unk75() = 0; virtual void Unk76() = 0; virtual void Unk77() = 0;
    virtual void Unk78() = 0; virtual void Unk79() = 0; virtual void Unk80() = 0; virtual void Unk81() = 0;
    virtual void Unk82() = 0; virtual void Unk83() = 0; virtual void Unk84() = 0; virtual void Unk85() = 0;
    virtual void Unk86() = 0; virtual void Unk87() = 0; virtual void Unk88() = 0; virtual void Unk89() = 0;
    virtual void Unk90() = 0; virtual void Unk91() = 0; virtual void Unk92() = 0; virtual void Unk93() = 0;
    virtual void Unk94() = 0; virtual void Unk95() = 0; virtual void Unk96() = 0; virtual void Unk97() = 0;
    virtual void Unk98() = 0; virtual void Unk99() = 0; virtual void Unk100() = 0; virtual void Unk101() = 0;
    virtual void Unk102() = 0; virtual void Unk103() = 0; virtual void Unk104() = 0; virtual void Unk105() = 0;
    virtual void Unk106() = 0; virtual void Unk107() = 0; virtual void Unk108() = 0; virtual void Unk109() = 0;
    virtual void Unk110() = 0; virtual void Unk111() = 0; virtual void Unk112() = 0; virtual void Unk113() = 0;
    virtual void Unk114() = 0; virtual void Unk115() = 0; virtual void Unk116() = 0; virtual void Unk117() = 0;
    virtual void Unk118() = 0; virtual void Unk119() = 0; virtual void Unk120() = 0; virtual void Unk121() = 0;
    virtual void Unk122() = 0; virtual void Unk123() = 0; virtual void Unk124() = 0; virtual void Unk125() = 0;
    virtual void Unk126() = 0; virtual void Unk127() = 0; virtual void Unk128() = 0; virtual void Unk129() = 0;
    virtual void Unk130() = 0; virtual void Unk131() = 0; virtual void Unk132() = 0; virtual void Unk133() = 0;
    virtual void Unk134() = 0; virtual void Unk135() = 0; virtual void Unk136() = 0; virtual void Unk137() = 0;
    virtual void Unk138() = 0; virtual void Unk139() = 0; virtual void Unk140() = 0; virtual void Unk141() = 0;
    virtual void Unk142() = 0; virtual void Unk143() = 0; virtual void Unk144() = 0; virtual void Unk145() = 0;
    virtual void Unk146() = 0; virtual void Unk147() = 0; virtual void Unk148() = 0; virtual void Unk149() = 0;
    virtual void Unk150() = 0; virtual void Unk151() = 0; virtual void Unk152() = 0; virtual void Unk153() = 0;
    virtual void Unk154() = 0; virtual void Unk155() = 0; virtual void Unk156() = 0; virtual void Unk157() = 0;
    virtual void Unk158() = 0; virtual void Unk159() = 0; virtual void Unk160() = 0; virtual void Unk161() = 0;
    virtual void Unk162() = 0; virtual void Unk163() = 0; virtual void Unk164() = 0; virtual void Unk165() = 0;
    virtual void Unk166() = 0; virtual void Unk167() = 0; virtual void Unk168() = 0; virtual void Unk169() = 0;
    virtual void Unk170() = 0; virtual void Unk171() = 0; virtual void Unk172() = 0; virtual void Unk173() = 0;
    virtual void Unk174() = 0; virtual void Unk175() = 0; virtual void Unk176() = 0; virtual void Unk177() = 0;
    virtual void Unk178() = 0; virtual void Unk179() = 0; virtual void Unk180() = 0; virtual void Unk181() = 0;
    virtual void Unk182() = 0; virtual void Unk183() = 0; virtual void Unk184() = 0; virtual void Unk185() = 0;
    virtual void Unk186() = 0; virtual void Unk187() = 0; virtual void Unk188() = 0; virtual void Unk189() = 0;
    virtual void Unk190() = 0; virtual void Unk191() = 0; virtual void Unk192() = 0; virtual void Unk193() = 0;
    virtual void Unk194() = 0; virtual void Unk195() = 0; virtual void Unk196() = 0; virtual void Unk197() = 0;
    virtual void Unk198() = 0; virtual void Unk199() = 0; virtual void Unk200() = 0; virtual void Unk201() = 0;
    virtual void Unk202() = 0; virtual void Unk203() = 0; virtual void Unk204() = 0; virtual void Unk205() = 0;
    virtual void Unk206() = 0; virtual void Unk207() = 0; virtual void Unk208() = 0; virtual void Unk209() = 0;
    virtual void Unk210() = 0;

};
static_assert(sizeof(ISource2Client) == 0x08, "ISource2Client interface pointer must be 8 bytes");

using CSource2Client = ISource2Client;

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_SOURCE2_CLIENT_H
