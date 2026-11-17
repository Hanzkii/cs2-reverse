# Counter-Strike 2 (Source 2) - Rubikon Convex Polytope & Continuous Collision Physics (`vphysics2.dll`)

> **Binary Target**: `vphysics2.dll` (Base Address: `0x180000000`, Image Size: 4,829,848 bytes)  
> **Subsystems Analyzed**: `CRnShape`, `RnHull_t`, `RnCapsule_t`, `RnOBB_t`, `RnSphere_t`, `CRubikonCollision`, `MortonBVH`, `CWorldStepJob`  
> **Core Algorithms**: Gilbert-Johnson-Keerthi (GJK) 3D, Expanding Polytope Algorithm (EPA), Conservative Advancement (CCD), Morton 3D Space-Filling BVH  
> **Verification Status**: 6 / 6 Test Suites Verified with MSVC x64 (`/std:c++20 /O2 /EHsc /W4 /WX`) ([`cs2_rubikon_collision_sim.cpp`](simulators/cs2_rubikon_collision_sim.cpp))

---

## 1. Rubikon Physics Engine Architecture in Source 2

In Counter-Strike 2, physics simulation, spatial queries, and ray/hull sweeps are handled by Valve's proprietary in-house physics engine **Rubikon** (`vphysics2.dll`).

Rubikon differs fundamentally from legacy physics engines (such as VPhysics/Havok in Source 1) through:
1. **Unified Memory & Job Integration**: Uses Source 2's `g_pMemAlloc` and the asynchronous ThreadPool (`CWorldStepJob`) instead of external heap managers.
2. **Convex Polyhedral Primitives**: Polyhedral hulls (`RnHull_t`), bounding capsules (`RnCapsule_t`), oriented bounding boxes (`RnOBB_t`), and spheres (`RnSphere_t`) are stored with dual representations: bounding half-space planes for slab clipping and vertex sets for extremal support mapping.
3. **Sub-Tick Narrowphase Pipeline**: Discrete collision and Continuous Collision Detection (CCD) utilize Gilbert-Johnson-Keerthi (GJK) simplex distance tracking and Expanding Polytope Algorithm (EPA) contact manifold extraction.
4. **48-byte Morton BVH Acceleration**: World geometry and composite shapes (`RnCompoundTree_t`) are partitioned using SIMD-aligned 48-byte nodes indexed along 3D Morton space-filling curves.

```mermaid
flowchart TD
    subgraph EngineClientServer ["Source 2 Game Execution Layer"]
        Pawn["C_CSPlayerPawn / Entity Movement"]
        RayQuery["Engine Trace Queries (Ray/Swept Capsule)"]
    end

    subgraph IVPhysics ["vphysics2.dll Interface Layer"]
        PhysWorld["CVPhys2World (0x1803F9B20)"]
        Filter["Collision Filter Matrix (ShouldCollide)"]
    end

    subgraph Broadphase ["Broadphase Spatial Acceleration"]
        Morton["Morton 3D (Z-Order) Spatial Tree"]
        BVHNodes["48-byte SIMD BVH Nodes (FUN_18000f4e0)"]
    end

    subgraph Narrowphase ["Narrowphase Convex Geometry Solver"]
        Extremal["Support Mappings: S_A(d), S_B(-d)"]
        GJK["Gilbert-Johnson-Keerthi (GJK) Solver"]
        EPA["Expanding Polytope Algorithm (EPA)"]
        CCD["Conservative Advancement (Time of Impact)"]
    end

    Pawn --> RayQuery
    RayQuery --> Filter
    Filter --> PhysWorld
    PhysWorld --> Morton
    Morton --> BVHNodes
    BVHNodes --> Extremal
    Extremal --> GJK
    GJK -->|Intersecting| EPA
    GJK -->|Disjoint / Swept| CCD
```

---

## 2. Convex Hull & Primitive Representations (`physicslib`)

Source 2 compiles physical collision models (`.vphys`) into binary structures registered in the `physicslib` schema binding table (`0x1803DADB0`).

