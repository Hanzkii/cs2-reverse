#include "navmesh.h"
#include <cstring>
#include <algorithm>

namespace cs2::navsystem {

// -----------------------------------------------------------------------------
// Buffer Reader Helper for Binary Deserialization
// -----------------------------------------------------------------------------
class BufferReader {
public:
    BufferReader(const uint8_t* data, size_t size) : m_data(data), m_size(size), m_pos(0) {}

    [[nodiscard]] bool HasBytes(size_t n) const noexcept {
        return m_pos + n <= m_size;
    }

    template <typename T>
    bool Read(T& outVal) noexcept {
        if (!HasBytes(sizeof(T))) return false;
        std::memcpy(&outVal, m_data + m_pos, sizeof(T));
        m_pos += sizeof(T);
        return true;
    }

    bool ReadBytes(void* outBuf, size_t n) noexcept {
        if (!HasBytes(n)) return false;
        std::memcpy(outBuf, m_data + m_pos, n);
        m_pos += n;
        return true;
    }

    bool ReadVector3(Vector3& outVec) noexcept {
        return Read(outVec.x) && Read(outVec.y) && Read(outVec.z);
    }

    bool ReadString(std::string& outStr, size_t length) {
        if (!HasBytes(length)) return false;
        outStr.assign(reinterpret_cast<const char*>(m_data + m_pos), length);
        m_pos += length;
        return true;
    }

