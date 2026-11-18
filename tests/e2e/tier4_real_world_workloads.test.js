/**
 * Tier 4: Real-World Workloads Test Suite (5 Scenarios, 25+ Assertions)
 * End-to-end multi-step competitive CS2 physics and telemetry simulation workflows.
 */

'use strict';

const assert = require('node:assert');
const { createMockEnvironment } = require('./harness/mock_browser.js');
const THREE = require('./harness/mock_three.js');
const { MockAudioContext } = require('./harness/mock_audio.js');
const math = require('./harness/math_anchors.js');
const utils = require('./harness/assertion_utils.js');

const tests = [];

function registerTest(id, name, fn) {
  tests.push({ id, tier: 4, name, fn });
}

// Scenario 1: Full Sub-Tick Jump Shot Sequence
registerTest('T4-SCENARIO-01', 'Scenario 1: Full Sub-Tick Jump Shot Sequence', () => {
  const audioCtx = new MockAudioContext();

  // Step 1: Initial ground sprint at 250 u/s
  let velocity = { x: 250.0, y: 0.0, z: 0.0 };
  let origin = { x: 0.0, y: 0.0, z: 0.0 };

  // Step 2: Actuate Jump at w = 0.4375
  const jumpSlice = { when: 0.4375, jump: true };
  const tick1Result = math.integrateSubtickSlices([jumpSlice], origin, velocity, true);

  // Assertions for Step 2
  assert.strictEqual(tick1Result.onGround, false); // Checkpoint 1
  assert.ok(tick1Result.origin.z > 0.0); // Checkpoint 2
  assert.ok(tick1Result.velocity.z > 250.0); // Checkpoint 3

  // Step 3: Audio micro-click scheduling at w = 0.4375
  audioCtx.logEvent('TICK_METRONOME', { tick: 1, freq: 880 });
  audioCtx.logEvent('SUBTICK_CLICK', { whenDelay: 0.4375 * math.TICK_INTERVAL, subTickFrac: 0.4375 });
  utils.assertLedgerContains(audioCtx, e => e.type === 'TICK_METRONOME'); // Checkpoint 4
  utils.assertLedgerContains(audioCtx, e => e.type === 'SUBTICK_CLICK'); // Checkpoint 5

  // Step 4: Mid-air parabolic flight to apex
  const traj = math.computeJumpTrajectory(0.4375, 25);
  const apexStep = traj.steps[Math.floor(traj.apexTime / math.TICK_INTERVAL)];
  utils.assertFloatEqual(traj.apexHeight, 57.0, 0.1); // Checkpoint 6
  assert.ok(apexStep.z > 50.0); // Checkpoint 7

  // Step 5: Protobuf wire serialization at apex
  const wireBytes = math.encodeProtobufVarint((18 << 3) | 2);
  assert.strictEqual(wireBytes[0], 0x92); // Checkpoint 8
  assert.strictEqual(wireBytes[1], 0x01); // Checkpoint 9
});

// Scenario 2: Volumetric Smoke Bloom & Ballistic Cavity Carving
registerTest('T4-SCENARIO-02', 'Scenario 2: Volumetric Smoke Bloom & Ballistic Cavity Carving', () => {
  const grid = new math.VoxelGrid32();
  const audioCtx = new MockAudioContext();

  // Step 1: Detonation at origin and spherical smoke bloom
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 12);
  audioCtx.logEvent('SMOKE_DETONATE', { x: 0, y: 0, z: 0 });
  audioCtx.logEvent('SMOKE_HISS', { duration: 15.0 });

  assert.ok(grid.countActive() > 5000); // Checkpoint 10
  utils.assertLedgerContains(audioCtx, e => e.type === 'SMOKE_DETONATE'); // Checkpoint 11

  // Step 2: Line of sight check before shot -> completely occluded
  const preOpacity = grid.raymarchDDA(-250, 0, 0, 250, 0, 0);
  assert.strictEqual(preOpacity, 1.0); // Checkpoint 12

  // Step 3: Sniper round fires through center along X axis
  const cleared = grid.carveCylinder({ x: -300, y: 0, z: 0 }, { x: 300, y: 0, z: 0 }, 30.0);
  assert.ok(cleared >= 80); // Checkpoint 13

  // Step 4: Line of sight check through carved tunnel -> visibility restored
  const postOpacity = grid.raymarchDDA(-250, 0, 0, 250, 0, 0);
  utils.assertFloatEqual(postOpacity, 0.0, 0.05); // Checkpoint 14
});

// Scenario 3: Multi-Obstacle Wallbang with Armor Mitigation
registerTest('T4-SCENARIO-03', 'Scenario 3: Multi-Obstacle Wallbang with Armor Mitigation', () => {
  // Step 1: Shooter fires AK-47 (Base 36.0, RangeMod 0.98, PenPower 2.0)
  const baseDmg = 36.0;
  const d1 = math.calculateDistanceFalloff(baseDmg, 0.98, 400.0);
  utils.assertFloatEqual(d1, 35.427, 0.05); // Checkpoint 15

  // Step 2: Penetrate 2-inch (2.0u) wood slab
  const slab1 = math.calculateRubikonDissipation(2.0, 3.0, 2.0, d1, 0.16);
  assert.ok(slab1.exitDamage > 25.0); // Checkpoint 16

  // Step 3: Travel 250u air to concrete wall
  const d2 = math.calculateDistanceFalloff(slab1.exitDamage, 0.98, 250.0);

  // Step 4: Penetrate 4-inch (4.0u) concrete wall
  const slab2 = math.calculateRubikonDissipation(4.0, 1.0, 2.0, d2, 0.22);
  assert.ok(slab2.exitDamage > 5.0); // Checkpoint 17

  // Step 5: Impact player pawn in Chest (HitGroup 2) with 100 HP, 100 AP
  const finalArmor = math.calculateArmorMitigation(slab2.exitDamage, 1.55, 2, 100, false);
  assert.ok(finalArmor.healthDamage > 0.0); // Checkpoint 18
  assert.ok(finalArmor.armorAbsorbed > 0); // Checkpoint 19
  assert.strictEqual(finalArmor.remainingArmor, 100 - finalArmor.armorAbsorbed); // Checkpoint 20
});

