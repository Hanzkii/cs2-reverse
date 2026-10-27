#include "pathfinder.h"
#include <cmath>
#include <algorithm>
#include <memory>

namespace cs2::sdk {

// ============================================================================
// INavPathCost / CNavPathCost Method Implementations
// ============================================================================
float CNavPathCost::GroundCost(const CNavArea* from, const CNavArea* to, float dist) const {
    if (!to || IsAreaBlocked(to)) {
        return -1.0f;
    }

    float cost = dist;

    // Movement stance modifiers
    if (to->HasFlag(NAV_MESH_CROUCH)) {
        cost *= 1.35f;
    }
    if (to->HasFlag(NAV_MESH_CRAWL)) {
        cost *= 2.0f;
    }
    if (to->HasFlag(NAV_MESH_JUMP)) {
        cost += 50.0f + m_flTransitionPenalty;
    }
    if (to->HasFlag(NAV_MESH_AVOID)) {
        cost *= 3.0f;
    }

    // Elevation climb penalty
    if (from) {
        float dz = to->GetCenter().z - from->GetCenter().z;
        if (dz > 18.0f) {
            cost += dz * 2.0f; // Extra effort for climbing
        }
    }

    return cost;
}

float CNavPathCost::LadderCost(const CNavLadder* ladder) const {
    if (!m_bAllowLadders || !ladder) {
        return -1.0f;
    }
    return ladder->m_length * 1.5f;
}

bool CNavPathCost::IsAreaBlocked(const CNavArea* area) const {
    if (!area) return true;
    return area->HasFlag(NAV_MESH_INVALID);
}

// ============================================================================
// CNavMesh Helper Query Implementations
// ============================================================================
CNavArea* CNavMesh::GetNearestNavArea(
    const Vector3& pos,
    float maxDistance,
    std::uint32_t excludedFlags) noexcept {
    CNavArea* bestArea = nullptr;
    float bestDistSqr = maxDistance * maxDistance;

    for (auto& area : m_areas) {
        if (excludedFlags != 0 && area.HasFlag(excludedFlags)) {
            continue;
        }

        Vector3 center = area.GetCenter();
        float distSqr = (center - pos).LengthSqr();

        // Check if inside polygon/bounding box
        if (area.Contains(pos)) {
            float z = area.ComputeZ(pos.x, pos.y);
            float dz = std::abs(pos.z - z);
            if (dz < 64.0f) {
                return &area; // Direct vertical match within step/jump tolerance
            }
        }

        if (distSqr < bestDistSqr) {
            bestDistSqr = distSqr;
            bestArea = &area;
        }
    }

    return bestArea;
}

} // namespace cs2::sdk

