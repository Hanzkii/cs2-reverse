#ifndef CS2_INTERFACES_TRACE_MANAGER_H
#define CS2_INTERFACES_TRACE_MANAGER_H

#include <cstdint>
#include "../shared/ray_trace.h"

namespace cs2::interfaces {

/**
 * @brief Source 2 Spatial Query & Ray-Tracing Subsystem Interface (ITraceManager / CGameTraceManager)
 * Token: "CGameTraceManager" in server.dll
 * VTable: server.dll + 0x19d2ba8 (6 virtual slots)
 */
class ITraceManager {
public:
    // Slot 0: Virtual destructor
    virtual ~ITraceManager() = default;

    // Slot 1: Clips ray trajectory against specific entity
    virtual bool ClipRayToEntity(const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, void* pEntity, shared::GameTrace_t* pTrace) = 0;

    // Slot 2: Clips ray against collideable physics proxy
    virtual bool ClipRayToCollideable(const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, void* pCollide, shared::GameTrace_t* pTrace) = 0;

    // Slot 3: Performs broadphase & narrowphase scene raycast
    virtual void TraceRay(const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, shared::GameTrace_t* pTrace) = 0;

    // Slot 4: Initializes bounding volume shape for sweep test
    virtual void SetupTraceShape(void* pShape, const shared::Ray_t* pRay) = 0;

    // Slot 5: Executes swept shape query against world physics
    virtual bool TraceShape(const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, shared::GameTrace_t* pTrace) = 0;

};
static_assert(sizeof(ITraceManager) == 0x08, "ITraceManager interface pointer must be 8 bytes");

using CGameTraceManager = ITraceManager;

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_TRACE_MANAGER_H
