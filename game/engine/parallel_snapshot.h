//===========================================================================//
// Valve Source 2 Engine - Clean-Room SDK
// Subsystem: Server Parallel Snapshot & Asynchronous Dispatch Pipeline
// Target Binary: engine2.dll (CS2 Build 2026-11-11_26598758)
//===========================================================================//

#pragma once

#include <cstdint>
#include <atomic>
#include <concepts>
#include <span>

namespace cs2::engine2 {

enum class ParallelSendSnapshotMode : int32_t {
    SERIAL_MAIN_THREAD      = 0, // Synchronous execution on main server thread
    ASYNC_LISTEN_ONLY       = 1, // Asynchronous on listen clients, synchronous on dedicated
    ASYNC_UNCONDITIONAL     = 2, // Asynchronous send jobs across all servers
    PARALLEL_TICK_BLOCKED   = 3, // Parallel send jobs with end-of-tick barrier
    PARALLEL_DAG_CHAINED    = 4  // Main server clients parallel, then HLTV jobs via DAG
};

enum class JobPriority_t : int32_t {
    PRIORITY_CRITICAL = 0,
    PRIORITY_HIGH     = 1,
    PRIORITY_NORMAL   = 2,
    PRIORITY_LOW      = 3,
    PRIORITY_CLEANUP  = 4
};

// 32-byte snapshot entity descriptor entry inside 1024-element chunks
struct alignas(8) FrameSnapshotEntityEntry_t {
    uint32_t m_nEntityIndex;     // +0x00
    uint32_t m_nClassID;         // +0x04
    uint32_t m_nSerialNumber;    // +0x08
    uint32_t m_nFlags;           // +0x0C
    void*    m_pBaseline;        // +0x10
    uint32_t m_nChangeTick;      // +0x18
    uint32_t m_nDataOffset;      // +0x1C
};
static_assert(sizeof(FrameSnapshotEntityEntry_t) == 0x20);

// Base class for parallel execution jobs in Source 2
class alignas(16) CJob {
public:
    virtual ~CJob() = default;
    virtual void AddRef() = 0;
    virtual int32_t Release() = 0;
    virtual const char* GetJobName() = 0;
    virtual void SetJobName(const char* pszName) = 0;
    virtual void Execute() = 0;
    virtual void OnCompleted() = 0;

public:
    std::atomic<int32_t> m_nRefCount{1};               // +0x08
    int32_t              m_nPad0{0};                   // +0x0C
    JobPriority_t        m_nPriority{JobPriority_t::PRIORITY_NORMAL}; // +0x10
    uint8_t              m_nFlags{0};                  // +0x14
    uint8_t              m_bAutoDelete{1};             // +0x15
    uint8_t              m_bIsFinished{0};             // +0x16
    uint8_t              m_nPad1{0};                   // +0x17
    void*                m_pThreadPool{nullptr};       // +0x18
    uint64_t             m_nAffinityMask{0};           // +0x20
    void*                m_pUserData{nullptr};         // +0x28
    char                 m_szJobName[32]{};            // +0x30
    void*                m_pCaptureContext{nullptr};   // +0x50
    void*                m_pCaptureTarget{nullptr};    // +0x58
    uint8_t              m_nPadGraph[0x28]{};          // +0x60
    std::atomic<int32_t> m_nPendingPredecessors{0};    // +0x88 (Atomic DAG Barrier)
};

// Snapshot representation holding 2D chunked entity states
class CFrameSnapshot {
public:
    uint16_t                     m_nSnapshotIndex;     // +0x00
    uint16_t                     m_nFlags;             // +0x02
    uint8_t                      m_nPad0[0x0C];        // +0x04
    FrameSnapshotEntityEntry_t** m_pEntityBlocks;      // +0x10 (Array of 1024-entry blocks)
    uint8_t                      m_nPad1[0x78];        // +0x18
    uint16_t*                    m_pSortedIndices;     // +0x98 (Entity indices sorted by RBTree)
    uint32_t                     m_nActiveEntityCount; // +0xA0
    uint8_t                      m_nPad2[0x10];        // +0xA4
    std::atomic<int32_t>         m_nReferenceCount;    // +0xB4 (Decremented via lock xadd)
    void*                        m_pSnapshotManager;   // +0xB8 (CFrameSnapshotManager*)

    void AddReference() noexcept {
        m_nReferenceCount.fetch_add(1, std::memory_order_relaxed);
    }

    void ReleaseReference(bool bAsyncDelete) noexcept;
};

// Snapshot Manager governing creation and asynchronous destruction
class CFrameSnapshotManager {
public:
    virtual ~CFrameSnapshotManager() = default;

    static ParallelSendSnapshotMode GetParallelMode() noexcept;
    static bool ShouldSortEntities() noexcept;
    static bool ShouldAsyncDelete() noexcept;

    CFrameSnapshot* CreateFrameSnapshot(int32_t nTick, bool bSortEntities);
    void DeleteFrameSnapshot(CFrameSnapshot* pSnapshot, bool bAsync);
    void DeleteFrameSnapshotInternal(CFrameSnapshot* pSnapshot);
};

} // namespace cs2::engine2
