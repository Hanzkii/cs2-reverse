#ifndef CS2_SHARED_SKELETON_H
#define CS2_SHARED_SKELETON_H

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include "math_types.h"
#include "entity_handle.h"

namespace cs2::shared {

// ============================================================================
// Bone Calculation & Usage Flags
// Reconstructed from client.dll & animationsystem.dll
// ============================================================================
enum BoneFlags_t : std::uint32_t {
    BONE_FLAG_NONE               = 0,
    BONE_FLAG_CALCULATED         = (1u << 0),  // 0x0001: Bone has been evaluated this frame
    BONE_FLAG_INTERPOLATED       = (1u << 1),  // 0x0002: Inter-tick interpolation completed
    BONE_FLAG_WORLD_SPACE        = (1u << 2),  // 0x0004: Bone is in world coordinate space
    BONE_USED_BY_HITBOX          = (1u << 8),  // 0x0100: Bone drives collision/hitbox geometry
    BONE_USED_BY_ATTACHMENT      = (1u << 9),  // 0x0200: Bone drives attachments (weapons, muzzle, optics)
    BONE_USED_BY_VERTEX_LOD0     = (1u << 10), // 0x0400: Bone used in primary LOD0 skinning
    BONE_USED_BY_BONE_MERGE      = (1u << 11), // 0x0800: Bone participates in bone merge cache
    BONE_USED_BY_ANYTHING        = 0x0007FF00u
};

// ============================================================================
// 3x4 Transformation Matrix (matrix3x4_t)
// 48 bytes (3 rows, 4 columns: 3x3 rotation/scale + translation column)
// Standard Source 2 matrix format for skeletal skinning and lag compensation.
// ============================================================================
struct matrix3x4_t {
    float m[3][4]{};

    constexpr matrix3x4_t() noexcept = default;

    [[nodiscard]] const float* operator[](int i) const noexcept { return m[i]; }
    [[nodiscard]] float* operator[](int i) noexcept { return m[i]; }

    [[nodiscard]] Vector3D GetOrigin() const noexcept {
        return {m[0][3], m[1][3], m[2][3]};
    }

    void SetOrigin(const Vector3D& origin) noexcept {
        m[0][3] = origin.x;
        m[1][3] = origin.y;
        m[2][3] = origin.z;
    }

    [[nodiscard]] Vector3D TransformVector(const Vector3D& in) const noexcept {
        return {
            m[0][0] * in.x + m[0][1] * in.y + m[0][2] * in.z + m[0][3],
            m[1][0] * in.x + m[1][1] * in.y + m[1][2] * in.z + m[1][3],
            m[2][0] * in.x + m[2][1] * in.y + m[2][2] * in.z + m[2][3]
        };
    }

    [[nodiscard]] Vector3D RotateVector(const Vector3D& in) const noexcept {
        return {
            m[0][0] * in.x + m[0][1] * in.y + m[0][2] * in.z,
            m[1][0] * in.x + m[1][1] * in.y + m[1][2] * in.z,
            m[2][0] * in.x + m[2][1] * in.y + m[2][2] * in.z
        };
    }

    static matrix3x4_t Identity() noexcept {
        matrix3x4_t mat;
        mat.m[0][0] = 1.0f; mat.m[1][1] = 1.0f; mat.m[2][2] = 1.0f;
        return mat;
    }
};
static_assert(sizeof(matrix3x4_t) == 48, "matrix3x4_t must be exactly 48 bytes (0x30)");

// ============================================================================
// Source 2 32-Byte CTransform (Quaternion + Position + Scale)
// Reconstructed from client.dll & animationsystem.dll
// Reference: docs/09_skeleton_bone_hierarchy_and_animgraph.md
// ============================================================================
struct CTransform {
    // Rotation Quaternion (qx, qy, qz, qw)
    float qx{0.0f}, qy{0.0f}, qz{0.0f}, qw{1.0f}; // +0x00 (16 bytes)

