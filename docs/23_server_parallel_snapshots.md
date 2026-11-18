# Chapter 23: Server Parallel Snapshot Pipeline & Asynchronous Dispatch Architecture

## 1. Executive Subsystem Summary

In Counter-Strike 2 engine revisions (including Build `2026-11-11_26598758`), Valve introduced a multi-stage, task-graph-driven asynchronous execution model to `engine2.dll` to replace the legacy single-threaded, synchronous per-client snapshot broadcast pipeline.

Governed by internal engine ConVars (`sv_parallel_sendsnapshot`, `sv_frame_snapshot_sort_entities`, and `sv_frame_snapshot_async_delete`), this architecture transitions server delta generation into an asynchronous pipeline:
1. **Parallel Entity Packing (`CNetworkGameServer::PackEntities_Normal`)**: Shards entity baseline and delta compression across worker threads using `IThreadPool::ParallelFor`.
2. **Chunked 2D Snapshot Matrix & Tree Sorting (`CFrameSnapshotManager::CreateFrameSnapshot`)**: Organizes active entity states into a 2D chunked matrix (1024 entities per block, 32-byte descriptor). When `sv_frame_snapshot_sort_entities` is enabled, entity indices are ordered via in-order traversal of a balanced Red-Black tree (`CUtlRBTree`).
3. **Atomic Task-Graph Barrier Chaining (`CServerSideClient::SetupParallelSnapshotDependencies`)**: Under Mode 4, regular client transmission jobs form a Directed Acyclic Graph (DAG) with an atomic dependency counter (`m_nPendingPredecessors`). When all client snapshot jobs complete, the predecessor count drops to zero, unblocking the dependent HLTV broadcast encoder job.
4. **Deferred Asynchronous Snapshot Reclamation (`CFrameSnapshot::ReleaseReference`)**: Uses atomic reference counting (`lock xadd`). When a snapshot is released, its deletion is offloaded to the thread pool as a low-priority `CLambdaJob` to avoid tick stalls.

---

## 2. ConVar Analysis & Value Semantics

### 2.1 `sv_parallel_sendsnapshot`
- **Registration Site**: `0x1800062b0` - `0x180006357`
- **String Identifier**: `"sv_parallel_sendsnapshot"` (`0x1805d73d0`)
- **Storage Pointer**: `0x180730b58` (`m_nValue` at `+0x58` as `int32_t`)
- **Flags**: `0x80000` (`FCVAR_RELEASE`)

#### Execution Modes:
| Value | Mode Identifier | Profile & Synchronization |
|:---:|:---|:---|
| **0** | `SERIAL_SYNCHRONOUS` | Synchronous execution on main server thread. |
| **1** | `ASYNC_CLIENT_ONLY` | Asynchronous worker jobs on listen server; synchronous on dedicated server. |
| **2** | `ASYNC_UNCONDITIONAL` | Asynchronous jobs across all configurations. |
| **3** | `PARALLEL_TICK_BLOCKED` | Parallel dispatch across threads, blocking at end-of-tick barrier. |
| **4** | `PARALLEL_DAG_CHAINED` | Main server player jobs run in parallel; HLTV jobs execute once client DAG unblocks. |

### 2.2 `sv_frame_snapshot_sort_entities` & `sv_frame_snapshot_async_delete`
- **`sv_frame_snapshot_sort_entities`** (`0x18096f530`, default `1`): When enabled, `CFrameSnapshotManager::CreateFrameSnapshot` performs an in-order walk of a `CUtlRBTree` to order entity indices sequentially, improving CPU L1 cache locality during delta compression.
- **`sv_frame_snapshot_async_delete`** (`0x18096f540`, default `0`): When reference count hits zero (`lock xadd [rcx + 0xb4], -1`), wraps `CFrameSnapshotManager::DeleteFrameSnapshotInternal` into a 96-byte `CLambdaJob` (`0x1805e1718`) at `PRIORITY_LOW` on the worker pool.

---

## 3. Disassembly & Task Graph Synchronization

### 3.1 Task Graph DAG Dependency Chaining (`0x1800beb00`)
```x86asm
; engine2.dll @ 0x1800beb66
mov     rax, cs:sv_parallel_sendsnapshot_ptr
mov     eax, [rax + 0x58]
cmp     eax, 4                           ; Check for Mode 4 (DAG Chained)
jl      short loc_skip_chaining

; Atomically increment HLTV job dependency count
lock inc dword ptr [rdi + 0x88]           ; m_nPendingPredecessors atomic increment
```

### 3.2 Snapshot Send Job Class Hierarchy
| Class Name | RTTI Complete Object Locator | Virtual Method Table VA | Execute Function VA |
|:---|:---:|:---:|:---:|
| `CServerSideClient::CSendJob_Regular` | `0x180637cc8` | `0x1805dcd88` | `0x1800b8e30` |
| `CServerSideClient::CSendJob_Empty` | `0x180637c28` | `0x1805dc878` | `0x1800beeb0` |
| `CServerSideClient::CSendJob_HltvSource` | `0x180637bd8` | `0x1805dca58` | `0x1800bef60` |
| `CServerSideClient::CSendJob_HltvReplay` | `0x180637b88` | `0x1805dcaa8` | `0x1800bf080` |

---

## 4. Reconstructed SDK Headers

See [game/engine/parallel_snapshot.h](../game/engine/parallel_snapshot.h) and [sdk/include/engine/parallel_snapshot.hpp](../sdk/include/engine/parallel_snapshot.hpp) for the clean-room C++20 structures.
