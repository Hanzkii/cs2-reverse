#ifndef CS2_NAVSYSTEM_NAVMESH_H
#define CS2_NAVSYSTEM_NAVMESH_H

#include <cstdint>
#include <cmath>
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <queue>
#include <unordered_map>
#include <limits>
#include <algorithm>

namespace cs2::navsystem {

// Source 2 Navigation Mesh Magic Identifier (0xFEEDFACE)
constexpr uint32_t NAV_MAGIC = 0xFEEDFACE;
constexpr uint32_t NAV_VERSION = 36; // Maximum supported in CS2 (0x24)
constexpr uint32_t NAV_SUBVERSION = 2;

// Cardinal traversal directions
enum NavDirType : uint32_t {
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3,
    NUM_DIRECTIONS = 4
};

// Ladder traversal directions
enum NavLadderDirType : uint32_t {
    LADDER_UP = 0,
    LADDER_DOWN = 1,
    NUM_LADDER_DIRECTIONS = 2
};

// Area navigational attribute flags
enum NavAreaFlags : uint32_t {
    NAV_MESH_INVALID        = 0,
    NAV_MESH_JUMP           = (1 << 1),   // 0x00000002: Area requires jump
    NAV_MESH_PRECISE        = (1 << 2),   // 0x00000004: Precise positioning (no shortcutting)
    NAV_MESH_NO_JUMP        = (1 << 3),   // 0x00000008: Inhibits bot jumping
    NAV_MESH_STOP           = (1 << 4),   // 0x00000010: Bot must stop briefly
    NAV_MESH_RUN            = (1 << 5),   // 0x00000020: Bot must run through
    NAV_MESH_WALK           = (1 << 6),   // 0x00000040: Bot must walk through
    NAV_MESH_AVOID          = (1 << 7),   // 0x00000080: Avoid area unless necessary
    NAV_MESH_TRANSIENT      = (1 << 8),   // 0x00000100: Dynamic/changing state (doors, breakables)
    NAV_MESH_DONT_HIDE      = (1 << 9),   // 0x00000200: Bots cannot hide here
    NAV_MESH_STAND          = (1 << 10),  // 0x00000400: Bots must stand (cannot crouch)
    NAV_MESH_NO_HOSTAGES    = (1 << 11),  // 0x00000800: Hostages excluded from entry
    NAV_MESH_STAIRS         = (1 << 12),  // 0x00001000: Staircase navigation
    NAV_MESH_OBSTACLE_TOP   = (1 << 13),  // 0x00002000: Mantle/obstacle top
    NAV_MESH_CLIFF          = (1 << 14),  // 0x00004000: Edge cliff drop-off
    NAV_MESH_CROUCH         = (1 << 16),  // 0x00010000: Must crouch
    NAV_MESH_CRAWL          = (1 << 18),  // 0x00040000: Must crawl (Source 2 addition)
    NAV_MESH_FIRST_CUSTOM   = (1 << 19)   // 0x00080000: Game-specific markup boundary
};

// Tactical hiding spot attributes
enum HidingSpotFlags : uint8_t {
    HIDING_SPOT_IN_COVER      = (1 << 0), // 0x01: High cover
    HIDING_SPOT_GOOD_SNIPER   = (1 << 1), // 0x02: Sniper vantage point
    HIDING_SPOT_IDEAL_SNIPER  = (1 << 2), // 0x04: Optimum sniper line of sight
    HIDING_SPOT_EXPOSED       = (1 << 3)  // 0x08: Exposed hiding spot
};

// 3D Cartesian Coordinate Vector
struct Vector3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    constexpr Vector3() noexcept = default;
    constexpr Vector3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}

    Vector3 operator+(const Vector3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    Vector3 operator/(float s) const noexcept { return {x / s, y / s, z / s}; }

    [[nodiscard]] float Dot(const Vector3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] float LengthSqr() const noexcept { return Dot(*this); }
    [[nodiscard]] float Length() const noexcept { return std::sqrt(LengthSqr()); }
    [[nodiscard]] float DistTo(const Vector3& o) const noexcept { return (*this - o).Length(); }

    [[nodiscard]] Vector3 Normalized() const noexcept {
        float l = Length();
        return l > 1e-6f ? (*this) * (1.0f / l) : Vector3{0.0f, 0.0f, 0.0f};
    }
};

// Tactical cover marker within a CNavArea
struct HidingSpot {
    uint32_t m_id{0};
    Vector3  m_pos{0.0f, 0.0f, 0.0f};
    uint8_t  m_flags{0};
};

// Directional connection from one area to another
struct NavConnect_t {
    uint32_t m_areaId{0};
    uint32_t m_edgeId{0};
};

// Ladder structure in navigation mesh
struct CNavLadder {
    uint32_t m_id{0};
    float    m_width{0.0f};
    Vector3  m_top{0.0f, 0.0f, 0.0f};
    Vector3  m_bottom{0.0f, 0.0f, 0.0f};
    float    m_length{0.0f};
    NavDirType m_dir{NORTH};
    Vector3  m_normal{0.0f, 0.0f, 0.0f};

    // Connected area IDs
    uint32_t m_topForwardAreaId{0};
    uint32_t m_topLeftAreaId{0};
    uint32_t m_topRightAreaId{0};
    uint32_t m_topBehindAreaId{0};
    uint32_t m_bottomAreaId{0};
    uint32_t m_bottomLeftAreaId{0};
    uint32_t m_bottomRightAreaId{0};
};