    [[nodiscard]] size_t GetPos() const noexcept { return m_pos; }
    void Seek(size_t pos) noexcept { m_pos = std::min(pos, m_size); }

private:
    const uint8_t* m_data{nullptr};
    size_t m_size{0};
    size_t m_pos{0};
};

// -----------------------------------------------------------------------------
// CNavArea Methods
// -----------------------------------------------------------------------------
bool CNavArea::Contains(const Vector3& pos) const noexcept {
    // 1. Check AABB bounding extents
    if (pos.x < m_nwCorner.x || pos.x > m_seCorner.x ||
        pos.y < m_nwCorner.y || pos.y > m_seCorner.y) {
        return false;
    }

    // 2. If arbitrary polygon vertices exist, perform point-in-polygon test
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

float CNavArea::ComputeZ(float x, float y) const noexcept {
    float width = m_seCorner.x - m_nwCorner.x;
    float height = m_seCorner.y - m_nwCorner.y;

    if (width <= 0.001f || height <= 0.001f) {
        return m_nwCorner.z;
    }

    float u = std::clamp((x - m_nwCorner.x) / width, 0.0f, 1.0f);
    float v = std::clamp((y - m_nwCorner.y) / height, 0.0f, 1.0f);

    // Bilinear interpolation across the 4 corners:
    // NW = m_nwCorner.z, NE = m_neZ, SW = m_swZ, SE = m_seCorner.z
    float zNorth = m_nwCorner.z + u * (m_neZ - m_nwCorner.z);
    float zSouth = m_swZ + u * (m_seCorner.z - m_swZ);
    return zNorth + v * (zSouth - zNorth);
}

// -----------------------------------------------------------------------------
// CNavPathCost Methods
// -----------------------------------------------------------------------------
float CNavPathCost::GroundCost(const CNavArea* from, const CNavArea* to, float dist) const {
    if (!from || !to) return -1.0f;
    if (IsAreaBlocked(to)) return -1.0f;

    float cost = dist;

    // Apply area attribute modifiers
    if (to->HasFlag(NAV_MESH_CROUCH)) {
        cost *= 1.35f; // Slower movement in crouch areas
    }
    if (to->HasFlag(NAV_MESH_JUMP)) {
        cost += 50.0f; // Jump traversal penalty
    }
    if (to->HasFlag(NAV_MESH_AVOID)) {
        cost *= 3.0f;  // Heavy avoid penalty
    }

    return cost;
}

float CNavPathCost::LadderCost(const CNavLadder* ladder) const {
    if (!ladder || !m_bAllowLadders) return -1.0f;
    return ladder->m_length * 1.5f; // Climbing is slower than running
}

bool CNavPathCost::IsAreaBlocked(const CNavArea* area) const {
    if (!area) return true;

    // Exclude blocked or transient locked areas
    if (m_navHull >= 0) {
        // Hull-specific compatibility checks
    }

    return false;
}

// -----------------------------------------------------------------------------
// CNavMesh Parsing & Search Methods
// -----------------------------------------------------------------------------
bool CNavMesh::LoadFromBuffer(const uint8_t* data, size_t dataSize) {
    if (!data || dataSize < 16) return false;

    BufferReader reader(data, dataSize);

    // 1. Header & Magic Verification
    if (!reader.Read(m_magic) || m_magic != NAV_MAGIC) {
        return false;
    }

    if (!reader.Read(m_version) || m_version > NAV_VERSION) {
        return false;
    }

    if (m_version > 9) {
        if (!reader.Read(m_subVersion)) return false;
    }

    if (m_version >= 4) {
        if (!reader.Read(m_bspSize)) return false;
    }

    if (m_version >= 14) {
        uint8_t analyzedByte = 0;
        if (!reader.Read(analyzedByte)) return false;
        m_isAnalyzed = (analyzedByte != 0);
    }

    // 2. Directory of Place Names
    if (m_version >= 5) {
        uint16_t placeCount = 0;
        if (!reader.Read(placeCount)) return false;

        m_places.clear();
        m_places.reserve(placeCount);

        for (uint16_t i = 0; i < placeCount; ++i) {
            uint16_t strLen = 0;
            if (!reader.Read(strLen)) return false;
            std::string placeStr;
            if (!reader.ReadString(placeStr, strLen)) return false;
            m_places.push_back(std::move(placeStr));
        }
    }

    // 3. Navigation Areas (CNavArea array)
    uint32_t areaCount = 0;
    if (!reader.Read(areaCount)) return false;

    m_areas.clear();
    m_areas.reserve(areaCount);
    m_areaIndexMap.clear();

    for (uint32_t i = 0; i < areaCount; ++i) {
        CNavArea area;
        if (!reader.Read(area.m_id)) return false;

        // Flags
        if (m_version < 9) {
            uint8_t f = 0;
            if (!reader.Read(f)) return false;
            area.m_flags = f;
        } else if (m_version < 13) {
            uint16_t f = 0;
            if (!reader.Read(f)) return false;
            area.m_flags = f;
        } else {
            if (!reader.Read(area.m_flags)) return false;
        }

        // Bounding extents
        if (m_version < 20) {
            if (!reader.ReadVector3(area.m_nwCorner)) return false;
            if (!reader.ReadVector3(area.m_seCorner)) return false;
            if (!reader.Read(area.m_neZ)) return false;
            if (!reader.Read(area.m_swZ)) return false;
        } else {
            uint32_t polyVertCount = 0;
            if (!reader.Read(polyVertCount)) return false;

            if (m_version < 31 && polyVertCount > 0 && polyVertCount < 100) {
                area.m_polyPoints.resize(polyVertCount);
                for (uint32_t v = 0; v < polyVertCount; ++v) {
                    if (!reader.ReadVector3(area.m_polyPoints[v])) return false;
                }
                // Derive AABB from poly vertices
                if (!area.m_polyPoints.empty()) {
                    area.m_nwCorner = area.m_polyPoints[0];
                    area.m_seCorner = area.m_polyPoints[0];
                    for (const auto& pt : area.m_polyPoints) {
                        area.m_nwCorner.x = std::min(area.m_nwCorner.x, pt.x);
                        area.m_nwCorner.y = std::min(area.m_nwCorner.y, pt.y);
                        area.m_nwCorner.z = std::min(area.m_nwCorner.z, pt.z);
                        area.m_seCorner.x = std::max(area.m_seCorner.x, pt.x);
                        area.m_seCorner.y = std::max(area.m_seCorner.y, pt.y);
                        area.m_seCorner.z = std::max(area.m_seCorner.z, pt.z);
                    }
                    area.m_neZ = area.m_nwCorner.z;
                    area.m_swZ = area.m_seCorner.z;
                }
            } else {
                if (!reader.ReadVector3(area.m_nwCorner)) return false;
                if (!reader.ReadVector3(area.m_seCorner)) return false;
                if (!reader.Read(area.m_neZ)) return false;
                if (!reader.Read(area.m_swZ)) return false;
            }
        }

        // Connections in 4 cardinal directions (North, East, South, West)
        for (uint32_t dir = 0; dir < NUM_DIRECTIONS; ++dir) {
            uint32_t connCount = 0;
            if (!reader.Read(connCount)) return false;

            area.m_adjacentAreas[dir].reserve(connCount);
            for (uint32_t c = 0; c < connCount; ++c) {
                NavConnect_t conn;
                if (!reader.Read(conn.m_areaId)) return false;
                if (m_version >= 21) {
                    if (!reader.Read(conn.m_edgeId)) return false;
                }
                area.m_adjacentAreas[dir].push_back(conn);
            }
        }

        // Hiding spots
        if (m_version >= 2) {
            uint8_t hidingSpotCount = 0;
            if (!reader.Read(hidingSpotCount)) return false;

            area.m_hidingSpots.reserve(hidingSpotCount);
            for (uint8_t h = 0; h < hidingSpotCount; ++h) {
                HidingSpot spot;
                if (!reader.Read(spot.m_id)) return false;
                if (!reader.ReadVector3(spot.m_pos)) return false;
                if (!reader.Read(spot.m_flags)) return false;
                area.m_hidingSpots.push_back(spot);
            }
        }

        // Ladder connections
        if (m_version >= 6) {
            for (uint32_t ladderDir = 0; ladderDir < NUM_LADDER_DIRECTIONS; ++ladderDir) {
                uint32_t ladderCount = 0;
                if (!reader.Read(ladderCount)) return false;

                area.m_ladderConnections[ladderDir].reserve(ladderCount);
                for (uint32_t l = 0; l < ladderCount; ++l) {
                    uint32_t ladderId = 0;
                    if (!reader.Read(ladderId)) return false;
                    area.m_ladderConnections[ladderDir].push_back(ladderId);
                }
            }
        }

        // Place ID
        if (m_version >= 5 && m_version < 23) {
            uint16_t pId = 0;
            if (!reader.Read(pId)) return false;
            area.m_placeId = pId;
        }

        m_areaIndexMap[area.m_id] = m_areas.size();
        m_areas.push_back(std::move(area));
    }

    // 4. Navigation Ladders (CNavLadder array)
    if (m_version >= 6) {
        uint32_t ladderCount = 0;
        if (!reader.Read(ladderCount)) return false;

        m_ladders.clear();
        m_ladders.reserve(ladderCount);
        m_ladderIndexMap.clear();

        for (uint32_t i = 0; i < ladderCount; ++i) {
            CNavLadder ladder;
            if (!reader.Read(ladder.m_id)) return false;
            if (!reader.Read(ladder.m_width)) return false;
            if (!reader.ReadVector3(ladder.m_top)) return false;
            if (!reader.ReadVector3(ladder.m_bottom)) return false;
            if (!reader.Read(ladder.m_length)) return false;

            uint32_t dirVal = 0;
            if (!reader.Read(dirVal)) return false;
            ladder.m_dir = static_cast<NavDirType>(dirVal % NUM_DIRECTIONS);

            // Compute ladder directional unit normal
            switch (ladder.m_dir) {
                case NORTH: ladder.m_normal = Vector3{ 0.0f, -1.0f, 0.0f}; break;
                case EAST:  ladder.m_normal = Vector3{ 1.0f,  0.0f, 0.0f}; break;
                case SOUTH: ladder.m_normal = Vector3{ 0.0f,  1.0f, 0.0f}; break;
                case WEST:  ladder.m_normal = Vector3{-1.0f,  0.0f, 0.0f}; break;
                default:    ladder.m_normal = Vector3{ 0.0f,  0.0f, 1.0f}; break;
            }

            // Area connections
            if (!reader.Read(ladder.m_topForwardAreaId)) return false;
            if (!reader.Read(ladder.m_topLeftAreaId)) return false;
            if (!reader.Read(ladder.m_topRightAreaId)) return false;
            if (!reader.Read(ladder.m_topBehindAreaId)) return false;
            if (!reader.Read(ladder.m_bottomAreaId)) return false;

            if (m_version >= 33) {
                if (!reader.Read(ladder.m_bottomLeftAreaId)) return false;
                if (!reader.Read(ladder.m_bottomRightAreaId)) return false;
            }

            m_ladderIndexMap[ladder.m_id] = m_ladders.size();
            m_ladders.push_back(std::move(ladder));
        }
    }

    return true;
}

CNavArea* CNavMesh::GetAreaByID(uint32_t id) noexcept {
    auto it = m_areaIndexMap.find(id);
    return (it != m_areaIndexMap.end()) ? &m_areas[it->second] : nullptr;
}

const CNavArea* CNavMesh::GetAreaByID(uint32_t id) const noexcept {
    auto it = m_areaIndexMap.find(id);
    return (it != m_areaIndexMap.end()) ? &m_areas[it->second] : nullptr;
}

CNavLadder* CNavMesh::GetLadderByID(uint32_t id) noexcept {
    auto it = m_ladderIndexMap.find(id);
    return (it != m_ladderIndexMap.end()) ? &m_ladders[it->second] : nullptr;
}

const CNavLadder* CNavMesh::GetLadderByID(uint32_t id) const noexcept {
    auto it = m_ladderIndexMap.find(id);
    return (it != m_ladderIndexMap.end()) ? &m_ladders[it->second] : nullptr;
}

CNavArea* CNavMesh::GetNearestNavArea(
    const Vector3& pos,
    float maxDistance,
    uint32_t excludedFlags) noexcept {

    CNavArea* bestArea = nullptr;
    float bestDistSq = maxDistance * maxDistance;

    for (auto& area : m_areas) {
        if (excludedFlags != 0 && (area.m_flags & excludedFlags) != 0) {
            continue;
        }

        if (area.Contains(pos)) {
            float areaZ = area.ComputeZ(pos.x, pos.y);
            float dz = std::abs(pos.z - areaZ);
            if (dz <= 71.0f) { // Source 2 player eye/origin tolerance
                return &area;
            }
        }

        Vector3 center = area.GetCenter();
        float distSq = (pos - center).LengthSqr();
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestArea = &area;
        }
    }

    return bestArea;
}

float CNavMesh::ComputeHeuristicDistance(
    const Vector3& from,
    const Vector3& goal,
    float stepHeight) noexcept {

    float dx = goal.x - from.x;
    float dy = goal.y - from.y;
    float rawDz = std::abs(goal.z - from.z);

    // Source 2 elevation penalty:
    // Vertical delta beyond standard step height (18 units * 2 = 36 units)
    // is penalized with a 3.0x scaling factor in navsystem.dll.
    float maxStepClimb = stepHeight * 2.0f;
    float dzClamped = std::min(rawDz, maxStepClimb);
    float dzPenalized = (rawDz - dzClamped) * 3.0f + dzClamped;

    return std::sqrt(dx * dx + dy * dy + dzPenalized * dzPenalized);
}

bool CNavMesh::FindPath(
    const Vector3& startPos,
    const Vector3& goalPos,
    CNavArea* startArea,
    CNavArea* goalArea,
    const INavPathCost& costFunc,
    std::vector<Vector3>& outPathWaypoints,
    float maxPathLength,
    float inadmissibleHeuristicFactor) {

    outPathWaypoints.clear();
    if (!startArea) startArea = GetNearestNavArea(startPos);
    if (!goalArea) goalArea = GetNearestNavArea(goalPos);

    if (!startArea || !goalArea) {
        return false;
    }

    // Trivial case: already in goal area
    if (startArea == goalArea) {
        outPathWaypoints.push_back(startPos);
        outPathWaypoints.push_back(goalPos);
        return true;
    }

    // Node allocator map
    std::unordered_map<uint32_t, std::unique_ptr<AStarNode>> nodePool;
    auto getNode = [&](CNavArea* area) -> AStarNode* {
        auto it = nodePool.find(area->m_id);
        if (it != nodePool.end()) return it->second.get();
        auto node = std::make_unique<AStarNode>();
        node->m_area = area;
        AStarNode* ptr = node.get();
        nodePool[area->m_id] = std::move(node);
        return ptr;
    };

    std::priority_queue<AStarNode*, std::vector<AStarNode*>, AStarNode::Compare> openList;

    AStarNode* startNode = getNode(startArea);
    startNode->m_gCost = 0.0f;
    startNode->m_hCost = ComputeHeuristicDistance(startPos, goalPos) * inadmissibleHeuristicFactor;
    startNode->m_totalCost = startNode->m_gCost + startNode->m_hCost;
    startNode->m_open = true;

    openList.push(startNode);
    AStarNode* goalNode = nullptr;

    while (!openList.empty()) {
        AStarNode* current = openList.top();
        openList.pop();

        if (current->m_closed) continue;
        current->m_open = false;
        current->m_closed = true;

        if (current->m_area == goalArea) {
            goalNode = current;
            break;
        }

        if (current->m_gCost > maxPathLength) {
            continue;
        }

        Vector3 currentPos = (current->m_parent == nullptr) ? startPos : current->m_area->GetCenter();

        // 1. Expand cardinal adjacent area connections
        for (uint32_t dir = 0; dir < NUM_DIRECTIONS; ++dir) {
            for (const auto& conn : current->m_area->m_adjacentAreas[dir]) {
                CNavArea* neighborArea = GetAreaByID(conn.m_areaId);
                if (!neighborArea) continue;

                AStarNode* neighborNode = getNode(neighborArea);
                if (neighborNode->m_closed) continue;

                Vector3 neighborPos = (neighborArea == goalArea) ? goalPos : neighborArea->GetCenter();
                float dist = currentPos.DistTo(neighborPos);

                float traversalCost = costFunc.GroundCost(current->m_area, neighborArea, dist);
                if (traversalCost < 0.0f) continue; // Obstacle / blocked

                float tentativeG = current->m_gCost + traversalCost;

                if (!neighborNode->m_open || tentativeG < neighborNode->m_gCost) {
                    neighborNode->m_parent = current;
                    neighborNode->m_ladder = nullptr;
                    neighborNode->m_gCost = tentativeG;
                    neighborNode->m_hCost = ComputeHeuristicDistance(neighborPos, goalPos) * inadmissibleHeuristicFactor;
                    neighborNode->m_totalCost = neighborNode->m_gCost + neighborNode->m_hCost;

                    if (!neighborNode->m_open) {
                        neighborNode->m_open = true;
                        openList.push(neighborNode);
                    }
                }
            }
        }

        // 2. Expand ladder connections
        if (costFunc.AllowLadders()) {
            for (uint32_t ladderDir = 0; ladderDir < NUM_LADDER_DIRECTIONS; ++ladderDir) {
                for (uint32_t ladderId : current->m_area->m_ladderConnections[ladderDir]) {
                    const CNavLadder* ladder = GetLadderByID(ladderId);
                    if (!ladder) continue;

                    // Ladder leads to target area (top or bottom)
                    uint32_t targetAreaIds[2] = {ladder->m_topForwardAreaId, ladder->m_bottomAreaId};
                    for (uint32_t targetId : targetAreaIds) {
                        if (targetId == 0 || targetId == current->m_area->m_id) continue;

                        CNavArea* ladderTargetArea = GetAreaByID(targetId);
                        if (!ladderTargetArea) continue;

                        AStarNode* targetNode = getNode(ladderTargetArea);
                        if (targetNode->m_closed) continue;

                        float ladderCost = costFunc.LadderCost(ladder);
                        if (ladderCost < 0.0f) continue;

                        float tentativeG = current->m_gCost + ladderCost;

                        if (!targetNode->m_open || tentativeG < targetNode->m_gCost) {
                            targetNode->m_parent = current;
                            targetNode->m_ladder = ladder;
                            targetNode->m_gCost = tentativeG;
                            Vector3 targetPos = (ladderTargetArea == goalArea) ? goalPos : ladderTargetArea->GetCenter();
                            targetNode->m_hCost = ComputeHeuristicDistance(targetPos, goalPos) * inadmissibleHeuristicFactor;
                            targetNode->m_totalCost = targetNode->m_gCost + targetNode->m_hCost;

                            if (!targetNode->m_open) {
                                targetNode->m_open = true;
                                openList.push(targetNode);
                            }
                        }
                    }
                }
            }
        }
    }

    if (!goalNode) {
        return false;
    }

    // Trace path back from goalNode to start
    std::vector<Vector3> reversePath;
    reversePath.push_back(goalPos);

    for (AStarNode* curr = goalNode; curr != nullptr; curr = curr->m_parent) {
        if (curr->m_ladder != nullptr) {
            // Include ladder top and bottom waypoints for vertical climbing
            reversePath.push_back(curr->m_ladder->m_top);
            reversePath.push_back(curr->m_ladder->m_bottom);
        } else if (curr != goalNode && curr != startNode) {
            reversePath.push_back(curr->m_area->GetCenter());
        }
    }
    reversePath.push_back(startPos);

    std::reverse(reversePath.begin(), reversePath.end());
    outPathWaypoints = std::move(reversePath);

    return true;
}

} // namespace cs2::navsystem
