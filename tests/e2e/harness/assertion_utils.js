/**
 * Tolerant Numerical and State Assertion Utilities
 * Machine epsilon comparisons (eps <= 1e-4), deep object tolerance, and SOS Event Ledger query assertions.
 */

'use strict';

const assert = require('node:assert');

function assertFloatEqual(actual, expected, epsilon = 1e-4, msg = '') {
  assert.ok(
    Number.isFinite(actual),
    `Expected actual value to be finite number, received: ${actual}. ${msg}`
  );
  assert.ok(
    Number.isFinite(expected),
    `Expected target value to be finite number, received: ${expected}. ${msg}`
  );
  const diff = Math.abs(actual - expected);
  assert.ok(
    diff <= epsilon,
    `Float equality mismatch: |${actual} - ${expected}| = ${diff} > epsilon (${epsilon}). ${msg}`
  );
}

function assertVec3Equal(actual, expected, epsilon = 1e-4, msg = '') {
  assert.ok(actual && typeof actual === 'object', `Expected Vector3 object, got: ${actual}`);
  assert.ok(expected && typeof expected === 'object', `Expected Vector3 object, got: ${expected}`);
  assertFloatEqual(actual.x, expected.x, epsilon, `${msg} (X coord)`);
  assertFloatEqual(actual.y, expected.y, epsilon, `${msg} (Y coord)`);
  assertFloatEqual(actual.z, expected.z, epsilon, `${msg} (Z coord)`);
}

function assertQuatEqual(actual, expected, epsilon = 1e-4, msg = '') {
  assert.ok(actual && typeof actual === 'object', `Expected Quaternion object, got: ${actual}`);
  assert.ok(expected && typeof expected === 'object', `Expected Quaternion object, got: ${expected}`);
  // Shortest arc check: q and -q represent same orientation
  const dot = Math.abs(
    actual.x * expected.x +
    actual.y * expected.y +
    actual.z * expected.z +
    actual.w * expected.w
  );
  assert.ok(
    Math.abs(dot - 1.0) <= epsilon,
    `Quaternion alignment mismatch: |dot| = ${dot}, expected ~1.0. ${msg}`
  );
}

function assertDeepCloseTo(actual, expected, epsilon = 1e-4, path = 'root') {
  if (typeof actual === 'number' && typeof expected === 'number') {
    assertFloatEqual(actual, expected, epsilon, `at ${path}`);
    return;
  }
  if (typeof actual !== typeof expected) {
    assert.fail(`Type mismatch at ${path}: ${typeof actual} !== ${typeof expected}`);
  }
  if (Array.isArray(actual) && Array.isArray(expected)) {
    assert.strictEqual(actual.length, expected.length, `Array length mismatch at ${path}`);
    for (let i = 0; i < actual.length; i++) {
      assertDeepCloseTo(actual[i], expected[i], epsilon, `${path}[${i}]`);
    }
    return;
  }
  if (actual && typeof actual === 'object' && expected && typeof expected === 'object') {
    const actualKeys = Object.keys(actual).sort();
    const expectedKeys = Object.keys(expected).sort();
    for (const key of expectedKeys) {
      assert.ok(key in actual, `Missing key ${key} at ${path}`);
      assertDeepCloseTo(actual[key], expected[key], epsilon, `${path}.${key}`);
    }
    return;
  }
  assert.strictEqual(actual, expected, `Strict equality mismatch at ${path}`);
}

function assertLedgerContains(ledger, filterFn, msg = '') {
  const events = Array.isArray(ledger) ? ledger : ledger._eventLog || [];
  const found = events.some(filterFn);
  assert.ok(found, `Expected ledger to contain matching event record. ${msg}`);
}

function assertLedgerEventCount(ledger, filterFn, expectedCount, msg = '') {
  const events = Array.isArray(ledger) ? ledger : ledger._eventLog || [];
  const matches = filterFn ? events.filter(filterFn) : events;
  assert.strictEqual(
    matches.length,
    expectedCount,
    `Expected ledger event count ${expectedCount}, received ${matches.length}. ${msg}`
  );
}

function assertNoNaNOrInf(obj, path = 'root') {
  if (typeof obj === 'number') {
    assert.ok(
      Number.isFinite(obj),
      `Non-finite numerical value detected at ${path}: ${obj}`
    );
    return;
  }
  if (Array.isArray(obj)) {
    for (let i = 0; i < obj.length; i++) {
      assertNoNaNOrInf(obj[i], `${path}[${i}]`);
    }
    return;
  }
  if (obj && typeof obj === 'object') {
    for (const [k, v] of Object.entries(obj)) {
      assertNoNaNOrInf(v, `${path}.${k}`);
    }
  }
}

function assertExitCode(actual, expected = 0) {
  assert.strictEqual(
    actual,
    expected,
    `Process exit code mismatch: received ${actual}, expected ${expected}`
  );
}

module.exports = {
  assertFloatEqual,
  assertVec3Equal,
  assertQuatEqual,
  assertDeepCloseTo,
  assertLedgerContains,
  assertLedgerEventCount,
  assertNoNaNOrInf,
  assertExitCode
};
