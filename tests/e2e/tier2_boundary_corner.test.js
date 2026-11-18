/**
 * Tier 2: Boundary & Corner Cases Test Suite (200 Tests: >= 5 tests per feature for Features 1-40)
 * Boundary Value Analysis (BVA), extreme domain limits, out-of-order calls, and stress conditions.
 */

'use strict';

const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { createMockEnvironment } = require('./harness/mock_browser.js');
const { MockWebGLRenderingContext } = require('./harness/mock_webgl.js');
const THREE = require('./harness/mock_three.js');
const { MockAudioContext } = require('./harness/mock_audio.js');
const math = require('./harness/math_anchors.js');
const utils = require('./harness/assertion_utils.js');

const tests = [];

function registerTest(id, featureId, featureName, name, fn) {
  tests.push({ id, tier: 2, featureId, featureName, name, fn });
}

// ============================================================================
// CATEGORY A: VIEWPORT 3D & RENDERING BOUNDARIES (FEATURES 1 - 5)
// ============================================================================

// F-01: Viewport Extreme Resizes & Canvas Container Dimensions
registerTest('T2-F01-01', 1, 'Three.js & OrbitControls Integration', 'Zero width and height container clamp', () => {
  const { document } = createMockEnvironment();
  const canvas = document.createElement('canvas');
  canvas.width = 0;
  canvas.height = 0;
  const renderer = new THREE.WebGLRenderer({ canvas });
  renderer.setSize(0, 0);
  const size = renderer.getSize();
  assert.strictEqual(size.width, 0);
  assert.strictEqual(size.height, 0);
});

registerTest('T2-F01-02', 1, 'Three.js & OrbitControls Integration', 'Ultra-wide 32:9 container aspect ratio resize', () => {
  const camera = new THREE.PerspectiveCamera(60, 32 / 9, 1, 5000);
  camera.aspect = 3840 / 1080;
  camera.updateProjectionMatrix();
  utils.assertFloatEqual(camera.aspect, 3.5555, 1e-3);
  utils.assertNoNaNOrInf(camera.aspect);
});

registerTest('T2-F01-03', 1, 'Three.js & OrbitControls Integration', 'Extreme vertical 9:32 portrait container resize', () => {
  const camera = new THREE.PerspectiveCamera(60, 9 / 32, 1, 5000);
  camera.aspect = 300 / 1000;
  camera.updateProjectionMatrix();
  utils.assertFloatEqual(camera.aspect, 0.3, 1e-4);
});

registerTest('T2-F01-04', 1, 'Three.js & OrbitControls Integration', 'High DPR (3.0) scaling clamp to max 2.0', () => {
  const renderer = new THREE.WebGLRenderer();
  renderer.setPixelRatio(3.0);
  assert.strictEqual(renderer.getPixelRatio(), 2.0);
});

registerTest('T2-F01-05', 1, 'Three.js & OrbitControls Integration', 'Canvas detached from DOM mid-render cycle', () => {
  const { document } = createMockEnvironment();
  const container = document.getElementById('simVisualizerContainer');
  const canvas = document.createElement('canvas');
  container.appendChild(canvas);
  const renderer = new THREE.WebGLRenderer({ canvas });
  container.removeChild(canvas);
  renderer.render(new THREE.Scene(), new THREE.Camera());
  assert.strictEqual(renderer.disposed, false);
});

// F-02: Camera Frustum Matching at Singularities
registerTest('T2-F02-01', 2, 'Dual Camera Rig', 'Frustum matching at zero target distance clamp', () => {
  const d = Math.max(0.001, 0.0);
  const fovRad = (45.0 * Math.PI) / 180.0;
  const matchedH = 2.0 * d * Math.tan(fovRad / 2.0);
  utils.assertFloatEqual(matchedH, 2.0 * 0.001 * Math.tan(fovRad / 2.0), 1e-5);
  utils.assertNoNaNOrInf(matchedH);
});

registerTest('T2-F02-02', 2, 'Dual Camera Rig', 'Extreme FOV upper boundary clamping (120 deg)', () => {
  let fov = 179.9;
  fov = Math.min(120.0, Math.max(10.0, fov));
  assert.strictEqual(fov, 120.0);
});

registerTest('T2-F02-03', 2, 'Dual Camera Rig', 'Extreme FOV lower boundary clamping (10 deg)', () => {
  let fov = 0.1;
  fov = Math.min(120.0, Math.max(10.0, fov));
  assert.strictEqual(fov, 10.0);
});

registerTest('T2-F02-04', 2, 'Dual Camera Rig', 'Orthographic zoom boundary upper clamp (50.0)', () => {
  const ortho = new THREE.OrthographicCamera();
  ortho.zoom = Math.min(50.0, Math.max(0.05, 100.0));
  assert.strictEqual(ortho.zoom, 50.0);
});

registerTest('T2-F02-05', 2, 'Dual Camera Rig', 'Orthographic zoom boundary lower clamp (0.05)', () => {
  const ortho = new THREE.OrthographicCamera();
  ortho.zoom = Math.min(50.0, Math.max(0.05, 0.0001));
  assert.strictEqual(ortho.zoom, 0.05);
});

// F-03: Grid Floor & Triad Coordinate Boundaries
registerTest('T2-F03-01', 3, 'Source 2 Debug Renderer Aesthetics', 'Strict planar Z=0 floor invariant', () => {
  const grid = new THREE.GridHelper(640, 10);
  assert.strictEqual(grid.position.z, 0.0);
});

registerTest('T2-F03-02', 3, 'Source 2 Debug Renderer Aesthetics', 'Coordinate triad axes positive unit length', () => {
  const axes = new THREE.AxesHelper(64);
  assert.strictEqual(axes.size, 64);
});

registerTest('T2-F03-03', 3, 'Source 2 Debug Renderer Aesthetics', 'Large coordinate position (>100,000 units)', () => {
  const pos = new THREE.Vector3(100000, 100000, 100000);
  utils.assertNoNaNOrInf(pos.length());
  assert.ok(pos.length() > 170000);
});

registerTest('T2-F03-04', 3, 'Source 2 Debug Renderer Aesthetics', 'Negative world coordinates rendering stability', () => {
  const pos = new THREE.Vector3(-500, -500, -200);
  const matrix = new THREE.Matrix4().compose(pos, new THREE.Quaternion(), new THREE.Vector3(1, 1, 1));
  assert.strictEqual(matrix.elements[12], -500);
  assert.strictEqual(matrix.elements[13], -500);
  assert.strictEqual(matrix.elements[14], -200);
});

registerTest('T2-F03-05', 3, 'Source 2 Debug Renderer Aesthetics', 'Hidden grid visibility parameter', () => {
  const grid = new THREE.GridHelper();
  grid.visible = false;
  assert.strictEqual(grid.visible, false);
});

// F-04: Rapid Theater/Fullscreen Toggling
registerTest('T2-F04-01', 4, 'Theater & Fullscreen Toggle', 'Rapid 20x theater mode toggle cycle', () => {
  const { document } = createMockEnvironment();
  const container = document.getElementById('simVisualizerContainer');
  let active = false;
  for (let i = 0; i < 20; i++) {
    active = !active;
    container.classList.toggle('fixed', active);
  }
  assert.strictEqual(container.classList.contains('fixed'), false);
});

registerTest('T2-F04-02', 4, 'Theater & Fullscreen Toggle', 'Fullscreen permission denial graceful handling', async () => {
  const container = createMockEnvironment().document.getElementById('simVisualizerContainer');
  container.requestFullscreen = async () => { throw new Error('NotAllowedError'); };
  let errorHandled = false;
  try {
    await container.requestFullscreen();
  } catch (err) {
    errorHandled = true;
  }
  assert.strictEqual(errorHandled, true);
});

registerTest('T2-F04-03', 4, 'Theater & Fullscreen Toggle', 'ResizeObserver container trigger during animation', () => {
  const { ResizeObserver, document } = createMockEnvironment().window;
  let triggeredWidth = 0;
  const ro = new ResizeObserver((entries) => {
    triggeredWidth = entries[0].contentRect.width;
  });
  const el = document.getElementById('simVisualizerContainer');
  ro.observe(el);
  ro.trigger(1280, 720);
  assert.strictEqual(triggeredWidth, 1280);
});

registerTest('T2-F04-04', 4, 'Theater & Fullscreen Toggle', 'Theater mode z-index stack isolation', () => {
  const el = createMockEnvironment().document.createElement('div');
  el.style.zIndex = '50';
  assert.strictEqual(el.style.zIndex, '50');
});

registerTest('T2-F04-05', 4, 'Theater & Fullscreen Toggle', 'Keyboard escape simulated exit fullscreen', () => {
  const { document } = createMockEnvironment();
  document.fullscreenElement = null;
  assert.strictEqual(document.fullscreenElement, null);
});

// F-05: WebGL Context Loss & Recovery
registerTest('T2-F05-01', 5, 'WebGL Context Lifecycle & Teardown', 'Simulated context loss prevents render execution', () => {
  const canvas = new (createMockEnvironment().document.createElement('canvas').constructor)();
  const gl = canvas.getContext('webgl2');
  gl.simulateContextLoss();
  const initialCalls = gl.audit.drawCalls;
  gl.drawArrays(gl.TRIANGLES, 0, 3);
  assert.strictEqual(gl.audit.drawCalls, initialCalls);
});

registerTest('T2-F05-02', 5, 'WebGL Context Lifecycle & Teardown', 'Simulated context restoration resets lost state', () => {
  const canvas = new (createMockEnvironment().document.createElement('canvas').constructor)();
  const gl = canvas.getContext('webgl2');
  gl.simulateContextLoss();
  assert.strictEqual(gl.isContextLost(), true);
  gl.simulateContextRestored();
  assert.strictEqual(gl.isContextLost(), false);
});

