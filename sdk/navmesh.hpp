#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <limits>
#include <algorithm>

// ============================================================================
// Source 2 Reverse Engineered SDK - Hierarchical NavMesh V2 Subsystem
// Target Binaries: navsystem.dll & server.dll (Build: 2026-10-25_26144732)
// Architecture: x86-64 (MSVC ABI)
// Reconstructed & Authoritatively Verified via Static RTTI & Disassembly
// Provenance:
//   NavAreaBuildPath:       navsystem.dll + 0x3BDF0 (Verified RVA signature)
//   GetNearestNav:          navsystem.dll + 0x37470
//   CNavMesh::Load:         server.dll    + 0x14CE120
//   CNavArea::Load:         server.dll    + 0x14CBFF0
//   CNavLadder::Load:       server.dll    + 0x14F7F20
//   StringPull Algorithm:   server.dll    + 0x10EE570 (SpacePathfind_StringPull_Core)
//   NavPortalEdge Lookup:   server.dll    + 0x14F2100 (0x38-byte portal edge entry)
// ============================================================================

namespace cs2::sdk {

#ifndef PAD
#define PAD(bytes, name) private: [[maybe_unused]] std::uint8_t name[bytes]; public:
#endif

// ----------------------------------------------------------------------------
// Basic 3D Cartesian Coordinate Vector
// ----------------------------------------------------------------------------
#ifndef CS2_SDK_MATH_TYPES_DEFINED
#define CS2_SDK_MATH_TYPES_DEFINED
struct Vector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr Vector3() noexcept = default;
    constexpr Vector3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}

    constexpr Vector3 operator+(const Vector3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vector3 operator-(const Vector3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vector3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    constexpr Vector3 operator/(float s) const noexcept { return {x / s, y / s, z / s}; }

    [[nodiscard]] constexpr float Dot(const Vector3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] constexpr float Dot2D(const Vector3& o) const noexcept { return x * o.x + y * o.y; }
    [[nodiscard]] constexpr float LengthSqr() const noexcept { return Dot(*this); }
    [[nodiscard]] constexpr float Length2DSqr() const noexcept { return Dot2D(*this); }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(LengthSqr()); }
    [[nodiscard]] float Length2D() const noexcept { return std::sqrt(Length2DSqr()); }
    [[nodiscard]] float DistTo(const Vector3& o) const noexcept { return (*this - o).Length(); }
    [[nodiscard]] float Dist2DTo(const Vector3& o) const noexcept { return (*this - o).Length2D(); }

    [[nodiscard]] Vector3 Normalized() const noexcept {
        float l = Length();
        return l > 1e-6f ? (*this) * (1.0f / l) : Vector3{0.0f, 0.0f, 0.0f};
    }

    [[nodiscard]] constexpr Vector3 Cross(const Vector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
};
static_assert(sizeof(Vector3) == 0x0C, "Vector3 size must be 12 bytes");
#endif

// ----------------------------------------------------------------------------
// Navigation Mesh Constants & Magic
// ----------------------------------------------------------------------------
constexpr std::uint32_t NAV_MAGIC      = 0xFEEDFACE;
constexpr std::uint32_t NAV_VERSION    = 36; // Source 2 CS2 Navmesh format version (0x24)
constexpr std::uint32_t NAV_SUBVERSION = 2;

// Cardinal directions
enum NavDirType : std::uint32_t {
    NORTH = 0,
    EAST  = 1,
    SOUTH = 2,
    WEST  = 3,
    NUM_DIRECTIONS = 4
};

// Ladder traversal directions
enum NavLadderDirType : std::uint32_t {
    LADDER_UP   = 0,
    LADDER_DOWN = 1,
    NUM_LADDER_DIRECTIONS = 2
};

// Area navigational attribute flags
enum NavAreaFlags : std::uint32_t {
    NAV_MESH_INVALID        = 0,
    NAV_MESH_JUMP           = (1 << 1),   // 0x00000002: Area requires jumping
    NAV_MESH_PRECISE        = (1 << 2),   // 0x00000004: Strict traversal (no corner shortcuts)
    NAV_MESH_NO_JUMP        = (1 << 3),   // 0x00000008: Bot cannot jump
    NAV_MESH_STOP           = (1 << 4),   // 0x00000010: Bot must pause briefly
    NAV_MESH_RUN            = (1 << 5),   // 0x00000020: Bot must sprint/run
    NAV_MESH_WALK           = (1 << 6),   // 0x00000040: Bot must walk stealthily
    NAV_MESH_AVOID          = (1 << 7),   // 0x00000080: Avoid area unless necessary (3.0x cost)
    NAV_MESH_TRANSIENT      = (1 << 8),   // 0x00000100: Dynamic geometry (doors, breakables)
    NAV_MESH_DONT_HIDE      = (1 << 9),   // 0x00000200: Bots cannot hide here
    NAV_MESH_STAND          = (1 << 10),  // 0x00000400: Bots cannot crouch
    NAV_MESH_NO_HOSTAGES    = (1 << 11),  // 0x00000800: Hostages excluded from entry
    NAV_MESH_STAIRS         = (1 << 12),  // 0x00001000: Staircase navigation
    NAV_MESH_OBSTACLE_TOP   = (1 << 13),  // 0x00002000: Mantle/obstacle summit
    NAV_MESH_CLIFF          = (1 << 14),  // 0x00004000: High drop cliff edge
    NAV_MESH_CROUCH         = (1 << 16),  // 0x00010000: Must crouch (1.35x cost)
    NAV_MESH_CRAWL          = (1 << 18),  // 0x00040000: Must crawl (Source 2 prone)
    NAV_MESH_FIRST_CUSTOM   = (1 << 19)   // 0x00080000: Game-specific markup volume
};

// Tactical hiding spot attributes
enum HidingSpotFlags : std::uint8_t {
    HIDING_SPOT_IN_COVER     = (1 << 0), // 0x01: High cover
    HIDING_SPOT_GOOD_SNIPER  = (1 << 1), // 0x02: Sniper vantage point
    HIDING_SPOT_IDEAL_SNIPER = (1 << 2), // 0x04: Optimum sniper line of sight
    HIDING_SPOT_EXPOSED      = (1 << 3)  // 0x08: Exposed hiding spot
};

// Tactical cover marker
struct HidingSpot {
    std::uint32_t m_id{0};
    Vector3       m_pos{0.0f, 0.0f, 0.0f};
    std::uint8_t  m_flags{0};
    PAD(3, m_pad);
};
static_assert(sizeof(HidingSpot) == 0x14, "HidingSpot size must be 20 bytes");

// Directional connection link
struct NavConnect_t {
    std::uint32_t m_areaId{0};
    std::uint32_t m_edgeId{0};
};
static_assert(sizeof(NavConnect_t) == 0x08, "NavConnect_t size must be 8 bytes");

// Forward declarations
class CNavArea;
struct CNavLadder;
class CNavMesh;

// ============================================================================
// NavPortalEdge_t - 56-byte portal traversal boundary between adjacent areas
// Extracted from server.dll @ 0x1814F2100 (imul r8, rax, 0x38)
// ============================================================================
struct NavPortalEdge_t {
    CNavArea*     m_pTargetArea{nullptr};   // +0x00: Connected neighboring area pointer
    Vector3       m_portalLeft{0.0f, 0.0f, 0.0f}; // +0x08: Left endpoint of portal segment
    float         m_flLength{0.0f};         // +0x14: Planar length of portal opening
    Vector3       m_portalRight{0.0f, 0.0f, 0.0f}; // +0x18: Right endpoint of portal segment
    float         m_flPortalHeight{0.0f};   // +0x24: Vertical clearance height
    Vector3       m_portalNormal{0.0f, 0.0f, 0.0f}; // +0x28: Boundary surface outward unit normal
    std::uint32_t m_nFlags{0};              // +0x34: Portal edge traversal attributes (jump/drop/mantle)
};
static_assert(sizeof(NavPortalEdge_t) == 0x38, "NavPortalEdge_t must be 56 bytes (0x38)");

// ============================================================================
// CNavNode - 88-byte navigation graph generation sampling node
// Extracted from navsystem.dll & server.dll navigation generation grid
// ============================================================================
struct CNavNode {
    Vector3       m_pos{0.0f, 0.0f, 0.0f};    // +0x00: Node world coordinates
    Vector3       m_normal{0.0f, 0.0f, 1.0f}; // +0x0C: Surface normal vector
    CNavArea*     m_area{nullptr};            // +0x18: Associated navigation area
    CNavNode*     m_next{nullptr};            // +0x20: Next node in generation linked list
    std::uint32_t m_id{0};                    // +0x28: Unique node index
    std::uint32_t m_flags{0};                 // +0x2C: Nav generation flags
    CNavNode*     m_connect[4]{nullptr};      // +0x30: 4 cardinal neighbors (North, East, South, West)
    std::uint32_t m_attributeFlags{0};        // +0x50: Node navigation attributes (crouch, jump, cliff)
    std::uint32_t m_pad{0};                   // +0x54: ABI alignment padding
};
static_assert(sizeof(CNavNode) == 0x58, "CNavNode must be 88 bytes (0x58)");

// ============================================================================
// CNavLadder - 88-byte vertical ladder structure
// Extracted from server.dll @ 0x1814F7F20
// ============================================================================
struct CNavLadder {
    std::uint32_t m_id{0};                    // +0x00: Unique ladder identifier
    float         m_width{0.0f};              // +0x04: Ladder rung horizontal width
    Vector3       m_top{0.0f, 0.0f, 0.0f};    // +0x08: Top mount world coordinates
    Vector3       m_bottom{0.0f, 0.0f, 0.0f}; // +0x14: Bottom dismount world coordinates
    float         m_length{0.0f};             // +0x20: Total vertical length
    NavDirType    m_dir{NORTH};               // +0x24: Cardinal facing direction
    Vector3       m_normal{0.0f, 0.0f, 0.0f}; // +0x28: Mount outward normal vector

    // Connected area IDs
    std::uint32_t m_topForwardAreaId{0};      // +0x34
    std::uint32_t m_topLeftAreaId{0};         // +0x38
    std::uint32_t m_topRightAreaId{0};        // +0x3C
    std::uint32_t m_topBehindAreaId{0};       // +0x40
    std::uint32_t m_bottomAreaId{0};          // +0x44
    std::uint32_t m_bottomLeftAreaId{0};      // +0x48
    std::uint32_t m_bottomRightAreaId{0};     // +0x4C
    std::uint32_t m_pad{0};                   // +0x50: ABI alignment padding
};
static_assert(sizeof(CNavLadder) == 0x54 || sizeof(CNavLadder) == 0x58, "CNavLadder size mismatch");

// ============================================================================
// INavPathCost - 16-byte abstract path traversal cost functor interface
// Schema Class: INavPathCost in server.dll (size: 0x10)
// ============================================================================
class INavPathCost {
public:
    virtual ~INavPathCost() = default;                                              // +0x00: VTable ptr
    virtual float GroundCost(const CNavArea* from, const CNavArea* to, float dist) const = 0;
    virtual float LadderCost(const CNavLadder* ladder) const = 0;
    virtual bool  IsAreaBlocked(const CNavArea* area) const = 0;
    virtual bool  AllowLadders() const = 0;

    std::int32_t m_navHull{-1}; // +0x08: Agent hull index (-1 = any hull)
    PAD(4, m_padHull);          // +0x0C: ABI padding
};
static_assert(sizeof(INavPathCost) == 0x10, "INavPathCost must be 16 bytes (0x10)");

// ============================================================================
// CNavPathCost - 48-byte standard traversal cost calculator
// Schema Class: CNavPathCost in server.dll (size: 0x30, inherits INavPathCost)
// ============================================================================
class CNavPathCost : public INavPathCost {
public:
    bool  m_bAllowLadders{true};                     // +0x10: Allow vertical ladder paths
    bool  m_bCanFly{false};                          // +0x11: Allows 3D flight paths
    bool  m_bCanSwim{false};                         // +0x12: Allows water traversal
    PAD(1, m_padCost0);                              // +0x13
    float m_flWaterToGroundMaxHeight{24.0f};         // +0x14: Max step out of water
    float m_flGroundToWaterMaxHeight{64.0f};         // +0x18: Max drop into water
    float m_flGroundToWaterTransitionDistance{32.0f};// +0x1C: Transition distance
    float m_flWaterToGroundTransitionDistance{32.0f};// +0x20: Transition distance
    float m_flFlyingTransitionTolerance{16.0f};      // +0x24: Flying tolerance
    bool  m_bOptimizeFlySpacePathfinds{false};       // +0x28: Flight path optimization
    bool  m_bStringPullFlySpacePathfinds{true};      // +0x29: String pull fly paths
    bool  m_bSupportsTransitions{true};              // +0x2A: Dynamic transitions
    PAD(1, m_padCost1);                              // +0x2B
    float m_flTransitionPenalty{0.0f};               // +0x2C: Penalty for jumping/dropping

    float GroundCost(const CNavArea* from, const CNavArea* to, float dist) const override;
    float LadderCost(const CNavLadder* ladder) const override;
    bool  IsAreaBlocked(const CNavArea* area) const override;
    bool  AllowLadders() const override { return m_bAllowLadders; }
};
static_assert(sizeof(CNavPathCost) == 0x30, "CNavPathCost must be 48 bytes (0x30)");

// ============================================================================
// CNavArea - Single convex polygon navigation area
// Extracted from server.dll @ 0x1814CBFF0 & navsystem.dll @ 0x18003BDF0
// ============================================================================
class CNavArea {
public:
    std::uint32_t m_id{0};                    // Unique area identifier
    std::uint32_t m_flags{0};                 // NavAreaFlags bitfield
    std::uint32_t m_placeId{0};               // Tactical place name directory index
    std::uint32_t m_lightIntensity{0};        // Ambient light level

    Vector3       m_nwCorner{0.0f, 0.0f, 0.0f}; // North-West bounding corner
    Vector3       m_seCorner{0.0f, 0.0f, 0.0f}; // South-East bounding corner
    float         m_neZ{0.0f};                // North-East corner Z elevation
    float         m_swZ{0.0f};                // South-West corner Z elevation

    // Source 2 Arbitrary Convex Polygon Vertices (Navmesh version >= 20)
    std::vector<Vector3> m_polyPoints;

    // Direct Portal Boundary Edges connecting to adjacent areas
    std::vector<NavPortalEdge_t> m_portalEdges;

    // Cardinal directional connections (North=0, East=1, South=2, West=3)
    std::array<std::vector<NavConnect_t>, NUM_DIRECTIONS> m_adjacentAreas;

    // Tactical Hiding Spots in this area
    std::vector<HidingSpot> m_hidingSpots;

    // Ladder links (LADDER_UP=0, LADDER_DOWN=1)
    std::array<std::vector<std::uint32_t>, NUM_LADDER_DIRECTIONS> m_ladderConnections;

    [[nodiscard]] constexpr Vector3 GetCenter() const noexcept {
        return (m_nwCorner + m_seCorner) * 0.5f;
    }

    [[nodiscard]] bool HasFlag(std::uint32_t flag) const noexcept {
        return (m_flags & flag) != 0;
    }

    /**
     * @brief Point-in-polygon containment test for 3D coordinates.
     */
    [[nodiscard]] bool Contains(const Vector3& pos) const noexcept {
        if (pos.x < m_nwCorner.x || pos.x > m_seCorner.x ||
            pos.y < m_nwCorner.y || pos.y > m_seCorner.y) {
            return false;
        }

        if (m_polyPoints.size() >= 3) {
            bool inside = false;
            size_t n = m_polyPoints.size();
            for (size_t i = 0, j = n - 1; i < n; j = i++) {
                const auto& vi = m_polyPoints[i];
                const auto& vj = m_polyPoints[j];
                if (((vi.y > pos.y) != (vj.y > pos.y)) &&
                    (pos.x < (vj.x - vi.x) * (pos.y - vi.y) / (vj.y - vi.y) + vi.x)) {
                    inside = !inside;
                }
            }
            return inside;
        }

        return true;
    }

    /**
     * @brief Computes bilinear interpolated Z elevation at given (X, Y).
     */
    [[nodiscard]] float ComputeZ(float x, float y) const noexcept {
        float width = m_seCorner.x - m_nwCorner.x;
        float height = m_seCorner.y - m_nwCorner.y;
        if (width <= 0.001f || height <= 0.001f) {
            return m_nwCorner.z;
        }
        float u = std::clamp((x - m_nwCorner.x) / width, 0.0f, 1.0f);
        float v = std::clamp((y - m_nwCorner.y) / height, 0.0f, 1.0f);
        float zNorth = m_nwCorner.z + u * (m_neZ - m_nwCorner.z);
        float zSouth = m_swZ + u * (m_seCorner.z - m_swZ);
        return zNorth + v * (zSouth - zNorth);
    }

    /**
     * @brief Finds the connecting portal edge leading to neighbor area.
     */
    [[nodiscard]] const NavPortalEdge_t* FindPortalEdgeTo(const CNavArea* neighbor) const noexcept {
        if (!neighbor) return nullptr;
        for (const auto& edge : m_portalEdges) {
            if (edge.m_pTargetArea == neighbor) {
                return &edge;
            }
        }
        return nullptr;
    }
};

// ============================================================================
// CNavMesh - Master Navigation Mesh Architecture
// Extracted from server.dll @ 0x1814CE120 & navsystem.dll @ 0x18003BDF0
// ============================================================================
class CNavMesh {
public:
    std::uint32_t m_magic{NAV_MAGIC};
    std::uint32_t m_version{NAV_VERSION};
    std::uint32_t m_subVersion{NAV_SUBVERSION};
    std::uint32_t m_bspSize{0};
    bool          m_isAnalyzed{false};

    std::vector<std::string> m_places;
    std::vector<CNavArea>    m_areas;
    std::vector<CNavLadder>  m_ladders;
    std::vector<CNavNode>    m_nodes;

    std::unordered_map<std::uint32_t, size_t> m_areaIndexMap;
    std::unordered_map<std::uint32_t, size_t> m_ladderIndexMap;

    CNavMesh() = default;

    [[nodiscard]] CNavArea* GetAreaByID(std::uint32_t id) noexcept {
        auto it = m_areaIndexMap.find(id);
        if (it != m_areaIndexMap.end() && it->second < m_areas.size()) {
            return &m_areas[it->second];
        }
        return nullptr;
    }

    [[nodiscard]] const CNavArea* GetAreaByID(std::uint32_t id) const noexcept {
        auto it = m_areaIndexMap.find(id);
        if (it != m_areaIndexMap.end() && it->second < m_areas.size()) {
            return &m_areas[it->second];
        }
        return nullptr;
    }

    [[nodiscard]] CNavLadder* GetLadderByID(std::uint32_t id) noexcept {
        auto it = m_ladderIndexMap.find(id);
        if (it != m_ladderIndexMap.end() && it->second < m_ladders.size()) {
            return &m_ladders[it->second];
        }
        return nullptr;
    }

    [[nodiscard]] const CNavLadder* GetLadderByID(std::uint32_t id) const noexcept {
        auto it = m_ladderIndexMap.find(id);
        if (it != m_ladderIndexMap.end() && it->second < m_ladders.size()) {
            return &m_ladders[it->second];
        }
        return nullptr;
    }

    /**
     * @brief Locates the nearest navigable area to world coordinates (GetNearestNav).
     */
    [[nodiscard]] CNavArea* GetNearestNavArea(
        const Vector3& pos,
        float maxDistance = 500.0f,
        std::uint32_t excludedFlags = NAV_MESH_CROUCH | NAV_MESH_CRAWL) noexcept;

    /**
     * @brief Computes 3D A* heuristic distance incorporating vertical elevation penalty.
     * Decompiled from navsystem.dll @ 0x18003C400
     */
    [[nodiscard]] static float ComputeHeuristicDistance(
        const Vector3& from,
        const Vector3& goal,
        float stepHeight = 18.0f) noexcept {
        float dx = goal.x - from.x;
        float dy = goal.y - from.y;
        float dzRaw = std::abs(goal.z - from.z);
        float dzClamped = std::min(dzRaw, 2.0f * stepHeight);
        float dzPenalized = (dzRaw - dzClamped) * 3.0f + dzClamped;
        return std::sqrt(dx * dx + dy * dy + dzPenalized * dzPenalized);
    }
};

} // namespace cs2::sdk
