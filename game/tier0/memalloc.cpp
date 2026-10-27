#include "memalloc.h"
#include <cstdlib>

namespace cs2::tier0 {

// Emulated default standard heap allocator for standalone simulators and tooling
class CMemAllocStandard : public IMemAlloc {
public:
    void* Alloc(std::size_t nSize) override {
        return std::malloc(nSize);
    }

    void* Realloc(void* pMem, std::size_t nSize) override {
        return std::realloc(pMem, nSize);
    }

    void Free(void* pMem) override {
        std::free(pMem);
    }

    void* Expand(void* /*pMem*/, std::size_t /*nSize*/) override {
        return nullptr;
    }

    void* AllocAligned(std::size_t nSize, std::size_t nAlignment) override {
#if defined(_MSC_VER)
        return _aligned_malloc(nSize, nAlignment);
#else
        return std::malloc(nSize);
#endif
    }

    void* ReallocAligned(void* pMem, std::size_t nSize, std::size_t nAlignment) override {
#if defined(_MSC_VER)
        return _aligned_realloc(pMem, nSize, nAlignment);
#else
        return std::realloc(pMem, nSize);
#endif
    }

    void FreeAligned(void* pMem) override {
#if defined(_MSC_VER)
        _aligned_free(pMem);
#else
        std::free(pMem);
#endif
    }

    std::size_t GetSize(void* /*pMem*/) override {
        return 0;
    }
};

static CMemAllocStandard s_DefaultMemAlloc;
static IMemAlloc* s_pMemAlloc = &s_DefaultMemAlloc;

IMemAlloc* GetMemAlloc() {
    return s_pMemAlloc;
}

} // namespace cs2::tier0
