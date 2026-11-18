/**
 * Authoritative Mathematical Reference Anchors
 * Extracted directly from C++20 reference simulators and disassembly:
 * - Sim 03 Sub-Tick Prediction & UserCmd wire codec
 * - Sim 09 Skeleton Hierarchy, Two-Bone IK & SLERP
 * - Sim 11 Volumetric Smoke, 3D Morton & DDA raymarch
 * - Sim 12 Lag Compensation 64-tick ring buffer & teleport breaker
 * - Sim 14 Bullet Penetration ODE, Ricochet gating & Armor jump table
 */

'use strict';

const TICK_INTERVAL = 0.015625; // 64 Hz (15.625 ms)
const JUMP_IMPULSE = 301.993; // u/s
const GRAVITY = 800.0; // u/s^2
const SV_FRICTION = 5.2;
const SV_STOPSPEED = 100.0;
const SV_MAXUNLAG = 0.200; // 200 ms
const TELEPORT_DIST_SQR = 4096.0; // 64 u displacement threshold
const VOXEL_GRID_DIM = 32;
const VOXEL_CELL_SIZE = 20.0;
const VOXEL_EXTENT = 640.0;
const SMOKE_OCCLUSION_THRESHOLD = 0.20;
const SMOKE_CARVE_RADIUS = 30.0;
const SMOKE_BLAST_CLEAR_RADIUS = 180.0;
const SMOKE_BLAST_PUSH_RADIUS = 260.0;
const RICOCHET_MAX_GRAZING_ANGLE_RAD = (15.0 * Math.PI) / 180.0;

const MATERIALS = {
  Wood: { penModifier: 3.0, damageModifier: 0.16 },
  Cardboard: { penModifier: 3.0, damageModifier: 0.10 },
  Plastic: { penModifier: 2.0, damageModifier: 0.16 },
  Concrete: { penModifier: 1.0, damageModifier: 0.22 },
  SolidMetal: { penModifier: 1.0, damageModifier: 0.20 },
  Glass: { penModifier: 3.0, damageModifier: 0.05 },
  Flesh: { penModifier: 1.0, damageModifier: 0.16 }
};

// ============================================================================
// SIM 03: SUB-TICK PREDICTION & USERCMD CODEC
// ============================================================================

function applyGroundFriction(vx, vy, dt, sv_friction = SV_FRICTION, sv_stopspeed = SV_STOPSPEED, surfaceFriction = 1.0) {
  const speed = Math.hypot(vx, vy);
  if (speed < 0.1) {
    return { vx: 0.0, vy: 0.0, speed: 0.0 };
  }

  const safeDt = Math.max(0.0, dt);
  const control = speed < sv_stopspeed ? sv_stopspeed : speed;
  const drop = control * sv_friction * surfaceFriction * safeDt;
  const newspeed = Math.max(0.0, speed - drop);

  if (newspeed !== speed) {
    const frac = newspeed / speed;
    return { vx: vx * frac, vy: vy * frac, speed: newspeed };
  }
  return { vx, vy, speed };
}

function integrateSubtickSlices(slices, initialOrigin, initialVelocity, onGround, options = {}) {
  const gravity = options.gravity !== undefined ? options.gravity : GRAVITY;
  const sv_friction = options.friction !== undefined ? options.friction : SV_FRICTION;
  const sv_stopspeed = options.stopspeed !== undefined ? options.stopspeed : SV_STOPSPEED;
  const surfaceFriction = options.surfaceFriction !== undefined ? options.surfaceFriction : 1.0;

  // Deduplicate and clamp sorted slices in [0.0, 1.0]
  const cleanSlices = [];
  const rawW = slices.map(s => (typeof s === 'number' ? s : s.when));
  const sortedW = Array.from(new Set([0.0, ...rawW, 1.0].map(w => Math.max(0.0, Math.min(1.0, w))))).sort((a, b) => a - b);

  for (let i = 0; i < sortedW.length - 1; i++) {
    const w0 = sortedW[i];
    const w1 = sortedW[i + 1];
    const dt = (w1 - w0) * TICK_INTERVAL;
    if (dt > 1e-9) {
      cleanSlices.push({ w0, w1, dt });
    }
  }

  let origin = { ...initialOrigin };
  let velocity = { ...initialVelocity };
  let ground = onGround;

  for (const slice of cleanSlices) {
    // Check if jump impulse is actuated in this slice
    const jumpActuated = slices.some(s => typeof s === 'object' && s.jump && Math.abs(s.when - slice.w0) < 1e-5);
    if (jumpActuated && ground) {
      velocity.z = JUMP_IMPULSE;
      ground = false;
    }

    if (ground) {
      const fric = applyGroundFriction(velocity.x, velocity.y, slice.dt, sv_friction, sv_stopspeed, surfaceFriction);
      velocity.x = fric.vx;
      velocity.y = fric.vy;
    } else {
      // Airborne gravity integration
      velocity.z -= gravity * slice.dt;
    }

    origin.x += velocity.x * slice.dt;
    origin.y += velocity.y * slice.dt;
    origin.z += velocity.z * slice.dt;

    if (origin.z <= 0.0 && velocity.z <= 0.0) {
      origin.z = 0.0;
      velocity.z = 0.0;
      ground = true;
    }
  }

  return { origin, velocity, onGround: ground, sliceCount: cleanSlices.length };
}

