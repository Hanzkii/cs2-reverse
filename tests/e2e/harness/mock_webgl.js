/**
 * Headless WebGL2 / WebGL1 State Machine Mock
 * Tracking draw calls, shaders, buffers, and context loss/restoration events.
 */

'use strict';

class MockWebGLRenderingContext {
  constructor(canvas, options = {}) {
    this.canvas = canvas;
    this.options = options;
    this._isContextLost = false;

    // WebGL Constants
    this.TRIANGLES = 0x0004;
    this.LINES = 0x0001;
    this.POINTS = 0x0000;
    this.LINE_STRIP = 0x0003;
    this.LINE_LOOP = 0x0002;
    this.TRIANGLE_STRIP = 0x0005;
    this.TRIANGLE_FAN = 0x0006;

    this.COLOR_BUFFER_BIT = 0x00004000;
    this.DEPTH_BUFFER_BIT = 0x00000100;
    this.STENCIL_BUFFER_BIT = 0x00000400;

    this.DEPTH_TEST = 0x0b71;
    this.BLEND = 0x0be2;
    this.CULL_FACE = 0x0b44;
    this.SCISSOR_TEST = 0x0c11;

    this.ARRAY_BUFFER = 0x8892;
    this.ELEMENT_ARRAY_BUFFER = 0x8893;
    this.STATIC_DRAW = 0x88e4;
    this.DYNAMIC_DRAW = 0x88e8;

    this.VERTEX_SHADER = 0x8b31;
    this.FRAGMENT_SHADER = 0x8b30;
    this.COMPILE_STATUS = 0x8b81;
    this.LINK_STATUS = 0x8b82;

    this.FLOAT = 0x1406;
    this.UNSIGNED_BYTE = 0x1401;
    this.UNSIGNED_SHORT = 0x1403;
    this.UNSIGNED_INT = 0x1405;

    // State tracking
    this.viewportX = 0;
    this.viewportY = 0;
    this.viewportWidth = canvas ? canvas.width || 800 : 800;
    this.viewportHeight = canvas ? canvas.height || 600 : 600;

    this.clearColorR = 0;
    this.clearColorG = 0;
    this.clearColorB = 0;
    this.clearColorA = 1;

    this.enabledCapabilities = new Set();
    this.boundBuffers = new Map();
    this.currentProgram = null;
    this.currentVAO = null;

    // Resource trackers & counters
    this.resources = {
      buffers: new Set(),
      shaders: new Set(),
      programs: new Set(),
      textures: new Set(),
      vertexArrays: new Set()
    };

    this.audit = {
      drawCalls: 0,
      trianglesDrawn: 0,
      linesDrawn: 0,
      pointsDrawn: 0,
      buffersCreated: 0,
      buffersDeleted: 0,
      programsCreated: 0,
      programsDeleted: 0,
      contextLossEvents: 0,
      contextRestoredEvents: 0
    };
  }

  isContextLost() {
    return this._isContextLost;
  }

  simulateContextLoss() {
    this._isContextLost = true;
    this.audit.contextLossEvents++;
    if (this.canvas && typeof this.canvas.dispatchEvent === 'function') {
      const event = {
        type: 'webglcontextlost',
        defaultPrevented: false,
        preventDefault() {
          this.defaultPrevented = true;
        }
      };
      this.canvas.dispatchEvent(event);
      return event.defaultPrevented;
    }
    return false;
  }

  simulateContextRestored() {
    this._isContextLost = false;
    this.audit.contextRestoredEvents++;
    if (this.canvas && typeof this.canvas.dispatchEvent === 'function') {
      const event = {
        type: 'webglcontextrestored',
        defaultPrevented: false,
        preventDefault() {
          this.defaultPrevented = true;
        }
      };
      this.canvas.dispatchEvent(event);
      return true;
    }
    return false;
  }

  viewport(x, y, w, h) {
    this.viewportX = x;
    this.viewportY = y;
    this.viewportWidth = w;
    this.viewportHeight = h;
  }

  clearColor(r, g, b, a) {
    this.clearColorR = r;
    this.clearColorG = g;
    this.clearColorB = b;
    this.clearColorA = a;
  }

  clear(mask) {
    if (this._isContextLost) return;
  }

  enable(cap) {
    this.enabledCapabilities.add(cap);
  }

  disable(cap) {
    this.enabledCapabilities.delete(cap);
  }

  blendFunc(sfactor, dfactor) {}
  depthFunc(func) {}
  cullFace(mode) {}
  scissor(x, y, w, h) {}

  createBuffer() {
    const buf = { id: Math.random().toString(36).slice(2), data: null, target: null };
    this.resources.buffers.add(buf);
    this.audit.buffersCreated++;
    return buf;
  }

  bindBuffer(target, buffer) {
    this.boundBuffers.set(target, buffer);
    if (buffer) buffer.target = target;
  }

  bufferData(target, data, usage) {
    const buf = this.boundBuffers.get(target);
    if (buf) {
      buf.data = data;
      buf.usage = usage;
    }
  }

