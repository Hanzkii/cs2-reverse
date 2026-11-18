#!/usr/bin/env node
/**
 * CS2 Physics & Telemetry Lab — E2E Test Runner
 * Headless, zero-dependency, automated opaque-box test runner.
 * Features: Tier filtering, Feature filtering, Verbose reporting, JSON export, Checklist.
 */

'use strict';

const process = require('node:process');
const { performance } = require('node:perf_hooks');

const tier1 = require('./tier1_feature_coverage.test.js');
const tier2 = require('./tier2_boundary_corner.test.js');
const tier3 = require('./tier3_pairwise_combinations.test.js');
const tier4 = require('./tier4_real_world_workloads.test.js');

const FEATURE_NAMES = [
  'Three.js & OrbitControls Integration',
  'Dual Camera Rig (Perspective & Orthographic)',
  'Source 2 Debug Renderer Aesthetics',
  'Theater & Fullscreen Toggle',
  'WebGL Context Lifecycle & Teardown',
  'Procedural Web Audio Engine (SOS Math)',
  '3D Spatial Audio Panning',
  'Sub-Tick Precision Audio Scheduling',
  'Procedural Sound Library',
  'Continuous Sub-Tick Timeline Engine',
  'Timeline Playback Controls',
  'Real-Time Telemetry HUD Overlay',
  'Multi-Slice Temporal Slicing (Sim 03)',
  'Continuous Analog Jump Trajectory (Sim 03)',
  'Ground Friction Deceleration Vectors (Sim 03)',
  'Protobuf Wire Codec Reference (Sim 03)',
  '32-Byte CTransform & Rodrigues Concatenation (Sim 09)',
  'Topological FK Propagation (Sim 09)',
  'Rotated Bounding Capsules (CHitBox) (Sim 09)',
  'Analytical Two-Bone IK Solver (Sim 09)',
  'Interactive 3D End-Effector Manipulator (Sim 09)',
  'Quaternion SLERP Pose Blending (Sim 09)',
  '32x32x32 Voxel Discretization (Sim 11)',
  '3D Morton Order Bit Interleaving (Sim 11)',
  '3D DDA Line-of-Sight Raymarch (Sim 11)',
  'Ballistic Cavity Carving (Sim 11)',
  'Explosive Blast Displacement (Sim 11)',
  'Multi-Material 3D Cross-Sections (Sim 14)',
  'Atmospheric Distance Falloff ODE (Sim 14)',
  'Rubikon Solid Medium Kinetic Dissipation (Sim 14)',
  'Ricochet Angle Gating (Sim 14)',
  'Player Armor Mitigation Jump Table (Sim 14)',
  '64-Tick Circular History Ring Buffer (Sim 12)',
  'Continuous Sub-Tick Rewind Query (Sim 12)',
  'Teleport Discontinuity Breaker (Sim 12)',
  '3D Spatial Lag Comp Arena (Sim 12)',
  'Rubikon BVH Tree Visualization (Sim 12)',
  'Portal Routing & Workstation Mounting',
  'Portal Build Pipeline Verification',
  'Repository Guardrails Verification',
  'E2E Test Suite (Tiers 1-4) & TEST_READY.md'
];

function parseArgs(args) {
  const options = {
    tier: 'all',
    feature: null,
    verbose: false,
    json: false,
    checklist: false,
    help: false
  };

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === '--help' || arg === '-h') {
      options.help = true;
    } else if (arg === '--verbose' || arg === '-v') {
      options.verbose = true;
    } else if (arg === '--json') {
      options.json = true;
    } else if (arg === '--checklist') {
      options.checklist = true;
    } else if (arg === '--tier') {
      options.tier = args[++i] || 'all';
    } else if (arg.startsWith('--tier=')) {
      options.tier = arg.split('=')[1];
    } else if (arg === '--feature') {
      options.feature = args[++i] || null;
    } else if (arg.startsWith('--feature=')) {
      options.feature = arg.split('=')[1];
    }
  }
  return options;
}