### 2.1 `RnHull_t` (Convex Polyhedron - Size `0x00F8` / 248 bytes)
A convex hull in Rubikon is defined by:
- **Support Vertices** (`m_VertexPositions`): Contiguous array of 3D points used by GJK support mapping.
- **Bounding Planes** (`m_FacePlanes`): Half-space bounding planes $P_i = (\vec{n}_i, d_i)$ used for fast polyhedral slab clipping with Valve's skin tolerance ($\varepsilon = 0.03125$ or $1/32$ inch).
- **Winged Half-Edges** (`m_Edges`): Explicit face adjacency topology.

```cpp
struct RnHull_t {
    Vector3                 m_vCentroid;           // +0x000 Center of mass
    float                   m_flMaxAngularRadius;  // +0x00C Maximum bounding radius
    float                   m_flMinCentroidRadius; // +0x010 Minimum surface distance
    AABB                    m_Bounds;              // +0x014 Local AABB min/max
    Vector3                 m_vOrthographicAreas;  // +0x02C Projected areas along X, Y, Z
    MassProperties_t        m_MassProperties;      // +0x038 Inertia tensor & mass
    float                   m_flVolume;            // +0x068 Geometric volume
    float                   m_flSurfaceArea;       // +0x06C Polygonal surface area
    CUtlVector<Vector3>     m_VertexPositions;     // +0x070 Array of support vertices
    CUtlVector<RnPlane_t>   m_FacePlanes;          // +0x088 Half-space bounding planes
    uint32_t                m_nFlags;              // +0x0A0 Feature flags
    CRegionSVM*             m_pRegionSVM;          // +0x0A8 Voronoi region accelerator
    CUtlVector<RnVertex_t>  m_Vertices;            // +0x0B0 Vertex connectivity
    CUtlVector<RnHalfEdge_t> m_Edges;              // +0x0C8 Winged half-edge topology
    CUtlVector<RnFace_t>    m_Faces;               // +0x0E0 Polygonal face indices
};
```

### 2.2 Extremal Support Mapping Functions

For an arbitrary convex shape $A$, the support mapping $S_A(\vec{d})$ returns the extremal point on the boundary of $A$ along query direction $\vec{d}$:

$$S_A(\vec{d}) = \arg\max_{v \in A} (v \cdot \vec{d})$$

| Shape Primitive | Mathematical Support Formulation | Implementation Complexity |
|---|---|---|
| **Sphere** ($c, r$) | $S(\vec{d}) = c + r \frac{\vec{d}}{\|\vec{d}\|}$ | $O(1)$ |
| **Capsule** ($p_0, p_1, r$) | $p^* = (p_0 \cdot \vec{d} \ge p_1 \cdot \vec{d}) \ ? \ p_0 : p_1$; $S(\vec{d}) = p^* + r \frac{\vec{d}}{\|\vec{d}\|}$ | $O(1)$ |
| **OBB** ($c, e, R$) | $\vec{d}_{loc} = R^T \vec{d}$; $v_{loc} = \text{sign}(\vec{d}_{loc}) \odot e$; $S(\vec{d}) = c + R v_{loc}$ | $O(1)$ |
| **Polyhedral Hull** ($V, R, c$) | $\vec{d}_{loc} = R^T \vec{d}$; $i^* = \arg\max_i (v_i \cdot \vec{d}_{loc})$; $S(\vec{d}) = c + R v_{i^*}$ | $O(N)$ (SIMD unrolled) |

### 2.3 Minkowski Difference Support
Given shapes $A$ and $B$, the Minkowski Difference is defined as:

$$A \ominus B = \{ a - b \mid a \in A, b \in B \}$$

Two convex shapes intersect if and only if the origin is contained within their Minkowski Difference:

$$\mathbf{0} \in A \ominus B \iff A \cap B \neq \emptyset$$

The support function of the Minkowski Difference decomposes linearly:

$$S_{A \ominus B}(\vec{d}) = S_A(\vec{d}) - S_B(-\vec{d})$$

Every Minkowski support query returns both the combined difference point $v = a - b$ and the individual shape witness vertices $a \in A$ and $b \in B$.

