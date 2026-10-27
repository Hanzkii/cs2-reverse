#include "bone_setup.h"
#include <cstring>
#include <cmath>

namespace cs2::client {

// ============================================================================
// CBoneMergeCache Implementation
// ============================================================================
void CBoneMergeCache::Init(CSkeletonInstance* pOwner, CSkeletonInstance* pFollower, const int* pBoneMap, int count) noexcept {
    m_pOwner = pOwner;
    m_pFollower = pFollower;
    m_mergedBoneIndices.clear();

    if (!pOwner || !pFollower || !pBoneMap || count <= 0) {
        return;
    }

    const int ownerMax = pOwner->GetBoneCount();
    for (int i = 0; i < count; ++i) {
        const int ownerIdx = pBoneMap[i];
        if (ownerIdx >= 0 && ownerIdx < ownerMax) {
            m_mergedBoneIndices.push_back({i, ownerIdx});
        }
    }
}

int CBoneMergeCache::MergeBones(matrix3x4_t* pOutFollowerBones, const matrix3x4_t* pOwnerBones) const noexcept {
    if (!pOutFollowerBones || !pOwnerBones || m_mergedBoneIndices.empty()) {
        return 0;
    }

    int merged = 0;
    for (const auto& pair : m_mergedBoneIndices) {
        pOutFollowerBones[pair.followerBoneIndex] = pOwnerBones[pair.ownerBoneIndex];
        ++merged;
    }
    return merged;
}

// ============================================================================
// C_BaseEntityAnimGraph Implementation
// ============================================================================
C_BaseEntityAnimGraph::C_BaseEntityAnimGraph() = default;

void C_BaseEntityAnimGraph::AttachSkeleton(CSkeletonInstance* pSkeleton, const int* pBoneParents, int boneCount) noexcept {
    m_pSkeletonInstance = pSkeleton;
    m_boneParents.clear();
    m_localBoneTransforms.clear();
    m_worldBoneTransforms.clear();
    m_cachedBoneMatrices.clear();

    if (!pSkeleton || boneCount <= 0) return;

    m_boneParents.resize(static_cast<std::size_t>(boneCount), -1);
    m_localBoneTransforms.resize(static_cast<std::size_t>(boneCount));
    m_worldBoneTransforms.resize(static_cast<std::size_t>(boneCount));
    m_cachedBoneMatrices.resize(static_cast<std::size_t>(boneCount));

    if (pBoneParents) {
        for (int i = 0; i < boneCount; ++i) {
            m_boneParents[static_cast<std::size_t>(i)] = pBoneParents[i];
        }
    }

    // Set bone array pointer and count in CModelState
    pSkeleton->m_modelState.m_nBoneCount = static_cast<std::uint16_t>(boneCount);
    pSkeleton->m_modelState.m_pBoneArray = m_worldBoneTransforms.data();
    pSkeleton->ClearCalculatedBoneFlags();
}

void C_BaseEntityAnimGraph::SetLocalBoneTransform(int boneIndex, const CTransform& localTransform) noexcept {
    if (boneIndex >= 0 && static_cast<std::size_t>(boneIndex) < m_localBoneTransforms.size()) {
        m_localBoneTransforms[static_cast<std::size_t>(boneIndex)] = localTransform;
        if (m_pSkeletonInstance) {
            m_pSkeletonInstance->ClearCalculatedBoneFlags();
        }
    }
}

void C_BaseEntityAnimGraph::CalculateWorldBones(std::uint32_t boneMask) noexcept {
    if (!m_pSkeletonInstance || m_boneParents.empty()) return;

    const std::size_t count = m_boneParents.size();
    const CTransform& rootToWorld = m_pSkeletonInstance->m_nodeToWorld;

    for (std::size_t i = 0; i < count; ++i) {
        const int parentIdx = m_boneParents[i];
        if (parentIdx < 0 || static_cast<std::size_t>(parentIdx) >= count) {
            // Root bone: concatenate with model's root m_nodeToWorld
            CTransform::ConcatenateTransforms(rootToWorld, m_localBoneTransforms[i], &m_worldBoneTransforms[i]);
        } else {
            // Child bone: concatenate with parent bone's evaluated world CTransform
            CTransform::ConcatenateTransforms(m_worldBoneTransforms[static_cast<std::size_t>(parentIdx)],
                                              m_localBoneTransforms[i],
                                              &m_worldBoneTransforms[i]);
        }

        // Convert to matrix3x4_t format for skinning and ray-tracing
        m_cachedBoneMatrices[i] = m_worldBoneTransforms[i].ToMatrix3x4();
    }

    m_pSkeletonInstance->SetCalculatedBoneFlags(boneMask | shared::BONE_FLAG_CALCULATED | shared::BONE_FLAG_WORLD_SPACE);
}

bool C_BaseEntityAnimGraph::SetupBones(matrix3x4_t* pOutBones, int maxBones, std::uint32_t boneMask, float currentTime) noexcept {
    if (!m_pSkeletonInstance || !pOutBones || maxBones <= 0) return false;

    const int boneCount = m_pSkeletonInstance->GetBoneCount();
    if (boneCount <= 0 || boneCount > maxBones) return false;

    // 1. Lazy evaluation check: if bones were already calculated for this mask and timestamp, copy cached matrices
    if (m_flLastBoneSetupTime == currentTime && m_pSkeletonInstance->HasCalculatedBones(boneMask)) {
        for (int i = 0; i < boneCount; ++i) {
            pOutBones[i] = m_cachedBoneMatrices[static_cast<std::size_t>(i)];
        }
        return true;
    }

    // 2. Evaluate forward kinematics traversal
    CalculateWorldBones(boneMask);
    m_flLastBoneSetupTime = currentTime;

    // 3. Export to caller buffer
    for (int i = 0; i < boneCount; ++i) {
        pOutBones[i] = m_cachedBoneMatrices[static_cast<std::size_t>(i)];
    }

    return true;
}

bool C_BaseEntityAnimGraph::GetBonePosition(int boneIndex, Vector3D& outPos) const noexcept {
    if (!m_pSkeletonInstance || boneIndex < 0 ||
        static_cast<std::size_t>(boneIndex) >= m_worldBoneTransforms.size()) {
        return false;
    }
    outPos = m_worldBoneTransforms[static_cast<std::size_t>(boneIndex)].GetPosition();
    return true;
}

bool C_BaseEntityAnimGraph::GetBoneMatrix(int boneIndex, matrix3x4_t& outMatrix) const noexcept {
    if (!m_pSkeletonInstance || boneIndex < 0 ||
        static_cast<std::size_t>(boneIndex) >= m_cachedBoneMatrices.size()) {
        return false;
    }
    outMatrix = m_cachedBoneMatrices[static_cast<std::size_t>(boneIndex)];
    return true;
}

bool C_BaseEntityAnimGraph::GetHitboxWorldBounds(const CHitBox& hitbox, Vector3D& outWorldCenter, Vector3D& outWorldMins, Vector3D& outWorldMaxs) const noexcept {
    matrix3x4_t boneMatrix;
    if (!GetBoneMatrix(static_cast<int>(hitbox.m_nBoneId), boneMatrix)) {
        return false;
    }

    // Transform local minimum and maximum bounding coordinates
    const Vector3D p1 = boneMatrix.TransformVector(hitbox.m_vMinBounds);
    const Vector3D p2 = boneMatrix.TransformVector(hitbox.m_vMaxBounds);

    outWorldCenter = (p1 + p2) * 0.5f;
    outWorldMins = {
        std::min(p1.x, p2.x) - hitbox.m_flShapeRadius,
        std::min(p1.y, p2.y) - hitbox.m_flShapeRadius,
        std::min(p1.z, p2.z) - hitbox.m_flShapeRadius
    };
    outWorldMaxs = {
        std::max(p1.x, p2.x) + hitbox.m_flShapeRadius,
        std::max(p1.y, p2.y) + hitbox.m_flShapeRadius,
        std::max(p1.z, p2.z) + hitbox.m_flShapeRadius
    };

    return true;
}

} // namespace cs2::client
