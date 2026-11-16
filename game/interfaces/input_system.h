#ifndef CS2_INTERFACES_INPUT_SYSTEM_H
#define CS2_INTERFACES_INPUT_SYSTEM_H

#include <cstdint>
#include <cstddef>

namespace cs2::interfaces {

/**
 * @brief Source 2 Hardware Input Processing & Button State Polling Interface
 * Token: "InputSystemVersion001" in inputsystem.dll
 * VTable: inputsystem.dll + 0x393e0 (106 virtual slots)
 */
class IInputSystem {
public:
    // Slot 0: Virtual destructor
    virtual ~IInputSystem() = default;

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

    // Virtual slots 6 to 18 (Internal / Unmapped)
    virtual void Unk06() = 0; virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0; virtual void Unk11() = 0; virtual void Unk12() = 0; virtual void Unk13() = 0;
    virtual void Unk14() = 0; virtual void Unk15() = 0; virtual void Unk16() = 0; virtual void Unk17() = 0;
    virtual void Unk18() = 0;

    // Slot 19: Polls instantaneous key/button state
    virtual bool IsButtonDown(std::int32_t nButtonCode) = 0;

    // Slot 20: Returns tick when button was pressed
    virtual std::uint32_t GetButtonPressedTick(std::int32_t nButtonCode) = 0;

    // Slot 21: Returns true if button was released in active frame
    virtual bool IsButtonReleased(std::int32_t nButtonCode) = 0;

    // Virtual slots 22 to 105 (Internal / Unmapped)
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

};
static_assert(sizeof(IInputSystem) == 0x08, "IInputSystem interface pointer must be 8 bytes");

using CInputSystem = IInputSystem;

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_INPUT_SYSTEM_H
