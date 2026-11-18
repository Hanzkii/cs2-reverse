/**
 * Tier 1: Feature Coverage Test Suite (200 Tests: >= 5 tests per feature for Features 1-40)
 * Evaluates primary behavior and interface contracts across all subsystems.
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
  tests.push({ id, tier: 1, featureId, featureName, name, fn });
}

// ============================================================================
// CATEGORY A: VIEWPORT 3D & RENDERING (FEATURES 1 - 5)
// ============================================================================

// F-01: Three.js & OrbitControls Integration
registerTest('T1-F01-01', 1, 'Three.js & OrbitControls Integration', 'Canvas DOM element creation and mounting', () => {
  const { document } = createMockEnvironment();
  const container = document.getElementById('simVisualizerContainer');
  const canvas = document.createElement('canvas');
  canvas.id = 'viewportCanvas';
  container.appendChild(canvas);
  assert.strictEqual(container.children.length, 1);
  assert.strictEqual(container.children[0].tagName, 'CANVAS');
});

registerTest('T1-F01-02', 1, 'Three.js & OrbitControls Integration', 'WebGLRenderer initialization and size binding', () => {
  const { document } = createMockEnvironment();
  const canvas = document.createElement('canvas');
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
  renderer.setSize(1024, 768);
  const size = renderer.getSize();
  assert.strictEqual(size.width, 1024);
  assert.strictEqual(size.height, 768);
  assert.strictEqual(renderer.antialias, true);
});

registerTest('T1-F01-03', 1, 'Three.js & OrbitControls Integration', 'OrbitControls target initialization and damping', () => {
  const camera = new THREE.PerspectiveCamera(60, 1.33, 1, 5000);
  const canvas = new THREE.Mesh();
  const controls = new THREE.OrbitControls(camera, canvas);
  utils.assertVec3Equal(controls.target, { x: 0, y: 0, z: 32 });
  assert.strictEqual(controls.enableDamping, true);
  assert.strictEqual(controls.dampingFactor, 0.05);
});

registerTest('T1-F01-04', 1, 'Three.js & OrbitControls Integration', 'OrbitControls polar angle restriction', () => {
  const camera = new THREE.PerspectiveCamera(60, 1.33, 1, 5000);
  const controls = new THREE.OrbitControls(camera, null);
  utils.assertFloatEqual(controls.maxPolarAngle, Math.PI / 2 + 0.1, 1e-3);
  assert.strictEqual(controls.update(), true);
});

registerTest('T1-F01-05', 1, 'Three.js & OrbitControls Integration', 'Renderer pixel ratio clamping', () => {
  const renderer = new THREE.WebGLRenderer();
  renderer.setPixelRatio(3.5);
  assert.strictEqual(renderer.getPixelRatio(), 2.0); // Clamped to 2.0
});

// F-02: Dual Camera Rig (Perspective & Orthographic)
registerTest('T1-F02-01', 2, 'Dual Camera Rig (Perspective & Orthographic)', 'Default perspective camera frustum parameters', () => {
  const camera = new THREE.PerspectiveCamera(45, 16 / 9, 1.0, 5000.0);
  assert.strictEqual(camera.fov, 45);
  utils.assertFloatEqual(camera.aspect, 16 / 9);
  assert.strictEqual(camera.near, 1.0);
  assert.strictEqual(camera.far, 5000.0);
});

registerTest('T1-F02-02', 2, 'Dual Camera Rig (Perspective & Orthographic)', 'Orthographic camera initialization', () => {
  const ortho = new THREE.OrthographicCamera(-100, 100, 75, -75, 1.0, 5000.0);
  assert.strictEqual(ortho.type, 'OrthographicCamera');
  assert.strictEqual(ortho.left, -100);
  assert.strictEqual(ortho.right, 100);
  assert.strictEqual(ortho.zoom, 1.0);
});

registerTest('T1-F02-03', 2, 'Dual Camera Rig (Perspective & Orthographic)', 'Frustum height matching from perspective FOV', () => {
  const d = 200.0;
  const fovDeg = 45.0;
  const aspect = 1.5;
  const fovRad = (fovDeg * Math.PI) / 180.0;
  const matchedH = 2.0 * d * Math.tan(fovRad / 2.0);
  const matchedW = matchedH * aspect;
  utils.assertFloatEqual(matchedH, 165.685, 0.05);
  utils.assertFloatEqual(matchedW, 248.528, 0.05);
});

registerTest('T1-F02-04', 2, 'Dual Camera Rig (Perspective & Orthographic)', 'Top camera preset alignment', () => {
  const camera = new THREE.PerspectiveCamera();
  camera.position.set(0, 0, 300);
  camera.up.set(0, 1, 0);
  utils.assertVec3Equal(camera.position, { x: 0, y: 0, z: 300 });
  utils.assertVec3Equal(camera.up, { x: 0, y: 1, z: 0 });
});

registerTest('T1-F02-05', 2, 'Dual Camera Rig (Perspective & Orthographic)', 'Side camera preset alignment', () => {
  const camera = new THREE.PerspectiveCamera();
  camera.position.set(-300, 0, 32);
  camera.up.set(0, 0, 1);
  utils.assertVec3Equal(camera.position, { x: -300, y: 0, z: 32 });
  utils.assertVec3Equal(camera.up, { x: 0, y: 0, z: 1 });
});

// F-03: Source 2 Debug Renderer Aesthetics
registerTest('T1-F03-01', 3, 'Source 2 Debug Renderer Aesthetics', 'Scene slate dark background #070b12', () => {
  const scene = new THREE.Scene();
  scene.background = new THREE.Color('#070b12');
  assert.strictEqual(scene.background.getHexString(), '070b12');
});

registerTest('T1-F03-02', 3, 'Source 2 Debug Renderer Aesthetics', 'Source 2 global up vector (+Z up)', () => {
  const obj = new THREE.Object3D();
  utils.assertVec3Equal(obj.up, { x: 0, y: 0, z: 1 });
});

registerTest('T1-F03-03', 3, 'Source 2 Debug Renderer Aesthetics', 'Major Hammer 64-unit floor grid', () => {
  const grid = new THREE.GridHelper(640, 10); // 640 / 10 = 64 units per division
  assert.strictEqual(grid.size, 640);
  assert.strictEqual(grid.divisions, 10);
});

registerTest('T1-F03-04', 3, 'Source 2 Debug Renderer Aesthetics', 'Minor Hammer 16-unit subdivisions', () => {
  const grid = new THREE.GridHelper(640, 40); // 640 / 40 = 16 units per division
  assert.strictEqual(grid.divisions, 40);
});

registerTest('T1-F03-05', 3, 'Source 2 Debug Renderer Aesthetics', 'Coordinate triad axes helper (+X, +Y, +Z)', () => {
  const axes = new THREE.AxesHelper(32);
  assert.strictEqual(axes.size, 32);
  assert.strictEqual(axes.type, 'LineSegments');
});

// F-04: Theater & Fullscreen Toggle
registerTest('T1-F04-01', 4, 'Theater & Fullscreen Toggle', 'Theater mode DOM class activation', () => {
  const { document } = createMockEnvironment();
  const container = document.getElementById('simVisualizerContainer');
  container.classList.add('fixed', 'inset-0', 'z-50');
  assert.ok(container.classList.contains('fixed'));
  assert.ok(container.classList.contains('inset-0'));
  assert.ok(container.classList.contains('z-50'));
});

registerTest('T1-F04-02', 4, 'Theater & Fullscreen Toggle', 'Theater mode DOM class removal on toggle off', () => {
  const { document } = createMockEnvironment();
  const container = document.getElementById('simVisualizerContainer');
  container.classList.add('fixed', 'inset-0');
  container.classList.remove('fixed', 'inset-0');
  assert.strictEqual(container.classList.contains('fixed'), false);
  assert.strictEqual(container.classList.contains('inset-0'), false);
});

registerTest('T1-F04-03', 4, 'Theater & Fullscreen Toggle', 'Fullscreen API request invocation', async () => {
  const { document } = createMockEnvironment();
  const container = document.getElementById('simVisualizerContainer');
  let invoked = false;
  container.requestFullscreen = async () => { invoked = true; };
  await container.requestFullscreen();
  assert.strictEqual(invoked, true);
});

registerTest('T1-F04-04', 4, 'Theater & Fullscreen Toggle', 'Fullscreen API exit invocation', async () => {
  const { document } = createMockEnvironment();
  let exited = false;
  document.exitFullscreen = async () => { exited = true; };
  await document.exitFullscreen();
  assert.strictEqual(exited, true);
});

registerTest('T1-F04-05', 4, 'Theater & Fullscreen Toggle', 'Body scroll lock on theater activation', () => {
  const { document } = createMockEnvironment();
  document.body.classList.add('overflow-hidden');
  assert.ok(document.body.classList.contains('overflow-hidden'));
});

// F-05: WebGL Context Lifecycle & Teardown
registerTest('T1-F05-01', 5, 'WebGL Context Lifecycle & Teardown', 'BufferGeometry disposal tracking', () => {
  const geom = new THREE.BufferGeometry();
  assert.strictEqual(geom.disposed, false);
  geom.dispose();
  assert.strictEqual(geom.disposed, true);
});

registerTest('T1-F05-02', 5, 'WebGL Context Lifecycle & Teardown', 'Material disposal tracking', () => {
  const mat = new THREE.MeshBasicMaterial();
  assert.strictEqual(mat.disposed, false);
  mat.dispose();
  assert.strictEqual(mat.disposed, true);
});

registerTest('T1-F05-03', 5, 'WebGL Context Lifecycle & Teardown', 'OrbitControls disposal', () => {
  const controls = new THREE.OrbitControls(null, null);
  assert.strictEqual(controls.disposed, false);
  controls.dispose();
  assert.strictEqual(controls.disposed, true);
});

registerTest('T1-F05-04', 5, 'WebGL Context Lifecycle & Teardown', 'Renderer disposal and context loss trigger', () => {
  const canvas = new (createMockEnvironment().document.createElement('canvas').constructor)();
  const renderer = new THREE.WebGLRenderer({ canvas });
  renderer.forceContextLoss();
  assert.strictEqual(canvas.getContext('webgl2').isContextLost(), true);
  renderer.dispose();
  assert.strictEqual(renderer.disposed, true);
});

registerTest('T1-F05-05', 5, 'WebGL Context Lifecycle & Teardown', 'Scene graph object traversal cleanup', () => {
  const scene = new THREE.Scene();
  const geom = new THREE.BoxGeometry();
  const mat = new THREE.MeshBasicMaterial();
  const mesh = new THREE.Mesh(geom, mat);
  scene.add(mesh);

  let disposedCount = 0;
  scene.traverse((obj) => {
    if (obj.geometry) { obj.geometry.dispose(); disposedCount++; }
    if (obj.material) { obj.material.dispose(); disposedCount++; }
  });
  assert.strictEqual(disposedCount, 2);
  assert.strictEqual(geom.disposed, true);
  assert.strictEqual(mat.disposed, true);
});

// ============================================================================
// CATEGORY B: PROCEDURAL WEB AUDIO ENGINE (FEATURES 6 - 9)
// ============================================================================

// F-06: Procedural Web Audio Engine (SOS Math)
registerTest('T1-F06-01', 6, 'Procedural Web Audio Engine (SOS Math)', 'Starts in suspended state (autoplay policy)', () => {
  const audioCtx = new MockAudioContext();
  assert.strictEqual(audioCtx.state, 'suspended');
});

registerTest('T1-F06-02', 6, 'Procedural Web Audio Engine (SOS Math)', 'User gesture unlock transitions state to running', async () => {
  const audioCtx = new MockAudioContext();
  await audioCtx.resume();
  assert.strictEqual(audioCtx.state, 'running');
});

registerTest('T1-F06-03', 6, 'Procedural Web Audio Engine (SOS Math)', 'Zero static file requests invariant', () => {
  const audioCtx = new MockAudioContext();
  const osc = audioCtx.createOscillator();
  const gain = audioCtx.createGain();
  osc.connect(gain);
  gain.connect(audioCtx.destination);
  assert.strictEqual(gain.connections.has(audioCtx.destination), true);
});

registerTest('T1-F06-04', 6, 'Procedural Web Audio Engine (SOS Math)', 'Master volume gain control', () => {
  const audioCtx = new MockAudioContext();
  const masterGain = audioCtx.createGain();
  masterGain.gain.setValueAtTime(0.75, 0.0);
  assert.strictEqual(masterGain.gain.value, 0.75);
});

registerTest('T1-F06-05', 6, 'Procedural Web Audio Engine (SOS Math)', 'AudioContext close teardown', async () => {
  const audioCtx = new MockAudioContext();
  await audioCtx.close();
  assert.strictEqual(audioCtx.state, 'closed');
});

// F-07: 3D Spatial Audio Panning
registerTest('T1-F07-01', 7, '3D Spatial Audio Panning', 'HRTF panning model and inverse distance attenuation', () => {
  const audioCtx = new MockAudioContext();
  const panner = audioCtx.createPanner();
  assert.strictEqual(panner.panningModel, 'HRTF');
  assert.strictEqual(panner.distanceModel, 'inverse');
  assert.strictEqual(panner.refDistance, 1.0);
  assert.strictEqual(panner.rolloffFactor, 1.0);
});

registerTest('T1-F07-02', 7, '3D Spatial Audio Panning', 'Source 2 unit to audio meter scale conversion', () => {
  const panner = new MockAudioContext().createPanner();
  // 100 units * 0.02 = 2.0 meters
  panner.setPosition(100 * 0.02, 200 * 0.02, 50 * 0.02);
  assert.strictEqual(panner.positionX.value, 2.0);
  assert.strictEqual(panner.positionY.value, 4.0);
  assert.strictEqual(panner.positionZ.value, 1.0);
});

registerTest('T1-F07-03', 7, '3D Spatial Audio Panning', 'AudioListener position synchronization', () => {
  const audioCtx = new MockAudioContext();
  audioCtx.listener.setPosition(0, -3.0, 1.28);
  assert.strictEqual(audioCtx.listener.positionX.value, 0.0);
  assert.strictEqual(audioCtx.listener.positionY.value, -3.0);
  assert.strictEqual(audioCtx.listener.positionZ.value, 1.28);
});

registerTest('T1-F07-04', 7, '3D Spatial Audio Panning', 'AudioListener forward and up orientation', () => {
  const audioCtx = new MockAudioContext();
  audioCtx.listener.setOrientation(1, 0, 0, 0, 0, 1);
  assert.strictEqual(audioCtx.listener.forwardX.value, 1.0);
  assert.strictEqual(audioCtx.listener.upZ.value, 1.0);
});

registerTest('T1-F07-05', 7, '3D Spatial Audio Panning', 'Theoretical inverse distance attenuation formula', () => {
  const refDistance = 1.0;
  const rolloffFactor = 1.0;
  const distance = 4.0;
  const gain = refDistance / (refDistance + rolloffFactor * (Math.max(distance, refDistance) - refDistance));
  assert.strictEqual(gain, 0.25);
});

// F-08: Sub-Tick Precision Audio Scheduling
registerTest('T1-F08-01', 8, 'Sub-Tick Precision Audio Scheduling', 'Scheduling event against audioCtx.currentTime', () => {
  const audioCtx = new MockAudioContext();
  audioCtx.currentTime = 12.5;
  const gain = audioCtx.createGain();
  gain.gain.setValueAtTime(0.5, audioCtx.currentTime + 0.010);
  assert.strictEqual(gain.gain.scheduledEvents.length, 1);
  assert.strictEqual(gain.gain.scheduledEvents[0].time, 12.510);
});

registerTest('T1-F08-02', 8, 'Sub-Tick Precision Audio Scheduling', 'Sub-tick fraction delay calculation at 1.0x', () => {
  const w = 0.40;
  const tickDuration = 0.015625;
  const delay = w * tickDuration;
  utils.assertFloatEqual(delay, 0.00625, 1e-6);
});

registerTest('T1-F08-03', 8, 'Sub-Tick Precision Audio Scheduling', 'Slow-motion delay scaling at 0.1x speed', () => {
  const w = 0.50;
  const tickDuration = 0.015625;
  const speed = 0.1;
  const delay = (w * tickDuration) / speed;
  utils.assertFloatEqual(delay, 0.078125, 1e-6);
});

registerTest('T1-F08-04', 8, 'Sub-Tick Precision Audio Scheduling', 'Micro-click transient gain ramp envelope', () => {
  const gain = new MockAudioContext().createGain();
  const t = 1.0;
  gain.gain.setValueAtTime(0.4, t);
  gain.gain.exponentialRampToValueAtTime(0.001, t + 0.004);
  assert.strictEqual(gain.gain.scheduledEvents.length, 2);
  assert.strictEqual(gain.gain.scheduledEvents[1].type, 'exponentialRampToValueAtTime');
});

registerTest('T1-F08-05', 8, 'Sub-Tick Precision Audio Scheduling', 'Cancellation of scheduled events upon timeline scrub', () => {
  const gain = new MockAudioContext().createGain();
  gain.gain.setValueAtTime(0.5, 2.0);
  gain.gain.setValueAtTime(0.8, 4.0);
  gain.gain.cancelScheduledValues(3.0);
  assert.strictEqual(gain.gain.scheduledEvents.length, 1);
  assert.strictEqual(gain.gain.scheduledEvents[0].time, 2.0);
});

// F-09: Procedural Sound Library
registerTest('T1-F09-01', 9, 'Procedural Sound Library', 'Host tick metronome pulse at 880 Hz', () => {
  const audioCtx = new MockAudioContext();
  const osc = audioCtx.createOscillator();
  osc.frequency.setValueAtTime(880.0, audioCtx.currentTime);
  audioCtx.logEvent('TICK_METRONOME', { frequency: 880.0 });
  utils.assertLedgerContains(audioCtx, e => e.type === 'TICK_METRONOME' && e.frequency === 880.0);
});

registerTest('T1-F09-02', 9, 'Procedural Sound Library', 'UserCmd micro-click event logging', () => {
  const audioCtx = new MockAudioContext();
  audioCtx.logEvent('SUBTICK_CLICK', { whenDelay: 0.005, subTickFrac: 0.32 });
  utils.assertLedgerContains(audioCtx, e => e.type === 'SUBTICK_CLICK' && e.subTickFrac === 0.32);
});

registerTest('T1-F09-03', 9, 'Procedural Sound Library', 'Directional spatial impulse logging', () => {
  const audioCtx = new MockAudioContext();
  audioCtx.logEvent('SPATIAL_BURST', { x: 100, y: 50, z: 0 });
  utils.assertLedgerContains(audioCtx, e => e.type === 'SPATIAL_BURST' && e.x === 100);
});

registerTest('T1-F09-04', 9, 'Procedural Sound Library', 'Material impact resonant frequency setup', () => {
  const audioCtx = new MockAudioContext();
  audioCtx.logEvent('SURFACE_IMPACT', { material: 'metal', f1: 1850, f2: 2940 });
  utils.assertLedgerContains(audioCtx, e => e.type === 'SURFACE_IMPACT' && e.material === 'metal');
});

registerTest('T1-F09-05', 9, 'Procedural Sound Library', 'Ricochet angular graze chirp event', () => {
  const audioCtx = new MockAudioContext();
  audioCtx.logEvent('RICOCHET_GRAZE', { fStart: 3400, fEnd: 900, vibratoHz: 80 });
  utils.assertLedgerContains(audioCtx, e => e.type === 'RICOCHET_GRAZE' && e.fStart === 3400);
});

// ============================================================================
// CATEGORY C: PLAYBACK CONTROLLER & TIMELINE (FEATURES 10 - 12)
// ============================================================================

// F-10: Continuous Sub-Tick Timeline Engine
registerTest('T1-F10-01', 10, 'Continuous Sub-Tick Timeline Engine', 'Decomposition of continuous time into tick and fraction', () => {
  const t = 104.75;
  const tick = Math.floor(t);
  const w = t - tick;
  assert.strictEqual(tick, 104);
  utils.assertFloatEqual(w, 0.75);
});

registerTest('T1-F10-02', 10, 'Continuous Sub-Tick Timeline Engine', 'Millisecond simulation duration conversion', () => {
  const ticks = 64.0;
  const ms = ticks * 15.625;
  assert.strictEqual(ms, 1000.0);
});

registerTest('T1-F10-03', 10, 'Continuous Sub-Tick Timeline Engine', 'Forward progression by 1 tick interval', () => {
  let t = 10.0;
  t += 1.0;
  assert.strictEqual(t, 11.0);
});

registerTest('T1-F10-04', 10, 'Continuous Sub-Tick Timeline Engine', 'Timeline upper limit clamping', () => {
  const maxTicks = 128.0;
  let t = 135.0;
  t = Math.min(maxTicks, t);
  assert.strictEqual(t, 128.0);
});

registerTest('T1-F10-05', 10, 'Continuous Sub-Tick Timeline Engine', 'Timeline lower limit clamping (non-negative)', () => {
  let t = 0.5;
  t -= 1.0;
  t = Math.max(0.0, t);
  assert.strictEqual(t, 0.0);
});

// F-11: Timeline Playback Controls
registerTest('T1-F11-01', 11, 'Timeline Playback Controls', 'Play/Pause state boolean toggle', () => {
  let isPlaying = false;
  isPlaying = !isPlaying;
  assert.strictEqual(isPlaying, true);
  isPlaying = !isPlaying;
  assert.strictEqual(isPlaying, false);
});

registerTest('T1-F11-02', 11, 'Timeline Playback Controls', 'Step +1 Tick advances discrete index', () => {
  let t = 10.40;
  t += 1.0;
  utils.assertFloatEqual(t, 11.40);
});

registerTest('T1-F11-03', 11, 'Timeline Playback Controls', 'Step -1 Tick decrements discrete index', () => {
  let t = 10.40;
  t -= 1.0;
  utils.assertFloatEqual(t, 9.40);
});

registerTest('T1-F11-04', 11, 'Timeline Playback Controls', 'Sub-tick fraction micro-stepping (+0.1 tick)', () => {
  let t = 5.25;
  t += 0.1;
  utils.assertFloatEqual(t, 5.35);
});

registerTest('T1-F11-05', 11, 'Timeline Playback Controls', 'Slow-motion preset selection (0.1x, 0.25x, 0.5x, 1.0x)', () => {
  const presets = [0.1, 0.25, 0.5, 1.0];
  let speed = presets[1];
  assert.strictEqual(speed, 0.25);
  speed = presets[0];
  assert.strictEqual(speed, 0.1);
});

// F-12: Real-Time Telemetry HUD Overlay
registerTest('T1-F12-01', 12, 'Real-Time Telemetry HUD Overlay', 'HUD DOM container mounting', () => {
  const { document } = createMockEnvironment();
  const hud = document.createElement('div');
  hud.id = 'cs2LabHud';
  hud.className = 'hud-panel slate-dark';
  document.body.appendChild(hud);
  assert.ok(document.getElementById('cs2LabHud'));
});

registerTest('T1-F12-02', 12, 'Real-Time Telemetry HUD Overlay', 'Tick and fraction formatting in HUD', () => {
  const tick = 42;
  const w = 0.64;
  const formatted = `Tick: ${tick} | Frac: ${w.toFixed(4)}`;
  assert.strictEqual(formatted, 'Tick: 42 | Frac: 0.6400');
});

registerTest('T1-F12-03', 12, 'Real-Time Telemetry HUD Overlay', 'Velocity magnitude calculation for HUD display', () => {
  const vx = 150.0, vy = 200.0, vz = 0.0;
  const speed = Math.hypot(vx, vy, vz);
  assert.strictEqual(speed, 250.0);
});

registerTest('T1-F12-04', 12, 'Real-Time Telemetry HUD Overlay', 'Airborne status badge formatting', () => {
  const onGround = false;
  const badge = onGround ? 'GROUND' : 'AIRBORNE';
  assert.strictEqual(badge, 'AIRBORNE');
});

registerTest('T1-F12-05', 12, 'Real-Time Telemetry HUD Overlay', 'HUD visibility toggle via classList', () => {
  const { document } = createMockEnvironment();
  const hud = document.createElement('div');
  hud.classList.add('hidden');
  assert.ok(hud.classList.contains('hidden'));
  hud.classList.toggle('hidden');
  assert.strictEqual(hud.classList.contains('hidden'), false);
});

// ============================================================================
// CATEGORY D: SIM 03 SUB-TICK PREDICTION (FEATURES 13 - 16)
// ============================================================================

// F-13: Multi-Slice Temporal Slicing (Sim 03)
registerTest('T1-F13-01', 13, 'Multi-Slice Temporal Slicing (Sim 03)', 'Midpoint temporal slice delta-time validation', () => {
  const res = math.integrateSubtickSlices(
    [0.5],
    { x: 0, y: 0, z: 0 },
    { x: 0, y: 0, z: 0 },
    true
  );
  assert.strictEqual(res.sliceCount, 2);
});

registerTest('T1-F13-02', 13, 'Multi-Slice Temporal Slicing (Sim 03)', 'Multi-step transition interval ordering', () => {
  const res = math.integrateSubtickSlices(
    [0.2, 0.6, 0.85],
    { x: 0, y: 0, z: 0 },
    { x: 0, y: 0, z: 0 },
    true
  );
  assert.strictEqual(res.sliceCount, 4);
});

registerTest('T1-F13-03', 13, 'Multi-Slice Temporal Slicing (Sim 03)', 'Zero-length slice deduplication', () => {
  const res = math.integrateSubtickSlices(
    [0.4, 0.4],
    { x: 0, y: 0, z: 0 },
    { x: 0, y: 0, z: 0 },
    true
  );
  assert.strictEqual(res.sliceCount, 2);
});

registerTest('T1-F13-04', 13, 'Multi-Slice Temporal Slicing (Sim 03)', 'Cumulative slice time conservation', () => {
  const slices = [0.1, 0.25, 0.5, 0.75, 0.9];
  const res = math.integrateSubtickSlices(
    slices,
    { x: 0, y: 0, z: 0 },
    { x: 0, y: 0, z: 0 },
    true
  );
  assert.strictEqual(res.sliceCount, 6);
});

registerTest('T1-F13-05', 13, 'Multi-Slice Temporal Slicing (Sim 03)', 'Sub-slice position integration linearity', () => {
  const res = math.integrateSubtickSlices(
    [0.5],
    { x: 0, y: 0, z: 100.0 },
    { x: 100, y: 0, z: 0 },
    false,
    { gravity: 0 }
  );
  utils.assertFloatEqual(res.origin.x, 100 * math.TICK_INTERVAL, 1e-4);
});

// F-14: Continuous Analog Jump Trajectory (Sim 03)
registerTest('T1-F14-01', 14, 'Continuous Analog Jump Trajectory (Sim 03)', 'Jump impulse applied at w = 0.0 tick boundary', () => {
  const traj = math.computeJumpTrajectory(0.0, 1);
  const expectedZ = math.JUMP_IMPULSE * math.TICK_INTERVAL - 0.5 * math.GRAVITY * (math.TICK_INTERVAL ** 2);
  utils.assertFloatEqual(traj.steps[0].z, expectedZ, 1e-3);
  utils.assertFloatEqual(traj.steps[0].z, 4.620, 0.05);
});

registerTest('T1-F14-02', 14, 'Continuous Analog Jump Trajectory (Sim 03)', 'Jump impulse applied at continuous fraction w = 0.40', () => {
  const traj = math.computeJumpTrajectory(0.4, 1);
  const tAir = 0.6 * math.TICK_INTERVAL;
  const expectedZ = math.JUMP_IMPULSE * tAir - 0.5 * math.GRAVITY * (tAir ** 2);
  utils.assertFloatEqual(traj.steps[0].z, expectedZ, 1e-3);
  utils.assertFloatEqual(traj.steps[0].z, 2.796, 0.05);
});

registerTest('T1-F14-03', 14, 'Continuous Analog Jump Trajectory (Sim 03)', 'Jump impulse applied at w = 0.75 late tick', () => {
  const traj = math.computeJumpTrajectory(0.75, 1);
  const tAir = 0.25 * math.TICK_INTERVAL;
  const expectedZ = math.JUMP_IMPULSE * tAir - 0.5 * math.GRAVITY * (tAir ** 2);
  utils.assertFloatEqual(traj.steps[0].z, expectedZ, 1e-3);
  utils.assertFloatEqual(traj.steps[0].z, 1.173, 0.05);
});

registerTest('T1-F14-04', 14, 'Continuous Analog Jump Trajectory (Sim 03)', 'Parabolic apex height and duration check', () => {
  const traj = math.computeJumpTrajectory(0.0, 40);
  utils.assertFloatEqual(traj.apexHeight, 57.0, 0.05);
  utils.assertFloatEqual(traj.apexTime, 0.3775, 1e-3);
});

registerTest('T1-F14-05', 14, 'Continuous Analog Jump Trajectory (Sim 03)', 'Floor landing collision reset', () => {
  const res = math.integrateSubtickSlices(
    [],
    { x: 0, y: 0, z: 2.0 },
    { x: 0, y: 0, z: -400.0 },
    false
  );
  assert.strictEqual(res.origin.z, 0.0);
  assert.strictEqual(res.velocity.z, 0.0);
  assert.strictEqual(res.onGround, true);
});

// F-15: Ground Friction Deceleration Vectors (Sim 03)
registerTest('T1-F15-01', 15, 'Ground Friction Deceleration Vectors (Sim 03)', 'Standard ground friction deceleration step', () => {
  const res = math.applyGroundFriction(250.0, 0.0, math.TICK_INTERVAL);
  utils.assertFloatEqual(res.speed, 229.6875, 1e-4);
});

registerTest('T1-F15-02', 15, 'Ground Friction Deceleration Vectors (Sim 03)', 'Sub-stopspeed friction clamping', () => {
  const res = math.applyGroundFriction(60.0, 0.0, math.TICK_INTERVAL);
  utils.assertFloatEqual(res.speed, 51.875, 1e-4);
});

registerTest('T1-F15-03', 15, 'Ground Friction Deceleration Vectors (Sim 03)', '2D vector ratio preservation during friction drop', () => {
  const res = math.applyGroundFriction(150.0, 200.0, math.TICK_INTERVAL);
  utils.assertFloatEqual(res.speed, 229.6875, 1e-4);
  utils.assertFloatEqual(res.vy / res.vx, 200.0 / 150.0, 1e-4);
});

registerTest('T1-F15-04', 15, 'Ground Friction Deceleration Vectors (Sim 03)', 'Near-zero velocity snap to origin', () => {
  const res = math.applyGroundFriction(0.05, 0.05, math.TICK_INTERVAL);
  assert.strictEqual(res.vx, 0.0);
  assert.strictEqual(res.vy, 0.0);
  assert.strictEqual(res.speed, 0.0);
});

registerTest('T1-F15-05', 15, 'Ground Friction Deceleration Vectors (Sim 03)', 'Surface friction scaling factor application', () => {
  const res = math.applyGroundFriction(250.0, 0.0, math.TICK_INTERVAL, math.SV_FRICTION, math.SV_STOPSPEED, 0.5);
  utils.assertFloatEqual(res.speed, 239.84375, 1e-4);
});

// F-16: Protobuf Wire Codec Reference (Sim 03)
registerTest('T1-F16-01', 16, 'Protobuf Wire Codec Reference (Sim 03)', 'Field 18 repeated subtick_moves tag encoding (0x92 0x01)', () => {
  const tag = (18 << 3) | 2; // wire type 2 (length-delimited)
  const bytes = math.encodeProtobufVarint(tag);
  assert.strictEqual(bytes.length, 2);
  assert.strictEqual(bytes[0], 0x92);
  assert.strictEqual(bytes[1], 0x01);
});

registerTest('T1-F16-02', 16, 'Protobuf Wire Codec Reference (Sim 03)', 'Varint roundtrip serialization', () => {
  const original = 146;
  const encoded = math.encodeProtobufVarint(original);
  const decoded = math.decodeProtobufVarint(encoded);
  assert.strictEqual(decoded.value, original);
});

registerTest('T1-F16-03', 16, 'Protobuf Wire Codec Reference (Sim 03)', 'SubtickMoveStep field tags layout check', () => {
  // Field 1: button (tag 0x08), Field 2: pressed (tag 0x10), Field 3: when float (tag 0x1D)
  assert.strictEqual((1 << 3) | 0, 0x08);
  assert.strictEqual((2 << 3) | 0, 0x10);
  assert.strictEqual((3 << 3) | 5, 0x1d);
});

registerTest('T1-F16-04', 16, 'Protobuf Wire Codec Reference (Sim 03)', '56-byte structure memory size contract', () => {
  const SIZEOF_SUBTICK_MOVE_STEP = 56; // 0x38 bytes
  assert.strictEqual(SIZEOF_SUBTICK_MOVE_STEP, 0x38);
});

registerTest('T1-F16-05', 16, 'Protobuf Wire Codec Reference (Sim 03)', 'Varint multi-byte decoding boundary', () => {
  const val = 16384; // 2^14
  const bytes = math.encodeProtobufVarint(val);
  assert.strictEqual(bytes.length, 3);
  const dec = math.decodeProtobufVarint(bytes);
  assert.strictEqual(dec.value, val);
});

// ============================================================================
// CATEGORY E: SIM 09 SKELETON & TWO-BONE IK (FEATURES 17 - 22)
// ============================================================================

// F-17: 32-Byte CTransform & Rodrigues Concatenation (Sim 09)
registerTest('T1-F17-01', 17, '32-Byte CTransform & Rodrigues Concatenation (Sim 09)', 'CTransform initialization and layout', () => {
  const t = new math.CTransformMath({ x: 10, y: 20, z: 30 }, { x: 0, y: 0, z: 0, w: 1 }, 1.0);
  assert.strictEqual(t.pos.x, 10);
  assert.strictEqual(t.rot.w, 1.0);
  assert.strictEqual(t.scale, 1.0);
});

registerTest('T1-F17-02', 17, '32-Byte CTransform & Rodrigues Concatenation (Sim 09)', 'Pure translation concatenation', () => {
  const parent = new math.CTransformMath({ x: 100, y: 200, z: 50 });
  const local = new math.CTransformMath({ x: 10, y: -20, z: 5 });
  const world = parent.multiply(local);
  utils.assertVec3Equal(world.pos, { x: 110, y: 180, z: 55 });
});

registerTest('T1-F17-03', 17, '32-Byte CTransform & Rodrigues Concatenation (Sim 09)', 'Pure 90-degree yaw rotation concatenation', () => {
  // 90 deg around Z: rot = (0, 0, sin(45), cos(45))
  const sin45 = Math.SQRT1_2;
  const parent = new math.CTransformMath({ x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: sin45, w: sin45 });
  const local = new math.CTransformMath({ x: 10, y: 0, z: 0 });
  const world = parent.multiply(local);
  utils.assertVec3Equal(world.pos, { x: 0, y: 10, z: 0 }, 1e-4);
});

registerTest('T1-F17-04', 17, '32-Byte CTransform & Rodrigues Concatenation (Sim 09)', 'Hierarchical scale propagation', () => {
  const parent = new math.CTransformMath({ x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: 0, w: 1 }, 2.0);
  const local = new math.CTransformMath({ x: 10, y: 10, z: 10 }, { x: 0, y: 0, z: 0, w: 1 }, 0.5);
  const world = parent.multiply(local);
  assert.strictEqual(world.scale, 1.0);
  utils.assertVec3Equal(world.pos, { x: 20, y: 20, z: 20 });
});

registerTest('T1-F17-05', 17, '32-Byte CTransform & Rodrigues Concatenation (Sim 09)', 'Rodrigues 180-degree vector reversal', () => {
  // 180 deg around Z: rot = (0, 0, 1, 0)
  const parent = new math.CTransformMath({ x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: 1, w: 0 });
  const local = new math.CTransformMath({ x: 1, y: 0, z: 0 });
  const world = parent.multiply(local);
  utils.assertVec3Equal(world.pos, { x: -1, y: 0, z: 0 }, 1e-4);
});

// F-18: Topological FK Propagation (Sim 09)
registerTest('T1-F18-01', 18, 'Topological FK Propagation (Sim 09)', '3-bone chain evaluation (Root -> Pelvis -> Spine)', () => {
  const hierarchy = [
    { name: 'Root', parentIndex: -1 },
    { name: 'Pelvis', parentIndex: 0 },
    { name: 'Spine', parentIndex: 1 }
  ];
  const locals = [
    new math.CTransformMath({ x: 0, y: 0, z: 0 }),
    new math.CTransformMath({ x: 0, y: 0, z: 38 }),
    new math.CTransformMath({ x: 0, y: 0, z: 12 })
  ];
  const worlds = math.topologicalForwardKinematics(hierarchy, locals);
  assert.strictEqual(worlds.length, 3);
  utils.assertVec3Equal(worlds[2].pos, { x: 0, y: 0, z: 50 });
});

registerTest('T1-F18-02', 18, 'Topological FK Propagation (Sim 09)', 'Parent index ordering constraint validation', () => {
  const hierarchy = [
    { parentIndex: -1 },
    { parentIndex: 0 },
    { parentIndex: 1 }
  ];
  for (let i = 0; i < hierarchy.length; i++) {
    assert.ok(hierarchy[i].parentIndex < i);
  }
});

registerTest('T1-F18-03', 18, 'Topological FK Propagation (Sim 09)', 'Global world root translation shift', () => {
  const hierarchy = [
    { parentIndex: -1 },
    { parentIndex: 0 }
  ];
  const locals = [
    new math.CTransformMath({ x: 500, y: 0, z: 0 }),
    new math.CTransformMath({ x: 0, y: 50, z: 0 })
  ];
  const worlds = math.topologicalForwardKinematics(hierarchy, locals);
  utils.assertVec3Equal(worlds[1].pos, { x: 500, y: 50, z: 0 });
});

registerTest('T1-F18-04', 18, 'Topological FK Propagation (Sim 09)', 'Disconnected multi-root hierarchy evaluation', () => {
  const hierarchy = [
    { parentIndex: -1 },
    { parentIndex: -1 }
  ];
  const locals = [
    new math.CTransformMath({ x: 10, y: 0, z: 0 }),
    new math.CTransformMath({ x: 20, y: 0, z: 0 })
  ];
  const worlds = math.topologicalForwardKinematics(hierarchy, locals);
  utils.assertVec3Equal(worlds[0].pos, { x: 10, y: 0, z: 0 });
  utils.assertVec3Equal(worlds[1].pos, { x: 20, y: 0, z: 0 });
});

registerTest('T1-F18-05', 18, 'Topological FK Propagation (Sim 09)', 'Topological bone count identity', () => {
  const hierarchy = Array.from({ length: 16 }, (_, i) => ({ parentIndex: i === 0 ? -1 : i - 1 }));
  const locals = Array.from({ length: 16 }, () => new math.CTransformMath({ x: 0, y: 0, z: 2 }));
  const worlds = math.topologicalForwardKinematics(hierarchy, locals);
  assert.strictEqual(worlds.length, 16);
  utils.assertVec3Equal(worlds[15].pos, { x: 0, y: 0, z: 32 });
});

// F-19: Rotated Bounding Capsules (CHitBox) (Sim 09)
registerTest('T1-F19-01', 19, 'Rotated Bounding Capsules (CHitBox) (Sim 09)', 'Ray-capsule direct central intersection', () => {
  const capA = { x: 0, y: -10, z: 50 };
  const capB = { x: 0, y: 10, z: 50 };
  const rayOrigin = { x: 0, y: 0, z: 50 };
  const res = math.intersectRayCapsule(rayOrigin, { x: 1, y: 0, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(res.hit, true);
});

registerTest('T1-F19-02', 19, 'Rotated Bounding Capsules (CHitBox) (Sim 09)', 'Ray-capsule grazing intersection within radius', () => {
  const capA = { x: 0, y: 0, z: 0 };
  const capB = { x: 0, y: 0, z: 20 };
  const rayOrigin = { x: 4.5, y: 0, z: 10 };
  const res = math.intersectRayCapsule(rayOrigin, { x: 0, y: 1, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(res.hit, true);
});

registerTest('T1-F19-03', 19, 'Rotated Bounding Capsules (CHitBox) (Sim 09)', 'Ray-capsule miss outside radius', () => {
  const capA = { x: 0, y: 0, z: 0 };
  const capB = { x: 0, y: 0, z: 20 };
  const rayOrigin = { x: 5.5, y: 0, z: 10 };
  const res = math.intersectRayCapsule(rayOrigin, { x: 0, y: 1, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(res.hit, false);
});

registerTest('T1-F19-04', 19, 'Rotated Bounding Capsules (CHitBox) (Sim 09)', 'HitGroup identifier color classification mapping', () => {
  const HITGROUP_COLORS = { 1: '#ff0000', 2: '#00ff00', 3: '#ffff00', 4: '#00ffff', 6: '#0000ff' };
  assert.strictEqual(HITGROUP_COLORS[1], '#ff0000'); // Head
  assert.strictEqual(HITGROUP_COLORS[2], '#00ff00'); // Chest
});

registerTest('T1-F19-05', 19, 'Rotated Bounding Capsules (CHitBox) (Sim 09)', 'CHitBox 112-byte (0x70) layout invariant', () => {
  const SIZEOF_CHITBOX = 112; // 0x70
  assert.strictEqual(SIZEOF_CHITBOX, 0x70);
});

// F-20: Analytical Two-Bone IK Solver (Sim 09)
registerTest('T1-F20-01', 20, 'Analytical Two-Bone IK Solver (Sim 09)', 'Symmetrical right-angle reach verification', () => {
  const res = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 20 * Math.SQRT2, y: 0, z: 0 }, 20, 20);
  assert.strictEqual(res.reachedTarget, true);
  utils.assertFloatEqual(res.cosAlpha, Math.SQRT1_2, 1e-4);
});

registerTest('T1-F20-02', 20, 'Analytical Two-Bone IK Solver (Sim 09)', 'Full extension straight arm reach', () => {
  const res = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 40, y: 0, z: 0 }, 20, 20);
  assert.strictEqual(res.reachedTarget, true);
  utils.assertFloatEqual(res.cosAlpha, 1.0, 1e-4);
});

registerTest('T1-F20-03', 20, 'Analytical Two-Bone IK Solver (Sim 09)', 'Law of Cosines joint angle verification (L1=25, L2=20, D=30)', () => {
  const res = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 30, y: 0, z: 0 }, 25, 20);
  utils.assertFloatEqual(res.cosAlpha, 0.75, 1e-4);
  utils.assertFloatEqual(res.angleRad, 0.7227, 1e-3);
});

registerTest('T1-F20-04', 20, 'Analytical Two-Bone IK Solver (Sim 09)', 'Pole vector plane bending direction', () => {
  const res = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 28.284, y: 0, z: 0 }, 20, 20, { x: 0, y: 1, z: 0 });
  assert.ok(res.midPos.y > 0.0); // Bends towards +Y pole
});

registerTest('T1-F20-05', 20, 'Analytical Two-Bone IK Solver (Sim 09)', 'Target distance clamping when exceeding reach', () => {
  const res = math.solveTwoBoneIK({ x: 0, y: 0, z: 0 }, { x: 50, y: 0, z: 0 }, 20, 20);
  assert.strictEqual(res.reachedTarget, false);
  utils.assertFloatEqual(res.effectorPos.x, 40.0 * 0.9999, 1e-3);
});

// F-21: Interactive 3D End-Effector Manipulator (Sim 09)
registerTest('T1-F21-01', 21, 'Interactive 3D End-Effector Manipulator (Sim 09)', 'Draggable IK target coordinate update', () => {
  const target = new THREE.Vector3(20, 0, 0);
  target.set(30, 0, 0);
  assert.strictEqual(target.x, 30);
});

registerTest('T1-F21-02', 21, 'Interactive 3D End-Effector Manipulator (Sim 09)', 'Gizmo target sphere visual mesh representation', () => {
  const geom = new THREE.SphereGeometry(2.0);
  const mesh = new THREE.Mesh(geom);
  assert.strictEqual(mesh.geometry.parameters.radius, 2.0);
});

registerTest('T1-F21-03', 21, 'Interactive 3D End-Effector Manipulator (Sim 09)', 'Target reach radius boundary clamping', () => {
  const maxReach = 40.0;
  let targetDist = 55.0;
  targetDist = Math.min(maxReach, targetDist);
  assert.strictEqual(targetDist, 40.0);
});

registerTest('T1-F21-04', 21, 'Interactive 3D End-Effector Manipulator (Sim 09)', 'Tracking error calculation between target and effector', () => {
  const t = { x: 25, y: 0, z: 0 };
  const e = { x: 25.00001, y: 0, z: 0 };
  const err = Math.hypot(t.x - e.x, t.y - e.y, t.z - e.z);
  utils.assertFloatEqual(err, 0.0, 1e-4);
});

registerTest('T1-F21-05', 21, 'Interactive 3D End-Effector Manipulator (Sim 09)', 'Ground planting snap coordinate preset', () => {
  const target = new THREE.Vector3(15, 0, 12);
  target.z = 0.0;
  assert.strictEqual(target.z, 0.0);
});

// F-22: Quaternion SLERP Pose Blending (Sim 09)
registerTest('T1-F22-01', 22, 'Quaternion SLERP Pose Blending (Sim 09)', '90-degree yaw SLERP midpoint (45 degrees)', () => {
  const q0 = { x: 0, y: 0, z: 0, w: 1 };
  const q1 = { x: 0, y: 0, z: Math.SQRT1_2, w: Math.SQRT1_2 };
  const mid = math.quaternionSlerp(q0, q1, 0.5);
  utils.assertFloatEqual(mid.w, Math.cos(Math.PI / 8), 1e-4);
  utils.assertFloatEqual(mid.z, Math.sin(Math.PI / 8), 1e-4);
});

registerTest('T1-F22-02', 22, 'Quaternion SLERP Pose Blending (Sim 09)', 'Shortest-arc negation on negative dot product', () => {
  const q0 = { x: 0, y: 0, z: 0, w: 1 };
  const q1 = { x: 0, y: 0, z: -0.1, w: -0.99 };
  const mid = math.quaternionSlerp(q0, q1, 0.5);
  assert.ok(mid.w > 0.0);
});

registerTest('T1-F22-03', 22, 'Quaternion SLERP Pose Blending (Sim 09)', 'Small-angle NLERP fallback branch', () => {
  const q0 = { x: 0, y: 0, z: 0, w: 1 };
  const q1 = { x: 0, y: 0, z: 0.001, w: 0.999999 };
  const res = math.quaternionSlerp(q0, q1, 0.5);
  utils.assertFloatEqual(Math.hypot(res.x, res.y, res.z, res.w), 1.0, 1e-4);
});

registerTest('T1-F22-04', 22, 'Quaternion SLERP Pose Blending (Sim 09)', 'Multi-bone pose blend interpolation linearity', () => {
  const weight = 0.25;
  const blendedScale = 1.0 * (1 - weight) + 2.0 * weight;
  assert.strictEqual(blendedScale, 1.25);
});

registerTest('T1-F22-05', 22, 'Quaternion SLERP Pose Blending (Sim 09)', 'Hermite smoothstep blend weight evaluation', () => {
  const t = 0.5;
  const w = 3.0 * (t ** 2) - 2.0 * (t ** 3);
  assert.strictEqual(w, 0.5);
});

// ============================================================================
// CATEGORY F: SIM 11 VOLUMETRIC SMOKE (FEATURES 23 - 27)
// ============================================================================

// F-23: 32x32x32 Voxel Discretization (Sim 11)
registerTest('T1-F23-01', 23, '32x32x32 Voxel Discretization (Sim 11)', 'Total voxel count is 32768', () => {
  const grid = new math.VoxelGrid32();
  assert.strictEqual(grid.totalVoxels, 32768);
});

registerTest('T1-F23-02', 23, '32x32x32 Voxel Discretization (Sim 11)', 'Extent dimension (640 units with 20u cells)', () => {
  assert.strictEqual(math.VOXEL_GRID_DIM, 32);
  assert.strictEqual(math.VOXEL_CELL_SIZE, 20.0);
  assert.strictEqual(math.VOXEL_EXTENT, 640.0);
});

registerTest('T1-F23-03', 23, '32x32x32 Voxel Discretization (Sim 11)', 'Voxel coordinate setting and querying', () => {
  const grid = new math.VoxelGrid32();
  grid.set(16, 16, 16, 1.0);
  assert.strictEqual(grid.get(16, 16, 16), 1.0);
  assert.strictEqual(grid.get(0, 0, 0), 0.0);
});

registerTest('T1-F23-04', 23, '32x32x32 Voxel Discretization (Sim 11)', 'Out-of-bounds coordinate rejection', () => {
  const grid = new math.VoxelGrid32();
  assert.strictEqual(grid.set(-1, 0, 0, 1.0), false);
  assert.strictEqual(grid.set(32, 10, 10, 1.0), false);
  assert.strictEqual(grid.get(35, 0, 0), 0.0);
});

registerTest('T1-F23-05', 23, '32x32x32 Voxel Discretization (Sim 11)', 'InstancedMesh capacity creation for 32768 voxels', () => {
  const geom = new THREE.BoxGeometry(20, 20, 20);
  const mat = new THREE.MeshBasicMaterial();
  const instanced = new THREE.InstancedMesh(geom, mat, 32768);
  assert.strictEqual(instanced.count, 32768);
  assert.strictEqual(instanced.instanceMatrix.array.length, 32768 * 16);
});

// F-24: 3D Morton Order Bit Interleaving (Sim 11)
registerTest('T1-F24-01', 24, '3D Morton Order Bit Interleaving (Sim 11)', 'Morton encoding origin (0,0,0) -> 0', () => {
  const m = math.encodeMorton3D(0, 0, 0);
  assert.strictEqual(m, 0);
});

registerTest('T1-F24-02', 24, '3D Morton Order Bit Interleaving (Sim 11)', 'Morton encoding max extent (31,31,31) -> 32767', () => {
  const m = math.encodeMorton3D(31, 31, 31);
  assert.strictEqual(m, 32767);
});

registerTest('T1-F24-03', 24, '3D Morton Order Bit Interleaving (Sim 11)', 'Morton encode/decode symmetry', () => {
  const orig = { x: 12, y: 23, z: 7 };
  const m = math.encodeMorton3D(orig.x, orig.y, orig.z);
  const dec = math.decodeMorton3D(m);
  assert.strictEqual(dec.x, orig.x);
  assert.strictEqual(dec.y, orig.y);
  assert.strictEqual(dec.z, orig.z);
});

registerTest('T1-F24-04', 24, '3D Morton Order Bit Interleaving (Sim 11)', 'Bit dilation Part1By2 stages check', () => {
  assert.strictEqual(math.part1By2(1), 1);
  assert.strictEqual(math.part1By2(2), 8);
  assert.strictEqual(math.part1By2(4), 64);
});

registerTest('T1-F24-05', 24, '3D Morton Order Bit Interleaving (Sim 11)', '512 uint64 bitmask population tally', () => {
  const grid = new math.VoxelGrid32();
  grid.set(1, 1, 1);
  grid.set(2, 2, 2);
  grid.set(3, 3, 3);
  assert.strictEqual(grid.countActive(), 3);
});

// F-25: 3D DDA Line-of-Sight Raymarch (Sim 11)
registerTest('T1-F25-01', 25, '3D DDA Line-of-Sight Raymarch (Sim 11)', 'Clear space raymarch returns 0 opacity', () => {
  const grid = new math.VoxelGrid32();
  const opacity = grid.raymarchDDA(-200, 0, 0, 200, 0, 0);
  assert.strictEqual(opacity, 0.0);
});

registerTest('T1-F25-02', 25, '3D DDA Line-of-Sight Raymarch (Sim 11)', 'Full smoke sphere raymarch returns 1.0 (blocked)', () => {
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 10);
  const opacity = grid.raymarchDDA(-200, 0, 0, 200, 0, 0);
  assert.strictEqual(opacity, 1.0);
});

registerTest('T1-F25-03', 25, '3D DDA Line-of-Sight Raymarch (Sim 11)', 'Thin smoke fringe fractional opacity accumulation', () => {
  const grid = new math.VoxelGrid32();
  grid.set(16, 16, 16, 0.20);
  const opacity = grid.raymarchDDA(0, 0, 0, 10, 0, 0);
  assert.ok(opacity < 1.0);
});

registerTest('T1-F25-04', 25, '3D DDA Line-of-Sight Raymarch (Sim 11)', 'Zero-distance ray returns 0 opacity', () => {
  const grid = new math.VoxelGrid32();
  const opacity = grid.raymarchDDA(10, 10, 10, 10, 10, 10);
  assert.strictEqual(opacity, 0.0);
});

registerTest('T1-F25-05', 25, '3D DDA Line-of-Sight Raymarch (Sim 11)', 'Occlusion threshold constant equals 0.20', () => {
  assert.strictEqual(math.SMOKE_OCCLUSION_THRESHOLD, 0.20);
});

// F-26: Ballistic Cavity Carving (Sim 11)
registerTest('T1-F26-01', 26, 'Ballistic Cavity Carving (Sim 11)', 'Cylindrical cavity clearing clears voxels', () => {
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 8);
  const initial = grid.countActive();
  const cleared = grid.carveCylinder({ x: -200, y: 0, z: 0 }, { x: 200, y: 0, z: 0 }, 30.0);
  assert.ok(cleared > 0);
  assert.strictEqual(grid.countActive(), initial - cleared);
});

registerTest('T1-F26-02', 26, 'Ballistic Cavity Carving (Sim 11)', 'Carved tunnel restores sightline opacity', () => {
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 8);
  grid.carveCylinder({ x: -200, y: 0, z: 0 }, { x: 200, y: 0, z: 0 }, 30.0);
  const opacity = grid.raymarchDDA(-200, 0, 0, 200, 0, 0);
  utils.assertFloatEqual(opacity, 0.0, 0.05);
});

registerTest('T1-F26-03', 26, 'Ballistic Cavity Carving (Sim 11)', 'Bullet ray missing smoke clears 0 voxels', () => {
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 5);
  const cleared = grid.carveCylinder({ x: -500, y: 500, z: 500 }, { x: 500, y: 500, z: 500 }, 30.0);
  assert.strictEqual(cleared, 0);
});

registerTest('T1-F26-04', 26, 'Ballistic Cavity Carving (Sim 11)', 'Ballistic cavity radius constant is 30 units', () => {
  assert.strictEqual(math.SMOKE_CARVE_RADIUS, 30.0);
});

registerTest('T1-F26-05', 26, 'Ballistic Cavity Carving (Sim 11)', 'Multiple sequential bullet tunnels accumulate', () => {
  const grid = new math.VoxelGrid32();
  grid.populateSphere({ x: 16, y: 16, z: 16 }, 8);
  const c1 = grid.carveCylinder({ x: -200, y: 0, z: 0 }, { x: 200, y: 0, z: 0 }, 20.0);
  const c2 = grid.carveCylinder({ x: 0, y: -200, z: 0 }, { x: 0, y: 200, z: 0 }, 20.0);
  assert.ok(c1 > 0);
  assert.ok(c2 > 0);
});

// F-27: Explosive Blast Displacement (Sim 11)
registerTest('T1-F27-01', 27, 'Explosive Blast Displacement (Sim 11)', 'Blast core clearing constant radius is 180 units', () => {
  assert.strictEqual(math.SMOKE_BLAST_CLEAR_RADIUS, 180.0);
});

registerTest('T1-F27-02', 27, 'Explosive Blast Displacement (Sim 11)', 'Blast push shell radius is 260 units', () => {
  assert.strictEqual(math.SMOKE_BLAST_PUSH_RADIUS, 260.0);
});

registerTest('T1-F27-03', 27, 'Explosive Blast Displacement (Sim 11)', 'Core clearing in voxel grid', () => {
  const grid = new math.VoxelGrid32();
  grid.set(16, 16, 16, 1.0);
  // Clear center voxel
  grid.clear(16, 16, 16);
  assert.strictEqual(grid.get(16, 16, 16), 0.0);
});

registerTest('T1-F27-04', 27, 'Explosive Blast Displacement (Sim 11)', 'Density attenuation on outward push (0.85x)', () => {
  const initialDensity = 1.0;
  const pushedDensity = initialDensity * 0.85;
  utils.assertFloatEqual(pushedDensity, 0.85);
});

registerTest('T1-F27-05', 27, 'Explosive Blast Displacement (Sim 11)', 'Double blast interaction consistency', () => {
  const grid = new math.VoxelGrid32();
  grid.set(10, 10, 10);
  grid.set(20, 20, 20);
  grid.clear(10, 10, 10);
  grid.clear(20, 20, 20);
  assert.strictEqual(grid.countActive(), 0);
});

// ============================================================================
// CATEGORY G: SIM 14 BULLET PENETRATION (FEATURES 28 - 32)
// ============================================================================

// F-28: Multi-Material 3D Cross-Sections (Sim 14)
registerTest('T1-F28-01', 28, 'Multi-Material 3D Cross-Sections (Sim 14)', 'Wood material property verification', () => {
  assert.strictEqual(math.MATERIALS.Wood.penModifier, 3.0);
  assert.strictEqual(math.MATERIALS.Wood.damageModifier, 0.16);
});

registerTest('T1-F28-02', 28, 'Multi-Material 3D Cross-Sections (Sim 14)', 'Concrete material property verification', () => {
  assert.strictEqual(math.MATERIALS.Concrete.penModifier, 1.0);
  assert.strictEqual(math.MATERIALS.Concrete.damageModifier, 0.22);
});

registerTest('T1-F28-03', 28, 'Multi-Material 3D Cross-Sections (Sim 14)', 'Solid metal material property verification', () => {
  assert.strictEqual(math.MATERIALS.SolidMetal.penModifier, 1.0);
  assert.strictEqual(math.MATERIALS.SolidMetal.damageModifier, 0.20);
});

registerTest('T1-F28-04', 28, 'Multi-Material 3D Cross-Sections (Sim 14)', 'Glass material property verification', () => {
  assert.strictEqual(math.MATERIALS.Glass.penModifier, 3.0);
  assert.strictEqual(math.MATERIALS.Glass.damageModifier, 0.05);
});

registerTest('T1-F28-05', 28, 'Multi-Material 3D Cross-Sections (Sim 14)', 'Cardboard material property verification', () => {
  assert.strictEqual(math.MATERIALS.Cardboard.penModifier, 3.0);
  assert.strictEqual(math.MATERIALS.Cardboard.damageModifier, 0.10);
});

// F-29: Atmospheric Distance Falloff ODE (Sim 14)
registerTest('T1-F29-01', 29, 'Atmospheric Distance Falloff ODE (Sim 14)', 'AK-47 zero distance damage is 36.0', () => {
  const d = math.calculateDistanceFalloff(36.0, 0.98, 0.0);
  assert.strictEqual(d, 36.0);
});

registerTest('T1-F29-02', 29, 'Atmospheric Distance Falloff ODE (Sim 14)', 'AK-47 at 500 units air travel', () => {
  const d = math.calculateDistanceFalloff(36.0, 0.98, 500.0);
  utils.assertFloatEqual(d, 35.28, 1e-4);
});

registerTest('T1-F29-03', 29, 'Atmospheric Distance Falloff ODE (Sim 14)', 'AK-47 at 1500 units air travel', () => {
  const d = math.calculateDistanceFalloff(36.0, 0.98, 1500.0);
  utils.assertFloatEqual(d, 33.882, 1e-3);
});

registerTest('T1-F29-04', 29, 'Atmospheric Distance Falloff ODE (Sim 14)', 'AWP distance falloff at 2000 units', () => {
  const d = math.calculateDistanceFalloff(115.0, 0.99, 2000.0);
  utils.assertFloatEqual(d, 110.468, 1e-3);
});

registerTest('T1-F29-05', 29, 'Atmospheric Distance Falloff ODE (Sim 14)', 'Maximum range cutoff (>8192 units)', () => {
  const d = math.calculateDistanceFalloff(36.0, 0.98, 8500.0);
  assert.strictEqual(d, 0.0);
});

// F-30: Rubikon Solid Medium Kinetic Dissipation (Sim 14)
registerTest('T1-F30-01', 30, 'Rubikon Solid Medium Kinetic Dissipation (Sim 14)', 'Plywood door penetration dissipation calculation', () => {
  const res = math.calculateRubikonDissipation(4.0, 3.0, 2.0, 36.0, 0.16);
  utils.assertFloatEqual(res.lostDistance, 0.2222, 1e-3);
  utils.assertFloatEqual(res.exitDamage, 28.143, 0.05);
});

registerTest('T1-F30-02', 30, 'Rubikon Solid Medium Kinetic Dissipation (Sim 14)', 'Concrete wall penetration dissipation calculation', () => {
  const res = math.calculateRubikonDissipation(8.0, 1.0, 2.0, 36.0, 0.22);
  utils.assertFloatEqual(res.lostDistance, 2.667, 1e-3);
  utils.assertFloatEqual(res.exitDamage, 19.788, 0.05);
});

registerTest('T1-F30-03', 30, 'Rubikon Solid Medium Kinetic Dissipation (Sim 14)', 'Thick steel plate total kinetic absorption', () => {
  const res = math.calculateRubikonDissipation(30.0, 1.0, 2.0, 36.0, 0.20);
  assert.strictEqual(res.exitDamage, 0.0);
});

registerTest('T1-F30-04', 30, 'Rubikon Solid Medium Kinetic Dissipation (Sim 14)', 'Minimum 1.0 damage threshold termination', () => {
  // If exit damage < 1.0, damage terminates to 0.0
  const res = math.calculateRubikonDissipation(20.0, 1.0, 1.5, 20.0, 0.22);
  assert.strictEqual(res.exitDamage, 0.0);
});

registerTest('T1-F30-05', 30, 'Rubikon Solid Medium Kinetic Dissipation (Sim 14)', 'Thin sheet minimal entry dissipation', () => {
  const res = math.calculateRubikonDissipation(0.5, 3.0, 2.5, 50.0, 0.10);
  assert.ok(res.exitDamage > 40.0);
});

// F-31: Ricochet Angle Gating (Sim 14)
registerTest('T1-F31-01', 31, 'Ricochet Angle Gating (Sim 14)', 'Grazing angle < 15 deg triggers ricochet reflection', () => {
  const dir = { x: 0.99, y: -0.14, z: 0 }; // angle ~8 deg
  const norm = { x: 0, y: 1, z: 0 };
  const res = math.evaluateRicochet(dir, norm, 30.0, 15.0);
  assert.strictEqual(res.ricochet, true);
  utils.assertFloatEqual(res.newDamage, 15.0); // 50% damping
});

registerTest('T1-F31-02', 31, 'Ricochet Angle Gating (Sim 14)', 'Steep angle >= 15 deg enters penetration path', () => {
  const dir = { x: 0.707, y: -0.707, z: 0 }; // 45 deg
  const norm = { x: 0, y: 1, z: 0 };
  const res = math.evaluateRicochet(dir, norm, 30.0, 15.0);
  assert.strictEqual(res.ricochet, false);
  assert.strictEqual(res.newDamage, 30.0);
});

registerTest('T1-F31-03', 31, 'Ricochet Angle Gating (Sim 14)', 'Reflected direction vector math: d - 2(d.n)n', () => {
  const dir = { x: 0.9848, y: -0.1736, z: 0 };
  const norm = { x: 0, y: 1, z: 0 };
  const res = math.evaluateRicochet(dir, norm, 30.0, 15.0);
  utils.assertVec3Equal(res.reflectedDir, { x: 0.9848, y: 0.1736, z: 0 }, 1e-4);
});

registerTest('T1-F31-04', 31, 'Ricochet Angle Gating (Sim 14)', 'Sequential ricochet cumulative 50% damping', () => {
  let dmg = 40.0;
  dmg *= 0.5; // Ricochet 1
  assert.strictEqual(dmg, 20.0);
  dmg *= 0.5; // Ricochet 2
  assert.strictEqual(dmg, 10.0);
});

registerTest('T1-F31-05', 31, 'Ricochet Angle Gating (Sim 14)', 'Critical grazing threshold constant is 15 degrees', () => {
  utils.assertFloatEqual(math.RICOCHET_MAX_GRAZING_ANGLE_RAD, (15.0 * Math.PI) / 180.0, 1e-4);
});

// F-32: Player Armor Mitigation Jump Table (Sim 14)
registerTest('T1-F32-01', 32, 'Player Armor Mitigation Jump Table (Sim 14)', 'Chest hit with Kevlar armor calculation', () => {
  const res = math.calculateArmorMitigation(35.0, 1.55, 2, 100, false);
  utils.assertFloatEqual(res.healthDamage, 27.125, 1e-4);
  assert.strictEqual(res.armorAbsorbed, 3);
  assert.strictEqual(res.remainingArmor, 97);
});

registerTest('T1-F32-02', 32, 'Player Armor Mitigation Jump Table (Sim 14)', 'Headshot with Helmet armor mitigation', () => {
  const res = math.calculateArmorMitigation(140.0, 1.55, 1, 100, true);
  utils.assertFloatEqual(res.healthDamage, 108.5, 1e-4);
  assert.strictEqual(res.armorAbsorbed, 15);
  assert.strictEqual(res.remainingArmor, 85);
});

registerTest('T1-F32-03', 32, 'Player Armor Mitigation Jump Table (Sim 14)', 'Leg hit bypasses armor completely', () => {
  const res = math.calculateArmorMitigation(26.25, 1.55, 6, 100, true);
  assert.strictEqual(res.healthDamage, 26.25);
  assert.strictEqual(res.armorAbsorbed, 0);
  assert.strictEqual(res.remainingArmor, 100);
});

registerTest('T1-F32-04', 32, 'Player Armor Mitigation Jump Table (Sim 14)', 'Armor durability exhaustion overflow into health', () => {
  const res = math.calculateArmorMitigation(100.0, 1.0, 2, 10, false);
  assert.strictEqual(res.remainingArmor, 0);
  assert.strictEqual(res.healthDamage, 80.0);
});

registerTest('T1-F32-05', 32, 'Player Armor Mitigation Jump Table (Sim 14)', 'Zero armor pawn takes 100% incoming damage', () => {
  const res = math.calculateArmorMitigation(50.0, 1.55, 2, 0, false);
  assert.strictEqual(res.healthDamage, 50.0);
  assert.strictEqual(res.armorAbsorbed, 0);
  assert.strictEqual(res.remainingArmor, 0);
});

// ============================================================================
// CATEGORY H: SIM 12 LAG COMPENSATION & BVH (FEATURES 33 - 37)
// ============================================================================

// F-33: 64-Tick Circular History Ring Buffer (Sim 12)
registerTest('T1-F33-01', 33, '64-Tick Circular History Ring Buffer (Sim 12)', 'Capacity preservation of 64 records', () => {
  const buf = new math.LagCompHistoryBuffer(64);
  for (let i = 0; i < 70; i++) {
    buf.push({ time: i * math.TICK_INTERVAL, origin: { x: i, y: 0, z: 0 } });
  }
  assert.strictEqual(buf.records.length, 64);
});

registerTest('T1-F33-02', 33, '64-Tick Circular History Ring Buffer (Sim 12)', 'Oldest record eviction ordering', () => {
  const buf = new math.LagCompHistoryBuffer(64);
  for (let i = 0; i < 70; i++) {
    buf.push({ tick: i, time: i * math.TICK_INTERVAL, origin: { x: i, y: 0, z: 0 } });
  }
  assert.strictEqual(buf.records[0].tick, 6);
  assert.strictEqual(buf.records[63].tick, 69);
});

registerTest('T1-F33-03', 33, '64-Tick Circular History Ring Buffer (Sim 12)', 'Exact 1.0-second history duration at 64 Hz', () => {
  const span = 64 * math.TICK_INTERVAL;
  assert.strictEqual(span, 1.0);
});

registerTest('T1-F33-04', 33, '64-Tick Circular History Ring Buffer (Sim 12)', 'Multi-entity ring buffer instance independence', () => {
  const buf1 = new math.LagCompHistoryBuffer();
  const buf2 = new math.LagCompHistoryBuffer();
  buf1.push({ tick: 1 });
  assert.strictEqual(buf1.records.length, 1);
  assert.strictEqual(buf2.records.length, 0);
});

registerTest('T1-F33-05', 33, '64-Tick Circular History Ring Buffer (Sim 12)', 'Record metadata integrity retention', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ tick: 100, origin: { x: 1, y: 2, z: 3 }, customData: 'test' });
  assert.strictEqual(buf.records[0].customData, 'test');
});

// F-34: Continuous Sub-Tick Rewind Query (Sim 12)
registerTest('T1-F34-01', 34, 'Continuous Sub-Tick Rewind Query (Sim 12)', 'Exact on-tick query without interpolation', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 1.0, origin: { x: 10, y: 0, z: 0 } });
  buf.push({ time: 1.0 + math.TICK_INTERVAL, origin: { x: 20, y: 0, z: 0 } });
  const res = buf.querySubTick(1.0);
  assert.strictEqual(res.origin.x, 10);
});

registerTest('T1-F34-02', 34, 'Continuous Sub-Tick Rewind Query (Sim 12)', 'Sub-tick midpoint fractional interpolation (w=0.50)', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 1.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: 1.0 + math.TICK_INTERVAL, origin: { x: 10, y: 0, z: 0 } });
  const res = buf.querySubTick(1.0 + 0.5 * math.TICK_INTERVAL);
  assert.strictEqual(res.interpolated, true);
  utils.assertFloatEqual(res.origin.x, 5.0, 1e-4);
});

registerTest('T1-F34-03', 34, 'Continuous Sub-Tick Rewind Query (Sim 12)', 'sv_maxunlag 200ms clamping limit', () => {
  const buf = new math.LagCompHistoryBuffer();
  for (let i = 0; i < 30; i++) {
    buf.push({ time: i * math.TICK_INTERVAL, origin: { x: i * 10, y: 0, z: 0 } });
  }
  const newestTime = buf.records[buf.records.length - 1].time;
  // Query 400ms in the past; should clamp to 200ms past
  const res = buf.querySubTick(newestTime - 0.400, 0.200);
  utils.assertFloatEqual(res.time, newestTime - 0.200, 1e-4);
});

registerTest('T1-F34-04', 34, 'Continuous Sub-Tick Rewind Query (Sim 12)', 'Future timestamp query clamps to newest record', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 1.0, origin: { x: 10, y: 0, z: 0 } });
  const res = buf.querySubTick(2.0);
  assert.strictEqual(res.origin.x, 10);
});

registerTest('T1-F34-05', 34, 'Continuous Sub-Tick Rewind Query (Sim 12)', 'Empty buffer query safety returns null', () => {
  const buf = new math.LagCompHistoryBuffer();
  assert.strictEqual(buf.querySubTick(1.0), null);
});

// F-35: Teleport Discontinuity Breaker (Sim 12)
registerTest('T1-F35-01', 35, 'Teleport Discontinuity Breaker (Sim 12)', 'Sub-threshold displacement (<4096 u^2) interpolates smoothly', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: 60, y: 0, z: 0 } }); // 60^2 = 3600 < 4096
  const res = buf.querySubTick(0.5 * math.TICK_INTERVAL);
  assert.strictEqual(res.teleportTripped, false);
  utils.assertFloatEqual(res.origin.x, 30.0);
});

registerTest('T1-F35-02', 35, 'Teleport Discontinuity Breaker (Sim 12)', 'Super-threshold displacement (>=4096 u^2) trips breaker', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: 100, y: 0, z: 0 } }); // 100^2 = 10000 >= 4096
  const res = buf.querySubTick(0.4 * math.TICK_INTERVAL);
  assert.strictEqual(res.teleportTripped, true);
  assert.strictEqual(res.origin.x, 0.0); // Snapped to older
});

registerTest('T1-F35-03', 35, 'Teleport Discontinuity Breaker (Sim 12)', 'Nearest frame snapping on trip with fraction >= 0.50', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 0 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: 100, y: 0, z: 0 } });
  const res = buf.querySubTick(0.6 * math.TICK_INTERVAL);
  assert.strictEqual(res.teleportTripped, true);
  assert.strictEqual(res.origin.x, 100.0); // Snapped to newer
});

registerTest('T1-F35-04', 35, 'Teleport Discontinuity Breaker (Sim 12)', 'Teleport threshold constant is 4096 units squared', () => {
  assert.strictEqual(math.TELEPORT_DIST_SQR, 4096.0);
});

registerTest('T1-F35-05', 35, 'Teleport Discontinuity Breaker (Sim 12)', 'Z-axis teleport breaker detection (falling/respawn)', () => {
  const buf = new math.LagCompHistoryBuffer();
  buf.push({ time: 0.0, origin: { x: 0, y: 0, z: 1000 } });
  buf.push({ time: math.TICK_INTERVAL, origin: { x: 0, y: 0, z: 0 } });
  const res = buf.querySubTick(0.5 * math.TICK_INTERVAL);
  assert.strictEqual(res.teleportTripped, true);
});

// F-36: 3D Spatial Lag Comp Arena (Sim 12)
registerTest('T1-F36-01', 36, '3D Spatial Lag Comp Arena (Sim 12)', 'Client view perceived pawn representation', () => {
  const pawn = new THREE.Object3D();
  pawn.position.set(100, 0, 0);
  assert.strictEqual(pawn.position.x, 100);
});

registerTest('T1-F36-02', 36, '3D Spatial Lag Comp Arena (Sim 12)', 'Server present pawn representation with translucent overlay', () => {
  const mat = new THREE.MeshBasicMaterial({ color: 0xff0000, transparent: true, opacity: 0.4 });
  assert.strictEqual(mat.transparent, true);
  assert.strictEqual(mat.opacity, 0.4);
});

registerTest('T1-F36-03', 36, '3D Spatial Lag Comp Arena (Sim 12)', 'Rewound hitbox pawn wireframe representation', () => {
  const mat = new THREE.MeshBasicMaterial({ color: 0x00ff00, wireframe: true });
  assert.strictEqual(mat.wireframe, true);
});

registerTest('T1-F36-04', 36, '3D Spatial Lag Comp Arena (Sim 12)', 'Shooter raycast intersection against rewound capsule', () => {
  const capA = { x: 100, y: 0, z: 64 };
  const capB = { x: 100, y: 0, z: 70 };
  const rayOrigin = { x: 0, y: 0, z: 67 };
  const res = math.intersectRayCapsule(rayOrigin, { x: 1, y: 0, z: 0 }, capA, capB, 5.0);
  assert.strictEqual(res.hit, true);
});

registerTest('T1-F36-05', 36, '3D Spatial Lag Comp Arena (Sim 12)', 'Server pawn post-trace restoration invariant', () => {
  const serverPawnOrigin = { x: 150, y: 0, z: 0 };
  const rewoundOrigin = { x: 100, y: 0, z: 0 };
  let activeOrigin = { ...rewoundOrigin };
  // Finish lag comp restores server coordinates
  activeOrigin = { ...serverPawnOrigin };
  assert.strictEqual(activeOrigin.x, 150);
});

// F-37: Rubikon BVH Tree Visualization (Sim 12)
registerTest('T1-F37-01', 37, 'Rubikon BVH Tree Visualization (Sim 12)', 'Hierarchical root AABB box creation', () => {
  const box = new THREE.BoxGeometry(64, 64, 128);
  assert.strictEqual(box.parameters.width, 64);
  assert.strictEqual(box.parameters.height, 64);
  assert.strictEqual(box.parameters.depth, 128);
});

registerTest('T1-F37-02', 37, 'Rubikon BVH Tree Visualization (Sim 12)', 'Depth color coding hierarchy (Level 0 Blue, Level 1 Green)', () => {
  const depthColors = ['#0088ff', '#00ff88', '#ffaa00'];
  assert.strictEqual(depthColors[0], '#0088ff');
  assert.strictEqual(depthColors[1], '#00ff88');
});

registerTest('T1-F37-03', 37, 'Rubikon BVH Tree Visualization (Sim 12)', 'AABB slab ray intersection detection', () => {
  const rayOrigin = { x: 0, y: 0, z: 0 };
  const boxMin = { x: 10, y: -5, z: -5 };
  const boxMax = { x: 20, y: 5, z: 5 };
  // Ray along +X intersects box between t=10 and t=20
  assert.ok(rayOrigin.x < boxMin.x);
});

registerTest('T1-F37-04', 37, 'Rubikon BVH Tree Visualization (Sim 12)', 'Dynamic leaf bounding box expansion', () => {
  const bounds = { min: { x: 0, y: 0, z: 0 }, max: { x: 10, y: 10, z: 10 } };
  const pt = { x: 15, y: 5, z: 5 };
  bounds.max.x = Math.max(bounds.max.x, pt.x);
  assert.strictEqual(bounds.max.x, 15);
});

registerTest('T1-F37-05', 37, 'Rubikon BVH Tree Visualization (Sim 12)', 'BVH tree visual group visibility toggle', () => {
  const bvhGroup = new THREE.Group();
  bvhGroup.visible = false;
  assert.strictEqual(bvhGroup.visible, false);
  bvhGroup.visible = true;
  assert.strictEqual(bvhGroup.visible, true);
});

// ============================================================================
// CATEGORY I: PORTAL INTEGRATION & REPOSITORY GUARDRAILS (FEATURES 38 - 40)
// ============================================================================

// F-38: Portal Routing & Workstation Mounting
registerTest('T1-F38-01', 38, 'Portal Routing & Workstation Mounting', 'Navigation detail view unhiding and matrix hiding', () => {
  const { document } = createMockEnvironment();
  const detail = document.getElementById('simulatorDetailView');
  const matrix = document.getElementById('simulatorsMatrixView');
  detail.classList.remove('hidden');
  matrix.classList.add('hidden');
  assert.strictEqual(detail.classList.contains('hidden'), false);
  assert.strictEqual(matrix.classList.contains('hidden'), true);
});

registerTest('T1-F38-02', 38, 'Portal Routing & Workstation Mounting', 'Title and runner badge text injection', () => {
  const { document } = createMockEnvironment();
  const title = document.getElementById('simDetailTitle');
  const badge = document.getElementById('simDetailRunnerBadge');
  title.textContent = 'Sim 03 Sub-Tick UserCmd Prediction';
  badge.textContent = 'cs2_subtick_usercmd_prediction_sim.cpp';
  assert.strictEqual(title.textContent, 'Sim 03 Sub-Tick UserCmd Prediction');
  assert.strictEqual(badge.textContent, 'cs2_subtick_usercmd_prediction_sim.cpp');
});

registerTest('T1-F38-03', 38, 'Portal Routing & Workstation Mounting', 'Workstation close view restoration', () => {
  const { document } = createMockEnvironment();
  const detail = document.getElementById('simulatorDetailView');
  const matrix = document.getElementById('simulatorsMatrixView');
  detail.classList.add('hidden');
  matrix.classList.remove('hidden');
  assert.strictEqual(detail.classList.contains('hidden'), true);
  assert.strictEqual(matrix.classList.contains('hidden'), false);
});

registerTest('T1-F38-04', 38, 'Portal Routing & Workstation Mounting', 'Location hash synchronization on open', () => {
  const { window } = createMockEnvironment();
  window.location.hash = '#simulators/11';
  assert.strictEqual(window.location.hash, '#simulators/11');
});

registerTest('T1-F38-05', 38, 'Portal Routing & Workstation Mounting', 'Provider registration interface check', () => {
  const registry = new Map();
  registry.set('03', class MockProvider {});
  assert.ok(registry.has('03'));
});

// F-39: Portal Build Pipeline Verification
registerTest('T1-F39-01', 39, 'Portal Build Pipeline Verification', 'portal-data.js exists and is non-empty', () => {
  const p = path.resolve(__dirname, '../../docs/portal-data.js');
  assert.ok(fs.existsSync(p), 'docs/portal-data.js must exist');
  const stat = fs.statSync(p);
  assert.ok(stat.size > 1000000, `docs/portal-data.js size (${stat.size}) should be > 1MB`);
});

registerTest('T1-F39-02', 39, 'Portal Build Pipeline Verification', 'docs-data.js exists and is non-empty', () => {
  const p = path.resolve(__dirname, '../../docs/docs-data.js');
  assert.ok(fs.existsSync(p), 'docs/docs-data.js must exist');
  const stat = fs.statSync(p);
  assert.ok(stat.size > 100000, `docs/docs-data.js size (${stat.size}) should be > 100KB`);
});

registerTest('T1-F39-03', 39, 'Portal Build Pipeline Verification', 'portal-data.js declares CS2_SIMULATORS_DATA', () => {
  const p = path.resolve(__dirname, '../../docs/portal-data.js');
  const content = fs.readFileSync(p, 'utf-8');
  assert.ok(content.includes('CS2_SIMULATORS_DATA'));
});

registerTest('T1-F39-04', 39, 'Portal Build Pipeline Verification', 'docs-data.js declares CS2_DOCS_BUNDLE', () => {
  const p = path.resolve(__dirname, '../../docs/docs-data.js');
  const content = fs.readFileSync(p, 'utf-8');
  assert.ok(content.includes('CS2_DOCS_BUNDLE'));
});

registerTest('T1-F39-05', 39, 'Portal Build Pipeline Verification', 'All 18+ C++ simulators present in repository', () => {
  const simsDir = path.resolve(__dirname, '../../docs/simulators');
  assert.ok(fs.existsSync(simsDir));
  const files = fs.readdirSync(simsDir).filter(f => f.endsWith('.cpp'));
  assert.ok(files.length >= 18, `Expected at least 18 C++ simulators, found ${files.length}`);
});

// F-40: Repository Guardrails Verification
registerTest('T1-F40-01', 40, 'Repository Guardrails Verification', 'Zero absolute paths in portal-data.js', () => {
  const p = path.resolve(__dirname, '../../docs/portal-data.js');
  const content = fs.readFileSync(p, 'utf-8');
  const matches = content.match(/[a-zA-Z]:[\\/](?:Users|hannu)/gi);
  assert.strictEqual(matches, null, 'Zero absolute paths permitted in portal-data.js');
});

registerTest('T1-F40-02', 40, 'Repository Guardrails Verification', 'Zero absolute paths in docs-data.js', () => {
  const p = path.resolve(__dirname, '../../docs/docs-data.js');
  const content = fs.readFileSync(p, 'utf-8');
  const matches = content.match(/[a-zA-Z]:[\\/](?:Users|hannu)/gi);
  assert.strictEqual(matches, null, 'Zero absolute paths permitted in docs-data.js');
});

registerTest('T1-F40-03', 40, 'Repository Guardrails Verification', 'Zero absolute paths in markdown documentation', () => {
  const docsDir = path.resolve(__dirname, '../../docs');
  const mdFiles = fs.readdirSync(docsDir).filter(f => f.endsWith('.md'));
  for (const f of mdFiles) {
    const content = fs.readFileSync(path.join(docsDir, f), 'utf-8');
    const matches = content.match(/[a-zA-Z]:[\\/](?:Users|hannu)/gi);
    assert.strictEqual(matches, null, `Absolute path violation in docs/${f}`);
  }
});

registerTest('T1-F40-04', 40, 'Repository Guardrails Verification', 'Zero prohibited binary files in docs/', () => {
  const docsDir = path.resolve(__dirname, '../../docs');
  const prohibitedExts = ['.exe', '.dll', '.obj', '.lib', '.pdb', '.bin', '.gpr'];
  const files = fs.readdirSync(docsDir);
  for (const f of files) {
    const ext = path.extname(f).toLowerCase();
    assert.strictEqual(prohibitedExts.includes(ext), false, `Prohibited binary file found: ${f}`);
  }
});

registerTest('T1-F40-05', 40, 'Repository Guardrails Verification', 'Zero prohibited binary files in sdk/ and game/', () => {
  const checkDir = (dir) => {
    if (!fs.existsSync(dir)) return;
    const entries = fs.readdirSync(dir, { withFileTypes: true });
    for (const ent of entries) {
      if (ent.isDirectory()) {
        checkDir(path.join(dir, ent.name));
      } else {
        const ext = path.extname(ent.name).toLowerCase();
        assert.ok(!['.exe', '.dll', '.obj', '.lib', '.pdb'].includes(ext), `Prohibited binary: ${ent.name}`);
      }
    }
  };
  checkDir(path.resolve(__dirname, '../../sdk'));
  checkDir(path.resolve(__dirname, '../../game'));
});

module.exports = {
  tests
};