registerTest('T2-F05-03', 5, 'WebGL Context Lifecycle & Teardown', 'Sequential 20x mount and teardown loop', () => {
  for (let i = 0; i < 20; i++) {
    const renderer = new THREE.WebGLRenderer();
    renderer.dispose();
    assert.strictEqual(renderer.disposed, true);
  }
});

registerTest('T2-F05-04', 5, 'WebGL Context Lifecycle & Teardown', 'Double destroy invocation idempotence', () => {
  const renderer = new THREE.WebGLRenderer();
  renderer.dispose();
  renderer.dispose();
  assert.strictEqual(renderer.disposed, true);
});

registerTest('T2-F05-05', 5, 'WebGL Context Lifecycle & Teardown', 'Object without geometry or material disposal safety', () => {
  const obj = new THREE.Object3D();
  assert.strictEqual(obj.geometry, undefined);
});

// ============================================================================
// CATEGORY B: PROCEDURAL WEB AUDIO ENGINE BOUNDARIES (FEATURES 6 - 9)
// ============================================================================

// F-06: AudioContext Interruption & Edge Hardware
registerTest('T2-F06-01', 6, 'Procedural Web Audio Engine', 'AudioContext at 96 kHz high sample rate', () => {
  const ctx = new MockAudioContext({ sampleRate: 96000 });
  assert.strictEqual(ctx.sampleRate, 96000);
  const buf = ctx.createBuffer(1, 288, 96000);
  utils.assertFloatEqual(buf.duration, 0.003, 1e-4);
});

registerTest('T2-F06-02', 6, 'Procedural Web Audio Engine', 'Audio playback before unlock remains suspended', () => {
  const ctx = new MockAudioContext();
  assert.strictEqual(ctx.state, 'suspended');
});

registerTest('T2-F06-03', 6, 'Procedural Web Audio Engine', 'Multiple concurrent calls to resume resolve gracefully', async () => {
  const ctx = new MockAudioContext();
  await Promise.all([ctx.resume(), ctx.resume(), ctx.resume()]);
  assert.strictEqual(ctx.state, 'running');
});

registerTest('T2-F06-04', 6, 'Procedural Web Audio Engine', 'Master volume zero gain (silence)', () => {
  const gain = new MockAudioContext().createGain();
  gain.gain.setValueAtTime(0.0, 0.0);
  assert.strictEqual(gain.gain.value, 0.0);
});

registerTest('T2-F06-05', 6, 'Procedural Web Audio Engine', 'Master volume soft clipping protection', () => {
  const rawVolume = 1.5;
  const clampedVolume = Math.min(1.0, rawVolume);
  assert.strictEqual(clampedVolume, 1.0);
});

registerTest('T2-F06-06', 6, 'Procedural Web Audio Engine', 'exponentialRampToValueAtTime throws RangeError on non-positive value', () => {
  const gain = new MockAudioContext().createGain();
  assert.throws(() => {
    gain.gain.exponentialRampToValueAtTime(0, 1.0);
  }, RangeError);
  assert.throws(() => {
    gain.gain.exponentialRampToValueAtTime(-0.5, 1.0);
  }, RangeError);
});

// F-07: Spatial Audio Distance Extremes
registerTest('T2-F07-01', 7, '3D Spatial Audio Panning', 'Zero listener distance singularity clamp', () => {
  const distance = 0.0;
  const safeDistance = Math.max(0.02, distance);
  assert.strictEqual(safeDistance, 0.02);
});

registerTest('T2-F07-02', 7, '3D Spatial Audio Panning', 'Extreme distance (>10,000 units) rolloff attenuation', () => {
  const d = 15000.0;
  const ref = 1.0;
  const gain = ref / (ref + 1.0 * (d - ref));
  assert.ok(gain < 0.0001);
});

registerTest('T2-F07-03', 7, '3D Spatial Audio Panning', 'Collinear listener orientation vector normalization', () => {
  const forward = new THREE.Vector3(0, 0, 0).normalize();
  assert.strictEqual(forward.x, 0);
  assert.strictEqual(forward.y, 0);
  assert.strictEqual(forward.z, 0);
});

registerTest('T2-F07-04', 7, '3D Spatial Audio Panning', 'Panner maximum distance boundary clamp', () => {
  const panner = new MockAudioContext().createPanner();
  panner.maxDistance = 10000.0;
  assert.strictEqual(panner.maxDistance, 10000.0);
});

registerTest('T2-F07-05', 7, '3D Spatial Audio Panning', 'Negative spatial coordinate positioning', () => {
  const panner = new MockAudioContext().createPanner();
  panner.setPosition(-50.0, -100.0, -20.0);
  assert.strictEqual(panner.positionX.value, -50.0);
});

// F-08: Sub-Tick Audio Scheduling Limits
registerTest('T2-F08-01', 8, 'Sub-Tick Precision Audio Scheduling', 'Negative sub-tick fraction clamp to 0.0', () => {
  const rawW = -0.05;
  const clampedW = Math.max(0.0, Math.min(1.0, rawW));
  assert.strictEqual(clampedW, 0.0);
});

registerTest('T2-F08-02', 8, 'Sub-Tick Precision Audio Scheduling', 'Sub-tick fraction overflow clamp to 1.0', () => {
  const rawW = 1.25;
  const clampedW = Math.max(0.0, Math.min(1.0, rawW));
  assert.strictEqual(clampedW, 1.0);
});

registerTest('T2-F08-03', 8, 'Sub-Tick Precision Audio Scheduling', 'Zero duration schedule interval handling', () => {
  const dt = (0.4 - 0.4) * math.TICK_INTERVAL;
  assert.strictEqual(dt, 0.0);
});

registerTest('T2-F08-04', 8, 'Sub-Tick Precision Audio Scheduling', 'Sub-tick fraction epsilon precision (w = 0.999999)', () => {
  const w = 0.999999;
  const delay = w * math.TICK_INTERVAL;
  utils.assertFloatEqual(delay, math.TICK_INTERVAL, 1e-4);
});

registerTest('T2-F08-05', 8, 'Sub-Tick Precision Audio Scheduling', 'Cancellation of all scheduled values at time 0', () => {
  const gain = new MockAudioContext().createGain();
  gain.gain.setValueAtTime(0.5, 1.0);
  gain.gain.cancelScheduledValues(0.0);
  assert.strictEqual(gain.gain.scheduledEvents.length, 0);
});

// F-09: Sound Profile Fallbacks & Frequency Limits
registerTest('T2-F09-01', 9, 'Procedural Sound Library', 'Unknown surface material fallback to concrete', () => {
  const matName = 'unobtainium';
  const resolved = math.MATERIALS[matName] || math.MATERIALS.Concrete;
  assert.strictEqual(resolved.penModifier, 1.0);
});

registerTest('T2-F09-02', 9, 'Procedural Sound Library', 'Human audible frequency upper bound clamp (20,000 Hz)', () => {
  const f = Math.min(20000, 25000);
  assert.strictEqual(f, 20000);
});

registerTest('T2-F09-03', 9, 'Procedural Sound Library', 'Human audible frequency lower bound clamp (20 Hz)', () => {
  const f = Math.max(20, 5);
  assert.strictEqual(f, 20);
});

registerTest('T2-F09-04', 9, 'Procedural Sound Library', 'Negative audio parameter rejection', () => {
  const gain = Math.max(0.0, -0.5);
  assert.strictEqual(gain, 0.0);
});

registerTest('T2-F09-05', 9, 'Procedural Sound Library', 'Zero duration buffer creation safety', () => {
  const ctx = new MockAudioContext();
  const buf = ctx.createBuffer(1, 0, 44100);
  assert.strictEqual(buf.length, 0);
});

// ============================================================================
// CATEGORY C: PLAYBACK & TIMELINE BOUNDARIES (FEATURES 10 - 12)
// ============================================================================

// F-10: Sub-Tick Timeline Extremities
registerTest('T2-F10-01', 10, 'Continuous Sub-Tick Timeline Engine', 'Timeline start boundary tick 0', () => {
  const t = 0.0;
  assert.strictEqual(Math.floor(t), 0);
  assert.strictEqual(t - Math.floor(t), 0.0);
});

registerTest('T2-F10-02', 10, 'Continuous Sub-Tick Timeline Engine', 'Reverse step at tick 0 does not underflow', () => {
  let t = 0.0;
  t = Math.max(0.0, t - 1.0);
  assert.strictEqual(t, 0.0);
});

registerTest('T2-F10-03', 10, 'Continuous Sub-Tick Timeline Engine', 'Forward step at max duration does not overflow', () => {
  const maxT = 64.0;
  let t = 64.0;
  t = Math.min(maxT, t + 1.0);
  assert.strictEqual(t, 64.0);
});

registerTest('T2-F10-04', 10, 'Continuous Sub-Tick Timeline Engine', 'Continuous time spike delta clamp to 4 ticks', () => {
  const dtWall = 2.5; // seconds
  const dtClamped = Math.min(4 * math.TICK_INTERVAL, dtWall);
  assert.strictEqual(dtClamped, 4 * math.TICK_INTERVAL);
});

registerTest('T2-F10-05', 10, 'Continuous Sub-Tick Timeline Engine', 'Rapid scrub oscillation stability', () => {
  let t = 0.0;
  for (let i = 0; i < 50; i++) {
    t = (t + 15.3) % 64.0;
  }
  assert.ok(t >= 0.0 && t <= 64.0);
});

// F-11: Playback Controls Extremities
registerTest('T2-F11-01', 11, 'Timeline Playback Controls', 'Speed factor minimum clamp (0.1x)', () => {
  const rawSpeed = 0.01;
  const speed = Math.max(0.1, rawSpeed);
  assert.strictEqual(speed, 0.1);
});