function computeJumpTrajectory(w, totalTicks) {
  const steps = [];
  let z = 0.0;
  let vz = 0.0;
  let ground = true;

  for (let tick = 0; tick < totalTicks; tick++) {
    if (tick === 0) {
      const tPre = w * TICK_INTERVAL;
      const tPost = (1.0 - w) * TICK_INTERVAL;
      vz = JUMP_IMPULSE;
      ground = false;
      vz -= GRAVITY * tPost;
      z += JUMP_IMPULSE * tPost - 0.5 * GRAVITY * tPost * tPost;
    } else {
      vz -= GRAVITY * TICK_INTERVAL;
      z += vz * TICK_INTERVAL;
      if (z <= 0.0) {
        z = 0.0;
        vz = 0.0;
        ground = true;
      }
    }
    steps.push({ tick, z: Math.max(0.0, z), vz, onGround: ground });
  }

  const apexTime = JUMP_IMPULSE / GRAVITY;
  const apexHeight = (JUMP_IMPULSE * JUMP_IMPULSE) / (2.0 * GRAVITY);
  return { steps, apexTime, apexHeight };
}

function encodeProtobufVarint(value) {
  const bytes = [];
  let val = value >>> 0;
  while (val >= 0x80) {
    bytes.push((val & 0x7f) | 0x80);
    val >>>= 7;
  }
  bytes.push(val & 0x7f);
  return bytes;
}

function decodeProtobufVarint(bytes, offset = 0) {
  let val = 0;
  let shift = 0;
  let cursor = offset;
  while (cursor < bytes.length) {
    const byte = bytes[cursor++];
    val |= (byte & 0x7f) << shift;
    if ((byte & 0x80) === 0) break;
    shift += 7;
  }
  return { value: val, bytesRead: cursor - offset };
}

// ============================================================================
// SIM 09: SKELETON TWO-BONE IK & SLERP
// ============================================================================

class CTransformMath {
  constructor(pos = { x: 0, y: 0, z: 0 }, rot = { x: 0, y: 0, z: 0, w: 1 }, scale = 1.0) {
    this.pos = { ...pos };
    this.rot = { ...rot };
    this.scale = scale;
  }

  multiply(local) {
    // Rodrigues vector rotation: R_p(x_l)
    const q = this.rot;
    const v = local.pos;
    const s = this.scale;

    const ix = q.w * v.x + q.y * v.z - q.z * v.y;
    const iy = q.w * v.y + q.z * v.x - q.x * v.z;
    const iz = q.w * v.z + q.x * v.y - q.y * v.x;
    const iw = -q.x * v.x - q.y * v.y - q.z * v.z;

    const rx = ix * q.w + iw * -q.x + iy * -q.z - iz * -q.y;
    const ry = iy * q.w + iw * -q.y + iz * -q.x - ix * -q.z;
    const rz = iz * q.w + iw * -q.z + ix * -q.y - iy * -q.x;

    const outPos = {
      x: this.pos.x + rx * s,
      y: this.pos.y + ry * s,
      z: this.pos.z + rz * s
    };

    // Quaternion multiplication: q_p * q_l
    const q1 = this.rot;
    const q2 = local.rot;
    const outRot = {
      x: q1.x * q2.w + q1.w * q2.x + q1.y * q2.z - q1.z * q2.y,
      y: q1.y * q2.w + q1.w * q2.y + q1.z * q2.x - q1.x * q2.z,
      z: q1.z * q2.w + q1.w * q2.z + q1.x * q2.y - q1.y * q2.x,
      w: q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z
    };

    return new CTransformMath(outPos, outRot, this.scale * local.scale);
  }
}