---

## 3. Gilbert-Johnson-Keerthi (GJK) Algorithm in 3D

The 3D GJK algorithm tracks an evolving simplex of up to 4 vertices within $A \ominus B$ to evaluate whether the origin $\mathbf{0}$ is enclosed, or to compute the Euclidean distance and closest witness points between disjoint shapes.

### 3.1 Simplex Evolution & Voronoi Decomposition
The simplex consists of 1 to 4 vertices:
- **0-Simplex (Point $A$)**: Closest point is $A$; search direction is $\vec{d} = -A$.
- **1-Simplex (Segment $AB$)**: Orthogonal projection of origin onto segment. If $t \in (0, 1)$, $\vec{d} = (AB \times AO) \times AB$.
- **2-Simplex (Triangle $ABC$)**: Evaluates Voronoi edge normals $n_{AB} = AB \times (AB \times AC)$ and $n_{AC} = (AB \times AC) \times AC$. If the origin projects inside the triangle, simplex is retained and search direction is set to triangle face normal $\pm (AB \times AC)$.
- **3-Simplex (Tetrahedron $ABCD$)**: Tests origin against the outward normals of all 4 triangular faces. If the origin is in the negative half-space of all 4 faces, the origin is enclosed.

### 3.2 Dual Termination Criteria

```
                        [ Start: dir = CenterA - CenterB ]
                                        |
                                        v
                            [ Query w = S_{A-B}(dir) ]
                                        |
                                        v
                 +---------------------------------------------+
                 | Compute Closest Point P to Origin on Simplex|
                 +---------------------------------------------+
                                        |
                 +----------------------+----------------------+
                 |                                             |
         [ ||P|| < tolerance ]                         [ ||P|| >= tolerance ]
                 |                                             |
                 v                                             v
     +-----------------------+                    [ Search dir d = -P / ||P|| ]
     | Origin is Enclosed!   |                                 |
     | Intersect = TRUE      |                    [ Query w = S_{A-B}(d) ]
     +-----------------------+                                 |
                 |                                [ Delta = ||P|| + w . d ]
                 v                                             |
        [ Proceed to EPA ]                        +------------+------------+
                                                  |                         |
                                         [ Delta <= tolerance ]   [ Delta > tolerance ]
                                                  |                         |
                                                  v                         v
                                      +------------------------+   [ Push w into Simplex ]
                                      | Disjoint Convergence!  |   [ Reduce to Subsimplex]
                                      | Distance = ||P||       |            |
                                      | Normal = -d            |            +---> Loop
                                      | Witnesses P_A, P_B     |
                                      +------------------------+
```

1. **Intersection Detection**: When the distance $\|P\|$ of the closest point to the origin drops below `tolerance` ($10^{-4}$), or when a 3-simplex tetrahedron encloses the origin, GJK terminates with `intersect = true`.
2. **Disjoint Distance Convergence**: In distance tracking mode, the supporting plane at $w$ provides a strict lower bound on distance:
   $$\Delta = \|P\| + (w \cdot \hat{d})$$
   When $\Delta \le \text{tolerance}$, no point in $A \ominus B$ can be closer to the origin than $P$. The algorithm terminates with exact separation distance $\|P\|$, separating normal $\hat{n} = P / \|P\|$, and interpolated witness points $P_A, P_B$.

---

## 4. Expanding Polytope Algorithm (EPA) in 3D

When GJK detects an intersection, the origin $\mathbf{0}$ lies inside $A \ominus B$. GJK alone cannot compute penetration depth. EPA expands the terminal polytope outwards towards the boundary of $A \ominus B$ to extract:
1. Minimum penetration depth scalar $\delta$.
2. Authoritative contact normal $\hat{\mathbf{n}}$ (pointing from Shape B to Shape A).
3. Contact witness points $P_A$ and $P_B$ satisfying $P_A - P_B = \delta \hat{\mathbf{n}}$.

### 4.1 Cardinal Octahedron Initialization
When GJK terminates, the origin may lie on an edge or vertex of the terminal simplex, causing initial faces to have zero distance from the origin ($d_f = 0$).

