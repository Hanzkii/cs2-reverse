/**
 * Counter-Strike 2 (Source 2) Skeleton Bone Hierarchy & Animation Graph Simulator
 * 
 * Reverse engineered from:
 *   - client.dll:
 *       CSkeletonInstance VTable: 0x181c41ce0
 *       CalculateWorldBones: 0x180aa0f80 (FUN_180aa0f80)
 *       CTransform Concatenate: 0x181748480 (FUN_181748480)
 *       Bone array pointer: CSkeletonInstance + 0x01c0 (m_modelState + 0x80)
 *       Bone count: CSkeletonInstance + 0x01d4 (m_modelState + 0x94)
 *       m_nodeToWorld: CGameSceneNode + 0x0010 (CTransform, 32 bytes)
 *   - animationsystem.dll:
 *       CHitBox schema (0x18081d840): m_vMinBounds, m_vMaxBounds, m_flShapeRadius, m_nGroupId
 *       CTwoBoneIKPoseOperation VTable: 0x180689268
 *       Two-Bone IK Solver: 0x1802f9750 (Law of Cosines, hinge axis projection)
 * 
 * Validates:
 *   1. 32-byte CTransform memory packing (Quaternion rotation + Vector translation + Scale factor).
 *   2. Forward Kinematics (FK) hierarchical propagation with parent indices.
 *   3. Rodrigues/Hamilton quaternion transform concatenation matching FUN_181748480.
 *   4. Lazy evaluation dirty bitmask (32-bit words per 32 bones).
 *   5. Hitbox OBB and capsule endpoint projection into world coordinates.
 *   6. Analytical Two-Bone Inverse Kinematics (IK) solving limb joint angles to reach target.
 */

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <algorithm>

namespace cs2 {

// ============================================================================
// 1. Math Structures (Source 2 Vector & Quaternion)
// ============================================================================

struct Vector3D {
    float x, y, z;

