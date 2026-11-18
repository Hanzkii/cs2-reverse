/**
 * Headless 3D Graphics Mathematics & Scene Graph Mock
 * Implements linear algebra, scene nodes, cameras, meshes, instanced geometry, and orbit controls.
 */

'use strict';

class Vector3 {
  constructor(x = 0, y = 0, z = 0) {
    this.x = x;
    this.y = y;
    this.z = z;
  }

  set(x, y, z) {
    this.x = x;
    this.y = y;
    this.z = z;
    return this;
  }

  copy(v) {
    this.x = v.x;
    this.y = v.y;
    this.z = v.z;
    return this;
  }

  clone() {
    return new Vector3(this.x, this.y, this.z);
  }

  add(v) {
    this.x += v.x;
    this.y += v.y;
    this.z += v.z;
    return this;
  }

  sub(v) {
    this.x -= v.x;
    this.y -= v.y;
    this.z -= v.z;
    return this;
  }

  multiplyScalar(s) {
    this.x *= s;
    this.y *= s;
    this.z *= s;
    return this;
  }

  divideScalar(s) {
    return this.multiplyScalar(1 / s);
  }

  dot(v) {
    return this.x * v.x + this.y * v.y + this.z * v.z;
  }

  cross(v) {
    return this.crossVectors(this, v);
  }

  crossVectors(a, b) {
    const ax = a.x, ay = a.y, az = a.z;
    const bx = b.x, by = b.y, bz = b.z;
    this.x = ay * bz - az * by;
    this.y = az * bx - ax * bz;
    this.z = ax * by - ay * bx;
    return this;
  }

  lengthSq() {
    return this.x * this.x + this.y * this.y + this.z * this.z;
  }

  length() {
    return Math.sqrt(this.lengthSq());
  }

  normalize() {
    const l = this.length();
    return l > 0 ? this.divideScalar(l) : this.set(0, 0, 0);
  }

  distanceTo(v) {
    return Math.sqrt(this.distanceToSquared(v));
  }

  distanceToSquared(v) {
    const dx = this.x - v.x, dy = this.y - v.y, dz = this.z - v.z;
    return dx * dx + dy * dy + dz * dz;
  }

  lerp(v, alpha) {
    this.x += (v.x - this.x) * alpha;
    this.y += (v.y - this.y) * alpha;
    this.z += (v.z - this.z) * alpha;
    return this;
  }

  applyQuaternion(q) {
    const x = this.x, y = this.y, z = this.z;
    const qx = q.x, qy = q.y, qz = q.z, qw = q.w;
    const ix = qw * x + qy * z - qz * y;
    const iy = qw * y + qz * x - qx * z;
    const iz = qw * z + qx * y - qy * x;
    const iw = -qx * x - qy * y - qz * z;
    this.x = ix * qw + iw * -qx + iy * -qz - iz * -qy;
    this.y = iy * qw + iw * -qy + iz * -qx - ix * -qz;
    this.z = iz * qw + iw * -qz + ix * -qy - iy * -qx;
    return this;
  }

  applyMatrix4(m) {
    const x = this.x, y = this.y, z = this.z;
    const e = m.elements;
    const w = 1 / (e[3] * x + e[7] * y + e[11] * z + e[15]);
    this.x = (e[0] * x + e[4] * y + e[8] * z + e[12]) * w;
    this.y = (e[1] * x + e[5] * y + e[9] * z + e[13]) * w;
    this.z = (e[2] * x + e[6] * y + e[10] * z + e[14]) * w;
    return this;
  }
}

class Quaternion {
  constructor(x = 0, y = 0, z = 0, w = 1) {
    this.x = x;
    this.y = y;
    this.z = z;
    this.w = w;
  }

  set(x, y, z, w) {
    this.x = x;
    this.y = y;
    this.z = z;
    this.w = w;
    return this;
  }

  copy(q) {
    this.x = q.x;
    this.y = q.y;
    this.z = q.z;
    this.w = q.w;
    return this;
  }

  clone() {
    return new Quaternion(this.x, this.y, this.z, this.w);
  }

  identity() {
    return this.set(0, 0, 0, 1);
  }