To prevent degenerate zero-distance traps, Rubikon initializes EPA using 6 cardinal extremal support queries:
$$\vec{d}_0 = (+1, 0, 0), \ \vec{d}_1 = (-1, 0, 0), \ \vec{d}_2 = (0, +1, 0), \ \vec{d}_3 = (0, -1, 0), \ \vec{d}_4 = (0, 0, +1), \ \vec{d}_5 = (0, 0, -1)$$

These 6 vertices form a regular octahedron with 8 triangular faces that strictly encloses the origin with positive distances $d_f > 0$.

### 4.2 Horizon Edge Detection & Polytope Expansion
In each EPA iteration:
1. **Find Closest Face**: Identify face $F_{\min}$ with minimum distance to origin:
   $$d_{\min} = \min_{f} (\hat{n}_f \cdot v_{f, 0})$$
2. **Support Query**: Query Minkowski difference support in direction of outward normal:
   $$w = S_{A \ominus B}(\hat{n}_{\min})$$
3. **Convergence Check**:
   $$w \cdot \hat{n}_{\min} - d_{\min} < \text{tolerance}$$
   If satisfied, the face lies on the boundary of $A \ominus B$. Terminate.
4. **Horizon Reconstruction**:
   Find all faces visible from $w$:
   $$\hat{n}_f \cdot (w - v_{f, 0}) > 0$$
   Collect boundary edges belonging to exactly one visible face (the "horizon").
   Remove visible faces and construct new triangular faces connecting each horizon edge to $w$.

### 4.3 Barycentric Contact Witness Points
The origin projects onto face $F_{\min}$ at $P_{\text{proj}} = d_{\min} \hat{n}_{\min}$. Barycentric coordinates $(u, v, w)$ are calculated such that:

$$P_{\text{proj}} = u v_0 + v v_1 + w v_2, \quad u + v + w = 1$$

The authoritative contact points on the two colliding bodies are computed via identical barycentric weights:

$$P_A = u a_0 + v a_1 + w a_2$$
$$P_B = u b_0 + v b_1 + w b_2$$

$$\vec{P}_A - \vec{P}_B = d_{\min} \hat{n}_{\min}$$

---

## 5. Continuous Collision Detection (CCD) via Conservative Advancement

At supersonic velocities, discrete frame-by-frame integration results in tunneling (an entity moves completely through a thin wall or another player between ticks).

Rubikon employs **Conservative Advancement** for continuous collision detection:

```
  Tick Start (t = 0)                                                    Tick End (t = 1)
  Shape A(0) ------> [ Safe Advancement: Delta t = (d - eps) / v_n ] ------> Shape A(1)
       |
       |  Separation Distance d(t)
       v
  Shape B (Stationary)
```

### 5.1 Formulation
1. Relative linear velocity: $\vec{v}_{rel} = \vec{v}_A - \vec{v}_B$.
2. At time $t$, evaluate disjoint distance $d(t)$ and separating unit normal $\hat{n}(t)$ (from B to A) via GJK.
3. Compute approach velocity along the separating normal:
   $$v_n = -\vec{v}_{rel} \cdot \hat{n}(t)$$
4. If $v_n \le 0$, the objects are moving apart or parallel; no collision occurs during this interval.
5. If $v_n > 0$, the maximum safe time step that guarantees non-penetration is:
   $$\Delta t = \frac{d(t) - \varepsilon_{\text{contact}}}{v_n \cdot \Delta t_{\text{frame}}}$$
6. Advance $t \leftarrow t + \Delta t$. If $d(t) \le \varepsilon_{\text{contact}}$, collision is confirmed at Time of Impact $\text{TOI} = t$.

---

## 6. Morton 3D Space-Filling BVH Acceleration

Broadphase collision culling in Source 2 transforms 3D AABB bounding boxes into 30-bit Morton codes (Z-order space-filling curve).

### 6.1 Coordinate Quantization & Bit Interleaving
Centroid coordinates $(x, y, z)$ within the world bounding box $[W_{\min}, W_{\max}]$ are normalized to 10-bit integers $[0, 1023]$:

$$q_x = \text{clamp}\left( \frac{x - W_{\min}.x}{W_{\max}.x - W_{\min}.x} \times 1023, 0, 1023 \right)$$

Bit expansion interleaves bits such that bit $i$ is shifted to position $3i$:
- $X$ bits occupy positions $0, 3, 6, 9, \dots$
- $Y$ bits occupy positions $1, 4, 7, 10, \dots$
- $Z$ bits occupy positions $2, 5, 8, 11, \dots$

$$\text{Morton3D}(x, y, z) = \text{Expand10}(x) \mid (\text{Expand10}(y) \ll 1) \mid (\text{Expand10}(z) \ll 2)$$

Primitives sorted by Morton code maintain 3D spatial locality in linear memory, optimizing cache line utilization during SIMD BVH traversal.

### 6.2 48-byte SIMD BVH Node Structure
Matching `FUN_18000f4e0` in `vphysics2.dll`:

```cpp
struct alignas(16) RubikonBvhNode_t {
    Vector3 minBounds;        // +0x00 AABB bounding min (12 bytes)
    uint32_t leftChild;       // +0x0C Left child index (0xFFFFFFFF = leaf) (4 bytes)
    Vector3 maxBounds;        // +0x10 AABB bounding max (12 bytes)
    uint32_t rightChild;      // +0x1C Right child index (0xFFFFFFFF = leaf) (4 bytes)
    uint32_t flags;           // +0x20 Node flags (0 = internal, 1 = leaf) (4 bytes)
    uint32_t primitiveIndex;  // +0x24 Leaf primitive payload index (4 bytes)
    uint32_t pad[2];          // +0x28 8-byte padding for 16-byte SIMD alignment
};
static_assert(sizeof(RubikonBvhNode_t) == 48);
```

---

## 7. Multi-Layer Collision Filtering Matrix

Rubikon evaluates collision feasibility through a two-tier predicate before dispatching GJK:

```mermaid
flowchart TD
    Pair["Collision Candidate Pair (Entity A, Entity B)"]
    MaskCheck{"Tier 1: Bitmask Match? (maskA & maskB) != 0"}
    GroupCheck{"Tier 2: Collision Group Rules?"}
    Narrowphase["Dispatch GJK Narrowphase"]
    Ignore["Ignore / Filter Out"]

    Pair --> MaskCheck
    MaskCheck -- No --> Ignore
    MaskCheck -- Yes --> GroupCheck
    GroupCheck -- Debris vs Debris --> Ignore
    GroupCheck -- Player vs Debris --> Ignore
    GroupCheck -- Allow --> Narrowphase
```

### 7.1 Authoritative Bitmasks
```cpp
namespace CollisionFlags {
    constexpr uint32_t CONTENTS_SOLID     = 0x1;
    constexpr uint32_t CONTENTS_WINDOW    = 0x2;
    constexpr uint32_t CONTENTS_GRATE     = 0x8;
    constexpr uint32_t CONTENTS_MOVEABLE  = 0x4000;
    constexpr uint32_t CONTENTS_PLAYERCLIP= 0x10000;
    constexpr uint32_t CONTENTS_MONSTER   = 0x2000000;
    constexpr uint32_t CONTENTS_DEBRIS    = 0x4000000;
    constexpr uint32_t CONTENTS_HITBOX    = 0x40000000;

    constexpr uint32_t MASK_SOLID         = (CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_GRATE);
    constexpr uint32_t MASK_PLAYERSOLID   = (MASK_SOLID | CONTENTS_PLAYERCLIP);
    constexpr uint32_t MASK_SHOT          = (CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_MONSTER | CONTENTS_WINDOW | CONTENTS_DEBRIS | CONTENTS_HITBOX);
}
```

---

## 8. Verification & Standalone Simulator Execution

The complete collision architecture is verified in [`docs/simulators/cs2_rubikon_collision_sim.cpp`](simulators/cs2_rubikon_collision_sim.cpp) with zero third-party dependencies.