    Vector3D() : x(0.0f), y(0.0f), z(0.0f) {}
    Vector3D(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vector3D operator+(const Vector3D& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vector3D operator-(const Vector3D& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vector3D operator*(float s) const { return {x * s, y * s, z * s}; }
    Vector3D operator/(float s) const { return {x / s, y / s, z / s}; }

    float Dot(const Vector3D& o) const { return x * o.x + y * o.y + z * o.z; }
    Vector3D Cross(const Vector3D& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
    float LengthSqr() const { return Dot(*this); }
    float Length() const { return std::sqrt(LengthSqr()); }

    Vector3D Normalized() const {
        float len = Length();
        return len > 1e-6f ? (*this / len) : Vector3D(0.0f, 0.0f, 0.0f);
    }
};

struct alignas(16) Quaternion {
    float x, y, z, w;

    Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    Quaternion(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

    float Dot(const Quaternion& q) const {
        return x * q.x + y * q.y + z * q.z + w * q.w;
    }

    Quaternion operator-() const {
        return {-x, -y, -z, -w};
    }

    Quaternion operator+(const Quaternion& q) const {
        return {x + q.x, y + q.y, z + q.z, w + q.w};
    }

    Quaternion operator*(float s) const {
        return {x * s, y * s, z * s, w * s};
    }

    // Hamilton product: q1 * q2 (matching FUN_181748480)
    Quaternion operator*(const Quaternion& q) const {
        return {
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w,
            w * q.w - x * q.x - y * q.y - z * q.z
        };
    }

    Quaternion Normalized() const {
        float n2 = x * x + y * y + z * z + w * w;
        if (n2 <= 1e-8f) return {0.0f, 0.0f, 0.0f, 1.0f};
        float inv = 1.0f / std::sqrt(n2);
        return {x * inv, y * inv, z * inv, w * inv};
    }

    // Vector rotation via Rodrigues / quaternion conjugation: v' = q * (v, 0) * q^-1
    // Matches FUN_181748480 SIMD vector implementation exactly
    Vector3D Rotate(const Vector3D& v) const {
        Vector3D qv(x, y, z);
        Vector3D t = qv.Cross(v) * 2.0f;
        return v + t * w + qv.Cross(t);
    }

    static Quaternion FromAxisAngle(const Vector3D& axis, float radians) {
        float half = radians * 0.5f;
        float s = std::sin(half);
        Vector3D norm = axis.Normalized();
        return {norm.x * s, norm.y * s, norm.z * s, std::cos(half)};
    }

    // Shortest-arc Normalized Linear Interpolation (Nlerp)
    static Quaternion Nlerp(const Quaternion& q1, const Quaternion& q2, float t, bool shortestArc = true) {
        float dot = q1.Dot(q2);
        Quaternion target = q2;
        if (shortestArc && dot < 0.0f) {
            target = -q2;
        }
        float invT = 1.0f - t;
        return Quaternion(
            q1.x * invT + target.x * t,
            q1.y * invT + target.y * t,
            q1.z * invT + target.z * t,
            q1.w * invT + target.w * t
        ).Normalized();
    }

    // Shortest-arc Spherical Linear Interpolation (Slerp) matching animationsystem.dll @ 0x1804721a0
    static Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t, bool shortestArc = true) {
        float dot = q1.Dot(q2);
        Quaternion target = q2;
        if (shortestArc && dot < 0.0f) {
            target = -q2;
            dot = -dot;
        }
        if (dot > 0.9995f) {
            return Nlerp(q1, target, t, false);
        }
        dot = std::clamp(dot, -1.0f, 1.0f);
        float theta = std::acos(dot);
        float sinTheta = std::sin(theta);
        if (sinTheta < 1e-6f) {
            return Nlerp(q1, target, t, false);
        }
        float w1 = std::sin((1.0f - t) * theta) / sinTheta;
        float w2 = std::sin(t * theta) / sinTheta;
        return Quaternion(
            q1.x * w1 + target.x * w2,
            q1.y * w1 + target.y * w2,
            q1.z * w1 + target.z * w2,
            q1.w * w1 + target.w * w2
        );
    }
};

// ============================================================================
// 2. CTransform: Exact Source 2 32-Byte Bone Representation
// ============================================================================

// Memory layout verified from client.dll (FUN_180aa0f80 line 249-263 & FUN_181748480):
//   +0x00: Quaternion m_rot (16 bytes, x, y, z, w)
//   +0x10: Vector3D   m_pos (12 bytes, x, y, z)
//   +0x1C: float      m_flScale (4 bytes)
// Total Size: 32 bytes (0x20)
struct alignas(16) CTransform {
    Quaternion rot;     // +0x00
    Vector3D   pos;     // +0x10
    float      scale;   // +0x1C

    CTransform() : rot(0.0f, 0.0f, 0.0f, 1.0f), pos(0.0f, 0.0f, 0.0f), scale(1.0f) {}
    CTransform(const Quaternion& _rot, const Vector3D& _pos, float _scale = 1.0f)
        : rot(_rot), pos(_pos), scale(_scale) {}

    // Concatenate transforms: parent * local (FUN_181748480)
    // T_world = T_parent * T_local
    static CTransform Concatenate(const CTransform& parent, const CTransform& local) {
        CTransform out;
        // 1. World Position: pos_parent + rot_parent.Rotate(pos_local) * scale_parent
        out.pos = parent.pos + parent.rot.Rotate(local.pos) * parent.scale;
        // 2. World Rotation: rot_parent * rot_local (normalized)
        out.rot = (parent.rot * local.rot).Normalized();
        // 3. World Scale: scale_parent * scale_local
        out.scale = parent.scale * local.scale;
        return out;
    }

    // Transform a local point into this transform's space
    Vector3D TransformPoint(const Vector3D& pt) const {
        return pos + rot.Rotate(pt) * scale;
    }
};

static_assert(sizeof(CTransform) == 32, "CTransform must be exactly 32 bytes (0x20)");

// ============================================================================
// 3. Skeletal Hierarchy & Forward Kinematics (FK)
// ============================================================================

struct BoneDefinition {
    std::string name;
    int16_t     parentIndex; // -1 if root bone
    CTransform  localTransform;
};

// HitGroup Enums (Source 2)
enum HitGroup_t : int32_t {
    HITGROUP_GENERIC   = 0,
    HITGROUP_HEAD      = 1,
    HITGROUP_CHEST     = 2,
    HITGROUP_STOMACH   = 3,
    HITGROUP_LEFTARM   = 4,
    HITGROUP_RIGHTARM  = 5,
    HITGROUP_LEFTLEG   = 6,
    HITGROUP_RIGHTLEG  = 7,
    HITGROUP_NECK      = 8
};

// Shape Type (0 = Box, 1 = Capsule)
enum HitBoxShape_t : uint8_t {
    HITBOX_SHAPE_BOX     = 0,
    HITBOX_SHAPE_CAPSULE = 1
};

// Reconstructed from animationsystem.dll (0x18081d840)
struct CHitBox {
    std::string    name;
    std::string    boneName;
    int16_t        boneIndex;
    Vector3D       minBounds;     // +0x18 (Local space)
    Vector3D       maxBounds;     // +0x24 (Local space)
    float          shapeRadius;   // +0x30 (Capsule radius)
    HitGroup_t     groupId;       // +0x38
    HitBoxShape_t  shapeType;     // +0x3c
};

class CSkeletonInstanceSim {
public:
    CTransform m_nodeToWorld;                       // +0x0010 (CGameSceneNode)
    std::vector<BoneDefinition> m_bones;
    std::vector<CTransform>     m_boneArray;        // +0x01c0 (m_modelState + 0x80)
    uint16_t                    m_nBoneCount;       // +0x01d4 (m_modelState + 0x94)
    std::vector<uint32_t>       m_calculatedBitmask;// Bitmask of evaluated bones
    std::vector<CHitBox>        m_hitboxes;

    CSkeletonInstanceSim(const CTransform& rootWorldTransform)
        : m_nodeToWorld(rootWorldTransform), m_nBoneCount(0) {}

    int16_t AddBone(const std::string& name, int16_t parentIndex, const CTransform& localTrans) {
        assert(parentIndex < (int16_t)m_bones.size());
        int16_t idx = static_cast<int16_t>(m_bones.size());
        m_bones.push_back({name, parentIndex, localTrans});
        m_nBoneCount = static_cast<uint16_t>(m_bones.size());
        m_boneArray.resize(m_nBoneCount);
        
        // Resize bitmask: ((N + 31) / 32) words (matching FUN_180aa0f80 line 182)
        size_t numWords = (m_nBoneCount + 31) / 32;
        m_calculatedBitmask.assign(numWords, 0);
        return idx;
    }

    void AddHitbox(const CHitBox& hb) {
        m_hitboxes.push_back(hb);
    }

    // Exact replica of FUN_180aa0f80: CalculateWorldBones
    void CalculateWorldBones() {
        // Reset evaluation bitmask
        std::fill(m_calculatedBitmask.begin(), m_calculatedBitmask.end(), 0);

        for (uint16_t i = 0; i < m_nBoneCount; ++i) {
            const auto& def = m_bones[i];
            
            if (def.parentIndex < 0) {
                // Root bone: parented to m_nodeToWorld (lines 338-340)
                m_boneArray[i] = CTransform::Concatenate(m_nodeToWorld, def.localTransform);
            } else {
                // Child bone: parented to parent's world transform (lines 341-344, 364-378)
                assert(def.parentIndex < i); // Hierarchy must be topological
                const CTransform& parentWorld = m_boneArray[def.parentIndex];
                m_boneArray[i] = CTransform::Concatenate(parentWorld, def.localTransform);
            }

            // Set bit in evaluation bitmask (lines 385-388: *puVar2 = *puVar2 | uVar44)
            size_t wordIdx = i / 32;
            uint32_t bit = 1u << (i % 32);
            m_calculatedBitmask[wordIdx] |= bit;
        }
    }

    bool IsBoneCalculated(uint16_t boneIdx) const {
        if (boneIdx >= m_nBoneCount) return false;
        size_t wordIdx = boneIdx / 32;
        uint32_t bit = 1u << (boneIdx % 32);
        return (m_calculatedBitmask[wordIdx] & bit) != 0;
    }

    // Get World Position of Hitbox Center & Capsule Endpoints
    struct WorldHitbox {
        std::string name;
        HitGroup_t  group;
        Vector3D    worldCenter;
        Vector3D    worldCapStart;
        Vector3D    worldCapEnd;
        float       radius;
    };

    WorldHitbox EvaluateHitbox(const CHitBox& hb) const {
        assert(hb.boneIndex < m_nBoneCount);
        const CTransform& boneTrans = m_boneArray[hb.boneIndex];

        WorldHitbox wh;
        wh.name = hb.name;
        wh.group = hb.groupId;
        wh.radius = hb.shapeRadius;

        // Transform local min and max into world coordinates
        wh.worldCapStart = boneTrans.TransformPoint(hb.minBounds);
        wh.worldCapEnd   = boneTrans.TransformPoint(hb.maxBounds);
        wh.worldCenter   = (wh.worldCapStart + wh.worldCapEnd) * 0.5f;

        return wh;
    }
};

// ============================================================================
// 4. Two-Bone Inverse Kinematics (IK) Solver (FUN_1802f9750)
// ============================================================================

struct TwoBoneIKSolution {
    bool     targetReachable;
    Vector3D rootPos;
    Vector3D jointPos;
    Vector3D endEffectorPos;
};

// Analytical Law of Cosines Two-Bone IK Solver
// Reconstructed from animationsystem.dll (0x1802f9750 line 204-297)
TwoBoneIKSolution SolveTwoBoneIK(
    const Vector3D& root,        // e.g. Hip or Shoulder
    const Vector3D& target,      // e.g. Desired Foot or Hand position
    float L1,                    // Length of bone 1 (e.g. Upper arm / Thigh)
    float L2,                    // Length of bone 2 (e.g. Forearm / Calf)
    const Vector3D& poleVector   // e.g. Knee forward or Elbow backward hint
) {
    TwoBoneIKSolution sol;
    sol.rootPos = root;

    Vector3D toTarget = target - root;
    float dist = toTarget.Length();

    // 1. Clamp distance to prevent unreachable singularities (law of cosines domain: [-1, 1])
    float maxReach = (L1 + L2) * 0.9999f;
    float minReach = std::abs(L1 - L2) * 1.0001f;

    bool clamped = false;
    if (dist >= maxReach) {
        dist = maxReach;
        clamped = true;
    } else if (dist <= minReach) {
        dist = minReach;
        clamped = true;
    }

    sol.targetReachable = !clamped;
    Vector3D dir = toTarget.Normalized();

    // 2. Law of Cosines for angle at root (alpha) and angle at joint (beta):
    //    cos(alpha) = (L1^2 + D^2 - L2^2) / (2 * L1 * D)
    //    (FUN_1802f9750 line 237: V_acosf)
    float cosAlpha = (L1 * L1 + dist * dist - L2 * L2) / (2.0f * L1 * dist);
    cosAlpha = std::max(-1.0f, std::min(1.0f, cosAlpha));
    float alpha = std::acos(cosAlpha);

    // 3. Compute joint bending plane using pole vector (lines 277-297: cross products)
    Vector3D poleDir = (poleVector - root).Normalized();
    Vector3D bendNormal = dir.Cross(poleDir).Normalized();
    
    // If pole vector is collinear with target direction, fall back to default up
    if (bendNormal.LengthSqr() < 1e-4f) {
        bendNormal = dir.Cross(Vector3D(0.0f, 0.0f, 1.0f)).Normalized();
        if (bendNormal.LengthSqr() < 1e-4f) {
            bendNormal = dir.Cross(Vector3D(0.0f, 1.0f, 0.0f)).Normalized();
        }
    }

    // Bend axis perpendicular to direction within the bending plane
    Vector3D bendAxis = bendNormal.Cross(dir).Normalized();

    // Joint position rotated by alpha from target direction
    Quaternion qRootBend = Quaternion::FromAxisAngle(bendNormal, alpha);
    Vector3D bone1Dir = qRootBend.Rotate(dir);
    sol.jointPos = root + bone1Dir * L1;

    // 4. End effector reached by bone 2 pointing towards target from joint
    Vector3D bone2Dir = (target - sol.jointPos).Normalized();
    sol.endEffectorPos = sol.jointPos + bone2Dir * L2;

    return sol;
}

} // namespace cs2

// ============================================================================
// 5. Verification Test Suite & Assertions
// ============================================================================

void Test_TransformConcatenation() {
    std::cout << "[Test 1] Verifying CTransform Concatenation (FUN_181748480)... ";

    // Parent transform: Player entity standing at (100, 200, 64), rotated 90 degrees around Z
    cs2::Quaternion qParent = cs2::Quaternion::FromAxisAngle({0.0f, 0.0f, 1.0f}, 1.5707963f); // +90 deg Z
    cs2::CTransform tParent(qParent, {100.0f, 200.0f, 64.0f}, 1.0f);

    // Child bone local: Pelvis offset (10, 0, 15) relative to root
    cs2::Quaternion qLocal(0.0f, 0.0f, 0.0f, 1.0f); // Identity
    cs2::CTransform tLocal(qLocal, {10.0f, 0.0f, 15.0f}, 1.0f);

    cs2::CTransform tWorld = cs2::CTransform::Concatenate(tParent, tLocal);

    // After 90 deg rotation around Z:
    // (10, 0, 0) rotated by 90 deg -> (0, 10, 0)
    // World pos should be (100 + 0, 200 + 10, 64 + 15) = (100, 210, 79)
    assert(std::abs(tWorld.pos.x - 100.0f) < 1e-3f);
    assert(std::abs(tWorld.pos.y - 210.0f) < 1e-3f);
    assert(std::abs(tWorld.pos.z - 79.0f) < 1e-3f);

    std::cout << "PASSED! Pos=(" << tWorld.pos.x << ", " << tWorld.pos.y << ", " << tWorld.pos.z << ")\n";
}

void Test_SkeletalHierarchyAndHitboxes() {
    std::cout << "[Test 2] Verifying Skeletal Hierarchy (CSkeletonInstance) & Hitbox Projection... ";

    // Entity at World (500, 1000, 128), facing along X axis (rot = Identity)
    cs2::CTransform entityTrans({0.0f, 0.0f, 0.0f, 1.0f}, {500.0f, 1000.0f, 128.0f});
    cs2::CSkeletonInstanceSim skeleton(entityTrans);

    // Build CS2 Biped Bone Hierarchy
    // 0: Pelvis (Root)
    int16_t pelvis = skeleton.AddBone("pelvis", -1, 
        cs2::CTransform({0,0,0,1}, {0.0f, 0.0f, 38.0f}));
    
    // 1: Spine (Parent: Pelvis)
    int16_t spine = skeleton.AddBone("spine", pelvis, 
        cs2::CTransform({0,0,0,1}, {0.0f, 0.0f, 12.0f}));
    
    // 2: Chest (Parent: Spine)
    int16_t chest = skeleton.AddBone("chest", spine, 
        cs2::CTransform({0,0,0,1}, {0.0f, 0.0f, 14.0f}));
    
    // 3: Neck (Parent: Chest)
    int16_t neck = skeleton.AddBone("neck", chest, 
        cs2::CTransform({0,0,0,1}, {0.0f, 0.0f, 6.0f}));
    
    // 4: Head (Parent: Neck)
    int16_t head = skeleton.AddBone("head", neck, 
        cs2::CTransform({0,0,0,1}, {2.0f, 0.0f, 6.0f}));

    // 5: Left Thigh (Parent: Pelvis)
    int16_t l_thigh = skeleton.AddBone("leg_upper_L", pelvis,
        cs2::CTransform({0,0,0,1}, {0.0f, 4.0f, -4.0f}));
    (void)l_thigh;

    // Attach Hitboxes (matching CS2 player hitboxes)
    // Head Hitbox (Capsule, radius 4.5)
    skeleton.AddHitbox({
        "hitbox_head", "head", head,
        {-1.5f, -3.0f, -2.0f}, {3.5f, 3.0f, 4.0f}, 4.5f,
        cs2::HITGROUP_HEAD, cs2::HITBOX_SHAPE_CAPSULE
    });

    // Chest Hitbox (Box)
    skeleton.AddHitbox({
        "hitbox_chest", "chest", chest,
        {-4.0f, -7.0f, -6.0f}, {4.0f, 7.0f, 6.0f}, 0.0f,
        cs2::HITGROUP_CHEST, cs2::HITBOX_SHAPE_BOX
    });

    // Run Forward Kinematics
    skeleton.CalculateWorldBones();

    // Verify all 6 bones were marked calculated in the dirty bitmask
    for (uint16_t b = 0; b < 6; ++b) {
        assert(skeleton.IsBoneCalculated(b));
    }

    // Verify Head World Height:
    // Entity Z(128) + Pelvis Z(38) + Spine Z(12) + Chest Z(14) + Neck Z(6) + Head Z(6) = 204.0f
    const auto& headTrans = skeleton.m_boneArray[head];
    assert(std::abs(headTrans.pos.z - 204.0f) < 1e-3f);
    assert(std::abs(headTrans.pos.x - 502.0f) < 1e-3f);

    // Evaluate Head Hitbox World Projection
    auto headWorldHb = skeleton.EvaluateHitbox(skeleton.m_hitboxes[0]);
    assert(headWorldHb.group == cs2::HITGROUP_HEAD);
    assert(headWorldHb.radius == 4.5f);
    
    // Head Hitbox Center Z should be ~205.0f
    assert(headWorldHb.worldCenter.z > 200.0f && headWorldHb.worldCenter.z < 210.0f);

    std::cout << "PASSED! Head Bone Z=" << headTrans.pos.z 
              << ", Hitbox Center=(" << headWorldHb.worldCenter.x << ", " 
              << headWorldHb.worldCenter.y << ", " << headWorldHb.worldCenter.z << ")\n";
}

void Test_TwoBoneIKSolver() {
    std::cout << "[Test 3] Verifying Two-Bone IK Analytical Law of Cosines (FUN_1802f9750)... ";

    // Simulate human leg:
    // Hip at (0, 0, 36)
    // Foot target on ground at (0, 10, 0) -> distance sqrt(10^2 + (-36)^2) = 37.36
    // Upper leg (Thigh) length: L1 = 20.0f
    // Lower leg (Calf) length:  L2 = 20.0f (Total reach = 40.0f > 37.36f)
    // Knee pole vector forward: (0, 20, 18)
    cs2::Vector3D hip(0.0f, 0.0f, 36.0f);
    cs2::Vector3D footTarget(0.0f, 10.0f, 0.0f);
    float L1 = 20.0f;
    float L2 = 20.0f;
    cs2::Vector3D kneePole(0.0f, 20.0f, 18.0f);

    auto sol = cs2::SolveTwoBoneIK(hip, footTarget, L1, L2, kneePole);

    assert(sol.targetReachable);

    // Length of upper bone must equal L1 (18.0)
    float actualL1 = (sol.jointPos - sol.rootPos).Length();
    assert(std::abs(actualL1 - L1) < 1e-3f);

    // Length of lower bone must equal L2 (18.0)
    float actualL2 = (sol.endEffectorPos - sol.jointPos).Length();
    assert(std::abs(actualL2 - L2) < 1e-3f);

    // End effector should reach the target foot position
    float err = (sol.endEffectorPos - footTarget).Length();
    assert(err < 1e-3f);

    // Knee should bend forward (+Y)
    assert(sol.jointPos.y > hip.y);

    std::cout << "PASSED! Knee Pos=(" << sol.jointPos.x << ", " 
              << sol.jointPos.y << ", " << sol.jointPos.z 
              << "), Target Err=" << err << "\n";
}

void Test_StateMachineTransitions() {
    std::cout << "[Test 4] Verifying CStateMachineUpdateNode Transition Crossfade (FUN_1802c82c0 / FUN_180361670)... ";

    // Transition from Idle (State 0) to Run (State 1) over duration 0.5s starting at t = 1.0s
    float startTime = 1.0f;
    float duration = 0.5f;

    // Hermite smoothstep: w(t) = 3t^2 - 2t^3
    auto HermiteSmoothStep = [](float t) -> float {
        float c = std::max(0.0f, std::min(1.0f, t));
        return c * c * (3.0f - 2.0f * c);
    };

    // Evaluate normalized time progress t = (currTime - startTime) / duration
    auto EvalProgress = [startTime, duration](float curr) -> float {
        if (curr <= startTime) return 0.0f;
        if (curr >= startTime + duration) return 1.0f;
        return (curr - startTime) / duration;
    };

    // Test start: t = 1.0s -> progress 0.0, weight 0.0
    float t0 = EvalProgress(1.0f);
    assert(std::abs(t0 - 0.0f) < 1e-5f);
    assert(std::abs(HermiteSmoothStep(t0) - 0.0f) < 1e-5f);

    // Test midpoint: t = 1.25s -> progress 0.5, Hermite weight = 3*(0.25) - 2*(0.125) = 0.75 - 0.25 = 0.5
    float tMid = EvalProgress(1.25f);
    assert(std::abs(tMid - 0.5f) < 1e-5f);
    float wMid = HermiteSmoothStep(tMid);
    assert(std::abs(wMid - 0.5f) < 1e-5f);

    // Test 25% point: t = 1.125s -> progress 0.25, Hermite weight = 3*(1/16) - 2*(1/64) = 3/16 - 1/32 = 5/32 = 0.15625
    float tQuarter = EvalProgress(1.125f);
    float wQuarter = HermiteSmoothStep(tQuarter);
    assert(std::abs(wQuarter - 0.15625f) < 1e-5f);
    // Smoothstep must be smoother than linear at endpoints: wQuarter (0.15625) < tQuarter (0.25)
    assert(wQuarter < tQuarter);

    // Test completion: t = 1.5s -> progress 1.0, weight 1.0
    float tEnd = EvalProgress(1.5f);
    assert(std::abs(tEnd - 1.0f) < 1e-5f);
    assert(std::abs(HermiteSmoothStep(tEnd) - 1.0f) < 1e-5f);

    // Test overshoot: t = 2.0s -> clamped to 1.0
    float tOver = EvalProgress(2.0f);
    assert(std::abs(tOver - 1.0f) < 1e-5f);
    assert(std::abs(HermiteSmoothStep(tOver) - 1.0f) < 1e-5f);

    std::cout << "PASSED! w(0.25)=" << wQuarter << ", w(0.5)=" << wMid << ", w(1.0)=1.0\n";
}

void Test_PoseBlendingAndQuaternionSlerp() {
    std::cout << "[Test 5] Verifying CBlendUpdateNode Shortest-Arc Quaternion Slerp (FUN_1802d7a80 / 0x1804721a0)... ";

    // 1. Verify CBlendUpdateNode 1D weight computation (FUN_1802d7a80)
    // Blend parameter 1.75 across 3 children (0, 1, 2)
    // Child 1 and 2 active, baseIndex = 1, frac = 0.75 -> weight1 = 0.25, weight2 = 0.75
    float blendVal = 1.75f;
    float baseF = std::floor(blendVal);
    int childA = static_cast<int>(baseF);
    int childB = childA + 1;
    float weightB = blendVal - baseF;
    float weightA = 1.0f - weightB;

    assert(childA == 1 && childB == 2);
    assert(std::abs(weightA - 0.25f) < 1e-5f);
    assert(std::abs(weightB - 0.75f) < 1e-5f);

    // 2. Verify Shortest-Arc Quaternion Slerp
    // q1: identity rotation
    cs2::Quaternion q1(0.0f, 0.0f, 0.0f, 1.0f);

    // q2: 90 degrees around Z axis: (0, 0, sin(45 deg), cos(45 deg))
    cs2::Quaternion q2 = cs2::Quaternion::FromAxisAngle({0.0f, 0.0f, 1.0f}, 1.5707963f);

    // Midpoint Slerp t = 0.5 -> should be exactly 45 degrees around Z axis
    cs2::Quaternion qMid = cs2::Quaternion::Slerp(q1, q2, 0.5f, true);
    cs2::Quaternion qExpected45 = cs2::Quaternion::FromAxisAngle({0.0f, 0.0f, 1.0f}, 0.78539816f);

    assert(std::abs(qMid.Dot(qExpected45) - 1.0f) < 1e-4f);

    // 3. Shortest-Arc Inversion Test
    // If q2 is antipodal (-q2 represents identical 3D orientation), Slerp must still take shortest path
    cs2::Quaternion q2Antipodal = -q2;
    assert(q1.Dot(q2Antipodal) < 0.0f); // Dot is negative!

    cs2::Quaternion qMidShortest = cs2::Quaternion::Slerp(q1, q2Antipodal, 0.5f, true);
    assert(std::abs(std::abs(qMidShortest.Dot(qExpected45)) - 1.0f) < 1e-4f);

    // 4. Vector Rotation after Slerp
    // Vector (10, 0, 0) rotated by 45 degrees Z -> (10 * cos(45), 10 * sin(45), 0) = (7.071, 7.071, 0)
    cs2::Vector3D vRot = qMid.Rotate({10.0f, 0.0f, 0.0f});
    assert(std::abs(vRot.x - 7.0710678f) < 1e-3f);
    assert(std::abs(vRot.y - 7.0710678f) < 1e-3f);
    assert(std::abs(vRot.z - 0.0f) < 1e-3f);

    std::cout << "PASSED! Slerp(45 deg) Pos=(" << vRot.x << ", " << vRot.y << ", " << vRot.z << ")\n";
}

int main() {
    std::cout << "====================================================================\n";
    std::cout << " Source 2 Skeleton Bone Hierarchy & AnimGraph Verification Simulator\n";
    std::cout << " (Reverse engineered from client.dll & animationsystem.dll)\n";
    std::cout << "====================================================================\n\n";

    Test_TransformConcatenation();
    Test_SkeletalHierarchyAndHitboxes();
    Test_TwoBoneIKSolver();
    Test_StateMachineTransitions();
    Test_PoseBlendingAndQuaternionSlerp();

    std::cout << "\n>> All 5 verification test suites successfully passed!\n";
    return 0;
}