namespace cs2::navsystem {

using namespace cs2::sdk;

// ============================================================================
// CPathfinder Static Geometric Primitives
// ============================================================================
Vector3 CPathfinder::ClosestPointOnSegment(
    const Vector3& p,
    const Vector3& a,
    const Vector3& b) noexcept {
    Vector3 ab = b - a;
    float lenSqr = ab.LengthSqr();
    if (lenSqr < 1e-6f) {
        return a;
    }

    float t = (p - a).Dot(ab) / lenSqr;
    t = std::clamp(t, 0.0f, 1.0f);
    return a + ab * t;
}

float CPathfinder::ComputeHeuristic(
    const Vector3& from,
    const Vector3& goal,
    float stepHeight) noexcept {
    float dx = goal.x - from.x;
    float dy = goal.y - from.y;
    float dzRaw = std::abs(goal.z - from.z);
    float dzClamped = std::min(dzRaw, 2.0f * stepHeight);
    float dzPenalized = (dzRaw - dzClamped) * 3.0f + dzClamped;
    return std::sqrt(dx * dx + dy * dy + dzPenalized * dzPenalized);
}

// ============================================================================
// CPathfinder Main Pipeline Execution
// ============================================================================
PathResult CPathfinder::FindPath(
    const Vector3& startPos,
    const Vector3& goalPos,
    const PathOptions& options,
    const INavPathCost* customCost) const {
    PathResult result;
    if (!m_pNavMesh) {
        result.m_status = PathResult::EStatus::Failed_NoPath;
        return result;
    }

    // Default cost functor if none specified
    CNavPathCost defaultCost;
    defaultCost.m_bAllowLadders = options.m_bAllowLadders;
    const INavPathCost& costFunc = customCost ? *customCost : defaultCost;

    // 1. Locate start and goal areas
    CNavArea* startArea = const_cast<CNavMesh*>(m_pNavMesh)->GetNearestNavArea(
        startPos, 600.0f, options.m_nExcludedFlags);
    if (!startArea) {
        result.m_status = PathResult::EStatus::Failed_NoStartArea;
        return result;
    }

    CNavArea* goalArea = const_cast<CNavMesh*>(m_pNavMesh)->GetNearestNavArea(
        goalPos, 600.0f, options.m_nExcludedFlags);
    if (!goalArea) {
        result.m_status = PathResult::EStatus::Failed_NoGoalArea;
        return result;
    }

    // Same area trivial path
    if (startArea == goalArea) {
        result.m_status = PathResult::EStatus::Success;
        result.m_areaCorridor.push_back(startArea);
        result.m_rawWaypoints.push_back(startPos);
        result.m_rawWaypoints.push_back(goalPos);
        result.m_smoothedWaypoints.push_back(startPos);
        result.m_smoothedWaypoints.push_back(goalPos);
        result.m_annotatedWaypoints.emplace_back(startPos, startArea->m_id, startArea->m_flags, NavWaypointType::Ground);
        result.m_annotatedWaypoints.emplace_back(goalPos, goalArea->m_id, goalArea->m_flags, NavWaypointType::Ground);
        result.m_totalCost = (goalPos - startPos).Length();
        result.m_totalLength = result.m_totalCost;
        result.m_nodesExplored = 1;
        return result;
    }

    // 2. Perform A* graph search over area adjacency graph
    bool found = SearchAStar(
        startArea, goalArea,
        startPos, goalPos,
        options, costFunc,
        result.m_areaCorridor,
        result.m_totalCost,
        result.m_nodesExplored);

    if (!found || result.m_areaCorridor.empty()) {
        result.m_status = PathResult::EStatus::Failed_NoPath;
        return result;
    }

    // 3. Extract portal gates between consecutive areas
    ExtractPortals(result.m_areaCorridor, startPos, goalPos, result.m_portals);

    // 4. Build raw waypoint trajectory (area centers & portal midpoints)
    result.m_rawWaypoints.push_back(startPos);
    for (const auto& portal : result.m_portals) {
        result.m_rawWaypoints.push_back((portal.m_left + portal.m_right) * 0.5f);
    }
    result.m_rawWaypoints.push_back(goalPos);

    // 5. Apply Simple Stupid Funnel Algorithm (SSFA) / String-Pulling
    if (options.m_bStringPullCorridor && !result.m_portals.empty()) {
        StringPullCorridor(
            startPos, goalPos,
            result.m_portals,
            result.m_areaCorridor,
            options.m_flAgentRadius,
            result.m_smoothedWaypoints);
    } else {
        result.m_smoothedWaypoints = result.m_rawWaypoints;
    }

    // 6. Annotate movement types and integrate Euclidean length
    result.m_totalLength = 0.0f;
    for (std::size_t i = 0; i < result.m_smoothedWaypoints.size(); ++i) {
        const auto& pt = result.m_smoothedWaypoints[i];
        if (i > 0) {
            result.m_totalLength += (pt - result.m_smoothedWaypoints[i - 1]).Length();
        }

        // Determine current area for waypoint
        CNavArea* currentArea = nullptr;
        for (auto* area : result.m_areaCorridor) {
            if (area && area->Contains(pt)) {
                currentArea = area;
                break;
            }
        }
        if (!currentArea) {
            currentArea = (i < result.m_areaCorridor.size()) ? result.m_areaCorridor[i] : result.m_areaCorridor.back();
        }

        NavWaypointType type = NavWaypointType::Ground;
        if (currentArea->HasFlag(NAV_MESH_JUMP)) {
            type = NavWaypointType::Jump;
        } else if (currentArea->HasFlag(NAV_MESH_CROUCH)) {
            type = NavWaypointType::Crouch;
        } else if (currentArea->HasFlag(NAV_MESH_CLIFF)) {
            type = NavWaypointType::DropCliff;
        }

        result.m_annotatedWaypoints.emplace_back(pt, currentArea->m_id, currentArea->m_flags, type);
    }

    result.m_status = PathResult::EStatus::Success;
    return result;
}

// ============================================================================
// A* Search Implementation (NavAreaBuildPath @ 0x18003BDF0)
// ============================================================================
bool CPathfinder::SearchAStar(
    CNavArea* startArea,
    CNavArea* goalArea,
    const Vector3& startPos,
    const Vector3& goalPos,
    const PathOptions& options,
    const INavPathCost& costFunc,
    std::vector<CNavArea*>& outCorridor,
    float& outTotalCost,
    std::size_t& outNodesExplored) const {
    outCorridor.clear();
    outTotalCost = 0.0f;
    outNodesExplored = 0;

    std::priority_queue<PathfinderAStarNode*, std::vector<PathfinderAStarNode*>, PathfinderAStarNode::MinHeapCompare> openList;
    std::unordered_map<std::uint32_t, std::unique_ptr<PathfinderAStarNode>> nodeMap;

    auto getNode = [&](CNavArea* area) -> PathfinderAStarNode* {
        if (!area) return nullptr;
        auto it = nodeMap.find(area->m_id);
        if (it != nodeMap.end()) {
            return it->second.get();
        }
        auto node = std::make_unique<PathfinderAStarNode>();
        node->m_area = area;
        PathfinderAStarNode* ptr = node.get();
        nodeMap[area->m_id] = std::move(node);
        return ptr;
    };

    PathfinderAStarNode* startNode = getNode(startArea);
    startNode->m_gCost = 0.0f;
    startNode->m_hCost = ComputeHeuristic(startPos, goalPos, options.m_flStepHeight);
    startNode->m_totalCost = startNode->m_gCost + options.m_flInadmissibleHeuristicFactor * startNode->m_hCost;
    startNode->m_open = true;
    openList.push(startNode);

    PathfinderAStarNode* goalNode = nullptr;

    while (!openList.empty()) {
        PathfinderAStarNode* current = openList.top();
        openList.pop();

        if (current->m_closed) {
            continue;
        }

        current->m_open = false;
        current->m_closed = true;
        ++outNodesExplored;

        // Check if goal reached
        if (current->m_area == goalArea) {
            goalNode = current;
            break;
        }

        // Check path length cutoff
        if (current->m_gCost > options.m_flMaxPathLength) {
            continue;
        }

        Vector3 curCenter = current->m_area->GetCenter();

        // ---------------------------------------------------------------------
        // 1. Traverse Direct Portal Edges
        // ---------------------------------------------------------------------
        for (const auto& edge : current->m_area->m_portalEdges) {
            CNavArea* neighbor = edge.m_pTargetArea;
            if (!neighbor || (options.m_nExcludedFlags && neighbor->HasFlag(options.m_nExcludedFlags))) {
                continue;
            }
            if (!options.m_bAllowCrouch && neighbor->HasFlag(NAV_MESH_CROUCH)) {
                continue;
            }
            if (!options.m_bAllowJump && neighbor->HasFlag(NAV_MESH_JUMP)) {
                continue;
            }

            float dist = edge.m_flLength > 0.0f ? (neighbor->GetCenter() - curCenter).Length() : 50.0f;
            float stepCost = costFunc.GroundCost(current->m_area, neighbor, dist);
            if (stepCost < 0.0f) continue; // Blocked

            float newGCost = current->m_gCost + stepCost;
            PathfinderAStarNode* neighborNode = getNode(neighbor);
            if (neighborNode->m_closed && newGCost >= neighborNode->m_gCost) {
                continue;
            }

            if (!neighborNode->m_open || newGCost < neighborNode->m_gCost) {
                neighborNode->m_parent = current;
                neighborNode->m_gCost = newGCost;
                neighborNode->m_hCost = ComputeHeuristic(neighbor->GetCenter(), goalPos, options.m_flStepHeight);
                neighborNode->m_totalCost = newGCost + options.m_flInadmissibleHeuristicFactor * neighborNode->m_hCost;
                neighborNode->m_open = true;
                neighborNode->m_closed = false;
                openList.push(neighborNode);
            }
        }

        // ---------------------------------------------------------------------
        // 2. Traverse Cardinal Directional Adjacencies
        // ---------------------------------------------------------------------
        for (std::uint32_t dir = 0; dir < NUM_DIRECTIONS; ++dir) {
            for (const auto& conn : current->m_area->m_adjacentAreas[dir]) {
                CNavArea* neighbor = const_cast<CNavMesh*>(m_pNavMesh)->GetAreaByID(conn.m_areaId);
                if (!neighbor || (options.m_nExcludedFlags && neighbor->HasFlag(options.m_nExcludedFlags))) {
                    continue;
                }
                if (!options.m_bAllowCrouch && neighbor->HasFlag(NAV_MESH_CROUCH)) {
                    continue;
                }
                if (!options.m_bAllowJump && neighbor->HasFlag(NAV_MESH_JUMP)) {
                    continue;
                }

                float dist = (neighbor->GetCenter() - curCenter).Length();
                float stepCost = costFunc.GroundCost(current->m_area, neighbor, dist);
                if (stepCost < 0.0f) continue;

                float newGCost = current->m_gCost + stepCost;
                PathfinderAStarNode* neighborNode = getNode(neighbor);
                if (neighborNode->m_closed && newGCost >= neighborNode->m_gCost) {
                    continue;
                }

                if (!neighborNode->m_open || newGCost < neighborNode->m_gCost) {
                    neighborNode->m_parent = current;
                    neighborNode->m_gCost = newGCost;
                    neighborNode->m_hCost = ComputeHeuristic(neighbor->GetCenter(), goalPos, options.m_flStepHeight);
                    neighborNode->m_totalCost = newGCost + options.m_flInadmissibleHeuristicFactor * neighborNode->m_hCost;
                    neighborNode->m_open = true;
                    neighborNode->m_closed = false;
                    openList.push(neighborNode);
                }
            }
        }

        // ---------------------------------------------------------------------
        // 3. Traverse Vertical Ladder Links
        // ---------------------------------------------------------------------
        if (options.m_bAllowLadders) {
            for (std::uint32_t lDir = 0; lDir < NUM_LADDER_DIRECTIONS; ++lDir) {
                for (std::uint32_t ladderId : current->m_area->m_ladderConnections[lDir]) {
                    const CNavLadder* ladder = m_pNavMesh->GetLadderByID(ladderId);
                    if (!ladder) continue;

                    float lCost = costFunc.LadderCost(ladder);
                    if (lCost < 0.0f) continue;

                    // Collect target connected areas
                    std::uint32_t targetIds[] = {
                        ladder->m_topForwardAreaId, ladder->m_topLeftAreaId,
                        ladder->m_topRightAreaId, ladder->m_topBehindAreaId,
                        ladder->m_bottomAreaId, ladder->m_bottomLeftAreaId,
                        ladder->m_bottomRightAreaId
                    };

                    for (std::uint32_t tid : targetIds) {
                        if (tid == 0 || tid == current->m_area->m_id) continue;
                        CNavArea* neighbor = const_cast<CNavMesh*>(m_pNavMesh)->GetAreaByID(tid);
                        if (!neighbor) continue;

                        float newGCost = current->m_gCost + lCost;
                        PathfinderAStarNode* neighborNode = getNode(neighbor);
                        if (neighborNode->m_closed && newGCost >= neighborNode->m_gCost) {
                            continue;
                        }

                        if (!neighborNode->m_open || newGCost < neighborNode->m_gCost) {
                            neighborNode->m_parent = current;
                            neighborNode->m_ladder = ladder;
                            neighborNode->m_gCost = newGCost;
                            neighborNode->m_hCost = ComputeHeuristic(neighbor->GetCenter(), goalPos, options.m_flStepHeight);
                            neighborNode->m_totalCost = newGCost + options.m_flInadmissibleHeuristicFactor * neighborNode->m_hCost;
                            neighborNode->m_open = true;
                            neighborNode->m_closed = false;
                            openList.push(neighborNode);
                        }
                    }
                }
            }
        }
    }

    if (!goalNode) {
        return false;
    }

    // Backtrack from goalNode to startNode
    outTotalCost = goalNode->m_gCost;
    PathfinderAStarNode* trace = goalNode;
    while (trace) {
        outCorridor.push_back(trace->m_area);
        trace = trace->m_parent;
    }
    std::reverse(outCorridor.begin(), outCorridor.end());
    return true;
}

// ============================================================================
// Extract Portals Connecting Consecutive Corridor Areas
// ============================================================================
bool CPathfinder::ExtractPortals(
    const std::vector<CNavArea*>& corridor,
    const Vector3& /*startPos*/,
    const Vector3& /*goalPos*/,
    std::vector<NavPortal>& outPortals) const {
    outPortals.clear();
    if (corridor.size() < 2) {
        return false;
    }

    for (std::size_t i = 0; i < corridor.size() - 1; ++i) {
        const CNavArea* from = corridor[i];
        const CNavArea* to = corridor[i + 1];
        if (!from || !to) continue;

        Vector3 leftPoint{0.0f, 0.0f, 0.0f};
        Vector3 rightPoint{0.0f, 0.0f, 0.0f};
        NavPortalEdge_t edgeData{};

        // Check if explicit portal edge exists in from area
        const NavPortalEdge_t* pEdge = from->FindPortalEdgeTo(to);
        if (pEdge) {
            leftPoint = pEdge->m_portalLeft;
            rightPoint = pEdge->m_portalRight;
            edgeData = *pEdge;
        } else {
            // Synthesize portal from overlapping AABB bounds
            float minX = std::max(from->m_nwCorner.x, to->m_nwCorner.x);
            float maxX = std::min(from->m_seCorner.x, to->m_seCorner.x);
            float minY = std::max(from->m_nwCorner.y, to->m_nwCorner.y);
            float maxY = std::min(from->m_seCorner.y, to->m_seCorner.y);

            if (maxX >= minX && (maxY - minY) < 2.0f) {
                // Horizontal edge boundary (North/South)
                float edgeY = (from->m_seCorner.y + to->m_nwCorner.y) * 0.5f;
                leftPoint = Vector3{minX, edgeY, from->ComputeZ(minX, edgeY)};
                rightPoint = Vector3{maxX, edgeY, from->ComputeZ(maxX, edgeY)};
            } else if (maxY >= minY && (maxX - minX) < 2.0f) {
                // Vertical edge boundary (East/West)
                float edgeX = (from->m_seCorner.x + to->m_nwCorner.x) * 0.5f;
                leftPoint = Vector3{edgeX, minY, from->ComputeZ(edgeX, minY)};
                rightPoint = Vector3{edgeX, maxY, from->ComputeZ(edgeX, maxY)};
            } else {
                // Fallback: connect centers
                Vector3 mid = (from->GetCenter() + to->GetCenter()) * 0.5f;
                leftPoint = mid;
                rightPoint = mid;
            }
        }

        // Consistent orientation: left must be to the left, right to the right
        // from perspective of moving fromCenter -> toCenter
        Vector3 fromCenter = from->GetCenter();
        if (TriArea2D(fromCenter, leftPoint, rightPoint) > 0.0f) {
            std::swap(leftPoint, rightPoint);
        }

        outPortals.emplace_back(leftPoint, rightPoint, from->m_id, to->m_id, edgeData);
    }

    return !outPortals.empty();
}

// ============================================================================
// Simple Stupid Funnel Algorithm (SSFA) / String-Pulling Optimization
// Decompiled from server.dll @ 0x1810EE570 (SpacePathfind_StringPull_Core)
// ============================================================================
bool CPathfinder::StringPullCorridor(
    const Vector3& startPos,
    const Vector3& goalPos,
    const std::vector<NavPortal>& portals,
    const std::vector<CNavArea*>& corridor,
    float agentRadius,
    std::vector<Vector3>& outSmoothedWaypoints) {
    outSmoothedWaypoints.clear();
    if (portals.empty()) {
        outSmoothedWaypoints.push_back(startPos);
        outSmoothedWaypoints.push_back(goalPos);
        return true;
    }

    // 1. Prepare portal boundary arrays with agent radius inset
    std::vector<Vector3> leftVerts;
    std::vector<Vector3> rightVerts;
    leftVerts.reserve(portals.size() + 2);
    rightVerts.reserve(portals.size() + 2);

    // Initial position (apex 0)
    leftVerts.push_back(startPos);
    rightVerts.push_back(startPos);

    for (const auto& portal : portals) {
        Vector3 l = portal.m_left;
        Vector3 r = portal.m_right;

        // Apply collision hull inset along portal segment
        Vector3 dir = r - l;
        float len = dir.Length2D();
        if (len > 2.0f * agentRadius && agentRadius > 0.0f) {
            Vector3 unitDir = dir / len;
            l = l + unitDir * agentRadius;
            r = r - unitDir * agentRadius;
        } else {
            // Narrow portal: clamp to midpoint
            Vector3 mid = (l + r) * 0.5f;
            l = mid;
            r = mid;
        }

        leftVerts.push_back(l);
        rightVerts.push_back(r);
    }

    // Goal position (end apex)
    leftVerts.push_back(goalPos);
    rightVerts.push_back(goalPos);

    // 2. Execute Funnel String-Pulling
    std::size_t numPortals = leftVerts.size();
    Vector3 apex = startPos;
    Vector3 leftRay = leftVerts[1];
    Vector3 rightRay = rightVerts[1];

    std::size_t apexIdx = 0;
    std::size_t leftIdx = 1;
    std::size_t rightIdx = 1;

    outSmoothedWaypoints.push_back(startPos);

    for (std::size_t i = 2; i < numPortals; ++i) {
        const Vector3& nextLeft = leftVerts[i];
        const Vector3& nextRight = rightVerts[i];

        // -----------------------------------------------------------------
        // Update Right Leg
        // -----------------------------------------------------------------
        if (TriArea2D(apex, rightRay, nextRight) <= 0.0f) {
            // Tighten right leg if within current left boundary
            if (apex.Dist2DTo(leftRay) < 1e-3f || TriArea2D(apex, leftRay, nextRight) > 0.0f) {
                rightRay = nextRight;
                rightIdx = i;
            } else {
                // Right leg crosses over left leg: corner found!
                apex = leftRay;
                apexIdx = leftIdx;

                // Adjust elevation to walkable area surface
                std::size_t areaIdx = std::min(apexIdx, corridor.size() - 1);
                if (areaIdx < corridor.size() && corridor[areaIdx]) {
                    apex.z = corridor[areaIdx]->ComputeZ(apex.x, apex.y);
                }

                outSmoothedWaypoints.push_back(apex);

                // Restart funnel from new apex
                leftRay = apex;
                rightRay = apex;
                leftIdx = apexIdx;
                rightIdx = apexIdx;
                i = apexIdx + 1;
                if (i < numPortals) {
                    leftRay = leftVerts[i];
                    rightRay = rightVerts[i];
                    leftIdx = i;
                    rightIdx = i;
                }
                continue;
            }
        }

        // -----------------------------------------------------------------
        // Update Left Leg
        // -----------------------------------------------------------------
        if (TriArea2D(apex, leftRay, nextLeft) >= 0.0f) {
            // Tighten left leg if within current right boundary
            if (apex.Dist2DTo(rightRay) < 1e-3f || TriArea2D(apex, rightRay, nextLeft) < 0.0f) {
                leftRay = nextLeft;
                leftIdx = i;
            } else {
                // Left leg crosses over right leg: corner found!
                apex = rightRay;
                apexIdx = rightIdx;

                // Adjust elevation to walkable area surface
                std::size_t areaIdx = std::min(apexIdx, corridor.size() - 1);
                if (areaIdx < corridor.size() && corridor[areaIdx]) {
                    apex.z = corridor[areaIdx]->ComputeZ(apex.x, apex.y);
                }

                outSmoothedWaypoints.push_back(apex);

                // Restart funnel from new apex
                leftRay = apex;
                rightRay = apex;
                leftIdx = apexIdx;
                rightIdx = apexIdx;
                i = apexIdx + 1;
                if (i < numPortals) {
                    leftRay = leftVerts[i];
                    rightRay = rightVerts[i];
                    leftIdx = i;
                    rightIdx = i;
                }
                continue;
            }
        }
    }

    // Final waypoint to goalPos
    if (outSmoothedWaypoints.empty() || outSmoothedWaypoints.back().DistTo(goalPos) > 1.0f) {
        outSmoothedWaypoints.push_back(goalPos);
    }

    return true;
}

} // namespace cs2::navsystem