function topologicalForwardKinematics(boneHierarchy, localTransforms) {
  const worldBones = [];
  for (let i = 0; i < boneHierarchy.length; i++) {
    const parentIdx = boneHierarchy[i].parentIndex;
    const local = localTransforms[i] || new CTransformMath();
    if (parentIdx < 0 || parentIdx >= i) {
      worldBones[i] = new CTransformMath(local.pos, local.rot, local.scale);
    } else {
      worldBones[i] = worldBones[parentIdx].multiply(local);
    }
  }
  return worldBones;
}

function solveTwoBoneIK(rootPos, targetPos, L1, L2, poleVec = { x: 0, y: 1, z: 0 }) {
  const dx = targetPos.x - rootPos.x;
  const dy = targetPos.y - rootPos.y;
  const dz = targetPos.z - rootPos.z;
  const targetDist = Math.sqrt(dx * dx + dy * dy + dz * dz);
  let dist = targetDist;

  const maxReach = L1 + L2;
  const minReach = Math.max(0.001, Math.abs(L1 - L2) * 1.0001);
  let reachedTarget = true;

  if (dist > maxReach) {
    dist = maxReach * 0.9999;
    reachedTarget = false;
  } else if (dist < minReach) {
    dist = minReach;
    reachedTarget = false;
  }

  const dir = targetDist < 1e-6 ? { x: 0, y: 0, z: 1 } : {
    x: dx / targetDist,
    y: dy / targetDist,
    z: dz / targetDist
  };

  // Law of Cosines: cos(alpha) = (L1^2 + dist^2 - L2^2) / (2 * L1 * dist)
  const cosAlpha = Math.max(-1.0, Math.min(1.0, (L1 * L1 + dist * dist - L2 * L2) / (2.0 * L1 * dist)));
  const alpha = Math.acos(cosAlpha);
  const sinAlpha = Math.sin(alpha);

  // Form orthogonal basis using pole vector
  let px = poleVec.x, py = poleVec.y, pz = poleVec.z;
  const dotP = px * dir.x + py * dir.y + pz * dir.z;
  px -= dotP * dir.x;
  py -= dotP * dir.y;
  pz -= dotP * dir.z;
  let pLen = Math.sqrt(px * px + py * py + pz * pz);
  if (pLen < 1e-4) {
    // Fallback perpendicular axis
    px = -dir.y;
    py = dir.x;
    pz = 0.0;
    pLen = Math.sqrt(px * px + py * py);
    if (pLen < 1e-4) {
      px = 0.0;
      py = -dir.z;
      pz = dir.y;
      pLen = Math.sqrt(py * py + pz * pz) || 1.0;
    }
  }
  px /= pLen;
  py /= pLen;
  pz /= pLen;

  const midPos = {
    x: rootPos.x + dir.x * (L1 * cosAlpha) + px * (L1 * sinAlpha),
    y: rootPos.y + dir.y * (L1 * cosAlpha) + py * (L1 * sinAlpha),
    z: rootPos.z + dir.z * (L1 * cosAlpha) + pz * (L1 * sinAlpha)
  };

  const effectorPos = {
    x: rootPos.x + dir.x * dist,
    y: rootPos.y + dir.y * dist,
    z: rootPos.z + dir.z * dist
  };

  return { midPos, effectorPos, reachedTarget, cosAlpha, angleRad: alpha };
}

