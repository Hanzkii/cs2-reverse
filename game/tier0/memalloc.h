#ifndef CS2_TIER0_MEMALLOC_H
#define CS2_TIER0_MEMALLOC_H

#include <cstdint>
#include <cstddef>

namespace cs2::tier0 {

/**
 * @brief Valve Memory Allocator Interface (IMemAlloc)
 * Exported by tier0.dll as 'g_pMemAlloc' (Export RVA: +0x3ac458, VTable RVA: +0x3277d0)
 * 85 virtual method slots.
 */
class IMemAlloc {
public:
    virtual ~IMemAlloc() = default;

    // Slot 1: Alloc
    virtual void* Alloc(std::size_t nSize) = 0;

    // Slot 2: Realloc
    virtual void* Realloc(void* pMem, std::size_t nSize) = 0;

    // Slot 3: Free
    virtual void  Free(void* pMem) = 0;

    // Slot 4: Expand
    virtual void* Expand(void* pMem, std::size_t nSize) = 0;

    // Slot 5: Alloc aligned
    virtual void* AllocAligned(std::size_t nSize, std::size_t nAlignment) = 0;

    // Slot 6: Realloc aligned
    virtual void* ReallocAligned(void* pMem, std::size_t nSize, std::size_t nAlignment) = 0;

    // Slot 7: Free aligned
    virtual void  FreeAligned(void* pMem) = 0;

    // Slot 8: GetSize
    virtual std::size_t GetSize(void* pMem) = 0;
};

/**
 * @brief Retrieves the authoritative global memory allocator instance.
 */
IMemAlloc* GetMemAlloc();

} // namespace cs2::tier0

#endif // CS2_TIER0_MEMALLOC_H
