#include "rubikon_collision.h"
#include <algorithm>

namespace cs2::vphysics2 {

RayCastResult CRubikonCollision::IntersectRayAABB(const Ray_t& ray, const AABB_t& box, float skin) {
    RayCastResult res;

    const Vector3 boxMin = {box.mins.x - skin, box.mins.y - skin, box.mins.z - skin};
    const Vector3 boxMax = {box.maxs.x + skin, box.maxs.y + skin, box.maxs.z + skin};

    float tmin = 0.0f;
    float tmax = 1.0f;
    Vector3 hitNormal{0, 0, 0};

    // Slab test for 3 dimensions
    for (int i = 0; i < 3; ++i) {
        const float rayStart = (i == 0) ? ray.start.x : (i == 1 ? ray.start.y : ray.start.z);
        const float rayDelta = (i == 0) ? ray.delta.x : (i == 1 ? ray.delta.y : ray.delta.z);
        const float bMin = (i == 0) ? boxMin.x : (i == 1 ? boxMin.y : boxMin.z);
        const float bMax = (i == 0) ? boxMax.x : (i == 1 ? boxMax.y : boxMax.z);

        if (std::abs(rayDelta) < 1e-8f) {
            if (rayStart < bMin || rayStart > bMax) {
                return res; // Parallel and outside
            }
        } else {
            const float ood = 1.0f / rayDelta;
            float t1 = (bMin - rayStart) * ood;
            float t2 = (bMax - rayStart) * ood;
            float sign = -1.0f;

            if (t1 > t2) {
                std::swap(t1, t2);
                sign = 1.0f;
            }

            if (t1 > tmin) {
                tmin = t1;
                hitNormal = {0, 0, 0};
                if (i == 0) hitNormal.x = sign;
                else if (i == 1) hitNormal.y = sign;
                else hitNormal.z = sign;
            }
            tmax = std::min(tmax, t2);

            if (tmin > tmax) {
                return res;
            }
        }
    }

    if (tmin >= 0.0f && tmin <= 1.0f) {
        res.hit = true;
        res.fraction = tmin;
        res.normal = hitNormal;
        res.hitPoint = ray.start + (ray.delta * tmin);
    }

    return res;
}

RayCastResult CRubikonCollision::SweepSphereAgainstCapsule(
    const Vector3& sphereStart, const Vector3& sphereEnd, float sphereRadius,
    const Vector3& capP0, const Vector3& capP1, float capRadius) {
    
    RayCastResult res;
    const float combinedRadius = sphereRadius + capRadius;
    const Vector3 rayDelta = sphereEnd - sphereStart;

    // Approximate closest distance between ray and capsule segment
    Vector3 capDelta = capP1 - capP0;
    float capLenSqr = capDelta.LengthSqr();

    constexpr int kSteps = 16;
    for (int i = 0; i <= kSteps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSteps);
        Vector3 spherePos = sphereStart + (rayDelta * t);

        // Project spherePos onto capsule segment
        float u = 0.0f;
        if (capLenSqr > 1e-6f) {
            u = std::clamp((spherePos - capP0).Dot(capDelta) / capLenSqr, 0.0f, 1.0f);
        }
        Vector3 closestOnCap = capP0 + (capDelta * u);
        Vector3 diff = spherePos - closestOnCap;

        if (diff.LengthSqr() <= combinedRadius * combinedRadius) {
            res.hit = true;
            res.fraction = t;
            res.normal = diff.Normalized();
            res.hitPoint = closestOnCap + (res.normal * capRadius);
            return res;
        }
    }

    return res;
}

std::optional<uint32_t> CRubikonCollision::TraceRayBvh(const Ray_t& ray, const std::vector<RubikonBvhNode_t>& nodes) {
    if (nodes.empty()) return std::nullopt;

    std::vector<uint32_t> stack;
    stack.reserve(32);
    stack.push_back(0);

    float closestFraction = 1.0f;
    std::optional<uint32_t> bestPrimitive = std::nullopt;

    while (!stack.empty()) {
        uint32_t nodeIdx = stack.back();
        stack.pop_back();

        if (nodeIdx >= nodes.size()) continue;
        const auto& node = nodes[nodeIdx];

        AABB_t box{node.minBounds, node.maxBounds};
        RayCastResult hit = IntersectRayAABB(ray, box, 0.0f);

        if (hit.hit && hit.fraction <= closestFraction) {
            if (node.leftChild == 0xFFFFFFFF && node.rightChild == 0xFFFFFFFF) {
                // Leaf primitive
                closestFraction = hit.fraction;
                bestPrimitive = node.primitiveIndex;
            } else {
                if (node.rightChild != 0xFFFFFFFF) stack.push_back(node.rightChild);
                if (node.leftChild != 0xFFFFFFFF) stack.push_back(node.leftChild);
            }
        }
    }

    return bestPrimitive;
}

} // namespace cs2::vphysics2