function printHelp() {
  console.log(`
CS2 Physics & Telemetry Lab — E2E Test Runner CLI

Usage:
  node tests/e2e/runner.js [options]

Options:
  --tier <1|2|3|4|all>    Execute specific tier (default: all)
  --feature <id|name>     Filter tests by feature ID (1-41) or name keyword
  -v, --verbose           Print verbose details for every test case
  --json                  Output machine-readable telemetry JSON stream
  --checklist             Output comprehensive 41-feature verification checklist
  -h, --help              Show this help dialog
`);
}

async function runSuite() {
  const options = parseArgs(process.argv.slice(2));

  if (options.help) {
    printHelp();
    process.exit(0);
  }

  const allTests = [
    ...tier1.tests,
    ...tier2.tests,
    ...tier3.tests,
    ...tier4.tests
  ];

  // Filter tests by Tier
  let selectedTests = allTests;
  if (options.tier && options.tier.toLowerCase() !== 'all') {
    const tierNum = parseInt(options.tier, 10);
    if ([1, 2, 3, 4].includes(tierNum)) {
      selectedTests = selectedTests.filter(t => t.tier === tierNum);
    }
  }

  // Filter tests by Feature
  if (options.feature) {
    const query = options.feature.toLowerCase();
    const queryNum = parseInt(options.feature, 10);
    selectedTests = selectedTests.filter(t => {
      if (!Number.isNaN(queryNum) && t.featureId === queryNum) return true;
      if (t.featureName && t.featureName.toLowerCase().includes(query)) return true;
      if (t.name && t.name.toLowerCase().includes(query)) return true;
      return false;
    });
  }

  const tierMetrics = {
    1: { passed: 0, failed: 0, total: 0, duration: 0 },
    2: { passed: 0, failed: 0, total: 0, duration: 0 },
    3: { passed: 0, failed: 0, total: 0, duration: 0 },
    4: { passed: 0, failed: 0, total: 0, duration: 0 }
  };

  const featureStatuses = {};
  for (let f = 1; f <= 41; f++) {
    featureStatuses[f] = { passed: true, testCount: 0 };
  }

  const testResults = [];
  const startTime = performance.now();

  for (const test of selectedTests) {
    const tStart = performance.now();
    let passed = false;
    let error = null;

    try {
      await test.fn();
      passed = true;
    } catch (err) {
      error = err.message || String(err);
      passed = false;
    }

    const tDuration = performance.now() - tStart;
    const tier = test.tier;

    if (tierMetrics[tier]) {
      tierMetrics[tier].total++;
      if (passed) {
        tierMetrics[tier].passed++;
      } else {
        tierMetrics[tier].failed++;
      }
      tierMetrics[tier].duration += tDuration;
    }

    if (test.featureId && featureStatuses[test.featureId]) {
      featureStatuses[test.featureId].testCount++;
      if (!passed) {
        featureStatuses[test.featureId].passed = false;
      }
    }

    testResults.push({
      id: test.id,
      tier: test.tier,
      featureId: test.featureId,
      name: test.name,
      passed,
      duration: tDuration,
      error
    });

    if (options.verbose && !options.json) {
      const statusStr = passed ? '[PASS]' : '[FAIL]';
      console.log(`  ${statusStr} [Tier ${test.tier}] ${test.id}: ${test.name} (${tDuration.toFixed(2)} ms)`);
      if (!passed && error) {
        console.error(`         Error: ${error}`);
      }
    }
  }

  const totalDuration = performance.now() - startTime;
  const totalPassed = selectedTests.filter(t => testResults.find(r => r.id === t.id && r.passed)).length;
  const totalFailed = selectedTests.length - totalPassed;
  const allPassed = totalFailed === 0 && selectedTests.length > 0;

  // Feature 41 evaluates complete E2E test suite pass status
  featureStatuses[41] = { passed: allPassed, testCount: selectedTests.length };

  if (options.json) {
    const jsonOutput = {
      runtime: {
        nodeVersion: process.version,
        platform: process.platform,
        headless: true
      },
      summary: {
        totalTests: selectedTests.length,
        passed: totalPassed,
        failed: totalFailed,
        durationMs: totalDuration,
        status: allPassed ? 'PASSED' : 'FAILED'
      },
      tiers: tierMetrics,
      features: Object.entries(featureStatuses).map(([id, st]) => ({
        id: parseInt(id, 10),
        name: FEATURE_NAMES[parseInt(id, 10) - 1] || `Feature ${id}`,
        status: st.passed ? 'PASS' : 'FAIL',
        testCount: st.testCount
      })),
      results: options.verbose ? testResults : undefined
    };
    console.log(JSON.stringify(jsonOutput, null, 2));
  } else {
    console.log('='.repeat(80));
    console.log('CS2 PHYSICS & TELEMETRY LAB — E2E TEST RUNNER');
    console.log('='.repeat(80));
    console.log(`Runtime: Node.js ${process.version} | Platform: ${process.platform} | Headless: True`);
    console.log('Mock Harness: WebGL2 (MockWebGLRenderingContext) | Audio: Web Audio API (SOS Mock)');
    console.log('-'.repeat(80));
    console.log('TIER EXECUTION SUMMARY:');
    console.log(`  Tier 1 (Feature Coverage):        ${String(tierMetrics[1].passed).padStart(3)} / ${String(tierMetrics[1].total).padStart(3)} passed (${tierMetrics[1].total ? ((tierMetrics[1].passed / tierMetrics[1].total) * 100).toFixed(1) : 0.0}%) [${tierMetrics[1].duration.toFixed(0)} ms]`);
    console.log(`  Tier 2 (Boundary & Corner Cases): ${String(tierMetrics[2].passed).padStart(3)} / ${String(tierMetrics[2].total).padStart(3)} passed (${tierMetrics[2].total ? ((tierMetrics[2].passed / tierMetrics[2].total) * 100).toFixed(1) : 0.0}%) [${tierMetrics[2].duration.toFixed(0)} ms]`);
    console.log(`  Tier 3 (Cross-Feature Pairwise):   ${String(tierMetrics[3].passed).padStart(3)} / ${String(tierMetrics[3].total).padStart(3)} passed (${tierMetrics[3].total ? ((tierMetrics[3].passed / tierMetrics[3].total) * 100).toFixed(1) : 0.0}%) [${tierMetrics[3].duration.toFixed(0)} ms]`);
    console.log(`  Tier 4 (Real-World Workloads):     ${String(tierMetrics[4].passed).padStart(3)} / ${String(tierMetrics[4].total).padStart(3)} passed (${tierMetrics[4].total ? ((tierMetrics[4].passed / tierMetrics[4].total) * 100).toFixed(1) : 0.0}%) [${tierMetrics[4].duration.toFixed(0)} ms]`);
    console.log('-'.repeat(80));
    console.log(`TOTAL: ${totalPassed} / ${selectedTests.length} passed (${totalFailed} failed, 0 skipped) in ${totalDuration.toFixed(0)} ms`);
    console.log(`STATUS: ${allPassed ? 'ALL TIERS PASSED [OK]' : 'FAILURES DETECTED [FAILED]'}`);
    console.log('-'.repeat(80));

    if (options.checklist || allPassed) {
      console.log('FEATURE VERIFICATION CHECKLIST:');
      for (let f = 1; f <= 41; f++) {
        const name = FEATURE_NAMES[f - 1] || `Feature ${f}`;
        const st = featureStatuses[f];
        const mark = st.passed ? '[PASS]' : '[FAIL]';
        console.log(`  ${mark} Feature ${String(f).padStart(2, '0')}: ${name}`);
      }
      console.log('='.repeat(80));
    }
  }

  process.exit(allPassed ? 0 : 1);
}

runSuite().catch(err => {
  console.error('Fatal Runner Error:', err);
  process.exit(1);
});