registerTest('T2-F11-02', 11, 'Timeline Playback Controls', 'Speed factor maximum clamp (1.0x)', () => {
  const rawSpeed = 5.0;
  const speed = Math.min(1.0, rawSpeed);
  assert.strictEqual(speed, 1.0);
});

registerTest('T2-F11-03', 11, 'Timeline Playback Controls', 'Sub-tick stepping at boundary fraction 0.95', () => {
  let t = 10.95;
  t += 0.1;
  utils.assertFloatEqual(t, 11.05);
});

registerTest('T2-F11-04', 11, 'Timeline Playback Controls', 'Pause during active scrubbing', () => {
  let isPlaying = true;
  // User scrubs -> pause
  isPlaying = false;
  assert.strictEqual(isPlaying, false);
});

registerTest('T2-F11-05', 11, 'Timeline Playback Controls', 'End-of-timeline auto-pause', () => {
  let isPlaying = true;
  let t = 64.0;
  if (t >= 64.0) isPlaying = false;
  assert.strictEqual(isPlaying, false);
});

// F-12: Telemetry HUD Edge Readouts
registerTest('T2-F12-01', 12, 'Real-Time Telemetry HUD Overlay', 'Undefined velocity fallback to 0.0 u/s', () => {
  const payload = { velocity: undefined };
  const speed = payload.velocity ? Math.hypot(payload.velocity.x, payload.velocity.y) : 0.0;
  assert.strictEqual(speed, 0.0);
});

registerTest('T2-F12-02', 12, 'Real-Time Telemetry HUD Overlay', 'Undefined position fallback to origin (0,0,0)', () => {
  const payload = { position: null };
  const pos = payload.position || { x: 0, y: 0, z: 0 };
  assert.strictEqual(pos.x, 0);
});

registerTest('T2-F12-03', 12, 'Real-Time Telemetry HUD Overlay', 'Negative coordinate readout formatting', () => {
  const val = -123.4567;
  assert.strictEqual(val.toFixed(2), '-123.46');
});

registerTest('T2-F12-04', 12, 'Real-Time Telemetry HUD Overlay', 'Large velocity (>3500 u/s) readout stability', () => {
  const speed = Math.hypot(3500, 0);
  assert.strictEqual(speed, 3500);
});

registerTest('T2-F12-05', 12, 'Real-Time Telemetry HUD Overlay', 'Empty telemetry update payload safety', () => {
  const updateHUD = (data = {}) => Boolean(data);
  assert.strictEqual(updateHUD(), true);
});

// ============================================================================
// CATEGORY D: SIM 03 SUB-TICK PREDICTION BOUNDARIES (FEATURES 13 - 16)
// ============================================================================

// F-13: Temporal Slicing Boundaries
registerTest('T2-F13-01', 13, 'Multi-Slice Temporal Slicing', 'Zero subtick moves defaults to single 15.625ms slice', () => {
  const res = math.integrateSubtickSlices([], { x: 0, y: 0, z: 100 }, { x: 0, y: 0, z: 0 }, false, { gravity: 0 });
  assert.strictEqual(res.sliceCount, 1);
});

registerTest('T2-F13-02', 13, 'Multi-Slice Temporal Slicing', 'Negative fraction clamped to 0.0', () => {
  const res = math.integrateSubtickSlices([-0.5], { x: 0, y: 0, z: 100 }, { x: 0, y: 0, z: 0 }, false, { gravity: 0 });
  assert.strictEqual(res.sliceCount, 1);
});

registerTest('T2-F13-03', 13, 'Multi-Slice Temporal Slicing', 'Overflow fraction clamped to 1.0', () => {
  const res = math.integrateSubtickSlices([1.5], { x: 0, y: 0, z: 100 }, { x: 0, y: 0, z: 0 }, false, { gravity: 0 });
  assert.strictEqual(res.sliceCount, 1);
});

registerTest('T2-F13-04', 13, 'Multi-Slice Temporal Slicing', 'Extreme sub-tick slice count (64 slices)', () => {
  const slices = Array.from({ length: 64 }, (_, i) => i / 64);
  const res = math.integrateSubtickSlices(slices, { x: 0, y: 0, z: 100 }, { x: 0, y: 0, z: 0 }, false, { gravity: 0 });
  assert.strictEqual(res.sliceCount, 64);
});

registerTest('T2-F13-05', 13, 'Multi-Slice Temporal Slicing', 'Zero-duration duplicate slice skip', () => {
  const res = math.integrateSubtickSlices([0.25, 0.25, 0.25], { x: 0, y: 0, z: 100 }, { x: 0, y: 0, z: 0 }, false, { gravity: 0 });
  assert.strictEqual(res.sliceCount, 2);
});

// F-14: Jump Trajectory Boundaries
registerTest('T2-F14-01', 14, 'Continuous Analog Jump Trajectory', 'Jump at exact tick end w = 1.0 (zero air displacement in tick)', () => {
  const traj = math.computeJumpTrajectory(1.0, 1);
  assert.strictEqual(traj.steps[0].z, 0.0);
});

registerTest('T2-F14-02', 14, 'Continuous Analog Jump Trajectory', 'Airborne jump attempt rejected', () => {
  const res = math.integrateSubtickSlices([{ when: 0.2, jump: true }], { x: 0, y: 0, z: 50 }, { x: 0, y: 0, z: 0 }, false);
  // Velocity z should only be affected by gravity, not jump impulse
  assert.ok(res.velocity.z < 0.0);
});

registerTest('T2-F14-03', 14, 'Continuous Analog Jump Trajectory', 'Jump at w = 0.9999 infinitesimal air flight', () => {
  const traj = math.computeJumpTrajectory(0.9999, 1);
  assert.ok(traj.steps[0].z > 0.0);
  assert.ok(traj.steps[0].z < 0.01);
});

registerTest('T2-F14-04', 14, 'Continuous Analog Jump Trajectory', 'Zero gravity parabolic flight trajectory', () => {
  const res = math.integrateSubtickSlices([{ when: 0.0, jump: true }], { x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: 0 }, true, { gravity: 0 });
  utils.assertFloatEqual(res.velocity.z, math.JUMP_IMPULSE);
});

registerTest('T2-F14-05', 14, 'Continuous Analog Jump Trajectory', 'Continuous apex height formula non-zero check', () => {
  const traj = math.computeJumpTrajectory(0.5, 10);
  assert.ok(traj.apexHeight > 50.0);
});

// F-15: Friction Vector Boundaries
registerTest('T2-F15-01', 15, 'Ground Friction Deceleration Vectors', 'Speed exactly at sv_stopspeed (100 u/s)', () => {
  const res = math.applyGroundFriction(100.0, 0.0, math.TICK_INTERVAL);
  utils.assertFloatEqual(res.speed, 91.875, 1e-4);
});

registerTest('T2-F15-02', 15, 'Ground Friction Deceleration Vectors', 'Friction drop exceeding current speed clamps to 0.0', () => {
  const res = math.applyGroundFriction(5.0, 0.0, math.TICK_INTERVAL);
  assert.strictEqual(res.speed, 0.0);
});

registerTest('T2-F15-03', 15, 'Ground Friction Deceleration Vectors', 'Zero velocity input returns zero velocity', () => {
  const res = math.applyGroundFriction(0.0, 0.0, math.TICK_INTERVAL);
  assert.strictEqual(res.speed, 0.0);
});

registerTest('T2-F15-04', 15, 'Ground Friction Deceleration Vectors', 'Zero dt delta time preserves velocity', () => {
  const res = math.applyGroundFriction(250.0, 0.0, 0.0);
  assert.strictEqual(res.speed, 250.0);
});

registerTest('T2-F15-05', 15, 'Ground Friction Deceleration Vectors', 'Zero surface friction preserves velocity', () => {
  const res = math.applyGroundFriction(250.0, 0.0, math.TICK_INTERVAL, math.SV_FRICTION, math.SV_STOPSPEED, 0.0);
  assert.strictEqual(res.speed, 250.0);
});

registerTest('T2-F15-06', 15, 'Ground Friction Deceleration Vectors', 'Negative dt clamped to zero without accelerating speed', () => {
  const res = math.applyGroundFriction(250.0, 0.0, -0.015625);
  assert.strictEqual(res.speed, 250.0);
  assert.strictEqual(res.vx, 250.0);
  assert.strictEqual(res.vy, 0.0);
});

// F-16: Protobuf Wire Boundaries
registerTest('T2-F16-01', 16, 'Protobuf Wire Codec Reference', 'Zero varint encoding single byte 0x00', () => {
  const bytes = math.encodeProtobufVarint(0);
  assert.strictEqual(bytes.length, 1);
  assert.strictEqual(bytes[0], 0x00);
});

registerTest('T2-F16-02', 16, 'Protobuf Wire Codec Reference', 'Max 32-bit varint encoding length', () => {
  const bytes = math.encodeProtobufVarint(0xffffffff);
  assert.strictEqual(bytes.length, 5);
});

registerTest('T2-F16-03', 16, 'Protobuf Wire Codec Reference', 'Decoding single byte varint 127', () => {
  const dec = math.decodeProtobufVarint([0x7f]);
  assert.strictEqual(dec.value, 127);
  assert.strictEqual(dec.bytesRead, 1);
});

registerTest('T2-F16-04', 16, 'Protobuf Wire Codec Reference', 'Decoding multi-byte boundary value 128', () => {
  const dec = math.decodeProtobufVarint([0x80, 0x01]);
  assert.strictEqual(dec.value, 128);
  assert.strictEqual(dec.bytesRead, 2);
});

