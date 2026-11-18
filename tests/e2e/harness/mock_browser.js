/**
 * Headless Browser DOM, Lifecycle, and Sandboxing Environment
 * Provides Window, Document, HTMLElement, Canvas, ResizeObserver, rAF, performance.now
 */

'use strict';

const vm = require('node:vm');
const path = require('node:path');
const fs = require('node:fs');
const { MockWebGLRenderingContext } = require('./mock_webgl.js');

class MockDOMTokenList {
  constructor() {
    this._tokens = new Set();
  }

  add(...tokens) {
    for (const t of tokens) {
      if (t) this._tokens.add(String(t));
    }
  }

  remove(...tokens) {
    for (const t of tokens) {
      this._tokens.delete(String(t));
    }
  }

  contains(token) {
    return this._tokens.has(String(token));
  }

  toggle(token, force) {
    token = String(token);
    if (force !== undefined) {
      if (force) {
        this._tokens.add(token);
        return true;
      }
      this._tokens.delete(token);
      return false;
    }
    if (this._tokens.has(token)) {
      this._tokens.delete(token);
      return false;
    }
    this._tokens.add(token);
    return true;
  }

  toString() {
    return Array.from(this._tokens).join(' ');
  }

  get value() {
    return this.toString();
  }

  set value(val) {
    this._tokens.clear();
    if (val) {
      val.split(/\s+/).filter(Boolean).forEach(t => this._tokens.add(t));
    }
  }
}

class MockHTMLElement {
  constructor(tagName = 'div') {
    this.tagName = tagName.toUpperCase();
    this.id = '';
    this._className = '';
    this.classList = new MockDOMTokenList();
    this.style = {};
    this.attributes = new Map();
    this.children = [];
    this.childNodes = this.children;
    this.parentNode = null;
    this._innerHTML = '';
    this._textContent = '';
    this.value = '';

    this.clientWidth = 800;
    this.clientHeight = 600;
    this.offsetWidth = 800;
    this.offsetHeight = 600;

    this._listeners = new Map();
  }

  get className() {
    return this.classList.toString();
  }

  set className(val) {
    this._className = val;
    this.classList.value = val;
  }

  get innerHTML() {
    return this._innerHTML;
  }

  set innerHTML(val) {
    this._innerHTML = String(val);
    this.children = [];
    this.childNodes = this.children;
  }

  get textContent() {
    return this._textContent;
  }

  set textContent(val) {
    this._textContent = String(val);
  }

  appendChild(child) {
    if (!child) return null;
    if (child.parentNode) {
      child.parentNode.removeChild(child);
    }
    child.parentNode = this;
    this.children.push(child);
    return child;
  }

  removeChild(child) {
    const idx = this.children.indexOf(child);
    if (idx !== -1) {
      this.children.splice(idx, 1);
      child.parentNode = null;
      return child;
    }
    return null;
  }

  contains(child) {
    let curr = child;
    while (curr) {
      if (curr === this) return true;
      curr = curr.parentNode;
    }
    return false;
  }

  setAttribute(name, val) {
    this.attributes.set(name, String(val));
    if (name === 'id') this.id = String(val);
    if (name === 'class') this.className = String(val);
  }

  getAttribute(name) {
    return this.attributes.has(name) ? this.attributes.get(name) : null;
  }

  removeAttribute(name) {
    this.attributes.delete(name);
  }

  hasAttribute(name) {
    return this.attributes.has(name);
  }

  addEventListener(type, listener) {
    if (!this._listeners.has(type)) {
      this._listeners.set(type, []);
    }
    this._listeners.get(type).push(listener);
  }

  removeEventListener(type, listener) {
    const arr = this._listeners.get(type);
    if (arr) {
      const idx = arr.indexOf(listener);
      if (idx !== -1) arr.splice(idx, 1);
    }
  }

  dispatchEvent(event) {
    if (!event.target) event.target = this;
    const arr = this._listeners.get(event.type);
    if (arr) {
      for (const listener of [...arr]) {
        try {
          listener.call(this, event);
        } catch (err) {
          // preserve mock execution
        }
      }
    }
    return !event.defaultPrevented;
  }

  getBoundingClientRect() {
    return {
      top: 0,
      left: 0,
      right: this.clientWidth,
      bottom: this.clientHeight,
      width: this.clientWidth,
      height: this.clientHeight,
      x: 0,
      y: 0
    };
  }

  requestFullscreen() {
    return Promise.resolve();
  }

  querySelector(selector) {
    const list = this.querySelectorAll(selector);
    return list.length > 0 ? list[0] : null;
  }