  dot(q) {
    return this.x * q.x + this.y * q.y + this.z * q.z + this.w * q.w;
  }

  lengthSq() {
    return this.x * this.x + this.y * this.y + this.z * this.z + this.w * this.w;
  }

  length() {
    return Math.sqrt(this.lengthSq());
  }

  normalize() {
    let l = this.length();
    if (l === 0) return this.identity();
    l = 1 / l;
    this.x *= l;
    this.y *= l;
    this.z *= l;
    this.w *= l;
    return this;
  }

  multiply(q) {
    return this.multiplyQuaternions(this, q);
  }

  multiplyQuaternions(a, b) {
    const qax = a.x, qay = a.y, qaz = a.z, qaw = a.w;
    const qbx = b.x, qby = b.y, qbz = b.z, qbw = b.w;
    this.x = qax * qbw + qaw * qbx + qay * qbz - qaz * qby;
    this.y = qay * qbw + qaw * qby + qaz * qbx - qax * qbz;
    this.z = qaz * qbw + qaw * qbz + qax * qby - qay * qbx;
    this.w = qaw * qbw - qax * qbx - qay * qby - qaz * qbz;
    return this;
  }

  slerp(qb, t) {
    return this.slerpQuaternions(this, qb, t);
  }

  slerpQuaternions(qa, qb, t) {
    if (t === 0) return this.copy(qa);
    if (t === 1) return this.copy(qb);

    let x = qa.x, y = qa.y, z = qa.z, w = qa.w;
    let cosHalfTheta = w * qb.w + x * qb.x + y * qb.y + z * qb.z;

    if (cosHalfTheta < 0) {
      this.w = -qb.w;
      this.x = -qb.x;
      this.y = -qb.y;
      this.z = -qb.z;
      cosHalfTheta = -cosHalfTheta;
    } else {
      this.copy(qb);
    }

    if (cosHalfTheta >= 0.9995) {
      this.w = w + t * (this.w - w);
      this.x = x + t * (this.x - x);
      this.y = y + t * (this.y - y);
      this.z = z + t * (this.z - z);
      return this.normalize();
    }

    const halfTheta = Math.acos(cosHalfTheta);
    const sinHalfTheta = Math.sqrt(1.0 - cosHalfTheta * cosHalfTheta);

    if (Math.abs(sinHalfTheta) < 0.001) {
      this.w = 0.5 * (w + this.w);
      this.x = 0.5 * (x + this.x);
      this.y = 0.5 * (y + this.y);
      this.z = 0.5 * (z + this.z);
      return this;
    }

    const ratioA = Math.sin((1 - t) * halfTheta) / sinHalfTheta;
    const ratioB = Math.sin(t * halfTheta) / sinHalfTheta;

    this.w = w * ratioA + this.w * ratioB;
    this.x = x * ratioA + this.x * ratioB;
    this.y = y * ratioA + this.y * ratioB;
    this.z = z * ratioA + this.z * ratioB;
    return this;
  }

  setFromAxisAngle(axis, angle) {
    const halfAngle = angle / 2;
    const s = Math.sin(halfAngle);
    this.x = axis.x * s;
    this.y = axis.y * s;
    this.z = axis.z * s;
    this.w = Math.cos(halfAngle);
    return this;
  }

  setFromRotationMatrix(m) {
    const te = m.elements;
    const m11 = te[0], m12 = te[4], m13 = te[8];
    const m21 = te[1], m22 = te[5], m23 = te[9];
    const m31 = te[2], m32 = te[6], m33 = te[10];
    const trace = m11 + m22 + m33;

    if (trace > 0) {
      const s = 0.5 / Math.sqrt(trace + 1.0);
      this.w = 0.25 / s;
      this.x = (m32 - m23) * s;
      this.y = (m13 - m31) * s;
      this.z = (m21 - m12) * s;
    } else if (m11 > m22 && m11 > m33) {
      const s = 2.0 * Math.sqrt(1.0 + m11 - m22 - m33);
      this.w = (m32 - m23) / s;
      this.x = 0.25 * s;
      this.y = (m12 + m21) / s;
      this.z = (m13 + m31) / s;
    } else if (m22 > m33) {
      const s = 2.0 * Math.sqrt(1.0 + m22 - m11 - m33);
      this.w = (m13 - m31) / s;
      this.x = (m12 + m21) / s;
      this.y = 0.25 * s;
      this.z = (m23 + m32) / s;
    } else {
      const s = 2.0 * Math.sqrt(1.0 + m33 - m11 - m22);
      this.w = (m21 - m12) / s;
      this.x = (m13 + m31) / s;
      this.y = (m23 + m32) / s;
      this.z = 0.25 * s;
    }
    return this;
  }
}

