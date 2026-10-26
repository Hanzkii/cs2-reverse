/**
 * VALVE SOURCE 2 (CS2) HITBOX STUDIO BONE OBB & CAPSULE RAYCAST SIMULATOR
 * Reconstructed directly from:
 *   - client.dll:
 *       CHitBox (0x70 bytes @ FUN_180a3e810)
 *       CHitBoxSet (0x30 bytes)
 *       Hitbox group damage multiplier table (Head=4.0x, Stomach=1.25x, Legs=0.75x)
 *   - vphysics2.dll:
 *       FUN_18016caf0 (Analytical ray-capsule swept cylinder & sphere solver)
 *
 * Demonstrates:
 *   1. CHitBox (0x70 bytes) layout with joint index, min/max bounds, and capsule radius.
 *   2. Bone matrix transformation mapping local hitbox primitives to world-space.
 *   3. Analytical finite ray-capsule distance & surface normal intersection solver.
 *   4. Multi-hitbox hierarchy intersection prioritization (closest positive t along ray).
 *   5. Authoritative hitgroup damage multiplier resolution.
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <string>
#include <algorithm>

namespace Source2 {

struct Vector3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    constexpr Vector3() = default;
    constexpr Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vector3 operator+(const Vector3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(float f) const noexcept { return {x * f, y * f, z * f}; }
    Vector3 operator/(float f) const noexcept { return {x / f, y / f, z / f}; }

    [[nodiscard]] float Dot(const Vector3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] Vector3 Cross(const Vector3& o) const noexcept {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(Dot(*this)); }
    [[nodiscard]] float LengthSqr() const noexcept { return Dot(*this); }
    [[nodiscard]] Vector3 Normalized() const noexcept {
        const float len = Length();
        return (len > 1e-6f) ? (*this / len) : Vector3{0, 0, 0};
    }
};

// 3x4 Affine Bone Transformation Matrix (MSVC 32-byte / 48-byte bone layout)
struct BoneMatrix {
    float m[3][4]{0.0f};

    static BoneMatrix Identity() {
        BoneMatrix b;
        b.m[0][0] = 1.0f; b.m[1][1] = 1.0f; b.m[2][2] = 1.0f;
        return b;
    }

    static BoneMatrix Translation(const Vector3& pos) {
        BoneMatrix b = Identity();
        b.m[0][3] = pos.x; b.m[1][3] = pos.y; b.m[2][3] = pos.z;
        return b;
    }

    [[nodiscard]] Vector3 TransformPoint(const Vector3& v) const noexcept {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3],
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3],
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3]
        };
    }
};

// HitGroup Enumeration
enum class HitGroup_t : std::int32_t {
    Generic  = 0,
    Head     = 1,
    Chest    = 2,
    Stomach  = 3,
    LeftArm  = 4,
    RightArm = 5,
    LeftLeg  = 6,
    RightLeg = 7,
    Neck     = 8
};

// Hitbox Structure (0x70 / 112 bytes layout in client.dll)
struct CHitBox {
    const char*   m_name{nullptr};          // +0x00
    const char*   m_sSurfaceProperty{nullptr}; // +0x08
    const char*   m_sBoneName{nullptr};     // +0x10
    Vector3       m_vMinBounds;             // +0x18
    Vector3       m_vMaxBounds;             // +0x24
    float         m_flShapeRadius{0.0f};    // +0x30 (Capsule radius if > 0.0f)
    std::uint32_t m_nBone{0};               // +0x34 (Parent bone index)
    HitGroup_t    m_nHitBoxGroup{HitGroup_t::Generic}; // +0x38
    std::uint8_t  m_pad3C[0x34]{0};         // +0x3C padding to 0x70 bytes
};
static_assert(sizeof(CHitBox) == 0x70, "CHitBox size mismatch");

struct Ray_t {
    Vector3 m_vStart;
    Vector3 m_vDirection; // Normalized
    float   m_flLength{8192.0f};
};

struct TraceHit_t {
    bool        m_bHit{false};
    float       m_flFraction{1.0f};
    float       m_flDistance{8192.0f};
    Vector3     m_vHitPoint;
    Vector3     m_vNormal;
    const CHitBox* m_pHitBox{nullptr};
    float       m_flDamageMultiplier{1.0f};
};

// ----------------------------------------------------------------------------
// Analytical Ray-Capsule Solver (FUN_18016caf0 in vphysics2.dll)
// ----------------------------------------------------------------------------
bool IntersectRayCapsule(
    const Ray_t& ray,
    const Vector3& p1,
    const Vector3& p2,
    float radius,
    float& outDist,
    Vector3& outNormal
) {
    const Vector3 d = p2 - p1;
    const Vector3 m = ray.m_vStart - p1;
    const Vector3 n = ray.m_vDirection;

    const float md = m.Dot(d);
    const float nd = n.Dot(d);
    const float dd = d.Dot(d);

    if (dd < 1e-6f) {
        // Degenerate capsule (sphere)
        const float b = m.Dot(n);
        const float c = m.Dot(m) - radius * radius;
        if (c > 0.0f && b > 0.0f) return false;
        const float discr = b * b - c;
        if (discr < 0.0f) return false;
        outDist = -b - std::sqrt(discr);
        if (outDist < 0.0f) outDist = 0.0f;
        outNormal = (ray.m_vStart + n * outDist - p1).Normalized();
        return true;
    }

    const float nn = n.Dot(n);
    const float mn = m.Dot(n);
    const float a = dd * nn - nd * nd;
    const float k = m.Dot(m) - radius * radius;
    const float c = dd * k - md * md;

    if (std::abs(a) < 1e-6f) {
        // Ray is parallel to cylinder axis
        return false;
    }

    const float b = dd * mn - nd * md;
    const float discr = b * b - a * c;
    if (discr < 0.0f) return false; // Missed infinite cylinder

    float t = (-b - std::sqrt(discr)) / a;
    if (t < 0.0f || t > ray.m_flLength) return false;

    // Check if hit point lies within segment interval [0, dd]
    const float y = md + t * nd;
    if (y >= 0.0f && y <= dd) {
        // Hit on cylinder mantle
        outDist = t;
        const Vector3 hitPt = ray.m_vStart + n * t;
        const Vector3 axisPt = p1 + d * (y / dd);
        outNormal = (hitPt - axisPt).Normalized();
        return true;
    }

    // Check hemispherical endcaps
    const Vector3 cap = (y < 0.0f) ? p1 : p2;
    const Vector3 mc = ray.m_vStart - cap;
    const float bc = mc.Dot(n);
    const float cc = mc.Dot(mc) - radius * radius;
    const float discrC = bc * bc - cc;
    if (discrC < 0.0f) return false;

    t = -bc - std::sqrt(discrC);
    if (t < 0.0f || t > ray.m_flLength) return false;

    outDist = t;
    outNormal = (ray.m_vStart + n * t - cap).Normalized();
    return true;
}

// Get damage multiplier for Source 2 hitgroups
float GetHitGroupMultiplier(HitGroup_t group) {
    switch (group) {
        case HitGroup_t::Head:    return 4.0f;
        case HitGroup_t::Stomach: return 1.25f;
        case HitGroup_t::Chest:
        case HitGroup_t::Neck:
        case HitGroup_t::Generic: return 1.0f;
        case HitGroup_t::LeftArm:
        case HitGroup_t::RightArm:return 1.0f;
        case HitGroup_t::LeftLeg:
        case HitGroup_t::RightLeg:return 0.75f;
        default: return 1.0f;
    }
}

} // namespace Source2

int main() {
    using namespace Source2;

    std::cout << "===============================================================================\n";
    std::cout << "  VALVE SOURCE 2 (CS2) HITBOX STUDIO BONE OBB & CAPSULE RAYCAST SIMULATOR\n";
    std::cout << "  Reconstructed from client.dll (CHitBox @ 0x70) & vphysics2.dll (FUN_18016caf0)\n";
    std::cout << "===============================================================================\n\n";

    // Setup bone matrices in world space
    BoneMatrix headBone = BoneMatrix::Translation({0.0f, 0.0f, 68.0f});   // Head joint z=68
    BoneMatrix chestBone = BoneMatrix::Translation({0.0f, 0.0f, 50.0f});  // Chest joint z=50
    BoneMatrix pelvisBone = BoneMatrix::Translation({0.0f, 0.0f, 36.0f}); // Pelvis joint z=36

    // Configure Hitboxes (Source 2 model configuration)
    std::vector<CHitBox> hitboxes(3);

    // Head capsule
    hitboxes[0].m_name = "hitbox_head";
    hitboxes[0].m_sBoneName = "head_0";
    hitboxes[0].m_nBone = 6;
    hitboxes[0].m_nHitBoxGroup = HitGroup_t::Head;
    hitboxes[0].m_vMinBounds = Vector3(-2.5f, 0.0f, -1.0f);
    hitboxes[0].m_vMaxBounds = Vector3(2.5f, 0.0f, 4.5f);
    hitboxes[0].m_flShapeRadius = 4.25f; // Capsule radius

    // Chest capsule
    hitboxes[1].m_name = "hitbox_chest";
    hitboxes[1].m_sBoneName = "spine_2";
    hitboxes[1].m_nBone = 5;
    hitboxes[1].m_nHitBoxGroup = HitGroup_t::Chest;
    hitboxes[1].m_vMinBounds = Vector3(-5.5f, -3.0f, -4.0f);
    hitboxes[1].m_vMaxBounds = Vector3(5.5f, 3.0f, 6.0f);
    hitboxes[1].m_flShapeRadius = 5.5f;

    // Pelvis/Stomach capsule
    hitboxes[2].m_name = "hitbox_pelvis";
    hitboxes[2].m_sBoneName = "pelvis";
    hitboxes[2].m_nBone = 0;
    hitboxes[2].m_nHitBoxGroup = HitGroup_t::Stomach;
    hitboxes[2].m_vMinBounds = Vector3(-6.0f, -4.0f, -5.0f);
    hitboxes[2].m_vMaxBounds = Vector3(6.0f, 4.0f, 3.0f);
    hitboxes[2].m_flShapeRadius = 6.0f;

    std::cout << "[PHASE 1] Configured Hitboxes with Exact 0x70 Struct Layout:\n";
    for (const auto& hb : hitboxes) {
        std::cout << "  - " << std::left << std::setw(15) << hb.m_name
                  << " Bone #" << hb.m_nBone 
                  << " Group: " << static_cast<int>(hb.m_nHitBoxGroup)
                  << " Radius: " << hb.m_flShapeRadius << " units\n";
    }

    std::cout << "\n[PHASE 2] Simulating Direct Bullet Rays from Eyepoint (0, -200, 70)...\n";
    const Vector3 eyeOrigin{0.0f, -200.0f, 70.0f};

    // Test 1: Headshot Ray aimed directly at z=70
    Ray_t headRay;
    headRay.m_vStart = eyeOrigin;
    headRay.m_vDirection = (Vector3{0.0f, 0.0f, 70.0f} - eyeOrigin).Normalized();

    TraceHit_t bestHit;
    for (const auto& hb : hitboxes) {
        const BoneMatrix& bMat = (hb.m_nHitBoxGroup == HitGroup_t::Head) ? headBone :
                                 (hb.m_nHitBoxGroup == HitGroup_t::Chest) ? chestBone : pelvisBone;
        const Vector3 worldP1 = bMat.TransformPoint(hb.m_vMinBounds);
        const Vector3 worldP2 = bMat.TransformPoint(hb.m_vMaxBounds);

        float dist = 0.0f;
        Vector3 norm;
        if (IntersectRayCapsule(headRay, worldP1, worldP2, hb.m_flShapeRadius, dist, norm)) {
            if (dist < bestHit.m_flDistance) {
                bestHit.m_bHit = true;
                bestHit.m_flDistance = dist;
                bestHit.m_vHitPoint = headRay.m_vStart + headRay.m_vDirection * dist;
                bestHit.m_vNormal = norm;
                bestHit.m_pHitBox = &hb;
                bestHit.m_flDamageMultiplier = GetHitGroupMultiplier(hb.m_nHitBoxGroup);
            }
        }
    }

    assert(bestHit.m_bHit);
    assert(bestHit.m_pHitBox->m_nHitBoxGroup == HitGroup_t::Head);
    std::cout << "  [TEST 1: HEADSHOT RAY]\n"
              << "    Hit: " << bestHit.m_pHitBox->m_name << " (" << bestHit.m_pHitBox->m_sBoneName << ")\n"
              << "    Distance: " << std::fixed << std::setprecision(2) << bestHit.m_flDistance << " units\n"
              << "    Normal: (" << bestHit.m_vNormal.x << ", " << bestHit.m_vNormal.y << ", " << bestHit.m_vNormal.z << ")\n"
              << "    Damage Multiplier: " << bestHit.m_flDamageMultiplier << "x (HEAD CRITICAL!)\n\n";

    // Test 2: Torso Ray aimed at z=52
    Ray_t chestRay;
    chestRay.m_vStart = eyeOrigin;
    chestRay.m_vDirection = (Vector3{0.0f, 0.0f, 52.0f} - eyeOrigin).Normalized();

    TraceHit_t chestHit;
    for (const auto& hb : hitboxes) {
        const BoneMatrix& bMat = (hb.m_nHitBoxGroup == HitGroup_t::Head) ? headBone :
                                 (hb.m_nHitBoxGroup == HitGroup_t::Chest) ? chestBone : pelvisBone;
        const Vector3 worldP1 = bMat.TransformPoint(hb.m_vMinBounds);
        const Vector3 worldP2 = bMat.TransformPoint(hb.m_vMaxBounds);

        float dist = 0.0f;
        Vector3 norm;
        if (IntersectRayCapsule(chestRay, worldP1, worldP2, hb.m_flShapeRadius, dist, norm)) {
            if (dist < chestHit.m_flDistance) {
                chestHit.m_bHit = true;
                chestHit.m_flDistance = dist;
                chestHit.m_pHitBox = &hb;
                chestHit.m_flDamageMultiplier = GetHitGroupMultiplier(hb.m_nHitBoxGroup);
            }
        }
    }

    assert(chestHit.m_bHit);
    assert(chestHit.m_pHitBox->m_nHitBoxGroup == HitGroup_t::Chest);
    std::cout << "  [TEST 2: CHEST RAY]\n"
              << "    Hit: " << chestHit.m_pHitBox->m_name << "\n"
              << "    Damage Multiplier: " << chestHit.m_flDamageMultiplier << "x (STANDARD BODY)\n\n";

    std::cout << "===============================================================================\n";
    std::cout << "  ALL SOURCE 2 HITBOX & CAPSULE RAYCAST CHECKS PASSED SUCCESSFULLY!\n";
    std::cout << "===============================================================================\n";
    return 0;
}
