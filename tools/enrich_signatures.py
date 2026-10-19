import json
import os

SNIPPETS_AND_EXPLANATIONS = {
    "client_angle_vectors": {
        "explanation": "Converts Euler viewing angles (Pitch, Yaw, Roll in degrees) into orthogonal 3D Cartesian basis unit vectors (Forward, Right, Up). In Source 2 x64, Valve uses SIMD vectorized sine and cosine computations with the conversion factor pi / 180.0f (0.017453292519943295f) for camera orientation and ray generation.",
        "code_snippet": """void AngleVectors(const QAngle& angles, Vector* forward, Vector* right, Vector* up) {
    constexpr float DEG2RAD = 0.017453292519943295f;
    float sp, cp, sy, cy, sr, cr;

    DirectX::XMScalarSinCos(&sp, &cp, angles.pitch * DEG2RAD);
    DirectX::XMScalarSinCos(&sy, &cy, angles.yaw   * DEG2RAD);
    DirectX::XMScalarSinCos(&sr, &cr, angles.roll  * DEG2RAD);

    if (forward) {
        forward->x = cp * cy;
        forward->y = cp * sy;
        forward->z = -sp;
    }
    if (right) {
        right->x = -1.0f * sr * sp * cy + -1.0f * cr * -sy;
        right->y = -1.0f * sr * sp * sy + -1.0f * cr * cy;
        right->z = -1.0f * sr * cp;
    }
    if (up) {
        up->x = cr * sp * cy + -sr * -sy;
        up->y = cr * sp * sy + -sr * cy;
        up->z = cr * cp;
    }
}"""
    },
    "client_calc_world_bones": {
        "explanation": "Executes hierarchical forward kinematics bone transform propagation on CSkeletonInstance. Traverses parent bone links in topological order and concatenates local joint matrices using SIMD Rodrigues quaternion multiplication and position offset addition.",
        "code_snippet": """void CSkeletonInstance::CalculateWorldBones(uint32_t boneMask) {
    CModelState* modelState = &this->m_modelState;
    CTransform* boneToWorld = modelState->m_pBoneTransforms;
    const int numBones = modelState->m_nBoneCount;

    for (int i = 0; i < numBones; ++i) {
        int parentIdx = modelState->m_pBoneParents[i];
        if (parentIdx == -1) {
            boneToWorld[i] = modelState->m_localTransforms[i];
        } else {
            // SIMD Rodrigues Quaternion concatenation + position translation
            CTransform::ConcatenateTransforms(boneToWorld[parentIdx], modelState->m_localTransforms[i], &boneToWorld[i]);
        }
    }
    this->m_nComputedBoneMask |= boneMask;
}"""
    },
    "client_concatenate_transforms": {
        "explanation": "High-performance SIMD routine concatenating two 32-byte Source 2 CTransform structures (Quaternion rot [XYZW] + Vector pos [XYZ] + padding). Rotates the child translation by the parent orientation quaternion before accumulating parent translation.",
        "code_snippet": """void CTransform::ConcatenateTransforms(const CTransform& parent, const CTransform& child, CTransform* out) {
    // 1. Quat Multiply: Q_out = Q_parent * Q_child
    __m128 qP = _mm_loadu_ps(&parent.rot.x);
    __m128 qC = _mm_loadu_ps(&child.rot.x);
    out->rot = QuatMultiplySIMD(qP, qC);

    // 2. Vector Translation: Pos_out = RotateVector(Q_parent, child.pos) + parent.pos
    Vector rotatedPos = RotateVectorQuat(parent.rot, child.pos);
    out->pos = parent.pos + rotatedPos;
}"""
    },
    "server_los_smoke": {
        "explanation": "Authoritative server-side line-of-sight check. Traverses all active CSmokeGrenadeProjectile voxel volumes between start and end coordinates, stepping through the 3D Morton-indexed voxel grid via 3D DDA raymarching. If integrated optical smoke density exceeds 0.20, line of sight is obstructed.",
        "code_snippet": """bool LineOfSightThroughSmoke(const Vector& vStart, const Vector& vEnd, float flDensityThreshold) {
    CSmokeGrenadeProjectile* pActiveSmoke = g_pSmokeListHead;
    while (pActiveSmoke) {
        if (pActiveSmoke->m_bDidSmokeDetonate && !pActiveSmoke->m_bSmokeFading) {
            float density = pActiveSmoke->RaymarchVoxelGrid(vStart, vEnd);
            if (density >= flDensityThreshold) {
                return false; // Trajectory is occluded by volumetric smoke
            }
        }
        pActiveSmoke = pActiveSmoke->m_pNext;
    }
    return true; // Clear line of sight
}"""
    },
    "server_smoke_spawn": {
        "explanation": "Initializes a detonated smoke grenade on the server. Allocates a 3,072-byte double-buffered bitmask representing a 32x32x32 voxel grid (20-unit cell spacing), initializes the particle simulation volume, and activates network delta tracking.",
        "code_snippet": """void CSmokeGrenadeProjectile::Spawn() {
    BaseClass::Spawn();
    this->m_bDidSmokeDetonate = true;
    this->m_flSpawnTime = gpGlobals->curtime;
    // Allocate 32x32x32 voxel bitmask buffer (512 uint64 words = 4096 bytes)
    this->m_pVoxelGrid = (VoxelGrid_t*)MemAlloc(sizeof(VoxelGrid_t));
    memset(this->m_pVoxelGrid->occupancy, 0, sizeof(this->m_pVoxelGrid->occupancy));
    this->ExpandInitialSphere(this->GetAbsOrigin(), 144.0f /* radius */);
}"""
    },
    "server_smoke_tick_update": {
        "explanation": "Tick-by-tick simulation loop for active volumetric smoke. Decrements smoke duration timer (18.0s), computes natural outward dissipation, checks explosive grenade shockwave radius to displace smoke voxels, and triggers SerializeSparseDelta if voxels are modified.",
        "code_snippet": """void CSmokeGrenadeProjectile::TickUpdate() {
    float flElapsed = gpGlobals->curtime - this->m_flSpawnTime;
    if (flElapsed > 18.0f) {
        this->StartFading();
        return;
    }
    // Update aerodynamic drag and blast wave displacement
    this->UpdateBlastDispersion();
    if (this->m_bGridDirty) {
        this->NetworkStateChanged();
    }
}"""
    },
    "server_voxel_raymarch": {
        "explanation": "Core 3D Digital Differential Analyzer (DDA) raymarching loop through the 32x32x32 voxel grid. Traverses Morton Z-order interleaved cells along the ray vector and integrates accumulated optical density with early exit on maximum opacity.",
        "code_snippet": """float VoxelGrid_Raymarch(const VoxelGrid_t* grid, const Vector& start, const Vector& end) {
    Vector dir = (end - start).Normalized();
    float totalDist = (end - start).Length();
    float stepSize = 10.0f; // Half voxel spacing
    float accumulatedDensity = 0.0f;

    for (float dist = 0.0f; dist < totalDist; dist += stepSize) {
        Vector pt = start + dir * dist;
        int vx = (int)(pt.x / 20.0f) & 31;
        int vy = (int)(pt.y / 20.0f) & 31;
        int vz = (int)(pt.z / 20.0f) & 31;
        uint32_t mortonIdx = Morton3D(vx, vy, vz);
        if (grid->TestBit(mortonIdx)) {
            accumulatedDensity += 0.15f * (stepSize / 20.0f);
            if (accumulatedDensity >= 1.0f) return 1.0f;
        }
    }
    return accumulatedDensity;
}"""
    },
    "server_voxel_expand_carve": {
        "explanation": "Manipulates voxel grid occupancy bitmasks for bullet penetration and grenade explosions. A bullet carve clears a cylindrical tunnel along the bullet trajectory, while HE grenade shockwaves push smoke outward radially.",
        "code_snippet": """void VoxelGrid_ExpandAndCarve(VoxelGrid_t* grid, const Vector& origin, const Vector& dir, float radius, bool isCarve) {
    // 3D Bresenham cylinder traversal
    for (int z = -2; z <= 2; ++z) {
        for (int y = -2; y <= 2; ++y) {
            for (int x = -2; x <= 2; ++x) {
                Vector cellOffset(x * 20.0f, y * 20.0f, z * 20.0f);
                if (cellOffset.Length() <= radius) {
                    uint32_t idx = Morton3DFromWorld(origin + cellOffset);
                    if (isCarve) grid->ClearBit(idx);
                    else grid->SetBit(idx);
                }
            }
        }
    }
}"""
    },
    "server_serialize_sparse_delta": {
        "explanation": "Serializes changed voxel words into a compact network bitstream. Instead of transmitting all 512 words (4,096 bytes) per tick, it writes an active dirty-word index followed by the 64-bit mask, reducing wire payload by over 80%.",
        "code_snippet": """void SerializeSparseDelta(bf_write* buf, const VoxelGrid_t* oldGrid, const VoxelGrid_t* newGrid) {
    for (int word = 0; word < 512; ++word) {
        uint64_t delta = oldGrid->words[word] ^ newGrid->words[word];
        if (delta != 0) {
            buf->WriteOneBit(1);                  // Has change flag
            buf->WriteUBitLong(word, 9);          // Word index (0..511 = 9 bits)
            buf->WriteBytes(&newGrid->words[word], 8); // 8-byte payload
        }
    }
    buf->WriteOneBit(0); // End-of-stream terminator
}"""
    },
    "server_lag_compensation": {
        "explanation": "Rolls back player hitboxes to their exact spatial coordinates at the time a client fired a shot. Computes interpolation fraction from sub-tick timestamp and samples the entity position history ring buffer.",
        "code_snippet": """void CLagCompensationManager::StartLagCompensation(CCSPlayerPawn* player, float flTargetTime) {
    for (CCSPlayerPawn* target : g_pActivePawns) {
        if (target == player || !target->IsAlive()) continue;
        // Sample tick history ring buffer at interpolated flTargetTime
        CPlayerStateRecord interpolatedRecord;
        if (target->m_pHistory->GetRecordAtTime(flTargetTime, &interpolatedRecord)) {
            target->SetupBonesFromRecord(interpolatedRecord);
        }
    }
}"""
    },
    "pulse_instruction_dispatch": {
        "explanation": "Virtual machine opcode execution loop for Valve's Pulse Visual Scripting System (pulse_system.dll). Dispatches 56-byte PGDInstruction_t instructions across 122 opcode types, manages the operand stack, and coordinates asynchronous coroutine fiber yielding.",
        "code_snippet": """PulseRuntimeState_t PulseInstructionDispatchLoop(CPulseCursor* pCursor, const PGDInstruction_t* pCode) {
    while (pCursor->m_pc < pCursor->m_nInstructionCount) {
        const PGDInstruction_t& inst = pCode[pCursor->m_pc++];
        switch (inst.opcode) {
            case OP_CALL_EXTERNAL:
                DispatchSchemaMethod(inst.targetSymbolHash, pCursor);
                break;
            case OP_YIELD:
                pCursor->m_state = PULSE_CURSOR_WAITING_ASYNC;
                return PULSE_SUSPENDED;
            case OP_JUMP_COND:
                if (pCursor->PopStackBool()) pCursor->m_pc = inst.jumpOffset;
                break;
            case OP_RETURN:
                return PULSE_COMPLETED;
        }
    }
    return PULSE_COMPLETED;
}"""
    },
    "tier0_cvar_findvar": {
        "explanation": "Hash-bucketed lookup into the Source 2 engine console variable registry (CCvar). Hashes the input variable name using caseless MurmurHash2 (seed 0x31415926) and walks the collision bucket linked list.",
        "code_snippet": """ConVar* CCvar::FindVar(const char* name) {
    uint32_t hash = HashStringCaseless(name);
    uint32_t bucket = hash % this->m_nBucketCount;
    for (ConVar* cvar = this->m_pBuckets[bucket]; cvar != nullptr; cvar = cvar->m_pNext) {
        if (cvar->m_hash == hash && _stricmp(cvar->m_pszName, name) == 0) {
            return cvar;
        }
    }
    return nullptr;
}"""
    },
    "tier0_hash_string_caseless": {
        "explanation": "Source 2 canonical caseless 32-bit MurmurHash2 implementation with seed 0x31415926. Converts characters to lowercase before mixing 4-byte chunks, ensuring consistent identifiers for schemas, cvars, and net messages.",
        "code_snippet": """uint32_t HashStringCaseless(const char* str) {
    if (!str) return 0;
    uint32_t len = (uint32_t)strlen(str);
    uint32_t h = (len ^ 0x31415926) >>> 0;
    const uint32_t m = 0x5bd1e995;
    while (len >= 4) {
        uint32_t k = (tolower(str[0])) | (tolower(str[1]) << 8) | (tolower(str[2]) << 16) | (tolower(str[3]) << 24);
        k *= m; k ^= k >> 24; k *= m;
        h *= m; h ^= k;
        str += 4; len -= 4;
    }
    // Final byte chunk mixing
    return h;
}"""
    },
    "network_read_varbit_msg_id": {
        "explanation": "Variable-bit message ID decoder from networksystem.dll. Inspects prefix bits to read 6-bit (ID 0..15), 10-bit (ID 16..255), 14-bit (ID 256..4095), or 34-bit token hashed message identifiers.",
        "code_snippet": """uint32_t ReadVariableBitMessageId(bf_read* buf) {
    if (buf->ReadOneBit() == 0) {
        return buf->ReadUBitLong(4); // Tier 0: 0..15 (6 bits total)
    }
    if (buf->ReadOneBit() == 0) {
        return buf->ReadUBitLong(8) + 16; // Tier 1: 16..255 (10 bits total)
    }
    if (buf->ReadOneBit() == 0) {
        return buf->ReadUBitLong(12) + 256; // Tier 2: 256..4095 (14 bits total)
    }
    return buf->ReadUBitLong(32); // Tier 3: 32-bit MurmurHash2 token (34 bits total)
}"""
    },
    "network_serialize_net_message": {
        "explanation": "Serializes a network message object into an outgoing UDP packet buffer. Writes the variable-bit message ID prefix header, encodes the Protobuf payload length via VarInt32, and direct-blits the serialized Protobuf byte buffer.",
        "code_snippet": """void SerializeNetMessage(bf_write* buf, uint32_t msgId, const google::protobuf::Message& msg) {
    WriteVariableBitMessageId(buf, msgId);
    uint32_t byteSize = (uint32_t)msg.ByteSizeLong();
    WriteVarInt32(buf, byteSize);
    msg.SerializeWithCachedSizesToArray(buf->GetWritePtr());
    buf->SeekToBit(buf->GetNumBitsWritten() + byteSize * 8);
}"""
    },
    "network_write_varint32": {
        "explanation": "Encodes a 32-bit unsigned integer into LEB128 variable-length format. Emits 7 payload bits per byte with bit 7 set as continuation flag, serializing small integers (0..127) in a single byte.",
        "code_snippet": """void WriteVarInt32(bf_write* buf, uint32_t val) {
    while (val >= 0x80) {
        buf->WriteUBitLong((val & 0x7F) | 0x80, 8);
        val >>= 7;
    }
    buf->WriteUBitLong(val, 8);
}"""
    },
    "network_read_varint32": {
        "explanation": "Decodes a LEB128 variable-length unsigned 32-bit integer from a network bitstream. Accumulates 7-bit chunks while bit 7 is set, aborting after 5 bytes to prevent integer overflow.",
        "code_snippet": """uint32_t ReadVarInt32(bf_read* buf) {
    uint32_t result = 0, shift = 0;
    while (shift < 35) {
        uint8_t byte = (uint8_t)buf->ReadUBitLong(8);
        result |= (byte & 0x7F) << shift;
        if (!(byte & 0x80)) return result;
        shift += 7;
    }
    return result;
}"""
    },
    "network_decode_fieldpaths": {
        "explanation": "Traverses a 40-operation Huffman prefix tree to unpack active property paths for flattened entity delta updates (CSVCMsg_PacketEntities), mapping compressed bitstream tokens to schema fields.",
        "code_snippet": """void DecodeFieldPaths(bf_read* buf, std::vector<FieldPath_t>* outPaths) {
    FieldPath_t fp = {0};
    while (true) {
        int op = ReadHuffmanOp(buf, g_pFieldPathHuffmanTree);
        if (op == OP_FINISH) break;
        ExecuteFieldPathOp(op, &fp, buf);
        outPaths->push_back(fp);
    }
}"""
    },
    "vphysics2_convex_raycast": {
        "explanation": "Raycasts against a convex polyhedron collision hull in Valve's Rubikon physics engine (vphysics2.dll). Computes entry/exit fractions across bounding half-spaces with a 1/32\" (0.03125 unit) skin tolerance.",
        "code_snippet": """bool ConvexPolyhedronRaycast(const RnPolyhedron_t* poly, const Vector& start, const Vector& delta, RaycastResult_t* result) {
    float enterFrac = 0.0f, exitFrac = 1.0f;
    Vector enterNormal = Vector(0, 0, 0);
    for (int i = 0; i < poly->m_nPlaneCount; ++i) {
        const Plane_t& plane = poly->m_pPlanes[i];
        float distStart = plane.Distance(start) - 0.03125f; // 1/32\" skin
        float distEnd = plane.Distance(start + delta) - 0.03125f;
        if (distStart > 0.0f && distEnd > 0.0f) return false; // Separating plane
        // Clip enter and exit fractions...
    }
    result->fraction = enterFrac;
    result->normal = enterNormal;
    return true;
}"""
    },
    "vphysics2_capsule_sweep": {
        "explanation": "Calculates the exact continuous collision sweep between a moving point/sphere and an oriented cylinder capsule with hemispherical endcaps in vphysics2.dll. Solves the closest point quadratic polynomial.",
        "code_snippet": """bool CapsuleSweepIntersection(const Capsule_t& capsule, const Vector& start, const Vector& delta, SweepHit_t* hit) {
    Vector segA = capsule.m_vCenter0, segB = capsule.m_vCenter1;
    float radius = capsule.m_flRadius;
    float tRay, tSeg;
    ClosestSegmentSegment(start, start + delta, segA, segB, &tRay, &tSeg);
    Vector ptRay = start + delta * tRay;
    Vector ptSeg = segA + (segB - segA) * tSeg;
    if ((ptRay - ptSeg).LengthSqr() <= radius * radius) {
        hit->fraction = tRay;
        hit->normal = (ptRay - ptSeg).Normalized();
        return true;
    }
    return false;
}"""
    },
    "animationsystem_twobone_ik": {
        "explanation": "Analytical two-bone inverse kinematics solver in animationsystem.dll. Applies the Law of Cosines to solve joint angles for player limbs (hip-to-knee or shoulder-to-elbow) toward an arbitrary IK target position.",
        "code_snippet": """bool CTwoBoneIK_LawOfCosines(float l1, float l2, float targetDist, float* outAngleA, float* outAngleB) {
    // Law of Cosines: c^2 = a^2 + b^2 - 2ab * cos(C)
    float clampedDist = std::clamp(targetDist, std::abs(l1 - l2) + 0.001f, (l1 + l2) - 0.001f);
    float cosB = (l1 * l1 + l2 * l2 - clampedDist * clampedDist) / (2.0f * l1 * l2);
    float cosA = (l1 * l1 + clampedDist * clampedDist - l2 * l2) / (2.0f * l1 * clampedDist);
    *outAngleB = std::acos(std::clamp(cosB, -1.0f, 1.0f)); // Knee / Elbow bend
    *outAngleA = std::acos(std::clamp(cosA, -1.0f, 1.0f)); // Shoulder / Hip bend
    return true;
}"""
    }
}

def main():
    target_path = os.path.join("data", "function_signatures.json")
    with open(target_path, "r", encoding="utf-8") as f:
        sigs = json.load(f)

    for s in sigs:
        sid = s.get("id")
        if sid in SNIPPETS_AND_EXPLANATIONS:
            info = SNIPPETS_AND_EXPLANATIONS[sid]
            s["explanation"] = info["explanation"]
            s["code_snippet"] = info["code_snippet"]

    with open(target_path, "w", encoding="utf-8") as f:
        json.dump(sigs, f, indent=2)

    print(f"Successfully updated {len(sigs)} signatures with explanations and code snippets.")

if __name__ == "__main__":
    main()