class Matrix4 {
  constructor() {
    this.elements = [
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1
    ];
  }

  identity() {
    this.elements = [
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1
    ];
    return this;
  }

  copy(m) {
    const te = this.elements;
    const me = m.elements;
    for (let i = 0; i < 16; i++) te[i] = me[i];
    return this;
  }

  clone() {
    return new Matrix4().copy(this);
  }

  compose(position, quaternion, scale) {
    const te = this.elements;
    const x = quaternion.x, y = quaternion.y, z = quaternion.z, w = quaternion.w;
    const x2 = x + x, y2 = y + y, z2 = z + z;
    const xx = x * x2, xy = x * y2, xz = x * z2;
    const yy = y * y2, yz = y * z2, zz = z * z2;
    const wx = w * x2, wy = w * y2, wz = w * z2;
    const sx = scale.x, sy = scale.y, sz = scale.z;

    te[0] = (1 - (yy + zz)) * sx;
    te[1] = (xy + wz) * sx;
    te[2] = (xz - wy) * sx;
    te[3] = 0;

    te[4] = (xy - wz) * sy;
    te[5] = (1 - (xx + zz)) * sy;
    te[6] = (yz + wx) * sy;
    te[7] = 0;

    te[8] = (xz + wy) * sz;
    te[9] = (yz - wx) * sz;
    te[10] = (1 - (xx + yy)) * sz;
    te[11] = 0;

    te[12] = position.x;
    te[13] = position.y;
    te[14] = position.z;
    te[15] = 1;

    return this;
  }

  determinant() {
    const te = this.elements;

    const n11 = te[0], n12 = te[4], n13 = te[8], n14 = te[12];
    const n21 = te[1], n22 = te[5], n23 = te[9], n24 = te[13];
    const n31 = te[2], n32 = te[6], n33 = te[10], n34 = te[14];
    const n41 = te[3], n42 = te[7], n43 = te[11], n44 = te[15];

    return (
      n41 * (
        + n14 * n23 * n32
        - n13 * n24 * n32
        - n14 * n22 * n33
        + n12 * n24 * n33
        + n13 * n22 * n34
        - n12 * n23 * n34
      ) +
      n42 * (
        + n11 * n23 * n34
        - n11 * n24 * n33
        + n14 * n21 * n33
        - n13 * n21 * n34
        + n13 * n24 * n31
        - n14 * n23 * n31
      ) +
      n43 * (
        + n11 * n24 * n32
        - n11 * n22 * n34
        - n14 * n21 * n32
        + n12 * n21 * n34
        + n14 * n22 * n31
        - n12 * n24 * n31
      ) +
      n44 * (
        - n13 * n22 * n31
        - n11 * n23 * n32
        + n11 * n22 * n33
        + n13 * n21 * n32
        - n12 * n21 * n33
        + n12 * n23 * n31
      )
    );
  }

