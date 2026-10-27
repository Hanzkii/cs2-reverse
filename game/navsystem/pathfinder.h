#pragma once

#include "sdk/navmesh.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <cmath>
#include <algorithm>
#include <cstdint>

// ============================================================================
// Source 2 Clean-Room A* Pathfinding Engine & Corridor Funnel String-Pulling
// Target Binaries: navsystem.dll & server.dll
// Architecture: x86-64 (MSVC ABI, /std:c++20 /W4 /WX)
// Provenance:
//   NavAreaBuildPath:               navsystem.dll @ 0x18003BDF0
//   Vertical Elevation Heuristic:   navsystem.dll @ 0x18003C400 (3.0x climb penalty)
//   CNavPathCost Traversal:         navsystem.dll @ 0x18003B420 & server.dll schemas
//   SpacePathfind_StringPull_Core:  server.dll    @ 0x1810EE570
//   Portal Traversal Intersect:     server.dll    @ 0x1814F2100 (0x38-byte NavPortalEdge_t)
// ============================================================================

namespace cs2::navsystem {

// Traversal movement taxonomy for path waypoints
enum class NavWaypointType : std::uint32_t {
    Ground      = 0,
    Jump        = 1,
    LadderUp    = 2,
    LadderDown  = 3,
    Crouch      = 4,
    DropCliff   = 5,
    Obstacle    = 6
};

// Annotated path waypoint along optimized trajectory
struct NavWaypoint {
    sdk::Vector3    m_position{0.0f, 0.0f, 0.0f};
    std::uint32_t   m_areaId{0};
    std::uint32_t   m_flags{0};
    NavWaypointType m_type{NavWaypointType::Ground};
    float           m_flCurvature{0.0f};

    constexpr NavWaypoint() noexcept = default;
    constexpr NavWaypoint(const sdk::Vector3& pos, std::uint32_t areaId, std::uint32_t flags, NavWaypointType type) noexcept
        : m_position(pos), m_areaId(areaId), m_flags(flags), m_type(type) {}
};

// Portal gate spanning the shared boundary between adjacent convex areas
struct NavPortal {
    sdk::Vector3         m_left{0.0f, 0.0f, 0.0f};   // Portal left endpoint (from traversal perspective)
    sdk::Vector3         m_right{0.0f, 0.0f, 0.0f};  // Portal right endpoint (from traversal perspective)
    std::uint32_t        m_fromAreaId{0};
    std::uint32_t        m_toAreaId{0};
    sdk::NavPortalEdge_t m_edge{};

    constexpr NavPortal() noexcept = default;
    constexpr NavPortal(const sdk::Vector3& l, const sdk::Vector3& r, std::uint32_t fromId, std::uint32_t toId, const sdk::NavPortalEdge_t& e) noexcept
        : m_left(l), m_right(r), m_fromAreaId(fromId), m_toAreaId(toId), m_edge(e) {}
};

// Algorithmic query tuning options
struct PathOptions {
    float         m_flMaxPathLength{15000.0f};                  // Max traversal distance cutoff
    float         m_flInadmissibleHeuristicFactor{1.0f};         // nav_pathfind_inadmissable_heuristic_factor
    float         m_flAgentRadius{16.0f};                       // Collision hull radius clearance (portal margin)
    float         m_flStepHeight{18.0f};                        // Standard step height threshold
    bool          m_bAllowLadders{true};                        // Enable vertical ladders
    bool          m_bAllowJump{true};                           // Enable jump links
    bool          m_bAllowCrouch{true};                         // Enable crouch/crawl areas
    bool          m_bStringPullCorridor{true};                  // Run Simple Stupid Funnel Algorithm
    std::uint32_t m_nExcludedFlags{sdk::NAV_MESH_INVALID};      // Excluded area bitmask
};

// Pathfinding query outcome container
struct PathResult {
    enum class EStatus : std::uint32_t {
        Success               = 0,
        Failed_NoStartArea    = 1,
        Failed_NoGoalArea     = 2,
        Failed_NoPath         = 3,
        Failed_LengthExceeded = 4
    };

