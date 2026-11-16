#ifndef CS2_INTERFACES_MEMALLOC_H
#define CS2_INTERFACES_MEMALLOC_H

#include <cstdint>
#include <cstddef>

namespace cs2::interfaces {

/**
 * @brief Source 2 Tier0 Heap Allocator Interface (IMemAlloc)
 * Token: "g_pMemAlloc" in tier0.dll
 * VTable: tier0.dll + 0x3277c0 (40 virtual slots)
 */
class IMemAlloc {
public:
    // Slot 0: Virtual destructor
    virtual ~IMemAlloc() = default;

    // Slot 1: Allocates raw memory block of nSize bytes
    virtual void* Alloc(std::size_t nSize) = 0;

    // Slot 2: Reallocates memory block to new size
    virtual void* Realloc(void* pMem, std::size_t nSize) = 0;

    // Slot 3: Frees previously allocated memory block
    virtual void Free(void* pMem) = 0;

    // Slot 4: Expands memory block in-place
    virtual void* Expand(void* pMem, std::size_t nSize) = 0;

    // Slot 5: Allocates aligned memory block
    virtual void* AllocAligned(std::size_t nSize, std::size_t nAlignment) = 0;

    // Slot 6: Reallocates aligned memory block
    virtual void* ReallocAligned(void* pMem, std::size_t nSize, std::size_t nAlignment) = 0;

    // Slot 7: Frees aligned memory block
    virtual void FreeAligned(void* pMem) = 0;

    // Slot 8: Returns usable allocation byte size
    virtual std::size_t GetSize(void* pMem) = 0;

    // Virtual slots 9 to 20 (Internal / Unmapped)
    virtual void Unk09() = 0; virtual void Unk10() = 0; virtual void Unk11() = 0; virtual void Unk12() = 0;
    virtual void Unk13() = 0; virtual void Unk14() = 0; virtual void Unk15() = 0; virtual void Unk16() = 0;
    virtual void Unk17() = 0; virtual void Unk18() = 0; virtual void Unk19() = 0; virtual void Unk20() = 0;

    // Slot 21: Returns detailed buffer allocation capacity
    virtual std::size_t GetSizeDetailed(void* pMem) = 0;

    // Virtual slots 22 to 39 (Internal / Unmapped)
    virtual void Unk22() = 0; virtual void Unk23() = 0; virtual void Unk24() = 0; virtual void Unk25() = 0;
    virtual void Unk26() = 0; virtual void Unk27() = 0; virtual void Unk28() = 0; virtual void Unk29() = 0;
    virtual void Unk30() = 0; virtual void Unk31() = 0; virtual void Unk32() = 0; virtual void Unk33() = 0;
    virtual void Unk34() = 0; virtual void Unk35() = 0; virtual void Unk36() = 0; virtual void Unk37() = 0;
    virtual void Unk38() = 0; virtual void Unk39() = 0;

};
static_assert(sizeof(IMemAlloc) == 0x08, "IMemAlloc interface pointer must be 8 bytes");

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_MEMALLOC_H
