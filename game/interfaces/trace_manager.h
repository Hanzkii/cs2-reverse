#ifndef CS2_INTERFACES_TRACE_MANAGER_H
#define CS2_INTERFACES_TRACE_MANAGER_H

#include <cstdint>
#include "../shared/ray_trace.h"

namespace cs2::interfaces {

/**
 * @brief Spatial Query Master Interface (CGameTraceManager / IEngineTrace)
 * Global Instance in client.dll: +0x02229020 (VA: 0x182229020)
 * Static BSS Instance in client.dll: +0x0254cf30 (VA: 0x18254cf30)
 */
class CGameTraceManager {
public:
    virtual ~CGameTraceManager() = default;

    // Slot 1: ClipRayToEntity (RVA: +0x00a1b8a0, VA: 0x180a1b8a0)
    virtual bool ClipRayToEntity(const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, void* pEntity, shared::GameTrace_t* pTrace) = 0;

    // Slot 2: ClipRayToCollideable
    virtual bool ClipRayToCollideable(const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, void* pCollide, shared::GameTrace_t* pTrace) = 0;

    // Slot 3: TraceRay
    virtual void TraceRay(const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, shared::GameTrace_t* pTrace) = 0;

    // Slot 4: SetupTraceShape
    virtual void SetupTraceShape(void* pShape, const shared::Ray_t* pRay) = 0;

    // Slot 5: TraceShape (RVA: +0x00a1ae30, VA: 0x180a1ae30)
    virtual bool TraceShape(const shared::Ray_t* pRay, const shared::CTraceFilter* pFilter, shared::GameTrace_t* pTrace) = 0;
};

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_TRACE_MANAGER_H