    EStatus                       m_status{EStatus::Failed_NoPath};
    std::vector<sdk::CNavArea*>   m_areaCorridor;       // Chronological area channel sequence
    std::vector<NavPortal>        m_portals;            // Geometric portals connecting adjacent areas
    std::vector<sdk::Vector3>     m_rawWaypoints;       // Polygon center / portal edge midpoints
    std::vector<sdk::Vector3>     m_smoothedWaypoints;  // Funnel string-pulled piecewise-linear trajectory
    std::vector<NavWaypoint>      m_annotatedWaypoints; // Movement metadata annotations
    float                         m_totalCost{0.0f};    // Accumulated A* traversal cost
    float                         m_totalLength{0.0f};  // Integrated 3D Euclidean trajectory length
    std::size_t                   m_nodesExplored{0};   // Open/closed node expansion count

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return m_status == EStatus::Success && !m_smoothedWaypoints.empty();
    }
};

// Internal A* Search OpenList Priority Queue Entry
struct PathfinderAStarNode {
    sdk::CNavArea*              m_area{nullptr};
    PathfinderAStarNode*        m_parent{nullptr};
    const sdk::CNavLadder*      m_ladder{nullptr};
    float                       m_gCost{0.0f};
    float                       m_hCost{0.0f};
    float                       m_totalCost{0.0f}; // f(n) = g(n) + alpha * h(n)
    bool                        m_open{false};
    bool                        m_closed{false};

    struct MinHeapCompare {
        bool operator()(const PathfinderAStarNode* a, const PathfinderAStarNode* b) const noexcept {
            return a->m_totalCost > b->m_totalCost;
        }
    };
};

// ============================================================================
// CPathfinder - Core Graph Pathfinding and Trajectory Optimization Engine
// ============================================================================
class CPathfinder {
public:
    explicit CPathfinder(const sdk::CNavMesh* pNavMesh) noexcept : m_pNavMesh(pNavMesh) {}

    /**
     * @brief Computes the shortest and smoothest navigable trajectory from start to goal.
     */
    [[nodiscard]] PathResult FindPath(
        const sdk::Vector3& startPos,
        const sdk::Vector3& goalPos,
        const PathOptions& options = {},
        const sdk::INavPathCost* customCost = nullptr) const;

    /**
     * @brief Executes A* graph search over convex navigation area graph.
     * Decompiled from navsystem.dll @ 0x18003BDF0 (NavAreaBuildPath)
     */
    bool SearchAStar(
        sdk::CNavArea* startArea,
        sdk::CNavArea* goalArea,
        const sdk::Vector3& startPos,
        const sdk::Vector3& goalPos,
        const PathOptions& options,
        const sdk::INavPathCost& costFunc,
        std::vector<sdk::CNavArea*>& outCorridor,
        float& outTotalCost,
        std::size_t& outNodesExplored) const;

    /**
     * @brief Extracts ordered portal segments between consecutive corridor areas.
     * Decompiled from server.dll @ 0x1814F2100
     */
    bool ExtractPortals(
        const std::vector<sdk::CNavArea*>& corridor,
        const sdk::Vector3& startPos,
        const sdk::Vector3& goalPos,
        std::vector<NavPortal>& outPortals) const;

    /**
     * @brief Applies Simple Stupid Funnel Algorithm (SSFA) / String-Pulling to straighten path.
     * Decompiled from server.dll @ 0x1810EE570 (SpacePathfind_StringPull_Core)
     */
    static bool StringPullCorridor(
        const sdk::Vector3& startPos,
        const sdk::Vector3& goalPos,
        const std::vector<NavPortal>& portals,
        const std::vector<sdk::CNavArea*>& corridor,
        float agentRadius,
        std::vector<sdk::Vector3>& outSmoothedWaypoints);

    /**
     * @brief Signed 2D cross product determining triangle orientation (CCW > 0, CW < 0).
     */
    [[nodiscard]] static constexpr float TriArea2D(
        const sdk::Vector3& a,
        const sdk::Vector3& b,
        const sdk::Vector3& c) noexcept {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }

    /**
     * @brief Finds closest point on 3D segment AB to point P.
     */
    [[nodiscard]] static sdk::Vector3 ClosestPointOnSegment(
        const sdk::Vector3& p,
        const sdk::Vector3& a,
        const sdk::Vector3& b) noexcept;

    /**
     * @brief Computes 3D A* heuristic with 3.0x vertical climb penalty above step clearance.
     * Decompiled from navsystem.dll @ 0x18003C400
     */
    [[nodiscard]] static float ComputeHeuristic(
        const sdk::Vector3& from,
        const sdk::Vector3& goal,
        float stepHeight = 18.0f) noexcept;

private:
    const sdk::CNavMesh* m_pNavMesh{nullptr};
};

} // namespace cs2::navsystem