  multiplyMatrices(a, b) {
    const ae = a.elements;
    const be = b.elements;
    const te = this.elements;

    const a11 = ae[0], a12 = ae[4], a13 = ae[8], a14 = ae[12];
    const a21 = ae[1], a22 = ae[5], a23 = ae[9], a24 = ae[13];
    const a31 = ae[2], a32 = ae[6], a33 = ae[10], a34 = ae[14];
    const a41 = ae[3], a42 = ae[7], a43 = ae[11], a44 = ae[15];

    const b11 = be[0], b12 = be[4], b13 = be[8], b14 = be[12];
    const b21 = be[1], b22 = be[5], b23 = be[9], b24 = be[13];
    const b31 = be[2], b32 = be[6], b33 = be[10], b34 = be[14];
    const b41 = be[3], b42 = be[7], b43 = be[11], b44 = be[15];

    te[0] = a11 * b11 + a12 * b21 + a13 * b31 + a14 * b41;
    te[1] = a21 * b11 + a22 * b21 + a23 * b31 + a24 * b41;
    te[2] = a31 * b11 + a32 * b21 + a33 * b31 + a34 * b41;
    te[3] = a41 * b11 + a42 * b21 + a43 * b31 + a44 * b41;

    te[4] = a11 * b12 + a12 * b22 + a13 * b32 + a14 * b42;
    te[5] = a21 * b12 + a22 * b22 + a23 * b32 + a24 * b42;
    te[6] = a31 * b12 + a32 * b22 + a33 * b32 + a34 * b42;
    te[7] = a41 * b12 + a42 * b22 + a43 * b32 + a44 * b42;

    te[8] = a11 * b13 + a12 * b23 + a13 * b33 + a14 * b43;
    te[9] = a21 * b13 + a22 * b23 + a23 * b33 + a24 * b43;
    te[10] = a31 * b13 + a32 * b23 + a33 * b33 + a34 * b43;
    te[11] = a41 * b13 + a42 * b23 + a43 * b33 + a44 * b43;

    te[12] = a11 * b14 + a12 * b24 + a13 * b34 + a14 * b44;
    te[13] = a21 * b14 + a22 * b24 + a23 * b34 + a24 * b44;
    te[14] = a31 * b14 + a32 * b24 + a33 * b34 + a34 * b44;
    te[15] = a41 * b14 + a42 * b24 + a43 * b34 + a44 * b44;

    return this;
  }

  multiply(m) {
    return this.multiplyMatrices(this, m);
  }

  premultiply(m) {
    return this.multiplyMatrices(m, this);
  }

  decompose(position, quaternion, scale) {
    const te = this.elements;
    let sx = Math.hypot(te[0], te[1], te[2]);
    let sy = Math.hypot(te[4], te[5], te[6]);
    let sz = Math.hypot(te[8], te[9], te[10]);

    const det = this.determinant();
    if (det < 0) sx = -sx;

    position.x = te[12];
    position.y = te[13];
    position.z = te[14];

    scale.x = sx;
    scale.y = sy;
    scale.z = sz;

    if (quaternion) {
      const invSX = 1 / (sx || 1);
      const invSY = 1 / (sy || 1);
      const invSZ = 1 / (sz || 1);

      const m11 = te[0] * invSX, m12 = te[4] * invSY, m13 = te[8] * invSZ;
      const m21 = te[1] * invSX, m22 = te[5] * invSY, m23 = te[9] * invSZ;
      const m31 = te[2] * invSX, m32 = te[6] * invSY, m33 = te[10] * invSZ;

      const trace = m11 + m22 + m33;
      if (trace > 0) {
        const s = 0.5 / Math.sqrt(trace + 1.0);
        quaternion.w = 0.25 / s;
        quaternion.x = (m32 - m23) * s;
        quaternion.y = (m13 - m31) * s;
        quaternion.z = (m21 - m12) * s;
      } else if (m11 > m22 && m11 > m33) {
        const s = 2.0 * Math.sqrt(1.0 + m11 - m22 - m33);
        quaternion.w = (m32 - m23) / s;
        quaternion.x = 0.25 * s;
        quaternion.y = (m12 + m21) / s;
        quaternion.z = (m13 + m31) / s;
      } else if (m22 > m33) {
        const s = 2.0 * Math.sqrt(1.0 + m22 - m11 - m33);
        quaternion.w = (m13 - m31) / s;
        quaternion.x = (m12 + m21) / s;
        quaternion.y = 0.25 * s;
        quaternion.z = (m23 + m32) / s;
      } else {
        const s = 2.0 * Math.sqrt(1.0 + m33 - m11 - m22);
        quaternion.w = (m21 - m12) / s;
        quaternion.x = (m13 + m31) / s;
        quaternion.y = (m23 + m32) / s;
        quaternion.z = 0.25 * s;
      }
    }

    return this;
  }
}

class Euler {
  constructor(x = 0, y = 0, z = 0, order = 'XYZ') {
    this.x = x;
    this.y = y;
    this.z = z;
    this.order = order;
  }