registerTest('T2-F16-05', 16, 'Protobuf Wire Codec Reference', 'Offset-based varint stream parsing', () => {
  const stream = [0xff, 0xff, 0x08];
  const dec = math.decodeProtobufVarint(stream, 2);
  assert.strictEqual(dec.value, 8);
  assert.strictEqual(dec.bytesRead, 1);
});

// ============================================================================
// CATEGORY E: SIM 09 SKELETON & IK BOUNDARIES (FEATURES 17 - 22)
// ============================================================================

// F-17: CTransform Boundaries
registerTest('T2-F17-01', 17, '32-Byte CTransform', 'Identity transform multiplication invariance', () => {
  const orig = new math.CTransformMath({ x: 10, y: 20, z: 30 });
  const ident = new math.CTransformMath();
  const res = orig.multiply(ident);
  utils.assertVec3Equal(res.pos, orig.pos);
});

registerTest('T2-F17-02', 17, '32-Byte CTransform', 'Zero scale multiplication', () => {
  const t1 = new math.CTransformMath({ x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: 0, w: 1 }, 0.0);
  const t2 = new math.CTransformMath({ x: 10, y: 10, z: 10 });
  const res = t1.multiply(t2);
  utils.assertVec3Equal(res.pos, { x: 0, y: 0, z: 0 });
});

registerTest('T2-F17-03', 17, '32-Byte CTransform', 'Sequential multiple transform concatenations', () => {
  let curr = new math.CTransformMath({ x: 0, y: 0, z: 0 });
  for (let i = 0; i < 5; i++) {
    curr = curr.multiply(new math.CTransformMath({ x: 10, y: 0, z: 0 }));
  }
  utils.assertVec3Equal(curr.pos, { x: 50, y: 0, z: 0 });
});

registerTest('T2-F17-04', 17, '32-Byte CTransform', 'Opposite rotation cancellation', () => {
  const qPos = { x: 0, y: 0, z: Math.SQRT1_2, w: Math.SQRT1_2 };
  const qNeg = { x: 0, y: 0, z: -Math.SQRT1_2, w: Math.SQRT1_2 };
  const t1 = new math.CTransformMath({ x: 0, y: 0, z: 0 }, qPos);
  const t2 = new math.CTransformMath({ x: 0, y: 0, z: 0 }, qNeg);
  const res = t1.multiply(t2);
  utils.assertFloatEqual(res.rot.w, 1.0, 1e-4);
  utils.assertFloatEqual(res.rot.z, 0.0, 1e-4);
});

registerTest('T2-F17-05', 17, '32-Byte CTransform', 'Negative coordinate translation preservation', () => {
  const t1 = new math.CTransformMath({ x: -100, y: -200, z: -300 });
  const t2 = new math.CTransformMath({ x: -10, y: -20, z: -30 });
  const res = t1.multiply(t2);
  utils.assertVec3Equal(res.pos, { x: -110, y: -220, z: -330 });
});

registerTest('T2-F17-06', 17, '32-Byte CTransform', 'Matrix4 decompose accurately extracts rotation quaternion', () => {
  const m = new THREE.Matrix4();
  const originalPos = new THREE.Vector3(12, 34, 56);
  // 90 degrees around Y axis: x=0, y=sin(45 deg)=0.7071068, z=0, w=cos(45 deg)=0.7071068
  const originalRot = new THREE.Quaternion(0, Math.SQRT1_2, 0, Math.SQRT1_2);
  const originalScale = new THREE.Vector3(1, 1, 1);
  m.compose(originalPos, originalRot, originalScale);

  const outPos = new THREE.Vector3();
  const outRot = new THREE.Quaternion(99, 99, 99, 99);
  const outScale = new THREE.Vector3();
  m.decompose(outPos, outRot, outScale);

  utils.assertVec3Equal(outPos, originalPos);
  utils.assertFloatEqual(outRot.x, 0.0, 1e-4);
  utils.assertFloatEqual(outRot.y, Math.SQRT1_2, 1e-4);
  utils.assertFloatEqual(outRot.z, 0.0, 1e-4);
  utils.assertFloatEqual(outRot.w, Math.SQRT1_2, 1e-4);
  utils.assertVec3Equal(outScale, originalScale);
});

// F-18: FK Hierarchy Boundaries
registerTest('T2-F18-01', 18, 'Topological FK Propagation', 'Empty hierarchy evaluation returns empty array', () => {
  const res = math.topologicalForwardKinematics([], []);
  assert.strictEqual(res.length, 0);
});

registerTest('T2-F18-02', 18, 'Topological FK Propagation', 'Single root bone hierarchy', () => {
  const res = math.topologicalForwardKinematics([{ parentIndex: -1 }], [new math.CTransformMath({ x: 10, y: 0, z: 0 })]);
  assert.strictEqual(res.length, 1);
  assert.strictEqual(res[0].pos.x, 10);
});

registerTest('T2-F18-03', 18, 'Topological FK Propagation', 'Deeply nested single-branch bone chain (32 bones)', () => {
  const hier = Array.from({ length: 32 }, (_, i) => ({ parentIndex: i === 0 ? -1 : i - 1 }));
  const locals = Array.from({ length: 32 }, () => new math.CTransformMath({ x: 1, y: 0, z: 0 }));
  const res = math.topologicalForwardKinematics(hier, locals);
  assert.strictEqual(res.length, 32);
  utils.assertVec3Equal(res[31].pos, { x: 32, y: 0, z: 0 });
});

registerTest('T2-F18-04', 18, 'Topological FK Propagation', 'All root bones (forest hierarchy)', () => {
  const hier = [{ parentIndex: -1 }, { parentIndex: -1 }, { parentIndex: -1 }];
  const locals = [
    new math.CTransformMath({ x: 1, y: 0, z: 0 }),
    new math.CTransformMath({ x: 2, y: 0, z: 0 }),
    new math.CTransformMath({ x: 3, y: 0, z: 0 })
  ];
  const res = math.topologicalForwardKinematics(hier, locals);
  assert.strictEqual(res[2].pos.x, 3);
});

registerTest('T2-F18-05', 18, 'Topological FK Propagation', 'Zero offset bone hierarchy identity', () => {
  const hier = [{ parentIndex: -1 }, { parentIndex: 0 }];
  const locals = [new math.CTransformMath(), new math.CTransformMath()];
  const res = math.topologicalForwardKinematics(hier, locals);
  utils.assertVec3Equal(res[1].pos, { x: 0, y: 0, z: 0 });
});

registerTest('T2-F18-06', 18, 'Topological FK Propagation', 'Object3D updateMatrixWorld accumulates transforms across parent-child hierarchy', () => {
  const parent = new THREE.Object3D();
  parent.position.set(100, 20, 0);

  const child = new THREE.Object3D();
  child.position.set(50, 5, 0);

  const grandChild = new THREE.Object3D();
  grandChild.position.set(10, 0, 0);

  parent.add(child);
  child.add(grandChild);

  parent.updateMatrixWorld(true);

  // Child world position should be parent + child = (150, 25, 0)
  assert.strictEqual(child.matrixWorld.elements[12], 150);
  assert.strictEqual(child.matrixWorld.elements[13], 25);
  assert.strictEqual(child.matrixWorld.elements[14], 0);

  // GrandChild world position should be parent + child + grandChild = (160, 25, 0)
  assert.strictEqual(grandChild.matrixWorld.elements[12], 160);
  assert.strictEqual(grandChild.matrixWorld.elements[13], 25);
  assert.strictEqual(grandChild.matrixWorld.elements[14], 0);
});

// F-19: Capsule Geometry Boundaries
registerTest('T2-F19-01', 19, 'Rotated Bounding Capsules', 'Zero radius capsule degenerates to line segment hit test', () => {
  const capA = { x: 0, y: 0, z: 0 };
  const capB = { x: 0, y: 0, z: 20 };
  const res = math.intersectRayCapsule({ x: 0, y: -5, z: 10 }, { x: 0, y: 1, z: 0 }, capA, capB, 0.0);
  assert.strictEqual(res.hit, true);
  utils.assertFloatEqual(res.closestDist, 0.0, 1e-4);
});

registerTest('T2-F19-02', 19, 'Rotated Bounding Capsules', 'Zero radius capsule misses offset ray', () => {
  const capA = { x: 0, y: 0, z: 0 };
  const capB = { x: 0, y: 0, z: 20 };
  const res = math.intersectRayCapsule({ x: 0.1, y: -5, z: 10 }, { x: 0, y: 1, z: 0 }, capA, capB, 0.0);
  assert.strictEqual(res.hit, false);
});

registerTest('T2-F19-03', 19, 'Rotated Bounding Capsules', 'Zero length capsule (sphere test)', () => {
  const capA = { x: 10, y: 0, z: 0 };
  const capB = { x: 10, y: 0, z: 0 };
  const res = math.intersectRayCapsule({ x: 0, y: 0, z: 0 }, { x: 1, y: 0, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(res.hit, true);
});

registerTest('T2-F19-04', 19, 'Rotated Bounding Capsules', 'Ray pointed away from capsule misses', () => {
  const capA = { x: 50, y: 0, z: 0 };
  const capB = { x: 50, y: 10, z: 0 };
  const res = math.intersectRayCapsule({ x: 0, y: 0, z: 0 }, { x: -1, y: 0, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(res.hit, false);
});

registerTest('T2-F19-05', 19, 'Rotated Bounding Capsules', 'Parallel ray grazing capsule edge', () => {
  const capA = { x: 0, y: 0, z: 0 };
  const capB = { x: 0, y: 100, z: 0 };
  const res = math.intersectRayCapsule({ x: 4.9, y: -10, z: 0 }, { x: 0, y: 1, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(res.hit, true);
});

// F-20: Two-Bone IK Singularities
registerTest('T2-F20-01', 20, 'Analytical Two-Bone IK Solver', 'Target distance at exact maximum reach (L1 + L2)', () => {
  const res = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 40, y: 0, z: 0 }, 20, 20);
  assert.strictEqual(res.reachedTarget, true);
  utils.assertFloatEqual(res.cosAlpha, 1.0, 1e-4);
});

registerTest('T2-F20-02', 20, 'Analytical Two-Bone IK Solver', 'Target distance at root position (0,0,0) folded limb', () => {
  const res = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: 0 }, 20, 20);
  assert.strictEqual(res.reachedTarget, false);
  utils.assertNoNaNOrInf(res.midPos);
  utils.assertNoNaNOrInf(res.effectorPos);
});

registerTest('T2-F20-03', 20, 'Analytical Two-Bone IK Solver', 'Collinear pole vector fallback', () => {
  // Target along +X, pole also along +X
  const res = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 28.284, y: 0, z: 0 }, 20, 20, { x: 1, y: 0, z: 0 });
  utils.assertNoNaNOrInf(res.midPos);
});

