//===========================================================================//
// Valve Source 2 Engine - Clean-Room SDK
// Subsystem: GPU-Driven Occlusion Culling & Indirect Draw Execution
// Target Binaries: client.dll & rendersystemdx11.dll (CS2 Build 2026-11-11_26598758)
//===========================================================================//

#pragma once

#include <cstdint>

namespace cs2::client {

// Render buffer usage flags extracted from rendersystemdx11.dll reflection table
enum RenderBufferUsage_t : uint32_t {
    RENDER_BUFFER_USAGE_NONE                  = 0x0,
    RENDER_BUFFER_USAGE_VERTEX_BUFFER         = 0x1,
    RENDER_BUFFER_USAGE_INDEX_BUFFER          = 0x2,
    RENDER_BUFFER_USAGE_SHADER_RESOURCE       = 0x4,
    RENDER_BUFFER_USAGE_UNORDERED_ACCESS      = 0x8,
    RENDER_BUFFER_BYTEADDRESS_BUFFER          = 0x10,
    RENDER_BUFFER_STRUCTURED_BUFFER           = 0x20,
    RENDER_BUFFER_UAV_DRAW_INDIRECT_ARGS      = 0x100,
    RENDER_BUFFER_ACCELERATION_STRUCTURE      = 0x200,
    RENDER_BUFFER_SHADER_BINDING_TABLE        = 0x400,
    RENDER_BUFFER_POOL_ALLOCATED              = 0x800,
    RENDER_BUFFER_USAGE_CONDITIONAL_RENDERING = 0x1000,
    RENDER_BUFFER_IMMOVABLE_ALLOCATION        = 0x2000,
    RENDER_BUFFER_DYNAMIC_ZERO_COPY           = 0x4000,
};

// Hardware D3D11 DrawIndexedInstancedIndirect argument layout (20 bytes)
struct alignas(4) DrawIndexedInstancedIndirectArgs_t {
    uint32_t m_nIndexCountPerInstance;
    uint32_t m_nInstanceCount;
    uint32_t m_nStartIndexLocation;
    int32_t  m_nBaseVertexLocation;
    uint32_t m_nStartInstanceLocation;
};
static_assert(sizeof(DrawIndexedInstancedIndirectArgs_t) == 0x14);

// Software command buffer: Dispatch Compute (16 bytes, ID: 0x108024)
struct RsCmdDispatchCompute_t {
    uint32_t m_nCommandId; // 0x108024
    uint32_t m_nThreadGroupsX;
    uint32_t m_nThreadGroupsY;
    uint32_t m_nThreadGroupsZ;
};

// Software command buffer: DrawIndexedInstancedIndirect (24 bytes, ID: 0x188021)
struct RsCmdDrawIndexedInstancedIndirect_t {
    uint32_t m_nCommandId; // 0x188021
    uint32_t m_nPad;
    void*    m_pBufferForArgs;            // ID3D11Buffer* (offset +0x08)
    uint32_t m_nAlignedByteOffsetForArgs; // offset +0x10
    uint32_t m_nPad2;
};

// Per-View Culling Constant Buffer Descriptor (client.dll offset +0x260)
struct alignas(16) GpuCullingViewConstants_t {
    uint8_t   m_bValid;                   // +0x260
    uint8_t   m_nPad0[3];                 // +0x261
    float     m_matView[16];              // +0x264 (64 bytes)
    float     m_matProj[16];              // +0x2A4 (64 bytes)
    float     m_matViewProj[16];          // +0x2E4 (64 bytes)
    float     m_vCameraPosition[3];       // +0x324 (12 bytes)
    float     m_flCameraOffset;           // +0x330 (from r_csgo_gpu_culling_camera_offset)
};

// Mesh Culling Instance entry passed to downsample/cull compute shader
struct alignas(16) MeshCullingInstance_t {
    float    m_vBoundsMin[3];             // +0x00
    uint32_t m_nDrawCommandIndex;         // +0x0C (index into DrawIndexedInstancedIndirectArgs)
    float    m_vBoundsMax[3];             // +0x10
    uint32_t m_nInstanceDataIndex;        // +0x1C (index into transform/material table)
};
static_assert(sizeof(MeshCullingInstance_t) == 0x20);

// CUtlStringToken hashed identifiers used in GPU culling shader binding
namespace CullingShaderTokens {
    constexpr uint32_t DepthPyramidTexture = 0x6B378FE1; // MurmurHash2("depthpyramid")
    constexpr uint32_t MaxMipLevel        = 0x5DD15C68; // MipCount - 1
    constexpr uint32_t ViewportWidth       = 0x0481F236; // Viewport width in pixels
    constexpr uint32_t ViewportHeight      = 0x125CF451; // Viewport height in pixels
    constexpr uint32_t InvViewportScale    = 0x430D5682; // Vector2D(1.0f / W, 1.0f / H)
    constexpr uint32_t CameraOffsetBasis   = 0xD1565D45; // Camera basis offset vector
}

} // namespace cs2::client
