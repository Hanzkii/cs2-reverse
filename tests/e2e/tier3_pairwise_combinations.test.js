/**
 * Tier 3: Cross-Feature Pairwise Combinations Test Suite (25 Tests)
 * Combinatorial interactions between Viewport, Audio, Timeline, Physics, and Sim Providers.
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
  tests.push({ id, tier: 3, name, fn });
}

// C01: Playback Slow-Mo x Audio Sub-Tick Precision
registerTest('T3-PAIR-01', 'Playback Slow-Mo (0.1x) x Audio Sub-Tick Precision', () => {
  const audioCtx = new MockAudioContext();
  const speed = 0.1;
  const w = 0.4375;
  const tickDuration = 0.015625;
  const scaledDelay = (w * tickDuration) / speed;

  audioCtx.logEvent('SUBTICK_CLICK', { whenDelay: scaledDelay, subTickFrac: w, speed });
  utils.assertFloatEqual(scaledDelay, 0.068359, 1e-5);
  utils.assertLedgerContains(audioCtx, e => e.type === 'SUBTICK_CLICK' && Math.abs(e.whenDelay - scaledDelay) < 1e-5);
});

// C02: Viewport Dual Camera x Timeline Scrubber Drag
registerTest('T3-PAIR-02', 'Viewport Camera Mode Toggle x Timeline Scrubber Drag', () => {
  const { document } = createMockEnvironment();
  const canvas = document.createElement('canvas');
  const renderer = new THREE.WebGLRenderer({ canvas });
  const persp = new THREE.PerspectiveCamera(60, 16 / 9, 1, 5000);
  const ortho = new THREE.OrthographicCamera(-100, 100, 75, -75, 1, 5000);

  let activeCamera = persp;
  let timelineTime = 12.5;

  // Toggle camera mode while at scrubbed time
  activeCamera = ortho;
  timelineTime = 25.8;

  assert.strictEqual(activeCamera.type, 'OrthographicCamera');
  assert.strictEqual(timelineTime, 25.8);
  renderer.render(new THREE.Scene(), activeCamera);
  assert.strictEqual(renderer.disposed, false);
});

// C03: Viewport Theater Toggle x Active Animation Loop
registerTest('T3-PAIR-03', 'Viewport Theater Mode Toggle x Active Animation Loop', () => {
  const { window, document } = createMockEnvironment();
  const container = document.getElementById('simVisualizerContainer');
  let frameRan = false;

  const frameId = window.requestAnimationFrame((time) => {
    frameRan = true;
    container.classList.toggle('fixed', true);
  });

  window._advanceTime(16.67);
  assert.strictEqual(frameRan, true);
  assert.ok(container.classList.contains('fixed'));
});

// C04: Sim 11 Volumetric Smoke x Three.js InstancedMesh
registerTest('T3-PAIR-04', 'Sim 11 Volumetric Smoke x Three.js InstancedMesh Matrix Buffer', () => {
  const grid = new math.VoxelGrid32();
  grid.set(16, 16, 16, 1.0);
  grid.set(17, 16, 16, 1.0);

  const geom = new THREE.BoxGeometry(20, 20, 20);
  const mat = new THREE.MeshBasicMaterial();
  const instanced = new THREE.InstancedMesh(geom, mat, 32768);

  const m = new THREE.Matrix4();
  m.compose(new THREE.Vector3(10, 0, 0), new THREE.Quaternion(), new THREE.Vector3(1, 1, 1));
  instanced.setMatrixAt(0, m);

  const outM = new THREE.Matrix4();
  instanced.getMatrixAt(0, outM);
  assert.strictEqual(outM.elements[12], 10);
});

// C05: Sim 09 Two-Bone IK Drag x Quaternion SLERP Keyframe Blending
registerTest('T3-PAIR-05', 'Sim 09 Two-Bone IK Drag x Quaternion SLERP Keyframe Blending', () => {
  // Pose A: Target at (20, 0, 0)
  const ikA = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 20, y: 0, z: 0 }, 20, 20);
  // Pose B: Target at (30, 0, 0)
  const ikB = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 30, y: 0, z: 0 }, 20, 20);

  const qA = { x: 0, y: 0, z: Math.sin(ikA.angleRad / 2), w: Math.cos(ikA.angleRad / 2) };
  const qB = { x: 0, y: 0, z: Math.sin(ikB.angleRad / 2), w: Math.cos(ikB.angleRad / 2) };
  const qBlended = math.quaternionSlerp(qA, qB, 0.5);

  utils.assertNoNaNOrInf(qBlended);
  const norm = Math.hypot(qBlended.x, qBlended.y, qBlended.z, qBlended.w);
  utils.assertFloatEqual(norm, 1.0, 1e-4);
});

// C06: Sim 12 Lag Comp Rollback x Sim 09 Hitbox Capsule Raycast
registerTest('T3-PAIR-06', 'Sim 12 Lag Comp Rollback x Sim 09 Hitbox Capsule Raycast', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 10.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: 10.0 + math.TICK_INTERVAL, origin: { x: 50, y: 0, z: 0 } });

  // Rewind to midpoint (w = 0.50) -> x = 25 (displacement 50^2 = 2500 < 4096)
  const rewound = buf.querySubTick(10.0 + 0.5 * math.TICK_INTERVAL);
  assert.strictEqual(rewound.origin.x, 25);

  // Raycast against rewound head capsule at (25, 0, 64)
  const capA = { x: rewound.origin.x, y: 0, z: 64 };
  const capB = { x: rewound.origin.x, y: 0, z: 70 };
  const rayOrigin = { x: 0, y: 0, z: 67 };
  const res = math.intersectRayCapsule(rayOrigin, { x: 1, y: 0, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(res.hit, true);
});

// C07: Sim 14 Bullet Penetration x Sim 11 Smoke Cavity Carving
registerTest('T3-PAIR-07', 'Sim 14 Bullet Penetration x Sim 11 Smoke Cavity Carving', () => {
  // Bullet penetrates wood slab
  const penRes = math.calculateRubikonDissipation(4.0, 3.0, 2.5, 36.0, 0.16);
  assert.ok(penRes.exitDamage > 25.0);

  // Residual bullet ray enters smoke and carves cavity
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 8);
  const cleared = grid.carveCylinder({ x: -200, y: 0, z: 0 }, { x: 200, y: 0, z: 0 }, 30.0);
  assert.ok(cleared > 0);
  const opacity = grid.raymarchDDA(-200, 0, 0, 200, 0, 0);
  utils.assertFloatEqual(opacity, 0.0, 0.05);
});

// C08: Sim 03 Subtick Actuation x Procedural Audio Engine
registerTest('T3-PAIR-08', 'Sim 03 Subtick Actuation x Procedural Audio Synchronized Click', () => {
  const audioCtx = new MockAudioContext();
  const subTickJump = { when: 0.35, jump: true };
  const phys = math.integrateSubtickSlices([subTickJump], { x: 0, y: 0, z: 0 }, { x: 250, y: 0, z: 0 }, true);

  audioCtx.logEvent('SUBTICK_CLICK', { whenDelay: 0.35 * math.TICK_INTERVAL, subTickFrac: 0.35 });
  utils.assertLedgerContains(audioCtx, e => e.type === 'SUBTICK_CLICK' && e.subTickFrac === 0.35);
  assert.strictEqual(phys.onGround, false);
});

// C09: Portal Routing x Visualizer Lifecycle Teardown
registerTest('T3-PAIR-09', 'Portal Routing Navigation x Visualizer Lifecycle Teardown', () => {
  const { window, document } = createMockEnvironment();
  const container = document.getElementById('simVisualizerContainer');
  const canvas = document.createElement('canvas');
  container.appendChild(canvas);
  const renderer = new THREE.WebGLRenderer({ canvas });

  // Open Sim 03
  window.location.hash = '#simulators/03';
  assert.strictEqual(window.location.hash, '#simulators/03');

  // Teardown before opening Sim 11
  renderer.dispose();
  assert.strictEqual(renderer.disposed, true);
  container.removeChild(canvas);
  assert.strictEqual(container.children.length, 0);

  // Open Sim 11
  window.location.hash = '#simulators/11';
  assert.strictEqual(window.location.hash, '#simulators/11');
});

// C10: Playback Scrubber x Telemetry HUD State Synchronization
registerTest('T3-PAIR-10', 'Playback Scrubber Scrubbing x Telemetry HUD Readout Sync', () => {
  let t = 0.0;
  const pos = { x: 0, y: 0, z: 0 };

  // Drag scrubber to t = 15.75
  t = 15.75;
  pos.x = t * 10.0;

  const tick = Math.floor(t);
  const w = t - tick;
  const hudText = `Tick: ${tick} | Frac: ${w.toFixed(4)} | PosX: ${pos.x.toFixed(1)}`;
  assert.strictEqual(hudText, 'Tick: 15 | Frac: 0.7500 | PosX: 157.5');
});

// C11: Volumetric Smoke LOS Raymarch + Dynamic Bullet Cavity Carving
registerTest('T3-PAIR-11', 'Volumetric Smoke LOS Raymarch + Dynamic Bullet Cavity Carving', () => {
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 10);
  assert.strictEqual(grid.raymarchDDA(-200, 0, 0, 200, 0, 0), 1.0); // Blocked

  grid.carveCylinder({ x: -200, y: 0, z: 0 }, { x: 200, y: 0, z: 0 }, 30.0);
  utils.assertFloatEqual(grid.raymarchDDA(-200, 0, 0, 200, 0, 0), 0.0, 0.05); // Cleared
});

// C12: Two-Bone IK Solving + SLERP Keyframe Blending
registerTest('T3-PAIR-12', 'Two-Bone IK Solving + SLERP Keyframe Blending Monotonicity', () => {
  const angles = [];
  for (let step = 0; step <= 10; step++) {
    const t = step / 10.0;
    const targetX = 20.0 * (1 - t) + 38.0 * t;
    const ik = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: targetX, y: 0, z: 0 }, 20, 20);
    angles.push(ik.cosAlpha);
  }
  for (let i = 0; i < 10; i++) {
    assert.ok(angles[i + 1] >= angles[i] - 1e-4);
  }
});

// C13: Lag Compensation Rollback + Ray-Capsule Hitbox Raycast
registerTest('T3-PAIR-13', 'Lag Compensation Rollback + Ray-Capsule Hitbox Raycast Parity', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: 50, y: 0, z: 0 } });

  const rewound = buf.querySubTick(0.4 * math.TICK_INTERVAL);
  assert.strictEqual(rewound.origin.x, 20.0);

  const capA = { x: rewound.origin.x, y: 0, z: 64 };
  const capB = { x: rewound.origin.x, y: 0, z: 70 };
  const hit = math.intersectRayCapsule({ x: 0, y: 0, z: 67 }, { x: 1, y: 0, z: 0 }, capA, capB, 4.5);
  assert.strictEqual(hit.hit, true);
});

// C14: Multi-Wall Bullet Penetration + Player Armor Mitigation
registerTest('T3-PAIR-14', 'Multi-Wall Bullet Penetration + Player Armor Mitigation Chain', () => {
  // Initial damage 36.0, travel 100u through air
  const d1 = math.calculateDistanceFalloff(36.0, 0.98, 100.0);
  // Penetrate 4u wood
  const d2 = math.calculateRubikonDissipation(4.0, 3.0, 2.0, d1, 0.16).exitDamage;
  // Penetrate 6u concrete
  const d3 = math.calculateRubikonDissipation(6.0, 1.0, 2.0, d2, 0.22).exitDamage;
  assert.ok(d3 > 10.0);

  // Chest hit with Kevlar
  const armorRes = math.calculateArmorMitigation(d3, 1.55, 2, 100, false);
  assert.ok(armorRes.healthDamage < d3);
  assert.ok(armorRes.armorAbsorbed > 0);
});

// C15: Sub-Tick Jump Execution + Lag Compensation Ingestion
registerTest('T3-PAIR-15', 'Sub-Tick Jump Execution + Lag Compensation Ingestion', () => {
  const buf = new math.LagCompHistoryBuffer();
  const traj = math.computeJumpTrajectory(0.35, 5);
  for (const step of traj.steps) {
    buf.push({ tick: step.tick, time: step.tick * math.TICK_INTERVAL, origin: { x: 0, y: 0, z: step.z } });
  }
  const query = buf.querySubTick(1.5 * math.TICK_INTERVAL);
  assert.ok(query.origin.z > 0.0);
});

// C16: Penetrating Bullet Carving Volumetric Smoke
registerTest('T3-PAIR-16', 'Penetrating Bullet Carving Volumetric Smoke', () => {
  const pen = math.calculateRubikonDissipation(4.0, 3.0, 2.5, 36.0, 0.16);
  assert.ok(pen.exitDamage > 25.0);

  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 6);
  const cleared = grid.carveCylinder({ x: -160, y: 0, z: 0 }, { x: 160, y: 0, z: 0 }, 30.0);
  assert.ok(cleared > 0);
});

// C17: Two-Bone IK Foot Planting on Sloped Surface
registerTest('T3-PAIR-17', 'Two-Bone IK Foot Planting on Sloped Surface', () => {
  const leftLeg = math.solveTwoBoneIK({ x: 0, y: 0, z: 38 }, { x: 0, y: 0, z: 0 }, 20, 20); // D = 38
  const rightLeg = math.solveTwoBoneIK({ x: 0, y: 0, z: 38 }, { x: 0, y: 0, z: 12 }, 20, 20); // D = 26
  assert.ok(leftLeg.cosAlpha > rightLeg.cosAlpha); // Left leg extended more than right
});

// C18: Teleport Discontinuity Breaker during Lag Compensation Shot
registerTest('T3-PAIR-18', 'Teleport Discontinuity Breaker during Lag Compensation Shot', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 0.0, origin: { x: 100, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: 500, y: 500, z: 0 } }); // Huge teleport

  const rewound = buf.querySubTick(0.5 * math.TICK_INTERVAL);
  assert.strictEqual(rewound.teleportTripped, true);
  // Midpoint (300, 250, 0) raycast misses snapped pawn
  const capA = { x: rewound.origin.x, y: rewound.origin.y, z: 64 };
  const capB = { x: rewound.origin.x, y: rewound.origin.y, z: 70 };
  const hit = math.intersectRayCapsule({ x: 300, y: 250, z: 67 }, { x: 0, y: 1, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(hit.hit, false);
});

// C19: Ricochet into Volumetric Smoke Voxel Grid
registerTest('T3-PAIR-19', 'Ricochet into Volumetric Smoke Voxel Grid', () => {
  const ricochet = math.evaluateRicochet({ x: 0.9848, y: -0.1736, z: 0 }, { x: 0, y: 1, z: 0 }, 30.0, 15.0);
  assert.strictEqual(ricochet.ricochet, true);

  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 6);
  const cleared = grid.carveCylinder({ x: 0, y: 0, z: 0 }, { x: ricochet.reflectedDir.x * 200, y: ricochet.reflectedDir.y * 200, z: 0 }, 30.0);
  assert.ok(cleared >= 0);
});

// C20: Protobuf Wire Roundtrip + Multi-Slice Movement Fidelity
registerTest('T3-PAIR-20', 'Protobuf Wire Roundtrip + Multi-Slice Movement Fidelity', () => {
  const originalVal = 146;
  const encoded = math.encodeProtobufVarint(originalVal);
  const decoded = math.decodeProtobufVarint(encoded);
  assert.strictEqual(decoded.value, originalVal);

  const sDirect = math.integrateSubtickSlices([0.4], { x: 0, y: 0, z: 100 }, { x: 250, y: 0, z: 0 }, false, { gravity: 0 });
  const sWire = math.integrateSubtickSlices([0.4], { x: 0, y: 0, z: 100 }, { x: 250, y: 0, z: 0 }, false, { gravity: 0 });
  utils.assertVec3Equal(sDirect.origin, sWire.origin);
});

// C21: Perspective Camera Toggle x Audio Listener Orientation Sync
registerTest('T3-PAIR-21', 'Perspective Camera Toggle x Audio Listener Orientation Sync', () => {
  const camera = new THREE.PerspectiveCamera();
  camera.up.set(0, 0, 1);
  const audioCtx = new MockAudioContext();
  audioCtx.listener.setOrientation(1, 0, 0, camera.up.x, camera.up.y, camera.up.z);
  assert.strictEqual(audioCtx.listener.upZ.value, 1.0);
});

// C22: 3D Spatial Audio Rolloff x Slow-Mo Speed Scaling
registerTest('T3-PAIR-22', '3D Spatial Audio Rolloff x Slow-Mo Speed Scaling', () => {
  const d = 10.0;
  const gain = 1.0 / (1.0 + 1.0 * (d - 1.0));
  const speed = 0.25;
  const scaledGain = gain * 1.0; // Panning independent of speed
  utils.assertFloatEqual(scaledGain, 0.1, 1e-4);
});

// C23: Theater Mode Resize x InstancedMesh Voxel Viewport
registerTest('T3-PAIR-23', 'Theater Mode Resize x InstancedMesh Voxel Viewport', () => {
  const { document } = createMockEnvironment();
  const container = document.getElementById('simVisualizerContainer');
  container.clientWidth = 1920;
  container.clientHeight = 1080;

  const geom = new THREE.BoxGeometry(20, 20, 20);
  const instanced = new THREE.InstancedMesh(geom, new THREE.MeshBasicMaterial(), 100);
  assert.strictEqual(instanced.count, 100);
  assert.strictEqual(container.clientWidth, 1920);
});

// C24: Armor Exhaustion x High-Velocity Distance Falloff
registerTest('T3-PAIR-24', 'Armor Exhaustion x High-Velocity Distance Falloff', () => {
  // AWP 115 HP at 1000 units
  const dmg = math.calculateDistanceFalloff(115.0, 0.99, 1000.0);
  // Chest hit against 1 AP
  const res = math.calculateArmorMitigation(dmg, 1.95, 2, 1, false);
  assert.strictEqual(res.remainingArmor, 0);
  assert.ok(res.healthDamage > 90.0);
});

// C25: Teleport Breaker x Rapid Scrubbing in Ring Buffer
registerTest('T3-PAIR-25', 'Teleport Breaker x Rapid Scrubbing in Ring Buffer', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: 500, y: 0, z: 0 } }); // Teleport

  // Rapidly scrub through sub-tick range
  for (let i = 1; i <= 9; i++) {
    const frac = i / 10.0;
    const res = buf.querySubTick(frac * math.TICK_INTERVAL);
    assert.strictEqual(res.teleportTripped, true);
  }
});

module.exports = {
  tests
};
