/**
 * Headless Procedural Web Audio API Engine & SOS Event Ledger
 * Zero external audio files; pure procedural DSP node graph simulation.
 */

'use strict';

class MockAudioParam {
  constructor(defaultValue = 1.0) {
    this.value = defaultValue;
    this.defaultValue = defaultValue;
    this.scheduledEvents = [];
  }

  setValueAtTime(val, startTime) {
    this.value = val;
    this.scheduledEvents.push({ type: 'setValueAtTime', value: val, time: startTime });
    return this;
  }

  linearRampToValueAtTime(val, endTime) {
    this.value = val;
    this.scheduledEvents.push({ type: 'linearRampToValueAtTime', value: val, time: endTime });
    return this;
  }

  exponentialRampToValueAtTime(val, endTime) {
    if (val <= 0) {
      throw new RangeError(`The value specified (${val}) must be a non-zero, positive number.`);
    }
    this.value = val;
    this.scheduledEvents.push({ type: 'exponentialRampToValueAtTime', value: val, time: endTime });
    return this;
  }

  cancelScheduledValues(startTime) {
    this.scheduledEvents = this.scheduledEvents.filter(e => e.time < startTime);
    return this;
  }
}

class MockAudioNode {
  constructor(context) {
    this.context = context;
    this.connections = new Set();
  }

  connect(destination) {
    this.connections.add(destination);
    return destination;
  }

  disconnect(destination) {
    if (destination) {
      this.connections.delete(destination);
    } else {
      this.connections.clear();
    }
  }
}

class MockGainNode extends MockAudioNode {
  constructor(context, defaultGain = 1.0) {
    super(context);
    this.gain = new MockAudioParam(defaultGain);
  }
}

class MockPannerNode extends MockAudioNode {
  constructor(context) {
    super(context);
    this.panningModel = 'HRTF';
    this.distanceModel = 'inverse';
    this.refDistance = 1.0;
    this.maxDistance = 10000.0;
    this.rolloffFactor = 1.0;

    this.positionX = new MockAudioParam(0.0);
    this.positionY = new MockAudioParam(0.0);
    this.positionZ = new MockAudioParam(0.0);

    this.orientationX = new MockAudioParam(1.0);
    this.orientationY = new MockAudioParam(0.0);
    this.orientationZ = new MockAudioParam(0.0);
  }

  setPosition(x, y, z) {
    this.positionX.value = x;
    this.positionY.value = y;
    this.positionZ.value = z;
  }

  setOrientation(x, y, z) {
    this.orientationX.value = x;
    this.orientationY.value = y;
    this.orientationZ.value = z;
  }
}

class MockOscillatorNode extends MockAudioNode {
  constructor(context) {
    super(context);
    this.type = 'sine';
    this.frequency = new MockAudioParam(440.0);
    this.started = false;
    this.stopped = false;
    this.startTime = null;
    this.stopTime = null;
  }

  start(when = 0) {
    this.started = true;
    this.startTime = when;
  }

  stop(when = 0) {
    this.stopped = true;
    this.stopTime = when;
  }
}

class MockBiquadFilterNode extends MockAudioNode {
  constructor(context) {
    super(context);
    this.type = 'lowpass';
    this.frequency = new MockAudioParam(350.0);
    this.Q = new MockAudioParam(1.0);
  }
}

class MockAudioBuffer {
  constructor(numberOfChannels, length, sampleRate) {
    this.numberOfChannels = numberOfChannels;
    this.length = length;
    this.sampleRate = sampleRate;
    this.duration = length / sampleRate;
    this._data = Array.from({ length: numberOfChannels }, () => new Float32Array(length));
  }

  getChannelData(channel) {
    return this._data[channel] || this._data[0];
  }
}

class MockAudioBufferSourceNode extends MockAudioNode {
  constructor(context) {
    super(context);
    this.buffer = null;
    this.playbackRate = new MockAudioParam(1.0);
    this.loop = false;
    this.started = false;
    this.stopped = false;
  }

  start(when = 0) {
    this.started = true;
  }

  stop(when = 0) {
    this.stopped = true;
  }
}

class MockAudioListener {
  constructor() {
    this.positionX = new MockAudioParam(0.0);
    this.positionY = new MockAudioParam(0.0);
    this.positionZ = new MockAudioParam(0.0);

    this.forwardX = new MockAudioParam(0.0);
    this.forwardY = new MockAudioParam(0.0);
    this.forwardZ = new MockAudioParam(-1.0);

    this.upX = new MockAudioParam(0.0);
    this.upY = new MockAudioParam(1.0);
    this.upZ = new MockAudioParam(0.0);
  }

  setPosition(x, y, z) {
    this.positionX.value = x;
    this.positionY.value = y;
    this.positionZ.value = z;
  }

  setOrientation(fx, fy, fz, ux, uy, uz) {
    this.forwardX.value = fx;
    this.forwardY.value = fy;
    this.forwardZ.value = fz;
    this.upX.value = ux;
    this.upY.value = uy;
    this.upZ.value = uz;
  }
}

class MockAudioContext {
  constructor(options = {}) {
    this.sampleRate = options.sampleRate || 44100;
    this.state = 'suspended'; // Autoplay compliance: starts suspended
    this.currentTime = 0.0;
    this.destination = new MockAudioNode(this);
    this.listener = new MockAudioListener();

    // SOS Event Ledger
    this._eventLog = [];
  }

  resume() {
    this.state = 'running';
    return Promise.resolve();
  }

  suspend() {
    this.state = 'suspended';
    return Promise.resolve();
  }

  close() {
    this.state = 'closed';
    return Promise.resolve();
  }

  createGain() {
    return new MockGainNode(this);
  }

  createPanner() {
    return new MockPannerNode(this);
  }

  createOscillator() {
    return new MockOscillatorNode(this);
  }

  createBiquadFilter() {
    return new MockBiquadFilterNode(this);
  }

  createBuffer(channels, length, sampleRate) {
    return new MockAudioBuffer(channels, length, sampleRate);
  }

  createBufferSource() {
    return new MockAudioBufferSourceNode(this);
  }

  logEvent(eventType, payload = {}) {
    const record = {
      id: this._eventLog.length + 1,
      timestamp: this.currentTime,
      type: eventType,
      ...payload
    };
    this._eventLog.push(record);
    return record;
  }

  queryEvents(filterFn) {
    if (!filterFn) return [...this._eventLog];
    return this._eventLog.filter(filterFn);
  }

  clearLedger() {
    this._eventLog.length = 0;
  }
}

module.exports = {
  MockAudioParam,
  MockAudioNode,
  MockGainNode,
  MockPannerNode,
  MockOscillatorNode,
  MockBiquadFilterNode,
  MockAudioBuffer,
  MockAudioBufferSourceNode,
  MockAudioListener,
  MockAudioContext
};