  set(x, y, z, order = this.order) {
    this.x = x;
    this.y = y;
    this.z = z;
    this.order = order;
    return this;
  }
}

class Color {
  constructor(r = 1, g = 1, b = 1) {
    if (typeof r === 'string') {
      this.setStyle(r);
    } else if (typeof r === 'number' && g === undefined) {
      this.setHex(r);
    } else {
      this.r = r;
      this.g = g;
      this.b = b;
    }
  }

  setHex(hex) {
    hex = Math.floor(hex);
    this.r = ((hex >> 16) & 255) / 255;
    this.g = ((hex >> 8) & 255) / 255;
    this.b = (hex & 255) / 255;
    return this;
  }

  setStyle(style) {
    if (style.startsWith('#')) {
      const hex = parseInt(style.slice(1), 16);
      return this.setHex(hex);
    }
    return this;
  }

  getHexString() {
    return (
      ('0' + Math.floor(this.r * 255).toString(16)).slice(-2) +
      ('0' + Math.floor(this.g * 255).toString(16)).slice(-2) +
      ('0' + Math.floor(this.b * 255).toString(16)).slice(-2)
    );
  }
}

class Object3D {
  constructor() {
    this.id = Math.floor(Math.random() * 1000000);
    this.uuid = Math.random().toString(36).slice(2);
    this.name = '';
    this.type = 'Object3D';
    this.parent = null;
    this.children = [];
    this.position = new Vector3();
    this.rotation = new Euler();
    this.quaternion = new Quaternion();
    this.scale = new Vector3(1, 1, 1);
    this.matrix = new Matrix4();
    this.matrixWorld = new Matrix4();
    this.visible = true;
    this.up = new Vector3(0, 0, 1); // Source 2 Z-up coordinate default
  }

  add(...objects) {
    for (const obj of objects) {
      if (obj && obj !== this) {
        if (obj.parent) obj.parent.remove(obj);
        obj.parent = this;
        this.children.push(obj);
      }
    }
    return this;
  }

  remove(...objects) {
    for (const obj of objects) {
      const idx = this.children.indexOf(obj);
      if (idx !== -1) {
        obj.parent = null;
        this.children.splice(idx, 1);
      }
    }
    return this;
  }

  traverse(callback) {
    callback(this);
    for (const child of this.children) {
      child.traverse(callback);
    }
  }

  updateMatrix() {
    this.matrix.compose(this.position, this.quaternion, this.scale);
  }

  updateMatrixWorld(force) {
    this.updateMatrix();
    if (this.parent) {
      // world transform product
      this.matrixWorld.multiplyMatrices(this.parent.matrixWorld, this.matrix);
    } else {
      this.matrixWorld.copy(this.matrix);
    }
    for (const child of this.children) {
      child.updateMatrixWorld(force);
    }
  }
}

class Scene extends Object3D {
  constructor() {
    super();
    this.type = 'Scene';
    this.background = null;
  }
}

class Group extends Object3D {
  constructor() {
    super();
    this.type = 'Group';
  }
}

class Camera extends Object3D {
  constructor() {
    super();
    this.type = 'Camera';
    this.matrixWorldInverse = new Matrix4();
    this.projectionMatrix = new Matrix4();
  }
}

class PerspectiveCamera extends Camera {
  constructor(fov = 50, aspect = 1, near = 0.1, far = 2000) {
    super();
    this.type = 'PerspectiveCamera';
    this.fov = fov;
    this.aspect = aspect;
    this.near = near;
    this.far = far;
    this.updateProjectionMatrix();
  }

  updateProjectionMatrix() {}
}

class OrthographicCamera extends Camera {
  constructor(left = -1, right = 1, top = 1, bottom = -1, near = 0.1, far = 2000) {
    super();
    this.type = 'OrthographicCamera';
    this.left = left;
    this.right = right;
    this.top = top;
    this.bottom = bottom;
    this.near = near;
    this.far = far;
    this.zoom = 1;
    this.updateProjectionMatrix();
  }

  updateProjectionMatrix() {}
}

class BufferAttribute {
  constructor(array, itemSize, normalized = false) {
    this.array = array;
    this.itemSize = itemSize;
    this.count = array ? array.length / itemSize : 0;
    this.normalized = normalized;
    this.needsUpdate = false;
  }

