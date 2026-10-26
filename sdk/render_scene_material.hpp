#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <type_traits>
#include "interfaces_registry.hpp"
#include "entity_system.hpp"
#include "client_classes.hpp"

// ============================================================================
// Source 2 Reconstructed SDK - Phase 2: Camera, Models, Rendering & Materials
// Target Build: 2026-10-25_26144732 (CS2 Update Archive)
// Architecture: x86-64 (MSVC ABI)
// ============================================================================

namespace cs2::sdk {

#define PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:

// ============================================================================
// 1. 4x4 View-Projection Matrix (VMatrix) & Camera Pipeline
// Discovered in client.dll:
// - Global Matrix Array: client.dll + 0x256cfe0 (VA: 0x18256cfe0)
// - Engine Accessor:     client.dll + 0xc14fb0 (GetViewMatrix(int nViewIndex))
// Assembly anchor:
//   movsxd rax, ecx
//   lea rcx, [rip + 0x1958026] ; 0x18256cfe0
//   shl rax, 6                 ; 64 bytes (sizeof(VMatrix))
//   add rax, rcx
//   ret
// ============================================================================
struct VMatrix {
    float m[4][4];

    constexpr const float* operator[](int index) const noexcept { return m[index]; }
    constexpr float* operator[](int index) noexcept { return m[index]; }

    // World to Screen transformation equation with frustum rejection
    bool WorldToScreen(const Vector3& world, float& screenX, float& screenY,
                       int screenWidth, int screenHeight) const noexcept {
        const float w = m[3][0] * world.x + m[3][1] * world.y + m[3][2] * world.z + m[3][3];

        // Homogeneous frustum rejection (behind camera near-clipping plane)
        if (w < 0.001f) {
            return false;
        }

        const float invW = 1.0f / w;
        const float x = (m[0][0] * world.x + m[0][1] * world.y + m[0][2] * world.z + m[0][3]) * invW;
        const float y = (m[1][0] * world.x + m[1][1] * world.y + m[1][2] * world.z + m[1][3]) * invW;

        // Viewport pixel coordinate mapping
        screenX = (screenWidth * 0.5f) + (0.5f * x * static_cast<float>(screenWidth) + 0.5f);
        screenY = (screenHeight * 0.5f) - (0.5f * y * static_cast<float>(screenHeight) + 0.5f);
        return true;
    }
};
static_assert(sizeof(VMatrix) == 0x40, "VMatrix size must be exactly 64 bytes");

// ============================================================================
// 2. CViewRender Layout
// Binary: client.dll
// VTable RVA: client.dll + 0x1c7f128 (VA: 0x181c7f128)
// Allocation Size: 0x1D60 bytes (7,520 bytes, verified at client.dll + 0xc05d0f)
// ============================================================================
class CViewRender {
public:
    virtual ~CViewRender() = default;

