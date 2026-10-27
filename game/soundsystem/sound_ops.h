#ifndef CS2_SOUNDSYSTEM_SOUND_OPS_H
#define CS2_SOUNDSYSTEM_SOUND_OPS_H

#include <cstdint>
#include <string_view>
#include "../interfaces/sound_system.h"
#include "../tier0/utl_string_token.h"

namespace cs2::soundsystem {

/**
 * @brief Sound operator helper & event dispatcher
 * Reconstructed from soundsystem.dll @ 0x1804ab140 and client.dll + 0xB4FB20
 */
class CSoundManager {
public:
    static CSoundManager& Get();

    /**
     * @brief Computes 32-bit MurmurHash2 sound event identifier
     */
    static constexpr std::uint32_t HashSoundEvent(std::string_view name) noexcept {
        return tier0::CUtlStringToken::Hash(name);
    }

    /**
     * @brief Formats a 3D sound event parameter packet
     */
    static interfaces::StartSoundEventInfo_t CreateSpatialSoundInfo(
        const shared::Vector3D& origin,
        float volume = 1.0f,
        float pitch = 1.0f,
        shared::CEntityHandle entity = shared::CEntityHandle{}
    ) noexcept;
};

} // namespace cs2::soundsystem

#endif // CS2_SOUNDSYSTEM_SOUND_OPS_H