  setXYZ(index, x, y, z) {
    const idx = index * this.itemSize;
    this.array[idx] = x;
    this.array[idx + 1] = y;
    this.array[idx + 2] = z;
    this.needsUpdate = true;
    return this;
  }
}

class InstancedBufferAttribute extends BufferAttribute {
  constructor(array, itemSize, normalized, meshPerAttribute = 1) {
    super(array, itemSize, normalized);
    this.meshPerAttribute = meshPerAttribute;
  }
}

class BufferGeometry {
  constructor() {
    this.id = Math.floor(Math.random() * 1000000);
    this.attributes = {};
    this.disposed = false;
  }

  setAttribute(name, attribute) {
    this.attributes[name] = attribute;
    return this;
  }

  getAttribute(name) {
    return this.attributes[name];
  }

  dispose() {
    this.disposed = true;
  }
}

class BoxGeometry extends BufferGeometry {
  constructor(width = 1, height = 1, depth = 1) {
    super();
    this.parameters = { width, height, depth };
  }
}

class SphereGeometry extends BufferGeometry {
  constructor(radius = 1, widthSegments = 8, heightSegments = 6) {
    super();
    this.parameters = { radius, widthSegments, heightSegments };
  }
}

class CylinderGeometry extends BufferGeometry {
  constructor(radiusTop = 1, radiusBottom = 1, height = 1) {
    super();
    this.parameters = { radiusTop, radiusBottom, height };
  }
}

class PlaneGeometry extends BufferGeometry {
  constructor(width = 1, height = 1) {
    super();
    this.parameters = { width, height };
  }
}

class Material {
  constructor() {
    this.id = Math.floor(Math.random() * 1000000);
    this.type = 'Material';
    this.opacity = 1;
    this.transparent = false;
    this.wireframe = false;
    this.visible = true;
    this.disposed = false;
  }

  dispose() {
    this.disposed = true;
  }
}

class MeshBasicMaterial extends Material {
  constructor(parameters = {}) {
    super();
    this.type = 'MeshBasicMaterial';
    this.color = new Color(parameters.color !== undefined ? parameters.color : 0xffffff);
    if (parameters.opacity !== undefined) this.opacity = parameters.opacity;
    if (parameters.transparent !== undefined) this.transparent = parameters.transparent;
    if (parameters.wireframe !== undefined) this.wireframe = parameters.wireframe;
  }
}

class MeshStandardMaterial extends Material {
  constructor(parameters = {}) {
    super();
    this.type = 'MeshStandardMaterial';
    this.color = new Color(parameters.color !== undefined ? parameters.color : 0xffffff);
    this.roughness = parameters.roughness !== undefined ? parameters.roughness : 0.5;
    this.metalness = parameters.metalness !== undefined ? parameters.metalness : 0.5;
  }
}

class LineBasicMaterial extends Material {
  constructor(parameters = {}) {
    super();
    this.type = 'LineBasicMaterial';
    this.color = new Color(parameters.color !== undefined ? parameters.color : 0xffffff);
  }
}

class Mesh extends Object3D {
  constructor(geometry, material) {
    super();
    this.type = 'Mesh';
    this.geometry = geometry || new BufferGeometry();
    this.material = material || new MeshBasicMaterial();
  }
}

class InstancedMesh extends Mesh {
  constructor(geometry, material, count) {
    super(geometry, material);
    this.type = 'InstancedMesh';
    this.count = count;
    this.instanceMatrix = new BufferAttribute(new Float32Array(count * 16), 16);
    this.instanceColor = new BufferAttribute(new Float32Array(count * 3), 3);
  }

  setMatrixAt(index, matrix) {
    const e = matrix.elements;
    const offset = index * 16;
    for (let i = 0; i < 16; i++) {
      this.instanceMatrix.array[offset + i] = e[i];
    }
    this.instanceMatrix.needsUpdate = true;
  }

  getMatrixAt(index, matrix) {
    const offset = index * 16;
    for (let i = 0; i < 16; i++) {
      matrix.elements[i] = this.instanceMatrix.array[offset + i];
    }
  }

