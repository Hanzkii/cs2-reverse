#ifndef CS2_TIER0_UTL_VECTOR_H
#define CS2_TIER0_UTL_VECTOR_H

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <initializer_list>
#include "memalloc.h"

namespace cs2::tier0 {

/**
 * @brief Valve Dynamic Vector Container (CUtlVector<T>)
 * Standard dynamic array layout across Source 2 engines.
 * Size: 24 bytes (0x18) on x64 (m_Size + padding + m_pMemory + m_nAllocationCount).
 */
template <typename T>
class CUtlVector {
public:
    int   m_Size{0};
    T*    m_pMemory{nullptr};
    int   m_nAllocationCount{0};
    int   m_nGrowSize{0};

    CUtlVector() = default;

    CUtlVector(int nGrowSize, int nInitAllocationCount)
        : m_nGrowSize(nGrowSize) {
        Grow(nInitAllocationCount);
    }

    ~CUtlVector() {
        Purge();
    }

    [[nodiscard]] int Count() const noexcept { return m_Size; }
    [[nodiscard]] int Size() const noexcept { return m_Size; }
    [[nodiscard]] bool IsEmpty() const noexcept { return m_Size == 0; }
    [[nodiscard]] int Capacity() const noexcept { return m_nAllocationCount; }

    T& operator[](int i) {
        assert(i >= 0 && i < m_Size);
        return m_pMemory[i];
    }

    const T& operator[](int i) const {
        assert(i >= 0 && i < m_Size);
        return m_pMemory[i];
    }

    T* Base() noexcept { return m_pMemory; }
    const T* Base() const noexcept { return m_pMemory; }

    int AddToTail(const T& src) {
        if (m_Size >= m_nAllocationCount) {
            Grow(m_nGrowSize > 0 ? m_nGrowSize : (m_nAllocationCount ? m_nAllocationCount : 4));
        }
        m_pMemory[m_Size] = src;
        return m_Size++;
    }

    void Purge() {
        if (m_pMemory) {
            GetMemAlloc()->Free(m_pMemory);
            m_pMemory = nullptr;
        }
        m_Size = 0;
        m_nAllocationCount = 0;
    }

    void RemoveAll() {
        m_Size = 0;
    }

private:
    void Grow(int nCount) {
        const int nNewAllocationCount = m_nAllocationCount + nCount;
        if (m_pMemory) {
            m_pMemory = static_cast<T*>(GetMemAlloc()->Realloc(m_pMemory, nNewAllocationCount * sizeof(T)));
        } else {
            m_pMemory = static_cast<T*>(GetMemAlloc()->Alloc(nNewAllocationCount * sizeof(T)));
        }
        m_nAllocationCount = nNewAllocationCount;
    }
};

} // namespace cs2::tier0

#endif // CS2_TIER0_UTL_VECTOR_H