### Compilation
```cmd
cl /std:c++20 /O2 /EHsc /W4 /WX docs/simulators/cs2_rubikon_collision_sim.cpp
```

### Verified Test Telemetry
```
================================================================================
 Source 2 Rubikon Physics & Continuous Collision Verification Suite
 Target: vphysics2.dll (Base: 0x180000000) | Image Size: 4,829,848 bytes
 Standard: Pure ISO C++20 | MSVC x64 (/std:c++20 /O2 /EHsc /W4 /WX)
================================================================================

[TEST 1] GJK Disjoint Convex Distance & Closest Witness Evaluation
   Sphere A: Center=(-15, 0, 0), Radius=5.0
   Sphere B: Center=( 15, 0, 0), Radius=5.0
   GJK Intersect: FALSE | Iterations: 2
   Separation Distance: 20.0000 units (Expected: 20.0000)
   Closest Point A: (-10.0000, 0.0000, 0.0000)
   Closest Point B: (10.0000, 0.0000, 0.0000)
   Separating Normal (B->A): (-1.0000, 0.0000, 0.0000)
   [PASS] GJK disjoint distance & closest witness points verified!

[TEST 2] GJK Convex Intersection Detection Across Polyhedral Shapes
   Spheres Overlap GJK: TRUE (Pass) | Iterations: 2
   OBB-Capsule Overlap GJK: TRUE (Pass) | Iterations: 3
   Hull-OBB Overlap GJK: TRUE (Pass) | Iterations: 2
   [PASS] GJK simplex solver accurately isolates interior origin!

[TEST 3] Expanding Polytope Algorithm (EPA) Penetration Depth Extraction
   EPA Status: CONVERGED | Iterations: 48
   Penetration Depth: 1.9998 units (Analytical: 2.0000)
   Contact Normal (B->A): (-0.9999, 0.0093, -0.0093) (Expected: -1, 0, 0)
   Witness Point A: (4.9998, -0.0093, 0.0093)
   Witness Point B: (3.0002, 0.0093, -0.0093)
   [PASS] EPA edge horizon expansion extracted exact minimum penetration vector!

[TEST 4] Continuous Collision Detection (CCD) via Conservative Advancement
   Moving Capsule: Start=(-500, 0, 0), V=(+2000, 0, 0), R=16
   Target Capsule: Pos=(0, 0, 0), V=(0, 0, 0), R=16
   Discrete Tunnel Check: Pos(t=1) = +1500 (Full Tunneling Miss)
   CCD Hit: TRUE | Iterations: 3
   Time of Impact (TOI): 0.2340 (Analytical: 0.2340)
   Contact Normal: (1.0000, -0.0000, -0.0000)
   Contact Point: (-15.9905, 0.0000, 0.0000)
   [PASS] Continuous collision root-finding eliminated supersonic tunnel effect!

[TEST 5] Morton-Coded (Z-Order) 3D BVH Broadphase Spatial Acceleration
   Primitives Ingested: 32
   BVH Total Tree Nodes: 63
   World Bounds: Min=(-420.0000, -220.0000, -20.0000) Max=(320.0000, 120.0000, 70.0000)
   AABB Query Box: [-220..-180, -120..-80, -30..30]
   BVH Overlap Hits: 1 primitives: #10 
   Raycast: Start=(-400, -400, 0), Delta=(0, 400, 0)
   Raycast Hit: TRUE | Frac: 0.4500 | HitPoint: (-400.0000, -220.0000, 0.0000)
   [PASS] 3D Morton BVH spatial tree acceleration verified with zero false negatives!

[TEST 6] Rubikon Content Bitmasks & Multi-Layer Collision Filtering
   Player vs World Solid: ALLOW (Expected: ALLOW)
   Player vs Debris:      FILTERED (Expected: FILTERED)
   Debris vs Debris:      FILTERED (Expected: FILTERED)
   Shot Query vs Hitbox:  ALLOW (Expected: ALLOW)
   [PASS] Rubikon multi-tier collision filtering matrix matches engine specs!
================================================================================
 VERIFICATION RESULTS: 6 / 6 Tests Passed (100% SUCCESS)
================================================================================
```