    // Position Vector (px, py, pz)
    float px{0.0f}, py{0.0f}, pz{0.0f};          // +0x10 (12 bytes)

    // Uniform Scale Factor
    float scale{1.0f};                           // +0x1C (4 bytes)

    constexpr CTransform() noexcept = default;
    constexpr CTransform(float _qx, float _qy, float _qz, float _qw,
                         float _px, float _py, float _pz, float _scale = 1.0f) noexcept
        : qx(_qx), qy(_qy), qz(_qz), qw(_qw), px(_px), py(_py), pz(_pz), scale(_scale) {}

    [[nodiscard]] Vector3D GetPosition() const noexcept { return {px, py, pz}; }
    void SetPosition(const Vector3D& pos) noexcept { px = pos.x; py = pos.y; pz = pos.z; }

    /**
     * @brief Rotate a 3D vector using Rodrigues formula (zero trig calls, pure multiply-adds).
     * Reconstructed from client.dll @ FUN_181748480.
     */
    [[nodiscard]] Vector3D RotateVector(const Vector3D& v) const noexcept {
        const float tx = 2.0f * (qy * v.z - qz * v.y);
        const float ty = 2.0f * (qz * v.x - qx * v.z);
        const float tz = 2.0f * (qx * v.y - qy * v.x);

        return {
            v.x + qw * tx + (qy * tz - qz * ty),
            v.y + qw * ty + (qz * tx - qx * tz),
            v.z + qw * tz + (qx * ty - qy * tx)
        };
    }

    /**
     * @brief Transforms local point to world space: p_world = p_trans + scale * Rotate(v).
     */
    [[nodiscard]] Vector3D TransformPoint(const Vector3D& point) const noexcept {
        const Vector3D rot = RotateVector(point);
        return {px + scale * rot.x, py + scale * rot.y, pz + scale * rot.z};
    }

    /**
     * @brief Concatenates parent transform and child local transform.
     * Reconstructed from client.dll @ FUN_181748480.
     */
    static void ConcatenateTransforms(const CTransform& parent, const CTransform& child, CTransform* out) noexcept {
        // 1. Position: p_out = p_parent + s_parent * Rotate(p_child)
        const Vector3D childPos{child.px, child.py, child.pz};
        const Vector3D rotChildPos = parent.RotateVector(childPos);
        out->px = parent.px + parent.scale * rotChildPos.x;
        out->py = parent.py + parent.scale * rotChildPos.y;
        out->pz = parent.pz + parent.scale * rotChildPos.z;

        // 2. Rotation: q_out = q_parent * q_child (Hamilton product)
        const float nx = parent.qw * child.qx + parent.qx * child.qw + parent.qy * child.qz - parent.qz * child.qy;
        const float ny = parent.qw * child.qy - parent.qx * child.qz + parent.qy * child.qw + parent.qz * child.qx;
        const float nz = parent.qw * child.qz + parent.qx * child.qy - parent.qy * child.qx + parent.qz * child.qw;
        const float nw = parent.qw * child.qw - parent.qx * child.qx - parent.qy * child.qy - parent.qz * child.qz;

        // Normalize resulting quaternion
        const float lenSq = nx * nx + ny * ny + nz * nz + nw * nw;
        if (lenSq > 1e-8f) {
            const float invLen = 1.0f / std::sqrt(lenSq);
            out->qx = nx * invLen;
            out->qy = ny * invLen;
            out->qz = nz * invLen;
            out->qw = nw * invLen;
        } else {
            out->qx = 0.0f; out->qy = 0.0f; out->qz = 0.0f; out->qw = 1.0f;
        }

        // 3. Scale: s_out = s_parent * s_child
        out->scale = parent.scale * child.scale;
    }