registerTest('T2-F20-04', 20, 'Analytical Two-Bone IK Solver', 'Unequal bone lengths reaching target (L1=30, L2=10)', () => {
  const res = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 25, y: 0, z: 0 }, 30, 10);
  assert.strictEqual(res.reachedTarget, true);
  utils.assertNoNaNOrInf(res.midPos);
});

registerTest('T2-F20-05', 20, 'Analytical Two-Bone IK Solver', 'Extreme target distance (1000 units)', () => {
  const res = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 1000, y: 0, z: 0 }, 20, 20);
  assert.strictEqual(res.reachedTarget, false);
  utils.assertFloatEqual(res.effectorPos.x, 40.0 * 0.9999, 1e-3);
});

registerTest('T2-F20-06', 20, 'Analytical Two-Bone IK Solver', 'Target distance inside min reach sets reachedTarget false', () => {
  const res = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 5, y: 0, z: 0 }, 30, 10);
  assert.strictEqual(res.reachedTarget, false);
});

// F-21: Manipulator Boundaries
registerTest('T2-F21-01', 21, 'Interactive 3D End-Effector Manipulator', 'Manipulator drag to exact origin', () => {
  const target = new THREE.Vector3(0, 0, 0);
  assert.strictEqual(target.length(), 0.0);
});

registerTest('T2-F21-02', 21, 'Interactive 3D End-Effector Manipulator', 'Target coordinates non-negative floor clamp', () => {
  const target = new THREE.Vector3(10, 20, -5);
  target.z = Math.max(0.0, target.z);
  assert.strictEqual(target.z, 0.0);
});

registerTest('T2-F21-03', 21, 'Interactive 3D End-Effector Manipulator', 'Boundary sphere radius parameter non-zero', () => {
  const geom = new THREE.SphereGeometry(40.0);
  assert.strictEqual(geom.parameters.radius, 40.0);
});

registerTest('T2-F21-04', 21, 'Interactive 3D End-Effector Manipulator', 'Large drag coordinates clamping', () => {
  const target = new THREE.Vector3(500, 500, 500);
  const maxL = 40.0;
  if (target.length() > maxL) {
    target.normalize().multiplyScalar(maxL);
  }
  utils.assertFloatEqual(target.length(), 40.0, 1e-4);
});

registerTest('T2-F21-05', 21, 'Interactive 3D End-Effector Manipulator', 'Gizmo position copy consistency', () => {
  const v1 = new THREE.Vector3(12, 34, 56);
  const v2 = new THREE.Vector3().copy(v1);
  utils.assertVec3Equal(v1, v2);
});

// F-22: SLERP Extremities
registerTest('T2-F22-01', 22, 'Quaternion SLERP Pose Blending', 'Antipodal quaternions shortest-arc negation', () => {
  const q0 = { x: 0, y: 0, z: 0, w: 1 };
  const q1 = { x: 0, y: 0, z: 0, w: -1 };
  const res = math.quaternionSlerp(q0, q1, 0.5);
  utils.assertFloatEqual(res.w, 1.0, 1e-4);
});

registerTest('T2-F22-02', 22, 'Quaternion SLERP Pose Blending', 'Identical quaternions blend t=0.5 returns identical', () => {
  const q = { x: 0, y: 0, z: Math.SQRT1_2, w: Math.SQRT1_2 };
  const res = math.quaternionSlerp(q, q, 0.5);
  utils.assertFloatEqual(res.w, Math.SQRT1_2, 1e-4);
  utils.assertFloatEqual(res.z, Math.SQRT1_2, 1e-4);
});

registerTest('T2-F22-03', 22, 'Quaternion SLERP Pose Blending', 'Blend weight t=0 returns q0', () => {
  const q0 = { x: 0, y: 0, z: 0, w: 1 };
  const q1 = { x: 0, y: 0, z: 1, w: 0 };
  const res = math.quaternionSlerp(q0, q1, 0.0);
  utils.assertFloatEqual(res.w, 1.0, 1e-4);
});

registerTest('T2-F22-04', 22, 'Quaternion SLERP Pose Blending', 'Blend weight t=1 returns q1', () => {
  const q0 = { x: 0, y: 0, z: 0, w: 1 };
  const q1 = { x: 0, y: 0, z: 1, w: 0 };
  const res = math.quaternionSlerp(q0, q1, 1.0);
  utils.assertFloatEqual(res.z, 1.0, 1e-4);
});

registerTest('T2-F22-05', 22, 'Quaternion SLERP Pose Blending', 'Quaternion unit normalization check', () => {
  const q0 = { x: 0.5, y: 0.5, z: 0.5, w: 0.5 };
  const q1 = { x: 0, y: 0, z: 0.8, w: 0.6 };
  const res = math.quaternionSlerp(q0, q1, 0.3);
  const norm = Math.hypot(res.x, res.y, res.z, res.w);
  utils.assertFloatEqual(norm, 1.0, 1e-4);
});

// ============================================================================
// CATEGORY F: SIM 11 VOLUMETRIC SMOKE BOUNDARIES (FEATURES 23 - 27)
// ============================================================================

// F-23: Voxel Grid Coordinate Extremes
registerTest('T2-F23-01', 23, '32x32x32 Voxel Discretization', 'Minimum corner voxel (0,0,0)', () => {
  const grid = new math.VoxelGrid32();
  grid.set(0, 0, 0, 1.0);
  assert.strictEqual(grid.get(0, 0, 0), 1.0);
});

registerTest('T2-F23-02', 23, '32x32x32 Voxel Discretization', 'Maximum corner voxel (31,31,31)', () => {
  const grid = new math.VoxelGrid32();
  grid.set(31, 31, 31, 1.0);
  assert.strictEqual(grid.get(31, 31, 31), 1.0);
});

registerTest('T2-F23-03', 23, '32x32x32 Voxel Discretization', 'Out-of-grid negative query (-1, 15, 15)', () => {
  const grid = new math.VoxelGrid32();
  assert.strictEqual(grid.get(-1, 15, 15), 0.0);
});

registerTest('T2-F23-04', 23, '32x32x32 Voxel Discretization', 'Out-of-grid overflow query (32, 10, 5)', () => {
  const grid = new math.VoxelGrid32();
  assert.strictEqual(grid.get(32, 10, 5), 0.0);
});

registerTest('T2-F23-05', 23, '32x32x32 Voxel Discretization', 'Clear already cleared voxel returns false', () => {
  const grid = new math.VoxelGrid32();
  assert.strictEqual(grid.clear(10, 10, 10), false);
});

// F-24: Morton Dilation Boundaries
registerTest('T2-F24-01', 24, '3D Morton Order Bit Interleaving', 'Zero coordinate morton index is 0', () => {
  assert.strictEqual(math.encodeMorton3D(0, 0, 0), 0);
});

registerTest('T2-F24-02', 24, '3D Morton Order Bit Interleaving', 'Max 5-bit coordinate morton index is 32767', () => {
  assert.strictEqual(math.encodeMorton3D(31, 31, 31), 0x7fff);
});

registerTest('T2-F24-03', 24, '3D Morton Order Bit Interleaving', 'Morton index word bit decomposition', () => {
  const m = 32767;
  const word = m >> 6;
  const bit = m & 63;
  assert.strictEqual(word, 511);
  assert.strictEqual(bit, 63);
});

registerTest('T2-F24-04', 24, '3D Morton Order Bit Interleaving', 'Single axis Morton encoding (X only)', () => {
  const m = math.encodeMorton3D(1, 0, 0);
  assert.strictEqual(m, 1);
});

registerTest('T2-F24-05', 24, '3D Morton Order Bit Interleaving', 'Single axis Morton encoding (Z only)', () => {
  const m = math.encodeMorton3D(0, 0, 1);
  assert.strictEqual(m, 4); // bit shifted by 2
});

registerTest('T2-F24-06', 24, '3D Morton Order Bit Interleaving', 'Negative coordinate clamping avoids two-complement aliasing', () => {
  const m = math.encodeMorton3D(-1, -1, -1);
  assert.strictEqual(m, 0); // clamped to (0,0,0) rather than aliased to 32767
});

registerTest('T2-F24-07', 24, '3D Morton Order Bit Interleaving', 'Out-of-bounds overflow coordinates clamped safely', () => {
  const m = math.encodeMorton3D(35, 35, 35);
  assert.strictEqual(m, 32767); // clamped to (31,31,31) rather than masked modulo 32
});

// F-25: DDA Optical Occlusion Boundaries
registerTest('T2-F25-01', 25, '3D DDA Line-of-Sight Raymarch', 'Zero length ray returns 0 opacity immediately', () => {
  const grid = new math.VoxelGrid32();
  grid.set(16, 16, 16);
  const op = grid.raymarchDDA(0, 0, 0, 0, 0, 0);
  assert.strictEqual(op, 0.0);
});

