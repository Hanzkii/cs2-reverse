#include "animgraph_blender.h"
#include <cmath>
#include <algorithm>
#include <cassert>

namespace cs2::animationsystem {

Quaternion Quaternion::FromAngleAxis(const Vector3& axis, float angleRad) noexcept {
    // Reconstructed from animationsystem.dll @ FUN_18038dcc0
    float lenSqr = axis.LengthSqr();
    if (lenSqr < 1e-6f || std::abs(angleRad) < 1e-6f) {
        return {0.0f, 0.0f, 0.0f, 1.0f}; // Identity
    }

    float halfAngle = angleRad * 0.5f;
    float sinHalf = std::sin(halfAngle);
    float cosHalf = std::cos(halfAngle);

    Vector3 normAxis = axis.Normalized();
    return {
        normAxis.x * sinHalf,
        normAxis.y * sinHalf,
        normAxis.z * sinHalf,
        cosHalf
    };
}

Quaternion Quaternion::Nlerp(
    const Quaternion& q1,
    const Quaternion& q2,
    float t,
    bool shortestArc) noexcept {

    float dot = q1.Dot(q2);
    Quaternion targetQ = q2;

    // Shortest-arc check: if dot < 0, invert target to prevent 360-degree spins
    if (shortestArc && dot < 0.0f) {
        targetQ = -q2;
    }

    float invT = 1.0f - t;
    Quaternion result{
        q1.x * invT + targetQ.x * t,
        q1.y * invT + targetQ.y * t,
        q1.z * invT + targetQ.z * t,
        q1.w * invT + targetQ.w * t
    };

    return result.Normalized();
}

Quaternion Quaternion::Slerp(
    const Quaternion& q1,
    const Quaternion& q2,
    float t,
    bool shortestArc) noexcept {

    // Reconstructed from animationsystem.dll @ 0x1804721a0 / 0x180472ba1
    float dot = q1.Dot(q2);
    Quaternion targetQ = q2;

    // Shortest-arc check
    if (shortestArc && dot < 0.0f) {
        targetQ = -q2;
        dot = -dot;
    }

    // If quaternions are extremely close, fall back to Nlerp to prevent division by zero in sin(theta)
    if (dot > 0.9995f) {
        return Nlerp(q1, targetQ, t, false);
    }

    dot = std::clamp(dot, -1.0f, 1.0f);
    float theta = std::acos(dot);
    float sinTheta = std::sin(theta);

    if (sinTheta < 1e-6f) {
        return Nlerp(q1, targetQ, t, false);
    }

    float w1 = std::sin((1.0f - t) * theta) / sinTheta;
    float w2 = std::sin(t * theta) / sinTheta;

    return {
        q1.x * w1 + targetQ.x * w2,
        q1.y * w1 + targetQ.y * w2,
        q1.z * w1 + targetQ.z * w2,
        q1.w * w1 + targetQ.w * w2
    };
}

BoneTransform BoneTransform::Lerp(
    const BoneTransform& a,
    const BoneTransform& b,
    float t,
    bool useSlerp) noexcept {

    BoneTransform res;
    float invT = 1.0f - t;

    // Translation LERP
    res.translation = a.translation * invT + b.translation * t;

    // Rotation shortest-arc SLERP or NLERP
    if (useSlerp) {
        res.rotation = Quaternion::Slerp(a.rotation, b.rotation, t, true);
    } else {
        res.rotation = Quaternion::Nlerp(a.rotation, b.rotation, t, true);
    }

    // Scale LERP
    res.scale = a.scale * invT + b.scale * t;
    return res;
}

void CStateMachineUpdateNode::StartTransition(
    int targetStateID,
    float startTime,
    float duration,
    TransitionBlendType blendType) noexcept {

    m_nTargetStateID = targetStateID;
    m_flTransitionStartTime = startTime;
    m_flTransitionDuration = std::max(duration, 1e-4f);
    m_bIsTransitioning = true;
    m_eBlendType = blendType;
}

float CStateMachineUpdateNode::EvaluateTransitionProgress(float currentTime) const noexcept {
    // Reconstructed from animationsystem.dll @ FUN_180361670
    if (!m_bIsTransitioning || m_flTransitionDuration <= 0.0f) {
        return 1.0f;
    }

    float elapsed = currentTime - m_flTransitionStartTime;
    if (elapsed <= 0.0f) {
        return 0.0f;
    }

    float progress = elapsed / m_flTransitionDuration;
    return std::clamp(progress, 0.0f, 1.0f);
}

float CStateMachineUpdateNode::EvaluateCrossfadeWeight(float currentTime) const noexcept {
    float t = EvaluateTransitionProgress(currentTime);

    switch (m_eBlendType) {
        case TransitionBlendType::Linear:
            return TransitionBlendMath::Linear(t);
        case TransitionBlendType::HermiteSmoothStep:
        default:
            return TransitionBlendMath::HermiteSmoothStep(t);
    }
}

void CStateMachineUpdateNode::Update(const CAnimUpdateContext& context) noexcept {
    // Reconstructed from animationsystem.dll @ FUN_1802c82c0
    if (!m_bIsTransitioning) {
        return;
    }

    float progress = EvaluateTransitionProgress(context.m_flCurrentTime);
    if (progress >= 1.0f) {
        // Transition complete: commit target state
        m_nCurrentStateID = m_nTargetStateID;
        m_nTargetStateID = -1;
        m_bIsTransitioning = false;
        m_flTransitionStartTime = 0.0f;
        m_flTransitionDuration = 0.0f;
    }
}

std::pair<CBlendUpdateNode::ChildWeight, CBlendUpdateNode::ChildWeight>
CBlendUpdateNode::ComputeBlendWeights(float blendValue, int numChildren) noexcept {
    // Reconstructed from animationsystem.dll @ FUN_1802d7a80
    ChildWeight first{-1, 0.0f};
    ChildWeight second{-1, 0.0f};

    if (numChildren <= 0) {
        return {first, second};
    }
    if (numChildren == 1) {
        first = {0, 1.0f};
        return {first, second};
    }

    float clampedVal = std::clamp(blendValue, 0.0f, static_cast<float>(numChildren - 1));
    float baseIndexF = std::floor(clampedVal);
    int baseIndex = static_cast<int>(baseIndexF);

    if (baseIndex >= numChildren - 1) {
        first = {numChildren - 1, 1.0f};
        return {first, second};
    }

    float frac = clampedVal - baseIndexF; // In [0, 1]
    float weightA = 1.0f - frac;
    float weightB = frac;

    first = {baseIndex, weightA};
    second = {baseIndex + 1, weightB};

    return {first, second};
}

void CBlendUpdateNode::BlendPose(
    const std::vector<BoneTransform>& poseA,
    const std::vector<BoneTransform>& poseB,
    float weight,
    std::vector<BoneTransform>& outPose,
    bool useSlerp) noexcept {

    size_t count = std::min(poseA.size(), poseB.size());
    outPose.resize(count);

    float clampedWeight = std::clamp(weight, 0.0f, 1.0f);

    for (size_t i = 0; i < count; ++i) {
        outPose[i] = BoneTransform::Lerp(poseA[i], poseB[i], clampedWeight, useSlerp);
    }
}

} // namespace cs2::animationsystem