  setColorAt(index, color) {
    const offset = index * 3;
    this.instanceColor.array[offset] = color.r;
    this.instanceColor.array[offset + 1] = color.g;
    this.instanceColor.array[offset + 2] = color.b;
    this.instanceColor.needsUpdate = true;
  }

  getColorAt(index, color) {
    const offset = index * 3;
    color.r = this.instanceColor.array[offset];
    color.g = this.instanceColor.array[offset + 1];
    color.b = this.instanceColor.array[offset + 2];
  }
}

class Line extends Object3D {
  constructor(geometry, material) {
    super();
    this.type = 'Line';
    this.geometry = geometry || new BufferGeometry();
    this.material = material || new LineBasicMaterial();
  }
}

class LineSegments extends Line {
  constructor(geometry, material) {
    super(geometry, material);
    this.type = 'LineSegments';
  }
}

class GridHelper extends LineSegments {
  constructor(size = 10, divisions = 10) {
    super();
    this.size = size;
    this.divisions = divisions;
  }
}

class AxesHelper extends LineSegments {
  constructor(size = 1) {
    super();
    this.size = size;
  }
}

class WebGLRenderer {
  constructor(parameters = {}) {
    this.domElement = parameters.canvas || {
      width: 800,
      height: 600,
      clientWidth: 800,
      clientHeight: 600,
      style: {},
      getContext: () => null
    };
    this.antialias = parameters.antialias !== undefined ? parameters.antialias : false;
    this.pixelRatio = 1.0;
    this.width = 800;
    this.height = 600;
    this.clearColor = new Color(0x000000);
    this.clearAlpha = 1.0;
    this.disposed = false;
  }

  setSize(width, height, updateStyle = true) {
    this.width = width;
    this.height = height;
    if (this.domElement) {
      this.domElement.width = width;
      this.domElement.height = height;
      if (updateStyle && this.domElement.style) {
        this.domElement.style.width = width + 'px';
        this.domElement.style.height = height + 'px';
      }
    }
  }

  getSize(target = {}) {
    target.width = this.width;
    target.height = this.height;
    return target;
  }

  setPixelRatio(ratio) {
    this.pixelRatio = Math.min(ratio, 2.0); // Safe clamp
  }

  getPixelRatio() {
    return this.pixelRatio;
  }

  setClearColor(color, alpha = 1.0) {
    if (typeof color === 'number' || typeof color === 'string') {
      this.clearColor.setHex(color);
    } else if (color instanceof Color) {
      this.clearColor = color;
    }
    this.clearAlpha = alpha;
  }

  clear() {}

  render(scene, camera) {
    if (this.disposed) return;
    if (scene) scene.updateMatrixWorld();
  }

  dispose() {
    this.disposed = true;
  }

  forceContextLoss() {
    const gl = typeof this.domElement.getContext === 'function' ? this.domElement.getContext('webgl2') : null;
    if (gl && typeof gl.simulateContextLoss === 'function') {
      gl.simulateContextLoss();
    }
  }
}

class OrbitControls {
  constructor(object, domElement) {
    this.object = object;
    this.domElement = domElement;
    this.enabled = true;
    this.target = new Vector3(0, 0, 32);
    this.enableDamping = true;
    this.dampingFactor = 0.05;
    this.maxPolarAngle = Math.PI / 2 + 0.1;
    this.minDistance = 1;
    this.maxDistance = 5000;
    this.disposed = false;
  }

  update() {
    return true;
  }

  dispose() {
    this.disposed = true;
  }
}

module.exports = {
  Vector3,
  Quaternion,
  Matrix4,
  Euler,
  Color,
  Object3D,
  Scene,
  Group,
  Camera,
  PerspectiveCamera,
  OrthographicCamera,
  BufferAttribute,
  InstancedBufferAttribute,
  BufferGeometry,
  BoxGeometry,
  SphereGeometry,
  CylinderGeometry,
  PlaneGeometry,
  Material,
  MeshBasicMaterial,
  MeshStandardMaterial,
  LineBasicMaterial,
  Mesh,
  InstancedMesh,
  Line,
  LineSegments,
  GridHelper,
  AxesHelper,
  WebGLRenderer,
  OrbitControls
};
