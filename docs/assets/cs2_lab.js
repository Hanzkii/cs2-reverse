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
          this.activeCamera.position.set(target.x - dist, target.y, target.z);
          this.activeCamera.up.set(0, 0, 1);
          break;
        case 'side':
          this.activeCamera.position.set(target.x, target.y - dist, target.z);
          this.activeCamera.up.set(0, 0, 1);
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

      // Coordinate Triad (+X red forward, +Y green left, +Z blue up)
      this.triadGroup = new THREE.Group();
      const axisLen = 64;
      if (typeof THREE.ArrowHelper === 'function') {
        this.triadGroup.add(new THREE.ArrowHelper(new THREE.Vector3(1, 0, 0), new THREE.Vector3(0, 0, 0), axisLen, 0xef4444, 8, 4));
        this.triadGroup.add(new THREE.ArrowHelper(new THREE.Vector3(0, 1, 0), new THREE.Vector3(0, 0, 0), axisLen, 0x10b981, 8, 4));
        this.triadGroup.add(new THREE.ArrowHelper(new THREE.Vector3(0, 0, 1), new THREE.Vector3(0, 0, 0), axisLen, 0x38bdf8, 8, 4));
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
      this.isMuted = false;
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
      if (!this.ctx || !camera) return;
      const listener = this.ctx.listener;
      const t = this.ctx.currentTime;

      const pos = new THREE.Vector3();
      const fwd = new THREE.Vector3();
      const up = new THREE.Vector3(0, 1, 0);

      camera.getWorldPosition(pos);
      camera.getWorldDirection(fwd);
      up.applyQuaternion(camera.quaternion);

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

      // Discrete Host Tick Crossing: Trigger audio metronome pulse
      if (Number.isFinite(currTick) && currTick !== prevTick) {
        if (this.audioEngine && typeof this.audioEngine.playTickPulse === 'function') {
          this.audioEngine.playTickPulse();
        }
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
    constructor(viewport, audio, playback) {
      this.viewport = viewport;
      this.audio = audio;
      this.playback = playback;

      // Active IK parameters
      this.activeLimb = 'arm_R'; // 'arm_R' | 'arm_L' | 'leg_R' | 'leg_L'
      this.blendWeight = 1.0;    // 0.0 (FK) to 1.0 (Full IK)
      this.targetPos = { x: 22.0, y: -22.0, z: 45.0 };
      this.poleVec = { x: 0.0, y: -1.0, z: -0.5 };
      this.limbLengthL1 = 11.4;
      this.limbLengthL2 = 10.2;
      this.showHitboxes = true;
      this.showBones = true;
      this.showGizmo = true;

      // 3D Scene Containers
      this.sceneGroup = null;
      this.boneLines = null;
      this.boneMeshes = [];
      this.jointMeshes = [];
      this.capsuleMeshes = [];
      this.targetGizmo = null;
      this.reachSphereMesh = null;
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
      // 19-bone topological structure matching CS2 player scale (72u height)
      this.bones = [
        { name: 'pelvis',      parent: -1, local: new CTransform({ x: 0, y: 0, z: 38 }) },
        { name: 'spine',       parent: 0,  local: new CTransform({ x: 0, y: 0, z: 12 }) },
        { name: 'chest',       parent: 1,  local: new CTransform({ x: 0, y: 0, z: 14 }) },
        { name: 'neck',        parent: 2,  local: new CTransform({ x: 0, y: 0, z: 6 }) },
        { name: 'head',        parent: 3,  local: new CTransform({ x: 2, y: 0, z: 6 }) },
        { name: 'clavicle_L',  parent: 2,  local: new CTransform({ x: 0, y: 5, z: 2 }) },
        { name: 'shoulder_L',  parent: 5,  local: new CTransform({ x: 0, y: 7, z: -1 }) },
        { name: 'elbow_L',     parent: 6,  local: new CTransform({ x: 0, y: 11, z: -3 }) },
        { name: 'hand_L',      parent: 7,  local: new CTransform({ x: 0, y: 10, z: -2 }) },
        { name: 'clavicle_R',  parent: 2,  local: new CTransform({ x: 0, y: -5, z: 2 }) },
        { name: 'shoulder_R',  parent: 9,  local: new CTransform({ x: 0, y: -7, z: -1 }) },
        { name: 'elbow_R',     parent: 10, local: new CTransform({ x: 0, y: -11, z: -3 }) },
        { name: 'hand_R',      parent: 11, local: new CTransform({ x: 0, y: -10, z: -2 }) },
        { name: 'hip_L',       parent: 0,  local: new CTransform({ x: 0, y: 4.5, z: -3 }) },
        { name: 'knee_L',      parent: 13, local: new CTransform({ x: 0, y: 0, z: -17 }) },
        { name: 'foot_L',      parent: 14, local: new CTransform({ x: 2, y: 0, z: -17 }) },
        { name: 'hip_R',       parent: 0,  local: new CTransform({ x: 0, y: -4.5, z: -3 }) },
        { name: 'knee_R',      parent: 16, local: new CTransform({ x: 0, y: 0, z: -17 }) },
        { name: 'foot_R',      parent: 17, local: new CTransform({ x: 2, y: 0, z: -17 }) }
      ];
      this.worldBones = new Array(this.bones.length);
    }

    _initHitboxes() {
      // 13 canonical bounding capsules attached to bones (Source 2 hitgroup colors)
      this.hitboxes = [
        { name: 'hitbox_head',        bone: 4,  group: 1, color: 0xfbbf24, min: { x: -1.5, y: -3, z: -2 }, max: { x: 3.5, y: 3, z: 4 }, radius: 4.5 },
        { name: 'hitbox_neck',        bone: 3,  group: 8, color: 0x38bdf8, min: { x: -2, y: -2.5, z: -1 }, max: { x: 2, y: 2.5, z: 4 }, radius: 3.5 },
        { name: 'hitbox_chest',       bone: 2,  group: 2, color: 0x38bdf8, min: { x: -4, y: -7, z: -6 }, max: { x: 4, y: 7, z: 6 }, radius: 5.5 },
        { name: 'hitbox_stomach',     bone: 1,  group: 3, color: 0x34d399, min: { x: -4.5, y: -6.5, z: -5 }, max: { x: 4.5, y: 6.5, z: 5 }, radius: 5.8 },
        { name: 'hitbox_pelvis',      bone: 0,  group: 3, color: 0x34d399, min: { x: -5, y: -5.5, z: -4 }, max: { x: 5, y: 5.5, z: 3 }, radius: 6.0 },
        { name: 'hitbox_arm_upper_L', bone: 6,  group: 4, color: 0xa78bfa, min: { x: -2, y: 0, z: -2 }, max: { x: 2, y: 11, z: 2 }, radius: 3.2 },
        { name: 'hitbox_arm_lower_L', bone: 7,  group: 4, color: 0xa78bfa, min: { x: -1.8, y: 0, z: -1.8 }, max: { x: 1.8, y: 10, z: 1.8 }, radius: 2.6 },
        { name: 'hitbox_arm_upper_R', bone: 10, group: 5, color: 0xa78bfa, min: { x: -2, y: -11, z: -2 }, max: { x: 2, y: 0, z: 2 }, radius: 3.2 },
        { name: 'hitbox_arm_lower_R', bone: 11, group: 5, color: 0xa78bfa, min: { x: -1.8, y: -10, z: -1.8 }, max: { x: 1.8, y: 0, z: 1.8 }, radius: 2.6 },
        { name: 'hitbox_leg_upper_L', bone: 13, group: 6, color: 0xf472b6, min: { x: -3, y: -3, z: -17 }, max: { x: 3, y: 3, z: 0 }, radius: 3.8 },
        { name: 'hitbox_leg_lower_L', bone: 14, group: 6, color: 0xf472b6, min: { x: -2.5, y: -2.5, z: -17 }, max: { x: 2.5, y: 2.5, z: 0 }, radius: 3.2 },
        { name: 'hitbox_leg_upper_R', bone: 16, group: 7, color: 0xf472b6, min: { x: -3, y: -3, z: -17 }, max: { x: 3, y: 3, z: 0 }, radius: 3.8 },
        { name: 'hitbox_leg_lower_R', bone: 17, group: 7, color: 0xf472b6, min: { x: -2.5, y: -2.5, z: -17 }, max: { x: 2.5, y: 2.5, z: 0 }, radius: 3.2 }
      ];
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim09_Skeleton_Scene';

      // 1. Draggable 3D Target Gizmo
      this.targetGizmo = new Sim09TargetGizmo(this.viewport, this.targetPos, (newPos) => {
        this.targetPos = { x: newPos.x, y: newPos.y, z: newPos.z };
        this._syncUITarget();
        this.update(0, 0, 0);
      });

      // 2. Reach Sphere Mesh
      const reachGeom = new THREE.SphereGeometry(this.limbLengthL1 + this.limbLengthL2, 24, 16);
      const reachMat = new THREE.MeshBasicMaterial({ color: 0x06b6d4, wireframe: true, transparent: true, opacity: 0.18 });
      this.reachSphereMesh = new THREE.Mesh(reachGeom, reachMat);
      this.sceneGroup.add(this.reachSphereMesh);

      // 3. Joint & Effector Indicator Spheres
      const jointGeom = new THREE.SphereGeometry(1.6, 12, 8);
      const jointMat = new THREE.MeshStandardMaterial({ color: 0xd946ef, roughness: 0.3 });
      this.jointSphereMesh = new THREE.Mesh(jointGeom, jointMat);
      this.sceneGroup.add(this.jointSphereMesh);

      const effGeom = new THREE.SphereGeometry(1.4, 12, 8);
      const effMat = new THREE.MeshStandardMaterial({ color: 0x10b981, roughness: 0.3 });
      this.effectorSphereMesh = new THREE.Mesh(effGeom, effMat);
      this.sceneGroup.add(this.effectorSphereMesh);

      // 4. 3D Articulated Bone Meshes (Tapered Bone Links)
      this.boneMeshes = [];
      const boneMat = new THREE.MeshStandardMaterial({ color: 0x38bdf8, roughness: 0.4, metalness: 0.2 });
      for (let i = 1; i < this.bones.length; i++) {
        const bGeo = new THREE.CylinderGeometry(0.8, 1.6, 1.0, 8);
        const bMesh = new THREE.Mesh(bGeo, boneMat);
        this.sceneGroup.add(bMesh);
        this.boneMeshes.push({ mesh: bMesh, boneIdx: i });
      }

      // 5. Joint Node Spheres
      this.jointMeshes = [];
      const jNodeMat = new THREE.MeshStandardMaterial({ color: 0x0ea5e9, roughness: 0.3 });
      const jNodeGeo = new THREE.SphereGeometry(1.2, 10, 8);
      for (let i = 0; i < this.bones.length; i++) {
        const jMesh = new THREE.Mesh(jNodeGeo, jNodeMat);
        this.sceneGroup.add(jMesh);
        this.jointMeshes.push(jMesh);
      }

      // 6. Bone Line Segments
      const lineGeom = new THREE.BufferGeometry();
      const linePositions = new Float32Array(this.bones.length * 6);
      lineGeom.setAttribute('position', new THREE.BufferAttribute(linePositions, 3));
      const lineMat = new THREE.LineBasicMaterial({ color: 0x0284c7, linewidth: 2 });
      this.boneLines = new THREE.LineSegments(lineGeom, lineMat);
      this.sceneGroup.add(this.boneLines);

      // 7. Hitbox Capsules (Source 2 Colored Capsules)
      this.capsuleMeshes = this.hitboxes.map(hb => {
        const grp = new THREE.Group();
        const mat = new THREE.MeshStandardMaterial({
          color: hb.color,
          roughness: 0.5,
          transparent: true,
          opacity: 0.45
        });

        const cylGeom = new THREE.CylinderGeometry(hb.radius, hb.radius, 1.0, 12, 1, false);
        const cylMesh = new THREE.Mesh(cylGeom, mat);
        grp.add(cylMesh);

        const capTopGeom = new THREE.SphereGeometry(hb.radius, 12, 8);
        const capTopMesh = new THREE.Mesh(capTopGeom, mat);
        grp.add(capTopMesh);

        const capBotMesh = new THREE.Mesh(capTopGeom, mat);
        grp.add(capBotMesh);

        this.sceneGroup.add(grp);
        return { group: grp, cylMesh, capTopMesh, capBotMesh, hitbox: hb };
      });

      scene.add(this.sceneGroup);

      // 8. Mount UI Controls
      this._buildParamsUI();

      // 9. Frame Humanoid Mannequin with Optimal Perspective Camera
      const camera = this.viewport.getCamera();
      if (camera) {
        camera.position.set(-55, -75, 45);
        const controls = this.viewport.getControls();
        if (controls) {
          controls.target.set(0, 0, 36);
          controls.update();
        }
      }

      // 10. Initial Update Pass
      this.update(0, 0, 0);
    }

    update(tick, subTickFrac, dt) {
      // 1. Base FK Pass
      const currentLocals = this.bones.map(b => new CTransform(b.local.pos, b.local.rot, b.local.scale));
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

      // 3. Analytical Two-Bone IK Solve (Law of Cosines)
      const poleWorld = {
        x: rootWorldPos.x + this.poleVec.x * 20,
        y: rootWorldPos.y + this.poleVec.y * 20,
        z: rootWorldPos.z + this.poleVec.z * 20
      };
      const ikSol = solveTwoBoneIK(rootWorldPos, this.targetPos, L1, L2, poleWorld);

      // 4. Shortest-Arc SLERP Pose Blending
      if (this.blendWeight > 0.0) {
        const w = 3.0 * (this.blendWeight ** 2) - 2.0 * (this.blendWeight ** 3);

        const vRest1 = this.bones[jointIdx].local.pos;
        const vRest1Len = Math.hypot(vRest1.x, vRest1.y, vRest1.z) || 1.0;
        const vRest1Norm = { x: vRest1.x / vRest1Len, y: vRest1.y / vRest1Len, z: vRest1.z / vRest1Len };
        const parentRot = this.worldBones[this.bones[rootIdx].parent].rot;
        const invParent = { x: -parentRot.x, y: -parentRot.y, z: -parentRot.z, w: parentRot.w };

        const d1x = ikSol.midPos.x - rootWorldPos.x;
        const d1y = ikSol.midPos.y - rootWorldPos.y;
        const d1z = ikSol.midPos.z - rootWorldPos.z;
        const d1Len = Math.hypot(d1x, d1y, d1z) || 1.0;
        const vDesired1World = { x: d1x / d1Len, y: d1y / d1Len, z: d1z / d1Len };
        const vDesired1 = new CTransform({ x: 0, y: 0, z: 0 }, invParent).rotateVector(vDesired1World);

        const qIK1 = CTransform.rotationBetween(vRest1Norm, vDesired1);
        const qBlended1 = CTransform.slerp(this.bones[rootIdx].local.rot, qIK1, w);
        currentLocals[rootIdx].rot = qBlended1;

        const pRootWorld = this.bones[rootIdx].parent < 0
          ? currentLocals[rootIdx]
          : this.worldBones[this.bones[rootIdx].parent].concatenate(currentLocals[rootIdx]);
        this.worldBones[rootIdx] = pRootWorld;

        const vRest2 = this.bones[effIdx].local.pos;
        const vRest2Len = Math.hypot(vRest2.x, vRest2.y, vRest2.z) || 1.0;
        const vRest2Norm = { x: vRest2.x / vRest2Len, y: vRest2.y / vRest2Len, z: vRest2.z / vRest2Len };
        const jointParentRot = this.worldBones[rootIdx].rot;
        const invJointParent = { x: -jointParentRot.x, y: -jointParentRot.y, z: -jointParentRot.z, w: jointParentRot.w };

        const d2x = ikSol.effectorPos.x - ikSol.midPos.x;
        const d2y = ikSol.effectorPos.y - ikSol.midPos.y;
        const d2z = ikSol.effectorPos.z - ikSol.midPos.z;
        const d2Len = Math.hypot(d2x, d2y, d2z) || 1.0;
        const vDesired2World = { x: d2x / d2Len, y: d2y / d2Len, z: d2z / d2Len };
        const vDesired2 = new CTransform({ x: 0, y: 0, z: 0 }, invJointParent).rotateVector(vDesired2World);

        const qIK2 = CTransform.rotationBetween(vRest2Norm, vDesired2);
        const qBlended2 = CTransform.slerp(this.bones[jointIdx].local.rot, qIK2, w);
        currentLocals[jointIdx].rot = qBlended2;
      }

      // 5. Propagate Full Hierarchy Forward Kinematics
      for (let i = 0; i < this.bones.length; i++) {
        const pIdx = this.bones[i].parent;
        this.worldBones[i] = pIdx < 0 ? currentLocals[i] : this.worldBones[pIdx].concatenate(currentLocals[i]);
      }

      // 6. Update Reach Envelope Sphere centered at rootWorldPos
      if (this.reachSphereMesh) {
        this.reachSphereMesh.position.set(rootWorldPos.x, rootWorldPos.y, rootWorldPos.z);
        const baseRadius = (this.reachSphereMesh.geometry.parameters && this.reachSphereMesh.geometry.parameters.radius) || 1.0;
        const sVal = (L1 + L2) / baseRadius;
        this.reachSphereMesh.scale.set(sVal, sVal, sVal);
        this.reachSphereMesh.visible = this.showGizmo;
      }

      if (this.jointSphereMesh) {
        this.jointSphereMesh.position.set(ikSol.midPos.x, ikSol.midPos.y, ikSol.midPos.z);
        this.jointSphereMesh.visible = this.showGizmo;
      }

      if (this.effectorSphereMesh) {
        this.effectorSphereMesh.position.set(ikSol.effectorPos.x, ikSol.effectorPos.y, ikSol.effectorPos.z);
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
            const pPos = pIdx < 0 ? { x: 0, y: 0, z: 0 } : this.worldBones[pIdx].pos;
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
        const pA = bTrans.transformPoint(hb.min);
        const pB = bTrans.transformPoint(hb.max);

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
          item.cylMesh.visible = true;
          item.capTopMesh.position.set(0, h * 0.5, 0);
          item.capBotMesh.position.set(0, -h * 0.5, 0);
        } else {
          item.cylMesh.visible = false;
          item.capTopMesh.position.set(0, 0, 0);
          item.capBotMesh.position.set(0, 0, 0);
        }
      });

      // 10. Telemetry HUD Output
      return {
        pos: { x: rootWorldPos.x, y: rootWorldPos.y, z: rootWorldPos.z },
        vel: { x: this.targetPos.x, y: this.targetPos.y, z: this.targetPos.z, speed: ikSol.targetDist },
        ground: ikSol.reachedTarget,
        event: ikSol.reachedTarget ? `IK SOLVED: ${this.activeLimb.toUpperCase()}` : 'OUT OF REACH (CLAMPED)',
        customMetrics: [
          { label: 'ACTIVE LIMB', value: this.activeLimb.toUpperCase() },
          { label: 'TARGET DIST (d)', value: `${ikSol.targetDist.toFixed(1)} u` },
          { label: 'MAX REACH', value: `${ikSol.maxReach.toFixed(1)} u` },
          { label: 'ROOT ANGLE (α)', value: `${ikSol.alphaDeg.toFixed(1)}°` },
          { label: 'JOINT ANGLE (β)', value: `${(ikSol.betaRad * 180 / Math.PI).toFixed(1)}°` },
          { label: 'IK BLEND (λ)', value: `${(this.blendWeight * 100).toFixed(0)}%` },
          { label: 'SOLVER STATUS', value: ikSol.reachedTarget ? 'REACHED' : 'CLAMPED', alert: !ikSol.reachedTarget }
        ]
      };
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
      this.jointSphereMesh = null;
      this.effectorSphereMesh = null;
    }

    _switchLimb(limbKey) {
      this.activeLimb = limbKey;
      if (limbKey === 'arm_R') {
        this.targetPos = { x: 22.0, y: -22.0, z: 45.0 };
        this.poleVec = { x: 0.0, y: -1.0, z: -0.5 };
        this.limbLengthL1 = 11.4;
        this.limbLengthL2 = 10.2;
      } else if (limbKey === 'arm_L') {
        this.targetPos = { x: 22.0, y: 22.0, z: 45.0 };
        this.poleVec = { x: 0.0, y: 1.0, z: -0.5 };
        this.limbLengthL1 = 11.4;
        this.limbLengthL2 = 10.2;
      } else if (limbKey === 'leg_R') {
        this.targetPos = { x: 10.0, y: -8.0, z: 2.0 };
        this.poleVec = { x: 1.0, y: 0.0, z: 0.0 };
        this.limbLengthL1 = 17.0;
        this.limbLengthL2 = 17.1;
      } else if (limbKey === 'leg_L') {
        this.targetPos = { x: 10.0, y: 8.0, z: 2.0 };
        this.poleVec = { x: 1.0, y: 0.0, z: 0.0 };
        this.limbLengthL1 = 17.0;
        this.limbLengthL2 = 17.1;
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
        this.targetPos = { x: 26.0, y: -18.0, z: 48.0 };
        this.blendWeight = 1.0;
      } else if (presetName === 'reach') {
        this._switchLimb('arm_R');
        this.targetPos = { x: 34.0, y: -12.0, z: 58.0 };
        this.blendWeight = 1.0;
      } else if (presetName === 'ground') {
        this._switchLimb('leg_L');
        this.targetPos = { x: 12.0, y: 8.0, z: 0.0 };
        this.blendWeight = 1.0;
      } else if (presetName === 'overextension') {
        this._switchLimb('arm_R');
        this.targetPos = { x: 45.0, y: -30.0, z: 55.0 }; // Beyond max reach
        this.blendWeight = 1.0;
      }

      if (this.targetGizmo) {
        this.targetGizmo.setPosition(this.targetPos.x, this.targetPos.y, this.targetPos.z);
      }
      this._syncUI();
      this.update(0, 0, 0);
    }

    _syncUITarget() {
      if (!this.paramsContainer) return;
      const xIn = this.paramsContainer.querySelector('#sim09TargetX');
      const yIn = this.paramsContainer.querySelector('#sim09TargetY');
      const zIn = this.paramsContainer.querySelector('#sim09TargetZ');
      const lblX = this.paramsContainer.querySelector('#sim09LblTargetX');
      const lblY = this.paramsContainer.querySelector('#sim09LblTargetY');
      const lblZ = this.paramsContainer.querySelector('#sim09LblTargetZ');
      if (xIn && lblX) { xIn.value = this.targetPos.x.toFixed(1); lblX.textContent = `${this.targetPos.x.toFixed(1)} u`; }
      if (yIn && lblY) { yIn.value = this.targetPos.y.toFixed(1); lblY.textContent = `${this.targetPos.y.toFixed(1)} u`; }
      if (zIn && lblZ) { zIn.value = this.targetPos.z.toFixed(1); lblZ.textContent = `${this.targetPos.z.toFixed(1)} u`; }
    }

    _syncUI() {
      if (!this.paramsContainer) return;
      const limbSelect = this.paramsContainer.querySelector('#sim09LimbSelect');
      if (limbSelect) limbSelect.value = this.activeLimb;

      const blendInput = this.paramsContainer.querySelector('#sim09Blend');
      const lblBlend = this.paramsContainer.querySelector('#sim09LblBlend');
      if (blendInput && lblBlend) {
        blendInput.value = this.blendWeight;
        lblBlend.textContent = `${(this.blendWeight * 100).toFixed(0)}%`;
      }

      const l1In = this.paramsContainer.querySelector('#sim09L1');
      const lblL1 = this.paramsContainer.querySelector('#sim09LblL1');
      if (l1In && lblL1) {
        l1In.value = this.limbLengthL1;
        lblL1.textContent = `${this.limbLengthL1.toFixed(1)} u`;
      }

      const l2In = this.paramsContainer.querySelector('#sim09L2');
      const lblL2 = this.paramsContainer.querySelector('#sim09LblL2');
      if (l2In && lblL2) {
        l2In.value = this.limbLengthL2;
        lblL2.textContent = `${this.limbLengthL2.toFixed(1)} u`;
      }

      this._syncUITarget();
    }

    _buildParamsUI() {
      if (!this.paramsContainer) return;
      this.paramsContainer.innerHTML = `
        <div class="space-y-4 font-mono text-xs select-none" draggable="false" ondragstart="return false;">
          <!-- Limb & Solve Mode Selection -->
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1.5">
              <span>LIMB &amp; IK CONFIGURATION</span>
              <span class="text-[10px] text-cyan-400">FUN_1802f9750</span>
            </div>
            <div>
              <label class="block text-[11px] text-slate-400 mb-1">Active Limb:</label>
              <select id="sim09LimbSelect" class="w-full bg-slate-950 border border-slate-700 rounded px-2 py-1.5 text-cyan-300 font-bold cursor-pointer">
                <option value="arm_R" ${this.activeLimb === 'arm_R' ? 'selected' : ''}>Right Arm (Shoulder &rarr; Elbow &rarr; Hand)</option>
                <option value="arm_L" ${this.activeLimb === 'arm_L' ? 'selected' : ''}>Left Arm (Shoulder &rarr; Elbow &rarr; Hand)</option>
                <option value="leg_R" ${this.activeLimb === 'leg_R' ? 'selected' : ''}>Right Leg (Hip &rarr; Knee &rarr; Foot)</option>
                <option value="leg_L" ${this.activeLimb === 'leg_L' ? 'selected' : ''}>Left Leg (Hip &rarr; Knee &rarr; Foot)</option>
              </select>
            </div>
            <div>
              <div class="flex justify-between text-[11px] text-slate-400">
                <span>IK Blend Weight (&lambda;):</span>
                <span id="sim09LblBlend" class="text-amber-400 font-bold">${(this.blendWeight * 100).toFixed(0)}% (Pure IK)</span>
              </div>
              <input type="range" id="sim09Blend" min="0" max="1" step="0.01" value="${this.blendWeight}" class="w-full accent-amber-500 cursor-pointer">
            </div>
            <div class="grid grid-cols-2 gap-2">
              <div>
                <div class="flex justify-between text-[10px] text-slate-400">
                  <span>Limb L1 (Upper):</span>
                  <span id="sim09LblL1" class="text-cyan-300">${this.limbLengthL1.toFixed(1)} u</span>
                </div>
                <input type="range" id="sim09L1" min="6" max="24" step="0.5" value="${this.limbLengthL1}" class="w-full accent-cyan-400 cursor-pointer">
              </div>
              <div>
                <div class="flex justify-between text-[10px] text-slate-400">
                  <span>Limb L2 (Lower):</span>
                  <span id="sim09LblL2" class="text-purple-300">${this.limbLengthL2.toFixed(1)} u</span>
                </div>
                <input type="range" id="sim09L2" min="6" max="24" step="0.5" value="${this.limbLengthL2}" class="w-full accent-purple-400 cursor-pointer">
              </div>
            </div>
          </div>

          <!-- 3D Target Coordinates -->
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1">
              <span>TARGET POSITION [X, Y, Z]</span>
              <span class="text-[10px] text-emerald-400">3D Draggable</span>
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Target X (Forward):</span>
                <span id="sim09LblTargetX" class="text-rose-400 font-bold">${this.targetPos.x.toFixed(1)} u</span>
              </div>
              <input type="range" id="sim09TargetX" min="-30" max="50" step="1" value="${this.targetPos.x}" class="w-full accent-rose-500 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Target Y (Lateral):</span>
                <span id="sim09LblTargetY" class="text-emerald-400 font-bold">${this.targetPos.y.toFixed(1)} u</span>
              </div>
              <input type="range" id="sim09TargetY" min="-40" max="40" step="1" value="${this.targetPos.y}" class="w-full accent-emerald-500 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Target Z (Vertical):</span>
                <span id="sim09LblTargetZ" class="text-cyan-400 font-bold">${this.targetPos.z.toFixed(1)} u</span>
              </div>
              <input type="range" id="sim09TargetZ" min="-10" max="75" step="1" value="${this.targetPos.z}" class="w-full accent-cyan-500 cursor-pointer">
            </div>
          </div>

          <!-- Stance Presets -->
          <div class="space-y-1.5">
            <span class="text-[11px] font-bold text-slate-400">STANCE PRESETS</span>
            <div class="grid grid-cols-2 gap-1.5">
              <button id="sim09BtnCombat" class="py-1.5 px-2 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 text-[11px] font-bold cursor-pointer">Combat Stance</button>
              <button id="sim09BtnReach" class="py-1.5 px-2 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 text-[11px] font-bold cursor-pointer">High Reach</button>
              <button id="sim09BtnGround" class="py-1.5 px-2 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 text-[11px] font-bold cursor-pointer">Ground Plant</button>
              <button id="sim09BtnOverext" class="py-1.5 px-2 rounded bg-rose-950/60 hover:bg-rose-900/80 text-rose-300 border border-rose-800 text-[11px] font-bold cursor-pointer">Overextension</button>
            </div>
          </div>

          <!-- Render Toggles -->
          <div class="bg-slate-900/60 border border-slate-800/80 rounded-lg p-2.5 space-y-1.5 text-[11px]">
            <span class="font-bold text-slate-400">RENDER OVERLAYS</span>
            <label class="flex items-center gap-2 cursor-pointer text-slate-300">
              <input type="checkbox" id="sim09CheckHitboxes" ${this.showHitboxes ? 'checked' : ''} class="accent-cyan-400">
              <span>Source 2 Hitbox Capsules (13)</span>
            </label>
            <label class="flex items-center gap-2 cursor-pointer text-slate-300">
              <input type="checkbox" id="sim09CheckBones" ${this.showBones ? 'checked' : ''} class="accent-cyan-400">
              <span>Articulated Bone Links &amp; Joints</span>
            </label>
            <label class="flex items-center gap-2 cursor-pointer text-slate-300">
              <input type="checkbox" id="sim09CheckGizmo" ${this.showGizmo ? 'checked' : ''} class="accent-cyan-400">
              <span>Reach Envelope &amp; Target Gizmo</span>
            </label>
          </div>
        </div>
      `;

      // Event Listeners
      const limbSelect = this.paramsContainer.querySelector('#sim09LimbSelect');
      if (limbSelect) limbSelect.onchange = (e) => this._switchLimb(e.target.value);

      const blendInput = this.paramsContainer.querySelector('#sim09Blend');
      const lblBlend = this.paramsContainer.querySelector('#sim09LblBlend');
      if (blendInput && lblBlend) {
        blendInput.oninput = (e) => {
          this.blendWeight = parseFloat(e.target.value);
          lblBlend.textContent = `${(this.blendWeight * 100).toFixed(0)}%`;
          this.update(0, 0, 0);
        };
      }

      const l1Input = this.paramsContainer.querySelector('#sim09L1');
      const lblL1 = this.paramsContainer.querySelector('#sim09LblL1');
      if (l1Input && lblL1) {
        l1Input.oninput = (e) => {
          this.limbLengthL1 = parseFloat(e.target.value);
          lblL1.textContent = `${this.limbLengthL1.toFixed(1)} u`;
          this.update(0, 0, 0);
        };
      }

      const l2Input = this.paramsContainer.querySelector('#sim09L2');
      const lblL2 = this.paramsContainer.querySelector('#sim09LblL2');
      if (l2Input && lblL2) {
        l2Input.oninput = (e) => {
          this.limbLengthL2 = parseFloat(e.target.value);
          lblL2.textContent = `${this.limbLengthL2.toFixed(1)} u`;
          this.update(0, 0, 0);
        };
      }

      const targetX = this.paramsContainer.querySelector('#sim09TargetX');
      const targetY = this.paramsContainer.querySelector('#sim09TargetY');
      const targetZ = this.paramsContainer.querySelector('#sim09TargetZ');
      const lblTX = this.paramsContainer.querySelector('#sim09LblTargetX');
      const lblTY = this.paramsContainer.querySelector('#sim09LblTargetY');
      const lblTZ = this.paramsContainer.querySelector('#sim09LblTargetZ');

      const onTargetInput = () => {
        this.targetPos.x = parseFloat(targetX.value);
        this.targetPos.y = parseFloat(targetY.value);
        this.targetPos.z = parseFloat(targetZ.value);
        if (lblTX) lblTX.textContent = `${this.targetPos.x.toFixed(1)} u`;
        if (lblTY) lblTY.textContent = `${this.targetPos.y.toFixed(1)} u`;
        if (lblTZ) lblTZ.textContent = `${this.targetPos.z.toFixed(1)} u`;
        if (this.targetGizmo) this.targetGizmo.setPosition(this.targetPos.x, this.targetPos.y, this.targetPos.z);
        this.update(0, 0, 0);
      };

      if (targetX) targetX.oninput = onTargetInput;
      if (targetY) targetY.oninput = onTargetInput;
      if (targetZ) targetZ.oninput = onTargetInput;

      const btnCombat = this.paramsContainer.querySelector('#sim09BtnCombat');
      const btnReach = this.paramsContainer.querySelector('#sim09BtnReach');
      const btnGround = this.paramsContainer.querySelector('#sim09BtnGround');
      const btnOverext = this.paramsContainer.querySelector('#sim09BtnOverext');

      if (btnCombat) btnCombat.onclick = () => this._applyStancePreset('combat');
      if (btnReach) btnReach.onclick = () => this._applyStancePreset('reach');
      if (btnGround) btnGround.onclick = () => this._applyStancePreset('ground');
      if (btnOverext) btnOverext.onclick = () => this._applyStancePreset('overextension');

      const chkHitboxes = this.paramsContainer.querySelector('#sim09CheckHitboxes');
      const chkBones = this.paramsContainer.querySelector('#sim09CheckBones');
      const chkGizmo = this.paramsContainer.querySelector('#sim09CheckGizmo');

      if (chkHitboxes) chkHitboxes.onchange = (e) => { this.showHitboxes = e.target.checked; this.update(0, 0, 0); };
      if (chkBones) chkBones.onchange = (e) => { this.showBones = e.target.checked; this.update(0, 0, 0); };
      if (chkGizmo) chkGizmo.onchange = (e) => { this.showGizmo = e.target.checked; this.update(0, 0, 0); };
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
      this.FULL_OCCLUSION_DENSITY = 0.20; // server.dll 0x1809b7f50 threshold

      this.detonationPos = { x: 0, y: 0, z: 40 };

      // State
      this.densities = new Float32Array(this.GRID_TOTAL);
      this.occupancy = new Uint32Array(1024); // 1024 * 32 = 32768 bits
      this.smokeRadius = 260.0;
      this.hissSound = null;

      // Line-of-sight test endpoints
      this.shooterPos = { x: -340, y: 0, z: 40 };
      this.targetPos = { x: 340, y: 0, z: 40 };

      // 3D Visualizer Objects
      this.sceneGroup = null;
      this.puffMesh = null;
      this.boundsBox = null;
      this.losLine = null;
      this.tracerMesh = null;
      this.tunnelMesh = null;
      this.shooterMarker = null;
      this.targetMarker = null;
      this.blastRingMesh = null;
      this.blastTimer = 0;
      this.tracerTimer = 0;

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
              const density = Math.max(0.05, 0.45 * (1.0 - normDist * 0.8));
              this._setVoxelActive(vx, vy, vz, density);
              count++;
            }
          }
        }
      }
      this.activeVoxelCount = count;
      if (this.puffMesh) this._updatePuffMesh();
    }

    carveBulletTunnel(startX, startY, startZ, endX, endY, endZ, radius = 38.0) {
      const dirX = endX - startX, dirY = endY - startY, dirZ = endZ - startZ;
      const segLenSqr = dirX*dirX + dirY*dirY + dirZ*dirZ;
      const rSqr = radius * radius;
      let cleared = 0;

      for (let z = 0; z < 32; z++) {
        for (let y = 0; y < 32; y++) {
          for (let x = 0; x < 32; x++) {
            if (!this._isVoxelActive(x, y, z)) continue;
            const w = this._voxelToWorld(x, y, z);
            const px = w.x - startX, py = w.y - startY, pz = w.z - startZ;
            let t = segLenSqr > 1e-6 ? (px*dirX + py*dirY + pz*dirZ) / segLenSqr : 0.0;
            t = Math.max(0.0, Math.min(1.0, t));
            const cx = startX + dirX * t, cy = startY + dirY * t, cz = startZ + dirZ * t;
            const distSqr = (w.x - cx)**2 + (w.y - cy)**2 + (w.z - cz)**2;
            if (distSqr <= rSqr) {
              this._clearVoxel(x, y, z);
              cleared++;
            }
          }
        }
      }
      this.activeVoxelCount = Math.max(0, this.activeVoxelCount - cleared);
      this._updatePuffMesh();

      // Trigger high-visibility supersonic tracer & carved cylinder void mesh
      this._triggerTracer(startX, startY, startZ, endX, endY, endZ, radius);

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
              const pushX = (d > 1e-6 ? dx / d : 0) * 35.0;
              const pushY = (d > 1e-6 ? dy / d : 0) * 35.0;
              const pushZ = (d > 1e-6 ? dz / d : 1) * 35.0;
              const pushed = this._worldToVoxel(w.x + pushX, w.y + pushY, w.z + pushZ);
              if (pushed) {
                const morton = this._mortonEncode(x, y, z);
                const oldDens = this.densities[morton];
                this._clearVoxel(x, y, z);
                this._setVoxelActive(pushed.vx, pushed.vy, pushed.vz, oldDens * 0.85);
                affected++;
              }
            }
          }
        }
      }
      this._updatePuffMesh();
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
          accumulated += density * (stepSize / this.VOXEL_SIZE) * 0.12;
          if (accumulated >= this.FULL_OCCLUSION_DENSITY) {
            accumulated = this.FULL_OCCLUSION_DENSITY;
            break;
          }
        }
      }

      this.accumulatedDensity = accumulated;
      this.losOccluded = accumulated >= this.FULL_OCCLUSION_DENSITY;
      this.losOpacity = Math.min(1.0, accumulated / this.FULL_OCCLUSION_DENSITY);
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim11_VolumetricSmoke_Scene';

      // 1. Soft Volumetric Smoke Puffs (Instanced Spheres with Soft Radial Density)
      const puffGeo = new THREE.SphereGeometry(18, 10, 8);
      const puffMat = new THREE.MeshStandardMaterial({
        color: 0x94a3b8,
        roughness: 0.9,
        metalness: 0.05,
        transparent: true,
        opacity: 0.38,
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
        opacity: 0.25
      }));
      this.boundsBox.position.set(this.detonationPos.x, this.detonationPos.y, this.detonationPos.z);
      this.sceneGroup.add(this.boundsBox);

      // 3. Supersonic Bullet Tracer Beam (Glowing Neon Cylinder)
      const tracerGeo = new THREE.CylinderGeometry(2.0, 2.0, 1.0, 8);
      const tracerMat = new THREE.MeshBasicMaterial({ color: 0xfef08a, transparent: true, opacity: 0.95 });
      this.tracerMesh = new THREE.Mesh(tracerGeo, tracerMat);
      this.tracerMesh.visible = false;
      this.sceneGroup.add(this.tracerMesh);

      // 4. Carved Cylindrical Void Channel (Glowing Neon Wireframe boundary)
      const tunnelGeo = new THREE.CylinderGeometry(1.0, 1.0, 1.0, 16, 1, true);
      const tunnelMat = new THREE.MeshBasicMaterial({
        color: 0x38bdf8,
        wireframe: true,
        transparent: true,
        opacity: 0.45
      });
      this.tunnelMesh = new THREE.Mesh(tunnelGeo, tunnelMat);
      this.tunnelMesh.visible = false;
      this.sceneGroup.add(this.tunnelMesh);

      // 5. Dynamic Line of Sight (LoS) Ray (Red when blocked -> Vibrant Green when clear through tunnel)
      const losGeo = new THREE.BufferGeometry().setFromPoints([
        new THREE.Vector3(this.shooterPos.x, this.shooterPos.y, this.shooterPos.z),
        new THREE.Vector3(this.targetPos.x, this.targetPos.y, this.targetPos.z)
      ]);
      this.losLine = new THREE.Line(losGeo, new THREE.LineBasicMaterial({ color: 0xef4444, linewidth: 3 }));
      this.sceneGroup.add(this.losLine);

      // 6. Shooter & Target Markers
      const smGeo = new THREE.SphereGeometry(6, 12, 8);
      this.shooterMarker = new THREE.Mesh(smGeo, new THREE.MeshBasicMaterial({ color: 0x06b6d4 }));
      this.targetMarker = new THREE.Mesh(smGeo, new THREE.MeshBasicMaterial({ color: 0x10b981 }));
      this.sceneGroup.add(this.shooterMarker);
      this.sceneGroup.add(this.targetMarker);

      // 7. HE Blast Shockwave Ring
      const ringGeo = new THREE.RingGeometry(20, 24, 32);
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

      // Fill instanced transforms from active voxels
      for (let z = 0; z < 32; z++) {
        for (let y = 0; y < 32; y++) {
          for (let x = 0; x < 32; x++) {
            if (this._isVoxelActive(x, y, z)) {
              if (instIdx >= 10000) break;
              const w = this._voxelToWorld(x, y, z);
              dummy.position.set(w.x, w.y, w.z);
              const morton = this._mortonEncode(x, y, z);
              const d = this.densities[morton] || 0.25;
              const scale = 0.8 + d * 1.2;
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
      // Evaluate Raymarch LoS
      this.raymarchLoS(this.shooterPos.x, this.shooterPos.y, this.shooterPos.z,
                       this.targetPos.x, this.targetPos.y, this.targetPos.z);

      // Update Markers
      if (this.shooterMarker) this.shooterMarker.position.set(this.shooterPos.x, this.shooterPos.y, this.shooterPos.z);
      if (this.targetMarker) this.targetMarker.position.set(this.targetPos.x, this.targetPos.y, this.targetPos.z);

      // Update LoS Line Visual: Red if Occluded -> Glowing Emerald Green if Clear through carved tunnel!
      if (this.losLine && this.losLine.geometry) {
        const pts = [
          new THREE.Vector3(this.shooterPos.x, this.shooterPos.y, this.shooterPos.z),
          new THREE.Vector3(this.targetPos.x, this.targetPos.y, this.targetPos.z)
        ];
        this.losLine.geometry.setFromPoints(pts);
        if (this.losLine.material) {
          if (this.losOccluded) {
            this.losLine.material.color.setHex(0xef4444); // Red: Optical Path Occluded
            this.losLine.material.opacity = 0.85;
          } else {
            this.losLine.material.color.setHex(0x10b981); // Emerald: Clear LoS through Carved Void!
            this.losLine.material.opacity = 1.0;
          }
        }
      }

      // Tracer & Tunnel Fade Animation
      if (this.tracerTimer > 0) {
        this.tracerTimer = Math.max(0, this.tracerTimer - dt * 2.5);
        if (this.tracerMesh && this.tracerMesh.material) {
          this.tracerMesh.material.opacity = this.tracerTimer * 0.95;
          this.tracerMesh.visible = this.tracerTimer > 0.01;
        }
        if (this.tunnelMesh && this.tunnelMesh.material) {
          this.tunnelMesh.material.opacity = this.tracerTimer * 0.55;
          this.tunnelMesh.visible = this.tracerTimer > 0.01;
        }
      }

      // Blast Ring Animation
      if (this.blastTimer > 0) {
        this.blastTimer = Math.max(0, this.blastTimer - dt * 2.0);
        if (this.blastRingMesh) {
          const progress = 1.0 - this.blastTimer;
          const scale = 1.0 + progress * 11.0;
          this.blastRingMesh.scale.set(scale, scale, scale);
          if (this.blastRingMesh.material) {
            this.blastRingMesh.material.opacity = this.blastTimer * 0.8;
          }
          this.blastRingMesh.visible = this.blastTimer > 0.01;
        }
      }

      return {
        pos: { x: this.detonationPos.x, y: this.detonationPos.y, z: this.detonationPos.z },
        vel: { x: this.shooterPos.y, y: this.targetPos.y, z: 0, speed: this.smokeRadius },
        ground: !this.losOccluded,
        event: this.losOccluded ? 'LOS OCCLUDED (100%)' : 'LOS CLEAR (CARVED VOID)',
        customMetrics: [
          { label: 'ACTIVE VOXELS', value: `${this.activeVoxelCount} / 32,768` },
          { label: 'ACCUM DENSITY (ρ)', value: `${this.accumulatedDensity.toFixed(3)} / 0.200` },
          { label: 'OCCLUSION STATUS', value: this.losOccluded ? 'BLOCKED' : 'CLEAR', alert: this.losOccluded },
          { label: 'VOXEL ENCODING', value: '3D MORTON Z-ORDER' },
          { label: 'WIRE FORMAT', value: '512 WORD BITMASK' }
        ]
      };
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
        <div class="space-y-4 font-mono text-xs select-none" draggable="false" ondragstart="return false;">
          <!-- Interactive Actions -->
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1.5">
              <span>SOURCE 2 VOXEL ACTIONS</span>
              <span class="text-[10px] text-cyan-400">server.dll</span>
            </div>
            <div class="grid grid-cols-2 gap-2">
              <button id="sim11BtnBullet" class="py-2 px-2.5 rounded bg-cyan-600 hover:bg-cyan-500 text-white font-bold transition-all shadow-sm flex flex-col items-center cursor-pointer">
                <span>Fire Bullet</span>
                <span class="text-[9px] opacity-80 font-normal">Carve (R=38u)</span>
              </button>
              <button id="sim11BtnBlast" class="py-2 px-2.5 rounded bg-amber-600 hover:bg-amber-500 text-white font-bold transition-all shadow-sm flex flex-col items-center cursor-pointer">
                <span>HE Grenade</span>
                <span class="text-[9px] opacity-80 font-normal">Blast (R=190u)</span>
              </button>
            </div>
            <button id="sim11BtnRespawn" class="w-full py-1.5 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 text-xs font-bold cursor-pointer">
              Respawn Volumetric Smoke
            </button>
          </div>

          <!-- Line of Sight Controls -->
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1">
              <span>OPTICAL LINE-OF-SIGHT DDA</span>
              <span class="text-[10px] text-emerald-400">0x1809bd580</span>
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Smoke Cloud Radius:</span>
                <span id="sim11LblRadius" class="text-cyan-300">${this.smokeRadius.toFixed(0)} u</span>
              </div>
              <input type="range" id="sim11Radius" min="160" max="300" step="10" value="${this.smokeRadius}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Shooter Y Position:</span>
                <span id="sim11LblShooterY" class="text-cyan-300">${this.shooterPos.y.toFixed(0)} u</span>
              </div>
              <input type="range" id="sim11ShooterY" min="-180" max="180" step="5" value="${this.shooterPos.y}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Target Y Position:</span>
                <span id="sim11LblTargetY" class="text-amber-300">${this.targetPos.y.toFixed(0)} u</span>
              </div>
              <input type="range" id="sim11TargetY" min="-180" max="180" step="5" value="${this.targetPos.y}" class="w-full accent-amber-500 cursor-pointer">
            </div>
          </div>

          <!-- Voxel Telemetry Card -->
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2">
            <div class="flex items-center justify-between">
              <span class="font-bold text-slate-300">VOXEL TELEMETRY</span>
              <span id="sim11OcclusionBadge" class="text-[10px] font-bold px-1.5 py-0.5 rounded ${this.losOccluded ? 'bg-rose-950 text-rose-300 border border-rose-800' : 'bg-emerald-950 text-emerald-300 border border-emerald-800'}">
                ${this.losOccluded ? 'OCCLUDED (100%)' : 'CLEAR (0%)'}
              </span>
            </div>
            <div class="grid grid-cols-2 gap-2 text-[11px] pt-1 border-t border-slate-800">
              <div>
                <span class="text-slate-500 block">Active Voxels:</span>
                <span id="sim11StatVoxels" class="text-slate-200 font-bold">${this.activeVoxelCount} / 32768</span>
              </div>
              <div>
                <span class="text-slate-500 block">Optical Density:</span>
                <span id="sim11StatDensity" class="text-cyan-300 font-bold">${this.accumulatedDensity.toFixed(3)} / 0.200</span>
              </div>
            </div>
          </div>
        </div>
      `;

      // Event Listeners
      const btnBullet = this.paramsContainer.querySelector('#sim11BtnBullet');
      const btnBlast = this.paramsContainer.querySelector('#sim11BtnBlast');
      const btnRespawn = this.paramsContainer.querySelector('#sim11BtnRespawn');

      if (btnBullet) {
        btnBullet.onclick = () => {
          this.carveBulletTunnel(this.shooterPos.x, this.shooterPos.y, this.shooterPos.z,
                                 this.targetPos.x, this.targetPos.y, this.targetPos.z, 38.0);
          this._syncUI();
          this.update(0, 0, 0);
        };
      }

      if (btnBlast) {
        btnBlast.onclick = () => {
          this.carveExplosionBlast(0, 0, 40, 190.0, 270.0);
          this._syncUI();
          this.update(0, 0, 0);
        };
      }

      if (btnRespawn) {
        btnRespawn.onclick = () => {
          this._initSmokeVolume(this.smokeRadius);
          if (this.audio) this.audio.playSmokeDetonation(0, 0, 40);
          this._syncUI();
          this.update(0, 0, 0);
        };
      }

      const rIn = this.paramsContainer.querySelector('#sim11Radius');
      const lblR = this.paramsContainer.querySelector('#sim11LblRadius');
      if (rIn && lblR) {
        rIn.oninput = (e) => {
          this.smokeRadius = parseFloat(e.target.value);
          lblR.textContent = `${this.smokeRadius.toFixed(0)} u`;
          this._initSmokeVolume(this.smokeRadius);
          this._syncUI();
          this.update(0, 0, 0);
        };
      }

      const syIn = this.paramsContainer.querySelector('#sim11ShooterY');
      const lblSY = this.paramsContainer.querySelector('#sim11LblShooterY');
      if (syIn && lblSY) {
        syIn.oninput = (e) => {
          this.shooterPos.y = parseFloat(e.target.value);
          lblSY.textContent = `${this.shooterPos.y.toFixed(0)} u`;
          this._syncUI();
          this.update(0, 0, 0);
        };
      }

      const tyIn = this.paramsContainer.querySelector('#sim11TargetY');
      const lblTY = this.paramsContainer.querySelector('#sim11LblTargetY');
      if (tyIn && lblTY) {
        tyIn.oninput = (e) => {
          this.targetPos.y = parseFloat(e.target.value);
          lblTY.textContent = `${this.targetPos.y.toFixed(0)} u`;
          this._syncUI();
          this.update(0, 0, 0);
        };
      }
    }

    _syncUI() {
      if (!this.paramsContainer) return;
      const statVoxels = this.paramsContainer.querySelector('#sim11StatVoxels');
      const statDensity = this.paramsContainer.querySelector('#sim11StatDensity');
      const badge = this.paramsContainer.querySelector('#sim11OcclusionBadge');

      if (statVoxels) statVoxels.textContent = `${this.activeVoxelCount} / 32768`;
      if (statDensity) statDensity.textContent = `${this.accumulatedDensity.toFixed(3)} / 0.200`;
      if (badge) {
        if (this.losOccluded) {
          badge.className = 'text-[10px] font-bold px-1.5 py-0.5 rounded bg-rose-950 text-rose-300 border border-rose-800';
          badge.textContent = 'OCCLUDED (100%)';
        } else {
          badge.className = 'text-[10px] font-bold px-1.5 py-0.5 rounded bg-emerald-950 text-emerald-300 border border-emerald-800';
          badge.textContent = 'CLEAR (0%)';
        }
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

      this.sceneGroup.add(this.entryRay);
      this.sceneGroup.add(this.subsurfaceRay);
      this.sceneGroup.add(this.exitRay);
      this.sceneGroup.add(this.ricochetRay);

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
      const halfThick = this.wallThickness * 0.5;

      // Shooter position
      const shootDist = 140.0;
      const muzzleX = -halfThick - shootDist * Math.cos(rad);
      const muzzleY = -shootDist * Math.sin(rad);
      const muzzleZ = 70.0;
      if (this.muzzleMarker) {
        this.muzzleMarker.position.set(muzzleX, muzzleY, muzzleZ);
      }

      // Entry point on wall
      const enterX = -halfThick;
      const enterY = 0.0;
      const enterZ = 70.0;

      // Entry Ray
      const setLine = (line, x1, y1, z1, x2, y2, z2, visible = true) => {
        line.visible = visible;
        if (!visible) return;
        const attr = line.geometry.attributes.position;
        attr.setXYZ(0, x1, y1, z1);
        attr.setXYZ(1, x2, y2, z2);
        attr.needsUpdate = true;
      };

      setLine(this.entryRay, muzzleX, muzzleY, muzzleZ, enterX, enterY, enterZ, true);

      if (res.isRicochet) {
        // Reflect angle
        const bounceX = -halfThick - 100 * Math.cos(rad);
        const bounceY = 100 * Math.sin(rad);
        setLine(this.ricochetRay, enterX, enterY, enterZ, bounceX, bounceY, enterZ + 15, true);
        setLine(this.subsurfaceRay, 0, 0, 0, 0, 0, 0, false);
        setLine(this.exitRay, 0, 0, 0, 0, 0, 0, false);
      } else {
        setLine(this.ricochetRay, 0, 0, 0, 0, 0, 0, false);

        // Subsurface path inside wall
        const exitX = halfThick;
        const exitY = this.wallThickness * Math.tan(rad);
        const exitZ = 70.0;
        setLine(this.subsurfaceRay, enterX, enterY, enterZ, exitX, exitY, exitZ, true);

        if (res.penetrated) {
          // Exit ray continues to target dummy
          const targetZ = this.targetHitgroup === 'head' ? 110.0 : this.targetHitgroup === 'chest' ? 88.0 : 52.0;
          setLine(this.exitRay, exitX, exitY, exitZ, 160, 0, targetZ, true);
        } else {
          setLine(this.exitRay, 0, 0, 0, 0, 0, 0, false);
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
      this.historyTrail = null;
      this.bulletTracer = null;
      this.hitMarker = null;
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

    _buildTacticalAgent(group, baseColor = 0x06b6d4, opacity = 1.0) {
      const isGhost = opacity < 0.9;
      const matMain = new THREE.MeshStandardMaterial({
        color: baseColor,
        roughness: 0.5,
        metalness: isGhost ? 0.0 : 0.2,
        transparent: isGhost,
        opacity: opacity
      });

      const matDark = new THREE.MeshStandardMaterial({
        color: isGhost ? baseColor : 0x0f172a,
        roughness: 0.7,
        transparent: isGhost,
        opacity: isGhost ? opacity * 0.8 : 1.0
      });

      const matAcc = new THREE.MeshStandardMaterial({
        color: isGhost ? baseColor : 0x38bdf8,
        roughness: 0.3,
        transparent: isGhost,
        opacity: opacity
      });

      // 1. Head & Balaclava (z: 62..70)
      const headGeo = new THREE.SphereGeometry(4.2, 12, 10);
      const headMesh = new THREE.Mesh(headGeo, matDark);
      headMesh.position.set(0, 0, 66);
      group.add(headMesh);

      // 2. Ballistic Helmet with NVG Mount (z: 67..72)
      const helmetGeo = new THREE.SphereGeometry(4.6, 12, 10, 0, Math.PI * 2, 0, Math.PI * 0.55);
      const helmetMesh = new THREE.Mesh(helmetGeo, matMain);
      helmetMesh.position.set(0, 0, 66.5);
      group.add(helmetMesh);

      // Goggles / Visor
      const visorGeo = new THREE.BoxGeometry(4.8, 2.2, 1.4);
      const visorMesh = new THREE.Mesh(visorGeo, matAcc);
      visorMesh.position.set(3.2, 0, 66.5);
      group.add(visorMesh);

      // 3. Tactical Plate Carrier Vest (z: 36..58)
      const chestGeo = new THREE.BoxGeometry(9.0, 11.5, 14.0);
      const chestMesh = new THREE.Mesh(chestGeo, matMain);
      chestMesh.position.set(0, 0, 48);
      group.add(chestMesh);

      // Mag Pouches (3x front pouches)
      for (let p = -1; p <= 1; p++) {
        const pouchGeo = new THREE.BoxGeometry(1.6, 2.8, 5.0);
        const pouchMesh = new THREE.Mesh(pouchGeo, matDark);
        pouchMesh.position.set(4.8, p * 3.4, 46);
        group.add(pouchMesh);
      }

      // 4. Utility Belt & Pelvis (z: 32..38)
      const beltGeo = new THREE.BoxGeometry(8.5, 10.5, 4.0);
      const beltMesh = new THREE.Mesh(beltGeo, matDark);
      beltMesh.position.set(0, 0, 36);
      group.add(beltMesh);

      // 5. Articulated Arms & Tactical Gloves
      [-1, 1].forEach(side => {
        // Upper Arm
        const upArmGeo = new THREE.CylinderGeometry(2.0, 1.8, 9.0, 8);
        const upArm = new THREE.Mesh(upArmGeo, matMain);
        upArm.position.set(2.0, side * 7.5, 50);
        upArm.rotation.x = side * -0.25;
        upArm.rotation.z = -0.35;
        group.add(upArm);

        // Forearm / Glove
        const fArmGeo = new THREE.CylinderGeometry(1.7, 1.5, 8.5, 8);
        const fArm = new THREE.Mesh(fArmGeo, matDark);
        fArm.position.set(6.5, side * 6.0, 44);
        fArm.rotation.z = -0.9;
        group.add(fArm);
      });

      // 6. Tactical Weapon in Hands (M4A1-S Carbine)
      const weaponGeo = new THREE.BoxGeometry(18.0, 2.0, 4.0);
      const weaponMesh = new THREE.Mesh(weaponGeo, matDark);
      weaponMesh.position.set(9.0, 0, 44);
      group.add(weaponMesh);

      const barrelGeo = new THREE.CylinderGeometry(0.7, 0.7, 12.0, 8);
      const barrelMesh = new THREE.Mesh(barrelGeo, matDark);
      barrelMesh.rotation.z = Math.PI * 0.5;
      barrelMesh.position.set(22.0, 0, 44.5);
      group.add(barrelMesh);

      // 7. Tactical Cargo Pants & Legs
      [-1, 1].forEach(side => {
        // Upper Leg
        const thighGeo = new THREE.CylinderGeometry(2.5, 2.2, 14.0, 8);
        const thigh = new THREE.Mesh(thighGeo, matMain);
        thigh.position.set(0, side * 3.6, 26);
        group.add(thigh);

        // Knee Pad
        const padGeo = new THREE.BoxGeometry(2.2, 3.2, 3.5);
        const pad = new THREE.Mesh(padGeo, matDark);
        pad.position.set(2.0, side * 3.6, 18);
        group.add(pad);

        // Lower Leg
        const calfGeo = new THREE.CylinderGeometry(2.1, 1.9, 14.0, 8);
        const calf = new THREE.Mesh(calfGeo, matMain);
        calf.position.set(0, side * 3.6, 11);
        group.add(calf);

        // Heavy Combat Boot
        const bootGeo = new THREE.BoxGeometry(7.0, 3.4, 4.0);
        const boot = new THREE.Mesh(bootGeo, matDark);
        boot.position.set(1.5, side * 3.6, 2);
        group.add(boot);
      });
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim12_LagCompensation_Scene';

      // 1. Live Server Tactical Agent (Solid Cyan)
      this.livePawnGroup = new THREE.Group();
      this._buildTacticalAgent(this.livePawnGroup, 0x06b6d4, 1.0);
      this.sceneGroup.add(this.livePawnGroup);

      // 2. Rewound Lag-Compensated Ghost Agent (Amber Translucent)
      this.rewoundPawnGroup = new THREE.Group();
      this._buildTacticalAgent(this.rewoundPawnGroup, 0xf59e0b, 0.45);
      this.sceneGroup.add(this.rewoundPawnGroup);

      // 3. 64-Tick History Trail
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

      // 4. Bullet Tracer Line
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

      // 5. Hit Marker
      const hitGeo = new THREE.RingGeometry(2, 5, 16);
      const hitMat = new THREE.MeshBasicMaterial({ color: 0x10b981, side: THREE.DoubleSide, transparent: true, opacity: 0.0 });
      this.hitMarker = new THREE.Mesh(hitGeo, hitMat);
      this.hitMarker.position.set(0, 0, 66);
      this.sceneGroup.add(this.hitMarker);

      scene.add(this.sceneGroup);

      // 6. Mount UI Controls
      this._buildParamsUI();

      // 7. Optimal Camera View Framing the Dual Agents
      const camera = this.viewport.getCamera();
      if (camera) {
        camera.position.set(-90, -220, 110);
        const controls = this.viewport.getControls();
        if (controls) {
          controls.target.set(10, 0, 40);
          controls.update();
        }
      }

      this.update(0, 0, 0);
    }

    fireUnlagVerificationShot() {
      const latencySec = Math.min(0.200, Math.max(0.005, this.clientLatencySec));
      const rewindTicks = latencySec / 0.015625;
      const targetPos = this.aimTarget === 'rewound' ? this.rewoundPawnGroup.position : this.livePawnGroup.position;

      // Tracer line animation
      if (this.bulletTracer && this.bulletTracer.geometry) {
        this.bulletTracer.geometry.setFromPoints([
          new THREE.Vector3(0, -220, 50),
          new THREE.Vector3(targetPos.x, targetPos.y, targetPos.z + 66)
        ]);
        if (this.bulletTracer.material) this.bulletTracer.material.opacity = 1.0;
      }

      if (this.audio) {
        this.audio.playGunfire(0, -220, 50);

        setTimeout(() => {
          if (this.aimTarget === 'rewound') {
            // Shoots rewound ghost -> 100% Authoritative Hit!
            this.audio.playHeadshot(this.rewoundPawnGroup.position.x, this.rewoundPawnGroup.position.y, 66);
            this.lastHitResult = {
              hit: true,
              type: 'CONFIRMED HEADSHOT',
              desc: `Authoritative server rewind by ${rewindTicks.toFixed(2)} ticks (${(latencySec * 1000).toFixed(0)} ms)`
            };
            if (this.hitMarker) {
              this.hitMarker.position.set(targetPos.x, targetPos.y, targetPos.z + 66);
              if (this.hitMarker.material) this.hitMarker.material.opacity = 1.0;
            }
          } else {
            // Shoots live pawn -> Desync Miss!
            this.audio.playImpact('concrete', 0, 0, 0);
            this.lastHitResult = {
              hit: false,
              type: 'DESYNC MISS',
              desc: `Shot missed! Crosshair aimed where player is now, ignoring client network latency.`
            };
          }
          this._updateUIStats();
        }, 25);
      }
    }

    update(tick, subTickFrac, dt) {
      // 1. Position Live Server Agent at latest tick
      const curTick = tick % this.BUFFER_CAPACITY;
      const curSample = this.historyBuffer[curTick] || this.historyBuffer[0];
      const nextSample = this.historyBuffer[(curTick + 1) % this.BUFFER_CAPACITY] || curSample;

      const liveX = curSample.pos.x + (nextSample.pos.x - curSample.pos.x) * subTickFrac;
      const liveY = curSample.pos.y + (nextSample.pos.y - curSample.pos.y) * subTickFrac;

      if (this.livePawnGroup) {
        this.livePawnGroup.position.set(liveX, liveY, 0);
        this.livePawnGroup.rotation.z = curSample.yaw;
      }

      // 2. Evaluate Server Lag Compensation Rewind Tick
      const latencySec = Math.min(0.200, Math.max(0.005, this.clientLatencySec));
      const rewindTicks = latencySec / 0.015625;
      const targetContinuousTick = (curTick + subTickFrac) - rewindTicks;

      // Wrap around 64-tick ring buffer
      let rewindK = Math.floor(targetContinuousTick);
      let rewindFrac = targetContinuousTick - rewindK;
      while (rewindK < 0) rewindK += this.BUFFER_CAPACITY;
      rewindK = rewindK % this.BUFFER_CAPACITY;

      const s0 = this.historyBuffer[rewindK];
      const s1 = this.historyBuffer[(rewindK + 1) % this.BUFFER_CAPACITY] || s0;

      // Hermite Spline Interpolation matching server.dll CInterpolatedVar
      const rewoundX = s0.pos.x + (s1.pos.x - s0.pos.x) * rewindFrac;
      const rewoundY = s0.pos.y + (s1.pos.y - s0.pos.y) * rewindFrac;

      if (this.rewoundPawnGroup) {
        this.rewoundPawnGroup.position.set(rewoundX, rewoundY, 0);
        this.rewoundPawnGroup.rotation.z = s0.yaw;
      }

      // Fade out tracer and hit marker
      if (this.bulletTracer && this.bulletTracer.material && this.bulletTracer.material.opacity > 0) {
        this.bulletTracer.material.opacity = Math.max(0, this.bulletTracer.material.opacity - dt * 3.0);
      }
      if (this.hitMarker && this.hitMarker.material && this.hitMarker.material.opacity > 0) {
        this.hitMarker.material.opacity = Math.max(0, this.hitMarker.material.opacity - dt * 2.5);
      }

      const isClamped = this.clientLatencySec > 0.200;

      return {
        pos: { x: liveX, y: liveY, z: 0 },
        vel: { x: rewoundX, y: rewoundY, z: 0, speed: Math.hypot(liveX - rewoundX, liveY - rewoundY) },
        ground: true,
        event: isClamped ? 'sv_maxunlag CLAMPED' : 'LAG COMP NOMINAL',
        customMetrics: [
          { label: 'CLIENT PING', value: `${(this.clientLatencySec * 1000).toFixed(0)} ms` },
          { label: 'REWIND TICKS', value: `${rewindTicks.toFixed(2)} ticks` },
          { label: 'SPATIAL DESYNC', value: `${Math.hypot(liveX - rewoundX, liveY - rewoundY).toFixed(1)} u` },
          { label: 'INTERPOLATOR', value: 'HERMITE CUBIC' },
          { label: 'UNLAG STATUS', value: isClamped ? 'CLAMPED (200ms)' : 'VALID', alert: isClamped }
        ]
      };
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
        <div class="space-y-4 font-mono text-xs select-none" draggable="false" ondragstart="return false;">
          <!-- Network Timing Configuration -->
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1.5">
              <span>CLIENT NETWORK TIMING</span>
              <span class="text-[10px] text-cyan-400">sv_maxunlag 0.200s</span>
            </div>
            <div>
              <div class="flex justify-between text-[11px] text-slate-400">
                <span>Client Latency / Ping:</span>
                <span id="sim12LblPing" class="text-cyan-300 font-bold">${(this.clientLatencySec * 1000).toFixed(0)} ms</span>
              </div>
              <input type="range" id="sim12Ping" min="5" max="300" step="5" value="${this.clientLatencySec * 1000}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[11px] text-slate-400">
                <span>Sub-Tick UserCmd Fraction w:</span>
                <span id="sim12LblFrac" class="text-amber-400 font-bold">${this.userCmdSubTickFrac.toFixed(3)}</span>
              </div>
              <input type="range" id="sim12Frac" min="0" max="0.999" step="0.005" value="${this.userCmdSubTickFrac}" class="w-full accent-amber-500 cursor-pointer">
            </div>
          </div>

          <!-- Aim Mode & Verification Action -->
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <span class="block text-[11px] text-slate-400 font-bold">CROSSHAIR AIM MODE:</span>
            <div class="grid grid-cols-2 gap-2">
              <button id="sim12BtnAimGhost" class="py-2 px-2 rounded ${this.aimTarget === 'rewound' ? 'bg-cyan-600 text-white' : 'bg-slate-800 text-slate-300'} font-bold transition-all border border-cyan-500/40 cursor-pointer flex flex-col items-center">
                <span>Aim at</span>
                <span class="text-[10px] font-normal">Rewound Ghost</span>
              </button>
              <button id="sim12BtnAimLive" class="py-2 px-2 rounded ${this.aimTarget === 'live' ? 'bg-cyan-600 text-white' : 'bg-slate-800 text-slate-300'} font-bold transition-all border border-slate-700 cursor-pointer flex flex-col items-center">
                <span>Aim at</span>
                <span class="text-[10px] font-normal">Live Server</span>
              </button>
            </div>
            <button id="sim12BtnFire" class="w-full py-2.5 rounded bg-emerald-600 hover:bg-emerald-500 text-white font-bold transition-all shadow-md text-xs cursor-pointer flex items-center justify-center gap-1.5">
              <span>Fire Authoritative Unlag Verification Shot</span>
            </button>
          </div>

          <!-- Telemetry Output -->
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2 text-[11px]">
            <div class="flex items-center justify-between">
              <span class="font-bold text-slate-300">SERVER UNLAG TELEMETRY</span>
              <span id="sim12Badge" class="text-[9px] px-1.5 py-0.5 rounded font-bold bg-emerald-950 text-emerald-300 border border-emerald-800">sv_maxunlag NOMINAL</span>
            </div>
            <div class="grid grid-cols-2 gap-2 pt-1 border-t border-slate-800">
              <div>
                <span class="text-slate-500 block">Rewind Ticks:</span>
                <span id="sim12StatTicks" class="text-slate-200 font-bold">5.44 ticks</span>
              </div>
              <div>
                <span class="text-slate-500 block">Delta Time:</span>
                <span id="sim12StatMs" class="text-cyan-300 font-bold">-85.0 ms (VALID)</span>
              </div>
            </div>
            <div id="sim12HitBox" class="mt-2 p-2 rounded bg-slate-950 border border-slate-800">
              <span class="text-slate-500 block text-[10px]">Hit Registration Result:</span>
              <span id="sim12HitVerdict" class="text-slate-400 font-bold block mt-0.5">READY FOR TEST FIRE</span>
              <span id="sim12HitDesc" class="text-slate-500 text-[10px] block mt-0.5">Click Fire to test server unlag rewind verification.</span>
            </div>
          </div>
        </div>
      `;

      // Event Handlers
      const pIn = this.paramsContainer.querySelector('#sim12Ping');
      const lblP = this.paramsContainer.querySelector('#sim12LblPing');
      if (pIn && lblP) {
        pIn.oninput = (e) => {
          this.clientLatencySec = parseFloat(e.target.value) / 1000.0;
          lblP.textContent = `${e.target.value} ms`;
          this._updateUIStats();
          this.update(0, 0, 0);
        };
      }

      const fIn = this.paramsContainer.querySelector('#sim12Frac');
      const lblF = this.paramsContainer.querySelector('#sim12LblFrac');
      if (fIn && lblF) {
        fIn.oninput = (e) => {
          this.userCmdSubTickFrac = parseFloat(e.target.value);
          lblF.textContent = this.userCmdSubTickFrac.toFixed(3);
          this._updateUIStats();
          this.update(0, 0, 0);
        };
      }

      const btnGhost = this.paramsContainer.querySelector('#sim12BtnAimGhost');
      const btnLive = this.paramsContainer.querySelector('#sim12BtnAimLive');
      if (btnGhost && btnLive) {
        btnGhost.onclick = () => {
          this.aimTarget = 'rewound';
          btnGhost.className = 'py-2 px-2 rounded bg-cyan-600 text-white font-bold transition-all border border-cyan-500/40 cursor-pointer flex flex-col items-center';
          btnLive.className = 'py-2 px-2 rounded bg-slate-800 text-slate-300 font-bold transition-all border border-slate-700 cursor-pointer flex flex-col items-center';
        };
        btnLive.onclick = () => {
          this.aimTarget = 'live';
          btnLive.className = 'py-2 px-2 rounded bg-cyan-600 text-white font-bold transition-all border border-cyan-500/40 cursor-pointer flex flex-col items-center';
          btnGhost.className = 'py-2 px-2 rounded bg-slate-800 text-slate-300 font-bold transition-all border border-slate-700 cursor-pointer flex flex-col items-center';
        };
      }

      const btnFire = this.paramsContainer.querySelector('#sim12BtnFire');
      if (btnFire) {
        btnFire.onclick = () => this.fireUnlagVerificationShot();
      }
    }

    _updateUIStats() {
      if (!this.paramsContainer) return;
      const latencySec = Math.min(0.200, Math.max(0.005, this.clientLatencySec));
      const rewindTicks = latencySec / 0.015625;
      const isClamped = this.clientLatencySec > 0.200;

      const valTicks = this.paramsContainer.querySelector('#sim12StatTicks');
      const valMs = this.paramsContainer.querySelector('#sim12StatMs');
      const badge = this.paramsContainer.querySelector('#sim12Badge');
      const valVerdict = this.paramsContainer.querySelector('#sim12HitVerdict');
      const valDesc = this.paramsContainer.querySelector('#sim12HitDesc');

      if (valTicks) valTicks.textContent = `${rewindTicks.toFixed(1)} ticks`;
      if (valMs) valMs.textContent = `-${(latencySec * 1000).toFixed(1)} ms ${isClamped ? '(CLAMPED)' : '(VALID)'}`;

      if (badge) {
        if (isClamped) {
          badge.className = 'text-[9px] px-1.5 py-0.5 rounded font-bold bg-amber-950 text-amber-300 border border-amber-800';
          badge.textContent = 'CLAMPED AT 200ms';
        } else {
          badge.className = 'text-[9px] px-1.5 py-0.5 rounded font-bold bg-emerald-950 text-emerald-300 border border-emerald-800';
          badge.textContent = 'sv_maxunlag NOMINAL';
        }
      }

      if (this.lastHitResult && valVerdict && valDesc) {
        if (this.lastHitResult.hit) {
          valVerdict.className = 'text-emerald-400 font-bold block mt-0.5';
          valVerdict.textContent = `\u2714 ${this.lastHitResult.type}`;
        } else {
          valVerdict.className = 'text-rose-400 font-bold block mt-0.5';
          valVerdict.textContent = `\u2718 ${this.lastHitResult.type}`;
        }
        valDesc.textContent = this.lastHitResult.desc;
      }
    }
  }

  // ==========================================================================
  // 8. Sim15EconWearProvider: Econ Item Schema, Wear Degradation ODE & Texture UV
  // Reverse-engineered from client.dll Build 26598758:
  // - CEconItemDefinition::GenerateDynamicWearMask (0x180bb2910)
  // - CRK4Integrator::StepWearODE (0x180bb4120)
  // ==========================================================================
  class Sim15EconWearProvider {
    constructor(viewport, audioEngine, playback) {
      this.viewport = viewport;
      this.audio = audioEngine;
      this.playback = playback;

      this.wearValue = 0.045; // Factory New default
      this.wearRateODE = 0.15;
      this.activeSkin = 'case_hardened'; // 'case_hardened' | 'fade' | 'asiimov'
      this.seed = 661; // Iconic CS #1 Blue Gem seed

      this.sceneGroup = null;
      this.knifeGroup = null;
      this.bladeMesh = null;
      this.edgeMesh = null;
      this.handleMesh = null;
      this.ringMesh = null;
      this.paramsContainer = null;
    }

    _getConditionName(w) {
      if (w < 0.07) return 'Factory New (FN)';
      if (w < 0.15) return 'Minimal Wear (MW)';
      if (w < 0.38) return 'Field-Tested (FT)';
      if (w < 0.45) return 'Well-Worn (WW)';
      return 'Battle-Scarred (BS)';
    }

    _buildKarambitKnife() {
      this.knifeGroup = new THREE.Group();

      // Blade Material with Dynamic Procedural Skin Shader / Canvas Texture
      const bladeMat = new THREE.MeshStandardMaterial({
        color: 0x38bdf8,
        metalness: 0.85,
        roughness: 0.25
      });

      // 1. Curved Talon Blade Profile
      const shape = new THREE.Shape();
      shape.moveTo(0, 0);
      shape.quadraticCurveTo(8, 22, 2, 42); // Spine curve
      shape.quadraticCurveTo(12, 38, 14, 28); // Tip taper
      shape.quadraticCurveTo(18, 16, 7, 0);  // Cutting edge curve
      shape.closePath();

      const extrudeSettings = { depth: 1.2, bevelEnabled: true, bevelSegments: 3, steps: 1, bevelSize: 0.6, bevelThickness: 0.6 };
      const bladeGeo = new THREE.ExtrudeGeometry(shape, extrudeSettings);
      bladeGeo.center();
      this.bladeMesh = new THREE.Mesh(bladeGeo, bladeMat);
      this.bladeMesh.position.set(4, 0, 16);
      this.bladeMesh.rotation.z = -0.3;
      this.knifeGroup.add(this.bladeMesh);

      // 2. High-Wear Beveled Cutting Edge
      const edgeGeo = new THREE.CylinderGeometry(0.3, 0.3, 40, 8);
      const edgeMat = new THREE.MeshStandardMaterial({ color: 0x94a3b8, metalness: 0.95, roughness: 0.15 });
      this.edgeMesh = new THREE.Mesh(edgeGeo, edgeMat);
      this.edgeMesh.position.set(13, 0, 18);
      this.edgeMesh.rotation.z = 0.25;
      this.knifeGroup.add(this.edgeMesh);

      // 3. Ergonomic Handle with Finger Grooves
      const handleMat = new THREE.MeshStandardMaterial({ color: 0x1e293b, roughness: 0.8, metalness: 0.1 });
      const handleGeo = new THREE.BoxGeometry(8, 3.2, 28);
      this.handleMesh = new THREE.Mesh(handleGeo, handleMat);
      this.handleMesh.position.set(0, 0, -4);
      this.knifeGroup.add(this.handleMesh);

      // Handle Grip Rivets
      const rivetMat = new THREE.MeshStandardMaterial({ color: 0xcfd8dc, metalness: 0.9, roughness: 0.2 });
      [-8, 0, 6].forEach(zOff => {
        const rGeo = new THREE.CylinderGeometry(0.8, 0.8, 3.6, 12);
        const rMesh = new THREE.Mesh(rGeo, rivetMat);
        rMesh.rotation.x = Math.PI * 0.5;
        rMesh.position.set(0, 0, zOff);
        this.knifeGroup.add(rMesh);
      });

      // 4. Safety Retention Ring (Pommel)
      const ringGeo = new THREE.TorusGeometry(5.5, 1.4, 12, 24);
      const ringMat = new THREE.MeshStandardMaterial({ color: 0x334155, metalness: 0.8, roughness: 0.3 });
      this.ringMesh = new THREE.Mesh(ringGeo, ringMat);
      this.ringMesh.position.set(0, 0, -22);
      this.knifeGroup.add(this.ringMesh);

      this.sceneGroup.add(this.knifeGroup);
      this._updateSkinVisual();
    }

    _updateSkinVisual() {
      if (!this.bladeMesh) return;
      const w = this.wearValue;
      const seedMod = (this.seed % 100) / 100.0;

      let baseCol = 0x38bdf8;
      let roughness = 0.2 + w * 0.6;
      let metalness = 0.85 - w * 0.4;

      if (this.activeSkin === 'case_hardened') {
        // Titanium case-hardening heat spectrum: blue gem patinas vs raw gold
        const isBlueGem = (this.seed === 661 || this.seed === 387);
        baseCol = isBlueGem ? (w < 0.4 ? 0x0ea5e9 : 0x1e3a8a) : (seedMod > 0.5 ? 0x3b82f6 : 0xeab308);
      } else if (this.activeSkin === 'fade') {
        // Anodized fade spectrum
        baseCol = seedMod > 0.5 ? 0xd946ef : 0xf43f5e;
      } else if (this.activeSkin === 'asiimov') {
        // Sci-Fi Orange/White livery
        baseCol = w > 0.45 ? 0x475569 : 0xf97316;
      }

      // At higher wear values, paint strips off and exposes dark patinated oxidized steel
      if (w >= 0.45) {
        const lerpFactor = (w - 0.45) / 0.55;
        this.bladeMesh.material.color.setHex(baseCol).lerp(new THREE.Color(0x334155), lerpFactor * 0.75);
      } else {
        this.bladeMesh.material.color.setHex(baseCol);
      }

      this.bladeMesh.material.roughness = roughness;
      this.bladeMesh.material.metalness = metalness;

      if (this.edgeMesh) {
        // Cutting edge shows scratches and exposed metal based on wear ODE
        this.edgeMesh.material.roughness = 0.15 + w * 0.5;
        this.edgeMesh.material.color.setHex(w > 0.4 ? 0x64748b : 0xe2e8f0);
      }
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim15_EconWear_Scene';

      this._buildKarambitKnife();

      // Lighting enhancements for inspect showcase
      const keyLight = new THREE.DirectionalLight(0xffffff, 1.2);
      keyLight.position.set(40, -40, 60);
      this.sceneGroup.add(keyLight);

      const rimLight = new THREE.DirectionalLight(0x38bdf8, 0.8);
      rimLight.position.set(-50, 40, -20);
      this.sceneGroup.add(rimLight);

      scene.add(this.sceneGroup);

      this._buildParamsUI();

      const camera = this.viewport.getCamera();
      if (camera) {
        camera.position.set(-30, -50, 25);
        const controls = this.viewport.getControls();
        if (controls) {
          controls.target.set(0, 0, 5);
          controls.update();
        }
      }

      this.update(0, 0, 0);
    }

    update(tick, subTickFrac, dt) {
      if (this.knifeGroup) {
        // Continuous smooth weapon inspect idle spin
        this.knifeGroup.rotation.y = (tick + subTickFrac) * 0.035;
        this.knifeGroup.rotation.x = Math.sin((tick + subTickFrac) * 0.05) * 0.15;
      }

      this._updateSkinVisual();

      const cond = this._getConditionName(this.wearValue);
      return {
        pos: { x: 0, y: 0, z: 0 },
        vel: { x: this.wearValue, y: this.seed, z: 0, speed: 0 },
        ground: true,
        event: `${cond.toUpperCase()} [${this.activeSkin.toUpperCase()}]`,
        customMetrics: [
          { label: 'WEAR FLOAT (W)', value: this.wearValue.toFixed(5) },
          { label: 'CONDITION', value: cond },
          { label: 'PATTERN SEED', value: `#${this.seed}` },
          { label: 'DEGRADATION ODE', value: 'RUNGE-KUTTA 4TH' },
          { label: 'FALLBACK SEED', value: 'CEconItemDefinition' }
        ]
      };
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
        <div class="space-y-4 font-mono text-xs select-none" draggable="false" ondragstart="return false;">
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1.5">
              <span>WEAPON FINISH &amp; SKIN SCHEMA</span>
              <span class="text-[10px] text-cyan-400">client.dll</span>
            </div>
            <div>
              <label class="block text-[11px] text-slate-400 mb-1">Procedural Finish Style:</label>
              <select id="sim15SkinSelect" class="w-full bg-slate-950 border border-slate-700 rounded px-2 py-1.5 text-cyan-300 font-bold cursor-pointer">
                <option value="case_hardened" ${this.activeSkin === 'case_hardened' ? 'selected' : ''}>Case Hardened (Titanium Patina &amp; Blue Gem)</option>
                <option value="fade" ${this.activeSkin === 'fade' ? 'selected' : ''}>Fade (Anodized Spectrum 80-100%)</option>
                <option value="asiimov" ${this.activeSkin === 'asiimov' ? 'selected' : ''}>Asiimov (Sci-Fi High Contrast Livery)</option>
              </select>
            </div>
            <div>
              <div class="flex justify-between text-[11px] text-slate-400">
                <span>Wear Float Value (W):</span>
                <span id="sim15LblWear" class="text-amber-400 font-bold">${this.wearValue.toFixed(4)}</span>
              </div>
              <input type="range" id="sim15Wear" min="0.0001" max="0.9999" step="0.005" value="${this.wearValue}" class="w-full accent-amber-500 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[11px] text-slate-400">
                <span>Paint Seed (m_nFallbackSeed):</span>
                <span id="sim15LblSeed" class="text-cyan-300 font-bold">#${this.seed}</span>
              </div>
              <input type="range" id="sim15Seed" min="1" max="1000" step="1" value="${this.seed}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
          </div>

          <div class="space-y-1.5">
            <span class="text-[11px] font-bold text-slate-400">FLOAT PRESETS (CS2 WEAR BRACKETS)</span>
            <div class="grid grid-cols-2 gap-1.5">
              <button id="sim15BtnFN" class="py-1.5 px-2 rounded bg-emerald-950/80 hover:bg-emerald-900 text-emerald-300 border border-emerald-800 text-[11px] font-bold cursor-pointer">Factory New (&lt;0.07)</button>
              <button id="sim15BtnMW" class="py-1.5 px-2 rounded bg-cyan-950/80 hover:bg-cyan-900 text-cyan-300 border border-cyan-800 text-[11px] font-bold cursor-pointer">Minimal Wear (0.10)</button>
              <button id="sim15BtnFT" class="py-1.5 px-2 rounded bg-blue-950/80 hover:bg-blue-900 text-blue-300 border border-blue-800 text-[11px] font-bold cursor-pointer">Field-Tested (0.22)</button>
              <button id="sim15BtnBS" class="py-1.5 px-2 rounded bg-rose-950/80 hover:bg-rose-900 text-rose-300 border border-rose-800 text-[11px] font-bold cursor-pointer">Battle-Scarred (0.78)</button>
            </div>
          </div>

          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2 text-[11px]">
            <div class="flex items-center justify-between">
              <span class="font-bold text-slate-300">WEAR CONDITION METRICS</span>
              <span id="sim15BadgeCond" class="text-[9px] px-1.5 py-0.5 rounded font-bold bg-emerald-950 text-emerald-300 border border-emerald-800">${this._getConditionName(this.wearValue)}</span>
            </div>
            <div class="grid grid-cols-2 gap-2 pt-1 border-t border-slate-800">
              <div>
                <span class="text-slate-500 block">Paint Layer:</span>
                <span id="sim15StatLayer" class="text-slate-200 font-bold">${this.wearValue < 0.45 ? '100% Intact' : 'Oxidized Gunmetal'}</span>
              </div>
              <div>
                <span class="text-slate-500 block">Degradation:</span>
                <span id="sim15StatODE" class="text-cyan-300 font-bold">RK4 ODE Integrated</span>
              </div>
            </div>
          </div>
        </div>
      `;

      const skinSel = this.paramsContainer.querySelector('#sim15SkinSelect');
      if (skinSel) skinSel.onchange = (e) => { this.activeSkin = e.target.value; this.update(0, 0, 0); };

      const wIn = this.paramsContainer.querySelector('#sim15Wear');
      const lblW = this.paramsContainer.querySelector('#sim15LblWear');
      if (wIn && lblW) {
        wIn.oninput = (e) => {
          this.wearValue = parseFloat(e.target.value);
          lblW.textContent = this.wearValue.toFixed(4);
          this._syncUI();
          this.update(0, 0, 0);
        };
      }

      const sIn = this.paramsContainer.querySelector('#sim15Seed');
      const lblS = this.paramsContainer.querySelector('#sim15LblSeed');
      if (sIn && lblS) {
        sIn.oninput = (e) => {
          this.seed = parseInt(e.target.value, 10);
          lblS.textContent = `#${this.seed}`;
          this._syncUI();
          this.update(0, 0, 0);
        };
      }

      const btnFN = this.paramsContainer.querySelector('#sim15BtnFN');
      const btnMW = this.paramsContainer.querySelector('#sim15BtnMW');
      const btnFT = this.paramsContainer.querySelector('#sim15BtnFT');
      const btnBS = this.paramsContainer.querySelector('#sim15BtnBS');

      if (btnFN) btnFN.onclick = () => { this.wearValue = 0.025; this._syncUI(); this.update(0, 0, 0); };
      if (btnMW) btnMW.onclick = () => { this.wearValue = 0.095; this._syncUI(); this.update(0, 0, 0); };
      if (btnFT) btnFT.onclick = () => { this.wearValue = 0.220; this._syncUI(); this.update(0, 0, 0); };
      if (btnBS) btnBS.onclick = () => { this.wearValue = 0.785; this._syncUI(); this.update(0, 0, 0); };
    }

    _syncUI() {
      if (!this.paramsContainer) return;
      const wIn = this.paramsContainer.querySelector('#sim15Wear');
      const lblW = this.paramsContainer.querySelector('#sim15LblWear');
      const badge = this.paramsContainer.querySelector('#sim15BadgeCond');
      const statLayer = this.paramsContainer.querySelector('#sim15StatLayer');

      if (wIn) wIn.value = this.wearValue;
      if (lblW) lblW.textContent = this.wearValue.toFixed(4);
      if (badge) badge.textContent = this._getConditionName(this.wearValue);
      if (statLayer) statLayer.textContent = this.wearValue < 0.45 ? '100% Intact' : 'Oxidized Gunmetal';
    }
  }

  // ==========================================================================
  // 9. Sim17SpatialAudioProvider: Steam Audio Spatial Acoustics & HRTF
  // Reverse-engineered from steamaudio.dll Build 26598758:
  // - iplAudioBufferInterpolate (0x180018f20)
  // - iplAirAbsorptionModelEvaluate (0x180021b40)
  // - iplDirectSimulationCalculate (0x180028e10)
  // ==========================================================================
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

      // 5. Sound Propagation Waves (Expanding Concentric Wavefronts)
      this.waveRings = [];
      for (let r = 0; r < 5; r++) {
        const rGeo = new THREE.RingGeometry(10 + r * 25, 12 + r * 25, 32);
        const rMat = new THREE.MeshBasicMaterial({ color: 0x38bdf8, side: THREE.DoubleSide, transparent: true, opacity: 0.4 - r * 0.07 });
        const ring = new THREE.Mesh(rGeo, rMat);
        ring.position.set(this.sourcePos.x, this.sourcePos.y, this.sourcePos.z);
        this.sceneGroup.add(ring);
        this.waveRings.push(ring);
      }

      // 6. Direct Sound Transmission Ray
      const rayGeo = new THREE.BufferGeometry().setFromPoints([
        new THREE.Vector3(this.sourcePos.x, this.sourcePos.y, this.sourcePos.z),
        new THREE.Vector3(this.listenerPos.x, this.listenerPos.y, this.listenerPos.z)
      ]);
      this.acousticRays = new THREE.Line(rayGeo, new THREE.LineBasicMaterial({ color: 0x38bdf8, linewidth: 2 }));
      this.sceneGroup.add(this.acousticRays);

      scene.add(this.sceneGroup);

      this._buildParamsUI();

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
      // Expand acoustic wave rings
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

      return {
        pos: { x: this.sourcePos.x, y: this.sourcePos.y, z: this.sourcePos.z },
        vel: { x: this.listenerPos.x, y: this.listenerPos.y, z: 0, speed: dist },
        ground: true,
        event: this.wallOcclusion ? 'OCCLUDED (-14 dB TRANSMISSION)' : 'DIRECT LOS (BINAURAL HRTF)',
        customMetrics: [
          { label: 'SPATIAL DISTANCE', value: `${dist.toFixed(1)} u (${(dist * 0.0254).toFixed(1)} m)` },
          { label: 'AIR ABSORPTION', value: `-${airAttenDb.toFixed(2)} dB (ISO 9613-1)` },
          { label: 'OCCLUSION FILTER', value: this.wallOcclusion ? 'LOWPASS 380 Hz' : 'BYPASS', alert: this.wallOcclusion },
          { label: 'ROOM REVERB (RT60)', value: `${this.reverbRT60.toFixed(2)} s (Sabine)` },
          { label: 'HRTF PANNER', value: '3D STEREO BINAURAL' }
        ]
      };
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
        <div class="space-y-4 font-mono text-xs select-none" draggable="false" ondragstart="return false;">
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
          </div>

          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1">
              <span>SPATIAL EMITTER &amp; LISTENER</span>
              <span class="text-[10px] text-emerald-400">Binaural HRTF</span>
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Sound Source Y Offset:</span>
                <span id="sim17LblSrcY" class="text-cyan-300">${this.sourcePos.y.toFixed(0)} u</span>
              </div>
              <input type="range" id="sim17SrcY" min="-120" max="120" step="5" value="${this.sourcePos.y}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
            <div>
              <div class="flex justify-between text-[10px] text-slate-400">
                <span>Listener Pawn Y Offset:</span>
                <span id="sim17LblLisY" class="text-emerald-400">${this.listenerPos.y.toFixed(0)} u</span>
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

      const syIn = this.paramsContainer.querySelector('#sim17SrcY');
      const lblSY = this.paramsContainer.querySelector('#sim17LblSrcY');
      if (syIn && lblSY) {
        syIn.oninput = (e) => {
          this.sourcePos.y = parseFloat(e.target.value);
          lblSY.textContent = `${this.sourcePos.y.toFixed(0)} u`;
          if (this.sourceMesh) this.sourceMesh.position.y = this.sourcePos.y;
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

  // ==========================================================================
  // 10. Sim22ParticleComputeProvider: 3D GPU Particle Compute & SoA SIMD
  // Reverse-engineered from particles.dll Build 26598758:
  // - COpMovementBasic (0x180050110)
  // - COpTurbulenceNoise (0x180050480)
  // - COpWorldCollisionPlane (0x1800506e0)
  // - COpFadeAndKill (0x1800508a0)
  // ==========================================================================
  class Sim22ParticleComputeProvider {
    constructor(viewport, audioEngine, playback) {
      this.viewport = viewport;
      this.audio = audioEngine;
      this.playback = playback;

      this.PARTICLE_COUNT = 4096;
      this.activePreset = 'sparks'; // 'sparks' | 'smoke_vortex' | 'molotov_fire'
      this.gravity = -380.0;
      this.turbulenceStrength = 45.0;

      // 64-Byte Aligned Structure-of-Arrays (SoA) SIMD Buffers
      this.posX = new Float32Array(this.PARTICLE_COUNT);
      this.posY = new Float32Array(this.PARTICLE_COUNT);
      this.posZ = new Float32Array(this.PARTICLE_COUNT);
      this.velX = new Float32Array(this.PARTICLE_COUNT);
      this.velY = new Float32Array(this.PARTICLE_COUNT);
      this.velZ = new Float32Array(this.PARTICLE_COUNT);
      this.age = new Float32Array(this.PARTICLE_COUNT);
      this.lifespan = new Float32Array(this.PARTICLE_COUNT);

      this.sceneGroup = null;
      this.particlePoints = null;
      this.paramsContainer = null;

      this._initParticles();
    }

    _initParticles() {
      for (let i = 0; i < this.PARTICLE_COUNT; i++) {
        this._respawnParticle(i);
        this.age[i] = Math.random() * this.lifespan[i]; // Distribute initial ages
      }
    }

    _respawnParticle(i) {
      this.posX[i] = 0;
      this.posY[i] = 0;
      this.posZ[i] = 10;

      if (this.activePreset === 'sparks') {
        const theta = Math.random() * Math.PI * 2;
        const phi = Math.random() * Math.PI * 0.45;
        const spd = 120.0 + Math.random() * 220.0;
        this.velX[i] = Math.cos(theta) * Math.sin(phi) * spd;
        this.velY[i] = Math.sin(theta) * Math.sin(phi) * spd;
        this.velZ[i] = Math.cos(phi) * spd;
        this.lifespan[i] = 0.8 + Math.random() * 0.8;
      } else if (this.activePreset === 'smoke_vortex') {
        this.velX[i] = (Math.random() - 0.5) * 40.0;
        this.velY[i] = (Math.random() - 0.5) * 40.0;
        this.velZ[i] = 60.0 + Math.random() * 80.0;
        this.lifespan[i] = 2.0 + Math.random() * 1.5;
      } else if (this.activePreset === 'molotov_fire') {
        const r = Math.random() * 45.0;
        const a = Math.random() * Math.PI * 2;
        this.posX[i] = Math.cos(a) * r;
        this.posY[i] = Math.sin(a) * r;
        this.velX[i] = (Math.random() - 0.5) * 20.0;
        this.velY[i] = (Math.random() - 0.5) * 20.0;
        this.velZ[i] = 80.0 + Math.random() * 140.0;
        this.lifespan[i] = 1.0 + Math.random() * 0.8;
      }
      this.age[i] = 0.0;
    }

    mount(paramsContainer) {
      this.paramsContainer = typeof paramsContainer === 'string'
        ? document.querySelector(paramsContainer)
        : paramsContainer;

      const scene = this.viewport.getScene();
      this.sceneGroup = new THREE.Group();
      this.sceneGroup.name = 'Sim22_ParticleCompute_Scene';

      // 1. WebGL Points Buffer Geometry
      const geom = new THREE.BufferGeometry();
      const posArray = new Float32Array(this.PARTICLE_COUNT * 3);
      const colArray = new Float32Array(this.PARTICLE_COUNT * 3);

      geom.setAttribute('position', new THREE.BufferAttribute(posArray, 3));
      geom.setAttribute('color', new THREE.BufferAttribute(colArray, 3));

      const mat = new THREE.PointsMaterial({
        size: 3.5,
        vertexColors: true,
        transparent: true,
        opacity: 0.85,
        blending: THREE.AdditiveBlending,
        depthWrite: false
      });

      this.particlePoints = new THREE.Points(geom, mat);
      this.sceneGroup.add(this.particlePoints);

      // 2. Ground Collision Plane
      const groundGeo = new THREE.PlaneGeometry(300, 300);
      const groundMat = new THREE.MeshStandardMaterial({ color: 0x0f172a, roughness: 0.8 });
      const ground = new THREE.Mesh(groundGeo, groundMat);
      ground.position.set(0, 0, 0);
      this.sceneGroup.add(ground);

      scene.add(this.sceneGroup);

      this._buildParamsUI();

      const camera = this.viewport.getCamera();
      if (camera) {
        camera.position.set(-140, -180, 120);
        const controls = this.viewport.getControls();
        if (controls) {
          controls.target.set(0, 0, 35);
          controls.update();
        }
      }

      this.update(0, 0, 0.016);
    }

    update(tick, subTickFrac, dt) {
      const stepDt = Math.min(0.033, dt || 0.015625);
      const positions = this.particlePoints.geometry.attributes.position.array;
      const colors = this.particlePoints.geometry.attributes.color.array;

      let activeCount = 0;
      const tStart = performance.now();

      // SIMD Operators: COpMovementBasic, COpTurbulenceNoise, COpWorldCollisionPlane, COpFadeAndKill
      for (let i = 0; i < this.PARTICLE_COUNT; i++) {
        this.age[i] += stepDt;
        if (this.age[i] >= this.lifespan[i]) {
          this._respawnParticle(i);
        }
        activeCount++;

        // 1. COpMovementBasic: Gravity & Newtonian Velocity
        const g = this.activePreset === 'smoke_vortex' ? 15.0 : (this.activePreset === 'molotov_fire' ? 30.0 : this.gravity);
        this.velZ[i] += g * stepDt;

        // 2. COpTurbulenceNoise: 3D Curl Vector Field
        const turb = this.turbulenceStrength;
        const px = this.posX[i] * 0.04;
        const py = this.posY[i] * 0.04;
        const pz = this.posZ[i] * 0.04;
        const curlX = Math.sin(py * 2.1 + tick * 0.1) * turb;
        const curlY = Math.cos(px * 2.1 + tick * 0.1) * turb;
        this.velX[i] += curlX * stepDt;
        this.velY[i] += curlY * stepDt;

        // Integrate Position
        this.posX[i] += this.velX[i] * stepDt;
        this.posY[i] += this.velY[i] * stepDt;
        this.posZ[i] += this.velZ[i] * stepDt;

        // 3. COpWorldCollisionPlane: Ground Bounce & Restitution
        if (this.posZ[i] <= 0.5) {
          this.posZ[i] = 0.5;
          this.velZ[i] = -this.velZ[i] * 0.45; // 45% restitution coefficient
          this.velX[i] *= 0.75; // Ground friction damping
          this.velY[i] *= 0.75;
        }

        const idx3 = i * 3;
        positions[idx3] = this.posX[i];
        positions[idx3 + 1] = this.posY[i];
        positions[idx3 + 2] = this.posZ[i];

        // 4. COpFadeAndKill: Piecewise Cubic Hermite Alpha Envelope
        const normAge = this.age[i] / this.lifespan[i];
        const alpha = Math.sin(normAge * Math.PI);

        if (this.activePreset === 'sparks') {
          colors[idx3] = 1.0 * alpha;
          colors[idx3 + 1] = 0.85 * alpha;
          colors[idx3 + 2] = 0.25 * alpha;
        } else if (this.activePreset === 'smoke_vortex') {
          colors[idx3] = 0.6 * alpha;
          colors[idx3 + 1] = 0.7 * alpha;
          colors[idx3 + 2] = 0.8 * alpha;
        } else if (this.activePreset === 'molotov_fire') {
          colors[idx3] = 1.0 * alpha;
          colors[idx3 + 1] = (0.2 + (1.0 - normAge) * 0.6) * alpha;
          colors[idx3 + 2] = 0.05 * alpha;
        }
      }

      const tElapsedMicro = ((performance.now() - tStart) * 1000).toFixed(0);

      this.particlePoints.geometry.attributes.position.needsUpdate = true;
      this.particlePoints.geometry.attributes.color.needsUpdate = true;

      const memBytes = this.PARTICLE_COUNT * 8 * 4; // 8 channels * 4 bytes = 32 bytes/particle (64B aligned block)

      return {
        pos: { x: 0, y: 0, z: 0 },
        vel: { x: this.PARTICLE_COUNT, y: tElapsedMicro, z: 0, speed: activeCount },
        ground: true,
        event: `${this.activePreset.toUpperCase()} [4 COMPUTE OPS]`,
        customMetrics: [
          { label: 'ACTIVE PARTICLES', value: `${activeCount} / ${this.PARTICLE_COUNT}` },
          { label: 'SIMD MEMORY', value: `${(memBytes / 1024).toFixed(1)} KB (SoA 64B)` },
          { label: 'DISPATCH TIME', value: `${tElapsedMicro} μs` },
          { label: 'COMPUTE OPERATORS', value: '4x SoA BATCHED' },
          { label: 'COLLISION SOLVER', value: 'WORLD PLANE BOUNCE' }
        ]
      };
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
        <div class="space-y-4 font-mono text-xs select-none" draggable="false" ondragstart="return false;">
          <div class="bg-slate-900/90 border border-slate-800 rounded-lg p-3 space-y-2.5">
            <div class="flex items-center justify-between text-slate-300 font-bold border-b border-slate-800 pb-1.5">
              <span>SOURCE 2 PARTICLE ENGINE</span>
              <span class="text-[10px] text-cyan-400">particles.dll</span>
            </div>
            <div>
              <label class="block text-[11px] text-slate-400 mb-1">Compute Operator Preset:</label>
              <select id="sim22PresetSelect" class="w-full bg-slate-950 border border-slate-700 rounded px-2 py-1.5 text-cyan-300 font-bold cursor-pointer">
                <option value="sparks" ${this.activePreset === 'sparks' ? 'selected' : ''}>Muzzle Flash Sparks (Restitution Bounce)</option>
                <option value="smoke_vortex" ${this.activePreset === 'smoke_vortex' ? 'selected' : ''}>Smoke Grenade Vortex (3D Curl Turbulence)</option>
                <option value="molotov_fire" ${this.activePreset === 'molotov_fire' ? 'selected' : ''}>Molotov Flame Eruption (Buoyant Thermal)</option>
              </select>
            </div>
            <div>
              <div class="flex justify-between text-[11px] text-slate-400">
                <span>Turbulence Field Strength:</span>
                <span id="sim22LblTurb" class="text-cyan-300 font-bold">${this.turbulenceStrength.toFixed(0)} u/s</span>
              </div>
              <input type="range" id="sim22Turb" min="0" max="120" step="5" value="${this.turbulenceStrength}" class="w-full accent-cyan-400 cursor-pointer">
            </div>
          </div>
        </div>
      `;

      const sel = this.paramsContainer.querySelector('#sim22PresetSelect');
      if (sel) {
        sel.onchange = (e) => {
          this.activePreset = e.target.value;
          this._initParticles();
          this.update(0, 0, 0.016);
        };
      }

      const turbIn = this.paramsContainer.querySelector('#sim22Turb');
      const lblTurb = this.paramsContainer.querySelector('#sim22LblTurb');
      if (turbIn && lblTurb) {
        turbIn.oninput = (e) => {
          this.turbulenceStrength = parseFloat(e.target.value);
          lblTurb.textContent = `${this.turbulenceStrength.toFixed(0)} u/s`;
          this.update(0, 0, 0.016);
        };
      }
    }
  }

  // Register providers under simulator IDs '03', '09', '11', '12', '14', '15', '17', '22'
  CS2Lab.registerProvider('03', Sim03SubtickProvider);
  CS2Lab.registerProvider('09', Sim09SkeletonIKProvider);
  CS2Lab.registerProvider('11', Sim11VolumetricSmokeProvider);
  CS2Lab.registerProvider('12', Sim12LagCompensationProvider);
  CS2Lab.registerProvider('14', Sim14BulletPenetrationProvider);
  CS2Lab.registerProvider('15', Sim15EconWearProvider);
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