// Single convex polygon region in the navigation mesh
class CNavArea {
public:
    uint32_t m_id{0};
    uint32_t m_flags{0};
    uint8_t  m_lightIntensity{0};
    Vector3  m_nwCorner{0.0f, 0.0f, 0.0f};
    Vector3  m_seCorner{0.0f, 0.0f, 0.0f};
    float    m_neZ{0.0f};
    float    m_swZ{0.0f};
    uint32_t m_placeId{0};

    // Source 2 arbitrary polygon vertices (when present)
    std::vector<Vector3> m_polyPoints;

    // Outgoing directional connections (North, East, South, West)
    std::array<std::vector<NavConnect_t>, NUM_DIRECTIONS> m_adjacentAreas;

    // Tactical hiding spots located in this area
    std::vector<HidingSpot> m_hidingSpots;

    // Ladder references (LADDER_UP, LADDER_DOWN)
    std::array<std::vector<uint32_t>, NUM_LADDER_DIRECTIONS> m_ladderConnections;

    [[nodiscard]] Vector3 GetCenter() const noexcept {
        return (m_nwCorner + m_seCorner) * 0.5f;
    }

    [[nodiscard]] bool Contains(const Vector3& pos) const noexcept;
    [[nodiscard]] float ComputeZ(float x, float y) const noexcept;
    [[nodiscard]] bool HasFlag(uint32_t flag) const noexcept { return (m_flags & flag) != 0; }
};

// Abstract path traversal cost functor interface (INavPathCost)
class INavPathCost {
public:
    virtual ~INavPathCost() = default;
    virtual float GroundCost(const CNavArea* from, const CNavArea* to, float dist) const = 0;
    virtual float LadderCost(const CNavLadder* ladder) const = 0;
    virtual bool  IsAreaBlocked(const CNavArea* area) const = 0;
    virtual bool  AllowLadders() const = 0;

    int32_t m_navHull{-1}; // Compatible agent hull index (-1 = any)
};

// Reconstructed clean-room CNavPathCost implementation
class CNavPathCost : public INavPathCost {
public:
    bool  m_bAllowLadders{true};
    bool  m_bCanFly{false};
    bool  m_bCanSwim{false};
    float m_flTransitionPenalty{0.0f};

    float GroundCost(const CNavArea* from, const CNavArea* to, float dist) const override;
    float LadderCost(const CNavLadder* ladder) const override;
    bool  IsAreaBlocked(const CNavArea* area) const override;
    bool  AllowLadders() const override { return m_bAllowLadders; }
};

// Internal A* Search OpenList Priority Queue Node
struct AStarNode {
    CNavArea*         m_area{nullptr};
    AStarNode*        m_parent{nullptr};
    const CNavLadder* m_ladder{nullptr};
    float             m_gCost{0.0f};      // Cost from start to current node
    float             m_hCost{0.0f};      // Heuristic estimate to goal
    float             m_totalCost{0.0f};  // f = g + h
    bool              m_open{false};
    bool              m_closed{false};

    struct Compare {
        bool operator()(const AStarNode* a, const AStarNode* b) const noexcept {
            return a->m_totalCost > b->m_totalCost; // Min-heap
        }
    };
};

// Core Navigation Mesh Architecture and A* Pathfinding Engine
class CNavMesh {
public:
    uint32_t m_magic{0};
    uint32_t m_version{0};
    uint32_t m_subVersion{0};
    uint32_t m_bspSize{0};
    bool     m_isAnalyzed{false};

    std::vector<std::string> m_places;
    std::vector<CNavArea>    m_areas;
    std::vector<CNavLadder>  m_ladders;

    std::unordered_map<uint32_t, size_t> m_areaIndexMap;
    std::unordered_map<uint32_t, size_t> m_ladderIndexMap;

    CNavMesh() = default;

    /**
     * @brief Parses a binary .nav file buffer matching Source 2 specifications.
     */
    bool LoadFromBuffer(const uint8_t* data, size_t dataSize);

    /**
     * @brief Retrieves a CNavArea pointer by unique ID.
     */
    [[nodiscard]] CNavArea* GetAreaByID(uint32_t id) noexcept;
    [[nodiscard]] const CNavArea* GetAreaByID(uint32_t id) const noexcept;

    /**
     * @brief Retrieves a CNavLadder pointer by unique ID.
     */
    [[nodiscard]] CNavLadder* GetLadderByID(uint32_t id) noexcept;
    [[nodiscard]] const CNavLadder* GetLadderByID(uint32_t id) const noexcept;

    /**
     * @brief Locates the nearest navigable area to world coordinates (GetNearestNav).
     */
    [[nodiscard]] CNavArea* GetNearestNavArea(
        const Vector3& pos,
        float maxDistance = 500.0f,
        uint32_t excludedFlags = NAV_MESH_CROUCH | NAV_MESH_CRAWL) noexcept;

    /**
     * @brief Executes A* pathfinding search heuristic (NavAreaBuildPath) from start to goal.
     * 
     * Incorporates Source 2 elevation penalties (3.0x multiplier on vertical climb)
     * and admissible/inadmissible heuristic factors.
     */
    bool FindPath(
        const Vector3& startPos,
        const Vector3& goalPos,
        CNavArea* startArea,
        CNavArea* goalArea,
        const INavPathCost& costFunc,
        std::vector<Vector3>& outPathWaypoints,
        float maxPathLength = 10000.0f,
        float inadmissibleHeuristicFactor = 1.0f);

    /**
     * @brief Computes 3D A* heuristic distance incorporating vertical elevation penalty.
     */
    static float ComputeHeuristicDistance(
        const Vector3& from,
        const Vector3& goal,
        float stepHeight = 18.0f) noexcept;
};

} // namespace cs2::navsystem

#endif // CS2_NAVSYSTEM_NAVMESH_H