function quaternionSlerp(q0, q1, t) {
  let cosHalfTheta = q0.w * q1.w + q0.x * q1.x + q0.y * q1.y + q0.z * q1.z;
  let q1x = q1.x, q1y = q1.y, q1z = q1.z, q1w = q1.w;

  if (cosHalfTheta < 0) {
    q1w = -q1w;
    q1x = -q1x;
    q1y = -q1y;
    q1z = -q1z;
    cosHalfTheta = -cosHalfTheta;
  }

  if (cosHalfTheta >= 0.9995) {
    const w = q0.w + t * (q1w - q0.w);
    const x = q0.x + t * (q1x - q0.x);
    const y = q0.y + t * (q1y - q0.y);
    const z = q0.z + t * (q1z - q0.z);
    const len = Math.hypot(w, x, y, z) || 1.0;
    return { x: x / len, y: y / len, z: z / len, w: w / len };
  }

  const halfTheta = Math.acos(cosHalfTheta);
  const sinHalfTheta = Math.sqrt(1.0 - cosHalfTheta * cosHalfTheta);
  const ratioA = Math.sin((1 - t) * halfTheta) / sinHalfTheta;
  const ratioB = Math.sin(t * halfTheta) / sinHalfTheta;

  return {
    x: q0.x * ratioA + q1x * ratioB,
    y: q0.y * ratioA + q1y * ratioB,
    z: q0.z * ratioA + q1z * ratioB,
    w: q0.w * ratioA + q1w * ratioB
  };
}

function intersectRayCapsule(rayOrigin, rayDir, capA, capB, radius) {
  const u = rayDir;
  const v = { x: capB.x - capA.x, y: capB.y - capA.y, z: capB.z - capA.z };
  const w0 = { x: rayOrigin.x - capA.x, y: rayOrigin.y - capA.y, z: rayOrigin.z - capA.z };
  const a = u.x * u.x + u.y * u.y + u.z * u.z;
  const c = v.x * v.x + v.y * v.y + v.z * v.z;

  if (c < 1e-6) {
    // Zero-length capsule degenerates to sphere at capA
    const s = Math.max(0.0, -(u.x * w0.x + u.y * w0.y + u.z * w0.z) / (a || 1.0));
    const p = { x: rayOrigin.x + u.x * s, y: rayOrigin.y + u.y * s, z: rayOrigin.z + u.z * s };
    const dist = Math.hypot(p.x - capA.x, p.y - capA.y, p.z - capA.z);
    return { hit: dist <= radius, closestDist: dist, rayDistance: s, segmentFraction: 0.0 };
  }

  const b = u.x * v.x + u.y * v.y + u.z * v.z;
  const d = u.x * w0.x + u.y * w0.y + u.z * w0.z;
  const e = v.x * w0.x + v.y * w0.y + v.z * w0.z;
  const D = a * c - b * b;
  let sN, sD = D, tN, tD = D;
  if (D < 1e-6) {
    sN = 0.0; sD = 1.0; tN = e; tD = c;
  } else {
    sN = b * e - c * d;
    tN = a * e - b * d;
    if (sN < 0.0) { sN = 0.0; tN = e; tD = c; }
  }
  if (tN < 0.0) {
    tN = 0.0;
    sN = -d < 0.0 ? 0.0 : -d;
    sD = a;
  } else if (tN > tD) {
    tN = tD;
    sN = (-d + b) < 0.0 ? 0.0 : (-d + b);
    sD = a;
  }
  const sc = Math.abs(sN) < 1e-6 ? 0.0 : sN / sD;
  const tc = Math.abs(tN) < 1e-6 ? 0.0 : tN / tD;
  const dP = {
    x: w0.x + sc * u.x - tc * v.x,
    y: w0.y + sc * u.y - tc * v.y,
    z: w0.z + sc * u.z - tc * v.z
  };
  const dist = Math.sqrt(dP.x * dP.x + dP.y * dP.y + dP.z * dP.z);
  return { hit: dist <= radius, closestDist: dist, rayDistance: sc, segmentFraction: tc };
}

// ============================================================================
// SIM 11: VOLUMETRIC SMOKE & 3D MORTON ORDER
// ============================================================================

function part1By2(x) {
  x = Math.max(0, Math.min(31, Math.floor(Number(x) || 0)));
  x = (x | (x << 8)) & 0x300f00f;
  x = (x ^ (x << 4)) & 0x30c30c3;
  x = (x ^ (x << 2)) & 0x9249249;
  return x >>> 0;
}

