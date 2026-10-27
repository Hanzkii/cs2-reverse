#include "sound_ops.h"

namespace cs2::soundsystem {

CSoundManager& CSoundManager::Get() {
    static CSoundManager s_Instance;
    return s_Instance;
}

interfaces::StartSoundEventInfo_t CSoundManager::CreateSpatialSoundInfo(
    const shared::Vector3D& origin,
    float volume,
    float pitch,
    shared::CEntityHandle entity
) noexcept {
    interfaces::StartSoundEventInfo_t info{};
    info.m_vecOrigin = origin;
    info.m_flVolume = volume;
    info.m_flPitch = pitch;
    info.m_hEntity = entity;

    // Set 3x4 identity orientation matrix at origin
    info.m_matSpatial[0][0] = 1.0f;
    info.m_matSpatial[1][1] = 1.0f;
    info.m_matSpatial[2][2] = 1.0f;
    info.m_matSpatial[0][3] = origin.x;
    info.m_matSpatial[1][3] = origin.y;
    info.m_matSpatial[2][3] = origin.z;

    return info;
}

} // namespace cs2::soundsystem