  querySelectorAll(selector) {
    const results = [];
    const isId = selector.startsWith('#');
    const isClass = selector.startsWith('.');
    const targetName = selector.replace(/^[#.]/, '');

    const traverse = (node) => {
      for (const child of node.children) {
        if (isId && child.id === targetName) {
          results.push(child);
        } else if (isClass && child.classList.contains(targetName)) {
          results.push(child);
        } else if (!isId && !isClass && child.tagName.toLowerCase() === selector.toLowerCase()) {
          results.push(child);
        }
        traverse(child);
      }
    };

    traverse(this);
    return results;
  }
}

class MockHTMLCanvasElement extends MockHTMLElement {
  constructor() {
    super('canvas');
    this.width = 800;
    this.height = 600;
    this._webgl2Context = null;
    this._webglContext = null;
  }

  getContext(type, options) {
    if (type === 'webgl2') {
      if (!this._webgl2Context) {
        this._webgl2Context = new MockWebGLRenderingContext(this, options);
      }
      return this._webgl2Context;
    }
    if (type === 'webgl') {
      if (!this._webglContext) {
        this._webglContext = new MockWebGLRenderingContext(this, options);
      }
      return this._webglContext;
    }
    if (type === '2d') {
      return {
        canvas: this,
        fillRect: () => {},
        clearRect: () => {},
        getImageData: () => ({ data: new Uint8ClampedArray(4) }),
        putImageData: () => {}
      };
    }
    return null;
  }
}

class MockResizeObserver {
  constructor(callback) {
    this.callback = callback;
    this.targets = new Set();
  }

  observe(target) {
    this.targets.add(target);
  }

  unobserve(target) {
    this.targets.delete(target);
  }

  disconnect() {
    this.targets.clear();
  }

  trigger(width, height) {
    const entries = Array.from(this.targets).map((target) => {
      if (width !== undefined) target.clientWidth = width;
      if (height !== undefined) target.clientHeight = height;
      return {
        target,
        contentRect: {
          width: target.clientWidth,
          height: target.clientHeight,
          top: 0,
          left: 0
        }
      };
    });
    this.callback(entries, this);
  }
}

class MockDocument {
  constructor() {
    this.documentElement = new MockHTMLElement('html');
    this.head = new MockHTMLElement('head');
    this.body = new MockHTMLElement('body');
    this.documentElement.appendChild(this.head);
    this.documentElement.appendChild(this.body);

    this.fullscreenElement = null;
    this._listeners = new Map();
  }

  createElement(tagName) {
    if (tagName.toLowerCase() === 'canvas') {
      return new MockHTMLCanvasElement();
    }
    return new MockHTMLElement(tagName);
  }

  getElementById(id) {
    const search = (node) => {
      if (node.id === id) return node;
      for (const child of node.children) {
        const found = search(child);
        if (found) return found;
      }
      return null;
    };
    return search(this.documentElement);
  }

  querySelector(selector) {
    return this.documentElement.querySelector(selector);
  }

  querySelectorAll(selector) {
    return this.documentElement.querySelectorAll(selector);
  }

  addEventListener(type, listener) {
    if (!this._listeners.has(type)) this._listeners.set(type, []);
    this._listeners.get(type).push(listener);
  }

  removeEventListener(type, listener) {
    const arr = this._listeners.get(type);
    if (arr) {
      const idx = arr.indexOf(listener);
      if (idx !== -1) arr.splice(idx, 1);
    }
  }

  dispatchEvent(event) {
    const arr = this._listeners.get(event.type);
    if (arr) {
      for (const listener of [...arr]) {
        listener.call(this, event);
      }
    }
    return !event.defaultPrevented;
  }

  exitFullscreen() {
    this.fullscreenElement = null;
    return Promise.resolve();
  }
}

function createMockEnvironment(options = {}) {
  const document = new MockDocument();
  let virtualTime = 0;
  let nextFrameId = 1;
  const pendingFrames = new Map();

  const window = {
    location: {
      hash: '#simulators',
      href: 'https://hanzkii.github.io/cs2-reverse/docs/#simulators'
    },
    navigator: {
      userAgent: 'HeadlessMockBrowser/1.0'
    },
    devicePixelRatio: 1.0,
    innerWidth: 1920,
    innerHeight: 1080,
    document,
    ResizeObserver: MockResizeObserver,
    HTMLElement: MockHTMLElement,
    HTMLCanvasElement: MockHTMLCanvasElement,
    performance: {
      now() {
        return virtualTime;
      }
    },
    requestAnimationFrame(cb) {
      const id = nextFrameId++;
      pendingFrames.set(id, cb);
      return id;
    },
    cancelAnimationFrame(id) {
      pendingFrames.delete(id);
    },
    _advanceTime(ms) {
      virtualTime += ms;
      const callbacks = Array.from(pendingFrames.values());
      pendingFrames.clear();
      for (const cb of callbacks) {
        cb(virtualTime);
      }
    },
    localStorage: {
      _data: new Map(),
      getItem(k) { return this._data.has(k) ? this._data.get(k) : null; },
      setItem(k, v) { this._data.set(k, String(v)); },
      removeItem(k) { this._data.delete(k); },
      clear() { this._data.clear(); }
    },
    addEventListener(type, listener) {
      document.addEventListener(type, listener);
    },
    removeEventListener(type, listener) {
      document.removeEventListener(type, listener);
    },
    dispatchEvent(event) {
      return document.dispatchEvent(event);
    }
  };

  window.window = window;
  window.self = window;
  document.defaultView = window;

  // Mount standard workstation DOM layout for documentation portal
  const simVisualizerContainer = document.createElement('div');
  simVisualizerContainer.id = 'simVisualizerContainer';
  document.body.appendChild(simVisualizerContainer);

  const simParamsContainer = document.createElement('div');
  simParamsContainer.id = 'simParamsContainer';
  document.body.appendChild(simParamsContainer);

  const simulatorDetailView = document.createElement('div');
  simulatorDetailView.id = 'simulatorDetailView';
  document.body.appendChild(simulatorDetailView);

  const simulatorsMatrixView = document.createElement('div');
  simulatorsMatrixView.id = 'simulatorsMatrixView';
  document.body.appendChild(simulatorsMatrixView);

  const simDetailTitle = document.createElement('div');
  simDetailTitle.id = 'simDetailTitle';
  document.body.appendChild(simDetailTitle);

  const simDetailRunnerBadge = document.createElement('div');
  simDetailRunnerBadge.id = 'simDetailRunnerBadge';
  document.body.appendChild(simDetailRunnerBadge);

  return { window, document };
}

module.exports = {
  MockDOMTokenList,
  MockHTMLElement,
  MockHTMLCanvasElement,
  MockResizeObserver,
  MockDocument,
  createMockEnvironment
};
