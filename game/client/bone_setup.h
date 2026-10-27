#ifndef CS2_CLIENT_BONE_SETUP_H
#define CS2_CLIENT_BONE_SETUP_H

#include <cstdint>
#include <vector>
#include <memory>
#include "../shared/skeleton.h"
#include "../shared/hitbox.h"

namespace cs2::client {

using shared::CTransform;
using shared::matrix3x4_t;
using shared::Vector3D;
using shared::CSkeletonInstance;
using shared::CHitBox;
using shared::BoneFlags_t;

// ============================================================================
// Source 2 Bone Merge Cache (CBoneMergeCache)
// Reconstructed from client.dll @ 0x180b3d820
// Handles hierarchical bone synchronization between parent models (e.g. player pawn)
// and child attachments (e.g. viewmodel weapons, equipment, gloves).
// ============================================================================
class CBoneMergeCache {
public:
    CBoneMergeCache() = default;
    ~CBoneMergeCache() = default;

    /**
     * @brief Initializes the merge lookup mapping from follower model bones to owner model bones.
     * @param pOwner Parent skeleton instance (e.g. player body)
     * @param pFollower Child skeleton instance (e.g. weapon / gloves)
     * @param pBoneMap Mapping table where index = followerBone, value = ownerBone (-1 if unmapped)
     * @param count Total number of follower bones
     */
    void Init(CSkeletonInstance* pOwner, CSkeletonInstance* pFollower, const int* pBoneMap, int count) noexcept;

    /**
     * @brief Copies and merges computed bone matrices from the owner skeleton to the follower.
     * @param pOutFollowerBones Output array of follower bone matrices
     * @param pOwnerBones Source array of computed owner bone matrices
     * @return Number of bones successfully merged
     */
    int MergeBones(matrix3x4_t* pOutFollowerBones, const matrix3x4_t* pOwnerBones) const noexcept;

    [[nodiscard]] bool IsValid() const noexcept {
        return m_pOwner != nullptr && m_pFollower != nullptr && !m_mergedBoneIndices.empty();
    }

    [[nodiscard]] int GetMergedCount() const noexcept {
        return static_cast<int>(m_mergedBoneIndices.size());
    }

private:
    CSkeletonInstance* m_pOwner{nullptr};
    CSkeletonInstance* m_pFollower{nullptr};
    struct MergedBonePair {
        int followerBoneIndex;
        int ownerBoneIndex;
    };
    std::vector<MergedBonePair> m_mergedBoneIndices;
};

// ============================================================================
// Animation Graph Controller (C_BaseAnimGraphController)
// Manages playback cycles, sequence transitions, and AnimGraph blend tree states.
// ============================================================================
class C_BaseAnimGraphController {
public:
    C_BaseAnimGraphController() = default;

    void Reset(int sequenceIndex = 0) noexcept {
        m_nSequence = sequenceIndex;
        m_flCycle = 0.0f;
        m_flPlaybackRate = 1.0f;
        m_bSequenceFinished = false;
    }

    void AdvanceTime(float dt) noexcept {
        if (m_bSequenceFinished) return;
        m_flCycle += dt * m_flPlaybackRate;
        if (m_flCycle >= 1.0f) {
            m_flCycle = 1.0f;
            m_bSequenceFinished = true;
        }
    }

    [[nodiscard]] float GetCycle() const noexcept { return m_flCycle; }
    [[nodiscard]] int GetSequence() const noexcept { return m_nSequence; }
    [[nodiscard]] float GetPlaybackRate() const noexcept { return m_flPlaybackRate; }
    [[nodiscard]] bool IsSequenceFinished() const noexcept { return m_bSequenceFinished; }

    void SetPlaybackRate(float rate) noexcept { m_flPlaybackRate = rate; }
    void SetCycle(float cycle) noexcept { m_flCycle = std::clamp(cycle, 0.0f, 1.0f); }

private:
    int   m_nSequence{0};
    float m_flCycle{0.0f};
    float m_flPlaybackRate{1.0f};
    bool  m_bSequenceFinished{false};
};

// ============================================================================
// Client Animated Entity Graph (C_BaseEntityAnimGraph)
// Reconstructed from client.dll (Size: 0x1268, Base Class of C_BaseCombatCharacter)
// Reference: docs/09_skeleton_bone_hierarchy_and_animgraph.md
// ============================================================================
class C_BaseEntityAnimGraph {
public:
    C_BaseEntityAnimGraph();
    virtual ~C_BaseEntityAnimGraph() = default;