function encodeMorton3D(x, y, z) {
  return (part1By2(x) | (part1By2(y) << 1) | (part1By2(z) << 2)) >>> 0;
}

function decodeMorton3D(morton) {
  let x = 0, y = 0, z = 0;
  for (let i = 0; i < 5; i++) {
    x |= ((morton >> (3 * i)) & 1) << i;
    y |= ((morton >> (3 * i + 1)) & 1) << i;
    z |= ((morton >> (3 * i + 2)) & 1) << i;
  }
  return { x, y, z };
}

class VoxelGrid32 {
  constructor() {
    this.dim = 32;
    this.totalVoxels = 32768;
    this.bitmask = new BigUint64Array(512); // 512 * 64 = 32768 bits
    this.densities = new Float32Array(32768);
  }

  set(x, y, z, density = 1.0) {
    if (x < 0 || x >= 32 || y < 0 || y >= 32 || z < 0 || z >= 32) return false;
    const m = encodeMorton3D(x, y, z);
    const word = m >> 6;
    const bit = BigInt(m & 63);
    this.bitmask[word] |= 1n << bit;
    this.densities[m] = density;
    return true;
  }

  get(x, y, z) {
    if (x < 0 || x >= 32 || y < 0 || y >= 32 || z < 0 || z >= 32) return 0.0;
    const m = encodeMorton3D(x, y, z);
    const word = m >> 6;
    const bit = BigInt(m & 63);
    const active = (this.bitmask[word] & (1n << bit)) !== 0n;
    return active ? this.densities[m] : 0.0;
  }

  clear(x, y, z) {
    if (x < 0 || x >= 32 || y < 0 || y >= 32 || z < 0 || z >= 32) return false;
    const m = encodeMorton3D(x, y, z);
    const word = m >> 6;
    const bit = BigInt(m & 63);
    const wasActive = (this.bitmask[word] & (1n << bit)) !== 0n;
    this.bitmask[word] &= ~(1n << bit);
    this.densities[m] = 0.0;
    return wasActive;
  }

  countActive() {
    let count = 0;
    for (let i = 0; i < 512; i++) {
      let w = this.bitmask[i];
      while (w > 0n) {
        count += Number(w & 1n);
        w >>= 1n;
      }
    }
    return count;
  }

  populateSphere(centerVoxel = { x: 16, y: 16, z: 16 }, radiusVoxels = 12) {
    for (let x = 0; x < 32; x++) {
      for (let y = 0; y < 32; y++) {
        for (let z = 0; z < 32; z++) {
          const dSq = (x - centerVoxel.x) ** 2 + (y - centerVoxel.y) ** 2 + (z - centerVoxel.z) ** 2;
          if (dSq <= radiusVoxels * radiusVoxels) {
            this.set(x, y, z, 1.0);
          }
        }
      }
    }
  }

  raymarchDDA(startX, startY, startZ, endX, endY, endZ) {
    const dx = endX - startX, dy = endY - startY, dz = endZ - startZ;
    const len = Math.hypot(dx, dy, dz);
    if (len < 1e-4) return 0.0;

    const stepSize = 10.0; // half voxel
    const steps = Math.min(128, Math.floor(len / stepSize));
    let accumulatedDensity = 0.0;

    for (let i = 0; i <= steps; i++) {
      const t = i / (steps || 1);
      const wx = startX + dx * t;
      const wy = startY + dy * t;
      const wz = startZ + dz * t;

      // Map world coords (centered at origin, -320 to +320) to voxel grid (0 to 31)
      const vx = Math.floor((wx + 320.0) / 20.0);
      const vy = Math.floor((wy + 320.0) / 20.0);
      const vz = Math.floor((wz + 320.0) / 20.0);

      const d = this.get(vx, vy, vz);
      accumulatedDensity += d * 0.5 * 0.12;

      if (accumulatedDensity >= SMOKE_OCCLUSION_THRESHOLD) {
        return 1.0; // Full occlusion early exit
      }
    }

    return accumulatedDensity / SMOKE_OCCLUSION_THRESHOLD;
  }

