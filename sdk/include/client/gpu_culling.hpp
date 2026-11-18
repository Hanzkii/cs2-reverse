#pragma once

#include "game/client/gpu_culling.h"

namespace cs2::sdk::client {
    using RenderBufferUsage_t = cs2::client::RenderBufferUsage_t;
    using DrawIndexedInstancedIndirectArgs_t = cs2::client::DrawIndexedInstancedIndirectArgs_t;
    using RsCmdDispatchCompute_t = cs2::client::RsCmdDispatchCompute_t;
    using RsCmdDrawIndexedInstancedIndirect_t = cs2::client::RsCmdDrawIndexedInstancedIndirect_t;
    using GpuCullingViewConstants_t = cs2::client::GpuCullingViewConstants_t;
    using MeshCullingInstance_t = cs2::client::MeshCullingInstance_t;
    namespace CullingShaderTokens = cs2::client::CullingShaderTokens;
} // namespace cs2::sdk::client