    PAD(0x1328, m_pad08);
    // Viewport rendering context
    std::uint8_t m_bRenderingView;     // +0x1330
    PAD(0x00F, m_pad1331);
    VMatrix      m_viewMatrix;         // +0x1340
    PAD(0x09E0, m_pad1380);
    void*        m_pRenderContext;     // +0x1D60
};

// ============================================================================
// 3. Studio Hitbox Sets & Hitbox Geometry
// Target Binary: vphysics2.dll / client.dll
// Schema Descriptors: CHitBox (size 0x70), CHitBoxSet (size 0x30), CHitBoxSetList (size 0x18)
// ============================================================================
enum EHitBoxGroup : std::uint32_t {
    HITGROUP_GENERIC   = 0,
    HITGROUP_HEAD      = 1,
    HITGROUP_CHEST     = 2,
    HITGROUP_STOMACH   = 3,
    HITGROUP_LEFTARM   = 4,
    HITGROUP_RIGHTARM  = 5,
    HITGROUP_LEFTLEG   = 6,
    HITGROUP_RIGHTLEG  = 7,
    HITGROUP_NECK      = 8,
    HITGROUP_UNUSED    = 9,
    HITGROUP_GEAR      = 10
};

struct CHitBox {
    const char*       m_pszName;            // +0x00: Target bone / hitbox name
    const char*       m_pszSurfaceProperty; // +0x08: Surface property token (flesh, metal)
    const char*       m_pszBoneName;        // +0x10: Target skeletal bone string
    Vector3           m_vMinBounds;         // +0x18: Minimum bounding box coordinate
    Vector3           m_vMaxBounds;         // +0x24: Maximum bounding box coordinate
    float             m_flShapeRadius;      // +0x30: Capsule pill radius
    std::uint32_t     m_nBoneNameHash;      // +0x34: MurmurHash2 bone token
    std::uint32_t     m_nGroupId;           // +0x38: EHitBoxGroup classification
    std::uint8_t      m_nShapeType;         // +0x3C: 0 = Box, 1 = Capsule
    bool              m_bTranslationOnly;   // +0x3D
    PAD(0x2, m_pad3E);
    std::uint32_t     m_CRC;                // +0x40
    std::uint32_t     m_cRenderColor;       // +0x44: Debug render color
    std::int32_t      m_nHitBoxIndex;       // +0x48: Zero-based hitbox array index
    PAD(0x24, m_pad4C);                     // +0x4C: Alignment to 0x70
};
static_assert(sizeof(CHitBox) == 0x70, "CHitBox must be exactly 0x70 (112) bytes");
static_assert(offsetof(CHitBox, m_vMinBounds) == 0x18, "CHitBox m_vMinBounds offset mismatch");
static_assert(offsetof(CHitBox, m_vMaxBounds) == 0x24, "CHitBox m_vMaxBounds offset mismatch");
static_assert(offsetof(CHitBox, m_flShapeRadius) == 0x30, "CHitBox m_flShapeRadius offset mismatch");
static_assert(offsetof(CHitBox, m_nGroupId) == 0x38, "CHitBox m_nGroupId offset mismatch");

struct CHitBoxSet {
    const char*            m_pszName;       // +0x00: Set identifier ("default")
    std::uint32_t          m_nNameHash;     // +0x08: Set name hash
    PAD(0x4, m_pad0C);                      // +0x0C
    PAD(0x18, m_HitBoxes);                  // +0x10: Dynamic vector of studio hitboxes (24 bytes)
    const char*            m_pszSourceFile; // +0x28: Source model asset file path
};
static_assert(sizeof(CHitBoxSet) == 0x30, "CHitBoxSet must be exactly 0x30 (48) bytes");
static_assert(offsetof(CHitBoxSet, m_pszSourceFile) == 0x28, "CHitBoxSet m_pszSourceFile offset mismatch");

struct CHitBoxSetList {
    PAD(0x18, m_HitBoxSets);                // +0x00: List of hitbox sets (24 bytes)
};
static_assert(sizeof(CHitBoxSetList) == 0x18, "CHitBoxSetList must be exactly 0x18 (24) bytes");

// ============================================================================
// 4. Material System 2 & CMaterial2
// Target Binary: materialsystem2.dll
// VTable RVA: materialsystem2.dll + 0x129ae0 (CMaterial2, 25 methods)
// VTable RVA: materialsystem2.dll + 0x12d920 (VMaterialSystem2_001, 57 methods)
// Singleton:  materialsystem2.dll + 0x163530
// ============================================================================
class CMaterial2 {
public:
    virtual ~CMaterial2() = default;

    // Slot 0: materialsystem2.dll + 0xb350
    virtual const char* GetName() const = 0;

    // Slot 1: materialsystem2.dll + 0xb370
    virtual const char* GetShaderName() const = 0;

    virtual void Unk02() = 0;
    virtual void Unk03() = 0;

    // Slot 4: materialsystem2.dll + 0xa100
    virtual std::uint8_t GetFlags() const = 0;
};
static_assert(sizeof(CMaterial2) == 0x08, "CMaterial2 pointer must be 8 bytes");

// ============================================================================
// 5. Scene System & Animatable Object Draw Dispatch
// Target Binary: scenesystem.dll
// VTable RVA: scenesystem.dll + 0x5e5e50 (CSceneAnimatableObject, 3 methods)
// VTable RVA: scenesystem.dll + 0x5e4748 (CSceneObject, 2 methods)
// ============================================================================
class CSceneObject {
public:
    virtual ~CSceneObject() = default;

    PAD(0x18, m_pad08);
    void* m_pOwner; // +0x20
};

class CSceneAnimatableObject : public CSceneObject {
public:
    virtual ~CSceneAnimatableObject() override = default;

    // Slot 1: scenesystem.dll + 0x36dc0 (Draws animatable mesh primitives)
    virtual void Draw(void* pRenderContext, void* pDrawContext) = 0;
};

#undef PAD

} // namespace cs2::sdk