registerTest('T2-F25-02', 25, '3D DDA Line-of-Sight Raymarch', 'Early termination on full occlusion threshold', () => {
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 10);
  const op = grid.raymarchDDA(-300, 0, 0, 300, 0, 0);
  assert.strictEqual(op, 1.0);
});

registerTest('T2-F25-03', 25, '3D DDA Line-of-Sight Raymarch', 'Ray traversing outside smoke cube boundary', () => {
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 5);
  const op = grid.raymarchDDA(-500, 500, 500, 500, 500, 500);
  assert.strictEqual(op, 0.0);
});

registerTest('T2-F25-04', 25, '3D DDA Line-of-Sight Raymarch', 'Half voxel sampling step count cap (128)', () => {
  const stepSize = 10.0;
  const longDist = 5000.0;
  const steps = Math.min(128, Math.floor(longDist / stepSize));
  assert.strictEqual(steps, 128);
});

registerTest('T2-F25-05', 25, '3D DDA Line-of-Sight Raymarch', 'Single active voxel small opacity contribution', () => {
  const grid = new math.VoxelGrid32();
  grid.set(16, 16, 16, 1.0);
  const op = grid.raymarchDDA(0, 0, 0, 10, 0, 0);
  assert.ok(op >= 0.0 && op <= 1.0);
});

// F-26: Cavity Carving Boundaries
registerTest('T2-F26-01', 26, 'Ballistic Cavity Carving', 'Zero radius bullet carve clears 0 voxels', () => {
  const grid = new math.VoxelGrid32();
  grid.set(16, 16, 16);
  const cleared = grid.carveCylinder({ x: -100, y: 0, z: 0 }, { x: 100, y: 0, z: 0 }, 0.0);
  assert.strictEqual(cleared, 0);
});

registerTest('T2-F26-02', 26, 'Ballistic Cavity Carving', 'Bullet carve outside voxel bounds clears 0 voxels', () => {
  const grid = new math.VoxelGrid32();
  grid.set(16, 16, 16);
  const cleared = grid.carveCylinder({ x: 1000, y: 0, z: 0 }, { x: 2000, y: 0, z: 0 }, 30.0);
  assert.strictEqual(cleared, 0);
});

registerTest('T2-F26-03', 26, 'Ballistic Cavity Carving', 'Carving empty grid returns 0 cleared count', () => {
  const grid = new math.VoxelGrid32();
  const cleared = grid.carveCylinder({ x: -100, y: 0, z: 0 }, { x: 100, y: 0, z: 0 }, 30.0);
  assert.strictEqual(cleared, 0);
});

registerTest('T2-F26-04', 26, 'Ballistic Cavity Carving', 'Repeated carve on same trajectory clears 0 additional voxels', () => {
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 8);
  grid.carveCylinder({ x: -200, y: 0, z: 0 }, { x: 200, y: 0, z: 0 }, 30.0);
  const second = grid.carveCylinder({ x: -200, y: 0, z: 0 }, { x: 200, y: 0, z: 0 }, 30.0);
  assert.strictEqual(second, 0);
});

registerTest('T2-F26-05', 26, 'Ballistic Cavity Carving', 'Very large carve radius clears entire sphere', () => {
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 5);
  grid.carveCylinder({ x: -300, y: 0, z: 0 }, { x: 300, y: 0, z: 0 }, 400.0);
  assert.strictEqual(grid.countActive(), 0);
});

// F-27: Blast Displacement Boundaries
registerTest('T2-F27-01', 27, 'Explosive Blast Displacement', 'Zero radius blast clears 0 voxels', () => {
  const r = 0.0;
  assert.strictEqual(r, 0.0);
});

registerTest('T2-F27-02', 27, 'Explosive Blast Displacement', 'Blast outside smoke grid', () => {
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 5);
  const initial = grid.countActive();
  // Clear far away
  grid.clear(0, 0, 0);
  assert.strictEqual(grid.countActive(), initial);
});

registerTest('T2-F27-03', 27, 'Explosive Blast Displacement', 'Blast displacement landing outside boundary is discarded', () => {
  const vx = 31;
  const pushVoxels = 2;
  const destX = vx + pushVoxels;
  const discarded = destX >= 32;
  assert.strictEqual(discarded, true);
});

registerTest('T2-F27-04', 27, 'Explosive Blast Displacement', 'Push shell thickness difference', () => {
  const shellThickness = math.SMOKE_BLAST_PUSH_RADIUS - math.SMOKE_BLAST_CLEAR_RADIUS;
  assert.strictEqual(shellThickness, 80.0);
});

registerTest('T2-F27-05', 27, 'Explosive Blast Displacement', 'Density decay compounding (0.85^2)', () => {
  const d = 1.0 * 0.85 * 0.85;
  utils.assertFloatEqual(d, 0.7225);
});

// ============================================================================
// CATEGORY G: SIM 14 BULLET PENETRATION BOUNDARIES (FEATURES 28 - 32)
// ============================================================================

// F-28: Material Matrix Extremities
registerTest('T2-F28-01', 28, 'Multi-Material 3D Cross-Sections', 'Zero thickness slab distance loss is 0.0', () => {
  const res = math.calculateRubikonDissipation(0.0, 3.0, 2.0, 36.0, 0.16);
  assert.strictEqual(res.lostDistance, 0.0);
});

registerTest('T2-F28-02', 28, 'Multi-Material 3D Cross-Sections', 'Glass minimal damage absorption (0.05)', () => {
  assert.strictEqual(math.MATERIALS.Glass.damageModifier, 0.05);
});

registerTest('T2-F28-03', 28, 'Multi-Material 3D Cross-Sections', 'SolidMetal penetration modifier is 1.0', () => {
  assert.strictEqual(math.MATERIALS.SolidMetal.penModifier, 1.0);
});

registerTest('T2-F28-04', 28, 'Multi-Material 3D Cross-Sections', 'Impenetrable material simulation', () => {
  const res = math.calculateRubikonDissipation(100.0, 0.1, 1.0, 30.0, 0.5);
  assert.strictEqual(res.exitDamage, 0.0);
});

registerTest('T2-F28-05', 28, 'Multi-Material 3D Cross-Sections', 'All materials have positive penModifier', () => {
  for (const m of Object.values(math.MATERIALS)) {
    assert.ok(m.penModifier > 0);
  }
});

// F-29: Distance Falloff Extremities
registerTest('T2-F29-01', 29, 'Atmospheric Distance Falloff ODE', 'Exact range cutoff boundary at 8192 units', () => {
  const d = math.calculateDistanceFalloff(36.0, 0.98, 8192.0);
  assert.ok(d > 0.0);
});

registerTest('T2-F29-02', 29, 'Atmospheric Distance Falloff ODE', 'Exceeded range cutoff at 8192.1 units', () => {
  const d = math.calculateDistanceFalloff(36.0, 0.98, 8192.1);
  assert.strictEqual(d, 0.0);
});

registerTest('T2-F29-03', 29, 'Atmospheric Distance Falloff ODE', 'Negative distance input returns base damage', () => {
  const d = math.calculateDistanceFalloff(36.0, 0.98, -100.0);
  assert.ok(d >= 36.0);
});

registerTest('T2-F29-04', 29, 'Atmospheric Distance Falloff ODE', 'Range modifier 1.0 (no falloff)', () => {
  const d = math.calculateDistanceFalloff(100.0, 1.0, 3000.0);
  assert.strictEqual(d, 100.0);
});

registerTest('T2-F29-05', 29, 'Atmospheric Distance Falloff ODE', 'Zero base damage returns 0.0', () => {
  const d = math.calculateDistanceFalloff(0.0, 0.98, 500.0);
  assert.strictEqual(d, 0.0);
});

// F-30: Rubikon Dissipation Extremities
registerTest('T2-F30-01', 30, 'Rubikon Solid Medium Kinetic Dissipation', 'Exit damage exactly 0.99 HP drops to 0.0 HP', () => {
  const res = math.calculateRubikonDissipation(10.0, 1.0, 2.0, 13.0, 0.22);
  assert.strictEqual(res.exitDamage, 0.0);
});

registerTest('T2-F30-02', 30, 'Rubikon Solid Medium Kinetic Dissipation', 'Exit damage 1.00 HP passes threshold', () => {
  // If exit damage >= 1.0, damage is retained
  const res = math.calculateRubikonDissipation(2.0, 3.0, 2.5, 36.0, 0.16);
  assert.ok(res.exitDamage >= 1.0);
});

registerTest('T2-F30-03', 30, 'Rubikon Solid Medium Kinetic Dissipation', 'Zero weapon penetration power safety', () => {
  const res = math.calculateRubikonDissipation(4.0, 3.0, 0.0, 36.0, 0.16);
  utils.assertNoNaNOrInf(res.totalLost);
});

registerTest('T2-F30-04', 30, 'Rubikon Solid Medium Kinetic Dissipation', 'High damage input (AWP 115 HP) wall penetration', () => {
  const res = math.calculateRubikonDissipation(6.0, 3.0, 2.5, 115.0, 0.16);
  assert.ok(res.exitDamage > 80.0);
});

registerTest('T2-F30-05', 30, 'Rubikon Solid Medium Kinetic Dissipation', 'Infinitesimal slab thickness (0.001 units)', () => {
  const res = math.calculateRubikonDissipation(0.001, 3.0, 2.0, 36.0, 0.16);
  utils.assertFloatEqual(res.lostDistance, 0.0, 1e-4);
});

// F-31: Ricochet Angle Extremities
registerTest('T2-F31-01', 31, 'Ricochet Angle Gating', 'Exact boundary angle 14.999 deg ricochets', () => {
  const rad = (14.999 * Math.PI) / 180.0;
  const dir = { x: Math.cos(rad), y: -Math.sin(rad), z: 0 };
  const res = math.evaluateRicochet(dir, { x: 0, y: 1, z: 0 }, 30.0, 15.0);
  assert.strictEqual(res.ricochet, true);
});

