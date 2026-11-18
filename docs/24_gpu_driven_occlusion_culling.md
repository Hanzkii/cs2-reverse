# Chapter 24: GPU-Driven Occlusion Culling & Indirect Draw Architecture

## 1. Executive Subsystem Summary

In Counter-Strike 2 (`client.dll` & `rendersystemdx11.dll`, Build `2026-11-11_26598758`), Valve introduced a high-performance **GPU-Driven Compute Occlusion Culling & Indirect Draw Pipeline**.

Traditionally, Source 2 evaluated visibility on the CPU using PVS clusters or CPU frustum bounding-box intersections. The GPU culling architecture moves instance visibility testing entirely to compute shaders on the GPU:
1. **Depth Pyramid / Hierarchical Z-Buffer (HZB) Construction (`CDepthPyramidComputeRenderer::RenderMultiPass`)**: The depth buffer from the depth prepass is iteratively downsampled into a mip chain texture (`depthpyramid.vtex`) using `materials/dev/downsample_depth_cs.vmat`.
2. **Compute Culling Shader Dispatch (`CRenderingPipelineCsgo::AddGpuCullingLayer`)**: Reads a `StructuredBuffer<MeshCullingInstance_t>`, projects instance bounding boxes into clip space, selects the appropriate HZB mip level, and tests depth occlusion against the 4-texel quad.
3. **Indirect Argument Generation**: Surviving instances have their draw parameters written directly into a GPU buffer (`RWByteAddressBuffer`) configured with `RENDER_BUFFER_UAV_DRAW_INDIRECT_ARGS` (`0x100`).
4. **Hardware Execution (`CRenderContextDx11::DrawIndexedInstancedIndirect`)**: Dispatches `ID3D11DeviceContext::DrawIndexedInstancedIndirect` (vtable slot 39 at `0x18001C760`), executing draw calls with zero CPU readback.

---

## 2. ConVar Analysis & Value Semantics

| ConVar Name | Registration VA | ConVar Object VA | Flags | Default | Category / Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `r_csgo_gpu_culling` | `0x180123F6A` | `0x1825E4DB0` | `0x100000` (Render Thread) | `true` (1) | Main switch for GPU compute culling |
| `r_csgo_gpu_culling_camera_offset` | `0x180123FC0` | `0x1825E4DD0` | `0x2` (Hidden/Dev) | `0.0f` | Camera depth bias offset in world units |
| `r_csgo_gpu_culling_shadows` | `0x180124060` | `0x1825E4DE0` | `0x100000` (Render Thread) | `false` (0) | GPU culling for Cascaded Shadow Maps |
| `r_csgo_gpu_culling_shadows_min_cascade` | `0x180124110` | `0x1825E5190` | `0x2` (Hidden/Dev) | `1` | Minimum CSM cascade index to cull |
| `r_csgo_gpu_culling_two_pass` | `0x1801241C0` | `0x1825E4DC0` | `0x100000` (Render Thread) | `false` (0) | Two-pass occlusion culling (re-cull after prepass) |

---

## 3. Mathematical & Projection Formulas

### 3.1 Conservative Clip-Space Extent Projection
To prevent false-positive culling near screen edges or silhouette silhouettes, Valve projects the AABB extents using the absolute value matrix:
$$\text{Extent}_{\text{clip}} = \text{Extent}_x \cdot |\mathbf{M}_{0}| + \text{Extent}_y \cdot |\mathbf{M}_{1}| + \text{Extent}_z \cdot |\mathbf{M}_{2}|$$

### 3.2 HZB Mip Level Selection
Given screen-space bounding rectangle $[x_{\min}, y_{\min}, x_{\max}, y_{\max}]$ with dimensions $\Delta x, \Delta y$:
$$\text{mip} = \text{clamp}\left(\left\lceil \log_2 \left(\max(\Delta x \cdot W, \Delta y \cdot H)\right) \right\rceil, 0, \text{nMipCount} - 1\right)$$

An instance is culled if the minimum projected depth $Z_{\min}$ is behind all 4 texels of the conservative HZB mip:
$$\text{if } Z_{\min} > Z_{\text{HZB}} \implies \text{Culled (InstanceCount} = 0\text{)}$$

---

## 4. Reconstructed SDK Headers

See [game/client/gpu_culling.h](../game/client/gpu_culling.h) and [sdk/include/client/gpu_culling.hpp](../sdk/include/client/gpu_culling.hpp).