    /**
     * @brief Primary client bone matrix setup pipeline.
     * Reconstructed from client.dll @ FUN_180aa0f80 / 0x180b2a310.
     * 1. Checks lazy evaluation dirty bitmask in CSkeletonInstance.
     * 2. Evaluates inter-tick bone transform interpolation (t_curr between t_prev and t_sim).
     * 3. Traverses skeletal hierarchy in topological order and concatenates local CTransforms.
     * 4. Evaluates optional two-bone IK constraints for leg/foot or arm/weapon placement.
     * 5. Converts CTransforms into 48-byte matrix3x4_t bones.
     * 6. Commits boneMask to m_nCalculatedBoneFlags to cache evaluation for the render frame.
     *
     * @param pOutBones Destination buffer for transformed matrix3x4_t bones
     * @param maxBones Size of destination buffer (must be >= boneCount)
     * @param boneMask Bitmask of requested bone types (e.g. BONE_USED_BY_HITBOX)
     * @param currentTime Current continuous render timestamp
     * @return true if bones successfully evaluated or retrieved from cache
     */
    bool SetupBones(matrix3x4_t* pOutBones, int maxBones, std::uint32_t boneMask, float currentTime) noexcept;

    /**
     * @brief Executes hierarchical forward kinematics bone transform propagation.
     * Matches client.dll @ CalculateWorldBones.
     */
    void CalculateWorldBones(std::uint32_t boneMask) noexcept;

    /**
     * @brief Computes world-space position of a bone by index.
     */
    [[nodiscard]] bool GetBonePosition(int boneIndex, Vector3D& outPos) const noexcept;

    /**
     * @brief Computes world-space matrix3x4_t of a bone by index.
     */
    [[nodiscard]] bool GetBoneMatrix(int boneIndex, matrix3x4_t& outMatrix) const noexcept;

    /**
     * @brief Projects a hitbox into world space using its attached bone matrix.
     */
    [[nodiscard]] bool GetHitboxWorldBounds(const CHitBox& hitbox, Vector3D& outWorldCenter, Vector3D& outWorldMins, Vector3D& outWorldMaxs) const noexcept;

    // Hierarchy Configuration
    void AttachSkeleton(CSkeletonInstance* pSkeleton, const int* pBoneParents, int boneCount) noexcept;
    void SetLocalBoneTransform(int boneIndex, const CTransform& localTransform) noexcept;

    // Simulation Timing
    void SetSimulationTimes(float flPrevSimTime, float flSimTime) noexcept {
        m_flPrevSimulationTime = flPrevSimTime;
        m_flSimulationTime = flSimTime;
    }

    [[nodiscard]] CSkeletonInstance* GetSkeletonInstance() const noexcept { return m_pSkeletonInstance; }
    [[nodiscard]] C_BaseAnimGraphController& GetAnimGraphController() noexcept { return m_animController; }
    [[nodiscard]] CBoneMergeCache& GetBoneMergeCache() noexcept { return m_mergeCache; }

private:
    CSkeletonInstance*        m_pSkeletonInstance{nullptr};
    C_BaseAnimGraphController m_animController;
    CBoneMergeCache           m_mergeCache;

    std::vector<int>          m_boneParents;          // Topological parent indices (-1 for root)
    std::vector<CTransform>   m_localBoneTransforms;  // Local bone pose relative to parent
    std::vector<CTransform>   m_worldBoneTransforms;  // Computed world-space CTransforms
    std::vector<matrix3x4_t>  m_cachedBoneMatrices;   // Cached matrix3x4_t array

    float m_flLastBoneSetupTime{-1.0f};
    float m_flPrevSimulationTime{0.0f};
    float m_flSimulationTime{0.0f};
};

} // namespace cs2::client

#endif // CS2_CLIENT_BONE_SETUP_H