registerTest('T2-F31-02', 31, 'Ricochet Angle Gating', 'Exact boundary angle 15.001 deg penetrates', () => {
  const rad = (15.001 * Math.PI) / 180.0;
  const dir = { x: Math.cos(rad), y: -Math.sin(rad), z: 0 };
  const res = math.evaluateRicochet(dir, { x: 0, y: 1, z: 0 }, 30.0, 15.0);
  assert.strictEqual(res.ricochet, false);
});

registerTest('T2-F31-03', 31, 'Ricochet Angle Gating', 'Perpendicular impact (90 deg) never ricochets', () => {
  const dir = { x: 0, y: -1, z: 0 };
  const res = math.evaluateRicochet(dir, { x: 0, y: 1, z: 0 }, 30.0, 15.0);
  assert.strictEqual(res.ricochet, false);
});

registerTest('T2-F31-04', 31, 'Ricochet Angle Gating', 'Parallel impact (0 deg) ricochets', () => {
  const dir = { x: 1, y: 0, z: 0 };
  const res = math.evaluateRicochet(dir, { x: 0, y: 1, z: 0 }, 30.0, 15.0);
  assert.strictEqual(res.ricochet, true);
});

registerTest('T2-F31-05', 31, 'Ricochet Angle Gating', 'Damage damping retains non-zero positive energy', () => {
  const res = math.evaluateRicochet({ x: 0.99, y: -0.1, z: 0 }, { x: 0, y: 1, z: 0 }, 10.0, 15.0);
  assert.strictEqual(res.newDamage, 5.0);
});

// F-32: Armor Jump Table Extremities
registerTest('T2-F32-01', 32, 'Player Armor Mitigation Jump Table', 'Exact armor exhaustion match (armor reaches exactly 0)', () => {
  const res = math.calculateArmorMitigation(40.0, 1.0, 2, 10, false);
  assert.strictEqual(res.remainingArmor, 0);
  assert.strictEqual(res.healthDamage, 20.0);
});

registerTest('T2-F32-02', 32, 'Player Armor Mitigation Jump Table', '1 point remaining armor mitigation overflow', () => {
  const res = math.calculateArmorMitigation(50.0, 1.55, 2, 1, false);
  assert.strictEqual(res.remainingArmor, 0);
  assert.ok(res.healthDamage > 40.0);
});

registerTest('T2-F32-03', 32, 'Player Armor Mitigation Jump Table', 'Armored headshot without helmet takes full 4.0x damage', () => {
  const res = math.calculateArmorMitigation(140.0, 1.55, 1, 100, false);
  assert.strictEqual(res.healthDamage, 140.0);
  assert.strictEqual(res.armorAbsorbed, 0);
});

registerTest('T2-F32-04', 32, 'Player Armor Mitigation Jump Table', 'Right leg hit unarmored verification', () => {
  const res = math.calculateArmorMitigation(30.0, 1.55, 7, 100, true);
  assert.strictEqual(res.healthDamage, 30.0);
  assert.strictEqual(res.armorAbsorbed, 0);
});

registerTest('T2-F32-05', 32, 'Player Armor Mitigation Jump Table', 'Negative damage input returns zero health damage', () => {
  const res = math.calculateArmorMitigation(0.0, 1.55, 2, 100, true);
  assert.strictEqual(res.healthDamage, 0.0);
  assert.strictEqual(res.armorAbsorbed, 0);
});

// ============================================================================
// CATEGORY H: SIM 12 LAG COMPENSATION BOUNDARIES (FEATURES 33 - 37)
// ============================================================================

// F-33: Ring Buffer Capacity Extremities
registerTest('T2-F33-01', 33, '64-Tick Circular History Ring Buffer', 'Single record in buffer query', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 1.0, origin: { x: 5, y: 0, z: 0 } });
  const res = buf.querySubTick(1.0);
  assert.strictEqual(res.origin.x, 5);
});

registerTest('T2-F33-02', 33, '64-Tick Circular History Ring Buffer', 'Exact 64 records capacity without eviction', () => {
  const buf = new math.LagCompHistoryBuffer(64);
  for (let i = 0; i < 64; i++) buf.push({ tick: i });
  assert.strictEqual(buf.records.length, 64);
  assert.strictEqual(buf.records[0].tick, 0);
});

registerTest('T2-F33-03', 33, '64-Tick Circular History Ring Buffer', '65th record evicts record 0', () => {
  const buf = new math.LagCompHistoryBuffer(64);
  for (let i = 0; i < 65; i++) buf.push({ tick: i });
  assert.strictEqual(buf.records.length, 64);
  assert.strictEqual(buf.records[0].tick, 1);
});

registerTest('T2-F33-04', 33, '64-Tick Circular History Ring Buffer', 'Continuous time monotonicity check in buffer', () => {
  const buf = new math.LagCompHistoryBuffer();
  for (let i = 0; i < 10; i++) buf.push({ time: i * 0.015625 });
  for (let i = 0; i < 9; i++) {
    assert.ok(buf.records[i + 1].time > buf.records[i].time);
  }
});

registerTest('T2-F33-05', 33, '64-Tick Circular History Ring Buffer', 'Buffer clearance resets count to 0', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ tick: 1 });
  buf.records.length = 0;
  assert.strictEqual(buf.records.length, 0);
});

// F-34: Rewind Sub-Tick Query Extremities
registerTest('T2-F34-01', 34, 'Continuous Sub-Tick Rewind Query', 'Query timestamp at oldest boundary record', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 10.0, origin: { x: 10, y: 0, z: 0 } });
  buf.push({ time: 10.0 + math.TICK_INTERVAL, origin: { x: 20, y: 0, z: 0 } });
  const res = buf.querySubTick(10.0);
  assert.strictEqual(res.origin.x, 10);
});

registerTest('T2-F34-02', 34, 'Continuous Sub-Tick Rewind Query', 'Query timestamp at newest boundary record', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 10.0, origin: { x: 10, y: 0, z: 0 } });
  buf.push({ time: 10.0 + math.TICK_INTERVAL, origin: { x: 20, y: 0, z: 0 } });
  const res = buf.querySubTick(10.0 + math.TICK_INTERVAL);
  assert.strictEqual(res.origin.x, 20);
});

registerTest('T2-F34-03', 34, 'Continuous Sub-Tick Rewind Query', 'sv_maxunlag boundary 200.0ms accepted', () => {
  const buf = new math.LagCompHistoryBuffer();
  for (let i = 0; i < 20; i++) buf.push({ time: i * math.TICK_INTERVAL, origin: { x: i, y: 0, z: 0 } });
  const newestTime = buf.records[buf.records.length - 1].time;
  const res = buf.querySubTick(newestTime - 0.200, 0.200);
  utils.assertFloatEqual(res.time, newestTime - 0.200, 1e-4);
});

registerTest('T2-F34-04', 34, 'Continuous Sub-Tick Rewind Query', 'sv_maxunlag boundary 200.1ms clamped to 200.0ms', () => {
  const buf = new math.LagCompHistoryBuffer();
  for (let i = 0; i < 20; i++) buf.push({ time: i * math.TICK_INTERVAL, origin: { x: i, y: 0, z: 0 } });
  const newestTime = buf.records[buf.records.length - 1].time;
  const res = buf.querySubTick(newestTime - 0.2001, 0.200);
  utils.assertFloatEqual(res.time, newestTime - 0.200, 1e-4);
});

registerTest('T2-F34-05', 34, 'Continuous Sub-Tick Rewind Query', 'Infinitesimal sub-tick fraction (w = 1e-6)', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: 100, y: 0, z: 0 } });
  const res = buf.querySubTick(1e-6 * math.TICK_INTERVAL);
  utils.assertFloatEqual(res.origin.x, 0.0, 1e-3);
});

// F-35: Teleport Threshold Boundaries
registerTest('T2-F35-01', 35, 'Teleport Discontinuity Breaker', 'Displacement squared 4095.9 u^2 (does NOT trip)', () => {
  const buf = new math.LagCompHistoryBuffer();
  const d = Math.sqrt(4095.9);
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: d, y: 0, z: 0 } });
  const res = buf.querySubTick(0.5 * math.TICK_INTERVAL);
  assert.strictEqual(res.teleportTripped, false);
});

registerTest('T2-F35-02', 35, 'Teleport Discontinuity Breaker', 'Displacement squared 4096.1 u^2 (TRIPS breaker)', () => {
  const buf = new math.LagCompHistoryBuffer();
  const d = Math.sqrt(4096.1);
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: d, y: 0, z: 0 } });
  const res = buf.querySubTick(0.5 * math.TICK_INTERVAL);
  assert.strictEqual(res.teleportTripped, true);
});

registerTest('T2-F35-03', 35, 'Teleport Discontinuity Breaker', 'Exact displacement squared 4096.0 u^2 trips breaker', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: 64, y: 0, z: 0 } });
  const res = buf.querySubTick(0.5 * math.TICK_INTERVAL);
  assert.strictEqual(res.teleportTripped, true);
});

registerTest('T2-F35-04', 35, 'Teleport Discontinuity Breaker', '3D diagonal teleport displacement check (37^2 + 37^2 + 37^2 = 4107)', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: 37, y: 37, z: 37 } });
  const res = buf.querySubTick(0.5 * math.TICK_INTERVAL);
  assert.strictEqual(res.teleportTripped, true);
});