    /**
     * @brief Converts 32-byte CTransform to 48-byte matrix3x4_t format.
     */
    [[nodiscard]] matrix3x4_t ToMatrix3x4() const noexcept {
        matrix3x4_t mat;
        const float xx = qx * qx;
        const float yy = qy * qy;
        const float zz = qz * qz;
        const float xy = qx * qy;
        const float xz = qx * qz;
        const float yz = qy * qz;
        const float wx = qw * qx;
        const float wy = qw * qy;
        const float wz = qw * qz;

        mat.m[0][0] = (1.0f - 2.0f * (yy + zz)) * scale;
        mat.m[0][1] = (2.0f * (xy - wz)) * scale;
        mat.m[0][2] = (2.0f * (xz + wy)) * scale;
        mat.m[0][3] = px;

        mat.m[1][0] = (2.0f * (xy + wz)) * scale;
        mat.m[1][1] = (1.0f - 2.0f * (xx + zz)) * scale;
        mat.m[1][2] = (2.0f * (yz - wx)) * scale;
        mat.m[1][3] = py;

        mat.m[2][0] = (2.0f * (xz - wy)) * scale;
        mat.m[2][1] = (2.0f * (yz + wx)) * scale;
        mat.m[2][2] = (1.0f - 2.0f * (xx + yy)) * scale;
        mat.m[2][3] = pz;

        return mat;
    }
};
static_assert(sizeof(CTransform) == 32, "CTransform must be exactly 32 bytes (0x20)");

// ============================================================================
// Model State Container (CModelState)
// Size: 0x2B0 (688 bytes)
// Reconstructed from client.dll @ 0x180aa0f80
// ============================================================================
class CModelState {
public:
    virtual ~CModelState() = default;                // +0x00
    std::uint8_t m_pad08[0x78]{};                    // +0x08..+0x80
    CTransform* m_pBoneArray{nullptr};               // +0x80: Pointer to dynamic CTransform bone matrix array
    std::uint8_t m_pad88[0x0C]{};                    // +0x88..+0x94
    std::uint16_t m_nBoneCount{0};                   // +0x94: Total number of active bones
    std::uint8_t m_pad96[0x0A]{};                    // +0x96..+0xA0
    std::uint64_t m_hModel{0};                       // +0xA0: CStrongHandle to compiled model asset
    const char* m_ModelName{nullptr};                // +0xA8: ASCII model asset path string
    std::uint8_t m_padB0[0x38]{};                    // +0xB0..+0xE8
    float m_flRootBoneOffset_x{0.0f};                // +0xE8
    float m_flRootBoneOffset_y{0.0f};                // +0xEC
    float m_flRootBoneOffset_z{0.0f};                // +0xF0
    std::uint8_t m_padF4[0x114]{};                   // +0xF4..+0x208
    std::uint64_t m_MeshGroupMask{0};                // +0x208: Visible mesh group bitmask
    std::uint8_t m_pad210[0xA0]{};                   // +0x210..+0x2B0
};
static_assert(sizeof(CModelState) == 0x2B0, "CModelState size must be exactly 0x2B0");
static_assert(offsetof(CModelState, m_pBoneArray) == 0x80, "m_pBoneArray offset mismatch");
static_assert(offsetof(CModelState, m_nBoneCount) == 0x94, "m_nBoneCount offset mismatch");

// ============================================================================
// Base Game Scene Node (CGameSceneNode)
// Size: 0x130 (304 bytes)
// ============================================================================
class CGameSceneNode {
public:
    virtual ~CGameSceneNode() = default;             // +0x00
    std::uint8_t m_pad08[0x08]{};                    // +0x08
    CTransform m_nodeToWorld{};                      // +0x10: Root spatial coordinate transformation
    void* m_pOwner{nullptr};                         // +0x30: Owning entity pointer
    CGameSceneNode* m_pParent{nullptr};              // +0x38: Parent node in spatial hierarchy
    CGameSceneNode* m_pChild{nullptr};               // +0x40: Child node
    CGameSceneNode* m_pNextSibling{nullptr};         // +0x48: Sibling node
    std::uint8_t m_pad50[0x20]{};                    // +0x50..+0x70
    CEntityHandle m_hParent{};                       // +0x70: Parent entity handle
    std::uint8_t m_pad74[0x0C]{};                    // +0x74..+0x80
    Vector3D m_vecOrigin{};                          // +0x80: Local origin offset
    std::uint8_t m_pad8C[0x2C]{};                    // +0x8C..+0xB8
    QAngle m_angRotation{};                          // +0xB8: Local rotation angles
    float m_flScale{1.0f};                           // +0xC4: Local scalar
    Vector3D m_vecAbsOrigin{};                       // +0xC8: Absolute world position
    QAngle m_angAbsRotation{};                       // +0xD4: Absolute world orientation angles
    float m_flAbsScale{1.0f};                        // +0xE0: Absolute world scalar
    std::uint8_t m_padE4[0x1F]{};                    // +0xE4..+0x103
    bool m_bDormant{false};                          // +0x103: PVS dormancy culling flag
    std::uint8_t m_pad104[0x2C]{};                   // +0x104..+0x130
};
static_assert(sizeof(CGameSceneNode) == 0x130, "CGameSceneNode size must be exactly 0x130");
static_assert(offsetof(CGameSceneNode, m_nodeToWorld) == 0x10, "m_nodeToWorld offset mismatch");
static_assert(offsetof(CGameSceneNode, m_vecOrigin) == 0x80, "m_vecOrigin offset mismatch");
static_assert(offsetof(CGameSceneNode, m_angRotation) == 0xB8, "m_angRotation offset mismatch");

// ============================================================================
// Skeleton Instance (CSkeletonInstance)
// Size: 0x490 (1168 bytes)
// Reconstructed from client.dll (VTable: 0x181c41ce0)
// ============================================================================
class CSkeletonInstance : public CGameSceneNode {
public:
    std::uint8_t m_pad130[0x10]{};                   // +0x130..+0x140
    CModelState m_modelState{};                      // +0x140: Embedded model state (Size: 0x2B0)
    bool m_bUseParentRenderBounds{false};            // +0x3F0
    std::uint8_t m_pad3F1[0x0B]{};                   // +0x3F1..+0x3FC
    std::uint8_t m_nHitboxSet{0};                    // +0x3FC: Active hitbox set index (0=default)
    std::uint8_t m_pad3FD[0x5B]{};                   // +0x3FD..+0x458
    std::uint32_t m_nCalculatedBoneFlags{0};         // +0x458: Forward kinematics dirty bitmask
    std::uint8_t m_pad45C[0x34]{};                   // +0x45C..+0x490

