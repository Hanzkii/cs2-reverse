#ifndef CS2_INTERFACES_SOUND_SYSTEM_H
#define CS2_INTERFACES_SOUND_SYSTEM_H

#include <cstdint>
#include <cstddef>
#include "../shared/math_types.h"
#include "../shared/entity_handle.h"

namespace cs2::interfaces {

#define CS2_PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

/**
 * @brief Dispatch Sound Event Parameter Payload (StartSoundEventInfo_t)
 * Total Size: exactly 128 bytes (0x80).
 */
struct StartSoundEventInfo_t {
    shared::Vector3D     m_vecOrigin{};          // +0x00: 3D spatial emitter coordinates
    shared::QAngle       m_angAngles{};          // +0x0C: Spatial directional orientation
    float                m_flVolume{ 1.0f };     // +0x18: Linear gain amplitude [0.0..1.0]
    float                m_flPitch{ 1.0f };      // +0x1C: Frequency pitch shift factor
    shared::CEntityHandle m_hEntity{};           // +0x20: Associated emitter entity handle
    CS2_PAD(0x0C, m_pad24);                      // +0x24: Padding to 16-byte alignment
    float                m_matSpatial[3][4]{};   // +0x30: 3x4 spatial coordinate transform
    CS2_PAD(0x20, m_pad60);                      // +0x60: DSP runtime context payload
};
static_assert(sizeof(StartSoundEventInfo_t) == 0x80, "StartSoundEventInfo_t must be 128 bytes (0x80)");

/**
 * @brief Source 2 Sound Operator Subsystem (ISoundOpSystem)
 * Token: "SoundOpSystem001" in soundsystem.dll
 * VTable: 0x1804AB140 (16 slots)
 */
class ISoundOpSystem {
public:
    virtual ~ISoundOpSystem() = default;

    // Slot 1..10: IAppSystem methods
    virtual bool Connect(void* /*pFactory*/) = 0;
    virtual void Disconnect() = 0;
    virtual void* QueryInterface(const char* /*pInterfaceName*/) = 0;
    virtual int  Init() = 0;
    virtual void Shutdown() = 0;

    // Slot 11: StartSoundEvent
    virtual std::uint64_t StartSoundEvent(const char* pEventName, const StartSoundEventInfo_t* pInfo) = 0;

    // Slot 12: StartSoundEventByHash
    virtual std::uint64_t StartSoundEventByHash(std::uint32_t nEventHash, const StartSoundEventInfo_t* pInfo) = 0;

    // Slot 13: StartSoundEventReliable
    virtual std::uint64_t StartSoundEventReliable(std::uint32_t nEventHash, const StartSoundEventInfo_t* pInfo) = 0;

    // Slot 14: StopSoundEvent
    virtual void StopSoundEvent(std::uint64_t nGuid, float flFadeTime) = 0;

    // Slot 15: SetSoundEventParam
    virtual void SetSoundEventParam(std::uint64_t nGuid, const char* pParamName, float flValue) = 0;
};

#undef CS2_PAD

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_SOUND_SYSTEM_H
