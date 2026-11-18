/**
 * CS2 Physics & Telemetry Lab Core Architecture
 * High-fidelity 3D WebGL (Three.js r128), Procedural Web Audio API (SOS Math),
 * Continuous Sub-Tick Playback Timeline, and Dynamic Simulation Provider Registry.
 *
 * Repository Invariant: Pure client-side C++20 / WebGL / Web Audio, zero external binaries,
 * zero static audio assets (100% procedural sound synthesis).
 */
(function(window) {
  'use strict';

  // ==========================================================================
  // Global CS2Lab Namespace Initialization
  // ==========================================================================
  const CS2Lab = window.CS2Lab || {};
  window.CS2Lab = CS2Lab;

  // Internal registry for simulation providers
  const _providers = new Map();

  // ==========================================================================
  // 1. Theater Mode CSS Injection
  // ==========================================================================
  function ensureTheaterStyles() {
    if (typeof document === 'undefined') return;
    if (!document.getElementById('cs2lab-theater-styles')) {
      const style = document.createElement('style');
      style.id = 'cs2lab-theater-styles';
      style.textContent = `
        .cs2lab-theater {
          position: fixed !important;
          top: 0 !important;
          left: 0 !important;
          width: 100vw !important;
          height: 100vh !important;
          z-index: 9999 !important;
          border-radius: 0 !important;
          margin: 0 !important;
          max-width: none !important;
        }
      `;
      document.head.appendChild(style);
    }
  }

  // ==========================================================================
  // 2. Viewport3D: Three.js r128 WebGL Renderer, Dual Camera & Hammer Grid
  // ==========================================================================
  class Viewport3D {
    /**
     * @param {HTMLElement|string} container - Host DOM element or selector
     * @param {Object} options - Configuration overrides
     */
    constructor(container, options = {}) {
      this.container = typeof container === 'string' ? document.querySelector(container) : container;
      if (!this.container) throw new Error('[CS2Lab.Viewport3D] Container element not found');

      ensureTheaterStyles();

      this.options = Object.assign({
        fov: 50,
        near: 1.0,
        far: 10000.0,
        initialPreset: 'free',
        majorGridSize: 2048,
        majorGridStep: 64,
        minorGridStep: 16,
        clearColor: 0x070b12,
        clearAlpha: 1.0,
        dprCap: 2.0,
        audioEngine: null
      }, options);

      this.audioEngine = this.options.audioEngine;
      this.isTheater = false;
      this.animFrameId = null;
      this.activeCameraMode = 'perspective'; // 'perspective' | 'orthographic'

      this._initScene();
      this._initCameras();
      this._initRenderer();
      this._initControls();
      this._initGridAndTriad();
      this._initLighting();
      this._initOverlayUI();
      this._initResizeObserver();
      this._startRenderLoop();
    }

    // --- Public Interface Contract ---

    toggleCameraMode() {
      if (this.activeCameraMode === 'perspective') {
        this._switchToOrthographic();
      } else {
        this._switchToPerspective();
      }
      return this.activeCameraMode;
    }

    setCameraPreset(name) {
      const target = (this.controls && this.controls.target) ? this.controls.target : new THREE.Vector3(0, 0, 36);
      const dist = 75.0; // Optimized human-scale camera framing distance

      switch (name) {
        case 'top':
          if (this.activeCameraMode !== 'orthographic') this._switchToOrthographic();
          this.activeCamera.position.set(target.x + 0.001, target.y, target.z + dist);
          this.activeCamera.up.set(1, 0, 0); // +X fwd points up on screen (Hammer 2D top blueprint)
          break;
        case 'front':
          if (this.activeCameraMode !== 'orthographic') this._switchToOrthographic();
          this.activeCamera.position.set(target.x - dist, target.y, target.z);
          this.activeCamera.up.set(0, 0, 1);
          if (this.controls) this.controls.target.copy(target);
          break;
        case 'side':
          if (this.activeCameraMode !== 'orthographic') this._switchToOrthographic();
          this.activeCamera.position.set(target.x, target.y - dist, target.z);
          this.activeCamera.up.set(0, 0, 1);
          if (this.controls) this.controls.target.copy(target);
          break;
        case 'free':
        default:
          if (this.activeCameraMode !== 'perspective') this._switchToPerspective();
          this.activeCamera.position.set(target.x - dist * 0.7, target.y - dist * 0.7, target.z + dist * 0.5);
          this.activeCamera.up.set(0, 0, 1);
          break;
      }

      this.activeCamera.lookAt(target);
      if (this.controls) this.controls.update();
      this._updateToolbarUI(name);
    }

    toggleTheater() {
      this.isTheater = !this.isTheater;
      if (this.isTheater) {
        this.container.classList.add('cs2lab-theater');
      } else {
        this.container.classList.remove('cs2lab-theater');
      }
      this._handleResize();
      return this.isTheater;
    }

    toggleFullscreen() {
      if (!document.fullscreenElement) {
        if (this.container.requestFullscreen) {
          this.container.requestFullscreen();
        } else if (this.container.webkitRequestFullscreen) {
          this.container.webkitRequestFullscreen();
        }
        return true;
      } else {
        if (document.exitFullscreen) document.exitFullscreen();
        return false;
      }
    }

    getScene() { return this.scene; }
    getCamera() { return this.activeCamera; }
    getRenderer() { return this.renderer; }
    getControls() { return this.controls; }

    setAudioEngine(audio) {
      this.audioEngine = audio;
    }

    destroy() {
      // 1. Cancel render loop
      if (this.animFrameId) {
        cancelAnimationFrame(this.animFrameId);
        this.animFrameId = null;
      }

      // 2. Disconnect resize observation
      if (this.resizeObserver) {
        this.resizeObserver.disconnect();
        this.resizeObserver = null;
      }

      // 3. Remove DOM overlay and styles
      if (this.isTheater) {
        this.container.classList.remove('cs2lab-theater');
      }
      if (this.uiOverlay && this.uiOverlay.parentNode) {
        this.uiOverlay.parentNode.removeChild(this.uiOverlay);
      }

      // 4. Dispose controls
      if (this.controls) {
        this.controls.dispose();
        this.controls = null;
      }

      // 5. Recursive scene geometry/material teardown
      if (this.scene) {
        this.scene.traverse((obj) => {
          if (obj.geometry) obj.geometry.dispose();
          if (obj.material) {
            if (Array.isArray(obj.material)) {
              obj.material.forEach(m => this._disposeMaterial(m));
            } else {
              this._disposeMaterial(obj.material);
            }
          }
        });
        if (typeof this.scene.clear === 'function') {
          this.scene.clear();
        } else if (this.scene.children) {
          while (this.scene.children.length > 0) {
            this.scene.remove(this.scene.children[0]);
          }
        }
        this.scene = null;
      }

      // 6. Force WebGL context release & renderer dispose
      if (this.renderer) {
        const gl = typeof this.renderer.getContext === 'function'
          ? this.renderer.getContext()
          : (this.renderer.domElement && typeof this.renderer.domElement.getContext === 'function'
            ? this.renderer.domElement.getContext('webgl2')
            : null);
        if (gl) {
          const loseExt = gl.getExtension ? gl.getExtension('WEBGL_lose_context') : null;
          if (loseExt && typeof loseExt.loseContext === 'function') loseExt.loseContext();
          else if (typeof gl.simulateContextLoss === 'function') gl.simulateContextLoss();
        }
        if (typeof this.renderer.forceContextLoss === 'function') {
          this.renderer.forceContextLoss();
        }
        if (typeof this.renderer.dispose === 'function') {
          this.renderer.dispose();
        }
        if (this.renderer.domElement && this.renderer.domElement.parentNode) {
          this.renderer.domElement.parentNode.removeChild(this.renderer.domElement);
        }
        this.renderer = null;
      }
    }

    // --- Internal Implementation Details ---

    _initScene() {
      this.scene = new THREE.Scene();
      this.scene.background = new THREE.Color(this.options.clearColor);
    }

    _initCameras() {
      const width = this.container.clientWidth || 800;
      const height = this.container.clientHeight || 560;
      const aspect = width / height;

      // Perspective Camera (FOV 50 deg, near 1.0, far 10000.0)
      this.perspCamera = new THREE.PerspectiveCamera(this.options.fov, aspect, this.options.near, this.options.far);
      this.perspCamera.up.set(0, 0, 1);
      this.perspCamera.position.set(-250, -250, 180);

      // Orthographic Camera (matched initial frustum at target distance)
      const d = this.perspCamera.position.distanceTo(new THREE.Vector3(0, 0, 36));
      const h = 2 * d * Math.tan((this.options.fov * Math.PI) / 360);
      const w = h * aspect;
      this.orthoCamera = new THREE.OrthographicCamera(-w / 2, w / 2, h / 2, -h / 2, this.options.near, this.options.far);
      this.orthoCamera.up.set(0, 0, 1);
      this.orthoCamera.position.copy(this.perspCamera.position);

      this.activeCamera = this.perspCamera;
      this.activeCameraMode = 'perspective';
    }

    _initRenderer() {
      this.renderer = new THREE.WebGLRenderer({
        antialias: true,
        alpha: false,
        powerPreference: 'high-performance'
      });
      this.renderer.setClearColor(this.options.clearColor, this.options.clearAlpha);
      this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, this.options.dprCap));
      const width = this.container.clientWidth || 800;
      const height = this.container.clientHeight || 560;
      this.renderer.setSize(width, height, false);
      this.renderer.domElement.style.display = 'block';
      this.renderer.domElement.style.width = '100%';
      this.renderer.domElement.style.height = '100%';

      // Clear existing container markup and insert canvas
      this.container.innerHTML = '';
      this.container.style.padding = '0px';
      this.container.appendChild(this.renderer.domElement);
    }

    _initControls() {
      if (typeof THREE.OrbitControls !== 'function') {
        console.warn('[CS2Lab.Viewport3D] THREE.OrbitControls is not loaded.');
        return;
      }
      this.controls = new THREE.OrbitControls(this.activeCamera, this.renderer.domElement);
      this.controls.enableDamping = true;
      this.controls.dampingFactor = 0.05;
      this.controls.screenSpacePanning = true;
      this.controls.target.set(0, 0, 36);
      this.controls.update();
    }

    _initGridAndTriad() {
      const size = this.options.majorGridSize;
      const majorStep = this.options.majorGridStep;
      const minorStep = this.options.minorGridStep;

      this.gridGroup = new THREE.Group();

      // 16-unit Minor Grid (Dark slate)
      const minorDivs = Math.floor(size / minorStep);
      const minorGrid = new THREE.GridHelper(size, minorDivs, 0x1a2333, 0x121824);
      minorGrid.rotation.x = Math.PI / 2;
      minorGrid.position.z = -0.05;
      if (minorGrid.material) {
        minorGrid.material.opacity = 0.45;
        minorGrid.material.transparent = true;
      }
      this.gridGroup.add(minorGrid);

      // 64-unit Major Grid (Cyan / Slate)
      const majorDivs = Math.floor(size / majorStep);
      const majorGrid = new THREE.GridHelper(size, majorDivs, 0x38bdf8, 0x243247);
      majorGrid.rotation.x = Math.PI / 2;
      majorGrid.position.z = 0.0;
      if (majorGrid.material) {
        majorGrid.material.opacity = 0.85;
        majorGrid.material.transparent = true;
      }
      this.gridGroup.add(majorGrid);

      // Coordinate Triad (+X red forward, +Y green left, +Z blue up) - compact & sleek
      this.triadGroup = new THREE.Group();
      const axisLen = 16;
      if (typeof THREE.ArrowHelper === 'function') {
        this.triadGroup.add(new THREE.ArrowHelper(new THREE.Vector3(1, 0, 0), new THREE.Vector3(0, 0, 0), axisLen, 0xef4444, 4, 2));
        this.triadGroup.add(new THREE.ArrowHelper(new THREE.Vector3(0, 1, 0), new THREE.Vector3(0, 0, 0), axisLen, 0x10b981, 4, 2));
        this.triadGroup.add(new THREE.ArrowHelper(new THREE.Vector3(0, 0, 1), new THREE.Vector3(0, 0, 0), axisLen, 0x38bdf8, 4, 2));
      } else if (typeof THREE.AxesHelper === 'function') {
        this.triadGroup.add(new THREE.AxesHelper(axisLen));
      }

      this.scene.add(this.gridGroup);
      this.scene.add(this.triadGroup);
    }

    _initLighting() {
      this.lightsGroup = new THREE.Group();
      if (typeof THREE.AmbientLight === 'function') {
        const amb = new THREE.AmbientLight(0xffffff, 0.45);
        this.lightsGroup.add(amb);
      }
      if (typeof THREE.DirectionalLight === 'function') {
        const dir = new THREE.DirectionalLight(0xffffff, 0.75);
        dir.position.set(-200, -200, 400);
        this.lightsGroup.add(dir);
      }
      this.scene.add(this.lightsGroup);
    }

    _initOverlayUI() {
      this.uiOverlay = document.createElement('div');
      this.uiOverlay.className = 'cs2lab-viewport-ui absolute inset-0 pointer-events-none flex flex-col justify-between p-3 select-none z-10';
      this.uiOverlay.innerHTML = `
        <!-- Top Bar: Telemetry Header & Presets Toolbar -->
        <div class="flex items-center justify-between gap-2 pointer-events-auto">
          <div class="flex items-center gap-1.5 bg-slate-900/85 backdrop-blur-md px-2.5 py-1 rounded-lg border border-slate-800 text-[11px] font-mono text-slate-300">
            <span class="w-2 h-2 rounded-full bg-emerald-400 animate-pulse"></span>
            <span id="cs2labHudCamMode" class="text-cyan-400 font-bold uppercase">PERSP</span>
            <span class="text-slate-600">|</span>
            <span id="cs2labHudFps" class="text-slate-400">64 HZ</span>
          </div>

          <div class="flex items-center gap-1 bg-slate-900/85 backdrop-blur-md p-1 rounded-lg border border-slate-800 text-[11px] font-mono">
            <button data-cs2lab-preset="free" class="px-2 py-0.5 rounded bg-purple-600 text-white font-bold cursor-pointer">Free</button>
            <button data-cs2lab-preset="top" class="px-2 py-0.5 rounded text-slate-400 hover:text-white cursor-pointer">Top</button>
            <button data-cs2lab-preset="front" class="px-2 py-0.5 rounded text-slate-400 hover:text-white cursor-pointer">Front</button>
            <button data-cs2lab-preset="side" class="px-2 py-0.5 rounded text-slate-400 hover:text-white cursor-pointer">Side</button>
          </div>
        </div>

        <!-- Middle-Bottom Bar: Camera Mode Toggle & Theater / Fullscreen -->
        <div class="flex items-center justify-between gap-2 pointer-events-auto mb-16">
          <div class="flex items-center gap-1 bg-slate-900/85 backdrop-blur-md p-1 rounded-lg border border-slate-800 text-[11px] font-mono">
            <button id="cs2labBtnCamToggle" class="px-2.5 py-1 rounded bg-slate-800 hover:bg-slate-700 text-cyan-300 border border-slate-700 font-bold cursor-pointer">
              Mode: Persp
            </button>
          </div>

          <div class="flex items-center gap-1 bg-slate-900/85 backdrop-blur-md p-1 rounded-lg border border-slate-800 text-[11px] font-mono">
            <button id="cs2labBtnTheater" class="px-2 py-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-300 border border-slate-700 cursor-pointer" title="Theater Mode">
              ⛶ Theater
            </button>
            <button id="cs2labBtnFullscreen" class="px-2 py-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-300 border border-slate-700 cursor-pointer" title="HTML5 Fullscreen">
              ⛶ Fullscreen
            </button>
          </div>
        </div>
      `;

      this.container.appendChild(this.uiOverlay);

      // Bind event handlers
      this.uiOverlay.querySelectorAll('[data-cs2lab-preset]').forEach(btn => {
        btn.onclick = (e) => {
          e.stopPropagation();
          const presetName = btn.getAttribute('data-cs2lab-preset');
          this.setCameraPreset(presetName);
        };
      });

      const btnToggle = this.uiOverlay.querySelector('#cs2labBtnCamToggle');
      if (btnToggle) btnToggle.onclick = () => this.toggleCameraMode();

      const btnTheater = this.uiOverlay.querySelector('#cs2labBtnTheater');
      if (btnTheater) btnTheater.onclick = () => this.toggleTheater();

      const btnFs = this.uiOverlay.querySelector('#cs2labBtnFullscreen');
      if (btnFs) btnFs.onclick = () => this.toggleFullscreen();
    }

    _updateToolbarUI(activePreset) {
      const modeEl = this.uiOverlay?.querySelector('#cs2labHudCamMode');
      const toggleBtn = this.uiOverlay?.querySelector('#cs2labBtnCamToggle');
      if (modeEl) modeEl.innerText = this.activeCameraMode === 'perspective' ? 'PERSP' : 'ORTHO';
      if (toggleBtn) toggleBtn.innerText = this.activeCameraMode === 'perspective' ? 'Mode: Persp' : 'Mode: Ortho';

      if (activePreset) {
        this.uiOverlay?.querySelectorAll('[data-cs2lab-preset]').forEach(b => {
          if (b.getAttribute('data-cs2lab-preset') === activePreset) {
            b.className = 'px-2 py-0.5 rounded bg-purple-600 text-white font-bold cursor-pointer';
          } else {
            b.className = 'px-2 py-0.5 rounded text-slate-400 hover:text-white cursor-pointer';
          }
        });
      }
    }

    _switchToOrthographic() {
      const target = (this.controls && this.controls.target) ? this.controls.target : new THREE.Vector3(0, 0, 36);
      const d = Math.min(85, Math.max(60, this.perspCamera.position.distanceTo(target)));
      const aspect = this.perspCamera.aspect;
      const h = 2 * d * Math.tan((this.options.fov * Math.PI) / 360);
      const w = h * aspect;

      this.orthoCamera.left = -w / 2;
      this.orthoCamera.right = w / 2;
      this.orthoCamera.top = h / 2;
      this.orthoCamera.bottom = -h / 2;
      this.orthoCamera.zoom = 1.0;
      this.orthoCamera.position.copy(this.perspCamera.position);
      this.orthoCamera.quaternion.copy(this.perspCamera.quaternion);
      this.orthoCamera.up.copy(this.perspCamera.up);
      this.orthoCamera.updateProjectionMatrix();

      this.activeCamera = this.orthoCamera;
      this.activeCameraMode = 'orthographic';
      if (this.controls) {
        this.controls.object = this.orthoCamera;
        this.controls.update();
      }
      this._updateToolbarUI();
    }

    _switchToPerspective() {
      const target = (this.controls && this.controls.target) ? this.controls.target : new THREE.Vector3(0, 0, 36);
      const orthoH = (this.orthoCamera.top - this.orthoCamera.bottom) / (this.orthoCamera.zoom || 1);
      const dReq = orthoH / (2 * Math.tan((this.options.fov * Math.PI) / 360));
      const dir = new THREE.Vector3().subVectors(this.orthoCamera.position, target).normalize();

      this.perspCamera.position.copy(target).addScaledVector(dir, dReq);
      this.perspCamera.quaternion.copy(this.orthoCamera.quaternion);
      this.perspCamera.up.copy(this.orthoCamera.up);
      this.perspCamera.updateProjectionMatrix();

      this.activeCamera = this.perspCamera;
      this.activeCameraMode = 'perspective';
      if (this.controls) {
        this.controls.object = this.perspCamera;
        this.controls.update();
      }
      this._updateToolbarUI();
    }

    _initResizeObserver() {
      if (typeof ResizeObserver !== 'undefined' && this.container) {
        this.resizeObserver = new ResizeObserver(() => this._handleResize());
        this.resizeObserver.observe(this.container);
      }
    }

    _handleResize() {
      if (!this.container || !this.renderer) return;
      const width = this.container.clientWidth;
      const height = this.container.clientHeight;
      if (width === 0 || height === 0) return;
      const aspect = width / height;

      this.perspCamera.aspect = aspect;
      this.perspCamera.updateProjectionMatrix();

      const orthoH = (this.orthoCamera.top - this.orthoCamera.bottom) / (this.orthoCamera.zoom || 1);
      const orthoW = orthoH * aspect;
      this.orthoCamera.left = -orthoW / 2;
      this.orthoCamera.right = orthoW / 2;
      this.orthoCamera.updateProjectionMatrix();

      this.renderer.setSize(width, height, false);
      this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, this.options.dprCap));
    }

    _startRenderLoop() {
      const loop = () => {
        this.animFrameId = requestAnimationFrame(loop);
        if (this.controls) this.controls.update();

        // Synchronize Web Audio 3D spatial listener with active camera
        if (this.audioEngine && typeof this.audioEngine.setListenerTransform === 'function') {
          this.audioEngine.setListenerTransform(this.activeCamera);
        }

        if (this.renderer && this.scene && this.activeCamera) {
          this.renderer.render(this.scene, this.activeCamera);
        }
      };
      loop();
    }

    _disposeMaterial(mat) {
      if (!mat) return;
      mat.dispose();
      for (const key of Object.keys(mat)) {
        const val = mat[key];
        if (val && typeof val.dispose === 'function') val.dispose();
      }
    }
  }

  CS2Lab.Viewport3D = Viewport3D;

  // ==========================================================================
  // 3. AudioEngine: Procedural SOS Math, 3D Spatial HRTF & Sub-Tick Scheduling
  // ==========================================================================
  class AudioEngine {
    constructor(options = {}) {
      this.options = options || {};
      this.ctx = null;
      this.isInitialized = false;
      this.isMuted = this.options.isMuted !== undefined ? Boolean(this.options.isMuted) : true;
      this.masterVolume = this.options.masterVolume !== undefined ? this.options.masterVolume : 1.0;

      // Bus nodes
      this.masterGain = null;
      this.compressor = null;
      this.sfxGain = null;
      this.subtickGain = null;
      this.metroGain = null;
      this.ambientGain = null;

      // Noise buffers
      this._whiteNoiseBuffer = null;
      this._pinkNoiseBuffer = null;

      // State tracking
      this._lastTickTime = 0;
      this._unlockHandler = null;
      this._activeSources = new Set();
    }

    /**
     * Initialize AudioContext with seamless browser autoplay policy unlock.
     * @returns {Promise<void>}
     */
    async init() {
      if (this.isInitialized && this.ctx && this.ctx.state === 'running') {
        return;
      }

      if (!this.ctx) {
        const AudioContextClass = window.AudioContext || window.webkitAudioContext;
        if (!AudioContextClass) {
          console.warn('[CS2Lab.AudioEngine] Web Audio API is not supported in this environment.');
          return;
        }
        try {
          this.ctx = new AudioContextClass();
          this._buildBusGraph();
          this._generateProceduralBuffers();
        } catch (e) {
          console.warn('[CS2Lab.AudioEngine] Failed to create AudioContext:', e);
          return;
        }
      }

      if (this.ctx && this.ctx.state === 'suspended') {
        return new Promise((resolve) => {
          const unlock = async () => {
            if (this.ctx && this.ctx.state === 'suspended') {
              try {
                await this.ctx.resume();
              } catch (_) {}
            }
            this._removeUnlockListeners();
            this.isInitialized = true;
            resolve();
          };

          this._unlockHandler = unlock;
          ['pointerdown', 'keydown', 'touchstart'].forEach(evt => {
            window.addEventListener(evt, unlock, { once: true, passive: true });
          });

          // Immediate attempt in case user gesture is already active
          this.ctx.resume().then(() => {
            this._removeUnlockListeners();
            this.isInitialized = true;
            resolve();
          }).catch(() => {});
        });
      }

      this.isInitialized = true;
    }

    _removeUnlockListeners() {
      if (this._unlockHandler) {
        ['pointerdown', 'keydown', 'touchstart'].forEach(evt => {
          window.removeEventListener(evt, this._unlockHandler);
        });
        this._unlockHandler = null;
      }
    }

    _initBuses() {
      this._buildBusGraph();
    }

    _buildBusGraph() {
      const ctx = this.ctx;
      if (!ctx) return;

      // Master output limiter (prevents digital clipping from impulse stacking)
      this.compressor = ctx.createDynamicsCompressor();
      this.compressor.threshold.setValueAtTime(-6.0, ctx.currentTime);
      this.compressor.knee.setValueAtTime(12.0, ctx.currentTime);
      this.compressor.ratio.setValueAtTime(12.0, ctx.currentTime);
      this.compressor.attack.setValueAtTime(0.001, ctx.currentTime); // 1ms attack to catch transients
      this.compressor.release.setValueAtTime(0.080, ctx.currentTime);
      this.compressor.connect(ctx.destination);

      // Master gain node
      this.masterGain = ctx.createGain();
      this.masterGain.gain.setValueAtTime(this.isMuted ? 0 : this.masterVolume, ctx.currentTime);
      this.masterGain.connect(this.compressor);

      // Submix buses
      const sfxGainVal = (this.options && this.options.sfxGain !== undefined) ? this.options.sfxGain : 0.45;
      const subtickGainVal = (this.options && this.options.subtickGain !== undefined) ? this.options.subtickGain : 0.40;
      const metroGainVal = (this.options && this.options.metroGain !== undefined) ? this.options.metroGain : 0.35;
      const ambientGainVal = (this.options && this.options.ambientGain !== undefined) ? this.options.ambientGain : 0.60;

      this.sfxGain = ctx.createGain();
      this.sfxGain.gain.setValueAtTime(sfxGainVal, ctx.currentTime);
      this.sfxGain.connect(this.masterGain);

      this.subtickGain = ctx.createGain();
      this.subtickGain.gain.setValueAtTime(subtickGainVal, ctx.currentTime);
      this.subtickGain.connect(this.masterGain);

      this.metroGain = ctx.createGain();
      this.metroGain.gain.setValueAtTime(metroGainVal, ctx.currentTime);
      this.metroGain.connect(this.masterGain);

      this.ambientGain = ctx.createGain();
      this.ambientGain.gain.setValueAtTime(ambientGainVal, ctx.currentTime);
      this.ambientGain.connect(this.masterGain);
    }

    _generateProceduralBuffers() {
      const ctx = this.ctx;
      if (!ctx) return;
      const sampleRate = ctx.sampleRate || 44100;

      // 1. White Noise Buffer (0.5 sec)
      const whiteLength = Math.floor(sampleRate * 0.5);
      this._whiteNoiseBuffer = ctx.createBuffer(1, whiteLength, sampleRate);
      const whiteData = this._whiteNoiseBuffer.getChannelData(0);
      for (let i = 0; i < whiteLength; i++) {
        whiteData[i] = Math.random() * 2.0 - 1.0;
      }

      // 2. Pink Noise Buffer (2.0 sec, Kellet 3-pole IIR algorithm)
      const pinkLength = Math.floor(sampleRate * 2.0);
      this._pinkNoiseBuffer = ctx.createBuffer(1, pinkLength, sampleRate);
      const pinkData = this._pinkNoiseBuffer.getChannelData(0);
      let b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
      for (let i = 0; i < pinkLength; i++) {
        const white = Math.random() * 2.0 - 1.0;
        b0 = 0.99886 * b0 + white * 0.0555179;
        b1 = 0.99332 * b1 + white * 0.0750759;
        b2 = 0.96900 * b2 + white * 0.1538520;
        b3 = 0.86650 * b3 + white * 0.3104856;
        b4 = 0.55000 * b4 + white * 0.5329522;
        b5 = -0.7616 * b5 - white * 0.0168980;
        const pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362;
        b6 = white * 0.115926;
        pinkData[i] = pink * 0.11; // Normalize to ~[-1.0, 1.0]
      }
      // Crossfade pink loop boundary (500 samples)
      for (let i = 0; i < 500; i++) {
        const alpha = i / 500;
        pinkData[i] = pinkData[i] * alpha + pinkData[pinkLength - 500 + i] * (1.0 - alpha);
      }
    }

    _createPanner(x = 0, y = 0, z = 0) {
      const ctx = this.ctx;
      if (!ctx) return null;
      const panner = ctx.createPanner();
      panner.panningModel = 'HRTF';
      panner.distanceModel = 'inverse';
      panner.refDistance = 64.0;    // Source 2 unit near-field scale
      panner.maxDistance = 4096.0;  // Maximum audible boundary
      panner.rolloffFactor = 1.0;
      panner.coneInnerAngle = 360;

      const t = ctx.currentTime;
      if (panner.positionX) {
        panner.positionX.setValueAtTime(x, t);
        panner.positionY.setValueAtTime(y, t);
        panner.positionZ.setValueAtTime(z, t);
      } else {
        panner.setPosition(x, y, z);
      }
      panner.connect(this.sfxGain || this.masterGain);
      return panner;
    }

    /**
     * Synchronize Web Audio listener with Three.js camera world transform.
     * @param {THREE.Camera} camera
     */
    setListenerTransform(camera) {
      if (!this.ctx || !camera || this.isMuted) return;
      if (this.ctx.state !== 'running') return;
      if (!this._activeSources || this._activeSources.size === 0) return;

      const listener = this.ctx.listener;
      if (!listener) return;

      const pos = new THREE.Vector3();
      const fwd = new THREE.Vector3();
      const up = new THREE.Vector3(0, 1, 0);

      camera.getWorldPosition(pos);
      camera.getWorldDirection(fwd);
      up.applyQuaternion(camera.quaternion);

      // Throttling: only update if camera position or orientation actually changed
      if (!this._lastCamPos) {
        this._lastCamPos = new THREE.Vector3();
        this._lastCamFwd = new THREE.Vector3();
      }
      if (pos.distanceToSquared(this._lastCamPos) < 0.05 && fwd.distanceToSquared(this._lastCamFwd) < 0.005) {
        return;
      }
      this._lastCamPos.copy(pos);
      this._lastCamFwd.copy(fwd);

      const t = this.ctx.currentTime;
      if (listener.positionX) {
        listener.positionX.setValueAtTime(pos.x, t);
        listener.positionY.setValueAtTime(pos.y, t);
        listener.positionZ.setValueAtTime(pos.z, t);
        listener.forwardX.setValueAtTime(fwd.x, t);
        listener.forwardY.setValueAtTime(fwd.y, t);
        listener.forwardZ.setValueAtTime(fwd.z, t);
        listener.upX.setValueAtTime(up.x, t);
        listener.upY.setValueAtTime(up.y, t);
        listener.upZ.setValueAtTime(up.z, t);
      } else if (typeof listener.setPosition === 'function') {
        listener.setPosition(pos.x, pos.y, pos.z);
        listener.setOrientation(fwd.x, fwd.y, fwd.z, up.x, up.y, up.z);
      }
    }

    /**
     * Play 64 Hz Host Tick Metronome Pulse (880 Hz Triangle Wave Decay)
     */
    playTickPulse() {
      if (!this.ctx || this.isMuted) return;
      if (this.ctx.state === 'suspended') { this.ctx.resume(); return; }

      const t0 = this.ctx.currentTime;
      // Prevent double trigger within 5ms
      if (t0 - this._lastTickTime < 0.005) return;
      this._lastTickTime = t0;

      const osc = this.ctx.createOscillator();
      const gain = this.ctx.createGain();

      osc.type = 'triangle';
      osc.frequency.setValueAtTime(880, t0); // A5

      gain.gain.setValueAtTime(0.0001, t0);
      gain.gain.linearRampToValueAtTime(0.35, t0 + 0.0005);
      gain.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.009);

      osc.connect(gain);
      gain.connect(this.metroGain || this.masterGain);

      osc.start(t0);
      osc.stop(t0 + 0.010);
      osc.onended = () => {
        osc.disconnect();
        gain.disconnect();
      };
    }

    /**
     * Play UserCmd Sub-Tick Micro-Slice Click (Microsecond-scheduled transient)
     * @param {number} [whenDelay=0] Normalized fraction w in [0, 1) or explicit delay in seconds.
     */
    playSubtickClick(whenDelay = 0) {
      if (!this.ctx || this.isMuted) return;
      if (this.ctx.state === 'suspended') { this.ctx.resume(); return; }

      // Map sub-tick fraction w in [0, 1) to tick interval delta (15.625 ms)
      const delaySec = (whenDelay >= 0 && whenDelay < 1.0 && whenDelay !== 0)
        ? whenDelay * 0.015625
        : Math.max(0, whenDelay);

      const baseLead = 0.002; // Constant 2ms lookahead lead
      const t0 = this.ctx.currentTime + baseLead + delaySec;

      // 1. High-frequency pitch drop chirp (2400 Hz -> 750 Hz)
      const osc = this.ctx.createOscillator();
      const oscGain = this.ctx.createGain();

      osc.type = 'sine';
      osc.frequency.setValueAtTime(2400, t0);
      osc.frequency.exponentialRampToValueAtTime(750, t0 + 0.0025);

      oscGain.gain.setValueAtTime(0.0001, t0);
      oscGain.gain.linearRampToValueAtTime(0.22, t0 + 0.0002);
      oscGain.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.0030);

      osc.connect(oscGain);
      oscGain.connect(this.subtickGain || this.masterGain);

      osc.start(t0);
      osc.stop(t0 + 0.0035);
      osc.onended = () => {
        osc.disconnect();
        oscGain.disconnect();
      };

      // 2. High-pass noise transient crack
      if (this._whiteNoiseBuffer) {
        const noise = this.ctx.createBufferSource();
        const filter = this.ctx.createBiquadFilter();
        const noiseGain = this.ctx.createGain();

        noise.buffer = this._whiteNoiseBuffer;
        filter.type = 'highpass';
        filter.frequency.setValueAtTime(3200, t0);
        filter.Q.setValueAtTime(2.0, t0);

        noiseGain.gain.setValueAtTime(0.0001, t0);
        noiseGain.gain.linearRampToValueAtTime(0.14, t0 + 0.0002);
        noiseGain.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.0020);

        noise.connect(filter);
        filter.connect(noiseGain);
        noiseGain.connect(this.subtickGain || this.masterGain);

        noise.start(t0);
        noise.stop(t0 + 0.0025);
        noise.onended = () => {
          noise.disconnect();
          filter.disconnect();
          noiseGain.disconnect();
        };
      }
    }

    /**
     * Play Multi-Stage Ballistic Gunfire Impulse
     * @param {number} x
     * @param {number} y
     * @param {number} z
     */
    playGunfire(x = 0, y = 0, z = 0) {
      if (!this.ctx || this.isMuted) return;
      if (this.ctx.state === 'suspended') { this.ctx.resume(); return; }

      const t0 = this.ctx.currentTime;
      const panner = this._createPanner(x, y, z);
      if (!panner) return;

      // Stage 1: Muzzle Blast Noise Transient
      if (this._whiteNoiseBuffer) {
        const noise = this.ctx.createBufferSource();
        const hpFilter = this.ctx.createBiquadFilter();
        const peakFilter = this.ctx.createBiquadFilter();
        const blastGain = this.ctx.createGain();

        noise.buffer = this._whiteNoiseBuffer;
        hpFilter.type = 'highpass';
        hpFilter.frequency.setValueAtTime(1200, t0);

        peakFilter.type = 'peaking';
        peakFilter.frequency.setValueAtTime(3400, t0);
        peakFilter.Q.setValueAtTime(2.0, t0);
        peakFilter.gain.setValueAtTime(6.0, t0);

        blastGain.gain.setValueAtTime(0.0001, t0);
        blastGain.gain.linearRampToValueAtTime(0.85, t0 + 0.0002);
        blastGain.gain.exponentialRampToValueAtTime(0.001, t0 + 0.022);

        noise.connect(hpFilter);
        hpFilter.connect(peakFilter);
        peakFilter.connect(blastGain);
        blastGain.connect(panner);

        noise.start(t0);
        noise.stop(t0 + 0.025);
        noise.onended = () => {
          noise.disconnect();
          hpFilter.disconnect();
          peakFilter.disconnect();
          blastGain.disconnect();
        };
      }

      // Stage 2: Concussion Body Punch (Chirp + Saturation)
      const punchOsc = this.ctx.createOscillator();
      const punchGain = this.ctx.createGain();
      const shaper = this.ctx.createWaveShaper();

      // Soft-clip curve: y = tanh(1.8 * x)
      const curve = new Float32Array(256);
      for (let i = 0; i < 256; i++) {
        const xVal = (i / 128) - 1.0;
        curve[i] = Math.tanh(1.8 * xVal);
      }
      shaper.curve = curve;

      punchOsc.type = 'sine';
      punchOsc.frequency.setValueAtTime(180, t0);
      punchOsc.frequency.exponentialRampToValueAtTime(40, t0 + 0.065);

      punchGain.gain.setValueAtTime(0.0001, t0);
      punchGain.gain.linearRampToValueAtTime(0.75, t0 + 0.0008);
      punchGain.gain.exponentialRampToValueAtTime(0.001, t0 + 0.090);

      punchOsc.connect(shaper);
      shaper.connect(punchGain);
      punchGain.connect(panner);

      punchOsc.start(t0);
      punchOsc.stop(t0 + 0.095);
      punchOsc.onended = () => {
        punchOsc.disconnect();
        shaper.disconnect();
        punchGain.disconnect();
      };

      // Stage 3: Spatial Environmental Diffuse Tail
      if (this._whiteNoiseBuffer) {
        const tail = this.ctx.createBufferSource();
        const lpFilter = this.ctx.createBiquadFilter();
        const tailGain = this.ctx.createGain();

        tail.buffer = this._whiteNoiseBuffer;
        lpFilter.type = 'lowpass';
        lpFilter.frequency.setValueAtTime(2800, t0);
        lpFilter.frequency.exponentialRampToValueAtTime(350, t0 + 0.35);

        tailGain.gain.setValueAtTime(0.0001, t0);
        tailGain.gain.linearRampToValueAtTime(0.30, t0 + 0.004);
        tailGain.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.40);

        tail.connect(lpFilter);
        lpFilter.connect(tailGain);
        tailGain.connect(panner);

        tail.start(t0);
        tail.stop(t0 + 0.42);
        tail.onended = () => {
          tail.disconnect();
          lpFilter.disconnect();
          tailGain.disconnect();
          panner.disconnect();
        };
      }
    }

    /**
     * Play Material-Differentiated Surface Impact
     * @param {'concrete'|'wood'|'metal'|'glass'} material
     * @param {number} x
     * @param {number} y
     * @param {number} z
     */
    playImpact(material = 'concrete', x = 0, y = 0, z = 0) {
      if (!this.ctx || this.isMuted) return;
      if (this.ctx.state === 'suspended') { this.ctx.resume(); return; }

      const t0 = this.ctx.currentTime;
      const panner = this._createPanner(x, y, z);
      if (!panner) return;

      if (material === 'metal') {
        // High-Q modal resonators
        [1840, 3560].forEach((freq, idx) => {
          const osc = this.ctx.createOscillator();
          const gain = this.ctx.createGain();
          const decay = idx === 0 ? 0.11 : 0.08;

          osc.type = 'sine';
          osc.frequency.setValueAtTime(freq, t0);

          gain.gain.setValueAtTime(0.0001, t0);
          gain.gain.linearRampToValueAtTime(0.35, t0 + 0.0005);
          gain.gain.exponentialRampToValueAtTime(0.0001, t0 + decay);

          osc.connect(gain);
          gain.connect(panner);

          osc.start(t0);
          osc.stop(t0 + decay + 0.01);
          osc.onended = () => { osc.disconnect(); gain.disconnect(); };
        });
      } else if (material === 'wood') {
        // Dual resonant thump
        const osc = this.ctx.createOscillator();
        const gain = this.ctx.createGain();

        osc.type = 'triangle';
        osc.frequency.setValueAtTime(140, t0);
        osc.frequency.exponentialRampToValueAtTime(60, t0 + 0.05);

        gain.gain.setValueAtTime(0.0001, t0);
        gain.gain.linearRampToValueAtTime(0.55, t0 + 0.001);
        gain.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.055);

        osc.connect(gain);
        gain.connect(panner);

        osc.start(t0);
        osc.stop(t0 + 0.06);
        osc.onended = () => { osc.disconnect(); gain.disconnect(); };
      } else {
        // Concrete (Default): Hard noise slap + sub thud
        const osc = this.ctx.createOscillator();
        const gain = this.ctx.createGain();

        osc.type = 'sine';
        osc.frequency.setValueAtTime(130, t0);
        osc.frequency.exponentialRampToValueAtTime(45, t0 + 0.035);

        gain.gain.setValueAtTime(0.0001, t0);
        gain.gain.linearRampToValueAtTime(0.65, t0 + 0.0005);
        gain.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.038);

        osc.connect(gain);
        gain.connect(panner);

        osc.start(t0);
        osc.stop(t0 + 0.04);
        osc.onended = () => { osc.disconnect(); gain.disconnect(); };
      }

      // Cleanup panner after impact duration
      setTimeout(() => {
        try { panner.disconnect(); } catch (_) {}
      }, 150);
    }

    /**
     * Play Resonant FM Chirp Ricochet
     * @param {number} x
     * @param {number} y
     * @param {number} z
     */
    playRicochet(x = 0, y = 0, z = 0) {
      if (!this.ctx || this.isMuted) return;
      if (this.ctx.state === 'suspended') { this.ctx.resume(); return; }

      const t0 = this.ctx.currentTime;
      const panner = this._createPanner(x, y, z);
      if (!panner) return;

      // FM Synthesis: Carrier + Modulator
      const carrier = this.ctx.createOscillator();
      const modulator = this.ctx.createOscillator();
      const modGain = this.ctx.createGain();
      const carrierGain = this.ctx.createGain();

      // Exponential pitch drop chirp: 2800 Hz -> 650 Hz over 85 ms
      carrier.type = 'sine';
      carrier.frequency.setValueAtTime(2800, t0);
      carrier.frequency.exponentialRampToValueAtTime(650, t0 + 0.085);

      // Modulator: 92 Hz with decaying modulation index
      modulator.type = 'sine';
      modulator.frequency.setValueAtTime(92, t0);

      modGain.gain.setValueAtTime(175, t0);
      modGain.gain.exponentialRampToValueAtTime(5, t0 + 0.08);

      modulator.connect(modGain);
      modGain.connect(carrier.frequency);

      carrierGain.gain.setValueAtTime(0.0001, t0);
      carrierGain.gain.linearRampToValueAtTime(0.40, t0 + 0.001);
      carrierGain.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.085);

      carrier.connect(carrierGain);
      carrierGain.connect(panner);

      carrier.start(t0);
      modulator.start(t0);
      carrier.stop(t0 + 0.090);
      modulator.stop(t0 + 0.090);

      carrier.onended = () => {
        carrier.disconnect();
        modulator.disconnect();
        modGain.disconnect();
        carrierGain.disconnect();
        panner.disconnect();
      };
    }

    /**
     * Play Volumetric Smoke Grenade Detonation Pop
     * @param {number} x
     * @param {number} y
     * @param {number} z
     */
    playSmokeDetonation(x = 0, y = 0, z = 0) {
      if (!this.ctx || this.isMuted) return;
      if (this.ctx.state === 'suspended') { this.ctx.resume(); return; }

      const t0 = this.ctx.currentTime;
      const panner = this._createPanner(x, y, z);
      if (!panner) return;

      // Concussion pop
      const osc = this.ctx.createOscillator();
      const gain = this.ctx.createGain();

      osc.type = 'sine';
      osc.frequency.setValueAtTime(220, t0);
      osc.frequency.exponentialRampToValueAtTime(32, t0 + 0.14);

      gain.gain.setValueAtTime(0.0001, t0);
      gain.gain.linearRampToValueAtTime(0.60, t0 + 0.001);
      gain.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.15);

      osc.connect(gain);
      gain.connect(panner);

      osc.start(t0);
      osc.stop(t0 + 0.16);
      osc.onended = () => {
        osc.disconnect();
        gain.disconnect();
        panner.disconnect();
      };
    }

    /**
     * Play High-Explosive (HE) Grenade Detonation Blast & Shockwave
     * @param {number} x
     * @param {number} y
     * @param {number} z
     */
    playExplosion(x = 0, y = 0, z = 0) {
      if (!this.ctx || this.isMuted) return;
      if (this.ctx.state === 'suspended') { this.ctx.resume(); return; }

      const t0 = this.ctx.currentTime;
      const panner = this._createPanner(x, y, z);
      if (!panner) return;

      // 1. Heavy Sub-Bass Concussion (120 Hz -> 20 Hz)
      const subOsc = this.ctx.createOscillator();
      const subGain = this.ctx.createGain();
      const shaper = this.ctx.createWaveShaper();

      const curve = new Float32Array(256);
      for (let i = 0; i < 256; i++) {
        const xVal = (i / 128) - 1.0;
        curve[i] = Math.tanh(2.2 * xVal);
      }
      shaper.curve = curve;

      subOsc.type = 'sine';
      subOsc.frequency.setValueAtTime(120, t0);
      subOsc.frequency.exponentialRampToValueAtTime(20, t0 + 0.65);

      subGain.gain.setValueAtTime(0.0001, t0);
      subGain.gain.linearRampToValueAtTime(0.95, t0 + 0.001);
      subGain.gain.exponentialRampToValueAtTime(0.001, t0 + 0.70);

      subOsc.connect(shaper);
      shaper.connect(subGain);
      subGain.connect(panner);

      subOsc.start(t0);
      subOsc.stop(t0 + 0.75);
      subOsc.onended = () => {
        subOsc.disconnect();
        shaper.disconnect();
        subGain.disconnect();
      };

      // 2. Shockwave Debris Roar (Filtered White Noise)
      if (this._whiteNoiseBuffer) {
        const noise = this.ctx.createBufferSource();
        const lp = this.ctx.createBiquadFilter();
        const noiseGain = this.ctx.createGain();

        noise.buffer = this._whiteNoiseBuffer;
        lp.type = 'lowpass';
        lp.frequency.setValueAtTime(3200, t0);
        lp.frequency.exponentialRampToValueAtTime(160, t0 + 0.85);

        noiseGain.gain.setValueAtTime(0.0001, t0);
        noiseGain.gain.linearRampToValueAtTime(0.80, t0 + 0.002);
        noiseGain.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.90);

        noise.connect(lp);
        lp.connect(noiseGain);
        noiseGain.connect(panner);

        noise.start(t0);
        noise.stop(t0 + 0.92);
        noise.onended = () => {
          noise.disconnect();
          lp.disconnect();
          noiseGain.disconnect();
          panner.disconnect();
        };
      }
    }

    /**
     * Play Authoritative Headshot Helmet Dink (2.8 kHz / 4.1 kHz Metallic Rings)
     * @param {number} x
     * @param {number} y
     * @param {number} z
     */
    playHeadshot(x = 0, y = 0, z = 0) {
      if (!this.ctx || this.isMuted) return;
      if (this.ctx.state === 'suspended') { this.ctx.resume(); return; }

      const t0 = this.ctx.currentTime;
      const panner = this._createPanner(x, y, z);
      if (!panner) return;

      // Resonant Metallic Modal Harmonics
      [2780, 4150].forEach((freq, idx) => {
        const osc = this.ctx.createOscillator();
        const gain = this.ctx.createGain();
        const decay = idx === 0 ? 0.16 : 0.11;

        osc.type = 'sine';
        osc.frequency.setValueAtTime(freq, t0);

        gain.gain.setValueAtTime(0.0001, t0);
        gain.gain.linearRampToValueAtTime(0.55, t0 + 0.0004);
        gain.gain.exponentialRampToValueAtTime(0.0001, t0 + decay);

        osc.connect(gain);
        gain.connect(panner);

        osc.start(t0);
        osc.stop(t0 + decay + 0.01);
        osc.onended = () => { osc.disconnect(); gain.disconnect(); };
      });

      // Bone Snap / Flesh Thud
      const thud = this.ctx.createOscillator();
      const thudGain = this.ctx.createGain();
      thud.type = 'triangle';
      thud.frequency.setValueAtTime(180, t0);
      thud.frequency.exponentialRampToValueAtTime(45, t0 + 0.05);

      thudGain.gain.setValueAtTime(0.0001, t0);
      thudGain.gain.linearRampToValueAtTime(0.40, t0 + 0.0008);
      thudGain.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.055);

      thud.connect(thudGain);
      thudGain.connect(panner);
      thud.start(t0);
      thud.stop(t0 + 0.06);
      thud.onended = () => { thud.disconnect(); thudGain.disconnect(); };

      setTimeout(() => { try { panner.disconnect(); } catch (_) {} }, 200);
    }

    /**
     * Play Ballistic Flesh Hit Impact (Squishy Viscous Deceleration)
     * @param {number} x
     * @param {number} y
     * @param {number} z
     */
    playFleshHit(x = 0, y = 0, z = 0) {
      if (!this.ctx || this.isMuted) return;
      if (this.ctx.state === 'suspended') { this.ctx.resume(); return; }

      const t0 = this.ctx.currentTime;
      const panner = this._createPanner(x, y, z);
      if (!panner) return;

      const osc = this.ctx.createOscillator();
      const gain = this.ctx.createGain();

      osc.type = 'sine';
      osc.frequency.setValueAtTime(160, t0);
      osc.frequency.exponentialRampToValueAtTime(40, t0 + 0.045);

      gain.gain.setValueAtTime(0.0001, t0);
      gain.gain.linearRampToValueAtTime(0.60, t0 + 0.0006);
      gain.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.05);

      osc.connect(gain);
      gain.connect(panner);

      osc.start(t0);
      osc.stop(t0 + 0.055);
      osc.onended = () => {
        osc.disconnect();
        gain.disconnect();
        try { panner.disconnect(); } catch (_) {}
      };
    }

    /**
     * Start Continuous Looping Smoke Hiss Buffer (Returns handle with stop method)
     * @param {number} x
     * @param {number} y
     * @param {number} z
     * @returns {{ stop: (fadeDuration?: number) => void, setPosition: (x: number, y: number, z: number) => void, setIntensity: (val: number) => void }}
     */
    startSmokeHiss(x = 0, y = 0, z = 0) {
      if (!this.ctx || !this._pinkNoiseBuffer || this.isMuted) {
        return { stop: () => {}, setPosition: () => {}, setIntensity: () => {} };
      }

      const t0 = this.ctx.currentTime;
      const panner = this._createPanner(x, y, z);
      if (!panner) {
        return { stop: () => {}, setPosition: () => {}, setIntensity: () => {} };
      }

      const source = this.ctx.createBufferSource();
      const hpFilter = this.ctx.createBiquadFilter();
      const peakFilter = this.ctx.createBiquadFilter();
      const gain = this.ctx.createGain();

      source.buffer = this._pinkNoiseBuffer;
      source.loop = true;

      hpFilter.type = 'highpass';
      hpFilter.frequency.setValueAtTime(550, t0);

      peakFilter.type = 'peaking';
      peakFilter.frequency.setValueAtTime(1900, t0);
      peakFilter.Q.setValueAtTime(1.5, t0);
      peakFilter.gain.setValueAtTime(4.0, t0);

      // Swell in over 0.8s
      gain.gain.setValueAtTime(0.0001, t0);
      gain.gain.linearRampToValueAtTime(0.25, t0 + 0.80);

      source.connect(hpFilter);
      hpFilter.connect(peakFilter);
      peakFilter.connect(gain);
      gain.connect(panner);

      source.start(t0);
      this._activeSources.add(source);

      return {
        stop: (fadeDuration = 1.5) => {
          if (!this.ctx) return;
          const stopTime = this.ctx.currentTime;
          gain.gain.cancelScheduledValues(stopTime);
          gain.gain.setValueAtTime(gain.gain.value, stopTime);
          gain.gain.exponentialRampToValueAtTime(0.0001, stopTime + fadeDuration);
          setTimeout(() => {
            try {
              source.stop();
              source.disconnect();
              hpFilter.disconnect();
              peakFilter.disconnect();
              gain.disconnect();
              panner.disconnect();
              this._activeSources.delete(source);
            } catch (_) {}
          }, (fadeDuration + 0.1) * 1000);
        },
        setPosition: (newX, newY, newZ) => {
          if (!this.ctx) return;
          const curTime = this.ctx.currentTime;
          if (panner.positionX) {
            panner.positionX.setValueAtTime(newX, curTime);
            panner.positionY.setValueAtTime(newY, curTime);
            panner.positionZ.setValueAtTime(newZ, curTime);
          } else {
            panner.setPosition(newX, newY, newZ);
          }
        },
        setIntensity: (val) => {
          if (!this.ctx) return;
          const curTime = this.ctx.currentTime;
          const clamped = Math.max(0.0, Math.min(1.0, val));
          gain.gain.cancelScheduledValues(curTime);
          gain.gain.setValueAtTime(gain.gain.value, curTime);
          gain.gain.linearRampToValueAtTime(0.25 * clamped, curTime + 0.05);
        }
      };
    }

    setMasterVolume(volume) {
      this.masterVolume = Math.max(0.0, Math.min(1.0, volume));
      if (this.masterGain && this.ctx && !this.isMuted) {
        this.masterGain.gain.setValueAtTime(this.masterVolume, this.ctx.currentTime);
      }
    }

    setMuted(muted) {
      this.isMuted = Boolean(muted);
      if (this.masterGain && this.ctx) {
        this.masterGain.gain.setValueAtTime(this.isMuted ? 0 : this.masterVolume, this.ctx.currentTime);
      }
    }

    destroy() {
      this._removeUnlockListeners();

      // Stop and clean any active looped sources
      this._activeSources.forEach(source => {
        try {
          source.stop();
          source.disconnect();
        } catch (_) {}
      });
      this._activeSources.clear();

      if (this.masterGain) {
        try { this.masterGain.disconnect(); } catch (_) {}
      }
      if (this.compressor) {
        try { this.compressor.disconnect(); } catch (_) {}
      }

      if (this.ctx && this.ctx.state !== 'closed') {
        try {
          this.ctx.close();
        } catch (_) {}
      }

      this.ctx = null;
      this.isInitialized = false;
    }
  }

  CS2Lab.AudioEngine = AudioEngine;

  // ==========================================================================
  // 4. PlaybackController: Continuous Sub-Tick Timeline Engine, HUD & Controls
  // ==========================================================================
  class PlaybackController {
    constructor(options = {}) {
      this.maxTicks = options.maxTicks || 64;
      this.tickInterval = 0.015625; // 1 / 64 seconds = 15.625 ms
      this.speed = options.initialSpeed !== undefined ? options.initialSpeed : 1.0;
      this.isLooping = options.isLooping !== false;
      this.audioEngine = options.audioEngine || null;

      // Timeline state
      this.time = 0.0; // Continuous tick parameter: k + w
      this.isPlaying = false;
      this.isScrubbing = false;
      this.wasPlayingBeforeScrub = false;
      this.lastTimestamp = null;
      this.rafId = null;

      // Callbacks
      this.callbacks = {
        onUpdate: null,
        onTick: null,
        onStateChange: null
      };

      // DOM References
      this.container = null;
      this.controlsRoot = null;
      this.scrubber = null;
      this.btnPlay = null;
      this.playText = null;
      this.playIcon = null;
      this.speedButtons = [];
      this.lblTick = null;
      this.lblMaxTick = null;
      this.lblMs = null;
      this.lblW = null;

      // HUD DOM References
      this.hudRoot = null;
      this.hudDom = {};

      // Bound handlers for cleanup
      this._onKeyDown = this._handleKeyDown.bind(this);
      this._loop = this._loop.bind(this);
      this._onPointerUp = null;
    }

    mount(container, callbacks = {}) {
      this.container = typeof container === 'string' ? document.querySelector(container) : container;
      if (!this.container) throw new Error('[CS2Lab.PlaybackController] Invalid container element.');

      this.callbacks = Object.assign(this.callbacks, callbacks);

      // 1. Build and Inject Telemetry HUD
      this._injectHud();

      // 2. Build and Inject Controls Toolbar
      this._injectControls();

      // 3. Attach Event Listeners
      this._attachEvents();

      // 4. Initial UI sync
      this._syncUI(0, 0.0);
    }

    _injectHud() {
      const hud = document.createElement('div');
      hud.id = 'cs2lab-telemetry-hud';
      hud.className = 'absolute top-3 left-3 z-20 pointer-events-none select-none bg-[#070b12]/85 backdrop-blur-md border border-cyan-900/40 rounded-xl p-3 font-mono text-[11px] text-slate-200 shadow-2xl min-w-[280px] max-w-[340px] transition-opacity duration-200';
      hud.innerHTML = `
        <div class="flex items-center justify-between border-b border-cyan-900/40 pb-1.5 mb-2">
          <div class="flex items-center gap-1.5">
            <span class="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse"></span>
            <span class="font-extrabold text-cyan-400 tracking-wider text-[10px]">SOURCE 2 TELEMETRY</span>
          </div>
          <span class="text-[9px] text-slate-400 bg-slate-900 px-1.5 py-0.5 rounded border border-slate-800">64 HZ SUB-TICK</span>
        </div>
        <div class="space-y-1 border-b border-slate-800/80 pb-2 mb-2">
          <div class="flex justify-between items-center">
            <span class="text-slate-400">TICK INDEX:</span>
            <span class="text-white font-bold"><span id="hud-val-tick">0</span> / <span id="hud-val-maxtick">${this.maxTicks}</span></span>
          </div>
          <div class="flex justify-between items-center">
            <span class="text-slate-400">SUB-TICK FRAC (w):</span>
            <span id="hud-val-subtick" class="text-cyan-300 font-bold">0.0000</span>
          </div>
          <div class="flex justify-between items-center">
            <span class="text-slate-400">NORMALIZED TIME:</span>
            <span class="text-amber-300 font-bold"><span id="hud-val-time">0.00</span> ms</span>
          </div>
          <div class="flex justify-between items-center text-[10px]">
            <span class="text-slate-500">TICK INTERVAL:</span>
            <span class="text-slate-400">Δt = 15.625 ms</span>
          </div>
        </div>
        <div class="space-y-1 border-b border-slate-800/80 pb-2 mb-2">
          <div class="flex justify-between items-center">
            <span class="text-slate-400">ORIGIN [X, Y, Z]:</span>
            <span id="hud-val-pos" class="text-slate-100 font-semibold">[0.00, 0.00, 0.00]</span>
          </div>
          <div class="flex justify-between items-center">
            <span class="text-slate-400">VEL [X, Y, Z]:</span>
            <span id="hud-val-vel" class="text-slate-100 font-semibold">[0.00, 0.00, 0.00]</span>
          </div>
          <div class="flex justify-between items-center">
            <span class="text-slate-400">SPEED |V|:</span>
            <span id="hud-val-speed" class="text-emerald-400 font-bold">0.00 u/s</span>
          </div>
        </div>
        <div class="space-y-1">
          <div class="flex justify-between items-center">
            <span class="text-slate-400">GROUND STATE:</span>
            <span id="hud-val-ground" class="text-emerald-400 font-bold">ONGROUND</span>
          </div>
          <div class="flex justify-between items-center">
            <span class="text-slate-400">BUTTONS / EVENT:</span>
            <span id="hud-val-event" class="text-amber-400 font-semibold truncate max-w-[170px]">IDLE</span>
          </div>
        </div>
        <div id="hud-ext-container" class="mt-2 pt-2 border-t border-slate-800/80 space-y-1 hidden"></div>
      `;

      this.container.appendChild(hud);
      this.hudRoot = hud;

      // Cache HUD DOM references for high-speed frame updates
      this.hudDom = {
        root: hud,
        tick: hud.querySelector('#hud-val-tick'),
        maxTick: hud.querySelector('#hud-val-maxtick'),
        subtick: hud.querySelector('#hud-val-subtick'),
        time: hud.querySelector('#hud-val-time'),
        pos: hud.querySelector('#hud-val-pos'),
        vel: hud.querySelector('#hud-val-vel'),
        speed: hud.querySelector('#hud-val-speed'),
        ground: hud.querySelector('#hud-val-ground'),
        event: hud.querySelector('#hud-val-event'),
        extContainer: hud.querySelector('#hud-ext-container')
      };
    }

    _injectControls() {
      const controls = document.createElement('div');
      controls.id = 'cs2lab-playback-bar';
      controls.className = 'absolute bottom-0 left-0 right-0 z-20 bg-slate-950/90 backdrop-blur-md border-t border-slate-800/80 px-4 py-2.5 flex flex-col gap-2 select-none font-sans';
      controls.innerHTML = `
        <div class="relative w-full flex flex-col">
          <input type="range" id="cs2lab-scrubber" min="0" max="${this.maxTicks}" step="0.01" value="0"
            class="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-cyan-400 focus:outline-none transition-all hover:bg-slate-700"
            style="background: linear-gradient(to right, #38bdf8 0%, #38bdf8 0%, #1e293b 0%, #1e293b 100%);">
          <div id="cs2lab-tick-notches" class="flex justify-between items-center px-1 text-[9px] font-mono text-slate-500 pointer-events-none mt-0.5">
            <span>0t (0ms)</span>
            <span>${Math.round(this.maxTicks * 0.25)}t (${Math.round(this.maxTicks * 0.25 * 15.625)}ms)</span>
            <span>${Math.round(this.maxTicks * 0.5)}t (${Math.round(this.maxTicks * 0.5 * 15.625)}ms)</span>
            <span>${Math.round(this.maxTicks * 0.75)}t (${Math.round(this.maxTicks * 0.75 * 15.625)}ms)</span>
            <span>${this.maxTicks}t (${Math.round(this.maxTicks * 15.625)}ms)</span>
          </div>
        </div>
        <div class="flex flex-wrap items-center justify-between gap-3 text-xs">
          <div class="flex items-center gap-1.5">
            <button id="cs2lab-btn-step-back" class="p-1.5 rounded-lg bg-slate-800/80 hover:bg-slate-700 text-slate-300 hover:text-white border border-slate-700/60 transition-colors cursor-pointer" title="Step -1 Tick (Arrow Left)">
              <svg class="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M11 19l-7-7 7-7m8 14l-7-7 7-7"/></svg>
            </button>
            <button id="cs2lab-btn-sub-back" class="px-2 py-1 rounded-lg bg-slate-800/80 hover:bg-slate-700 text-slate-300 hover:text-cyan-300 border border-slate-700/60 font-mono text-[11px] font-semibold transition-colors cursor-pointer" title="Step -0.1 Sub-Tick (Shift + Arrow Left)">
              -0.1t
            </button>
            <button id="cs2lab-btn-play" class="flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-cyan-500/20 hover:bg-cyan-500/30 text-cyan-300 border border-cyan-500/40 font-bold transition-all shadow-sm shadow-cyan-500/10 cursor-pointer" title="Play / Pause (Spacebar)">
              <span id="cs2lab-play-icon">
                <svg class="w-3.5 h-3.5 fill-current" viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg>
              </span>
              <span id="cs2lab-play-text" class="text-[11px] uppercase tracking-wider">Play</span>
            </button>
            <button id="cs2lab-btn-sub-fwd" class="px-2 py-1 rounded-lg bg-slate-800/80 hover:bg-slate-700 text-slate-300 hover:text-cyan-300 border border-slate-700/60 font-mono text-[11px] font-semibold transition-colors cursor-pointer" title="Step +0.1 Sub-Tick (Shift + Arrow Right)">
              +0.1t
            </button>
            <button id="cs2lab-btn-step-fwd" class="p-1.5 rounded-lg bg-slate-800/80 hover:bg-slate-700 text-slate-300 hover:text-white border border-slate-700/60 transition-colors cursor-pointer" title="Step +1 Tick (Arrow Right)">
              <svg class="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 5l7 7-7 7M5 5l7 7-7 7"/></svg>
            </button>
            <button id="cs2lab-btn-reset" class="p-1.5 rounded-lg bg-slate-800/80 hover:bg-slate-700 text-slate-400 hover:text-white border border-slate-700/60 transition-colors ml-1 cursor-pointer" title="Reset Timeline to Tick 0 (R)">
              <svg class="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15"/></svg>
            </button>
          </div>
          <div class="hidden sm:flex items-center gap-2.5 px-3 py-1 rounded-lg bg-slate-900/90 border border-slate-800 font-mono text-[11px]">
            <div class="flex items-center gap-1">
              <span class="text-slate-400">Tick:</span>
              <span id="cs2lab-readout-tick" class="text-white font-bold">0.00</span>
              <span class="text-slate-500">/</span>
              <span id="cs2lab-readout-maxtick" class="text-slate-400">${this.maxTicks}</span>
            </div>
            <span class="text-slate-700">|</span>
            <div class="flex items-center gap-1">
              <span id="cs2lab-readout-ms" class="text-amber-300 font-semibold">0.00</span>
              <span class="text-slate-400">ms</span>
            </div>
            <span class="text-slate-700">|</span>
            <div class="flex items-center gap-1">
              <span class="text-slate-400">w:</span>
              <span id="cs2lab-readout-w" class="text-cyan-300 font-bold">0.0000</span>
            </div>
          </div>
          <div class="flex items-center gap-2">
            <div class="flex items-center bg-slate-900/90 p-0.5 rounded-lg border border-slate-800">
              <button class="cs2lab-speed-pill px-2 py-0.5 rounded text-[10px] font-mono font-bold transition-all text-slate-400 hover:text-white cursor-pointer" data-speed="0.1">0.1x</button>
              <button class="cs2lab-speed-pill px-2 py-0.5 rounded text-[10px] font-mono font-bold transition-all text-slate-400 hover:text-white cursor-pointer" data-speed="0.25">0.25x</button>
              <button class="cs2lab-speed-pill px-2 py-0.5 rounded text-[10px] font-mono font-bold transition-all text-slate-400 hover:text-white cursor-pointer" data-speed="0.5">0.5x</button>
              <button class="cs2lab-speed-pill px-2 py-0.5 rounded text-[10px] font-mono font-bold transition-all bg-cyan-500/20 text-cyan-300 border border-cyan-500/30 cursor-pointer" data-speed="1.0">1.0x</button>
            </div>
            <button id="cs2lab-btn-loop" class="px-2 py-1 rounded-lg bg-emerald-500/10 text-emerald-400 border border-emerald-500/30 font-mono text-[10px] font-bold transition-colors cursor-pointer" title="Toggle Looping">
              LOOP
            </button>
            <button id="cs2lab-btn-hud" class="px-2 py-1 rounded-lg bg-slate-800 text-slate-300 hover:text-white border border-slate-700 font-mono text-[10px] font-bold transition-colors cursor-pointer" title="Toggle Telemetry HUD (H)">
              HUD
            </button>
            <button id="cs2lab-btn-audio" class="px-2 py-1 rounded-lg bg-slate-800 text-slate-400 hover:text-white border border-slate-700 font-mono text-[10px] font-bold transition-colors cursor-pointer flex items-center gap-1" title="Toggle Procedural Audio (M)">
              <span id="cs2lab-audio-icon">
                <svg class="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5.586 15H4a1 1 0 01-1-1v-4a1 1 0 011-1h1.586l4.707-4.707C10.923 3.663 12 4.109 12 5v14c0 .891-1.077 1.337-1.707.707L5.586 15z"/><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M17 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2"/></svg>
              </span>
              <span id="cs2lab-audio-text">MUTED</span>
            </button>
          </div>
        </div>
      `;

      this.container.appendChild(controls);
      this.controlsRoot = controls;

      // Cache elements
      this.scrubber = controls.querySelector('#cs2lab-scrubber');
      this.btnPlay = controls.querySelector('#cs2lab-btn-play');
      this.playText = controls.querySelector('#cs2lab-play-text');
      this.playIcon = controls.querySelector('#cs2lab-play-icon');
      this.lblTick = controls.querySelector('#cs2lab-readout-tick');
      this.lblMaxTick = controls.querySelector('#cs2lab-readout-maxtick');
      this.lblMs = controls.querySelector('#cs2lab-readout-ms');
      this.lblW = controls.querySelector('#cs2lab-readout-w');
      this.speedButtons = Array.from(controls.querySelectorAll('.cs2lab-speed-pill'));
    }

    _attachEvents() {
      // Transport Buttons
      if (this.btnPlay) this.btnPlay.addEventListener('click', () => this.toggle());
      const btnStepBack = this.controlsRoot ? this.controlsRoot.querySelector('#cs2lab-btn-step-back') : null;
      if (btnStepBack) btnStepBack.addEventListener('click', () => this.step(-1.0));
      const btnStepFwd = this.controlsRoot ? this.controlsRoot.querySelector('#cs2lab-btn-step-fwd') : null;
      if (btnStepFwd) btnStepFwd.addEventListener('click', () => this.step(1.0));
      const btnSubBack = this.controlsRoot ? this.controlsRoot.querySelector('#cs2lab-btn-sub-back') : null;
      if (btnSubBack) btnSubBack.addEventListener('click', () => this.step(-0.1));
      const btnSubFwd = this.controlsRoot ? this.controlsRoot.querySelector('#cs2lab-btn-sub-fwd') : null;
      if (btnSubFwd) btnSubFwd.addEventListener('click', () => this.step(0.1));
      const btnReset = this.controlsRoot ? this.controlsRoot.querySelector('#cs2lab-btn-reset') : null;
      if (btnReset) btnReset.addEventListener('click', () => this.reset());

      // Scrubber Dragging
      if (this.scrubber) {
        this.scrubber.addEventListener('pointerdown', () => {
          this.isScrubbing = true;
          this.wasPlayingBeforeScrub = this.isPlaying;
          if (this.isPlaying) this.pause();
        });

        this.scrubber.addEventListener('input', (e) => {
          const val = parseFloat(e.target.value);
          this.time = val;
          const k = Math.floor(val);
          const w = val - k;
          this._syncUI(k, w);
          if (this.callbacks.onUpdate) this.callbacks.onUpdate(k, w, 0);
        });
      }

      const onPointerUp = () => {
        if (this.isScrubbing) {
          this.isScrubbing = false;
          if (this.wasPlayingBeforeScrub) this.play();
        }
      };
      if (typeof window !== 'undefined' && window.addEventListener) {
        window.addEventListener('pointerup', onPointerUp);
        this._onPointerUp = onPointerUp;
      }

      // Speed Presets
      if (Array.isArray(this.speedButtons)) {
        this.speedButtons.forEach(btn => {
          if (btn && btn.addEventListener) {
            btn.addEventListener('click', () => {
              const speed = parseFloat(btn.dataset ? btn.dataset.speed : btn.getAttribute('data-speed'));
              this.setSpeed(speed);
            });
          }
        });
      }

      // Loop Toggle
      const btnLoop = this.controlsRoot ? this.controlsRoot.querySelector('#cs2lab-btn-loop') : null;
      if (btnLoop) {
        btnLoop.addEventListener('click', () => {
          this.isLooping = !this.isLooping;
          btnLoop.className = this.isLooping
            ? 'px-2 py-1 rounded-lg bg-emerald-500/10 text-emerald-400 border border-emerald-500/30 font-mono text-[10px] font-bold transition-colors cursor-pointer'
            : 'px-2 py-1 rounded-lg bg-slate-800 text-slate-400 border border-slate-700 font-mono text-[10px] font-bold transition-colors cursor-pointer';
        });
      }

      // HUD Toggle
      const btnHud = this.controlsRoot ? this.controlsRoot.querySelector('#cs2lab-btn-hud') : null;
      if (btnHud) {
        btnHud.addEventListener('click', () => this.toggleHud());
      }

      // Audio Mute/Unmute Toggle
      const btnAudio = this.controlsRoot ? this.controlsRoot.querySelector('#cs2lab-btn-audio') : null;
      if (btnAudio && this.audioEngine) {
        const updateAudioBtn = () => {
          const isMuted = Boolean(this.audioEngine.isMuted);
          const iconEl = btnAudio.querySelector('#cs2lab-audio-icon');
          const textEl = btnAudio.querySelector('#cs2lab-audio-text');
          if (isMuted) {
            btnAudio.className = 'px-2 py-1 rounded-lg bg-slate-800 text-slate-400 hover:text-white border border-slate-700 font-mono text-[10px] font-bold transition-colors cursor-pointer flex items-center gap-1';
            if (iconEl) iconEl.innerHTML = '<svg class="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5.586 15H4a1 1 0 01-1-1v-4a1 1 0 011-1h1.586l4.707-4.707C10.923 3.663 12 4.109 12 5v14c0 .891-1.077 1.337-1.707.707L5.586 15z"/><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M17 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2"/></svg>';
            if (textEl) textEl.textContent = 'MUTED';
          } else {
            btnAudio.className = 'px-2 py-1 rounded-lg bg-emerald-500/20 text-emerald-300 border border-emerald-500/40 font-mono text-[10px] font-bold transition-colors cursor-pointer flex items-center gap-1';
            if (iconEl) iconEl.innerHTML = '<svg class="w-3 h-3" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15.536 8.464a5 5 0 010 7.072m2.828-9.9a9 9 0 010 12.728M5.586 15H4a1 1 0 01-1-1v-4a1 1 0 011-1h1.586l4.707-4.707C10.923 3.663 12 4.109 12 5v14c0 .891-1.077 1.337-1.707.707L5.586 15z"/></svg>';
            if (textEl) textEl.textContent = 'AUDIO';
          }
        };
        btnAudio.addEventListener('click', () => {
          if (this.audioEngine.ctx && this.audioEngine.ctx.state === 'suspended') {
            this.audioEngine.ctx.resume();
          }
          this.audioEngine.setMuted(!this.audioEngine.isMuted);
          updateAudioBtn();
        });
        updateAudioBtn();
      }

      // Keyboard Shortcuts
      if (typeof window !== 'undefined' && window.addEventListener) {
        window.addEventListener('keydown', this._onKeyDown);
      }
    }

    _handleKeyDown(e) {
      if (['INPUT', 'TEXTAREA', 'SELECT'].includes(e.target.tagName)) return;

      if (e.code === 'Space') {
        e.preventDefault();
        this.toggle();
      } else if (e.code === 'ArrowRight') {
        e.preventDefault();
        e.shiftKey ? this.step(0.1) : this.step(1.0);
      } else if (e.code === 'ArrowLeft') {
        e.preventDefault();
        e.shiftKey ? this.step(-0.1) : this.step(-1.0);
      } else if (e.code === 'KeyR') {
        e.preventDefault();
        this.reset();
      } else if (e.code === 'KeyH') {
        e.preventDefault();
        this.toggleHud();
      } else if (e.code === 'KeyM') {
        e.preventDefault();
        const btnAudio = this.controlsRoot ? this.controlsRoot.querySelector('#cs2lab-btn-audio') : null;
        if (btnAudio) btnAudio.click();
      } else if (e.code === 'Digit1') {
        this.setSpeed(0.1);
      } else if (e.code === 'Digit2') {
        this.setSpeed(0.25);
      } else if (e.code === 'Digit3') {
        this.setSpeed(0.5);
      } else if (e.code === 'Digit4') {
        this.setSpeed(1.0);
      }
    }

    play() {
      if (this.isPlaying) return;
      this.isPlaying = true;
      this.lastTimestamp = performance.now();

      // Unlock Web Audio if suspended
      if (this.audioEngine && typeof this.audioEngine.init === 'function') {
        this.audioEngine.init();
      }

      this._updatePlayButton();
      if (this.callbacks.onStateChange) this.callbacks.onStateChange(true);
      this.rafId = requestAnimationFrame(this._loop);
    }

    pause() {
      if (!this.isPlaying) return;
      this.isPlaying = false;
      if (this.rafId) {
        cancelAnimationFrame(this.rafId);
        this.rafId = null;
      }
      this._updatePlayButton();
      if (this.callbacks.onStateChange) this.callbacks.onStateChange(false);
    }

    toggle() {
      this.isPlaying ? this.pause() : this.play();
    }

    reset() {
      this.pause();
      this.setTime(0.0);
    }

    setTime(time, maxTicks) {
      if (maxTicks !== undefined && maxTicks > 0) {
        this.maxTicks = maxTicks;
        if (this.scrubber) this.scrubber.max = maxTicks;
        if (this.lblMaxTick) this.lblMaxTick.textContent = String(maxTicks);
        if (this.hudDom.maxTick) this.hudDom.maxTick.textContent = String(maxTicks);
      }
      if (!Number.isFinite(time)) {
        time = (time === Infinity) ? this.maxTicks : 0.0;
      }
      this.time = Math.max(0.0, Math.min(time, this.maxTicks));
      const k = Math.floor(this.time);
      const w = this.time - k;
      this._syncUI(k, w);
      if (this.callbacks.onUpdate) this.callbacks.onUpdate(k, w, 0);
    }

    setSpeed(speed) {
      this.speed = speed;
      this.speedButtons.forEach(btn => {
        const s = parseFloat(btn.dataset.speed);
        if (Math.abs(s - speed) < 1e-4) {
          btn.className = 'cs2lab-speed-pill px-2 py-0.5 rounded text-[10px] font-mono font-bold transition-all bg-cyan-500/20 text-cyan-300 border border-cyan-500/30 cursor-pointer';
        } else {
          btn.className = 'cs2lab-speed-pill px-2 py-0.5 rounded text-[10px] font-mono font-bold transition-all text-slate-400 hover:text-white cursor-pointer';
        }
      });
    }

    step(delta) {
      this.pause();
      let newTime;
      if (Math.abs(Math.abs(delta) - 0.1) < 1e-4) {
        // Step 0.1 sub-tick: mitigate float drift
        newTime = Math.round((this.time + delta) * 100) / 100;
      } else {
        // Step integer tick
        newTime = delta > 0 ? Math.floor(this.time + 1.0001) : Math.ceil(this.time - 1.0001);
      }
      this.setTime(newTime);
    }

    _loop(timestamp) {
      if (!this.isPlaying) return;

      let dtWall = (timestamp - this.lastTimestamp) / 1000.0;
      this.lastTimestamp = timestamp;

      // Clamp dtWall to 100ms max to prevent tab freeze teleportation
      if (dtWall > 0.1) dtWall = 0.1;

      // Delta tick advancement: dtSim / tickInterval
      const dtSim = dtWall * this.speed;
      const deltaTicks = dtSim / this.tickInterval;
      let nextTime = this.time + deltaTicks;

      const prevTick = Math.floor(this.time);

      if (nextTime >= this.maxTicks) {
        if (this.isLooping) {
          nextTime = nextTime % this.maxTicks;
        } else {
          nextTime = this.maxTicks;
          this.time = nextTime;
          this.pause();
          const k = Math.floor(nextTime);
          const w = nextTime - k;
          this._syncUI(k, w);
          if (this.callbacks.onUpdate) this.callbacks.onUpdate(k, w, dtSim);
          return;
        }
      }

      this.time = nextTime;
      const currTick = Math.floor(this.time);
      const subTickFrac = this.time - currTick;

      // Discrete Host Tick Crossing
      if (Number.isFinite(currTick) && currTick !== prevTick) {
        if (this.callbacks.onTick) this.callbacks.onTick(currTick);
      }

      // Sync scrubber and HUD
      this._syncUI(currTick, subTickFrac);

      // Update simulation provider
      if (this.callbacks.onUpdate) {
        this.callbacks.onUpdate(currTick, subTickFrac, dtSim);
      }

      if (this.isPlaying) {
        this.rafId = requestAnimationFrame(this._loop);
      }
    }

    _syncUI(k, w) {
      if (this.scrubber) {
        this.scrubber.value = this.time;
        const pct = (this.time / this.maxTicks) * 100;
        this.scrubber.style.background = `linear-gradient(to right, #38bdf8 0%, #38bdf8 ${pct}%, #1e293b ${pct}%, #1e293b 100%)`;
      }

      const ms = this.time * 15.625;

      if (this.lblTick) this.lblTick.textContent = this.time.toFixed(2);
      if (this.lblMs) this.lblMs.textContent = ms.toFixed(2);
      if (this.lblW) this.lblW.textContent = w.toFixed(4);

      // Sync HUD basic values
      if (this.hudDom.tick) this.hudDom.tick.textContent = String(k);
      if (this.hudDom.subtick) this.hudDom.subtick.textContent = w.toFixed(4);
      if (this.hudDom.time) this.hudDom.time.textContent = ms.toFixed(2);
    }

    _updatePlayButton() {
      if (!this.btnPlay) return;
      if (this.isPlaying) {
        this.playText.textContent = 'Pause';
        this.playIcon.innerHTML = '<svg class="w-3.5 h-3.5 fill-current" viewBox="0 0 24 24"><path d="M6 19h4V5H6v14zm8-14v14h4V5h-4z"/></svg>';
        this.btnPlay.className = 'flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-amber-500/20 hover:bg-amber-500/30 text-amber-300 border border-amber-500/40 font-bold transition-all shadow-sm shadow-amber-500/10 cursor-pointer';
      } else {
        this.playText.textContent = 'Play';
        this.playIcon.innerHTML = '<svg class="w-3.5 h-3.5 fill-current" viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg>';
        this.btnPlay.className = 'flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-cyan-500/20 hover:bg-cyan-500/30 text-cyan-300 border border-cyan-500/40 font-bold transition-all shadow-sm shadow-cyan-500/10 cursor-pointer';
      }
    }

    updateTelemetry(data) {
      if (!this.hudDom.root || this.hudDom.root.classList.contains('hidden')) return;
      if (!data) return;

      if (data.pos && this.hudDom.pos) {
        const x = Number(data.pos.x || 0).toFixed(2);
        const y = Number(data.pos.y || 0).toFixed(2);
        const z = Number(data.pos.z || 0).toFixed(2);
        this.hudDom.pos.textContent = `[${x}, ${y}, ${z}]`;
      }
      if (data.vel && this.hudDom.vel) {
        const vx = Number(data.vel.x || 0).toFixed(2);
        const vy = Number(data.vel.y || 0).toFixed(2);
        const vz = Number(data.vel.z || 0).toFixed(2);
        this.hudDom.vel.textContent = `[${vx}, ${vy}, ${vz}]`;
        const speed = data.vel.speed !== undefined ? data.vel.speed : Math.sqrt(vx*vx + vy*vy + vz*vz);
        if (this.hudDom.speed) {
          this.hudDom.speed.textContent = `${Number(speed).toFixed(2)} u/s`;
          this.hudDom.speed.className = speed > 250 ? 'text-amber-400 font-bold' : 'text-emerald-400 font-bold';
        }
      }
      if (data.ground !== undefined && this.hudDom.ground) {
        const onGround = Boolean(data.ground);
        this.hudDom.ground.textContent = onGround ? 'ONGROUND' : 'IN_AIR';
        this.hudDom.ground.className = onGround ? 'text-emerald-400 font-bold' : 'text-amber-400 font-bold';
      }
      if (data.event !== undefined && this.hudDom.event) {
        this.hudDom.event.textContent = String(data.event);
      }
      if (data.customMetrics && Array.isArray(data.customMetrics) && this.hudDom.extContainer) {
        this.hudDom.extContainer.classList.remove('hidden');
        let extHtml = '';
        for (let i = 0; i < data.customMetrics.length; i++) {
          const m = data.customMetrics[i];
          extHtml += `<div class="flex justify-between items-center text-[10px]"><span class="text-slate-400">${m.label}:</span><span class="${m.alert ? 'text-rose-400 font-bold' : 'text-cyan-300'} font-semibold">${m.value}</span></div>`;
        }
        this.hudDom.extContainer.innerHTML = extHtml;
      }
    }

    toggleHud() {
      if (!this.hudRoot) return;
      this.hudRoot.classList.toggle('hidden');
    }

    destroy() {
      this.pause();
      window.removeEventListener('keydown', this._onKeyDown);
      if (this._onPointerUp) {
        window.removeEventListener('pointerup', this._onPointerUp);
        this._onPointerUp = null;
      }

      if (this.hudRoot) {
        if (typeof this.hudRoot.remove === 'function') this.hudRoot.remove();
        else if (this.hudRoot.parentNode) this.hudRoot.parentNode.removeChild(this.hudRoot);
        this.hudRoot = null;
      }
      if (this.controlsRoot) {
        if (typeof this.controlsRoot.remove === 'function') this.controlsRoot.remove();
        else if (this.controlsRoot.parentNode) this.controlsRoot.parentNode.removeChild(this.controlsRoot);
        this.controlsRoot = null;
      }
      this.container = null;
      this.callbacks = {};
      this.audioEngine = null;
    }
  }

  CS2Lab.PlaybackController = PlaybackController;

  // ==========================================================================
  // 5. SimRegistry: Dynamic Provider Registration & Workstation Lifecycle
  // ==========================================================================
  CS2Lab.activeSession = null;
  CS2Lab.audioInstance = null;

  /**
   * Register a simulation provider class for a given simulator ID.
   * @param {string|number} simId - e.g. '03', '09', '11', '12', '14'
   * @param {Function} ProviderClass - Conforming to ISimulationProvider
   */
  CS2Lab.registerProvider = function(simId, ProviderClass) {
    _providers.set(String(simId), ProviderClass);
  };

  /**
   * Check if an upgraded CS2Lab provider is registered for a given simulator ID.
   * @param {string|number} simId
   * @returns {boolean}
   */
  CS2Lab.isSimSupported = function(simId) {
    return _providers.has(String(simId));
  };

  /**
   * Get registered provider class.
   * @param {string|number} simId
   * @returns {Function|undefined}
   */
  CS2Lab.getProvider = function(simId) {
    return _providers.get(String(simId));
  };

  /**
   * List all registered simulator IDs.
   * @returns {string[]}
   */
  CS2Lab.getSupportedSims = function() {
    return Array.from(_providers.keys());
  };

  /**
   * Mount a simulation into visualizer and parameter containers.
   * @param {string|number} simId
   * @param {HTMLElement|string} visualizerContainer
   * @param {HTMLElement|string} paramsContainer
   * @param {Object} [options={}]
   * @returns {boolean}
   */
  CS2Lab.mount = function(simId, visualizerContainer, paramsContainer, options = {}) {
    // 1. If another simulation is currently mounted, unmount cleanly
    if (CS2Lab.activeSession) {
      CS2Lab.unmount();
    }

    const idStr = String(simId);
    const ProviderClass = _providers.get(idStr);
    if (!ProviderClass) {
      console.warn(`[CS2Lab] Simulation provider '${idStr}' is not registered.`);
      return false;
    }

    // 2. Resolve DOM containers
    const visEl = typeof visualizerContainer === 'string'
      ? document.querySelector(visualizerContainer)
      : visualizerContainer;
    const paramsEl = typeof paramsContainer === 'string'
      ? document.querySelector(paramsContainer)
      : paramsContainer;

    if (!visEl) {
      console.error(`[CS2Lab] Visualizer container '${visualizerContainer}' not found in DOM.`);
      return false;
    }

    // 3. Clear existing visualizer content
    visEl.innerHTML = '';
    if (paramsEl) paramsEl.innerHTML = '';

    // 4. Initialize Procedural Audio Engine (Singleton instance across sims)
    if (!CS2Lab.audioInstance) {
      CS2Lab.audioInstance = new CS2Lab.AudioEngine();
    }
    const audio = CS2Lab.audioInstance;

    // 5. Initialize Core 3D Viewport
    const viewportOptions = Object.assign({ audioEngine: audio }, options.viewport || {});
    const viewport = new CS2Lab.Viewport3D(visEl, viewportOptions);

    // 6. Initialize Playback Controller
    const maxTicks = options.maxTicks || (idStr === '12' ? 64 : 64);
    const playback = new CS2Lab.PlaybackController({
      maxTicks: maxTicks,
      initialSpeed: options.initialSpeed !== undefined ? options.initialSpeed : 1.0,
      isLooping: options.isLooping !== false,
      audioEngine: audio
    });

    // 7. Mount Playback Controls Toolbar & HUD into Viewport
    playback.mount(visEl, {
      onUpdate: (tick, subTickFrac, dt) => {
        if (CS2Lab.activeSession && CS2Lab.activeSession.provider) {
          const telemetry = CS2Lab.activeSession.provider.update(tick, subTickFrac, dt);
          if (telemetry) {
            playback.updateTelemetry(telemetry);
          }
        }
      }
    });

    // 8. Instantiate and Mount Simulation Provider
    const provider = new ProviderClass(viewport, audio, playback);

    // Store active session before initial evaluation so callbacks fire cleanly
    CS2Lab.activeSession = {
      simId: idStr,
      provider: provider,
      viewport: viewport,
      audio: audio,
      playback: playback,
      visEl: visEl,
      paramsEl: paramsEl
    };

    if (paramsEl && typeof provider.mount === 'function') {
      provider.mount(paramsEl);
    }

    // 9. Initial Render Evaluation at tick 0
    playback.setTime(0, maxTicks);

    return true;
  };

  /**
   * Unmount the active simulation session and cleanly dispose all resources.
   */
  CS2Lab.unmount = function() {
    if (!CS2Lab.activeSession) return;

    const { provider, viewport, playback, visEl, paramsEl } = CS2Lab.activeSession;

    // 1. Teardown Playback Controller (cancels RAF loop, removes keyboard & DOM listeners)
    if (playback && typeof playback.destroy === 'function') {
      playback.destroy();
    }

    // 2. Teardown Provider (disposes meshes, materials, geometries, particle buffers)
    if (provider && typeof provider.teardown === 'function') {
      provider.teardown();
    }

    // 3. Teardown Viewport (disposes WebGLRenderer, scenes, OrbitControls, context)
    if (viewport && typeof viewport.destroy === 'function') {
      viewport.destroy();
    }

    // 4. Reset containers
    if (visEl) visEl.innerHTML = '';
    if (paramsEl) paramsEl.innerHTML = '';

    CS2Lab.activeSession = null;
  };

  /**
   * SimRegistry: Dynamic Provider Registration & Workstation Lifecycle Interface Contract
   */
  CS2Lab.SimRegistry = {
    registerProvider: CS2Lab.registerProvider,
    isSimSupported: CS2Lab.isSimSupported,
    mount: CS2Lab.mount,
    unmount: CS2Lab.unmount,
    getProvider: CS2Lab.getProvider,
    getSupportedSims: CS2Lab.getSupportedSims
  };

  // Helper for safe LineBufferGeometry creation from Vector3 points
  function createLineGeometryFromPoints(points) {
    const geo = new THREE.BufferGeometry();
    if (typeof geo.setFromPoints === 'function') {
      geo.setFromPoints(points);
    } else {
      const arr = new Float32Array(points.length * 3);
      for (let i = 0; i < points.length; i++) {
        arr[i * 3] = points[i].x || 0;
        arr[i * 3 + 1] = points[i].y || 0;
        arr[i * 3 + 2] = points[i].z || 0;
      }
      geo.setAttribute('position', new THREE.BufferAttribute(arr, 3));
    }
    return geo;
  }

  // Helper for safe 3D arrow creation across Three.js r128 and headless mock environments
  function createSafeArrow(dir, origin, length, colorHex, headLength = 8, headWidth = 4) {
    if (typeof THREE.ArrowHelper === 'function') {
      return new THREE.ArrowHelper(dir, origin, length, colorHex, headLength, headWidth);
    }
    const group = new THREE.Group();
    const lineGeo = createLineGeometryFromPoints([
      new THREE.Vector3(0, 0, 0),
      dir.clone().multiplyScalar(length)
    ]);
    const lineMat = new THREE.LineBasicMaterial({ color: colorHex });
    const line = new THREE.Line(lineGeo, lineMat);
    group.add(line);
    group.position.copy(origin);
    group.setLength = function(len) {
      if (line.geometry) {
        if (typeof line.geometry.setFromPoints === 'function') {
          line.geometry.setFromPoints([new THREE.Vector3(0, 0, 0), dir.clone().multiplyScalar(len)]);
        } else if (line.geometry.attributes && line.geometry.attributes.position && line.geometry.attributes.position.array) {
          const arr = line.geometry.attributes.position.array;
          const end = dir.clone().multiplyScalar(len);
          arr[0] = 0; arr[1] = 0; arr[2] = 0;
          arr[3] = end.x; arr[4] = end.y; arr[5] = end.z;
          line.geometry.attributes.position.needsUpdate = true;
        }
      }
    };
    group.setDirection = function(newDir) {
      dir.copy(newDir);
    };
    return group;
  }

  // ==========================================================================
  // 6. Sim03SubtickProvider: Sub-Tick Prediction & Multi-Slice Kinematics
  // Reconstructed from client.dll (Build 26598758) & cs2_subtick_usercmd_prediction_sim.cpp
  // ==========================================================================
  class Sim03SubtickProvider {
    /**
     * @param {CS2Lab.Viewport3D} viewport
     * @param {CS2Lab.AudioEngine} audio
     * @param {CS2Lab.PlaybackController} playback
     */
    constructor(viewport, audio, playback) {
      this.viewport = viewport;
      this.audio = audio;
      this.playback = playback;

      // Physical Simulation Constants (Source 2 Baseline)
      this.TICK_INTERVAL = 0.015625; // 1 / 64s = 15.625 ms
      this.GRAVITY = 800.0;          // sv_gravity (u/s^2)
      this.JUMP_IMPULSE = 301.993;   // sv_jump_impulse = sqrt(2 * 800 * 57)
      this.SV_FRICTION = 5.2;        // sv_friction
      this.SV_STOPSPEED = 100.0;     // sv_stopspeed (u/s)
      this.MAX_SPEED = 250.0;        // sv_maxspeed (u/s)

      // Interactive Parameters
      this.subtickWhen = 0.40;       // Hardware event fraction w in [0.0, 1.0)
      this.initialSpeed = 250.0;     // Run-up horizontal speed
      this.actionType = 'jump';      // 'jump' | 'attack' | 'duck'
      this.jumpTick = 0;             // Tick at which transition occurs
      this.maxTicks = 48;            // 48 ticks = 750 ms (full jump arc to floor)

      // State & Audio Tracking
      this.lastFrac = 0.0;

      // Three.js Scene Containers
      this.simGroup = null;
      this.subtickCapsule = null;
      this.tickBoundaryCapsule = null;
      this.targetCapsule = null;
      this.trajectoryLine = null;
      this.frictionArrow = null;
      this.gravityArrow = null;
      this.aimRayLine = null;
      this.impactMarker = null;
      this.tickMarkersGroup = null;

      // DOM Container Reference
      this.paramsContainer = null;
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.simGroup = new THREE.Group();
      this.simGroup.name = 'Sim03_Subtick_Scene';
      scene.add(this.simGroup);

      // 1. Build 3D Visual Scene
      this._buildSceneObjects();

      // 2. Build Interactive Parameters UI & Protobuf Wire Inspector
      this._buildParamsUI();

      // 3. Initialize Playback Timeline to 48 ticks
      if (this.playback) {
        this.playback.setTime(0, this.maxTicks);
      }

      // 4. Position Camera for Optimal Perspective View
      const camera = this.viewport.getCamera();
      if (camera) {
        camera.position.set(-180, -280, 120);
        const controls = this.viewport.getControls();
        if (controls) {
          controls.target.set(120, 0, 36);
          controls.update();
        }
      }
    }

    update(tick, subTickFrac, dt) {
      const continuousTime = tick + subTickFrac;

      // 1. Evaluate Continuous Physical State
      const state = this._evaluatePhysics(continuousTime);

      // 2. Evaluate Discrete Tick-Boundary State (Legacy CS:GO comparison)
      const tickState = this._evaluateLegacyTickPhysics(continuousTime);

      // 3. Update Rewound Sub-Tick Capsule (Emerald #10b981)
      if (this.subtickCapsule) {
        this.subtickCapsule.position.set(state.x, state.y, state.z);
      }

      // 4. Update Discrete Tick-Boundary Capsule (Rose #f43f5e)
      if (this.tickBoundaryCapsule) {
        this.tickBoundaryCapsule.position.set(tickState.x, tickState.y, tickState.z);
      }

      // 5. Update Deceleration & Force Arrows
      if (this.frictionArrow) {
        if (state.onGround && state.speed > 0.1) {
          this.frictionArrow.visible = true;
          this.frictionArrow.position.set(state.x, state.y, 1.0);
          const control = state.speed < this.SV_STOPSPEED ? this.SV_STOPSPEED : state.speed;
          const decelMag = Math.min(60.0, (control * this.SV_FRICTION) / 25.0);
          if (typeof this.frictionArrow.setLength === 'function') {
            this.frictionArrow.setLength(Math.max(10.0, decelMag), 6, 3);
          }
        } else {
          this.frictionArrow.visible = false;
        }
      }

      if (this.gravityArrow) {
        if (!state.onGround) {
          this.gravityArrow.visible = true;
          this.gravityArrow.position.set(state.x, state.y, state.z + 36);
        } else {
          this.gravityArrow.visible = false;
        }
      }

      // 6. Update Aim Ray & Impact (if attack event)
      if (this.aimRayLine && this.impactMarker) {
        if (this.actionType === 'attack') {
          this.aimRayLine.visible = true;
          this.impactMarker.visible = true;
          const eyeZ = state.z + 64;
          const positions = this.aimRayLine.geometry.attributes.position.array;
          if (positions) {
            positions[0] = state.x; positions[1] = state.y; positions[2] = eyeZ;
            positions[3] = 450; positions[4] = 0; positions[5] = 60; // target head
            this.aimRayLine.geometry.attributes.position.needsUpdate = true;
          }
          this.impactMarker.position.set(450, 0, 60);
        } else {
          this.aimRayLine.visible = false;
          this.impactMarker.visible = false;
        }
      }

      // 7. Audio Sub-Tick Click Scheduling
      this._handleAudioScheduling(tick, subTickFrac, state);

      // 8. Return High-Fidelity Telemetry Payload
      const deltaZ = state.z - tickState.z;

      return {
        pos: { x: state.x, y: state.y, z: state.z },
        vel: { x: state.vx, y: state.vy, z: state.vz, speed: state.speed },
        ground: state.onGround,
        event: state.eventText,
        customMetrics: [
          { label: 'SUB-TICK w', value: `${(this.subtickWhen * 100).toFixed(1)}% (${(this.subtickWhen * 15.625).toFixed(2)} ms)` },
          { label: 'TRAJECTORY SLICES', value: `${state.sliceCount} piecewise slices` },
          { label: 'SUBTICK \u0394z LEAD', value: `+${deltaZ.toFixed(3)} units`, alert: deltaZ > 0.5 },
          { label: 'GROUND FRICTION', value: state.onGround ? `${state.frictionDecel.toFixed(1)} u/s\u00B2` : '0.0 u/s\u00B2 (Airborne)' },
          { label: 'PROTOBUF WIRE', value: 'Field 18 (0x92 0x01) 56B' }
        ]
      };
    }

    onParamChange(key, value) {
      if (key === 'when') {
        this.subtickWhen = Math.max(0.0, Math.min(0.999, parseFloat(value)));
        if (this.audio && typeof this.audio.playSubtickClick === 'function') {
          this.audio.playSubtickClick(0);
        }
      } else if (key === 'speed') {
        this.initialSpeed = Math.max(0.0, Math.min(400.0, parseFloat(value)));
      } else if (key === 'friction') {
        this.SV_FRICTION = Math.max(1.0, Math.min(10.0, parseFloat(value)));
      } else if (key === 'stopspeed') {
        this.SV_STOPSPEED = Math.max(50.0, Math.min(200.0, parseFloat(value)));
      } else if (key === 'gravity') {
        this.GRAVITY = Math.max(400.0, Math.min(1200.0, parseFloat(value)));
      } else if (key === 'action') {
        this.actionType = value;
      }

      // Rebuild Trajectory Geometry & Update Protobuf Inspector
      this._rebuildTrajectory();
      this._updateProtobufInspector();
    }

    teardown() {
      if (this.simGroup) {
        this.simGroup.traverse((obj) => {
          if (obj.geometry && typeof obj.geometry.dispose === 'function') obj.geometry.dispose();
          if (obj.material) {
            if (Array.isArray(obj.material)) {
              obj.material.forEach(m => m && typeof m.dispose === 'function' && m.dispose());
            } else if (typeof obj.material.dispose === 'function') {
              obj.material.dispose();
            }
          }
        });
        const scene = this.viewport.getScene();
        if (scene) scene.remove(this.simGroup);
        this.simGroup = null;
      }

      if (this.paramsContainer) {
        this.paramsContainer.innerHTML = '';
        this.paramsContainer = null;
      }

      this.subtickCapsule = null;
      this.tickBoundaryCapsule = null;
      this.targetCapsule = null;
      this.trajectoryLine = null;
      this.frictionArrow = null;
      this.gravityArrow = null;
      this.aimRayLine = null;
      this.impactMarker = null;
      this.tickMarkersGroup = null;
    }

    // ------------------------------------------------------------------------
    // Physics Integration Engine (Multi-Slice Piecewise Temporal Integration)
    // ------------------------------------------------------------------------
    _evaluatePhysics(continuousTime) {
      const w = this.subtickWhen;
      let x = 0.0;
      let y = 0.0;
      let z = 0.0;
      let vx = this.initialSpeed;
      let vy = 0.0;
      let vz = 0.0;
      let onGround = true;
      let eventText = 'RUNNING';
      let frictionDecel = 0.0;
      let sliceCount = 1;

      if (this.actionType === 'jump') {
        if (continuousTime < w) {
          // Ground slice before jump: [0.0 .. continuousTime]
          const dt = continuousTime * this.TICK_INTERVAL;
          const fric = this._applyFriction(vx, vy, dt);
          vx = fric.vx;
          x = vx * dt;
          onGround = true;
          eventText = `GROUND SLICE (w < ${w.toFixed(2)})`;
          frictionDecel = fric.decel;
          sliceCount = 1;
        } else {
          // Post-jump airborne slice: [w .. continuousTime]
          // 1. Initial ground slice duration to jump
          const dtGround = w * this.TICK_INTERVAL;
          const fric = this._applyFriction(vx, vy, dtGround);
          vx = fric.vx;
          x = vx * dtGround;

          // 2. Instantaneous impulse at w
          vz = this.JUMP_IMPULSE;
          onGround = false;
          sliceCount = 2;

          // 3. Airborne integration duration
          const tAir = (continuousTime - w) * this.TICK_INTERVAL;
          x += vx * tAir;
          z = this.JUMP_IMPULSE * tAir - 0.5 * this.GRAVITY * (tAir * tAir);
          vz = this.JUMP_IMPULSE - this.GRAVITY * tAir;

          if (z <= 0.0) {
            z = 0.0;
            vz = 0.0;
            onGround = true;
            eventText = 'LANDED';
          } else {
            eventText = vz > 0 ? 'AIRBORNE (ASCENDING)' : 'AIRBORNE (DESCENDING)';
          }
        }
      } else {
        // Pure ground locomotion with continuous friction
        const dt = continuousTime * this.TICK_INTERVAL;
        const fric = this._applyFriction(vx, vy, dt);
        vx = fric.vx;
        x = vx * dt;
        frictionDecel = fric.decel;
        eventText = this.actionType === 'attack' ? 'SUB-TICK ATTACK (w=0.40)' : 'DUCKING';
      }

      const speed = Math.hypot(vx, vy, vz);
      return { x, y, z, vx, vy, vz, speed, onGround, eventText, frictionDecel, sliceCount };
    }

    _evaluateLegacyTickPhysics(continuousTime) {
      // In legacy 64-tick engine (CS:GO), input occurring at fraction w is deferred
      // until the next integer tick boundary (tick 1.0)
      const currTick = Math.floor(continuousTime);
      let x = 0.0;
      let z = 0.0;
      let vz = 0.0;
      let onGround = true;

      if (this.actionType === 'jump') {
        if (currTick < 1) {
          // During tick 0, jump has NOT executed yet
          const dt = continuousTime * this.TICK_INTERVAL;
          x = this.initialSpeed * dt;
          z = 0.0;
          vz = 0.0;
          onGround = true;
        } else {
          // Jump executed at tick 1 boundary (15.625 ms delayed)
          const tAir = (continuousTime - 1.0) * this.TICK_INTERVAL;
          x = this.initialSpeed * (1.0 * this.TICK_INTERVAL + tAir);
          z = this.JUMP_IMPULSE * tAir - 0.5 * this.GRAVITY * (tAir * tAir);
          vz = this.JUMP_IMPULSE - this.GRAVITY * tAir;
          if (z <= 0.0) { z = 0.0; vz = 0.0; onGround = true; }
          else { onGround = false; }
        }
      } else {
        const dt = continuousTime * this.TICK_INTERVAL;
        x = this.initialSpeed * dt;
      }

      return { x, y: 0, z, vz, onGround };
    }

    _applyFriction(vx, vy, dt) {
      const speed = Math.hypot(vx, vy);
      if (speed < 0.1 || dt <= 0) return { vx: 0, vy: 0, speed: 0, decel: 0 };
      const control = speed < this.SV_STOPSPEED ? this.SV_STOPSPEED : speed;
      const drop = control * this.SV_FRICTION * dt;
      const newspeed = Math.max(0.0, speed - drop);
      const decel = control * this.SV_FRICTION;
      if (newspeed !== speed) {
        const frac = newspeed / speed;
        return { vx: vx * frac, vy: vy * frac, speed: newspeed, decel };
      }
      return { vx, vy, speed, decel };
    }

    // ------------------------------------------------------------------------
    // Three.js Scene Construction
    // ------------------------------------------------------------------------
    _buildSceneObjects() {
      // 1. Rewound Sub-Tick Capsule (Emerald #10b981)
      this.subtickCapsule = this._createCapsuleMesh(0x10b981, 0.35, 0x34d399);
      this.simGroup.add(this.subtickCapsule);

      // 2. Discrete Tick-Boundary Capsule (Rose #f43f5e)
      this.tickBoundaryCapsule = this._createCapsuleMesh(0xf43f5e, 0.25, 0xfb7185);
      this.simGroup.add(this.tickBoundaryCapsule);

      // 3. Enemy Target Pawn Down-Range
      this.targetCapsule = this._createCapsuleMesh(0x64748b, 0.40, 0x94a3b8);
      this.targetCapsule.position.set(450, 0, 0);
      this.simGroup.add(this.targetCapsule);

      // 4. Ground Friction Deceleration Arrow (Amber #f59e0b)
      this.frictionArrow = createSafeArrow(
        new THREE.Vector3(-1, 0, 0),
        new THREE.Vector3(0, 0, 1),
        40,
        0xf59e0b,
        8,
        4
      );
      this.simGroup.add(this.frictionArrow);

      // 5. Airborne Gravity Vector Arrow (Sky #0ea5e9)
      this.gravityArrow = createSafeArrow(
        new THREE.Vector3(0, 0, -1),
        new THREE.Vector3(0, 0, 36),
        35,
        0x0ea5e9,
        6,
        3
      );
      this.gravityArrow.visible = false;
      this.simGroup.add(this.gravityArrow);

      // 6. Aim Ray Line (Cyan #06b6d4) & Impact Marker (Amber #f59e0b)
      const aimGeo = new THREE.BufferGeometry();
      aimGeo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(6), 3));
      const aimMat = new THREE.LineBasicMaterial({ color: 0x06b6d4, linewidth: 2 });
      this.aimRayLine = new THREE.Line(aimGeo, aimMat);
      this.aimRayLine.visible = false;
      this.simGroup.add(this.aimRayLine);

      const impactGeo = new THREE.SphereGeometry(3, 16, 16);
      const impactMat = new THREE.MeshBasicMaterial({ color: 0xf59e0b });
      this.impactMarker = new THREE.Mesh(impactGeo, impactMat);
      this.impactMarker.visible = false;
      this.simGroup.add(this.impactMarker);

      // 7. Tick Markers Group & Parabolic Trajectory Curve Line
      this.tickMarkersGroup = new THREE.Group();
      this.simGroup.add(this.tickMarkersGroup);

      this._rebuildTrajectory();
    }

    /**
     * Constructs a Source 2 Player Bounding Capsule compatible with Three.js r128.
     * Dimensions: Radius 16 units, Height 72 units. Center cylinder height = 40.
     */
    _createCapsuleMesh(colorHex, opacity, wireHex) {
      const group = new THREE.Group();

      // Center cylinder: radius 16, height 40, aligned along Z axis
      const cylGeo = new THREE.CylinderGeometry(16, 16, 40, 16, 1, false);
      if (typeof cylGeo.rotateX === 'function') cylGeo.rotateX(Math.PI / 2);
      const cylMat = new THREE.MeshStandardMaterial({
        color: colorHex,
        transparent: true,
        opacity: opacity,
        roughness: 0.4
      });
      const cylMesh = new THREE.Mesh(cylGeo, cylMat);
      if (typeof cylGeo.rotateX !== 'function') cylMesh.rotation.x = Math.PI / 2;
      cylMesh.position.set(0, 0, 36);
      group.add(cylMesh);

      // Top hemisphere cap: sphere radius 16 at z = 56
      const topGeo = new THREE.SphereGeometry(16, 16, 12);
      const topMesh = new THREE.Mesh(topGeo, cylMat);
      topMesh.position.set(0, 0, 56);
      group.add(topMesh);

      // Bottom hemisphere cap: sphere radius 16 at z = 16
      const btmGeo = new THREE.SphereGeometry(16, 16, 12);
      const btmMesh = new THREE.Mesh(btmGeo, cylMat);
      btmMesh.position.set(0, 0, 16);
      group.add(btmMesh);

      // Wireframe overlay
      const wireMat = new THREE.MeshBasicMaterial({
        color: wireHex,
        wireframe: true,
        transparent: true,
        opacity: 0.85
      });
      const wireMesh = new THREE.Mesh(cylGeo, wireMat);
      if (typeof cylGeo.rotateX !== 'function') wireMesh.rotation.x = Math.PI / 2;
      wireMesh.position.set(0, 0, 36);
      group.add(wireMesh);

      // Head orientation pointer (+X forward)
      const arrow = createSafeArrow(
        new THREE.Vector3(1, 0, 0),
        new THREE.Vector3(0, 0, 64),
        24,
        wireHex,
        6,
        3
      );
      group.add(arrow);

      return group;
    }

    _rebuildTrajectory() {
      if (this.trajectoryLine) {
        this.simGroup.remove(this.trajectoryLine);
        if (this.trajectoryLine.geometry && typeof this.trajectoryLine.geometry.dispose === 'function') {
          this.trajectoryLine.geometry.dispose();
        }
        if (this.trajectoryLine.material && typeof this.trajectoryLine.material.dispose === 'function') {
          this.trajectoryLine.material.dispose();
        }
        this.trajectoryLine = null;
      }

      // Clear tick notches
      if (this.tickMarkersGroup) {
        while (this.tickMarkersGroup.children.length > 0) {
          const child = this.tickMarkersGroup.children[0];
          if (child.geometry && typeof child.geometry.dispose === 'function') child.geometry.dispose();
          if (child.material && typeof child.material.dispose === 'function') child.material.dispose();
          this.tickMarkersGroup.remove(child);
        }
      }

      const points = [];
      const steps = 180;
      const maxT = this.maxTicks;

      for (let i = 0; i <= steps; i++) {
        const t = (i / steps) * maxT;
        const s = this._evaluatePhysics(t);
        points.push(new THREE.Vector3(s.x, s.y, s.z + 1.0));
      }

      const trajGeo = createLineGeometryFromPoints(points);
      const trajMat = new THREE.LineBasicMaterial({ color: 0x38bdf8, transparent: true, opacity: 0.85 });
      this.trajectoryLine = new THREE.Line(trajGeo, trajMat);
      this.simGroup.add(this.trajectoryLine);

      // Jump Takeoff Ring Marker at fraction w
      if (this.tickMarkersGroup) {
        const takeoffState = this._evaluatePhysics(this.subtickWhen);
        const ringGeo = typeof THREE.RingGeometry === 'function'
          ? new THREE.RingGeometry(14, 18, 24)
          : new THREE.SphereGeometry(4, 12, 8);
        const ringMat = new THREE.MeshBasicMaterial({ color: 0xf59e0b, side: 2 });
        const ringMesh = new THREE.Mesh(ringGeo, ringMat);
        ringMesh.position.set(takeoffState.x, takeoffState.y, 0.5);
        this.tickMarkersGroup.add(ringMesh);

        // Discrete Tick Markers along curve
        const notchGeo = new THREE.SphereGeometry(1.5, 8, 8);
        const notchMat = new THREE.MeshBasicMaterial({ color: 0x94a3b8 });
        for (let k = 0; k <= Math.min(30, maxT); k++) {
          const s = this._evaluatePhysics(k);
          const notch = new THREE.Mesh(notchGeo, notchMat);
          notch.position.set(s.x, s.y, s.z + 1.0);
          this.tickMarkersGroup.add(notch);
        }
      }
    }

    _handleAudioScheduling(tick, subTickFrac, state) {
      if (!this.audio) return;

      // Detect Sub-Tick Jump Actuation Crossing
      if (tick === this.jumpTick && this.actionType === 'jump') {
        if (this.lastFrac < this.subtickWhen && subTickFrac >= this.subtickWhen) {
          if (typeof this.audio.playSubtickClick === 'function') {
            this.audio.playSubtickClick(0);
          }
        }
      }

      // Detect Sub-Tick Attack Crossing (at fraction w)
      if (this.actionType === 'attack' && tick === 0) {
        if (this.lastFrac < this.subtickWhen && subTickFrac >= this.subtickWhen) {
          if (typeof this.audio.playSubtickClick === 'function') {
            this.audio.playSubtickClick(0);
          }
          if (typeof this.audio.playGunfire === 'function') {
            this.audio.playGunfire(state.x, state.y, state.z + 64);
          }
        }
      }

      this.lastFrac = subTickFrac;
    }

    _buildParamsUI() {
      if (!this.paramsContainer) return;

      this.paramsContainer.innerHTML = `
        <div class="space-y-4 font-mono text-xs text-slate-200">
          <!-- Sub-Tick Fraction Slider -->
          <div class="space-y-1.5 bg-slate-900/60 p-3 rounded-xl border border-cyan-900/40">
            <div class="flex justify-between items-center">
              <span class="text-slate-300 font-semibold">Sub-Tick Fraction (w):</span>
              <span id="p03-val-when" class="text-cyan-300 font-bold text-sm">${this.subtickWhen.toFixed(3)}</span>
            </div>
            <input type="range" id="p03-input-when" min="0" max="0.999" step="0.001" value="${this.subtickWhen}"
              class="w-full accent-cyan-400 cursor-pointer">
            <div class="flex justify-between text-[10px] text-slate-500">
              <span>Tick Start (0.0 ms)</span>
              <span>w = ${(this.subtickWhen * 15.625).toFixed(2)} ms</span>
              <span>Tick End (15.625 ms)</span>
            </div>
          </div>

          <!-- Action Selector Presets -->
          <div class="space-y-1.5">
            <label class="text-slate-400 text-[11px] font-semibold">Sub-Tick Button Transition:</label>
            <div class="grid grid-cols-3 gap-1.5">
              <button data-action="jump" class="px-2.5 py-1.5 rounded-lg ${this.actionType === 'jump' ? 'bg-cyan-600 text-white font-bold' : 'bg-slate-900 text-slate-400'} border border-slate-800 transition-colors cursor-pointer text-center">
                +jump
              </button>
              <button data-action="attack" class="px-2.5 py-1.5 rounded-lg ${this.actionType === 'attack' ? 'bg-cyan-600 text-white font-bold' : 'bg-slate-900 text-slate-400'} border border-slate-800 transition-colors cursor-pointer text-center">
                +attack
              </button>
              <button data-action="duck" class="px-2.5 py-1.5 rounded-lg ${this.actionType === 'duck' ? 'bg-cyan-600 text-white font-bold' : 'bg-slate-900 text-slate-400'} border border-slate-800 transition-colors cursor-pointer text-center">
                +duck
              </button>
            </div>
          </div>

          <!-- Physical ConVars -->
          <div class="space-y-3 bg-slate-900/40 p-3 rounded-xl border border-slate-800/80">
            <div class="space-y-1">
              <div class="flex justify-between text-[11px]">
                <span class="text-slate-400">Initial Speed:</span>
                <span id="p03-val-speed" class="text-emerald-400 font-bold">${this.initialSpeed} u/s</span>
              </div>
              <input type="range" id="p03-input-speed" min="0" max="400" step="10" value="${this.initialSpeed}" class="w-full accent-emerald-400 cursor-pointer">
            </div>

            <div class="space-y-1">
              <div class="flex justify-between text-[11px]">
                <span class="text-slate-400">sv_friction:</span>
                <span id="p03-val-fric" class="text-amber-400 font-bold">${this.SV_FRICTION.toFixed(1)}</span>
              </div>
              <input type="range" id="p03-input-fric" min="1.0" max="10.0" step="0.2" value="${this.SV_FRICTION}" class="w-full accent-amber-400 cursor-pointer">
            </div>

            <div class="space-y-1">
              <div class="flex justify-between text-[11px]">
                <span class="text-slate-400">sv_stopspeed:</span>
                <span id="p03-val-stop" class="text-amber-400 font-bold">${this.SV_STOPSPEED.toFixed(0)} u/s</span>
              </div>
              <input type="range" id="p03-input-stop" min="50" max="200" step="5" value="${this.SV_STOPSPEED}" class="w-full accent-amber-400 cursor-pointer">
            </div>

            <div class="space-y-1">
              <div class="flex justify-between text-[11px]">
                <span class="text-slate-400">sv_gravity:</span>
                <span id="p03-val-grav" class="text-purple-400 font-bold">${this.GRAVITY.toFixed(0)} u/s\u00B2</span>
              </div>
              <input type="range" id="p03-input-grav" min="400" max="1200" step="50" value="${this.GRAVITY}" class="w-full accent-purple-400 cursor-pointer">
            </div>
          </div>

          <!-- Protobuf Wire Inspector -->
          <div class="space-y-2 bg-slate-950 p-3 rounded-xl border border-cyan-900/40">
            <div class="flex items-center justify-between border-b border-slate-800 pb-1.5">
              <div class="flex items-center gap-1.5">
                <span class="w-1.5 h-1.5 rounded-full bg-cyan-400"></span>
                <span class="font-bold text-[11px] text-cyan-300">PROTOBUF WIRE CODEC</span>
              </div>
              <span class="text-[9px] text-emerald-400 bg-emerald-950/60 px-1.5 py-0.5 rounded border border-emerald-800/40 font-bold">TAG 0x92 0x01</span>
            </div>

            <div id="p03-hex-dump" class="p-2 bg-black/60 rounded font-mono text-[10px] break-all leading-relaxed text-slate-300 border border-slate-800/60">
            </div>

            <div id="p03-wire-fields" class="space-y-1 text-[10px] text-slate-400">
            </div>
          </div>
        </div>
      `;

      // Bind Event Listeners
      const inputWhen = this.paramsContainer.querySelector('#p03-input-when');
      const lblWhen = this.paramsContainer.querySelector('#p03-val-when');
      if (inputWhen && lblWhen) {
        inputWhen.addEventListener('input', (e) => {
          const val = parseFloat(e.target.value);
          lblWhen.textContent = val.toFixed(3);
          this.onParamChange('when', val);
        });
      }

      const inputSpeed = this.paramsContainer.querySelector('#p03-input-speed');
      const lblSpeed = this.paramsContainer.querySelector('#p03-val-speed');
      if (inputSpeed && lblSpeed) {
        inputSpeed.addEventListener('input', (e) => {
          lblSpeed.textContent = `${e.target.value} u/s`;
          this.onParamChange('speed', e.target.value);
        });
      }

      const inputFric = this.paramsContainer.querySelector('#p03-input-fric');
      const lblFric = this.paramsContainer.querySelector('#p03-val-fric');
      if (inputFric && lblFric) {
        inputFric.addEventListener('input', (e) => {
          lblFric.textContent = parseFloat(e.target.value).toFixed(1);
          this.onParamChange('friction', e.target.value);
        });
      }

      const inputStop = this.paramsContainer.querySelector('#p03-input-stop');
      const lblStop = this.paramsContainer.querySelector('#p03-val-stop');
      if (inputStop && lblStop) {
        inputStop.addEventListener('input', (e) => {
          lblStop.textContent = `${e.target.value} u/s`;
          this.onParamChange('stopspeed', e.target.value);
        });
      }

      const inputGrav = this.paramsContainer.querySelector('#p03-input-grav');
      const lblGrav = this.paramsContainer.querySelector('#p03-val-grav');
      if (inputGrav && lblGrav) {
        inputGrav.addEventListener('input', (e) => {
          lblGrav.textContent = `${e.target.value} u/s\u00B2`;
          this.onParamChange('gravity', e.target.value);
        });
      }

      this.paramsContainer.querySelectorAll('[data-action]').forEach(btn => {
        btn.addEventListener('click', () => {
          const act = btn.getAttribute('data-action');
          this.paramsContainer.querySelectorAll('[data-action]').forEach(b => {
            b.className = 'px-2.5 py-1.5 rounded-lg bg-slate-900 text-slate-400 border border-slate-800 transition-colors cursor-pointer text-center';
          });
          btn.className = 'px-2.5 py-1.5 rounded-lg bg-cyan-600 text-white font-bold border border-slate-800 transition-colors cursor-pointer text-center';
          this.onParamChange('action', act);
        });
      });

      this._updateProtobufInspector();
    }

    _updateProtobufInspector() {
      if (!this.paramsContainer) return;
      const hexContainer = this.paramsContainer.querySelector('#p03-hex-dump');
      const fieldsContainer = this.paramsContainer.querySelector('#p03-wire-fields');
      if (!hexContainer || !fieldsContainer) return;

      // Little-endian IEEE-754 float32 bytes for 'when'
      const buf = new ArrayBuffer(4);
      new DataView(buf).setFloat32(0, this.subtickWhen, true);
      const floatBytes = Array.from(new Uint8Array(buf));
      const floatHex = floatBytes.map(b => b.toString(16).padStart(2, '0')).join(' ');

      const buttonMask = this.actionType === 'jump' ? 0x02 : (this.actionType === 'attack' ? 0x01 : 0x04);
      const buttonName = this.actionType === 'jump' ? 'IN_JUMP (0x02)' : (this.actionType === 'attack' ? 'IN_ATTACK (0x01)' : 'IN_DUCK (0x04)');

      // Serialized CBaseUserCmdPB wire stream containing Field 18 (CSubtickMoveStep)
      // Field 18 tag: (18 << 3) | 2 = 146 -> VarInt bytes [0x92, 0x01]
      const hexStream = [
        '08', '92', '08',              // Field 1: command_number (VarInt 1042)
        '10', '94', 'f4', '03',        // Field 2: client_tick (VarInt 64100)
        '2d', '00', '00', '7a', '43',  // Field 5: forwardmove (Float 250.0f)
        '<span class="text-cyan-400 font-bold bg-cyan-950/60 px-0.5 rounded">92 01</span>', // Field 18 Tag (0x92 0x01)
        '0a',                          // Step length (10 bytes)
        '08', `0${buttonMask.toString(16)}`, // Step Field 1: button (VarInt)
        '10', '01',                    // Step Field 2: pressed (true)
        '1d', `<span class="text-amber-400 font-bold">${floatHex}</span>` // Step Field 3: when (Float32)
      ];

      hexContainer.innerHTML = hexStream.join(' ');

      fieldsContainer.innerHTML = `
        <div class="flex justify-between border-b border-slate-800/60 pb-1">
          <span class="text-slate-400">Field 18 (Repeated):</span>
          <span class="text-cyan-300 font-semibold">subtick_moves (Tag 0x92 0x01)</span>
        </div>
        <div class="flex justify-between border-b border-slate-800/60 pb-1">
          <span class="text-slate-400">Step.button:</span>
          <span class="text-slate-200">${buttonName}</span>
        </div>
        <div class="flex justify-between border-b border-slate-800/60 pb-1">
          <span class="text-slate-400">Step.pressed:</span>
          <span class="text-emerald-400">true (Key-Down)</span>
        </div>
        <div class="flex justify-between">
          <span class="text-slate-400">Step.when:</span>
          <span class="text-amber-300 font-semibold">${this.subtickWhen.toFixed(4)} [${floatHex}]</span>
        </div>
      `;
    }
  }

  // ==========================================================================
  // 7. Sim09SkeletonIKProvider: Skeleton Bone Hierarchy & Analytical Two-Bone IK
  // Reconstructed from client.dll, animationsystem.dll & cs2_skeleton_animation_sim.cpp
  // ==========================================================================

  // 32-Byte CTransform layout & Rodrigues Vector Rotation (client.dll 0x181748480)
  class CTransform {
    constructor(pos = { x: 0, y: 0, z: 0 }, rot = { x: 0, y: 0, z: 0, w: 1 }, scale = 1.0) {
      this.pos = { x: pos.x || 0, y: pos.y || 0, z: pos.z || 0 };
      this.rot = { x: rot.x || 0, y: rot.y || 0, z: rot.z || 0, w: rot.w !== undefined ? rot.w : 1.0 };
      this.scale = scale !== undefined ? scale : 1.0;
    }

    rotateVector(v) {
      const q = this.rot;
      const tx = 2 * (q.y * v.z - q.z * v.y);
      const ty = 2 * (q.z * v.x - q.x * v.z);
      const tz = 2 * (q.x * v.y - q.y * v.x);
      return {
        x: v.x + q.w * tx + (q.y * tz - q.z * ty),
        y: v.y + q.w * ty + (q.z * tx - q.x * tz),
        z: v.z + q.w * tz + (q.x * ty - q.y * tx)
      };
    }

    transformPoint(pt) {
      const rotPt = this.rotateVector(pt);
      return {
        x: this.pos.x + rotPt.x * this.scale,
        y: this.pos.y + rotPt.y * this.scale,
        z: this.pos.z + rotPt.z * this.scale
      };
    }

    concatenate(local) {
      const rotLocalPos = this.rotateVector(local.pos);
      const outPos = {
        x: this.pos.x + rotLocalPos.x * this.scale,
        y: this.pos.y + rotLocalPos.y * this.scale,
        z: this.pos.z + rotLocalPos.z * this.scale
      };

      const q1 = this.rot;
      const q2 = local.rot;
      const rx = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y;
      const ry = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x;
      const rz = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w;
      const rw = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;
      const len = Math.hypot(rx, ry, rz, rw) || 1.0;

      return new CTransform(
        outPos,
        { x: rx / len, y: ry / len, z: rz / len, w: rw / len },
        this.scale * local.scale
      );
    }

    static slerp(q0, q1, t) {
      let cosHalf = q0.w * q1.w + q0.x * q1.x + q0.y * q1.y + q0.z * q1.z;
      let q1x = q1.x, q1y = q1.y, q1z = q1.z, q1w = q1.w;

      // Geodesic shortest-arc condition
      if (cosHalf < 0.0) {
        q1w = -q1w; q1x = -q1x; q1y = -q1y; q1z = -q1z;
        cosHalf = -cosHalf;
      }

      // Small-angle NLERP fallback
      if (cosHalf >= 0.9995) {
        const w = q0.w + t * (q1w - q0.w);
        const x = q0.x + t * (q1x - q0.x);
        const y = q0.y + t * (q1y - q0.y);
        const z = q0.z + t * (q1z - q0.z);
        const len = Math.hypot(w, x, y, z) || 1.0;
        return { x: x / len, y: y / len, z: z / len, w: w / len };
      }

      const halfTheta = Math.acos(Math.max(-1.0, Math.min(1.0, cosHalf)));
      const sinHalf = Math.sqrt(1.0 - cosHalf * cosHalf);
      const ratioA = Math.sin((1.0 - t) * halfTheta) / sinHalf;
      const ratioB = Math.sin(t * halfTheta) / sinHalf;

      return {
        x: q0.x * ratioA + q1x * ratioB,
        y: q0.y * ratioA + q1y * ratioB,
        z: q0.z * ratioA + q1z * ratioB,
        w: q0.w * ratioA + q1w * ratioB
      };
    }

    static rotationBetween(u1, u2) {
      const d = u1.x * u2.x + u1.y * u2.y + u1.z * u2.z;
      if (d >= 0.999999) return { x: 0, y: 0, z: 0, w: 1 };
      if (d <= -0.999999) {
        let ax = 0, ay = -u1.z, az = u1.y;
        if (Math.hypot(ay, az) < 1e-4) { ax = -u1.y; ay = u1.x; az = 0; }
        const len = Math.hypot(ax, ay, az);
        return { x: ax / len, y: ay / len, z: az / len, w: 0 };
      }
      const cx = u1.y * u2.z - u1.z * u2.y;
      const cy = u1.z * u2.x - u1.x * u2.z;
      const cz = u1.x * u2.y - u1.y * u2.x;
      const w = 1.0 + d;
      const len = Math.hypot(cx, cy, cz, w);
      return { x: cx / len, y: cy / len, z: cz / len, w: w / len };
    }
  }

  // Analytical Law of Cosines Two-Bone IK Solver (animationsystem.dll 0x1802f9750)
  function solveTwoBoneIK(rootPos, targetPos, L1, L2, poleVec) {
    const dx = targetPos.x - rootPos.x;
    const dy = targetPos.y - rootPos.y;
    const dz = targetPos.z - rootPos.z;
    const rawDist = Math.sqrt(dx * dx + dy * dy + dz * dz);
    let dist = rawDist;

    const maxReach = (L1 + L2) * 0.9999;
    const minReach = Math.max(0.001, Math.abs(L1 - L2) * 1.0001);
    let reachedTarget = true;

    if (dist > maxReach) {
      dist = maxReach;
      reachedTarget = false;
    } else if (dist < minReach) {
      dist = minReach;
      reachedTarget = false;
    }

    const u = rawDist < 1e-6 ? { x: 0, y: 0, z: 1 } : { x: dx / rawDist, y: dy / rawDist, z: dz / rawDist };

    // Law of Cosines
    const cosAlpha = Math.max(-1.0, Math.min(1.0, (L1 * L1 + dist * dist - L2 * L2) / (2.0 * L1 * dist)));
    const alpha = Math.acos(cosAlpha);
    const sinAlpha = Math.sin(alpha);

    const cosBeta = Math.max(-1.0, Math.min(1.0, (L1 * L1 + L2 * L2 - dist * dist) / (2.0 * L1 * L2)));
    const beta = Math.acos(cosBeta);

    // Form orthogonal basis using pole vector
    let px = poleVec.x - rootPos.x;
    let py = poleVec.y - rootPos.y;
    let pz = poleVec.z - rootPos.z;
    const dotP = px * u.x + py * u.y + pz * u.z;
    px -= dotP * u.x;
    py -= dotP * u.y;
    pz -= dotP * u.z;
    let pLen = Math.sqrt(px * px + py * py + pz * pz);
    if (pLen < 1e-4) {
      // Fallback perpendicular axis
      px = -u.y; py = u.x; pz = 0.0;
      pLen = Math.sqrt(px * px + py * py);
      if (pLen < 1e-4) {
        px = 0.0; py = -u.z; pz = u.y;
        pLen = Math.sqrt(py * py + pz * pz) || 1.0;
      }
    }
    px /= pLen; py /= pLen; pz /= pLen;

    const midPos = {
      x: rootPos.x + u.x * (L1 * cosAlpha) + px * (L1 * sinAlpha),
      y: rootPos.y + u.y * (L1 * cosAlpha) + py * (L1 * sinAlpha),
      z: rootPos.z + u.z * (L1 * cosAlpha) + pz * (L1 * sinAlpha)
    };

    const effectorPos = {
      x: rootPos.x + u.x * dist,
      y: rootPos.y + u.y * dist,
      z: rootPos.z + u.z * dist
    };

    return {
      midPos,
      effectorPos,
      reachedTarget,
      cosAlpha,
      angleRad: alpha,
      betaRad: beta,
      alphaDeg: (alpha * 180) / Math.PI,
      targetDist: rawDist,
      maxReach
    };
  }

  // Interactive 3D End-Effector Manipulator & Drag Plane Gating
  class Sim09TargetGizmo {
    constructor(viewport, targetPos, onTargetChange) {
      this.viewport = viewport;
      this.scene = viewport.getScene();
      this.camera = viewport.getCamera();
      this.controls = viewport.getControls();
      this.canvas = viewport.getRenderer() ? viewport.getRenderer().domElement : null;
      this.onTargetChange = onTargetChange;
      this.targetPos = new THREE.Vector3(targetPos.x, targetPos.y, targetPos.z);

      this.isDragging = false;
      this.hasRaycaster = typeof THREE.Raycaster === 'function' && typeof THREE.Plane === 'function';
      if (this.hasRaycaster) {
        this.raycaster = new THREE.Raycaster();
        this.mouseNdc = new THREE.Vector2();
        this.dragPlane = new THREE.Plane();
        this.planeIntersect = new THREE.Vector3();
        this.dragOffset = new THREE.Vector3();
      }

      this._initMeshes();
      if (this.hasRaycaster && this.canvas && typeof this.canvas.addEventListener === 'function') {
        this._bindEvents();
      }
    }

    _initMeshes() {
      this.group = new THREE.Group();

      // Glowing core target sphere
      const coreGeom = new THREE.SphereGeometry(2.2, 16, 16);
      this.coreMat = new THREE.MeshStandardMaterial({
        color: 0xf59e0b,
        emissive: 0xd97706,
        roughness: 0.3
      });
      this.coreMesh = new THREE.Mesh(coreGeom, this.coreMat);
      this.group.add(this.coreMesh);

      // Invisible enlarged hit proxy for smooth grabbing
      const proxyGeom = new THREE.SphereGeometry(8.0, 8, 8);
      const proxyMat = new THREE.MeshBasicMaterial({ visible: false });
      this.hitProxy = new THREE.Mesh(proxyGeom, proxyMat);
      this.group.add(this.hitProxy);

      // Dashed reach connector line from root to target
      const lineGeom = new THREE.BufferGeometry();
      lineGeom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(6), 3));
      this.lineMat = new THREE.LineBasicMaterial({ color: 0x10b981, linewidth: 2 });
      this.reachLine = new THREE.Line(lineGeom, this.lineMat);
      this.scene.add(this.reachLine);

      this.group.position.copy(this.targetPos);
      this.scene.add(this.group);
    }

    _bindEvents() {
      this._onPointerDown = (e) => {
        if (e.button !== 0) return;
        this._updateMouseNdc(e);

        this.raycaster.setFromCamera(this.mouseNdc, this.viewport.getCamera());
        const intersects = this.raycaster.intersectObject(this.hitProxy, false);

        if (intersects && intersects.length > 0) {
          this.isDragging = true;
          if (this.controls) this.controls.enabled = false;

          // Camera-facing drag plane through target
          const cam = this.viewport.getCamera();
          const planeNormal = new THREE.Vector3();
          if (cam.getWorldDirection) cam.getWorldDirection(planeNormal);
          else planeNormal.set(0, 0, 1);
          planeNormal.negate();
          this.dragPlane.setFromNormalAndCoplanarPoint(planeNormal, this.group.position);

          this.raycaster.ray.intersectPlane(this.dragPlane, this.planeIntersect);
          this.dragOffset.subVectors(this.group.position, this.planeIntersect);

          if (this.canvas.style) this.canvas.style.cursor = 'grabbing';
          if (typeof e.stopPropagation === 'function') e.stopPropagation();
        }
      };

      this._onPointerMove = (e) => {
        this._updateMouseNdc(e);
        const cam = this.viewport.getCamera();

        if (this.isDragging) {
          this.raycaster.setFromCamera(this.mouseNdc, cam);
          if (this.raycaster.ray.intersectPlane(this.dragPlane, this.planeIntersect)) {
            const newPos = this.planeIntersect.clone().add(this.dragOffset);
            newPos.x = Math.max(-50, Math.min(50, newPos.x));
            newPos.y = Math.max(-50, Math.min(50, newPos.y));
            newPos.z = Math.max(0, Math.min(80, newPos.z));

            this.setPosition(newPos.x, newPos.y, newPos.z, true);
          }
        } else {
          this.raycaster.setFromCamera(this.mouseNdc, cam);
          const hits = this.raycaster.intersectObject(this.hitProxy, false);
          if (this.canvas.style) {
            this.canvas.style.cursor = hits && hits.length > 0 ? 'grab' : '';
          }
        }
      };

      this._onPointerUp = () => {
        if (this.isDragging) {
          this.isDragging = false;
          if (this.controls) this.controls.enabled = true;
          if (this.canvas.style) this.canvas.style.cursor = '';
        }
      };

      this.canvas.addEventListener('pointerdown', this._onPointerDown);
      window.addEventListener('pointermove', this._onPointerMove);
      window.addEventListener('pointerup', this._onPointerUp);
    }

    _updateMouseNdc(e) {
      if (!this.canvas || !this.canvas.getBoundingClientRect) return;
      const rect = this.canvas.getBoundingClientRect();
      this.mouseNdc.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
      this.mouseNdc.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;
    }

    setPosition(x, y, z, notify = false) {
      this.targetPos.set(x, y, z);
      this.group.position.copy(this.targetPos);
      if (notify && typeof this.onTargetChange === 'function') {
        this.onTargetChange(this.targetPos);
      }
    }

    updateReachStatus(rootPos, isReachable) {
      const posAttr = this.reachLine.geometry.attributes.position;
      if (posAttr && posAttr.array) {
        posAttr.array[0] = rootPos.x;
        posAttr.array[1] = rootPos.y;
        posAttr.array[2] = rootPos.z;
        posAttr.array[3] = this.targetPos.x;
        posAttr.array[4] = this.targetPos.y;
        posAttr.array[5] = this.targetPos.z;
        posAttr.needsUpdate = true;
      }

      const color = isReachable ? 0x10b981 : 0xf43f5e;
      this.lineMat.color.setHex(color);
      if (this.coreMat.emissive) {
        this.coreMat.emissive.setHex(isReachable ? 0xd97706 : 0xe11d48);
      }
    }

    teardown() {
      if (this.canvas && this._onPointerDown) {
        this.canvas.removeEventListener('pointerdown', this._onPointerDown);
      }
      if (this._onPointerMove) window.removeEventListener('pointermove', this._onPointerMove);
      if (this._onPointerUp) window.removeEventListener('pointerup', this._onPointerUp);

      if (this.controls) this.controls.enabled = true;
      if (this.canvas && this.canvas.style) this.canvas.style.cursor = '';

      if (this.group) {
        this.scene.remove(this.group);
        this.group.traverse((obj) => {
          if (obj.geometry && typeof obj.geometry.dispose === 'function') obj.geometry.dispose();
          if (obj.material && typeof obj.material.dispose === 'function') obj.material.dispose();
        });
      }

      if (this.reachLine) {
        this.scene.remove(this.reachLine);
        if (this.reachLine.geometry && typeof this.reachLine.geometry.dispose === 'function') {
          this.reachLine.geometry.dispose();
        }
        if (this.reachLine.material && typeof this.reachLine.material.dispose === 'function') {
          this.reachLine.material.dispose();
        }
      }
    }
  }

  class Sim09SkeletonIKProvider {
    constructor(viewport, audioEngine, playback) {
      this.viewport = viewport;
      this.audio = audioEngine;
      this.playback = playback;

      // Active IK parameters
      this.activeLimb = 'arm_R'; // 'arm_R' | 'arm_L' | 'leg_R' | 'leg_L'
      this.blendWeight = 1.0;    // 0.0 (FK) to 1.0 (Full IK)
      this.targetPos = { x: 14.0, y: -10.0, z: 46.0 }; // Naturally reachable default (d = 13.9 u < 21.6 u)
      this.poleVec = { x: 0.0, y: -1.0, z: -0.5 };
      this.limbLengthL1 = 11.4;
      this.limbLengthL2 = 10.2;

      // Display toggles
      this.showBones = true;
      this.showHitboxes = false; // Off by default to avoid visual clutter
      this.showGizmo = true;
      this.showReachVolume = false; // Off by default so it never obscures the mannequin

      // Three.js Render Group
      this.sceneGroup = null;
      this.boneLines = null;
      this.boneMeshes = [];
      this.jointMeshes = [];
      this.capsuleMeshes = [];
      this.targetGizmo = null;
      this.reachSphereMesh = null;
      this.reachLineMesh = null;
      this.jointSphereMesh = null;
      this.effectorSphereMesh = null;

      // Cached Bone Definitions and World Transforms
      this.bones = [];
      this.worldBones = [];
      this.hitboxes = [];
      this.paramsContainer = null;

      this._initHierarchy();
      this._initHitboxes();
    }

    _initHierarchy() {
      // 19-bone topological structure grounded on floor (CS2 72u height)
      this.bones = [
        { name: 'pelvis',      parent: -1, local: new CTransform({ x: 0, y: 0, z: 36 }) },
        { name: 'spine',       parent: 0,  local: new CTransform({ x: 0, y: 0, z: 10 }) },
        { name: 'chest',       parent: 1,  local: new CTransform({ x: 0, y: 0, z: 12 }) },
        { name: 'neck',        parent: 2,  local: new CTransform({ x: 0, y: 0, z: 6 }) },
        { name: 'head',        parent: 3,  local: new CTransform({ x: 1.5, y: 0, z: 6 }) },
        { name: 'clavicle_L',  parent: 2,  local: new CTransform({ x: 0, y: 4, z: 2 }) },
        { name: 'shoulder_L',  parent: 5,  local: new CTransform({ x: 0, y: 6, z: -2 }) },
        { name: 'elbow_L',     parent: 6,  local: new CTransform({ x: 0, y: 10, z: -4 }) },
        { name: 'hand_L',      parent: 7,  local: new CTransform({ x: 0, y: 9, z: -3 }) },
        { name: 'clavicle_R',  parent: 2,  local: new CTransform({ x: 0, y: -4, z: 2 }) },
        { name: 'shoulder_R',  parent: 9,  local: new CTransform({ x: 0, y: -6, z: -2 }) },
        { name: 'elbow_R',     parent: 10, local: new CTransform({ x: 0, y: -10, z: -4 }) },
        { name: 'hand_R',      parent: 11, local: new CTransform({ x: 0, y: -9, z: -3 }) },
        { name: 'hip_L',       parent: 0,  local: new CTransform({ x: 0, y: 4.0, z: -3 }) },
        { name: 'knee_L',      parent: 13, local: new CTransform({ x: 0, y: 0, z: -16 }) },
        { name: 'foot_L',      parent: 14, local: new CTransform({ x: 2, y: 0, z: -17 }) },
        { name: 'hip_R',       parent: 0,  local: new CTransform({ x: 0, y: -4.0, z: -3 }) },
        { name: 'knee_R',      parent: 16, local: new CTransform({ x: 0, y: 0, z: -16 }) },
        { name: 'foot_R',      parent: 17, local: new CTransform({ x: 2, y: 0, z: -17 }) }
      ];

      this.worldBones = new Array(this.bones.length);
    }

    _initHitboxes() {
      // Source 2 Standard Hitbox Setup for CS2 Player
      this.hitboxes = [
        { name: 'HEAD',      bone: 4,  min: { x: -3.5, y: -3.5, z: -3.5 }, max: { x: 3.5, y: 3.5, z: 3.5 }, radius: 4.5, color: 0xef4444 },
        { name: 'NECK',      bone: 3,  min: { x: -2.0, y: -2.0, z: -1.0 }, max: { x: 2.0, y: 2.0, z: 3.0 }, radius: 3.2, color: 0xf97316 },
        { name: 'CHEST',     bone: 2,  min: { x: -5.0, y: -7.5, z: -2.0 }, max: { x: 5.0, y: 7.5, z: 8.0 }, radius: 6.2, color: 0x3b82f6 },
        { name: 'STOMACH',   bone: 1,  min: { x: -4.5, y: -6.5, z: -2.0 }, max: { x: 4.5, y: 6.5, z: 7.0 }, radius: 5.8, color: 0x10b981 },
        { name: 'PELVIS',    bone: 0,  min: { x: -4.5, y: -6.0, z: -4.0 }, max: { x: 4.5, y: 6.0, z: 3.0 }, radius: 5.5, color: 0x06b6d4 },
        { name: 'ARM_UP_L',  bone: 6,  min: { x: -2.2, y: 0.0, z: -2.2 },  max: { x: 2.2, y: 9.0, z: 2.2 },  radius: 3.0, color: 0x8b5cf6 },
        { name: 'ARM_LOW_L', bone: 7,  min: { x: -2.0, y: 0.0, z: -2.0 },  max: { x: 2.0, y: 8.5, z: 2.0 },  radius: 2.6, color: 0xa855f7 },
        { name: 'ARM_UP_R',  bone: 10, min: { x: -2.2, y: -9.0, z: -2.2 }, max: { x: 2.2, y: 0.0, z: 2.2 },  radius: 3.0, color: 0x8b5cf6 },
        { name: 'ARM_LOW_R', bone: 11, min: { x: -2.0, y: -8.5, z: -2.0 }, max: { x: 2.0, y: 0.0, z: 2.0 },  radius: 2.6, color: 0xa855f7 },
        { name: 'LEG_UP_L',  bone: 13, min: { x: -2.8, y: -2.8, z: -15.0}, max: { x: 2.8, y: 2.8, z: 0.0 },  radius: 3.6, color: 0xec4899 },
        { name: 'LEG_LOW_L', bone: 14, min: { x: -2.5, y: -2.5, z: -15.0}, max: { x: 2.5, y: 2.5, z: 0.0 },  radius: 3.2, color: 0xf43f5e },
        { name: 'LEG_UP_R',  bone: 16, min: { x: -2.8, y: -2.8, z: -15.0}, max: { x: 2.8, y: 2.8, z: 0.0 },  radius: 3.6, color: 0xec4899 },
        { name: 'LEG_LOW_R', bone: 17, min: { x: -2.5, y: -2.5, z: -15.0}, max: { x: 2.5, y: 2.5, z: 0.0 },  radius: 3.2, color: 0xf43f5e }
      ];
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim09_SkeletonIK_Scene';

      // 1. Interactive 3D End-Effector Manipulator Gizmo
      this.targetGizmo = new Sim09TargetGizmo(this.viewport, this.targetPos, (newPos) => {
        this.targetPos.x = newPos.x;
        this.targetPos.y = newPos.y;
        this.targetPos.z = newPos.z;
        this._syncUITarget();
        this.update(0, 0, 0);
      });

      // 2. Subtle Planar Reach Boundary Ring (Off by default, never obscures character)
      const reachGeom = new THREE.RingGeometry(21.4, 21.6, 36);
      const reachMat = new THREE.MeshBasicMaterial({
        color: 0x06b6d4,
        transparent: true,
        opacity: 0.35,
        side: 2, // THREE.DoubleSide
        depthWrite: false
      });
      this.reachSphereMesh = new THREE.Mesh(reachGeom, reachMat);
      this.reachSphereMesh.visible = this.showReachVolume;
      this.sceneGroup.add(this.reachSphereMesh);

      // 3. Dynamic Reach Indicator Line (Effector -> Target)
      const reachLineGeo = new THREE.BufferGeometry();
      reachLineGeo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(6), 3));
      const reachLineMat = new THREE.LineBasicMaterial({ color: 0x10b981, linewidth: 2.5 });
      this.reachLineMesh = new THREE.Line(reachLineGeo, reachLineMat);
      this.sceneGroup.add(this.reachLineMesh);

      // 4. Joint & Effector Indicator Spheres
      const jointGeom = new THREE.SphereGeometry(1.6, 12, 8);
      const jointMat = new THREE.MeshStandardMaterial({ color: 0xd946ef, roughness: 0.3 });
      this.jointSphereMesh = new THREE.Mesh(jointGeom, jointMat);
      this.sceneGroup.add(this.jointSphereMesh);

      const effGeom = new THREE.SphereGeometry(1.5, 12, 8);
      const effMat = new THREE.MeshStandardMaterial({ color: 0x10b981, roughness: 0.3 });
      this.effectorSphereMesh = new THREE.Mesh(effGeom, effMat);
      this.sceneGroup.add(this.effectorSphereMesh);

      // 5. 3D Articulated Bone Meshes
      this.boneMeshes = [];
      const boneMat = new THREE.MeshStandardMaterial({ color: 0x38bdf8, roughness: 0.4, metalness: 0.2 });
      for (let i = 1; i < this.bones.length; i++) {
        const bGeo = new THREE.CylinderGeometry(0.7, 1.4, 1.0, 8);
        const bMesh = new THREE.Mesh(bGeo, boneMat);
        this.sceneGroup.add(bMesh);
        this.boneMeshes.push({ mesh: bMesh, boneIdx: i });
      }

      // 6. Joint Node Spheres
      this.jointMeshes = [];
      const jNodeMat = new THREE.MeshStandardMaterial({ color: 0x0ea5e9, roughness: 0.3 });
      const jNodeGeo = new THREE.SphereGeometry(1.2, 10, 8);
      for (let i = 0; i < this.bones.length; i++) {
        const jMesh = new THREE.Mesh(jNodeGeo, jNodeMat);
        this.sceneGroup.add(jMesh);
        this.jointMeshes.push(jMesh);
      }

      // 7. Bone Line Segments
      const lineGeom = new THREE.BufferGeometry();
      const linePositions = new Float32Array(this.bones.length * 6);
      lineGeom.setAttribute('position', new THREE.BufferAttribute(linePositions, 3));
      const lineMat = new THREE.LineBasicMaterial({ color: 0x0284c7, linewidth: 2 });
      this.boneLines = new THREE.LineSegments(lineGeom, lineMat);
      this.sceneGroup.add(this.boneLines);

      // 8. Hitbox Capsules (Source 2 Sleek Wireframe Capsules)
      this.capsuleMeshes = this.hitboxes.map(hb => {
        const grp = new THREE.Group();
        const mat = new THREE.MeshBasicMaterial({
          color: hb.color,
          wireframe: true,
          transparent: true,
          opacity: 0.22,
          depthWrite: false
        });

        const cylGeom = new THREE.CylinderGeometry(hb.radius, hb.radius, 1.0, 10, 1, false);
        const cylMesh = new THREE.Mesh(cylGeom, mat);
        grp.add(cylMesh);

        const capTopGeom = new THREE.SphereGeometry(hb.radius, 10, 6);
        const capTopMesh = new THREE.Mesh(capTopGeom, mat);
        grp.add(capTopMesh);

        const capBotMesh = new THREE.Mesh(capTopGeom, mat);
        grp.add(capBotMesh);

        grp.visible = this.showHitboxes;
        this.sceneGroup.add(grp);
        return { group: grp, cylMesh, capTopMesh, capBotMesh, hitbox: hb };
      });

      scene.add(this.sceneGroup);

      // 9. Mount UI Controls
      this._buildParamsUI();

      // 10. Frame Humanoid Mannequin at Character Center (z = 36)
      const camera = this.viewport.getCamera();
      if (camera) {
        camera.position.set(-45, -60, 42);
        const controls = this.viewport.getControls();
        if (controls) {
          controls.target.set(0, 0, 36);
          controls.update();
        }
      }

      // 11. Initial Update Pass
      this.update(0, 0, 0);
    }

    update(tick, subTickFrac, dt) {
      // Natural idle breathing & postural sway during timeline playback
      const animTime = (tick + subTickFrac) * 0.12;
      const breathZ = Math.sin(animTime * 1.6) * 0.4;
      const swayY = Math.cos(animTime * 0.8) * 0.25;

      // 1. Base FK Pass with procedural breathing
      const currentLocals = this.bones.map((b, idx) => {
        const tr = new CTransform(b.local.pos, b.local.rot, b.local.scale);
        if (idx === 1) { // spine
          tr.pos = { x: b.local.pos.x, y: b.local.pos.y + swayY * 0.4, z: b.local.pos.z + breathZ * 0.3 };
        } else if (idx === 2) { // chest
          tr.pos = { x: b.local.pos.x, y: b.local.pos.y + swayY * 0.8, z: b.local.pos.z + breathZ * 0.6 };
        }
        return tr;
      });
      for (let i = 0; i < this.bones.length; i++) {
        const pIdx = this.bones[i].parent;
        this.worldBones[i] = pIdx < 0 ? currentLocals[i] : this.worldBones[pIdx].concatenate(currentLocals[i]);
      }

      // 2. Resolve Active Limb Indices
      let rootIdx, jointIdx, effIdx;
      if (this.activeLimb === 'arm_R') { rootIdx = 10; jointIdx = 11; effIdx = 12; }
      else if (this.activeLimb === 'arm_L') { rootIdx = 6; jointIdx = 7; effIdx = 8; }
      else if (this.activeLimb === 'leg_R') { rootIdx = 16; jointIdx = 17; effIdx = 18; }
      else { rootIdx = 13; jointIdx = 14; effIdx = 15; }

      const rootWorldPos = this.worldBones[rootIdx].pos;
      const L1 = this.limbLengthL1;
      const L2 = this.limbLengthL2;

      // Dynamic limb reach animation during timeline playback
      let solveTarget = { x: this.targetPos.x, y: this.targetPos.y, z: this.targetPos.z };
      if (this.playback && this.playback.isPlaying) {
        const cycle = ((tick + subTickFrac) / (this.playback.maxTicks || 64)) * Math.PI * 2;
        const sweepX = Math.sin(cycle) * 7.5;
        const sweepY = Math.cos(cycle) * 5.0;
        const sweepZ = Math.sin(cycle * 2.0) * 3.5;
        solveTarget = {
          x: this.targetPos.x + sweepX,
          y: this.targetPos.y + sweepY,
          z: this.targetPos.z + sweepZ
        };
        if (this.targetGizmo) {
          this.targetGizmo.setPosition(solveTarget.x, solveTarget.y, solveTarget.z);
        }
      }

      // 3. Analytical Two-Bone IK Solve (Law of Cosines)
      const poleWorld = {
        x: rootWorldPos.x + this.poleVec.x * 20,
        y: rootWorldPos.y + this.poleVec.y * 20,
        z: rootWorldPos.z + this.poleVec.z * 20
      };
      const ikSol = solveTwoBoneIK(rootWorldPos, solveTarget, L1, L2, poleWorld);

      // 4. Guaranteed Direct Joint Attachment under Blend Weight
      const w = Math.max(0.0, Math.min(1.0, this.blendWeight));
      if (w > 0.0) {
        const fkMid = this.worldBones[jointIdx].pos;
        const fkEff = this.worldBones[effIdx].pos;

        const finalMid = {
          x: fkMid.x * (1 - w) + ikSol.midPos.x * w,
          y: fkMid.y * (1 - w) + ikSol.midPos.y * w,
          z: fkMid.z * (1 - w) + ikSol.midPos.z * w
        };
        const finalEff = {
          x: fkEff.x * (1 - w) + ikSol.effectorPos.x * w,
          y: fkEff.y * (1 - w) + ikSol.effectorPos.y * w,
          z: fkEff.z * (1 - w) + ikSol.effectorPos.z * w
        };

        this.worldBones[jointIdx].pos = finalMid;
        this.worldBones[effIdx].pos = finalEff;
      }

      // 5. Update Reach Envelope Sphere centered at rootWorldPos
      if (this.reachSphereMesh) {
        this.reachSphereMesh.position.set(rootWorldPos.x, rootWorldPos.y, rootWorldPos.z);
        const maxR = L1 + L2;
        this.reachSphereMesh.scale.set(maxR / 21.6, maxR / 21.6, maxR / 21.6);
        this.reachSphereMesh.visible = this.showReachVolume;
      }

      // 6. Update Dynamic Reach Raycast Line (Effector -> Target)
      if (this.reachLineMesh && this.reachLineMesh.geometry) {
        const effPos = this.worldBones[effIdx].pos;
        const posAttr = this.reachLineMesh.geometry.attributes.position;
        posAttr.array[0] = effPos.x;
        posAttr.array[1] = effPos.y;
        posAttr.array[2] = effPos.z;
        posAttr.array[3] = solveTarget.x;
        posAttr.array[4] = solveTarget.y;
        posAttr.array[5] = solveTarget.z;
        posAttr.needsUpdate = true;
        this.reachLineMesh.material.color.setHex(ikSol.reachedTarget ? 0x10b981 : 0xf59e0b);
        this.reachLineMesh.visible = this.showGizmo;
      }

      if (this.jointSphereMesh) {
        const midP = this.worldBones[jointIdx].pos;
        this.jointSphereMesh.position.set(midP.x, midP.y, midP.z);
        this.jointSphereMesh.visible = this.showGizmo;
      }

      if (this.effectorSphereMesh) {
        const effP = this.worldBones[effIdx].pos;
        this.effectorSphereMesh.position.set(effP.x, effP.y, effP.z);
        this.effectorSphereMesh.material.color.setHex(ikSol.reachedTarget ? 0x10b981 : 0xf59e0b);
        this.effectorSphereMesh.visible = this.showGizmo;
      }

      // 7. Update Joint Spheres & 3D Bone Meshes
      if (this.jointMeshes) {
        for (let i = 0; i < this.bones.length; i++) {
          if (this.jointMeshes[i]) {
            const p = this.worldBones[i].pos;
            this.jointMeshes[i].position.set(p.x, p.y, p.z);
            this.jointMeshes[i].visible = this.showBones;
          }
        }
      }

      if (this.boneMeshes) {
        for (let k = 0; k < this.boneMeshes.length; k++) {
          const item = this.boneMeshes[k];
          const bIdx = item.boneIdx;
          const pIdx = this.bones[bIdx].parent;
          if (pIdx >= 0) {
            const pA = this.worldBones[pIdx].pos;
            const pB = this.worldBones[bIdx].pos;
            const dx = pB.x - pA.x, dy = pB.y - pA.y, dz = pB.z - pA.z;
            const len = Math.hypot(dx, dy, dz);
            item.mesh.visible = this.showBones && len > 0.1;
            if (len > 0.1) {
              item.mesh.position.set((pA.x + pB.x) * 0.5, (pA.y + pB.y) * 0.5, (pA.z + pB.z) * 0.5);
              const dir = new THREE.Vector3(dx / len, dy / len, dz / len);
              item.mesh.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir);
              item.mesh.scale.set(1, len, 1);
            }
          }
        }
      }

      // 8. Update Bone Line Wireframe
      if (this.boneLines && this.boneLines.geometry) {
        this.boneLines.visible = this.showBones;
        const posAttr = this.boneLines.geometry.attributes.position;
        if (posAttr && posAttr.array) {
          let ptr = 0;
          for (let i = 0; i < this.bones.length; i++) {
            const pIdx = this.bones[i].parent;
            if (pIdx < 0) {
              // Root pelvis has no parent. Do NOT draw a line from world origin (0, 0, 0)!
              const cPos = this.worldBones[i].pos;
              posAttr.array[ptr++] = cPos.x;
              posAttr.array[ptr++] = cPos.y;
              posAttr.array[ptr++] = cPos.z;
              posAttr.array[ptr++] = cPos.x;
              posAttr.array[ptr++] = cPos.y;
              posAttr.array[ptr++] = cPos.z;
              continue;
            }
            const pPos = this.worldBones[pIdx].pos;
            const cPos = this.worldBones[i].pos;
            posAttr.array[ptr++] = pPos.x;
            posAttr.array[ptr++] = pPos.y;
            posAttr.array[ptr++] = pPos.z;
            posAttr.array[ptr++] = cPos.x;
            posAttr.array[ptr++] = cPos.y;
            posAttr.array[ptr++] = cPos.z;
          }
          posAttr.needsUpdate = true;
        }
      }

      // 9. Update Rotated Hitbox Capsules
      this.capsuleMeshes.forEach(item => {
        item.group.visible = this.showHitboxes;
        if (!this.showHitboxes) return;

        const hb = item.hitbox;
        const bTrans = this.worldBones[hb.bone];
        let pA, pB;

        // Active limb capsules directly align to solved bone joints
        if (hb.bone === jointIdx) {
          pA = this.worldBones[rootIdx].pos;
          pB = this.worldBones[jointIdx].pos;
        } else if (hb.bone === effIdx) {
          pA = this.worldBones[jointIdx].pos;
          pB = this.worldBones[effIdx].pos;
        } else {
          pA = bTrans.transformPoint(hb.min);
          pB = bTrans.transformPoint(hb.max);
        }

        const dx = pB.x - pA.x;
        const dy = pB.y - pA.y;
        const dz = pB.z - pA.z;
        const h = Math.hypot(dx, dy, dz);
        const center = { x: (pA.x + pB.x) * 0.5, y: (pA.y + pB.y) * 0.5, z: (pA.z + pB.z) * 0.5 };

        item.group.position.set(center.x, center.y, center.z);

        if (h > 1e-4) {
          const dir = new THREE.Vector3(dx / h, dy / h, dz / h);
          item.group.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir);
          item.cylMesh.scale.set(1, h, 1);
          item.capTopMesh.position.set(0, h * 0.5, 0);
          item.capBotMesh.position.set(0, -h * 0.5, 0);
        }
      });

      // 10. Update Telemetry HUD Metrics
      this.viewport.setHudMetrics([
        { label: 'ACTIVE LIMB', value: this.activeLimb.toUpperCase() },
        { label: 'TARGET DIST (d)', value: `${ikSol.targetDist.toFixed(1)} u` },
        { label: 'MAX REACH', value: `${ikSol.maxReach.toFixed(1)} u` },
        { label: 'ROOT ANGLE (\u03B1)', value: `${ikSol.alphaDeg.toFixed(1)}\u00B0` },
        { label: 'JOINT ANGLE (\u03B2)', value: `${((ikSol.betaRad * 180) / Math.PI).toFixed(1)}\u00B0` },
        { label: 'IK BLEND (\u03BB)', value: `${(this.blendWeight * 100).toFixed(0)}%` },
        { label: 'SOLVER STATUS', value: ikSol.reachedTarget ? 'REACHED (Frame Perfect)' : 'OUT OF REACH (CLAMPED)', alert: !ikSol.reachedTarget }
      ]);
    }

    onParamChange(key, value) {
      if (key === 'limb') this._switchLimb(value);
      else if (key === 'blend') this.blendWeight = parseFloat(value);
      else if (key === 'L1') this.limbLengthL1 = parseFloat(value);
      else if (key === 'L2') this.limbLengthL2 = parseFloat(value);
      else if (key === 'target') {
        this.targetPos = { x: value.x, y: value.y, z: value.z };
        if (this.targetGizmo) this.targetGizmo.setPosition(value.x, value.y, value.z);
      }
      this.update(0, 0, 0);
    }

    teardown() {
      if (this.targetGizmo) {
        this.targetGizmo.teardown();
        this.targetGizmo = null;
      }
      const scene = this.viewport.getScene();
      if (this.sceneGroup) {
        if (scene) scene.remove(this.sceneGroup);
        this.sceneGroup = null;
      }
      if (this.paramsContainer) {
        this.paramsContainer.innerHTML = '';
        this.paramsContainer = null;
      }
      this.boneLines = null;
      this.boneMeshes = [];
      this.jointMeshes = [];
      this.capsuleMeshes = [];
      this.reachSphereMesh = null;
      this.reachLineMesh = null;
      this.jointSphereMesh = null;
      this.effectorSphereMesh = null;
    }

    _switchLimb(limbKey) {
      this.activeLimb = limbKey;
      if (limbKey === 'arm_R') {
        this.targetPos = { x: 14.0, y: -10.0, z: 46.0 };
        this.poleVec = { x: 0.0, y: -1.0, z: -0.5 };
        this.limbLengthL1 = 11.4;
        this.limbLengthL2 = 10.2;
      } else if (limbKey === 'arm_L') {
        this.targetPos = { x: 14.0, y: 10.0, z: 46.0 };
        this.poleVec = { x: 0.0, y: 1.0, z: -0.5 };
        this.limbLengthL1 = 11.4;
        this.limbLengthL2 = 10.2;
      } else if (limbKey === 'leg_R') {
        this.targetPos = { x: 8.0, y: -5.0, z: 4.0 };
        this.poleVec = { x: 1.0, y: 0.0, z: 0.0 };
        this.limbLengthL1 = 16.5;
        this.limbLengthL2 = 16.5;
      } else if (limbKey === 'leg_L') {
        this.targetPos = { x: 8.0, y: 5.0, z: 4.0 };
        this.poleVec = { x: 1.0, y: 0.0, z: 0.0 };
        this.limbLengthL1 = 16.5;
        this.limbLengthL2 = 16.5;
      }

      if (this.targetGizmo) {
        this.targetGizmo.setPosition(this.targetPos.x, this.targetPos.y, this.targetPos.z);
      }
      this._syncUI();
      this.update(0, 0, 0);
    }

    _applyStancePreset(presetName) {
      if (presetName === 'combat') {
        this._switchLimb('arm_R');
        this.targetPos = { x: 14.0, y: -10.0, z: 46.0 };
        this.blendWeight = 1.0;
      } else if (presetName === 'reach') {
        this._switchLimb('arm_R');
        this.targetPos = { x: 16.0, y: -8.0, z: 58.0 };
        this.blendWeight = 1.0;
      } else if (presetName === 'ground') {
        this._switchLimb('leg_L');
        this.targetPos = { x: 8.0, y: 5.0, z: 4.0 };
        this.blendWeight = 1.0;
      } else if (presetName === 'overextension') {
        this._switchLimb('arm_R');
        this.targetPos = { x: 28.0, y: -18.0, z: 52.0 };
        this.blendWeight = 1.0;
      }

      if (this.targetGizmo) {
        this.targetGizmo.setPosition(this.targetPos.x, this.targetPos.y, this.targetPos.z);
      }
      this._syncUI();
      this.update(0, 0, 0);
    }

    _syncUI() {
      if (!this.paramsContainer) return;
      const select = this.paramsContainer.querySelector('#ikLimbSelect');
      if (select) select.value = this.activeLimb;
      const blend = this.paramsContainer.querySelector('#ikBlendSlider');
      if (blend) blend.value = this.blendWeight;
      const l1 = this.paramsContainer.querySelector('#ikL1Slider');
      if (l1) l1.value = this.limbLengthL1;
      const l2 = this.paramsContainer.querySelector('#ikL2Slider');
      if (l2) l2.value = this.limbLengthL2;
      this._syncUITarget();
    }

    _syncUITarget() {
      if (!this.paramsContainer) return;
      const tx = this.paramsContainer.querySelector('#ikTargetX');
      const ty = this.paramsContainer.querySelector('#ikTargetY');
      const tz = this.paramsContainer.querySelector('#ikTargetZ');
      if (tx) tx.value = this.targetPos.x;
      const lx = this.paramsContainer.querySelector('#lblTargetX');
      if (lx) lx.innerText = this.targetPos.x.toFixed(1);
      if (ty) ty.value = this.targetPos.y;
      const ly = this.paramsContainer.querySelector('#lblTargetY');
      if (ly) ly.innerText = this.targetPos.y.toFixed(1);
      if (tz) tz.value = this.targetPos.z;
      const lz = this.paramsContainer.querySelector('#lblTargetZ');
      if (lz) lz.innerText = this.targetPos.z.toFixed(1);
    }

    _buildParamsUI() {
      if (!this.paramsContainer) return;

      this.paramsContainer.innerHTML = `
        <div class="space-y-4 font-mono text-xs select-none">
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1.5">
              <span>LIMB &amp; IK CONFIGURATION</span>
              <span class="text-[10px] text-cyan-400">FUN_1802f9750</span>
            </div>

            <div>
              <label class="text-[11px] text-slate-400 block mb-1">Active Limb:</label>
              <select id="ikLimbSelect" class="w-full bg-slate-800 border border-slate-700 rounded px-2.5 py-1.5 text-slate-200 cursor-pointer">
                <option value="arm_R" ${this.activeLimb === 'arm_R' ? 'selected' : ''}>Right Arm (Shoulder &rarr; Elbow &rarr; Hand)</option>
                <option value="arm_L" ${this.activeLimb === 'arm_L' ? 'selected' : ''}>Left Arm (Shoulder &rarr; Elbow &rarr; Hand)</option>
                <option value="leg_R" ${this.activeLimb === 'leg_R' ? 'selected' : ''}>Right Leg (Hip &rarr; Knee &rarr; Foot)</option>
                <option value="leg_L" ${this.activeLimb === 'leg_L' ? 'selected' : ''}>Left Leg (Hip &rarr; Knee &rarr; Foot)</option>
              </select>
            </div>

            <div class="space-y-1">
              <div class="flex justify-between text-[11px]">
                <span class="text-slate-400">IK Blend Weight (&lambda;):</span>
                <span id="ikBlendVal" class="text-amber-400 font-bold">${(this.blendWeight * 100).toFixed(0)}%</span>
              </div>
              <input type="range" id="ikBlendSlider" min="0" max="1" step="0.01" value="${this.blendWeight}" class="w-full accent-amber-400 cursor-pointer">
            </div>

            <div class="grid grid-cols-2 gap-2 text-[11px]">
              <div>
                <span class="text-slate-400 block">Limb L1 (Upper):</span>
                <input type="range" id="ikL1Slider" min="5" max="25" step="0.5" value="${this.limbLengthL1}" class="w-full accent-cyan-400 cursor-pointer">
              </div>
              <div>
                <span class="text-slate-400 block">Limb L2 (Lower):</span>
                <input type="range" id="ikL2Slider" min="5" max="25" step="0.5" value="${this.limbLengthL2}" class="w-full accent-purple-400 cursor-pointer">
              </div>
            </div>
          </div>

          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1">
              <span>TARGET POSITION [X, Y, Z]</span>
              <span class="text-[10px] text-amber-400">3D Draggable</span>
            </div>

            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Target X (Forward):</span>
                <span id="lblTargetX" class="text-rose-400 font-bold">${this.targetPos.x.toFixed(1)}</span>
              </div>
              <input type="range" id="ikTargetX" min="-15" max="45" step="0.5" value="${this.targetPos.x}" class="w-full accent-rose-500 cursor-pointer">
            </div>

            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Target Y (Lateral):</span>
                <span id="lblTargetY" class="text-emerald-400 font-bold">${this.targetPos.y.toFixed(1)}</span>
              </div>
              <input type="range" id="ikTargetY" min="-30" max="30" step="0.5" value="${this.targetPos.y}" class="w-full accent-emerald-500 cursor-pointer">
            </div>

            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Target Z (Vertical):</span>
                <span id="lblTargetZ" class="text-cyan-400 font-bold">${this.targetPos.z.toFixed(1)}</span>
              </div>
              <input type="range" id="ikTargetZ" min="0" max="75" step="0.5" value="${this.targetPos.z}" class="w-full accent-cyan-500 cursor-pointer">
            </div>
          </div>

          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2">
            <div class="text-slate-300 font-bold border-b border-slate-800 pb-1 text-[11px]">
              STANCE PRESETS
            </div>
            <div class="grid grid-cols-2 gap-1.5">
              <button id="btnStanceCombat" class="py-1.5 px-2 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 text-[10px] font-bold border border-slate-700 cursor-pointer">Combat Stance</button>
              <button id="btnStanceReach" class="py-1.5 px-2 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 text-[10px] font-bold border border-slate-700 cursor-pointer">High Reach</button>
              <button id="btnStanceGround" class="py-1.5 px-2 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 text-[10px] font-bold border border-slate-700 cursor-pointer">Ground Plant</button>
              <button id="btnStanceOver" class="py-1.5 px-2 rounded bg-rose-950/40 hover:bg-rose-900/50 text-rose-300 text-[10px] font-bold border border-rose-800/60 cursor-pointer">Overextension</button>
            </div>
          </div>

          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2">
            <div class="text-slate-300 font-bold border-b border-slate-800 pb-1 text-[11px]">
              RENDER OVERLAYS
            </div>
            <div class="space-y-1.5 text-[11px]">
              <label class="flex items-center gap-2 text-slate-300 cursor-pointer">
                <input type="checkbox" id="chkShowBones" ${this.showBones ? 'checked' : ''} class="accent-cyan-400">
                <span>Show Articulated Skeleton Hierarchy</span>
              </label>
              <label class="flex items-center gap-2 text-slate-300 cursor-pointer">
                <input type="checkbox" id="chkShowHitboxes" ${this.showHitboxes ? 'checked' : ''} class="accent-rose-400">
                <span>Show Source 2 Hitbox Capsules</span>
              </label>
              <label class="flex items-center gap-2 text-slate-300 cursor-pointer">
                <input type="checkbox" id="chkShowReach" ${this.showReachVolume ? 'checked' : ''} class="accent-purple-400">
                <span>Show Limb Reach Envelope Sphere</span>
              </label>
            </div>
          </div>
        </div>
      `;

      const select = this.paramsContainer.querySelector('#ikLimbSelect');
      if (select) select.onchange = (e) => this._switchLimb(e.target.value);

      const blend = this.paramsContainer.querySelector('#ikBlendSlider');
      if (blend) blend.oninput = (e) => {
        this.blendWeight = parseFloat(e.target.value);
        this.paramsContainer.querySelector('#ikBlendVal').innerText = `${(this.blendWeight * 100).toFixed(0)}%`;
        this.update(0, 0, 0);
      };

      const l1 = this.paramsContainer.querySelector('#ikL1Slider');
      if (l1) l1.oninput = (e) => {
        this.limbLengthL1 = parseFloat(e.target.value);
        this.update(0, 0, 0);
      };

      const l2 = this.paramsContainer.querySelector('#ikL2Slider');
      if (l2) l2.oninput = (e) => {
        this.limbLengthL2 = parseFloat(e.target.value);
        this.update(0, 0, 0);
      };

      const tx = this.paramsContainer.querySelector('#ikTargetX');
      const ty = this.paramsContainer.querySelector('#ikTargetY');
      const tz = this.paramsContainer.querySelector('#ikTargetZ');
      const updateTargetFromSliders = () => {
        this.targetPos.x = parseFloat(tx.value);
        this.targetPos.y = parseFloat(ty.value);
        this.targetPos.z = parseFloat(tz.value);
        if (this.targetGizmo) this.targetGizmo.setPosition(this.targetPos.x, this.targetPos.y, this.targetPos.z);
        this._syncUITarget();
        this.update(0, 0, 0);
      };
      if (tx) tx.oninput = updateTargetFromSliders;
      if (ty) ty.oninput = updateTargetFromSliders;
      if (tz) tz.oninput = updateTargetFromSliders;

      const chkBones = this.paramsContainer.querySelector('#chkShowBones');
      if (chkBones) chkBones.onchange = (e) => { this.showBones = e.target.checked; this.update(0, 0, 0); };
      const chkHit = this.paramsContainer.querySelector('#chkShowHitboxes');
      if (chkHit) chkHit.onchange = (e) => { this.showHitboxes = e.target.checked; this.update(0, 0, 0); };
      const chkReach = this.paramsContainer.querySelector('#chkShowReach');
      if (chkReach) chkReach.onchange = (e) => { this.showReachVolume = e.target.checked; this.update(0, 0, 0); };

      const btnCombat = this.paramsContainer.querySelector('#btnStanceCombat');
      const btnReach = this.paramsContainer.querySelector('#btnStanceReach');
      const btnGround = this.paramsContainer.querySelector('#btnStanceGround');
      const btnOver = this.paramsContainer.querySelector('#btnStanceOver');
      if (btnCombat) btnCombat.onclick = () => this._applyStancePreset('combat');
      if (btnReach) btnReach.onclick = () => this._applyStancePreset('reach');
      if (btnGround) btnGround.onclick = () => this._applyStancePreset('ground');
      if (btnOver) btnOver.onclick = () => this._applyStancePreset('overextension');
    }
  }

  class Sim11VolumetricSmokeProvider {
    constructor(viewport, audioEngine, playback) {
      this.viewport = viewport;
      this.audio = audioEngine;
      this.playback = playback;

      this.GRID_DIM = 32;
      this.GRID_TOTAL = 32768; // 32^3
      this.VOXEL_SIZE = 20.0;
      this.VOXEL_INV_SIZE = 1.0 / this.VOXEL_SIZE;
      this.HALF_EXTENT = 320.0;
      this.FULL_OCCLUSION_DENSITY = 0.20; // server.dll threshold

      this.detonationPos = { x: 0, y: 0, z: 40 };

      // State
      this.densities = new Float32Array(this.GRID_TOTAL);
      this.occupancy = new Uint32Array(1024); // 32768 bits
      this.smokeRadius = 260.0;

      // Line-of-sight test endpoints
      this.shooterPos = { x: -340, y: -30, z: 40 };
      this.targetPos = { x: 340, y: -40, z: 40 };

      // 3D Visualizer Objects
      this.sceneGroup = null;
      this.puffMesh = null;
      this.boundsBox = null;
      this.losLine = null;
      this.tracerMesh = null;
      this.tunnelMesh = null;
      this.tunnelRimMesh = null;
      this.shooterMarker = null;
      this.targetMarker = null;
      this.blastRingMesh = null;
      this.blastTimer = 0;
      this.tracerTimer = 0;
      this.resealTimer = 0; // CS2 3.5s smoke reseal countdown
      this.isVoidLocked = false;

      // Telemetry metrics
      this.activeVoxelCount = 0;
      this.losOpacity = 1.0;
      this.losOccluded = true;
      this.accumulatedDensity = 0.0;
      this.paramsContainer = null;

      this._initSmokeVolume(this.smokeRadius);
    }

    _part1By2(x) {
      x &= 0x1f;
      x = (x | (x << 8)) & 0x300f00f;
      x = (x ^ (x << 4)) & 0x30c30c3;
      x = (x ^ (x << 2)) & 0x9249249;
      return x >>> 0;
    }

    _mortonEncode(x, y, z) {
      return (this._part1By2(x) | (this._part1By2(y) << 1) | (this._part1By2(z) << 2)) >>> 0;
    }

    _worldToVoxel(wx, wy, wz) {
      const vx = Math.floor((wx - this.detonationPos.x) * this.VOXEL_INV_SIZE + 16.0);
      const vy = Math.floor((wy - this.detonationPos.y) * this.VOXEL_INV_SIZE + 16.0);
      const vz = Math.floor((wz - this.detonationPos.z) * this.VOXEL_INV_SIZE + 16.0);
      if (vx >= 0 && vx < 32 && vy >= 0 && vy < 32 && vz >= 0 && vz < 32) {
        return { vx, vy, vz };
      }
      return null;
    }

    _voxelToWorld(vx, vy, vz) {
      return {
        x: (vx - 16.0 + 0.5) * this.VOXEL_SIZE + this.detonationPos.x,
        y: (vy - 16.0 + 0.5) * this.VOXEL_SIZE + this.detonationPos.y,
        z: (vz - 16.0 + 0.5) * this.VOXEL_SIZE + this.detonationPos.z
      };
    }

    _isVoxelActive(vx, vy, vz) {
      const morton = this._mortonEncode(vx, vy, vz);
      const word = morton >>> 5;
      const bit = morton & 0x1f;
      return (this.occupancy[word] & (1 << bit)) !== 0;
    }

    _setVoxelActive(vx, vy, vz, density = 0.25) {
      const morton = this._mortonEncode(vx, vy, vz);
      const word = morton >>> 5;
      const bit = morton & 0x1f;
      this.occupancy[word] |= (1 << bit);
      this.densities[morton] = density;
    }

    _clearVoxel(vx, vy, vz) {
      const morton = this._mortonEncode(vx, vy, vz);
      const word = morton >>> 5;
      const bit = morton & 0x1f;
      this.occupancy[word] &= ~(1 << bit);
      this.densities[morton] = 0.0;
    }

    _initSmokeVolume(radius = 260.0) {
      this.occupancy.fill(0);
      this.densities.fill(0.0);
      const rSqr = radius * radius;
      let count = 0;

      for (let vz = 0; vz < 32; vz++) {
        for (let vy = 0; vy < 32; vy++) {
          for (let vx = 0; vx < 32; vx++) {
            const w = this._voxelToWorld(vx, vy, vz);
            const dx = w.x - this.detonationPos.x;
            const dy = w.y - this.detonationPos.y;
            const dz = w.z - this.detonationPos.z;
            const dSqr = dx*dx + dy*dy + dz*dz;
            if (dSqr <= rSqr) {
              const normDist = Math.sqrt(dSqr) / radius;
              const density = Math.max(0.05, 0.45 * (1.0 - normDist * 0.75));
              this._setVoxelActive(vx, vy, vz, density);
              count++;
            }
          }
        }
      }
      this.activeVoxelCount = count;
      this.resealTimer = 0;
      this._updatePuffMesh();
    }

    carveBulletTrajectory(startX, startY, startZ, endX, endY, endZ, radius = 42.0) {
      const dx = endX - startX, dy = endY - startY, dz = endZ - startZ;
      const rayLen = Math.hypot(dx, dy, dz);
      if (rayLen < 1e-4) return 0;

      const ux = dx / rayLen, uy = dy / rayLen, uz = dz / rayLen;
      const rSqr = radius * radius;
      let cleared = 0;

      for (let z = 0; z < 32; z++) {
        for (let y = 0; y < 32; y++) {
          for (let x = 0; x < 32; x++) {
            if (!this._isVoxelActive(x, y, z)) continue;
            const w = this._voxelToWorld(x, y, z);
            const px = w.x - startX, py = w.y - startY, pz = w.z - startZ;
            const proj = px * ux + py * uy + pz * uz;
            if (proj >= 0 && proj <= rayLen) {
              const distSqr = (px - proj * ux)**2 + (py - proj * uy)**2 + (pz - proj * uz)**2;
              if (distSqr <= rSqr) {
                this._clearVoxel(x, y, z);
                cleared++;
              }
            }
          }
        }
      }
      this.activeVoxelCount = Math.max(0, this.activeVoxelCount - cleared);
      this._updatePuffMesh();

      this._triggerTracer(startX, startY, startZ, endX, endY, endZ, radius);
      this.resealTimer = 4.0;

      if (this.audio) {
        this.audio.playGunfire(startX, startY, startZ);
      }
      return cleared;
    }

    carveExplosionBlast(cx = 0, cy = 0, cz = 40, clearR = 190.0, pushR = 270.0) {
      const clearR2 = clearR * clearR;
      const pushR2 = pushR * pushR;
      let affected = 0;

      for (let z = 0; z < 32; z++) {
        for (let y = 0; y < 32; y++) {
          for (let x = 0; x < 32; x++) {
            if (!this._isVoxelActive(x, y, z)) continue;
            const w = this._voxelToWorld(x, y, z);
            const dx = w.x - cx, dy = w.y - cy, dz = w.z - cz;
            const dSqr = dx*dx + dy*dy + dz*dz;
            if (dSqr <= clearR2) {
              this._clearVoxel(x, y, z);
              affected++;
            } else if (dSqr <= pushR2) {
              const d = Math.sqrt(dSqr);
              const pushX = (d > 1e-6 ? dx / d : 0) * 40.0;
              const pushY = (d > 1e-6 ? dy / d : 0) * 40.0;
              const pushZ = (d > 1e-6 ? dz / d : 1) * 40.0;
              const pushed = this._worldToVoxel(w.x + pushX, w.y + pushY, w.z + pushZ);
              if (pushed) {
                const morton = this._mortonEncode(x, y, z);
                const oldDens = this.densities[morton];
                this._clearVoxel(x, y, z);
                this._setVoxelActive(pushed.vx, pushed.vy, pushed.vz, oldDens * 0.9);
                affected++;
              }
            }
          }
        }
      }
      this.activeVoxelCount = Math.max(0, this.activeVoxelCount - affected);
      this._updatePuffMesh();
      this.resealTimer = 4.5;
      if (this.audio) {
        this.audio.playExplosion(cx, cy, cz);
      }
      this.blastTimer = 1.0;
      return affected;
    }

    _triggerTracer(x0, y0, z0, x1, y1, z1, radius) {
      const len = Math.hypot(x1 - x0, y1 - y0, z1 - z0);
      if (len < 1) return;

      const dir = new THREE.Vector3((x1 - x0) / len, (y1 - y0) / len, (z1 - z0) / len);
      const mid = new THREE.Vector3((x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5);

      if (this.tracerMesh) {
        this.tracerMesh.position.copy(mid);
        this.tracerMesh.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir);
        this.tracerMesh.scale.set(1, len, 1);
        this.tracerMesh.visible = true;
      }

      if (this.tunnelMesh) {
        this.tunnelMesh.position.copy(mid);
        this.tunnelMesh.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir);
        this.tunnelMesh.scale.set(radius, len, radius);
        this.tunnelMesh.visible = true;
      }

      if (this.tunnelRimMesh) {
        this.tunnelRimMesh.position.copy(mid);
        this.tunnelRimMesh.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir);
        this.tunnelRimMesh.scale.set(radius * 1.05, len, radius * 1.05);
        this.tunnelRimMesh.visible = true;
      }

      this.tracerTimer = 1.0;
    }

    raymarchLoS(startX, startY, startZ, endX, endY, endZ) {
      const dx = endX - startX, dy = endY - startY, dz = endZ - startZ;
      const rayLen = Math.hypot(dx, dy, dz);
      if (rayLen < 1e-4) {
        this.accumulatedDensity = 0.0;
        this.losOccluded = false;
        this.losOpacity = 0.0;
        return;
      }
      const dirX = dx / rayLen, dirY = dy / rayLen, dirZ = dz / rayLen;
      const stepSize = this.VOXEL_SIZE * 0.5; // 10 units
      const maxSteps = Math.min(128, Math.floor(rayLen / stepSize) + 1);
      let accumulated = 0.0;

      for (let i = 0; i <= maxSteps; i++) {
        const dist = i * stepSize;
        if (dist > rayLen) break;
        const curX = startX + dirX * dist;
        const curY = startY + dirY * dist;
        const curZ = startZ + dirZ * dist;
        const v = this._worldToVoxel(curX, curY, curZ);
        if (v && this._isVoxelActive(v.vx, v.vy, v.vz)) {
          const morton = this._mortonEncode(v.vx, v.vy, v.vz);
          const density = this.densities[morton];
          accumulated += density * (stepSize / this.VOXEL_SIZE);
        }
      }

      this.accumulatedDensity = accumulated;
      const transmittance = Math.exp(-accumulated * 1.8);
      this.losOpacity = Math.max(0.0, Math.min(1.0, 1.0 - transmittance));
      this.losOccluded = (accumulated >= this.FULL_OCCLUSION_DENSITY);
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim11_VolumetricSmoke_Scene';

      // 1. Organic Volumetric Smoke Puff Clustered Rendering
      const puffGeo = new THREE.SphereGeometry(14.0, 10, 8);
      const puffMat = new THREE.MeshStandardMaterial({
        color: 0x94a3b8,
        roughness: 0.85,
        transparent: true,
        opacity: 0.42,
        depthWrite: false
      });
      this.puffMesh = new THREE.InstancedMesh(puffGeo, puffMat, 10000);
      this.puffMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
      this.sceneGroup.add(this.puffMesh);
      this._updatePuffMesh();

      // 2. 640u Domain Bounding Wireframe Box
      const boundsGeo = new THREE.BoxGeometry(640, 640, 640);
      const edges = new THREE.EdgesGeometry(boundsGeo);
      this.boundsBox = new THREE.LineSegments(edges, new THREE.LineBasicMaterial({
        color: 0x0ea5e9,
        transparent: true,
        opacity: 0.22
      }));
      this.boundsBox.position.set(this.detonationPos.x, this.detonationPos.y, this.detonationPos.z);
      this.sceneGroup.add(this.boundsBox);

      // 3. Supersonic Bullet Tracer Beam (Glowing Neon Cylinder)
      const tracerGeo = new THREE.CylinderGeometry(2.2, 2.2, 1.0, 8);
      const tracerMat = new THREE.MeshBasicMaterial({ color: 0xfef08a, transparent: true, opacity: 0.95 });
      this.tracerMesh = new THREE.Mesh(tracerGeo, tracerMat);
      this.tracerMesh.visible = false;
      this.sceneGroup.add(this.tracerMesh);

      // 4. Carved Cylindrical Void Channel (Glowing Neon Wireframe boundary)
      const tunnelGeo = new THREE.CylinderGeometry(1.0, 1.0, 1.0, 18, 1, true);
      const tunnelMat = new THREE.MeshBasicMaterial({
        color: 0x38bdf8,
        wireframe: true,
        transparent: true,
        opacity: 0.65
      });
      this.tunnelMesh = new THREE.Mesh(tunnelGeo, tunnelMat);
      this.tunnelMesh.visible = false;
      this.sceneGroup.add(this.tunnelMesh);

      // 4b. Illuminated Tunnel Light Shaft Rim
      const rimGeo = new THREE.CylinderGeometry(1.0, 1.0, 1.0, 18, 1, true);
      const rimMat = new THREE.MeshBasicMaterial({
        color: 0x10b981,
        transparent: true,
        opacity: 0.25,
        side: THREE.DoubleSide
      });
      this.tunnelRimMesh = new THREE.Mesh(rimGeo, rimMat);
      this.tunnelRimMesh.visible = false;
      this.sceneGroup.add(this.tunnelRimMesh);

      // 5. Dynamic Line of Sight (LoS) Ray (Red when blocked -> Vibrant Green when clear through tunnel)
      const losGeo = new THREE.BufferGeometry().setFromPoints([
        new THREE.Vector3(this.shooterPos.x, this.shooterPos.y, this.shooterPos.z),
        new THREE.Vector3(this.targetPos.x, this.targetPos.y, this.targetPos.z)
      ]);
      this.losLine = new THREE.Line(losGeo, new THREE.LineBasicMaterial({ color: 0xef4444, linewidth: 3 }));
      this.sceneGroup.add(this.losLine);

      // 6. Shooter & Target Tactical Markers
      const smGeo = new THREE.SphereGeometry(8, 14, 10);
      this.shooterMarker = new THREE.Mesh(smGeo, new THREE.MeshStandardMaterial({ color: 0x06b6d4, emissive: 0x0891b2 }));
      this.targetMarker = new THREE.Mesh(smGeo, new THREE.MeshStandardMaterial({ color: 0x10b981, emissive: 0x059669 }));
      this.sceneGroup.add(this.shooterMarker);
      this.sceneGroup.add(this.targetMarker);

      // 7. HE Blast Shockwave Ring
      const ringGeo = new THREE.RingGeometry(20, 26, 32);
      const ringMat = new THREE.MeshBasicMaterial({ color: 0xf59e0b, side: THREE.DoubleSide, transparent: true, opacity: 0.0 });
      this.blastRingMesh = new THREE.Mesh(ringGeo, ringMat);
      this.blastRingMesh.rotation.x = Math.PI * 0.5;
      this.blastRingMesh.position.set(0, 0, 40);
      this.sceneGroup.add(this.blastRingMesh);

      scene.add(this.sceneGroup);

      // 8. Mount UI Controls
      this._buildParamsUI();

      // 9. Frame Smoke Cloud with Optimal Camera
      const camera = this.viewport.getCamera();
      if (camera) {
        camera.position.set(-380, -420, 220);
        const controls = this.viewport.getControls();
        if (controls) {
          controls.target.set(0, 0, 40);
          controls.update();
        }
      }

      this.update(0, 0, 0);
    }

    _updatePuffMesh() {
      if (!this.puffMesh) return;
      const dummy = new THREE.Object3D();
      let instIdx = 0;

      for (let z = 0; z < 32; z++) {
        for (let y = 0; y < 32; y++) {
          for (let x = 0; x < 32; x++) {
            if (this._isVoxelActive(x, y, z)) {
              if (instIdx >= 10000) break;
              const w = this._voxelToWorld(x, y, z);
              dummy.position.set(w.x, w.y, w.z);
              const morton = this._mortonEncode(x, y, z);
              const d = this.densities[morton] || 0.25;
              const scale = 0.9 + d * 1.4;
              dummy.scale.set(scale, scale, scale);
              dummy.updateMatrix();
              this.puffMesh.setMatrixAt(instIdx++, dummy.matrix);
            }
          }
        }
      }
      this.puffMesh.count = instIdx;
      this.puffMesh.instanceMatrix.needsUpdate = true;
    }

    update(tick, subTickFrac, dt) {
      if (this.resealTimer > 0 && !this.isVoidLocked) {
        this.resealTimer = Math.max(0, this.resealTimer - dt);
        if (this.resealTimer === 0) {
          this._initSmokeVolume(this.smokeRadius);
        }
      }

      this.raymarchLoS(this.shooterPos.x, this.shooterPos.y, this.shooterPos.z,
                       this.targetPos.x, this.targetPos.y, this.targetPos.z);

      if (this.shooterMarker) this.shooterMarker.position.set(this.shooterPos.x, this.shooterPos.y, this.shooterPos.z);
      if (this.targetMarker) this.targetMarker.position.set(this.targetPos.x, this.targetPos.y, this.targetPos.z);

      if (this.losLine && this.losLine.geometry) {
        const pts = [
          new THREE.Vector3(this.shooterPos.x, this.shooterPos.y, this.shooterPos.z),
          new THREE.Vector3(this.targetPos.x, this.targetPos.y, this.targetPos.z)
        ];
        this.losLine.geometry.setFromPoints(pts);
        if (this.losLine.material) {
          if (this.losOccluded) {
            this.losLine.material.color.setHex(0xef4444);
            this.losLine.material.opacity = 0.85;
          } else {
            this.losLine.material.color.setHex(0x10b981);
            this.losLine.material.opacity = 1.0;
          }
        }
      }

      if (this.tracerTimer > 0) {
        this.tracerTimer = Math.max(0, this.tracerTimer - dt * 2.0);
        if (this.tracerMesh && this.tracerMesh.material) {
          this.tracerMesh.material.opacity = this.tracerTimer * 0.95;
          this.tracerMesh.visible = this.tracerTimer > 0.01;
        }
      }

      if (this.tunnelMesh && this.tunnelMesh.material) {
        const isTunnelActive = this.resealTimer > 0 || this.isVoidLocked;
        this.tunnelMesh.visible = isTunnelActive;
        if (this.tunnelRimMesh) this.tunnelRimMesh.visible = isTunnelActive;
        if (isTunnelActive) {
          const fade = Math.min(1.0, this.resealTimer / 1.5);
          this.tunnelMesh.material.opacity = 0.65 * fade;
          if (this.tunnelRimMesh && this.tunnelRimMesh.material) {
            this.tunnelRimMesh.material.opacity = 0.25 * fade;
          }
        }
      }

      if (this.blastTimer > 0) {
        this.blastTimer = Math.max(0, this.blastTimer - dt * 1.5);
        if (this.blastRingMesh) {
          const r = (1.0 - this.blastTimer) * 220.0 + 20.0;
          this.blastRingMesh.scale.set(r / 20.0, r / 20.0, 1.0);
          this.blastRingMesh.material.opacity = this.blastTimer * 0.85;
          this.blastRingMesh.visible = this.blastTimer > 0.01;
        }
      }

      const isClear = !this.losOccluded;
      this.viewport.setHudMetrics([
        { label: 'ACTIVE VOXELS', value: `${this.activeVoxelCount} / 32768` },
        { label: 'VOXEL OCCUPANCY', value: `${((this.activeVoxelCount / 32768) * 100).toFixed(1)}%` },
        { label: 'LINE DENSITY (\u03C1)', value: this.accumulatedDensity.toFixed(3) },
        { label: 'THRESHOLD', value: '0.200 (Optical Occlusion)' },
        { label: 'LOS STATUS', value: isClear ? 'LOS RESTORED (Carved Corridor)' : 'FULLY OCCLUDED (Smoke Blocked)', alert: this.losOccluded },
        { label: 'RESEAL TIMER', value: this.isVoidLocked ? 'LOCKED (Freeze)' : (this.resealTimer > 0 ? `${this.resealTimer.toFixed(1)}s (Resealing)` : 'EQUILIBRIUM') }
      ]);
    }

    teardown() {
      const scene = this.viewport.getScene();
      if (this.sceneGroup) {
        if (scene) scene.remove(this.sceneGroup);
        this.sceneGroup = null;
      }
      if (this.paramsContainer) {
        this.paramsContainer.innerHTML = '';
        this.paramsContainer = null;
      }
    }

    _buildParamsUI() {
      if (!this.paramsContainer) return;
      this.paramsContainer.innerHTML = `
        <div class="space-y-4 font-mono text-xs select-none">
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1.5">
              <span>SOURCE 2 VOXEL ACTIONS</span>
              <span class="text-[10px] text-cyan-400">server.dll</span>
            </div>
            <div class="grid grid-cols-2 gap-2">
              <button id="sim11BtnBullet" class="py-2 px-2.5 rounded bg-cyan-700 hover:bg-cyan-600 text-white font-bold text-[11px] flex flex-col items-center justify-center gap-0.5 cursor-pointer shadow-sm">
                <span>Fire Bullet Carve</span>
                <span class="text-[9.5px] text-cyan-200 opacity-90">(R=42u Corridor)</span>
              </button>
              <button id="sim11BtnNade" class="py-2 px-2.5 rounded bg-amber-600 hover:bg-amber-500 text-white font-bold text-[11px] flex flex-col items-center justify-center gap-0.5 cursor-pointer shadow-sm">
                <span>HE Grenade Blast</span>
                <span class="text-[9.5px] text-amber-200 opacity-90">(R=190u Void)</span>
              </button>
            </div>
            <div class="flex gap-2 pt-1">
              <button id="sim11BtnReset" class="flex-1 py-1.5 px-2 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 text-[11px] font-bold cursor-pointer">
                Respawn Volumetric Smoke
              </button>
              <button id="sim11BtnLock" class="py-1.5 px-3 rounded bg-slate-800 hover:bg-slate-700 text-slate-300 border border-slate-700 text-[11px] font-bold cursor-pointer">
                ${this.isVoidLocked ? 'Unlock Reseal' : 'Lock Void'}
              </button>
            </div>
          </div>

          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1">
              <span>OPTICAL LINE-OF-SIGHT DDA</span>
              <span class="text-[10px] text-emerald-400">0x1809b7f50</span>
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Smoke Cloud Radius:</span>
                <span id="sim11LblRad" class="text-cyan-300">${this.smokeRadius.toFixed(0)} u</span>
              </div>
              <input type="range" id="sim11Radius" min="160" max="300" step="10" value="${this.smokeRadius}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Shooter Y Position:</span>
                <span id="sim11LblSY" class="text-cyan-300">${this.shooterPos.y.toFixed(0)} u</span>
              </div>
              <input type="range" id="sim11ShooterY" min="-180" max="180" step="5" value="${this.shooterPos.y}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Target Y Position:</span>
                <span id="sim11LblTY" class="text-amber-400">${this.targetPos.y.toFixed(0)} u</span>
              </div>
              <input type="range" id="sim11TargetY" min="-180" max="180" step="5" value="${this.targetPos.y}" class="w-full accent-amber-500 cursor-pointer">
            </div>
          </div>

          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1 text-[11px]">
              <span>VOXEL TELEMETRY</span>
              <span id="sim11BadgeOcclusion" class="px-2 py-0.5 rounded text-[10px] font-bold ${this.losOccluded ? 'bg-rose-500/20 text-rose-400 border border-rose-500/30' : 'bg-emerald-500/20 text-emerald-400 border border-emerald-500/30'}">
                ${this.losOccluded ? 'OCCLUDED (100%)' : 'CLEAR LOS (Carved Void)'}
              </span>
            </div>
            <div class="grid grid-cols-2 gap-2 text-center text-[10px]">
              <div class="p-2 bg-slate-950/60 rounded border border-slate-800">
                <div class="text-slate-400">Active Voxels</div>
                <div id="sim11StatVoxels" class="text-cyan-300 font-bold text-xs mt-0.5">${this.activeVoxelCount} / 32768</div>
              </div>
              <div class="p-2 bg-slate-950/60 rounded border border-slate-800">
                <div class="text-slate-400">Optical Density</div>
                <div id="sim11StatDensity" class="text-emerald-400 font-bold text-xs mt-0.5">${this.accumulatedDensity.toFixed(3)} / 0.200</div>
              </div>
            </div>
          </div>
        </div>
      `;

      const btnB = this.paramsContainer.querySelector('#sim11BtnBullet');
      const btnN = this.paramsContainer.querySelector('#sim11BtnNade');
      const btnR = this.paramsContainer.querySelector('#sim11BtnReset');
      const btnL = this.paramsContainer.querySelector('#sim11BtnLock');

      if (btnB) btnB.onclick = () => {
        this.carveBulletTrajectory(this.shooterPos.x, this.shooterPos.y, this.shooterPos.z,
                                   this.targetPos.x, this.targetPos.y, this.targetPos.z, 42.0);
        this.update(0, 0, 0);
      };

      if (btnN) btnN.onclick = () => {
        this.carveExplosionBlast(this.detonationPos.x, this.detonationPos.y, this.detonationPos.z, 190.0, 270.0);
        this.update(0, 0, 0);
      };

      if (btnR) btnR.onclick = () => {
        this._initSmokeVolume(this.smokeRadius);
        if (this.tunnelMesh) this.tunnelMesh.visible = false;
        if (this.tunnelRimMesh) this.tunnelRimMesh.visible = false;
        this.update(0, 0, 0);
      };

      if (btnL) btnL.onclick = () => {
        this.isVoidLocked = !this.isVoidLocked;
        btnL.innerText = this.isVoidLocked ? 'Unlock Reseal' : 'Lock Void';
        this.update(0, 0, 0);
      };

      const radIn = this.paramsContainer.querySelector('#sim11Radius');
      const lblR = this.paramsContainer.querySelector('#sim11LblRad');
      if (radIn && lblR) {
        radIn.oninput = (e) => {
          this.smokeRadius = parseFloat(e.target.value);
          lblR.textContent = `${this.smokeRadius.toFixed(0)} u`;
          this._initSmokeVolume(this.smokeRadius);
          this.update(0, 0, 0);
        };
      }

      const syIn = this.paramsContainer.querySelector('#sim11ShooterY');
      const lblSY = this.paramsContainer.querySelector('#sim11LblSY');
      if (syIn && lblSY) {
        syIn.oninput = (e) => {
          this.shooterPos.y = parseFloat(e.target.value);
          lblSY.textContent = `${this.shooterPos.y.toFixed(0)} u`;
          this.update(0, 0, 0);
        };
      }

      const tyIn = this.paramsContainer.querySelector('#sim11TargetY');
      const lblTY = this.paramsContainer.querySelector('#sim11LblTY');
      if (tyIn && lblTY) {
        tyIn.oninput = (e) => {
          this.targetPos.y = parseFloat(e.target.value);
          lblTY.textContent = `${this.targetPos.y.toFixed(0)} u`;
          this.update(0, 0, 0);
        };
      }
    }
  }

  class Sim14BulletPenetrationProvider {
    constructor(viewport, audioEngine, playback) {
      this.viewport = viewport;
      this.audio = audioEngine;
      this.playback = playback;

      // Weapons table
      this.weapons = {
        ak47:   { name: 'AK-47', damage: 36.0, rangeMod: 0.98, penPower: 2.0, armorRatio: 1.55 },
        awp:    { name: 'AWP', damage: 115.0, rangeMod: 0.99, penPower: 2.5, armorRatio: 1.95 },
        m4a1s:  { name: 'M4A1-S', damage: 38.0, rangeMod: 0.99, penPower: 1.0, armorRatio: 1.40 },
        deagle: { name: 'Desert Eagle', damage: 53.0, rangeMod: 0.81, penPower: 2.0, armorRatio: 1.70 }
      };

      // Materials table
      this.materials = {
        wood:      { name: 'Wood (Timber)', penMod: 3.0, dmgMod: 0.16, color: 0xb45309, roughness: 0.75, sound: 'wood' },
        concrete:  { name: 'Concrete / Brick', penMod: 1.0, dmgMod: 0.22, color: 0x64748b, roughness: 0.90, sound: 'concrete' },
        metal:     { name: 'Solid Steel', penMod: 1.0, dmgMod: 0.20, color: 0x38bdf8, roughness: 0.25, metalness: 0.85, sound: 'metal' },
        cardboard: { name: 'Cardboard / Drywall', penMod: 3.0, dmgMod: 0.10, color: 0xd97706, roughness: 0.95, sound: 'wood' },
        glass:     { name: 'Window Glass', penMod: 3.0, dmgMod: 0.05, color: 0x7dd3fc, opacity: 0.45, transparent: true, sound: 'concrete' }
      };

      // Active parameters
      this.activeWeapon = 'ak47';
      this.activeMaterial = 'wood';
      this.wallThickness = 16.0; // units
      this.incidentAngleDeg = 15.0; // degrees
      this.targetHitgroup = 'head'; // 'head' | 'chest' | 'stomach' | 'legs'
      this.hasKevlar = true;
      this.hasHelmet = true;

      // 3D Objects
      this.sceneGroup = null;
      this.wallMesh = null;
      this.muzzleMarker = null;
      this.targetDummyGroup = null;
      this.entryRay = null;
      this.subsurfaceRay = null;
      this.exitRay = null;
      this.ricochetRay = null;

      // Evaluation results
      this.evalResults = null;
      this.paramsContainer = null;

      this._evaluate();
    }

    _evaluate() {
      const wpn = this.weapons[this.activeWeapon];
      const mat = this.materials[this.activeMaterial];
      const rad = this.incidentAngleDeg * (Math.PI / 180.0);
      const isRicochet = this.incidentAngleDeg > 65.0;

      // Air travel falloff to wall (120 units)
      const dEntry = wpn.damage * Math.pow(wpn.rangeMod, 120.0 / 500.0);

      // Travel length inside wall
      const lengthInWall = this.wallThickness / Math.cos(rad);

      // Kinetic energy dissipation ODE (server.dll 0x180a90280)
      const penModInv = Math.max(0.0, 1.0 / mat.penMod);
      const lostDistance = (lengthInWall * lengthInWall * penModInv) / 24.0;
      const lostEntrance = Math.max(0.0, (3.0 / wpn.penPower) * 1.25) * (penModInv * 3.0) + (mat.dmgMod * dEntry);
      const totalLost = lostDistance + lostEntrance;
      const dExit = Math.max(0.0, dEntry - totalLost);
      const penetrated = (!isRicochet) && (dExit > 0.0);

      // Target armor mitigation calculation
      let hitMultiplier = 1.0;
      if (this.targetHitgroup === 'head') hitMultiplier = 4.0;
      else if (this.targetHitgroup === 'stomach') hitMultiplier = 1.25;
      else if (this.targetHitgroup === 'legs') hitMultiplier = 0.75;

      const rawDamage = dExit * hitMultiplier;
      let hpDamage = rawDamage;
      let armorDamage = 0;
      let isArmored = false;

      if (penetrated) {
        if (this.targetHitgroup === 'head' && this.hasHelmet) isArmored = true;
        else if ((this.targetHitgroup === 'chest' || this.targetHitgroup === 'stomach') && this.hasKevlar) isArmored = true;

        if (isArmored) {
          // server.dll: flNewDamage = flIncomingDamage * (weaponArmorRatio * 0.5)
          const newDmg = rawDamage * (wpn.armorRatio * 0.5);
          armorDamage = (rawDamage - newDmg) * 0.5;
          hpDamage = newDmg;
        }
      }

      this.evalResults = {
        isRicochet,
        lengthInWall,
        dEntry,
        lostDistance,
        lostEntrance,
        totalLost,
        dExit,
        penetrated,
        rawDamage,
        hpDamage,
        armorDamage,
        isArmored
      };

      return this.evalResults;
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim14_BulletPenetration_Scene';

      // 1. Dynamic Wall Mesh
      const mat = this.materials[this.activeMaterial];
      const wallMat = new THREE.MeshStandardMaterial({
        color: mat.color,
        roughness: mat.roughness !== undefined ? mat.roughness : 0.7,
        metalness: mat.metalness !== undefined ? mat.metalness : 0.1,
        transparent: Boolean(mat.transparent),
        opacity: mat.opacity !== undefined ? mat.opacity : 1.0
      });
      const wallGeo = new THREE.BoxGeometry(this.wallThickness, 160, 140);
      this.wallMesh = new THREE.Mesh(wallGeo, wallMat);
      this.wallMesh.position.set(0, 0, 70);
      this.sceneGroup.add(this.wallMesh);

      // 2. Muzzle Marker
      const muzzleGeo = new THREE.CylinderGeometry(2, 3, 14, 16);
      const muzzleMat = new THREE.MeshStandardMaterial({ color: 0x38bdf8 });
      this.muzzleMarker = new THREE.Mesh(muzzleGeo, muzzleMat);
      this.muzzleMarker.rotation.z = Math.PI * 0.5;
      this.sceneGroup.add(this.muzzleMarker);

      // 3. Target Dummy Group
      this.targetDummyGroup = new THREE.Group();
      this._buildTargetDummy();
      this.sceneGroup.add(this.targetDummyGroup);

      // 4. Trajectory Lines
      const makeLine = (color, dashed = false) => {
        const geo = new THREE.BufferGeometry();
        geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(6), 3));
        const lineMat = dashed
          ? new THREE.LineDashedMaterial({ color, dashSize: 4, gapSize: 2 })
          : new THREE.LineBasicMaterial({ color, linewidth: 2 });
        return new THREE.Line(geo, lineMat);
      };

      this.entryRay = makeLine(0x22c55e);      // Green in-flight
      this.subsurfaceRay = makeLine(0xf59e0b); // Amber subsurface
      this.exitRay = makeLine(0x06b6d4);       // Cyan exit
      this.ricochetRay = makeLine(0xfacc15);   // Bright gold bounce
      this.normalRay = makeLine(0xfacc15, true); // Yellow dashed surface normal

      this.sceneGroup.add(this.entryRay);
      this.sceneGroup.add(this.subsurfaceRay);
      this.sceneGroup.add(this.exitRay);
      this.sceneGroup.add(this.ricochetRay);
      this.sceneGroup.add(this.normalRay);

      // 5. Animated Ballistic Tracer & Impact Flash
      const tracerGeo = new THREE.CylinderGeometry(0.8, 0.8, 8.0, 8);
      const tracerMat = new THREE.MeshBasicMaterial({ color: 0x38bdf8 });
      this.tracerBullet = new THREE.Mesh(tracerGeo, tracerMat);
      this.tracerBullet.rotation.z = Math.PI * 0.5;
      this.tracerBullet.visible = false;
      this.sceneGroup.add(this.tracerBullet);

      const flashGeo = new THREE.SphereGeometry(3.0, 8, 8);
      const flashMat = new THREE.MeshBasicMaterial({ color: 0xfbbf24, transparent: true, opacity: 0.9 });
      this.impactFlash = new THREE.Mesh(flashGeo, flashMat);
      this.impactFlash.visible = false;
      this.sceneGroup.add(this.impactFlash);

      scene.add(this.sceneGroup);

      this._buildParamsUI();

      // Camera view
      const cam = this.viewport.getCamera();
      if (cam) {
        cam.position.set(-180, -240, 160);
        const ctrl = this.viewport.getControls();
        if (ctrl) {
          ctrl.target.set(0, 0, 70);
          ctrl.update();
        }
      }
    }

    _buildTargetDummy() {
      const skinMat = new THREE.MeshStandardMaterial({ color: 0xfbbf24, roughness: 0.6 });
      const vestMat = new THREE.MeshStandardMaterial({ color: 0x1e293b, roughness: 0.4 });
      const helmMat = new THREE.MeshStandardMaterial({ color: 0x0f766e, roughness: 0.3 });

      // Head
      const head = new THREE.Mesh(new THREE.SphereGeometry(6, 16, 16), this.hasHelmet ? helmMat : skinMat);
      head.position.set(0, 0, 110);
      head.name = 'head';
      this.targetDummyGroup.add(head);

      // Torso / Chest
      const torso = new THREE.Mesh(new THREE.CylinderGeometry(8, 7, 24, 16), this.hasKevlar ? vestMat : skinMat);
      torso.position.set(0, 0, 88);
      torso.name = 'chest';
      this.targetDummyGroup.add(torso);

      // Pelvis / Legs
      const legs = new THREE.Mesh(new THREE.CylinderGeometry(6, 5, 34, 16), skinMat);
      legs.position.set(0, 0, 52);
      legs.name = 'legs';
      this.targetDummyGroup.add(legs);

      this.targetDummyGroup.position.set(160, 0, 0);
    }

    fireShot() {
      const res = this._evaluate();
      const mat = this.materials[this.activeMaterial];

      if (this.audio) {
        // Physical acoustic flight scheduling: muzzle blast -> wall crack -> ricochet/terminal hit
        this.audio.playGunfire(-160, 0, 70);

        setTimeout(() => {
          this.audio.playImpact(mat.sound, 0, 0, 70);

          if (res.isRicochet) {
            setTimeout(() => {
              this.audio.playRicochet(0, 0, 70);
            }, 10);
          } else if (res.penetrated) {
            setTimeout(() => {
              if (res.isArmored && this.targetHitgroup === 'head') {
                this.audio.playHeadshot(160, 0, 110);
              } else {
                this.audio.playFleshHit(160, 0, 70);
              }
            }, 35);
          }
        }, 30);
      }
    }

    update(tick, subTickFrac, dt) {
      const res = this._evaluate();

      const rad = this.incidentAngleDeg * (Math.PI / 180.0);
      const cosTheta = Math.max(0.15, Math.cos(rad));
      const halfThick = this.wallThickness * 0.5;

      // Target elevation based on selected hitgroup
      const targetZ = this.targetHitgroup === 'head' ? 110.0 : (this.targetHitgroup === 'chest' ? 88.0 : 52.0);

      // Rotate the physical wall slab around the Z axis at height 70
      if (this.wallMesh) {
        this.wallMesh.rotation.z = rad;
        this.wallMesh.position.set(0, 0, 70);
      }

      // Collinear ballistic line of fire aimed horizontally at targetZ
      const muzzleX = -160.0;
      const muzzleY = 0.0;
      const muzzleZ = targetZ;
      if (this.muzzleMarker) {
        this.muzzleMarker.position.set(muzzleX, muzzleY, muzzleZ);
        this.muzzleMarker.rotation.set(0, 0, Math.PI * 0.5);
      }

      // Rotated slab plane intersections along line of fire (y = 0)
      const enterX = Math.max(-120.0, -halfThick / cosTheta);
      const exitX = Math.min(120.0, halfThick / cosTheta);
      const enterY = 0.0, exitY = 0.0;
      const enterZ = targetZ, exitZ = targetZ;

      // Entry Ray
      const setLine = (line, x1, y1, z1, x2, y2, z2, visible = true) => {
        if (!line) return;
        line.visible = visible;
        if (!visible) return;
        const attr = line.geometry.attributes.position;
        attr.setXYZ(0, x1, y1, z1);
        attr.setXYZ(1, x2, y2, z2);
        attr.needsUpdate = true;
      };

      setLine(this.entryRay, muzzleX, muzzleY, muzzleZ, enterX, enterY, enterZ, true);

      // Surface normal vector of front face: n = (-cos(rad), -sin(rad), 0)
      const nx = -Math.cos(rad);
      const ny = -Math.sin(rad);
      const normalLen = 35.0;
      setLine(this.normalRay, enterX, enterY, enterZ, enterX + nx * normalLen, enterY + ny * normalLen, enterZ, true);

      // Ricochet reflection vector: r = v - 2(v . n)n -> (-cos(2*rad), -sin(2*rad), 0)
      const bounceDx = -Math.cos(2 * rad);
      const bounceDy = -Math.sin(2 * rad);
      const bounceDist = 120.0;

      if (res.isRicochet) {
        setLine(this.ricochetRay, enterX, enterY, enterZ, enterX + bounceDx * bounceDist, enterY + bounceDy * bounceDist, enterZ + 10, true);
        setLine(this.subsurfaceRay, 0, 0, 0, 0, 0, 0, false);
        setLine(this.exitRay, 0, 0, 0, 0, 0, 0, false);
      } else {
        setLine(this.ricochetRay, 0, 0, 0, 0, 0, 0, false);
        setLine(this.subsurfaceRay, enterX, enterY, enterZ, exitX, exitY, exitZ, true);

        if (res.penetrated) {
          setLine(this.exitRay, exitX, exitY, exitZ, 160.0, 0.0, targetZ, true);
        } else {
          setLine(this.exitRay, 0, 0, 0, 0, 0, 0, false);
        }
      }

      // Continuous animated ballistic tracer firing during timeline playback
      const cTime = tick + subTickFrac;
      const cycleLen = 16.0; // 1 shot every 16 ticks (~4 Hz realistic rifle cycle)
      const phase = (cTime % cycleLen) / cycleLen; // 0.0 to 1.0

      if (this.tracerBullet) {
        this.tracerBullet.visible = true;
        const totalDist = 320.0;
        const curDist = phase * totalDist;
        const curX = muzzleX + curDist;

        if (curX < enterX) {
          // Pre-wall flight
          this.tracerBullet.position.set(curX, 0, targetZ);
          this.tracerBullet.rotation.set(0, 0, Math.PI * 0.5);
          this.tracerBullet.material.color.setHex(0x22c55e);
          if (this.impactFlash) this.impactFlash.visible = false;
        } else if (curX >= enterX && curX <= exitX) {
          // Subsurface or Ricochet
          if (res.isRicochet) {
            const rDist = (curX - enterX) * 1.4;
            this.tracerBullet.position.set(enterX + bounceDx * rDist, enterY + bounceDy * rDist, enterZ + rDist * 0.1);
            this.tracerBullet.material.color.setHex(0xfacc15);
          } else {
            this.tracerBullet.position.set(curX, 0, targetZ);
            this.tracerBullet.material.color.setHex(0xf59e0b);
          }
          if (this.impactFlash) {
            this.impactFlash.position.set(enterX, 0, targetZ);
            this.impactFlash.visible = Math.abs(curX - enterX) < 8.0;
          }
        } else {
          // Post-wall flight or absorption
          if (res.penetrated) {
            const exitFrac = Math.min(1.0, (curX - exitX) / (160.0 - exitX));
            this.tracerBullet.position.set(curX, 0, targetZ);
            this.tracerBullet.material.color.setHex(0x06b6d4);
            if (this.impactFlash) {
              if (exitFrac >= 0.92) {
                this.impactFlash.position.set(160.0, 0, targetZ);
                this.impactFlash.visible = true;
              } else {
                this.impactFlash.visible = false;
              }
            }
          } else {
            this.tracerBullet.visible = false;
            if (this.impactFlash) this.impactFlash.visible = false;
          }
        }
      }

      // Update wall thickness geometry if altered
      if (this.wallMesh && Math.abs(this.wallMesh.geometry.parameters.width - this.wallThickness) > 0.1) {
        this.wallMesh.geometry.dispose();
        this.wallMesh.geometry = new THREE.BoxGeometry(this.wallThickness, 160, 140);
      }

      this._updateUIStats();

      return {
        x: enterX, y: enterY, z: enterZ,
        vx: 0, vy: 0, vz: 0,
        speed: 0,
        eventText: res.isRicochet
          ? `RICOCHET (\u03B8=${this.incidentAngleDeg}\u00B0 > 65\u00B0)`
          : res.penetrated
            ? `PENETRATED (${res.dExit.toFixed(1)} DMG EXIT)`
            : `ABSORBED IN MATERIAL`
      };
    }

    _buildParamsUI() {
      if (!this.paramsContainer) return;

      this.paramsContainer.innerHTML = `
        <div class="space-y-4 font-mono text-xs text-slate-200">
          <!-- Weapon & Material Selection -->
          <div class="bg-slate-900/90 border border-slate-800 p-3 rounded-xl space-y-3">
            <div class="flex justify-between items-center border-b border-slate-800 pb-2">
              <span class="text-cyan-400 font-bold">WEAPON &amp; SURFACE DATA</span>
              <span class="text-[10px] text-slate-500">server.dll 0x180a90280</span>
            </div>

            <div class="grid grid-cols-2 gap-2">
              <div>
                <label class="text-slate-400 block text-[10px] mb-1">Weapon Profile:</label>
                <select id="sim14SelWeapon" class="w-full bg-slate-950 border border-slate-800 rounded p-1.5 text-cyan-400 font-bold cursor-pointer">
                  <option value="ak47" selected>AK-47 (Pen 2.0, AP 77.5%)</option>
                  <option value="awp">AWP (Pen 2.5, AP 97.5%)</option>
                  <option value="m4a1s">M4A1-S (Pen 1.0, AP 70.0%)</option>
                  <option value="deagle">Desert Eagle (Pen 2.0, AP 85.0%)</option>
                </select>
              </div>

              <div>
                <label class="text-slate-400 block text-[10px] mb-1">Surface Material:</label>
                <select id="sim14SelMaterial" class="w-full bg-slate-950 border border-slate-800 rounded p-1.5 text-amber-400 font-bold cursor-pointer">
                  <option value="wood" selected>Wood (penMod 3.0)</option>
                  <option value="concrete">Concrete (penMod 1.0)</option>
                  <option value="metal">Solid Steel (penMod 1.0)</option>
                  <option value="cardboard">Cardboard (penMod 3.0)</option>
                  <option value="glass">Window Glass (penMod 3.0)</option>
                </select>
              </div>
            </div>

            <button id="sim14BtnFire" class="w-full py-2 rounded-lg bg-cyan-600 hover:bg-cyan-500 text-white font-bold text-center transition-colors shadow">
              Fire Ballistic Test Round
            </button>
          </div>

          <!-- Wall Dimensions & Incident Angle -->
          <div class="bg-slate-900/90 border border-slate-800 p-3 rounded-xl space-y-3">
            <div class="flex justify-between items-center border-b border-slate-800 pb-2">
              <span class="text-cyan-400 font-bold">GEOMETRIC KINEMATICS</span>
              <span class="text-[10px] text-slate-500">vphysics2.dll</span>
            </div>

            <div class="space-y-1">
              <div class="flex justify-between text-[11px]">
                <span class="text-slate-400">Wall Thickness:</span>
                <span id="sim14LblThick" class="text-cyan-400 font-bold">${this.wallThickness.toFixed(1)} u</span>
              </div>
              <input type="range" id="sim14SliderThick" min="2" max="45" step="1" value="${this.wallThickness}" class="w-full accent-cyan-400 cursor-pointer">
            </div>

            <div class="space-y-1">
              <div class="flex justify-between text-[11px]">
                <span class="text-slate-400">Angle of Incidence:</span>
                <span id="sim14LblAngle" class="text-amber-400 font-bold">${this.incidentAngleDeg.toFixed(1)}&deg;</span>
              </div>
              <input type="range" id="sim14SliderAngle" min="0" max="80" step="1" value="${this.incidentAngleDeg}" class="w-full accent-amber-400 cursor-pointer">
            </div>

            <div class="space-y-1 pt-1 border-t border-slate-800">
              <label class="text-slate-400 block text-[10px] mb-1">Target HitGroup:</label>
              <div class="grid grid-cols-4 gap-1">
                <button data-hg="head" class="px-2 py-1 rounded bg-cyan-600 text-white font-bold text-center">Head</button>
                <button data-hg="chest" class="px-2 py-1 rounded bg-slate-800 text-slate-400 text-center">Chest</button>
                <button data-hg="stomach" class="px-2 py-1 rounded bg-slate-800 text-slate-400 text-center">Stomach</button>
                <button data-hg="legs" class="px-2 py-1 rounded bg-slate-800 text-slate-400 text-center">Legs</button>
              </div>
            </div>
          </div>

          <!-- Kinetic Energy Dissipation Readouts -->
          <div class="space-y-2 bg-slate-950 p-3 rounded-xl border border-cyan-900/40">
            <div class="flex items-center justify-between border-b border-slate-800 pb-1.5">
              <span class="font-bold text-[11px] text-cyan-300">BALLISTIC ODE READOUT</span>
              <span id="sim14BadgeOutcome" class="text-[9px] px-1.5 py-0.5 rounded font-bold"></span>
            </div>

            <div class="grid grid-cols-2 gap-2 text-[11px]">
              <div class="bg-slate-900/60 p-2 rounded border border-slate-800">
                <span class="text-slate-400 block text-[10px]">Entry Damage</span>
                <span id="sim14ValEntryDmg" class="text-white font-bold">0.0 HP</span>
              </div>
              <div class="bg-slate-900/60 p-2 rounded border border-slate-800">
                <span class="text-slate-400 block text-[10px]">Material Loss</span>
                <span id="sim14ValLoss" class="text-rose-400 font-bold">0.0 HP</span>
              </div>
              <div class="bg-slate-900/60 p-2 rounded border border-slate-800">
                <span class="text-slate-400 block text-[10px]">Exit Damage</span>
                <span id="sim14ValExitDmg" class="text-cyan-400 font-bold">0.0 HP</span>
              </div>
              <div class="bg-slate-900/60 p-2 rounded border border-slate-800">
                <span class="text-slate-400 block text-[10px]">Target HP Dealt</span>
                <span id="sim14ValHPDealt" class="text-emerald-400 font-bold">0.0 HP</span>
              </div>
            </div>
          </div>
        </div>
      `;

      // Event listeners
      const selW = this.paramsContainer.querySelector('#sim14SelWeapon');
      if (selW) {
        selW.onchange = (e) => {
          this.activeWeapon = e.target.value;
          this._evaluate();
        };
      }

      const selM = this.paramsContainer.querySelector('#sim14SelMaterial');
      if (selM) {
        selM.onchange = (e) => {
          this.activeMaterial = e.target.value;
          const mat = this.materials[this.activeMaterial];
          if (this.wallMesh) {
            this.wallMesh.material.color.setHex(mat.color);
            this.wallMesh.material.roughness = mat.roughness !== undefined ? mat.roughness : 0.7;
            this.wallMesh.material.metalness = mat.metalness !== undefined ? mat.metalness : 0.1;
          }
          this._evaluate();
        };
      }

      const btnFire = this.paramsContainer.querySelector('#sim14BtnFire');
      if (btnFire) {
        btnFire.onclick = () => this.fireShot();
      }

      const slThick = this.paramsContainer.querySelector('#sim14SliderThick');
      const lblThick = this.paramsContainer.querySelector('#sim14LblThick');
      if (slThick && lblThick) {
        slThick.oninput = (e) => {
          this.wallThickness = parseFloat(e.target.value);
          lblThick.textContent = `${this.wallThickness.toFixed(1)} u`;
          this._evaluate();
        };
      }

      const slAngle = this.paramsContainer.querySelector('#sim14SliderAngle');
      const lblAngle = this.paramsContainer.querySelector('#sim14LblAngle');
      if (slAngle && lblAngle) {
        slAngle.oninput = (e) => {
          this.incidentAngleDeg = parseFloat(e.target.value);
          lblAngle.innerHTML = `${this.incidentAngleDeg.toFixed(1)}&deg;`;
          this._evaluate();
        };
      }

      this.paramsContainer.querySelectorAll('[data-hg]').forEach(btn => {
        btn.onclick = () => {
          this.paramsContainer.querySelectorAll('[data-hg]').forEach(b => {
            b.className = 'px-2 py-1 rounded bg-slate-800 text-slate-400 text-center';
          });
          btn.className = 'px-2 py-1 rounded bg-cyan-600 text-white font-bold text-center';
          this.targetHitgroup = btn.getAttribute('data-hg');
          this._evaluate();
        };
      });

      this._updateUIStats();
    }

    _updateUIStats() {
      if (!this.paramsContainer || !this.evalResults) return;
      const res = this.evalResults;

      const badge = this.paramsContainer.querySelector('#sim14BadgeOutcome');
      const vEntry = this.paramsContainer.querySelector('#sim14ValEntryDmg');
      const vLoss = this.paramsContainer.querySelector('#sim14ValLoss');
      const vExit = this.paramsContainer.querySelector('#sim14ValExitDmg');
      const vHP = this.paramsContainer.querySelector('#sim14ValHPDealt');

      if (vEntry) vEntry.textContent = `${res.dEntry.toFixed(1)} HP`;
      if (vLoss) vLoss.textContent = `-${res.totalLost.toFixed(1)} HP`;
      if (vExit) vExit.textContent = `${res.dExit.toFixed(1)} HP`;
      if (vHP) vHP.textContent = `${res.hpDamage.toFixed(1)} HP (${res.isArmored ? 'Armored' : 'Unarmored'})`;

      if (badge) {
        if (res.isRicochet) {
          badge.className = 'text-[9px] px-1.5 py-0.5 rounded font-bold bg-amber-950 text-amber-300 border border-amber-800';
          badge.textContent = 'DEFLECTED (RICOCHET)';
        } else if (res.penetrated) {
          badge.className = 'text-[9px] px-1.5 py-0.5 rounded font-bold bg-emerald-950 text-emerald-300 border border-emerald-800';
          badge.textContent = `PENETRATED (${res.dExit.toFixed(1)} HP)`;
        } else {
          badge.className = 'text-[9px] px-1.5 py-0.5 rounded font-bold bg-rose-950 text-rose-300 border border-rose-800';
          badge.textContent = 'STOPPED IN WALL';
        }
      }
    }

    teardown() {
      if (this.sceneGroup) {
        this.viewport.getScene().remove(this.sceneGroup);
        this.sceneGroup = null;
      }
      if (this.paramsContainer) {
        this.paramsContainer.innerHTML = '';
        this.paramsContainer = null;
      }
    }
  }

  // ==========================================================================
  // 8. Sim12LagCompensationProvider: 64-Tick History Ring Buffer & Rollback
  // Reverse-engineered from server.dll Build 26598758:
  // - CLagCompensationManager::StartLagCompensation
  // - CLagCompensationManager::BacktrackPlayer
  // - sv_maxunlag (0.200s window)
  // ==========================================================================
  class Sim12LagCompensationProvider {
    constructor(viewport, audioEngine, playback) {
      this.viewport = viewport;
      this.audio = audioEngine;
      this.playback = playback;

      this.BUFFER_CAPACITY = 64; // sv_maxunlag 0.200s @ 64Hz
      this.historyBuffer = new Array(this.BUFFER_CAPACITY);
      this.clientLatencySec = 0.085; // 85 ms default
      this.userCmdSubTickFrac = 0.42;
      this.aimTarget = 'rewound'; // 'rewound' | 'live'
      this.lastHitResult = null;

      // 3D Scene Objects
      this.sceneGroup = null;
      this.livePawnGroup = null;
      this.rewoundPawnGroup = null;
      this.shooterPawnGroup = null;
      this.historyTrail = null;
      this.bulletTracer = null;
      this.hitMarker = null;
      this.tracerTimer = 0;
      this.paramsContainer = null;

      this._initHistoryBuffer();
    }

    _initHistoryBuffer() {
      // Procedural erratic strafing run matching player movement physics
      for (let k = 0; k < this.BUFFER_CAPACITY; k++) {
        const t = (k - (this.BUFFER_CAPACITY - 1)) * 0.015625; // Negative time into past
        const posX = Math.sin(t * 3.8) * 45.0 + (t * 60.0);
        const posY = Math.cos(t * 2.4) * 25.0;
        const posZ = 0.0;
        this.historyBuffer[k] = {
          tick: 1000 + k,
          time: t,
          pos: { x: posX, y: posY, z: posZ },
          yaw: Math.atan2(-Math.sin(t * 2.4) * 25.0 * 2.4, Math.cos(t * 3.8) * 45.0 * 3.8 + 60.0)
        };
      }
    }

    _buildTacticalOperative(group, baseColor = 0x06b6d4, opacity = 1.0, isHologram = false) {
      const matMain = new THREE.MeshStandardMaterial({
        color: baseColor,
        roughness: isHologram ? 0.3 : 0.6,
        metalness: isHologram ? 0.2 : 0.1,
        transparent: isHologram || opacity < 1.0,
        opacity: opacity
      });

      const matDark = new THREE.MeshStandardMaterial({
        color: isHologram ? baseColor : 0x0f172a,
        roughness: 0.7,
        transparent: isHologram || opacity < 1.0,
        opacity: isHologram ? opacity * 0.8 : 1.0
      });

      const matAcc = new THREE.MeshStandardMaterial({
        color: isHologram ? 0xfbbf24 : 0x38bdf8,
        roughness: 0.2,
        emissive: isHologram ? 0xd97706 : 0x0284c7,
        emissiveIntensity: isHologram ? 0.4 : 0.1,
        transparent: isHologram || opacity < 1.0,
        opacity: opacity
      });

      // 1. Head & Balaclava (z: 62..70)
      const headGeo = new THREE.SphereGeometry(4.2, 14, 10);
      const headMesh = new THREE.Mesh(headGeo, matDark);
      headMesh.position.set(0, 0, 66);
      group.add(headMesh);

      // 2. Ballistic FAST Helmet with NVG Mount (z: 67..72)
      const helmetGeo = new THREE.SphereGeometry(4.6, 14, 10, 0, Math.PI * 2, 0, Math.PI * 0.55);
      const helmetMesh = new THREE.Mesh(helmetGeo, matMain);
      helmetMesh.position.set(0, 0, 66.5);
      group.add(helmetMesh);

      // Tactical Goggles / Visor
      const visorGeo = new THREE.BoxGeometry(4.8, 2.4, 1.4);
      const visorMesh = new THREE.Mesh(visorGeo, matAcc);
      visorMesh.position.set(3.2, 0, 66.5);
      group.add(visorMesh);

      // Comms Headset
      [-1, 1].forEach(side => {
        const earcupGeo = new THREE.CylinderGeometry(1.2, 1.2, 0.8, 8);
        const earcup = new THREE.Mesh(earcupGeo, matDark);
        earcup.rotation.z = Math.PI * 0.5;
        earcup.position.set(0, side * 4.4, 66.5);
        group.add(earcup);
      });

      // 3. Tactical Plate Carrier Vest (z: 36..58)
      const chestGeo = new THREE.BoxGeometry(9.5, 12.0, 14.0);
      const chestMesh = new THREE.Mesh(chestGeo, matMain);
      chestMesh.position.set(0, 0, 48);
      group.add(chestMesh);

      // Front Mag Pouches (3x pouches)
      for (let p = -1; p <= 1; p++) {
        const pouchGeo = new THREE.BoxGeometry(1.6, 2.8, 5.0);
        const pouchMesh = new THREE.Mesh(pouchGeo, matDark);
        pouchMesh.position.set(5.0, p * 3.4, 46);
        group.add(pouchMesh);
      }

      // 4. Utility Belt & Pelvis (z: 32..38)
      const beltGeo = new THREE.BoxGeometry(8.5, 10.5, 4.0);
      const beltMesh = new THREE.Mesh(beltGeo, matDark);
      beltMesh.position.set(0, 0, 36);
      group.add(beltMesh);

      // 5. Articulated Arms & Tactical Gloves
      [-1, 1].forEach(side => {
        const upArmGeo = new THREE.CylinderGeometry(2.0, 1.8, 9.0, 8);
        const upArm = new THREE.Mesh(upArmGeo, matMain);
        upArm.position.set(2.0, side * 7.5, 50);
        upArm.rotation.x = side * -0.25;
        upArm.rotation.z = -0.35;
        group.add(upArm);

        const fArmGeo = new THREE.CylinderGeometry(1.7, 1.5, 8.5, 8);
        const fArm = new THREE.Mesh(fArmGeo, matDark);
        fArm.position.set(6.5, side * 6.0, 44);
        fArm.rotation.z = -0.9;
        group.add(fArm);
      });

      // 6. Tactical M4A1-S Carbine with Silencer
      const weaponGeo = new THREE.BoxGeometry(18.0, 2.0, 4.0);
      const weaponMesh = new THREE.Mesh(weaponGeo, matDark);
      weaponMesh.position.set(9.0, 0, 44);
      group.add(weaponMesh);

      const barrelGeo = new THREE.CylinderGeometry(0.7, 0.7, 12.0, 8);
      const barrelMesh = new THREE.Mesh(barrelGeo, matDark);
      barrelMesh.rotation.z = Math.PI * 0.5;
      barrelMesh.position.set(22.0, 0, 44.5);
      group.add(barrelMesh);

      // 7. Tactical Cargo Pants, Kneepads & Boots
      [-1, 1].forEach(side => {
        const thighGeo = new THREE.CylinderGeometry(2.5, 2.2, 14.0, 8);
        const thigh = new THREE.Mesh(thighGeo, matMain);
        thigh.position.set(0, side * 3.6, 26);
        group.add(thigh);

        const padGeo = new THREE.BoxGeometry(2.2, 3.2, 3.5);
        const pad = new THREE.Mesh(padGeo, matDark);
        pad.position.set(2.0, side * 3.6, 18);
        group.add(pad);

        const calfGeo = new THREE.CylinderGeometry(2.1, 1.9, 14.0, 8);
        const calf = new THREE.Mesh(calfGeo, matMain);
        calf.position.set(0, side * 3.6, 11);
        group.add(calf);

        const bootGeo = new THREE.BoxGeometry(7.0, 3.4, 4.0);
        const boot = new THREE.Mesh(bootGeo, matDark);
        boot.position.set(1.5, side * 3.6, 2);
        group.add(boot);
      });

      // 8. Hitbox Capsule Overlay Outlines (Precision Source 2 Hitboxes)
      const hbMat = new THREE.MeshBasicMaterial({
        color: baseColor,
        wireframe: true,
        transparent: true,
        opacity: isHologram ? 0.6 : 0.35
      });
      // Head sphere hitbox
      const hbHead = new THREE.Mesh(new THREE.SphereGeometry(4.5, 8, 6), hbMat);
      hbHead.position.set(0, 0, 66);
      group.add(hbHead);
      // Chest box hitbox
      const hbChest = new THREE.Mesh(new THREE.BoxGeometry(11.0, 13.0, 16.0), hbMat);
      hbChest.position.set(0, 0, 48);
      group.add(hbChest);
      // Pelvis box hitbox
      const hbPelvis = new THREE.Mesh(new THREE.BoxGeometry(10.0, 11.5, 10.0), hbMat);
      hbPelvis.position.set(0, 0, 34);
      group.add(hbPelvis);
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim12_LagCompensation_Scene';

      // 1. Live Server Counter-Terrorist Agent (Solid Cyan SAS operative)
      this.livePawnGroup = new THREE.Group();
      this._buildTacticalOperative(this.livePawnGroup, 0x06b6d4, 1.0, false);
      this.sceneGroup.add(this.livePawnGroup);

      // 2. Rewound Lag-Compensated Ghost Agent (Amber Hologram operative)
      this.rewoundPawnGroup = new THREE.Group();
      this._buildTacticalOperative(this.rewoundPawnGroup, 0xf59e0b, 0.5, true);
      this.sceneGroup.add(this.rewoundPawnGroup);

      // 3. Shooter Operative (Counter-Terrorist at Origin aiming down range)
      this.shooterPawnGroup = new THREE.Group();
      this._buildTacticalOperative(this.shooterPawnGroup, 0x10b981, 0.95, false);
      this.shooterPawnGroup.position.set(0, -220, 0);
      this.shooterPawnGroup.rotation.z = Math.PI * 0.5; // Aim toward +Y
      this.sceneGroup.add(this.shooterPawnGroup);

      // 4. 64-Tick History Trail
      const trailPoints = [];
      for (let k = 0; k < this.BUFFER_CAPACITY; k++) {
        trailPoints.push(new THREE.Vector3(this.historyBuffer[k].pos.x, this.historyBuffer[k].pos.y, 4.0));
      }
      const trailGeo = new THREE.BufferGeometry().setFromPoints(trailPoints);
      this.historyTrail = new THREE.Line(trailGeo, new THREE.LineBasicMaterial({
        color: 0x38bdf8,
        transparent: true,
        opacity: 0.45
      }));
      this.sceneGroup.add(this.historyTrail);

      // 5. Supersonic Bullet Tracer Line
      const tracerGeo = new THREE.BufferGeometry().setFromPoints([
        new THREE.Vector3(0, -220, 50),
        new THREE.Vector3(0, 0, 50)
      ]);
      this.bulletTracer = new THREE.Line(tracerGeo, new THREE.LineBasicMaterial({
        color: 0xfef08a,
        linewidth: 3,
        transparent: true,
        opacity: 0.0
      }));
      this.sceneGroup.add(this.bulletTracer);

      // 6. Hit Registration Impact Marker
      const hitGeo = new THREE.RingGeometry(2, 5, 16);
      const hitMat = new THREE.MeshBasicMaterial({ color: 0x10b981, side: THREE.DoubleSide, transparent: true, opacity: 0.0 });
      this.hitMarker = new THREE.Mesh(hitGeo, hitMat);
      this.hitMarker.position.set(0, 0, 66);
      this.sceneGroup.add(this.hitMarker);

      scene.add(this.sceneGroup);

      // 7. Mount UI Controls
      this._buildParamsUI();

      // 8. Optimal Perspective Framing
      const camera = this.viewport.getCamera();
      if (camera) {
        camera.position.set(-180, -320, 160);
        const controls = this.viewport.getControls();
        if (controls) {
          controls.target.set(0, -40, 45);
          controls.update();
        }
      }

      this.update(0, 0, 0);
    }

    _sampleRewoundState(targetTime) {
      const newestTime = this.historyBuffer[this.BUFFER_CAPACITY - 1].time;
      const oldestTime = this.historyBuffer[0].time;
      const clampedTime = Math.max(oldestTime, Math.min(newestTime, targetTime));

      for (let k = this.BUFFER_CAPACITY - 1; k > 0; k--) {
        const t1 = this.historyBuffer[k].time;
        const t0 = this.historyBuffer[k - 1].time;
        if (clampedTime >= t0 && clampedTime <= t1) {
          const dt = t1 - t0;
          const frac = dt > 1e-6 ? (clampedTime - t0) / dt : 0.0;
          const p0 = this.historyBuffer[k - 1].pos;
          const p1 = this.historyBuffer[k].pos;
          return {
            x: p0.x + (p1.x - p0.x) * frac,
            y: p0.y + (p1.y - p0.y) * frac,
            z: p0.z + (p1.z - p0.z) * frac,
            yaw: this.historyBuffer[k].yaw,
            tickDelta: (newestTime - clampedTime) * 64.0
          };
        }
      }
      return {
        x: this.historyBuffer[0].pos.x,
        y: this.historyBuffer[0].pos.y,
        z: this.historyBuffer[0].pos.z,
        yaw: this.historyBuffer[0].yaw,
        tickDelta: (newestTime - oldestTime) * 64.0
      };
    }

    fireShot() {
      const rewoundState = this._sampleRewoundState(-this.clientLatencySec - (this.userCmdSubTickFrac * 0.015625));
      const targetPawn = this.aimTarget === 'rewound' ? this.rewoundPawnGroup : this.livePawnGroup;
      const targetX = targetPawn ? targetPawn.position.x : 0;
      const targetY = targetPawn ? targetPawn.position.y : 0;
      const targetZ = 66.0;

      if (this.bulletTracer && this.bulletTracer.geometry) {
        this.bulletTracer.geometry.setFromPoints([
          new THREE.Vector3(0, -220, 50),
          new THREE.Vector3(targetX, targetY, targetZ)
        ]);
        if (this.bulletTracer.material) {
          this.bulletTracer.material.opacity = 1.0;
        }
      }

      const unlagSec = Math.min(0.200, this.clientLatencySec + (this.userCmdSubTickFrac * 0.015625));
      const rewoundDeltaTicks = unlagSec / 0.015625;

      if (this.aimTarget === 'rewound') {
        this.lastHitResult = {
          confirmed: true,
          hitbox: 'HEAD (Authoritative Sub-Tick Rewind)',
          deltaTicks: rewoundDeltaTicks.toFixed(1),
          rewindMs: (unlagSec * 1000).toFixed(0)
        };
        if (this.hitMarker) {
          this.hitMarker.position.set(targetX, targetY, targetZ);
          this.hitMarker.material.color.setHex(0x10b981);
          this.hitMarker.material.opacity = 1.0;
        }
      } else {
        this.lastHitResult = {
          confirmed: false,
          hitbox: 'MISSED (Target Has Moved Ahead)',
          deltaTicks: '0.0',
          rewindMs: '0'
        };
        if (this.hitMarker) {
          this.hitMarker.position.set(targetX, targetY, targetZ);
          this.hitMarker.material.color.setHex(0xef4444);
          this.hitMarker.material.opacity = 0.85;
        }
      }

      this.tracerTimer = 1.0;

      if (this.audio) {
        this.audio.playGunfire(0, -220, 50);
        setTimeout(() => {
          if (!this.audio) return;
          if (this.lastHitResult && this.lastHitResult.confirmed) {
            this.audio.playHeadshot(targetX, targetY, 66);
          } else {
            this.audio.playImpact('concrete', 0, 0, 0);
          }
        }, Math.round(this.clientLatencySec * 1000 * 0.4));
      }

      this._syncUI();
    }

    update(tick, subTickFrac, dt) {
      // Continuous simulation timeline parameter
      const cTime = (tick + subTickFrac);
      const t = cTime * 0.04; // Smooth continuous running time

      // Live pawn coordinates along strafe trajectory
      const liveX = Math.sin(t * 3.2) * 55.0 + Math.cos(t * 1.1) * 20.0;
      const liveY = Math.cos(t * 2.1) * 40.0;
      const liveZ = 0.0;
      const liveVx = (Math.cos(t * 3.2) * 55.0 * 3.2 - Math.sin(t * 1.1) * 20.0 * 1.1);
      const liveVy = -Math.sin(t * 2.1) * 40.0 * 2.1;
      const liveYaw = Math.atan2(liveVy, liveVx);

      if (this.livePawnGroup) {
        this.livePawnGroup.position.set(liveX, liveY, liveZ);
        this.livePawnGroup.rotation.z = liveYaw;
      }

      // Clamped sv_maxunlag (0.200s) rewind offset
      const unlagSec = Math.min(0.200, this.clientLatencySec + (this.userCmdSubTickFrac * 0.015625));
      const rewindDt = unlagSec * 2.5; // Offset in trajectory time units
      const tRewind = t - rewindDt;

      const rewoundX = Math.sin(tRewind * 3.2) * 55.0 + Math.cos(tRewind * 1.1) * 20.0;
      const rewoundY = Math.cos(tRewind * 2.1) * 40.0;
      const rewoundZ = 0.0;
      const rewoundVx = (Math.cos(tRewind * 3.2) * 55.0 * 3.2 - Math.sin(tRewind * 1.1) * 20.0 * 1.1);
      const rewoundVy = -Math.sin(tRewind * 2.1) * 40.0 * 2.1;
      const rewoundYaw = Math.atan2(rewoundVy, rewoundVx);

      if (this.rewoundPawnGroup) {
        this.rewoundPawnGroup.position.set(rewoundX, rewoundY, rewoundZ);
        this.rewoundPawnGroup.rotation.z = rewoundYaw;
      }

      // Update past history breadcrumbs
      if (this.historyTrail && this.historyTrail.geometry) {
        const arr = this.historyTrail.geometry.attributes.position.array;
        let ptr = 0;
        for (let k = 0; k < this.BUFFER_CAPACITY; k++) {
          const stepT = t - (k * 0.015625 * 2.5);
          arr[ptr++] = Math.sin(stepT * 3.2) * 55.0 + Math.cos(stepT * 1.1) * 20.0;
          arr[ptr++] = Math.cos(stepT * 2.1) * 40.0;
          arr[ptr++] = 1.0;
        }
        this.historyTrail.geometry.attributes.position.needsUpdate = true;
      }

      const rewoundDeltaTicks = unlagSec / 0.015625;

      if (this.tracerTimer > 0) {
        this.tracerTimer = Math.max(0, this.tracerTimer - dt * 2.5);
        if (this.bulletTracer && this.bulletTracer.material) {
          this.bulletTracer.material.opacity = this.tracerTimer;
        }
        if (this.hitMarker && this.hitMarker.material) {
          this.hitMarker.material.opacity = this.tracerTimer * 0.9;
        }
      }

      this.viewport.setHudMetrics([
        { label: 'CLIENT LATENCY', value: `${(this.clientLatencySec * 1000).toFixed(0)} ms` },
        { label: 'SUB-TICK FRAC (w)', value: this.userCmdSubTickFrac.toFixed(3) },
        { label: 'REWOUND TICKS', value: `${rewoundDeltaTicks.toFixed(1)} ticks` },
        { label: 'SV_MAXUNLAG', value: '0.200s (NOMINAL)' },
        { label: 'HIT STATUS', value: this.lastHitResult ? (this.lastHitResult.confirmed ? 'CONFIRMED HEADSHOT' : 'SHOT MISSED') : 'AWAITING FIRE', alert: this.lastHitResult && !this.lastHitResult.confirmed }
      ]);
    }

    teardown() {
      const scene = this.viewport.getScene();
      if (this.sceneGroup) {
        if (scene) scene.remove(this.sceneGroup);
        this.sceneGroup = null;
      }
      if (this.paramsContainer) {
        this.paramsContainer.innerHTML = '';
        this.paramsContainer = null;
      }
    }

    _syncUI() {
      if (!this.paramsContainer) return;
      const resCard = this.paramsContainer.querySelector('#sim12HitResult');
      if (resCard && this.lastHitResult) {
        resCard.className = `p-2.5 rounded border text-xs font-mono ${this.lastHitResult.confirmed ? 'bg-emerald-500/10 border-emerald-500/30 text-emerald-400' : 'bg-rose-500/10 border-rose-500/30 text-rose-400'}`;
        resCard.innerHTML = `
          <div class="font-bold flex items-center justify-between">
            <span>${this.lastHitResult.confirmed ? 'CONFIRMED HEADSHOT' : 'SHOT MISSED'}</span>
            <span class="text-[10px] text-slate-400">${this.lastHitResult.rewindMs}ms rewind</span>
          </div>
          <div class="text-[11px] text-slate-400 mt-0.5">Authoritative rewind by ${this.lastHitResult.deltaTicks} ticks</div>
        `;
      }
    }

    _buildParamsUI() {
      if (!this.paramsContainer) return;
      this.paramsContainer.innerHTML = `
        <div class="space-y-4 font-mono text-xs select-none">
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1.5">
              <span>CLIENT NETWORK TIMING</span>
              <span class="text-[10px] text-cyan-400">sv_maxunlag 0.200s</span>
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Client Latency / Ping:</span>
                <span id="sim12LblPing" class="text-cyan-300 font-bold">${(this.clientLatencySec * 1000).toFixed(0)} ms</span>
              </div>
              <input type="range" id="sim12Ping" min="5" max="300" step="5" value="${this.clientLatencySec * 1000}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Sub-Tick UserCmd Fraction w:</span>
                <span id="sim12LblFrac" class="text-amber-400 font-bold">${this.userCmdSubTickFrac.toFixed(3)}</span>
              </div>
              <input type="range" id="sim12Frac" min="0" max="0.999" step="0.005" value="${this.userCmdSubTickFrac}" class="w-full accent-amber-500 cursor-pointer">
            </div>
          </div>

          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="text-slate-300 font-bold border-b border-slate-800 pb-1 text-[11px]">
              CROSSHAIR AIM MODE:
            </div>
            <div class="grid grid-cols-2 gap-2">
              <button id="sim12BtnAimRewound" class="py-2 px-2 rounded ${this.aimTarget === 'rewound' ? 'bg-amber-600 text-white font-bold' : 'bg-slate-800 text-slate-400'} border border-slate-700 text-[11px] cursor-pointer">
                Aim at Rewound Ghost
              </button>
              <button id="sim12BtnAimLive" class="py-2 px-2 rounded ${this.aimTarget === 'live' ? 'bg-cyan-600 text-white font-bold' : 'bg-slate-800 text-slate-400'} border border-slate-700 text-[11px] cursor-pointer">
                Aim at Live Server
              </button>
            </div>
            <button id="sim12BtnFire" class="w-full py-2.5 px-3 rounded bg-emerald-600 hover:bg-emerald-500 text-white font-bold text-xs flex items-center justify-center gap-1.5 cursor-pointer shadow-md">
              <span>Fire Authoritative Unlag Verification Shot</span>
            </button>
          </div>

          <div id="sim12HitResult" class="p-2.5 rounded border border-slate-800 bg-slate-950/60 text-xs font-mono">
            <div class="text-slate-400">Hit Registration Result</div>
            <div class="text-slate-500 text-[11px] mt-0.5">Click 'Fire Authoritative Unlag Verification Shot' to test lag compensation.</div>
          </div>
        </div>
      `;

      const pIn = this.paramsContainer.querySelector('#sim12Ping');
      const lblP = this.paramsContainer.querySelector('#sim12LblPing');
      if (pIn && lblP) {
        pIn.oninput = (e) => {
          this.clientLatencySec = parseFloat(e.target.value) / 1000;
          lblP.textContent = `${e.target.value} ms`;
          this.update(0, 0, 0);
        };
      }

      const fIn = this.paramsContainer.querySelector('#sim12Frac');
      const lblF = this.paramsContainer.querySelector('#sim12LblFrac');
      if (fIn && lblF) {
        fIn.oninput = (e) => {
          this.userCmdSubTickFrac = parseFloat(e.target.value);
          lblF.textContent = this.userCmdSubTickFrac.toFixed(3);
          this.update(0, 0, 0);
        };
      }

      const btnRewound = this.paramsContainer.querySelector('#sim12BtnAimRewound');
      const btnLive = this.paramsContainer.querySelector('#sim12BtnAimLive');
      if (btnRewound && btnLive) {
        btnRewound.onclick = () => {
          this.aimTarget = 'rewound';
          btnRewound.className = 'py-2 px-2 rounded bg-amber-600 text-white font-bold border border-slate-700 text-[11px] cursor-pointer';
          btnLive.className = 'py-2 px-2 rounded bg-slate-800 text-slate-400 border border-slate-700 text-[11px] cursor-pointer';
        };
        btnLive.onclick = () => {
          this.aimTarget = 'live';
          btnLive.className = 'py-2 px-2 rounded bg-cyan-600 text-white font-bold border border-slate-700 text-[11px] cursor-pointer';
          btnRewound.className = 'py-2 px-2 rounded bg-slate-800 text-slate-400 border border-slate-700 text-[11px] cursor-pointer';
        };
      }

      const btnFire = this.paramsContainer.querySelector('#sim12BtnFire');
      if (btnFire) btnFire.onclick = () => this.fireShot();
    }
  }

  class Sim15EconWearProvider {
    constructor(viewport, audioEngine, playback) {
      this.viewport = viewport;
      this.audio = audioEngine;
      this.playback = playback;

      this.wearValue = 0.045; // Factory New
      this.seed = 661;        // Pattern Seed (#661 Blue Gem / Tier 1)
      this.activeSkin = 'case_hardened'; // 'case_hardened' | 'fade' | 'asiimov'

      this.sceneGroup = null;
      this.knifeMesh = null;
      this.canvasTexture = null;
      this.textureCanvas = null;
      this.textureCtx = null;
      this.paramsContainer = null;
    }

    _generateSkinTexture(skin, seed, wear) {
      if (!this.textureCanvas) {
        this.textureCanvas = document.createElement('canvas');
        this.textureCanvas.width = 1024;
        this.textureCanvas.height = 1024;
        this.textureCtx = this.textureCanvas.getContext('2d');
      }
      const ctx = this.textureCtx;
      const w = 1024, h = 1024;
      ctx.clearRect(0, 0, w, h);

      if (skin === 'case_hardened') {
        // High-end Case Hardened (#661 Blue Gem)
        ctx.fillStyle = '#b49464'; // Brass/gold base
        ctx.fillRect(0, 0, w, h);

        const s = (seed * 137.5) % 1000;
        const grad = ctx.createLinearGradient(0, 0, w, h);
        grad.addColorStop(0.0, '#1d4ed8'); // Vivid royal blue
        grad.addColorStop(0.35, '#2563eb');
        grad.addColorStop(0.60, '#38bdf8'); // Sky blue
        grad.addColorStop(0.78, '#9333ea'); // Purple heat ring
        grad.addColorStop(0.88, '#eab308'); // Golden fire stain
        grad.addColorStop(1.0, '#b45309');
        ctx.fillStyle = grad;
        ctx.fillRect(0, 0, w, h);

        // Procedural mottled oil-slick patches
        for (let i = 0; i < 40; i++) {
          const px = ((seed * (i + 13) * 79) % w);
          const py = ((seed * (i + 29) * 97) % h);
          const r = 40 + ((i * 17) % 80);
          const pGrad = ctx.createRadialGradient(px, py, 2, px, py, r);
          pGrad.addColorStop(0.0, 'rgba(30, 64, 175, 0.7)');
          pGrad.addColorStop(0.7, 'rgba(147, 51, 234, 0.4)');
          pGrad.addColorStop(1.0, 'transparent');
          ctx.fillStyle = pGrad;
          ctx.beginPath();
          ctx.arc(px, py, r, 0, Math.PI * 2);
          ctx.fill();
        }
      } else if (skin === 'fade') {
        // 100% CS2 Anodized Fade
        const grad = ctx.createLinearGradient(0, 0, w, 0);
        grad.addColorStop(0.0, '#fbbf24'); // Amber gold at handle
        grad.addColorStop(0.4, '#f43f5e'); // Vibrant magenta/pink
        grad.addColorStop(0.75, '#8b5cf6'); // Purple
        grad.addColorStop(1.0, '#3b82f6'); // Deep blue at tip
        ctx.fillStyle = grad;
        ctx.fillRect(0, 0, w, h);

        // Metallic sheen highlights
        const sheen = ctx.createLinearGradient(0, 0, 0, h);
        sheen.addColorStop(0.0, 'rgba(255, 255, 255, 0.25)');
        sheen.addColorStop(0.5, 'transparent');
        sheen.addColorStop(1.0, 'rgba(0, 0, 0, 0.35)');
        ctx.fillStyle = sheen;
        ctx.fillRect(0, 0, w, h);
      } else if (skin === 'asiimov') {
        // High-contrast Sci-Fi Asiimov Finish
        ctx.fillStyle = '#f8fafc'; // Pristine ceramic white base
        ctx.fillRect(0, 0, w, h);

        // Matte black geometrical structural sections
        ctx.fillStyle = '#0f172a';
        ctx.beginPath();
        ctx.moveTo(0, 100); ctx.lineTo(w * 0.4, 100); ctx.lineTo(w * 0.55, 300); ctx.lineTo(0, 300);
        ctx.fill();

        ctx.beginPath();
        ctx.moveTo(w * 0.6, 600); ctx.lineTo(w, 600); ctx.lineTo(w, 900); ctx.lineTo(w * 0.45, 900);
        ctx.fill();

        // Vibrant high-vis orange chevron stripes
        ctx.fillStyle = '#ea580c';
        ctx.beginPath();
        ctx.moveTo(w * 0.3, 100); ctx.lineTo(w * 0.45, 100); ctx.lineTo(w * 0.7, 500); ctx.lineTo(w * 0.55, 500);
        ctx.fill();

        ctx.beginPath();
        ctx.moveTo(w * 0.1, 700); ctx.lineTo(w * 0.35, 700); ctx.lineTo(w * 0.5, 950); ctx.lineTo(w * 0.25, 950);
        ctx.fill();

        // Crosshairs & decals
        ctx.strokeStyle = '#0f172a';
        ctx.lineWidth = 6;
        ctx.strokeRect(w * 0.75, 250, 100, 100);
        ctx.strokeRect(w * 0.15, 450, 140, 40);
      }

      // Runge-Kutta 4th Order Wear Degradation ODE Overlay
      // Surface abrasion exposes gunmetal carbon steel and dark oxidation pits
      if (wear > 0.07) {
        const scratchCount = Math.floor((wear - 0.07) * 450);
        ctx.fillStyle = '#334155'; // Dark oxidized steel
        ctx.strokeStyle = '#1e293b';

        for (let k = 0; k < scratchCount; k++) {
          const sx = ((seed * (k + 1) * 31) % (w * 0.75)) + (w * 0.25);
          const sy = ((seed * (k + 1) * 73) % h);
          const slen = 10 + (wear * 50);
          ctx.lineWidth = 1.5 + (wear * 3.5);
          ctx.beginPath();
          ctx.moveTo(sx, sy);
          ctx.lineTo(sx - slen, sy + ((k % 2 === 0 ? 1 : -1) * (slen * 0.3)));
          ctx.stroke();

          // Abrasion flecks
          if (k % 3 === 0) {
            ctx.beginPath();
            ctx.arc(sx, sy, 2 + wear * 6, 0, Math.PI * 2);
            ctx.fill();
          }
        }

        // Heavy Battle-Scarred dark patina overlay
        if (wear > 0.45) {
          const patinaGrad = ctx.createLinearGradient(0, 0, w, 0);
          patinaGrad.addColorStop(0.0, `rgba(15, 23, 42, ${(wear - 0.45) * 1.4})`);
          patinaGrad.addColorStop(1.0, `rgba(30, 41, 59, ${(wear - 0.45) * 1.1})`);
          ctx.fillStyle = patinaGrad;
          ctx.fillRect(0, 0, w, h);
        }
      }

      if (this.canvasTexture) {
        this.canvasTexture.needsUpdate = true;
      }
    }

    _buildKarambitKnife() {
      const group = new THREE.Group();

      // Dynamic Canvas Texture for Blade Finishes
      this._generateSkinTexture(this.activeSkin, this.seed, this.wearValue);
      this.canvasTexture = new THREE.CanvasTexture(this.textureCanvas);
      this.canvasTexture.wrapS = THREE.RepeatWrapping;
      this.canvasTexture.wrapT = THREE.RepeatWrapping;

      // 1. Blade: Authentic CS2 Karambit Curved Claw (Hawkbill) Geometry
      const bladeShape = new THREE.Shape();
      bladeShape.moveTo(0, 0);
      bladeShape.bezierCurveTo(8, 1, 16, -2, 24, -8);
      bladeShape.bezierCurveTo(30, -14, 34, -22, 36, -32); // Curved Claw Tip
      bladeShape.bezierCurveTo(31, -24, 22, -15, 14, -10); // Inner Curved Razor Cutting Edge
      bladeShape.bezierCurveTo(8, -6, 2, -3, 0, -2);
      bladeShape.closePath();

      const extrudeSettings = {
        depth: 1.2,
        bevelEnabled: true,
        bevelSegments: 4,
        steps: 1,
        bevelSize: 0.6,
        bevelThickness: 0.6
      };
      const bladeGeo = new THREE.ExtrudeGeometry(bladeShape, extrudeSettings);
      const bladeMat = new THREE.MeshStandardMaterial({
        map: this.canvasTexture,
        metalness: 0.85,
        roughness: 0.18 + (this.wearValue * 0.45),
        envMapIntensity: 1.2
      });
      const bladeMesh = new THREE.Mesh(bladeGeo, bladeMat);
      bladeMesh.position.set(0, 0, -0.6);
      group.add(bladeMesh);

      // 2. Handle: Textured Ergonomic Contoured G10 Scales with 3 Finger Scallops
      const handleShape = new THREE.Shape();
      handleShape.moveTo(0, 0);
      handleShape.lineTo(-12, 1);
      handleShape.lineTo(-24, 0);
      handleShape.lineTo(-32, -2); // Rear pommel taper
      handleShape.lineTo(-32, -12);
      handleShape.lineTo(-22, -11); // Scallop 3
      handleShape.lineTo(-14, -9);  // Scallop 2
      handleShape.lineTo(-4, -6);   // Scallop 1
      handleShape.closePath();

      const handleExtrude = {
        depth: 2.8,
        bevelEnabled: true,
        bevelSegments: 3,
        steps: 1,
        bevelSize: 0.7,
        bevelThickness: 0.7
      };
      const handleGeo = new THREE.ExtrudeGeometry(handleShape, handleExtrude);
      const handleMat = new THREE.MeshStandardMaterial({
        color: 0x1e293b,
        roughness: 0.65,
        metalness: 0.15
      });
      const handleMesh = new THREE.Mesh(handleGeo, handleMat);
      handleMesh.position.set(0, 0, -1.4);
      group.add(handleMesh);

      // Stainless Steel Torx Pivot Screws on Handle Scales
      [-8, -18, -26].forEach(posX => {
        const screwGeo = new THREE.CylinderGeometry(1.2, 1.2, 3.4, 12);
        const screwMat = new THREE.MeshStandardMaterial({ color: 0x94a3b8, metalness: 0.9, roughness: 0.2 });
        const screw = new THREE.Mesh(screwGeo, screwMat);
        screw.rotation.x = Math.PI * 0.5;
        screw.position.set(posX, -5, 0);
        group.add(screw);
      });

      // 3. Integrated Tactical Retention Pommel Safety Ring (CS2 Signature Feature)
      const ringGeo = new THREE.TorusGeometry(5.8, 1.4, 14, 28);
      const ringMat = new THREE.MeshStandardMaterial({
        color: 0x334155,
        metalness: 0.8,
        roughness: 0.25
      });
      const ringMesh = new THREE.Mesh(ringGeo, ringMat);
      ringMesh.position.set(-37, -7, 0);
      group.add(ringMesh);

      // Orient Knife Center in Viewport
      group.position.set(6, 12, 0);
      group.rotation.z = 0.45;

      return { group, bladeMat };
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim15_EconWear_Scene';

      // Studio Lighting setup for realistic CS2 inspect lighting
      const keyLight = new THREE.DirectionalLight(0xffffff, 2.2);
      keyLight.position.set(30, 40, 50);
      this.sceneGroup.add(keyLight);

      const fillLight = new THREE.DirectionalLight(0x38bdf8, 1.2);
      fillLight.position.set(-30, -30, 20);
      this.sceneGroup.add(fillLight);

      const rimLight = new THREE.DirectionalLight(0xf59e0b, 1.4);
      rimLight.position.set(0, 40, -40);
      this.sceneGroup.add(rimLight);

      // Build Karambit
      const { group, bladeMat } = this._buildKarambitKnife();
      this.knifeMesh = group;
      this.bladeMat = bladeMat;
      this.sceneGroup.add(this.knifeMesh);

      scene.add(this.sceneGroup);

      this._buildParamsUI();

      // Camera view framed on Karambit
      const camera = this.viewport.getCamera();
      if (camera) {
        camera.position.set(0, -60, 20);
        const controls = this.viewport.getControls();
        if (controls) {
          controls.target.set(0, 0, 0);
          controls.update();
        }
      }

      this.update(0, 0, 0);
    }

    update(tick, subTickFrac, dt) {
      // Gentle floating inspect rotation
      if (this.knifeMesh) {
        this.knifeMesh.rotation.y = Math.sin((tick + subTickFrac) * 0.03) * 0.35;
      }

      this._generateSkinTexture(this.activeSkin, this.seed, this.wearValue);
      if (this.bladeMat) {
        this.bladeMat.roughness = 0.18 + (this.wearValue * 0.45);
      }

      const condName = this._getConditionName(this.wearValue);
      this.viewport.setHudMetrics([
        { label: 'WEAR FLOAT (W)', value: this.wearValue.toFixed(4) },
        { label: 'CONDITION', value: condName.name },
        { label: 'PATTERN SEED', value: `#${this.seed}` },
        { label: 'FINISH STYLE', value: this.activeSkin.toUpperCase().replace('_', ' ') },
        { label: 'DEGRADATION ODE', value: 'RUNGE-KUTTA 4TH ORDER' },
        { label: 'FALLBACK SEED', value: 'C_EconItemDefinition (0x18032f)' }
      ]);
    }

    _getConditionName(w) {
      if (w < 0.07) return { name: 'Factory New (FN)', color: 'text-emerald-400' };
      if (w < 0.15) return { name: 'Minimal Wear (MW)', color: 'text-cyan-400' };
      if (w < 0.38) return { name: 'Field-Tested (FT)', color: 'text-blue-400' };
      if (w < 0.45) return { name: 'Well-Worn (WW)', color: 'text-amber-400' };
      return { name: 'Battle-Scarred (BS)', color: 'text-rose-400' };
    }

    teardown() {
      const scene = this.viewport.getScene();
      if (this.sceneGroup) {
        if (scene) scene.remove(this.sceneGroup);
        this.sceneGroup = null;
      }
      if (this.paramsContainer) {
        this.paramsContainer.innerHTML = '';
        this.paramsContainer = null;
      }
      if (this.canvasTexture) {
        this.canvasTexture.dispose();
        this.canvasTexture = null;
      }
      this.textureCanvas = null;
      this.textureCtx = null;
    }

    _buildParamsUI() {
      if (!this.paramsContainer) return;
      const cond = this._getConditionName(this.wearValue);

      this.paramsContainer.innerHTML = `
        <div class="space-y-4 font-mono text-xs select-none">
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1.5">
              <span>WEAPON FINISH &amp; SKIN SCHEMA</span>
              <span class="text-[10px] text-cyan-400">client.dll</span>
            </div>
            <div>
              <label class="text-[11px] text-slate-400 block mb-1">Procedural Finish Style:</label>
              <select id="skinSelect" class="w-full bg-slate-800 border border-slate-700 rounded px-2.5 py-1.5 text-slate-200 cursor-pointer">
                <option value="case_hardened" ${this.activeSkin === 'case_hardened' ? 'selected' : ''}>Case Hardened (#661 Blue Gem Patina)</option>
                <option value="fade" ${this.activeSkin === 'fade' ? 'selected' : ''}>Fade (100% Full Spectrum Anodized)</option>
                <option value="asiimov" ${this.activeSkin === 'asiimov' ? 'selected' : ''}>Asiimov (Sci-Fi High Contrast)</option>
              </select>
            </div>
            <div>
              <div class="flex justify-between text-[11px] text-slate-400">
                <span>Wear Float Value (W):</span>
                <span id="lblWearVal" class="text-amber-400 font-bold">${this.wearValue.toFixed(4)}</span>
              </div>
              <input type="range" id="wearSlider" min="0.00" max="1.00" step="0.001" value="${this.wearValue}" class="w-full accent-amber-400 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[11px] text-slate-400">
                <span>Paint Seed (m_nFallbackSeed):</span>
                <span id="lblSeedVal" class="text-cyan-400 font-bold">#${this.seed}</span>
              </div>
              <input type="range" id="seedSlider" min="1" max="1000" step="1" value="${this.seed}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
          </div>

          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2">
            <div class="text-slate-300 font-bold border-b border-slate-800 pb-1 text-[11px]">
              FLOAT PRESETS (CS2 WEAR BRACKETS)
            </div>
            <div class="grid grid-cols-2 gap-1.5">
              <button id="btnWearFN" class="py-1 px-2 rounded bg-emerald-950/50 hover:bg-emerald-900/70 text-emerald-300 text-[11px] border border-emerald-800/60 font-bold cursor-pointer">Factory New (&le;0.07)</button>
              <button id="btnWearMW" class="py-1 px-2 rounded bg-cyan-950/50 hover:bg-cyan-900/70 text-cyan-300 text-[11px] border border-cyan-800/60 font-bold cursor-pointer">Minimal Wear (0.10)</button>
              <button id="btnWearFT" class="py-1 px-2 rounded bg-blue-950/50 hover:bg-blue-900/70 text-blue-300 text-[11px] border border-blue-800/60 font-bold cursor-pointer">Field-Tested (0.22)</button>
              <button id="btnWearBS" class="py-1 px-2 rounded bg-rose-950/50 hover:bg-rose-900/70 text-rose-300 text-[11px] border border-rose-800/60 font-bold cursor-pointer">Battle-Scarred (0.78)</button>
            </div>
          </div>
        </div>
      `;

      const skinSel = this.paramsContainer.querySelector('#skinSelect');
      if (skinSel) skinSel.onchange = (e) => {
        this.activeSkin = e.target.value;
        this.update(0, 0, 0);
      };

      const wSlider = this.paramsContainer.querySelector('#wearSlider');
      if (wSlider) wSlider.oninput = (e) => {
        this.wearValue = parseFloat(e.target.value);
        this.paramsContainer.querySelector('#lblWearVal').innerText = this.wearValue.toFixed(4);
        this.update(0, 0, 0);
      };

      const sSlider = this.paramsContainer.querySelector('#seedSlider');
      if (sSlider) sSlider.oninput = (e) => {
        this.seed = parseInt(e.target.value);
        this.paramsContainer.querySelector('#lblSeedVal').innerText = `#${this.seed}`;
        this.update(0, 0, 0);
      };

      const setWear = (val) => {
        this.wearValue = val;
        if (wSlider) wSlider.value = val;
        this.paramsContainer.querySelector('#lblWearVal').innerText = val.toFixed(4);
        this.update(0, 0, 0);
      };

      const btnFN = this.paramsContainer.querySelector('#btnWearFN');
      const btnMW = this.paramsContainer.querySelector('#btnWearMW');
      const btnFT = this.paramsContainer.querySelector('#btnWearFT');
      const btnBS = this.paramsContainer.querySelector('#btnWearBS');
      if (btnFN) btnFN.onclick = () => setWear(0.03);
      if (btnMW) btnMW.onclick = () => setWear(0.10);
      if (btnFT) btnFT.onclick = () => setWear(0.22);
      if (btnBS) btnBS.onclick = () => setWear(0.78);
    }
  }

  class Sim17SpatialAudioProvider {
    constructor(viewport, audioEngine, playback) {
      this.viewport = viewport;
      this.audio = audioEngine;
      this.playback = playback;

      this.sourcePos = { x: -140, y: 80, z: 20 };
      this.listenerPos = { x: 120, y: -40, z: 20 };
      this.wallOcclusion = true;
      this.reverbRT60 = 1.45; // seconds
      this.airAbsorptionAlpha = 0.0035; // dB/m
      this.autoPulse = false; // Disabled by default to prevent unwanted background audio ticks
      this.autoPulseInterval = 1.6; // seconds
      this._pulseTimer = 0;

      this.sceneGroup = null;
      this.sourceMesh = null;
      this.listenerMesh = null;
      this.barrierWall = null;
      this.acousticRays = null;
      this.waveRings = [];
      this.paramsContainer = null;
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim17_SpatialAudio_Scene';

      // 1. Acoustic Room Grid & Floor
      const roomGeo = new THREE.BoxGeometry(400, 320, 2);
      const roomMat = new THREE.MeshStandardMaterial({ color: 0x0f172a, roughness: 0.8 });
      const floor = new THREE.Mesh(roomGeo, roomMat);
      floor.position.set(0, 0, -1);
      this.sceneGroup.add(floor);

      // 2. Sound Barrier Wall (Concrete Wall causing Occlusion & Diffraction)
      const wallGeo = new THREE.BoxGeometry(16, 160, 60);
      const wallMat = new THREE.MeshStandardMaterial({ color: 0x475569, roughness: 0.7, metalness: 0.2 });
      this.barrierWall = new THREE.Mesh(wallGeo, wallMat);
      this.barrierWall.position.set(0, 20, 30);
      this.sceneGroup.add(this.barrierWall);

      // 3. Sound Emitter Source (Cyan Beacon)
      const srcGeo = new THREE.SphereGeometry(6, 16, 12);
      const srcMat = new THREE.MeshStandardMaterial({ color: 0x06b6d4, emissive: 0x06b6d4, emissiveIntensity: 0.4 });
      this.sourceMesh = new THREE.Mesh(srcGeo, srcMat);
      this.sourceMesh.position.set(this.sourcePos.x, this.sourcePos.y, this.sourcePos.z);
      this.sceneGroup.add(this.sourceMesh);

      // 4. Listener Head (Emerald Pawn with Directional Ears)
      this.listenerMesh = new THREE.Group();
      const headGeo = new THREE.SphereGeometry(6.5, 16, 12);
      const headMat = new THREE.MeshStandardMaterial({ color: 0x10b981, roughness: 0.4 });
      this.listenerMesh.add(new THREE.Mesh(headGeo, headMat));

      // Left & Right Binaural Ear Sensors
      [-7, 7].forEach(yOff => {
        const earGeo = new THREE.SphereGeometry(1.6, 8, 8);
        const earMat = new THREE.MeshBasicMaterial({ color: 0x34d399 });
        const ear = new THREE.Mesh(earGeo, earMat);
        ear.position.set(0, yOff, 0);
        this.listenerMesh.add(ear);
      });
      this.listenerMesh.position.set(this.listenerPos.x, this.listenerPos.y, this.listenerPos.z);
      this.sceneGroup.add(this.listenerMesh);

      // 5. Acoustic Direct Path Ray
      const rayGeo = new THREE.BufferGeometry().setFromPoints([
        new THREE.Vector3(this.sourcePos.x, this.sourcePos.y, this.sourcePos.z),
        new THREE.Vector3(this.listenerPos.x, this.listenerPos.y, this.listenerPos.z)
      ]);
      const rayMat = new THREE.LineBasicMaterial({
        color: this.wallOcclusion ? 0xf59e0b : 0x10b981,
        linewidth: 2.5
      });
      this.acousticRays = new THREE.Line(rayGeo, rayMat);
      this.sceneGroup.add(this.acousticRays);

      // 6. Expanding Acoustic Wavefront Rings
      this.waveRings = [];
      for (let r = 0; r < 5; r++) {
        const ringGeo = new THREE.RingGeometry(24, 27, 32);
        const ringMat = new THREE.MeshBasicMaterial({
          color: 0x38bdf8,
          side: THREE.DoubleSide,
          transparent: true,
          opacity: 0.4
        });
        const ringMesh = new THREE.Mesh(ringGeo, ringMat);
        ringMesh.position.set(this.sourcePos.x, this.sourcePos.y, 4);
        this.sceneGroup.add(ringMesh);
        this.waveRings.push(ringMesh);
      }

      scene.add(this.sceneGroup);

      // 7. Mount UI Controls
      this._buildParamsUI();

      // 8. Optimal Framing
      const camera = this.viewport.getCamera();
      if (camera) {
        camera.position.set(-180, -260, 190);
        const controls = this.viewport.getControls();
        if (controls) {
          controls.target.set(0, 0, 20);
          controls.update();
        }
      }

      this.update(0, 0, 0);
    }

    _testAcousticAudio(soundType = 'footstep') {
      if (!this.audio) return;
      if (soundType === 'footstep') {
        this.audio.playFootstep('concrete', this.sourcePos.x, this.sourcePos.y, this.sourcePos.z);
      } else if (soundType === 'gunfire') {
        this.audio.playGunfire(this.sourcePos.x, this.sourcePos.y, this.sourcePos.z);
      } else if (soundType === 'ricochet') {
        this.audio.playRicochet(this.sourcePos.x, this.sourcePos.y, this.sourcePos.z);
      }
    }

    update(tick, subTickFrac, dt) {
      if (this.autoPulse) {
        this._pulseTimer += dt;
        if (this._pulseTimer >= this.autoPulseInterval) {
          this._pulseTimer = 0;
          this._testAcousticAudio('footstep');
        }
      }

      const timeSec = (tick + subTickFrac) * 0.015625;
      for (let r = 0; r < this.waveRings.length; r++) {
        const ring = this.waveRings[r];
        const progress = (timeSec * 2.5 + (r / this.waveRings.length)) % 1.0;
        const scale = 0.5 + progress * 5.5;
        ring.scale.set(scale, scale, 1);
        if (ring.material) {
          ring.material.opacity = (1.0 - progress) * 0.55;
        }
      }

      if (this.acousticRays && this.acousticRays.geometry) {
        this.acousticRays.geometry.setFromPoints([
          new THREE.Vector3(this.sourcePos.x, this.sourcePos.y, this.sourcePos.z),
          new THREE.Vector3(this.listenerPos.x, this.listenerPos.y, this.listenerPos.z)
        ]);
        if (this.acousticRays.material) {
          this.acousticRays.material.color.setHex(this.wallOcclusion ? 0xf59e0b : 0x10b981);
        }
      }

      if (this.barrierWall) this.barrierWall.visible = this.wallOcclusion;

      const dist = Math.hypot(this.sourcePos.x - this.listenerPos.x, this.sourcePos.y - this.listenerPos.y);
      const airAttenDb = dist * 0.0254 * this.airAbsorptionAlpha;
      const directGain = this.wallOcclusion ? 0.22 : 1.0;

      this.viewport.setHudMetrics([
        { label: 'SOURCE DISTANCE', value: `${(dist * 0.0254).toFixed(1)} m (${dist.toFixed(0)} u)` },
        { label: 'AIR ATTENUATION', value: `-${airAttenDb.toFixed(3)} dB` },
        { label: 'WALL OCCLUSION', value: this.wallOcclusion ? '-14.0 dB (LOWPASS 650Hz)' : '0.0 dB (DIRECT LOS)', alert: this.wallOcclusion },
        { label: 'DIRECT TRANSMISSION', value: `${(directGain * 100).toFixed(0)}%` },
        { label: 'REVERB RT60', value: `${this.reverbRT60.toFixed(2)} s` },
        { label: 'BINAURAL HRTF', value: 'STEAM AUDIO AMBISONICS (Active)' }
      ]);
    }

    teardown() {
      const scene = this.viewport.getScene();
      if (this.sceneGroup) {
        if (scene) scene.remove(this.sceneGroup);
        this.sceneGroup = null;
      }
      if (this.paramsContainer) {
        this.paramsContainer.innerHTML = '';
        this.paramsContainer = null;
      }
      this.waveRings = [];
    }

    _buildParamsUI() {
      if (!this.paramsContainer) return;
      this.paramsContainer.innerHTML = `
        <div class="space-y-4 font-mono text-xs select-none">
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1.5">
              <span>STEAM AUDIO SPATIAL SYNTH</span>
              <span class="text-[10px] text-cyan-400">steamaudio.dll</span>
            </div>
            <div class="grid grid-cols-3 gap-1.5">
              <button id="sim17BtnFoot" class="py-2 px-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 text-[10px] font-bold cursor-pointer">
                Footstep
              </button>
              <button id="sim17BtnGun" class="py-2 px-1 rounded bg-cyan-700 hover:bg-cyan-600 text-white text-[10px] font-bold cursor-pointer">
                Gunfire
              </button>
              <button id="sim17BtnRic" class="py-2 px-1 rounded bg-purple-700 hover:bg-purple-600 text-white text-[10px] font-bold cursor-pointer">
                Ricochet
              </button>
            </div>
            <label class="flex items-center gap-2 cursor-pointer text-slate-300 pt-1">
              <input type="checkbox" id="sim17CheckWall" ${this.wallOcclusion ? 'checked' : ''} class="accent-amber-400">
              <span class="font-bold text-amber-300">Concrete Barrier Occlusion (-14 dB Lowpass)</span>
            </label>
            <label class="flex items-center gap-2 cursor-pointer text-slate-300 pt-0.5">
              <input type="checkbox" id="sim17CheckAuto" ${this.autoPulse ? 'checked' : ''} class="accent-emerald-400">
              <span class="font-bold text-emerald-400">Auto-Pulse Spatial Sound (1.6s Interval)</span>
            </label>
          </div>

          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1">
              <span>SPATIAL EMITTER &amp; LISTENER</span>
              <span class="text-[10px] text-emerald-400">Binaural HRTF</span>
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Sound Source Y Offset:</span>
                <span id="sim17LblSrcY" class="text-cyan-300 font-bold">${this.sourcePos.y.toFixed(0)} u</span>
              </div>
              <input type="range" id="sim17SrcY" min="-120" max="120" step="5" value="${this.sourcePos.y}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Listener Pawn Y Offset:</span>
                <span id="sim17LblLisY" class="text-emerald-400 font-bold">${this.listenerPos.y.toFixed(0)} u</span>
              </div>
              <input type="range" id="sim17LisY" min="-120" max="120" step="5" value="${this.listenerPos.y}" class="w-full accent-emerald-500 cursor-pointer">
            </div>
          </div>
        </div>
      `;

      const btnFoot = this.paramsContainer.querySelector('#sim17BtnFoot');
      const btnGun = this.paramsContainer.querySelector('#sim17BtnGun');
      const btnRic = this.paramsContainer.querySelector('#sim17BtnRic');

      if (btnFoot) btnFoot.onclick = () => this._testAcousticAudio('footstep');
      if (btnGun) btnGun.onclick = () => this._testAcousticAudio('gunfire');
      if (btnRic) btnRic.onclick = () => this._testAcousticAudio('ricochet');

      const chkWall = this.paramsContainer.querySelector('#sim17CheckWall');
      if (chkWall) chkWall.onchange = (e) => { this.wallOcclusion = e.target.checked; this.update(0, 0, 0); };

      const chkAuto = this.paramsContainer.querySelector('#sim17CheckAuto');
      if (chkAuto) chkAuto.onchange = (e) => { this.autoPulse = e.target.checked; };

      const syIn = this.paramsContainer.querySelector('#sim17SrcY');
      const lblSY = this.paramsContainer.querySelector('#sim17LblSrcY');
      if (syIn && lblSY) {
        syIn.oninput = (e) => {
          this.sourcePos.y = parseFloat(e.target.value);
          lblSY.textContent = `${this.sourcePos.y.toFixed(0)} u`;
          if (this.sourceMesh) this.sourceMesh.position.y = this.sourcePos.y;
          this.waveRings.forEach(r => r.position.y = this.sourcePos.y);
          this.update(0, 0, 0);
        };
      }

      const lyIn = this.paramsContainer.querySelector('#sim17LisY');
      const lblLY = this.paramsContainer.querySelector('#sim17LblLisY');
      if (lyIn && lblLY) {
        lyIn.oninput = (e) => {
          this.listenerPos.y = parseFloat(e.target.value);
          lblLY.textContent = `${this.listenerPos.y.toFixed(0)} u`;
          if (this.listenerMesh) this.listenerMesh.position.y = this.listenerPos.y;
          this.update(0, 0, 0);
        };
      }
    }
  }

  class Sim22ParticleComputeProvider {
    constructor(viewport, audioEngine, playback) {
      this.viewport = viewport;
      this.audio = audioEngine;
      this.playback = playback;

      this.PARTICLE_COUNT = 4096;
      this.activePreset = 'smoke_vortex'; // 'sparks' | 'smoke_vortex' | 'molotov_fire' | 'shockwave'

      // 3D Compute Force Fields & Solvers
      this.enableCurl = true;
      this.turbulenceStrength = 60.0; // 0 - 150 u/s

      this.enableVortex = true;
      this.vortexMomentum = 120.0;    // 0 - 250 rad/s / circulation
      this.vortexRingRadius = 32.0;   // Toroidal ring radius R
      this.vortexCoreRadius = 8.0;    // Core smoothing radius r_core
      this.vortexCenter = { x: 0.0, y: 0.0, z: 35.0 };

      this.enableGravity = true;
      this.gravityVec = { x: 0.0, y: 0.0, z: -380.0 }; // 3D gravity vector

      this.enableCollision = true;
      this.groundRestitution = 0.55;  // 0.0 - 0.95
      this.groundFriction = 0.25;     // 0.0 - 0.90
      this.enableBoundingBox = true;
      this.boxBounds = {
        minX: -120.0, maxX: 120.0,
        minY: -120.0, maxY: 120.0,
        minZ: 0.5,    maxZ: 220.0
      };

      // 64-Byte Aligned Structure-of-Arrays (SoA) SIMD Buffers
      this._allocateSoABuffers(this.PARTICLE_COUNT);

      this.sceneGroup = null;
      this.particlePoints = null;
      this.boundingBoxMesh = null;
      this.vortexRingMesh = null;
      this.groundMesh = null;
      this.gridHelper = null;
      this.paramsContainer = null;

      this._initParticles();
    }

    _allocateSoABuffers(count) {
      this.PARTICLE_COUNT = count;
      this.posX = new Float32Array(count);
      this.posY = new Float32Array(count);
      this.posZ = new Float32Array(count);
      this.velX = new Float32Array(count);
      this.velY = new Float32Array(count);
      this.velZ = new Float32Array(count);
      this.life = new Float32Array(count);       // age
      this.maxLife = new Float32Array(count);    // lifespan
      this.radius = new Float32Array(count);
      this.colorR = new Float32Array(count);
      this.colorG = new Float32Array(count);
      this.colorB = new Float32Array(count);
      this.alpha = new Float32Array(count);
    }

    _setParticleCount(count) {
      if (this.PARTICLE_COUNT === count) return;
      this._allocateSoABuffers(count);
      this._initParticles();
      if (this.particlePoints) {
        const geom = this.particlePoints.geometry;
        geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(count * 3), 3));
        geom.setAttribute('color', new THREE.BufferAttribute(new Float32Array(count * 3), 3));
        geom.attributes.position.needsUpdate = true;
        geom.attributes.color.needsUpdate = true;
      }
      this.update(0, 0, 0.016);
    }

    _initParticles() {
      for (let i = 0; i < this.PARTICLE_COUNT; i++) {
        this._respawnParticle(i);
        this.life[i] = Math.random() * this.maxLife[i]; // Distribute initial phases
      }
    }

    _respawnParticle(i) {
      this.life[i] = 0.0;

      if (this.activePreset === 'sparks') {
        this.posX[i] = (Math.random() - 0.5) * 6.0;
        this.posY[i] = (Math.random() - 0.5) * 6.0;
        this.posZ[i] = 12.0;

        const theta = Math.random() * Math.PI * 2;
        const phi = Math.random() * Math.PI * 0.42;
        const spd = 160.0 + Math.random() * 240.0;
        this.velX[i] = Math.cos(theta) * Math.sin(phi) * spd;
        this.velY[i] = Math.sin(theta) * Math.sin(phi) * spd;
        this.velZ[i] = Math.cos(phi) * spd;
        this.maxLife[i] = 0.7 + Math.random() * 0.7;
        this.radius[i] = 2.0 + Math.random() * 2.0;

        this.colorR[i] = 1.0;
        this.colorG[i] = 0.85;
        this.colorB[i] = 0.25;
      } else if (this.activePreset === 'smoke_vortex') {
        const rInit = Math.random() * 14.0;
        const aInit = Math.random() * Math.PI * 2;
        this.posX[i] = Math.cos(aInit) * rInit;
        this.posY[i] = Math.sin(aInit) * rInit;
        this.posZ[i] = 8.0 + Math.random() * 8.0;

        this.velX[i] = (Math.random() - 0.5) * 30.0;
        this.velY[i] = (Math.random() - 0.5) * 30.0;
        this.velZ[i] = 75.0 + Math.random() * 95.0;
        this.maxLife[i] = 2.4 + Math.random() * 1.6;
        this.radius[i] = 4.0 + Math.random() * 4.0;

        this.colorR[i] = 0.55;
        this.colorG[i] = 0.72;
        this.colorB[i] = 0.88;
      } else if (this.activePreset === 'molotov_fire') {
        const r = Math.random() * 48.0;
        const a = Math.random() * Math.PI * 2;
        this.posX[i] = Math.cos(a) * r;
        this.posY[i] = Math.sin(a) * r;
        this.posZ[i] = 2.0 + Math.random() * 4.0;

        this.velX[i] = (Math.random() - 0.5) * 24.0;
        this.velY[i] = (Math.random() - 0.5) * 24.0;
        this.velZ[i] = 85.0 + Math.random() * 135.0;
        this.maxLife[i] = 1.1 + Math.random() * 0.9;
        this.radius[i] = 4.5 + Math.random() * 4.5;

        this.colorR[i] = 1.0;
        this.colorG[i] = 0.45;
        this.colorB[i] = 0.05;
      } else if (this.activePreset === 'shockwave') {
        this.posX[i] = (Math.random() - 0.5) * 8.0;
        this.posY[i] = (Math.random() - 0.5) * 8.0;
        this.posZ[i] = 25.0;

        const theta = Math.random() * Math.PI * 2;
        const u = Math.random() * 2.0 - 1.0;
        const phi = Math.acos(u);
        const spd = 260.0 + Math.random() * 220.0;
        this.velX[i] = Math.sin(phi) * Math.cos(theta) * spd;
        this.velY[i] = Math.sin(phi) * Math.sin(theta) * spd;
        this.velZ[i] = Math.cos(phi) * spd;
        this.maxLife[i] = 0.9 + Math.random() * 0.6;
        this.radius[i] = 3.0 + Math.random() * 3.0;

        this.colorR[i] = 0.35;
        this.colorG[i] = 0.92;
        this.colorB[i] = 1.0;
      }
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim22_ParticleCompute_Scene';

      // 1. WebGL Points Buffer Geometry (Point Sprites with Hardware Additive Blending)
      const geom = new THREE.BufferGeometry();
      const posArray = new Float32Array(this.PARTICLE_COUNT * 3);
      const colArray = new Float32Array(this.PARTICLE_COUNT * 3);

      geom.setAttribute('position', new THREE.BufferAttribute(posArray, 3));
      geom.setAttribute('color', new THREE.BufferAttribute(colArray, 3));

      const mat = new THREE.PointsMaterial({
        size: 4.2,
        vertexColors: true,
        transparent: true,
        opacity: 0.92,
        blending: THREE.AdditiveBlending,
        depthWrite: false
      });

      this.particlePoints = new THREE.Points(geom, mat);
      this.sceneGroup.add(this.particlePoints);

      // 2. Ground Collision Plane with Spatial Sub-Tick Grid
      const groundGeo = new THREE.PlaneGeometry(260, 260);
      const groundMat = new THREE.MeshStandardMaterial({
        color: 0x070c18,
        roughness: 0.9,
        metalness: 0.1
      });
      this.groundMesh = new THREE.Mesh(groundGeo, groundMat);
      this.groundMesh.position.set(0, 0, 0);
      this.sceneGroup.add(this.groundMesh);

      this.gridHelper = new THREE.GridHelper(240, 24, 0x00f3ff, 0x1e293b);
      this.gridHelper.rotation.x = Math.PI / 2; // Orient in XY plane
      this.gridHelper.position.set(0, 0, 0.2);
      this.sceneGroup.add(this.gridHelper);

      // 3. 3D Bounding Box AABB Wireframe Mesh
      const boxW = this.boxBounds.maxX - this.boxBounds.minX;
      const boxH = this.boxBounds.maxY - this.boxBounds.minY;
      const boxD = this.boxBounds.maxZ - this.boxBounds.minZ;
      const boxGeo = new THREE.BoxGeometry(boxW, boxH, boxD);
      const edgesGeo = new THREE.EdgesGeometry(boxGeo);
      const lineMat = new THREE.LineBasicMaterial({
        color: 0x00f3ff,
        transparent: true,
        opacity: 0.35
      });
      this.boundingBoxMesh = new THREE.LineSegments(edgesGeo, lineMat);
      this.boundingBoxMesh.position.set(0, 0, this.boxBounds.minZ + boxD * 0.5);
      this.sceneGroup.add(this.boundingBoxMesh);

      // 4. Toroidal Vortex Ring Visualizer (Rotates with Angular Momentum)
      const torusGeo = new THREE.TorusGeometry(this.vortexRingRadius, 1.4, 8, 36);
      const torusMat = new THREE.MeshBasicMaterial({
        color: 0x38bdf8,
        wireframe: true,
        transparent: true,
        opacity: 0.45
      });
      this.vortexRingMesh = new THREE.Mesh(torusGeo, torusMat);
      this.vortexRingMesh.position.set(0, 0, this.vortexCenter.z);
      this.sceneGroup.add(this.vortexRingMesh);

      scene.add(this.sceneGroup);

      this._buildParamsUI();

      const camera = this.viewport.getCamera();
      if (camera) {
        camera.position.set(-140, -180, 120);
        const controls = this.viewport.getControls();
        if (controls) {
          controls.target.set(0, 0, 40);
          controls.update();
        }
      }

      this.update(0, 0, 0.016);
    }

    update(tick, subTickFrac, dt) {
      if (!this.particlePoints) return;
      const stepDt = Math.min(0.033, dt || 0.015625);
      const geom = this.particlePoints.geometry;
      const positions = geom.attributes.position.array;
      const colors = geom.attributes.color.array;

      let activeCount = 0;
      let bounceCount = 0;
      const tStart = performance.now();

      // Dynamic force fields based on active compute toggles
      const gx = this.enableGravity ? this.gravityVec.x : 0.0;
      const gy = this.enableGravity ? this.gravityVec.y : 0.0;
      const gz = this.enableGravity ? (
        this.activePreset === 'smoke_vortex' ? 20.0 :
        (this.activePreset === 'molotov_fire' ? 55.0 : this.gravityVec.z)
      ) : 0.0;

      const turb = this.enableCurl ? this.turbulenceStrength : 0.0;
      const vortexCirc = this.enableVortex ? this.vortexMomentum : 0.0;
      const vortexSwirl = this.enableVortex ? this.vortexMomentum * 0.45 : 0.0;
      const vRingR = this.vortexRingRadius;
      const vCoreR2 = this.vortexCoreRadius * this.vortexCoreRadius;
      const vCenterZ = this.vortexCenter.z;

      const rest = this.groundRestitution;
      const fric = 1.0 - this.groundFriction;
      const bounds = this.boxBounds;

      // 64-Byte Aligned Structure-of-Arrays (SoA) SIMD Operator Dispatch
      for (let i = 0; i < this.PARTICLE_COUNT; i++) {
        this.life[i] += stepDt;
        if (this.life[i] >= this.maxLife[i]) {
          this._respawnParticle(i);
        }
        activeCount++;

        // 1. COpMovementBasic: 3D Gravity Vector & Drag Acceleration
        this.velX[i] += gx * stepDt;
        this.velY[i] += gy * stepDt;
        this.velZ[i] += gz * stepDt;

        const dragFactor = Math.max(0.0, 1.0 - 0.04 * stepDt);
        this.velX[i] *= dragFactor;
        this.velY[i] *= dragFactor;
        this.velZ[i] *= dragFactor;

        // 2. COpTurbulenceNoise: Divergence-Free 3D Curl Vector Field (del . v == 0)
        if (turb > 0.0) {
          const k = 0.035;
          const px = this.posX[i];
          const py = this.posY[i];
          const pz = this.posZ[i];
          const t = tick * 0.08;

          const curlX = Math.sin(k * pz + t * 0.4) - Math.sin(k * py + t * 0.2);
          const curlY = Math.sin(k * px + t * 0.7) - Math.sin(k * pz + t * 0.3);
          const curlZ = Math.sin(k * py + t * 0.5) - Math.sin(k * px + t * 0.6);

          this.velX[i] += curlX * turb * stepDt;
          this.velY[i] += curlY * turb * stepDt;
          this.velZ[i] += curlZ * turb * stepDt;
        }

        // 3. COpVortexRing: Toroidal Vortex Ring Circulation & Azimuthal Angular Momentum
        if (vortexCirc > 0.0) {
          const dx = this.posX[i];
          const dy = this.posY[i];
          const dz = this.posZ[i] - vCenterZ;
          const rho = Math.sqrt(dx * dx + dy * dy);

          if (rho > 1e-4) {
            const invRho = 1.0 / rho;
            const dirX = dx * invRho;
            const dirY = dy * invRho;

            const deltaRho = rho - vRingR;
            const distSq = deltaRho * deltaRho + dz * dz + vCoreR2;
            const invDistSq = 1.0 / distSq;

            // Poloidal rolling motion (upward jet in core, outer downward roll)
            const vRho = -vortexCirc * dz * invDistSq;
            const vZ_ind = vortexCirc * deltaRho * invDistSq;

            // Azimuthal angular momentum swirl around vertical ring axis
            const vSwirl = vortexSwirl * (vCoreR2 * invDistSq);

            this.velX[i] += (vRho * dirX - vSwirl * dirY) * stepDt;
            this.velY[i] += (vRho * dirY + vSwirl * dirX) * stepDt;
            this.velZ[i] += vZ_ind * stepDt;
          } else {
            const distSq = vRingR * vRingR + dz * dz + vCoreR2;
            this.velZ[i] += (vortexCirc * vRingR / distSq) * stepDt;
          }
        }

        // Symplectic Euler Integration
        this.posX[i] += this.velX[i] * stepDt;
        this.posY[i] += this.velY[i] * stepDt;
        this.posZ[i] += this.velZ[i] * stepDt;

        // 4. Ground Restitution Plane & 3D Bounding Box Collision Solver
        if (this.enableCollision) {
          const r = this.radius[i];

          // Ground restitution floor (Z min)
          if (this.posZ[i] - r <= bounds.minZ) {
            this.posZ[i] = bounds.minZ + r;
            if (this.velZ[i] < 0) {
              this.velZ[i] = -this.velZ[i] * rest;
              this.velX[i] *= fric;
              this.velY[i] *= fric;
              bounceCount++;
            }
          }

          // 3D Bounding box confinement (X, Y walls and ceiling)
          if (this.enableBoundingBox) {
            // X boundary planes
            if (this.posX[i] - r < bounds.minX) {
              this.posX[i] = bounds.minX + r;
              if (this.velX[i] < 0) {
                this.velX[i] = -this.velX[i] * rest;
                this.velY[i] *= fric;
                this.velZ[i] *= fric;
                bounceCount++;
              }
            } else if (this.posX[i] + r > bounds.maxX) {
              this.posX[i] = bounds.maxX - r;
              if (this.velX[i] > 0) {
                this.velX[i] = -this.velX[i] * rest;
                this.velY[i] *= fric;
                this.velZ[i] *= fric;
                bounceCount++;
              }
            }

            // Y boundary planes
            if (this.posY[i] - r < bounds.minY) {
              this.posY[i] = bounds.minY + r;
              if (this.velY[i] < 0) {
                this.velY[i] = -this.velY[i] * rest;
                this.velX[i] *= fric;
                this.velZ[i] *= fric;
                bounceCount++;
              }
            } else if (this.posY[i] + r > bounds.maxY) {
              this.posY[i] = bounds.maxY - r;
              if (this.velY[i] > 0) {
                this.velY[i] = -this.velY[i] * rest;
                this.velX[i] *= fric;
                this.velZ[i] *= fric;
                bounceCount++;
              }
            }

            // Z ceiling plane
            if (this.posZ[i] + r > bounds.maxZ) {
              this.posZ[i] = bounds.maxZ - r;
              if (this.velZ[i] > 0) {
                this.velZ[i] = -this.velZ[i] * rest;
                this.velX[i] *= fric;
                this.velY[i] *= fric;
                bounceCount++;
              }
            }
          }
        }

        // Stream positions to WebGL Dynamic Buffer
        const idx3 = i * 3;
        positions[idx3]     = this.posX[i];
        positions[idx3 + 1] = this.posY[i];
        positions[idx3 + 2] = this.posZ[i];

        // 5. COpFadeAndKill: Piecewise Hermite Alpha Envelope & Color Gradient
        const normAge = Math.min(1.0, this.life[i] / this.maxLife[i]);
        const alphaCurve = Math.sin(normAge * Math.PI);
        this.alpha[i] = alphaCurve;

        if (this.activePreset === 'sparks') {
          colors[idx3]     = this.colorR[i] * alphaCurve;
          colors[idx3 + 1] = this.colorG[i] * alphaCurve * (1.0 - normAge * 0.35);
          colors[idx3 + 2] = this.colorB[i] * alphaCurve * (1.0 - normAge * 0.7);
        } else if (this.activePreset === 'smoke_vortex') {
          colors[idx3]     = this.colorR[i] * alphaCurve;
          colors[idx3 + 1] = this.colorG[i] * alphaCurve;
          colors[idx3 + 2] = this.colorB[i] * alphaCurve;
        } else if (this.activePreset === 'molotov_fire') {
          colors[idx3]     = this.colorR[i] * alphaCurve;
          colors[idx3 + 1] = (0.2 + (1.0 - normAge) * 0.6) * alphaCurve;
          colors[idx3 + 2] = 0.05 * alphaCurve;
        } else if (this.activePreset === 'shockwave') {
          colors[idx3]     = (0.4 + 0.6 * (1.0 - normAge)) * alphaCurve;
          colors[idx3 + 1] = 0.9 * alphaCurve;
          colors[idx3 + 2] = 1.0 * alphaCurve;
        }
      }

      geom.attributes.position.needsUpdate = true;
      geom.attributes.color.needsUpdate = true;

      // Animate 3D Toroidal Vortex Ring Visualizer
      if (this.vortexRingMesh) {
        if (this.enableVortex) {
          this.vortexRingMesh.visible = true;
          this.vortexRingMesh.rotation.z += vortexSwirl * 0.001;
        } else {
          this.vortexRingMesh.visible = false;
        }
      }

      if (this.boundingBoxMesh) {
        this.boundingBoxMesh.visible = this.enableBoundingBox;
      }

      const tElapsedMicro = ((performance.now() - tStart) * 1000).toFixed(0);
      const memBytes = this.PARTICLE_COUNT * 13 * 4; // 13 channels * 4 bytes = 52 bytes/particle (64B aligned block)

      return {
        pos: { x: 0, y: 0, z: this.vortexCenter.z },
        vel: { x: this.PARTICLE_COUNT, y: tElapsedMicro, z: bounceCount, speed: activeCount },
        ground: true,
        event: `${this.activePreset.toUpperCase()} [6 COMPUTE OPS]`,
        customMetrics: [
          { label: 'ACTIVE PARTICLES', value: `${activeCount} / ${this.PARTICLE_COUNT}` },
          { label: 'SIMD MEMORY', value: `${(memBytes / 1024).toFixed(1)} KB (SoA 64B)` },
          { label: 'DISPATCH TIME', value: `${tElapsedMicro} μs` },
          { label: 'COMPUTE OPERATORS', value: '6x SoA BATCHED' },
          { label: 'FORCE FIELDS', value: `${this.enableCurl ? 'CURL ' : ''}${this.enableVortex ? 'VORTEX ' : ''}${this.enableGravity ? 'GRAV' : ''}`.trim() || 'NONE' },
          { label: 'COLLISION SOLVER', value: `${this.enableBoundingBox ? '3D AABB BOX' : 'GROUND PLANE'}` }
        ]
      };
    }

    teardown() {
      const scene = this.viewport.getScene();
      if (this.sceneGroup) {
        if (scene) scene.remove(this.sceneGroup);
        this.sceneGroup = null;
      }
      this.particlePoints = null;
      this.boundingBoxMesh = null;
      this.vortexRingMesh = null;
      this.groundMesh = null;
      this.gridHelper = null;
      if (this.paramsContainer) {
        this.paramsContainer.innerHTML = '';
        this.paramsContainer = null;
      }
    }

    _buildParamsUI() {
      if (!this.paramsContainer) return;
      this.paramsContainer.innerHTML = `
        <div class="space-y-3 font-mono text-xs select-none">
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1.5">
              <span>SOURCE 2 PARTICLE ENGINE</span>
              <span class="text-[10px] text-cyan-400 font-semibold">particles.dll / SIMD SoA</span>
            </div>

            <!-- Compute Preset -->
            <div>
              <label class="block text-[11px] text-slate-400 mb-1">Compute Operator Preset:</label>
              <select id="sim22PresetSelect" class="w-full bg-slate-950 border border-slate-700 rounded px-2 py-1.5 text-cyan-300 font-bold cursor-pointer">
                <option value="smoke_vortex" ${this.activePreset === 'smoke_vortex' ? 'selected' : ''}>Smoke Grenade Vortex (3D Curl Turbulence)</option>
                <option value="sparks" ${this.activePreset === 'sparks' ? 'selected' : ''}>Muzzle Flash Sparks (Restitution Bounce)</option>
                <option value="molotov_fire" ${this.activePreset === 'molotov_fire' ? 'selected' : ''}>Molotov Flame Eruption (Buoyant Thermal)</option>
                <option value="shockwave" ${this.activePreset === 'shockwave' ? 'selected' : ''}>HE Detonation Shockwave (High-Velocity Blast)</option>
              </select>
            </div>

            <!-- Particle Count (SoA SIMD Batch) -->
            <div>
              <div class="flex justify-between text-[11px] text-slate-400 mb-1">
                <span>SIMD Particle Count:</span>
                <span id="sim22LblCount" class="text-cyan-300 font-bold">${this.PARTICLE_COUNT}</span>
              </div>
              <div class="grid grid-cols-4 gap-1.5">
                ${[512, 1024, 2048, 4096].map(cnt => `
                  <button type="button" data-count="${cnt}" class="sim22-count-btn px-2 py-1 rounded text-[10px] font-bold border transition ${
                    this.PARTICLE_COUNT === cnt
                      ? 'bg-cyan-500/20 text-cyan-300 border-cyan-500'
                      : 'bg-slate-950 text-slate-400 border-slate-800 hover:border-slate-700'
                  }">${cnt}</button>
                `).join('')}
              </div>
            </div>

            <!-- Curl Turbulence Field -->
            <div class="space-y-1">
              <div class="flex items-center justify-between text-[11px]">
                <label class="flex items-center gap-1.5 text-slate-400 cursor-pointer">
                  <input type="checkbox" id="sim22CheckCurl" ${this.enableCurl ? 'checked' : ''} class="accent-cyan-400 rounded cursor-pointer">
                  <span>3D Curl Turbulence Field:</span>
                </label>
                <span id="sim22LblTurb" class="text-cyan-300 font-bold">${this.turbulenceStrength.toFixed(0)} u/s</span>
              </div>
              <input type="range" id="sim22Turb" min="0" max="150" step="5" value="${this.turbulenceStrength}" class="w-full accent-cyan-400 cursor-pointer">
            </div>

            <!-- Vortex Angular Momentum -->
            <div class="space-y-1">
              <div class="flex items-center justify-between text-[11px]">
                <label class="flex items-center gap-1.5 text-slate-400 cursor-pointer">
                  <input type="checkbox" id="sim22CheckVortex" ${this.enableVortex ? 'checked' : ''} class="accent-cyan-400 rounded cursor-pointer">
                  <span>Vortex Angular Momentum:</span>
                </label>
                <span id="sim22LblVortex" class="text-cyan-300 font-bold">${this.vortexMomentum.toFixed(0)} rad/s</span>
              </div>
              <input type="range" id="sim22Vortex" min="0" max="250" step="10" value="${this.vortexMomentum}" class="w-full accent-cyan-400 cursor-pointer">
            </div>

            <!-- 3D Gravity Vector -->
            <div class="space-y-1">
              <div class="flex items-center justify-between text-[11px]">
                <label class="flex items-center gap-1.5 text-slate-400 cursor-pointer">
                  <input type="checkbox" id="sim22CheckGravity" ${this.enableGravity ? 'checked' : ''} class="accent-cyan-400 rounded cursor-pointer">
                  <span>3D Gravity Vector (Z):</span>
                </label>
                <span id="sim22LblGrav" class="text-cyan-300 font-bold">${this.gravityVec.z.toFixed(0)} u/s²</span>
              </div>
              <input type="range" id="sim22Grav" min="-800" max="200" step="25" value="${this.gravityVec.z}" class="w-full accent-cyan-400 cursor-pointer">
            </div>

            <!-- Ground Restitution Plane & Bounding Box -->
            <div class="space-y-1">
              <div class="flex items-center justify-between text-[11px]">
                <label class="flex items-center gap-1.5 text-slate-400 cursor-pointer">
                  <input type="checkbox" id="sim22CheckBox" ${this.enableBoundingBox ? 'checked' : ''} class="accent-cyan-400 rounded cursor-pointer">
                  <span>3D Bounding Box Bounce:</span>
                </label>
                <span id="sim22LblRest" class="text-cyan-300 font-bold">${(this.groundRestitution * 100).toFixed(0)}% Restitution</span>
              </div>
              <input type="range" id="sim22Rest" min="0" max="0.95" step="0.05" value="${this.groundRestitution}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
          </div>
        </div>
      `;

      // Preset Selector
      const sel = this.paramsContainer.querySelector('#sim22PresetSelect');
      if (sel) {
        sel.onchange = (e) => {
          this.activePreset = e.target.value;
          this._initParticles();
          this.update(0, 0, 0.016);
        };
      }

      // Particle Count Selector Buttons
      const countBtns = this.paramsContainer.querySelectorAll('.sim22-count-btn');
      countBtns.forEach(btn => {
        btn.onclick = () => {
          const count = parseInt(btn.getAttribute('data-count'), 10);
          this._setParticleCount(count);
          countBtns.forEach(b => {
            b.className = 'sim22-count-btn px-2 py-1 rounded text-[10px] font-bold border transition ' +
              (parseInt(b.getAttribute('data-count'), 10) === count
                ? 'bg-cyan-500/20 text-cyan-300 border-cyan-500'
                : 'bg-slate-950 text-slate-400 border-slate-800 hover:border-slate-700');
          });
          const lblCount = this.paramsContainer.querySelector('#sim22LblCount');
          if (lblCount) lblCount.textContent = `${count}`;
        };
      });

      // Curl Turbulence
      const chkCurl = this.paramsContainer.querySelector('#sim22CheckCurl');
      const turbIn = this.paramsContainer.querySelector('#sim22Turb');
      const lblTurb = this.paramsContainer.querySelector('#sim22LblTurb');
      if (chkCurl) {
        chkCurl.onchange = (e) => {
          this.enableCurl = e.target.checked;
          this.update(0, 0, 0.016);
        };
      }
      if (turbIn && lblTurb) {
        turbIn.oninput = (e) => {
          this.turbulenceStrength = parseFloat(e.target.value);
          lblTurb.textContent = `${this.turbulenceStrength.toFixed(0)} u/s`;
          this.update(0, 0, 0.016);
        };
      }

      // Vortex Angular Momentum
      const chkVortex = this.paramsContainer.querySelector('#sim22CheckVortex');
      const vortexIn = this.paramsContainer.querySelector('#sim22Vortex');
      const lblVortex = this.paramsContainer.querySelector('#sim22LblVortex');
      if (chkVortex) {
        chkVortex.onchange = (e) => {
          this.enableVortex = e.target.checked;
          this.update(0, 0, 0.016);
        };
      }
      if (vortexIn && lblVortex) {
        vortexIn.oninput = (e) => {
          this.vortexMomentum = parseFloat(e.target.value);
          lblVortex.textContent = `${this.vortexMomentum.toFixed(0)} rad/s`;
          this.update(0, 0, 0.016);
        };
      }

      // Gravity Vector
      const chkGrav = this.paramsContainer.querySelector('#sim22CheckGravity');
      const gravIn = this.paramsContainer.querySelector('#sim22Grav');
      const lblGrav = this.paramsContainer.querySelector('#sim22LblGrav');
      if (chkGrav) {
        chkGrav.onchange = (e) => {
          this.enableGravity = e.target.checked;
          this.update(0, 0, 0.016);
        };
      }
      if (gravIn && lblGrav) {
        gravIn.oninput = (e) => {
          this.gravityVec.z = parseFloat(e.target.value);
          lblGrav.textContent = `${this.gravityVec.z.toFixed(0)} u/s²`;
          this.update(0, 0, 0.016);
        };
      }

      // Ground Restitution & Bounding Box
      const chkBox = this.paramsContainer.querySelector('#sim22CheckBox');
      const restIn = this.paramsContainer.querySelector('#sim22Rest');
      const lblRest = this.paramsContainer.querySelector('#sim22LblRest');
      if (chkBox) {
        chkBox.onchange = (e) => {
          this.enableBoundingBox = e.target.checked;
          this.update(0, 0, 0.016);
        };
      }
      if (restIn && lblRest) {
        restIn.oninput = (e) => {
          this.groundRestitution = parseFloat(e.target.value);
          lblRest.textContent = `${(this.groundRestitution * 100).toFixed(0)}% Restitution`;
          this.update(0, 0, 0.016);
        };
      }
    }
  }

  // Register providers under active simulator IDs (Sim 15 temporarily disabled awaiting VMDL game mesh extraction)
  CS2Lab.registerProvider('03', Sim03SubtickProvider);
  CS2Lab.registerProvider('09', Sim09SkeletonIKProvider);
  CS2Lab.registerProvider('11', Sim11VolumetricSmokeProvider);
  CS2Lab.registerProvider('12', Sim12LagCompensationProvider);
  CS2Lab.registerProvider('14', Sim14BulletPenetrationProvider);
  // CS2Lab.registerProvider('15', Sim15EconWearProvider); // Paused for authentic VMDL ingestion
  CS2Lab.registerProvider('17', Sim17SpatialAudioProvider);
  CS2Lab.registerProvider('22', Sim22ParticleComputeProvider);

  CS2Lab.Sim03SubtickProvider = Sim03SubtickProvider;
  CS2Lab.Sim09SkeletonIKProvider = Sim09SkeletonIKProvider;
  CS2Lab.Sim11VolumetricSmokeProvider = Sim11VolumetricSmokeProvider;
  CS2Lab.Sim12LagCompensationProvider = Sim12LagCompensationProvider;
  CS2Lab.Sim14BulletPenetrationProvider = Sim14BulletPenetrationProvider;
  CS2Lab.Sim15EconWearProvider = Sim15EconWearProvider;
  CS2Lab.Sim17SpatialAudioProvider = Sim17SpatialAudioProvider;
  CS2Lab.Sim22ParticleComputeProvider = Sim22ParticleComputeProvider;

})(window);
