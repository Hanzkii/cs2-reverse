#ifndef CS2_SCENESYSTEM_H
#define CS2_SCENESYSTEM_H

#include <cstdint>
#include <cstddef>
#include "../shared/math_types.h"

namespace cs2::scenesystem {

#define CS2_PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

/**
 * @brief Animatable Scene Graph Object (CSceneAnimatableObject)
 * Reconstructed from scenesystem.dll @ 0x1805f17c0.
 */
class CSceneAnimatableObject {
public:
    virtual ~CSceneAnimatableObject() = default;

    CS2_PAD(0x18, m_pad0);
    shared::Vector3D m_vOrigin{};           // +0x20: World coordinates
    CS2_PAD(0x0C, m_pad2c);
    shared::QAngle   m_angRotation{};        // +0x38: World rotation angles
};

/**
 * @brief Source 2 Scene System Interface (CSceneSystem)
 * Token: "SceneSystem_002" in scenesystem.dll
 * VTable: +0x5F17C0 (199 slots)
 */
class CSceneSystem {
public:
    virtual ~CSceneSystem() = default;

    // Slot 8: CreateSceneView
    virtual void* CreateSceneView() = 0;

    // Slot 9: DestroySceneView
    virtual void DestroySceneView(void* pView) = 0;

    // Slot 22: GetActiveScene
    virtual void* GetActiveScene() = 0;
};

#undef CS2_PAD

} // namespace cs2::scenesystem

#endif // CS2_SCENESYSTEM_H