  carveCylinder(p0, p1, radiusUnits = SMOKE_CARVE_RADIUS) {
    let clearedCount = 0;

    for (let x = 0; x < 32; x++) {
      for (let y = 0; y < 32; y++) {
        for (let z = 0; z < 32; z++) {
          const wx = (x - 16) * 20.0 + 10.0;
          const wy = (y - 16) * 20.0 + 10.0;
          const wz = (z - 16) * 20.0 + 10.0;

          // Perpendicular distance to bullet segment p0-p1
          const segX = p1.x - p0.x, segY = p1.y - p0.y, segZ = p1.z - p0.z;
          const segLenSq = segX * segX + segY * segY + segZ * segZ || 1.0;
          const t = Math.max(0.0, Math.min(1.0, ((wx - p0.x) * segX + (wy - p0.y) * segY + (wz - p0.z) * segZ) / segLenSq));
          const cx = p0.x + segX * t, cy = p0.y + segY * t, cz = p0.z + segZ * t;
          const distSq = (wx - cx) ** 2 + (wy - cy) ** 2 + (wz - cz) ** 2;

          if (distSq <= radiusUnits * radiusUnits) {
            if (this.clear(x, y, z)) clearedCount++;
          }
        }
      }
    }
    return clearedCount;
  }
}

// ============================================================================
// SIM 12: LAG COMPENSATION 64-TICK CIRCULAR BUFFER & TELEPORT BREAKER
// ============================================================================

class LagCompHistoryBuffer {
  constructor(capacity = 64) {
    this.capacity = capacity;
    this.records = [];
  }

  push(record) {
    this.records.push({ ...record });
    if (this.records.length > this.capacity) {
      this.records.shift();
    }
  }

  querySubTick(targetTime, maxUnlag = SV_MAXUNLAG) {
    if (this.records.length === 0) return null;
    const newest = this.records[this.records.length - 1];
    const oldest = this.records[0];

    // Clamp within sv_maxunlag
    const clampedTarget = Math.max(newest.time - maxUnlag, Math.min(newest.time, targetTime));

    if (clampedTarget <= oldest.time) return { ...oldest, interpolated: false };
    if (clampedTarget >= newest.time) return { ...newest, interpolated: false };

    // Find bracketing records
    let older = oldest;
    let newer = newest;
    for (let i = 0; i < this.records.length - 1; i++) {
      if (this.records[i].time <= clampedTarget && this.records[i + 1].time >= clampedTarget) {
        older = this.records[i];
        newer = this.records[i + 1];
        break;
      }
    }

    const dt = newer.time - older.time;
    const frac = dt > 1e-6 ? (clampedTarget - older.time) / dt : 0.0;

    // Check teleport breaker: distSq >= 4096.0 u^2
    const dx = newer.origin.x - older.origin.x;
    const dy = newer.origin.y - older.origin.y;
    const dz = newer.origin.z - older.origin.z;
    const distSq = dx * dx + dy * dy + dz * dz;

    if (distSq >= TELEPORT_DIST_SQR) {
      // Teleport breaker TRIPS: snap to discrete frame without interpolation
      return frac >= 0.5 ? { ...newer, interpolated: false, teleportTripped: true } : { ...older, interpolated: false, teleportTripped: true };
    }

    // Continuous linear interpolation
    return {
      time: clampedTarget,
      origin: {
        x: older.origin.x + dx * frac,
        y: older.origin.y + dy * frac,
        z: older.origin.z + dz * frac
      },
      interpolated: true,
      teleportTripped: false
    };
  }
}

// ============================================================================
// SIM 14: BULLET PENETRATION, RICOCHET & ARMOR JUMP TABLE
// ============================================================================

function calculateDistanceFalloff(baseDamage, rangeModifier, distance) {
  if (distance > 8192.0) return 0.0;
  return baseDamage * Math.pow(rangeModifier, distance / 500.0);
}

function calculateRubikonDissipation(thickness, penModifier, penPower, currentDamage, damageModifier) {
  const penModInv = Math.max(0.0, 1.0 / (penModifier || 1.0));
  const lostDistance = (thickness * thickness * penModInv) / 24.0;
  const penPowerTerm = Math.max(0.0, (3.0 / (penPower || 1.0)) * 1.25);
  const lostEntry = penPowerTerm * (penModInv * 3.0) + damageModifier * currentDamage;
  const totalLost = lostDistance + lostEntry;
  const exitDamage = Math.max(0.0, currentDamage - totalLost);
  return { lostDistance, lostEntry, totalLost, exitDamage: exitDamage < 1.0 ? 0.0 : exitDamage };
}

