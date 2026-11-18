#pragma once

#include "game/engine/parallel_snapshot.h"

namespace cs2::sdk::engine {
    using ParallelSendSnapshotMode = cs2::engine2::ParallelSendSnapshotMode;
    using JobPriority_t = cs2::engine2::JobPriority_t;
    using FrameSnapshotEntityEntry_t = cs2::engine2::FrameSnapshotEntityEntry_t;
    using CJob = cs2::engine2::CJob;
    using CFrameSnapshot = cs2::engine2::CFrameSnapshot;
    using CFrameSnapshotManager = cs2::engine2::CFrameSnapshotManager;
} // namespace cs2::sdk::engine