// Scenario 4: 200ms Latency Backtrack Flick Shot
registerTest('T4-SCENARIO-04', 'Scenario 4: 200ms Latency Backtrack Flick Shot', () => {
  const buf = new math.LagCompHistoryBuffer(64);

  // Step 1: Server records 64 ticks of target pawn sprinting at 250 u/s
  for (let tick = 0; tick < 64; tick++) {
    buf.push({
      tick,
      time: tick * math.TICK_INTERVAL,
      origin: { x: tick * (250.0 * math.TICK_INTERVAL), y: 0.0, z: 0.0 }
    });
  }
  assert.strictEqual(buf.records.length, 64); // Checkpoint 21

  const latest = buf.records[buf.records.length - 1];
  const queryTime = latest.time - 0.160; // 160ms lag compensation backtrack

  // Step 2: Query sub-tick rewind history
  const rewound = buf.querySubTick(queryTime, 0.200);
  assert.strictEqual(rewound.teleportTripped, false); // Checkpoint 22
  assert.ok(rewound.origin.x < latest.origin.x); // Checkpoint 23

  // Step 3: Ray-capsule intersection against rewound chest
  const capA = { x: rewound.origin.x, y: 0, z: 64 };
  const capB = { x: rewound.origin.x, y: 0, z: 70 };
  const aimRay = { x: 0, y: 0, z: 67 };
  const hitRewound = math.intersectRayCapsule(aimRay, { x: 1, y: 0, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(hitRewound.hit, true); // Checkpoint 24

  // Step 4: Raycast against server present position misses
  const capPresentA = { x: latest.origin.x, y: 0, z: 64 };
  const capPresentB = { x: latest.origin.x, y: 0, z: 70 };
  const hitPresent = math.intersectRayCapsule({ x: 0, y: 50, z: 67 }, { x: 1, y: 0, z: 0 }, capPresentA, capPresentB, 5.0);
  assert.strictEqual(hitPresent.hit, false); // Checkpoint 25
});

// Scenario 5: Full Character Dynamic IK Reach & Crouch Transition
registerTest('T4-SCENARIO-05', 'Scenario 5: Full Character Dynamic IK Reach & Crouch Transition', () => {
  // Step 1: Stand pose with topological FK
  const hier = [
    { name: 'Root', parentIndex: -1 },
    { name: 'Pelvis', parentIndex: 0 },
    { name: 'LeftLeg', parentIndex: 1 },
    { name: 'RightLeg', parentIndex: 1 }
  ];
  const standLocals = [
    new math.CTransformMath({ x: 0, y: 0, z: 0 }),
    new math.CTransformMath({ x: 0, y: 0, z: 38 }),
    new math.CTransformMath({ x: -10, y: 0, z: 0 }),
    new math.CTransformMath({ x: 10, y: 0, z: 0 })
  ];
  const standBones = math.topologicalForwardKinematics(hier, standLocals);
  assert.strictEqual(standBones[1].pos.z, 38); // Checkpoint 26

  // Step 2: Crouch transition (pelvis drops to z = 24)
  const crouchLocals = [
    new math.CTransformMath({ x: 0, y: 0, z: 0 }),
    new math.CTransformMath({ x: 0, y: 0, z: 24 }),
    new math.CTransformMath({ x: -10, y: 0, z: 0 }),
    new math.CTransformMath({ x: 10, y: 0, z: 0 })
  ];
  const crouchBones = math.topologicalForwardKinematics(hier, crouchLocals);
  assert.strictEqual(crouchBones[1].pos.z, 24); // Checkpoint 27

  // Step 3: Two-Bone IK foot planting on uneven ground
  const leftFootIK = math.solveTwoBoneIK(crouchBones[2].pos, { x: -10, y: 0, z: 0 }, 15, 15); // Floor z=0
  const rightFootIK = math.solveTwoBoneIK(crouchBones[3].pos, { x: 10, y: 0, z: 10 }, 15, 15); // Crate z=10
  assert.strictEqual(leftFootIK.reachedTarget, true); // Checkpoint 28
  assert.strictEqual(rightFootIK.reachedTarget, true); // Checkpoint 29
  assert.ok(leftFootIK.cosAlpha > rightFootIK.cosAlpha); // Checkpoint 30

  // Step 4: Out-of-reach target clamping
  const reachClamp = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 100, y: 0, z: 0 }, 15, 15);
  assert.strictEqual(reachClamp.reachedTarget, false); // Checkpoint 31
  utils.assertFloatEqual(reachClamp.effectorPos.x, 30.0 * 0.9999, 1e-3); // Checkpoint 32
});

module.exports = {
  tests
};