function evaluateRicochet(incidentDir, surfaceNormal, currentDamage, criticalAngleDeg = 15.0) {
  const dot = incidentDir.x * surfaceNormal.x + incidentDir.y * surfaceNormal.y + incidentDir.z * surfaceNormal.z;
  const grazingAngleRad = Math.asin(Math.abs(dot));
  const critRad = (criticalAngleDeg * Math.PI) / 180.0;

  if (grazingAngleRad < critRad) {
    // Ricochet occurs: reflect vector and apply 50% kinetic damping
    const rx = incidentDir.x - 2.0 * dot * surfaceNormal.x;
    const ry = incidentDir.y - 2.0 * dot * surfaceNormal.y;
    const rz = incidentDir.z - 2.0 * dot * surfaceNormal.z;
    return {
      ricochet: true,
      reflectedDir: { x: rx, y: ry, z: rz },
      newDamage: currentDamage * 0.5,
      grazingAngleDeg: (grazingAngleRad * 180.0) / Math.PI
    };
  }
  return {
    ricochet: false,
    reflectedDir: null,
    newDamage: currentDamage,
    grazingAngleDeg: (grazingAngleRad * 180.0) / Math.PI
  };
}

function calculateArmorMitigation(incomingDamage, weaponArmorRatio, hitGroup, currentArmor, hasHelmet = false) {
  // HitGroups: 1: Head, 2: Chest, 3: Stomach, 4: LeftArm, 5: RightArm, 6: LeftLeg, 7: RightLeg
  const isHead = hitGroup === 1;
  const isLeg = hitGroup === 6 || hitGroup === 7;

  let isArmored = currentArmor > 0;
  if (isLeg) isArmored = false;
  if (isHead && !hasHelmet) isArmored = false;

  if (!isArmored) {
    return { healthDamage: incomingDamage, armorAbsorbed: 0, remainingArmor: currentArmor };
  }

  const armorRatio = weaponArmorRatio * 0.5;
  const nominalHealthDamage = incomingDamage * armorRatio;
  const nominalArmorDamage = (incomingDamage - nominalHealthDamage) * 0.5;

  if (nominalArmorDamage > currentArmor) {
    // Exhaustion overflow: armor breaks, remaining damage passes to health
    const healthDamage = incomingDamage - currentArmor / 0.5;
    return { healthDamage, armorAbsorbed: currentArmor, remainingArmor: 0 };
  }

  return {
    healthDamage: nominalHealthDamage,
    armorAbsorbed: Math.floor(nominalArmorDamage),
    remainingArmor: currentArmor - Math.floor(nominalArmorDamage)
  };
}

module.exports = {
  TICK_INTERVAL,
  JUMP_IMPULSE,
  GRAVITY,
  SV_FRICTION,
  SV_STOPSPEED,
  SV_MAXUNLAG,
  TELEPORT_DIST_SQR,
  VOXEL_GRID_DIM,
  VOXEL_CELL_SIZE,
  VOXEL_EXTENT,
  SMOKE_OCCLUSION_THRESHOLD,
  SMOKE_CARVE_RADIUS,
  SMOKE_BLAST_CLEAR_RADIUS,
  SMOKE_BLAST_PUSH_RADIUS,
  RICOCHET_MAX_GRAZING_ANGLE_RAD,
  MATERIALS,
  applyGroundFriction,
  integrateSubtickSlices,
  computeJumpTrajectory,
  encodeProtobufVarint,
  decodeProtobufVarint,
  CTransformMath,
  topologicalForwardKinematics,
  solveTwoBoneIK,
  quaternionSlerp,
  intersectRayCapsule,
  part1By2,
  encodeMorton3D,
  decodeMorton3D,
  VoxelGrid32,
  LagCompHistoryBuffer,
  calculateDistanceFalloff,
  calculateRubikonDissipation,
  evaluateRicochet,
  calculateArmorMitigation
};