  deleteBuffer(buffer) {
    if (buffer) {
      this.resources.buffers.delete(buffer);
      this.audit.buffersDeleted++;
      for (const [target, buf] of this.boundBuffers.entries()) {
        if (buf === buffer) this.boundBuffers.delete(target);
      }
    }
  }

  createShader(type) {
    const shader = { id: Math.random().toString(36).slice(2), type, source: '', compiled: true };
    this.resources.shaders.add(shader);
    return shader;
  }

  shaderSource(shader, source) {
    if (shader) shader.source = source;
  }

  compileShader(shader) {
    if (shader) shader.compiled = true;
  }

  getShaderParameter(shader, pname) {
    if (pname === this.COMPILE_STATUS) return true;
    return 1;
  }

  getShaderInfoLog(shader) {
    return '';
  }

  deleteShader(shader) {
    if (shader) this.resources.shaders.delete(shader);
  }

  createProgram() {
    const prog = {
      id: Math.random().toString(36).slice(2),
      shaders: [],
      uniforms: new Map(),
      attributes: new Map(),
      linked: true
    };
    this.resources.programs.add(prog);
    this.audit.programsCreated++;
    return prog;
  }

  attachShader(program, shader) {
    if (program && shader) program.shaders.push(shader);
  }

  linkProgram(program) {
    if (program) program.linked = true;
  }

  getProgramParameter(program, pname) {
    if (pname === this.LINK_STATUS) return true;
    return 1;
  }

  getProgramInfoLog(program) {
    return '';
  }

  useProgram(program) {
    this.currentProgram = program;
  }

  deleteProgram(program) {
    if (program) {
      this.resources.programs.delete(program);
      this.audit.programsDeleted++;
      if (this.currentProgram === program) this.currentProgram = null;
    }
  }

  getUniformLocation(program, name) {
    return { program, name, id: Math.random().toString(36).slice(2) };
  }

  getAttribLocation(program, name) {
    return 0;
  }

  createVertexArray() {
    const vao = { id: Math.random().toString(36).slice(2), attributes: [] };
    this.resources.vertexArrays.add(vao);
    return vao;
  }

  bindVertexArray(vao) {
    this.currentVAO = vao;
  }

  deleteVertexArray(vao) {
    if (vao) this.resources.vertexArrays.delete(vao);
  }

  drawArrays(mode, first, count) {
    if (this._isContextLost) return;
    this.audit.drawCalls++;
    if (mode === this.TRIANGLES) {
      this.audit.trianglesDrawn += Math.floor(count / 3);
    } else if (mode === this.LINES) {
      this.audit.linesDrawn += Math.floor(count / 2);
    } else {
      this.audit.pointsDrawn += count;
    }
  }

  drawElements(mode, count, type, offset) {
    if (this._isContextLost) return;
    this.audit.drawCalls++;
    if (mode === this.TRIANGLES) {
      this.audit.trianglesDrawn += Math.floor(count / 3);
    } else if (mode === this.LINES) {
      this.audit.linesDrawn += Math.floor(count / 2);
    } else {
      this.audit.pointsDrawn += count;
    }
  }

  drawArraysInstanced(mode, first, count, instanceCount) {
    if (this._isContextLost) return;
    this.audit.drawCalls++;
    if (mode === this.TRIANGLES) {
      this.audit.trianglesDrawn += Math.floor(count / 3) * instanceCount;
    } else if (mode === this.LINES) {
      this.audit.linesDrawn += Math.floor(count / 2) * instanceCount;
    }
  }

  drawElementsInstanced(mode, count, type, offset, instanceCount) {
    if (this._isContextLost) return;
    this.audit.drawCalls++;
    if (mode === this.TRIANGLES) {
      this.audit.trianglesDrawn += Math.floor(count / 3) * instanceCount;
    } else if (mode === this.LINES) {
      this.audit.linesDrawn += Math.floor(count / 2) * instanceCount;
    }
  }

  getExtension(name) {
    if (name === 'WEBGL_lose_context') {
      return {
        loseContext: () => this.simulateContextLoss(),
        restoreContext: () => this.simulateContextRestored()
      };
    }
    if (name === 'ANGLE_instanced_arrays') {
      return {
        drawArraysInstancedANGLE: (mode, first, count, primcount) =>
          this.drawArraysInstanced(mode, first, count, primcount),
        drawElementsInstancedANGLE: (mode, count, type, offset, primcount) =>
          this.drawElementsInstanced(mode, count, type, offset, primcount),
        vertexAttribDivisorANGLE: () => {}
      };
    }
    return null;
  }

  getParameter(pname) {
    if (pname === 0x1f02) return 'WebGL 2.0 (Headless Mock)';
    if (pname === 0x1f00) return 'Mock Vendor';
    if (pname === 0x1f01) return 'Mock Renderer';
    return 16;
  }
}

module.exports = {
  MockWebGLRenderingContext
};