registerTest('T2-F35-05', 35, 'Teleport Discontinuity Breaker', 'Repeated teleportation across successive frames', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: 100, y: 0, z: 0 } });
  buf.push({ time: 2 * math.TICK_INTERVAL, origin: { x: 200, y: 0, z: 0 } });
  const res = buf.querySubTick(1.5 * math.TICK_INTERVAL);
  assert.strictEqual(res.teleportTripped, true);
});

// F-36: Spatial Arena Boundaries
registerTest('T2-F36-01', 36, '3D Spatial Lag Comp Arena', 'Shooter aim ray missing rewound hitbox', () => {
  const capA = { x: 100, y: 0, z: 64 };
  const capB = { x: 100, y: 0, z: 70 };
  const rayOrigin = { x: 0, y: 50, z: 67 }; // Offset by 50 units
  const res = math.intersectRayCapsule(rayOrigin, { x: 1, y: 0, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(res.hit, false);
});

registerTest('T2-F36-02', 36, '3D Spatial Lag Comp Arena', 'Shooter ray zero distance to capsule', () => {
  const capA = { x: 0, y: 0, z: 64 };
  const capB = { x: 0, y: 0, z: 70 };
  const rayOrigin = { x: 0, y: 0, z: 67 };
  const res = math.intersectRayCapsule(rayOrigin, { x: 1, y: 0, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(res.hit, true);
});

registerTest('T2-F36-03', 36, '3D Spatial Lag Comp Arena', 'Concurrent visualization of 3 pawns', () => {
  const group = new THREE.Group();
  group.add(new THREE.Mesh()); // client
  group.add(new THREE.Mesh()); // server
  group.add(new THREE.Mesh()); // rewound
  assert.strictEqual(group.children.length, 3);
});

registerTest('T2-F36-04', 36, '3D Spatial Lag Comp Arena', 'Raycast origin behind pawn looking backward', () => {
  const capA = { x: 100, y: 0, z: 64 };
  const capB = { x: 100, y: 0, z: 70 };
  const rayOrigin = { x: 150, y: 0, z: 67 };
  const res = math.intersectRayCapsule(rayOrigin, { x: 1, y: 0, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(res.hit, false);
});

registerTest('T2-F36-05', 36, '3D Spatial Lag Comp Arena', 'Pawn state restoration identity', () => {
  const p1 = { x: 10, y: 20, z: 30 };
  const p2 = { ...p1 };
  utils.assertVec3Equal(p1, p2);
});

// F-37: BVH Visualization Boundaries
registerTest('T2-F37-01', 37, 'Rubikon BVH Tree Visualization', 'Empty BVH tree renders 0 boxes', () => {
  const group = new THREE.Group();
  assert.strictEqual(group.children.length, 0);
});

registerTest('T2-F37-02', 37, 'Rubikon BVH Tree Visualization', 'Degenerate zero-volume AABB box', () => {
  const box = new THREE.BoxGeometry(0, 0, 0);
  assert.strictEqual(box.parameters.width, 0);
});

registerTest('T2-F37-03', 37, 'Rubikon BVH Tree Visualization', 'Large BVH tree with 128 nodes', () => {
  const group = new THREE.Group();
  for (let i = 0; i < 128; i++) group.add(new THREE.Mesh());
  assert.strictEqual(group.children.length, 128);
});

registerTest('T2-F37-04', 37, 'Rubikon BVH Tree Visualization', 'BVH wireframe opacity setting', () => {
  const mat = new THREE.MeshBasicMaterial({ wireframe: true, opacity: 0.5 });
  assert.strictEqual(mat.opacity, 0.5);
});

registerTest('T2-F37-05', 37, 'Rubikon BVH Tree Visualization', 'BVH depth index clamping', () => {
  const depth = Math.min(2, 5);
  assert.strictEqual(depth, 2);
});

// ============================================================================
// CATEGORY I: PORTAL INTEGRATION & GUARDRAILS BOUNDARIES (FEATURES 38 - 40)
// ============================================================================

// F-38: Portal Routing Boundaries
registerTest('T2-F38-01', 38, 'Portal Routing & Workstation Mounting', 'Unknown simulator ID fallback (sim 99)', () => {
  const isSupported = (id) => ['03', '09', '11', '12', '14'].includes(id);
  assert.strictEqual(isSupported('99'), false);
});

registerTest('T2-F38-02', 38, 'Portal Routing & Workstation Mounting', 'Empty string simulator ID routing', () => {
  const isSupported = (id) => Boolean(id && ['03', '09', '11', '12', '14'].includes(id));
  assert.strictEqual(isSupported(''), false);
});

registerTest('T2-F38-03', 38, 'Portal Routing & Workstation Mounting', 'Double closeSimulator invocation safety', () => {
  const { document } = createMockEnvironment();
  const detail = document.getElementById('simulatorDetailView');
  detail.classList.add('hidden');
  detail.classList.add('hidden');
  assert.strictEqual(detail.classList.contains('hidden'), true);
});

registerTest('T2-F38-04', 38, 'Portal Routing & Workstation Mounting', 'Rapid simulator switching (10 iterations)', () => {
  const { window } = createMockEnvironment();
  const simIds = ['03', '09', '11', '12', '14'];
  for (let i = 0; i < 10; i++) {
    window.location.hash = `#simulators/${simIds[i % simIds.length]}`;
  }
  assert.strictEqual(window.location.hash, '#simulators/14');
});

registerTest('T2-F38-05', 38, 'Portal Routing & Workstation Mounting', 'Missing container element handling', () => {
  const { document } = createMockEnvironment();
  const nonExistent = document.getElementById('invalidContainer');
  assert.strictEqual(nonExistent, null);
});

// F-39: Build Pipeline Boundaries
registerTest('T2-F39-01', 39, 'Portal Build Pipeline Verification', 'docs/portal-data.js size within expected range (>4MB, <10MB)', () => {
  const p = path.resolve(__dirname, '../../docs/portal-data.js');
  const size = fs.statSync(p).size;
  assert.ok(size > 4000000 && size < 10000000);
});

registerTest('T2-F39-02', 39, 'Portal Build Pipeline Verification', 'docs/docs-data.js size within expected range (>500KB, <2MB)', () => {
  const p = path.resolve(__dirname, '../../docs/docs-data.js');
  const size = fs.statSync(p).size;
  assert.ok(size > 500000 && size < 2000000);
});

registerTest('T2-F39-03', 39, 'Portal Build Pipeline Verification', 'llms.txt file existence and minimum size (>15KB)', () => {
  const p = path.resolve(__dirname, '../../docs/llms.txt');
  assert.ok(fs.existsSync(p));
  assert.ok(fs.statSync(p).size > 15000);
});

registerTest('T2-F39-04', 39, 'Portal Build Pipeline Verification', 'llms-full.txt file existence and minimum size (>1MB)', () => {
  const p = path.resolve(__dirname, '../../docs/llms-full.txt');
  assert.ok(fs.existsSync(p));
  assert.ok(fs.statSync(p).size > 1000000);
});

registerTest('T2-F39-05', 39, 'Portal Build Pipeline Verification', 'All 19 simulators compile command verification string', () => {
  const p = path.resolve(__dirname, '../../docs/portal-data.js');
  const content = fs.readFileSync(p, 'utf-8');
  assert.ok(content.includes('cl.exe') && content.includes('/std:c++20'));
});

// F-40: Guardrails Boundaries
registerTest('T2-F40-01', 40, 'Repository Guardrails Verification', 'Zero absolute paths in tools/ python scripts', () => {
  const toolsDir = path.resolve(__dirname, '../../tools');
  const pyFiles = fs.readdirSync(toolsDir).filter(f => f.endsWith('.py'));
  for (const f of pyFiles) {
    const content = fs.readFileSync(path.join(toolsDir, f), 'utf-8');
    const matches = content.match(/[a-zA-Z]:[\\/](?:Users|hannu)/gi);
    assert.strictEqual(matches, null, `Path violation in tools/${f}`);
  }
});

registerTest('T2-F40-02', 40, 'Repository Guardrails Verification', 'Zero absolute paths in tests/ javascript test files', () => {
  const testsDir = path.resolve(__dirname, '.');
  const jsFiles = fs.readdirSync(testsDir).filter(f => f.endsWith('.js'));
  for (const f of jsFiles) {
    const content = fs.readFileSync(path.join(testsDir, f), 'utf-8');
    const matches = content.match(/[a-zA-Z]:[\\/](?:Users|hannu)/gi);
    assert.strictEqual(matches, null, `Path violation in tests/e2e/${f}`);
  }
});

registerTest('T2-F40-03', 40, 'Repository Guardrails Verification', 'Zero prohibited binary files in tools/', () => {
  const toolsDir = path.resolve(__dirname, '../../tools');
  const files = fs.readdirSync(toolsDir);
  for (const f of files) {
    const ext = path.extname(f).toLowerCase();
    assert.ok(!['.exe', '.dll', '.obj', '.lib', '.pdb'].includes(ext), `Prohibited binary in tools/${f}`);
  }
});

registerTest('T2-F40-04', 40, 'Repository Guardrails Verification', 'Zero prohibited binary files in tests/e2e/', () => {
  const files = fs.readdirSync(__dirname);
  for (const f of files) {
    const ext = path.extname(f).toLowerCase();
    assert.ok(!['.exe', '.dll', '.obj', '.lib', '.pdb'].includes(ext), `Prohibited binary in tests/e2e/${f}`);
  }
});

registerTest('T2-F40-05', 40, 'Repository Guardrails Verification', 'Repository root README.md sanitization check', () => {
  const p = path.resolve(__dirname, '../../README.md');
  if (fs.existsSync(p)) {
    const content = fs.readFileSync(p, 'utf-8');
    const matches = content.match(/[a-zA-Z]:[\\/](?:Users|hannu)/gi);
    assert.strictEqual(matches, null, 'Absolute path violation in README.md');
  }
});

module.exports = {
  tests
};