    [[nodiscard]] inline CTransform* GetBoneArray() const noexcept {
        return m_modelState.m_pBoneArray;
    }

    [[nodiscard]] inline std::uint16_t GetBoneCount() const noexcept {
        return m_modelState.m_nBoneCount;
    }

    [[nodiscard]] inline bool HasCalculatedBones(std::uint32_t mask) const noexcept {
        return (m_nCalculatedBoneFlags & mask) == mask;
    }

    inline void SetCalculatedBoneFlags(std::uint32_t mask) noexcept {
        m_nCalculatedBoneFlags |= mask;
    }

    inline void ClearCalculatedBoneFlags() noexcept {
        m_nCalculatedBoneFlags = 0;
    }
};
static_assert(sizeof(CSkeletonInstance) == 0x490, "CSkeletonInstance size must be exactly 0x490");
static_assert(offsetof(CSkeletonInstance, m_modelState) == 0x140, "m_modelState offset mismatch");
static_assert(offsetof(CSkeletonInstance, m_nHitboxSet) == 0x3FC, "m_nHitboxSet offset mismatch");
static_assert(offsetof(CSkeletonInstance, m_nCalculatedBoneFlags) == 0x458, "m_nCalculatedBoneFlags offset mismatch");

} // namespace cs2::shared

#endif // CS2_SHARED_SKELETON_H
