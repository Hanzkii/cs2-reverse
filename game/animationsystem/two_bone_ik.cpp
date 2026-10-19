#include "two_bone_ik.h"
#include <algorithm>

namespace cs2::animationsystem {

IKSolveResult CTwoBoneIKSolver::Solve(
    const Vector3& root,
    const Vector3& target,
    const Vector3& poleTarget,
    float len1,
    float len2) {

    IKSolveResult res;
    res.rootPos = root;

    Vector3 toTarget = target - root;
    float dist = toTarget.Length();
    const float maxDist = len1 + len2;
    const float minDist = std::abs(len1 - len2);

    // Target clamp / reachability
    if (dist >= maxDist) {
        res.reachedTarget = false;
        Vector3 dir = toTarget.Normalized();
        res.midPos = root + (dir * len1);
        res.effectorPos = root + (dir * maxDist);
        return res;
    }

    if (dist <= minDist) {
        dist = minDist + 1e-4f;
    }

    Vector3 targetDir = toTarget * (1.0f / dist);

    // Law of Cosines for the angle at the root joint: cos(alpha) = (L1^2 + D^2 - L2^2) / (2 * L1 * D)
    float cosAlpha = (len1 * len1 + dist * dist - len2 * len2) / (2.0f * len1 * dist);
    cosAlpha = std::clamp(cosAlpha, -1.0f, 1.0f);
    float sinAlpha = std::sqrt(std::max(0.0f, 1.0f - cosAlpha * cosAlpha));

    // Plane normal defined by root-target line and pole target
    Vector3 poleDir = (poleTarget - root).Normalized();
    Vector3 planeNormal = targetDir.Cross(poleDir);
    if (planeNormal.LengthSqr() < 1e-6f) {
        // Fallback default up vector
        planeNormal = targetDir.Cross(Vector3{0.0f, 0.0f, 1.0f});
        if (planeNormal.LengthSqr() < 1e-6f) {
            planeNormal = targetDir.Cross(Vector3{0.0f, 1.0f, 0.0f});
        }
    }
    planeNormal = planeNormal.Normalized();

    // Bend axis perpendicular to target line in the plane of the pole vector
    Vector3 bendDir = planeNormal.Cross(targetDir).Normalized();

    // Mid position = root + (targetDir * cosAlpha * len1) + (bendDir * sinAlpha * len1)
    res.midPos = root + (targetDir * (cosAlpha * len1)) + (bendDir * (sinAlpha * len1));
    res.effectorPos = target;
    res.reachedTarget = true;

    return res;
}

} // namespace cs2::animationsystem
