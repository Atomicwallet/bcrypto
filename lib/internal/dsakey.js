/*!
 * dsakey.js - DSA keys for javascript
 * Copyright (c) 2018-2019, Christopher Jeffrey (MIT License).
 * https://github.com/bcoin-org/bcrypto
 *
 * Resources:
 *   https://tools.ietf.org/html/rfc7517
 *   https://tools.ietf.org/html/rfc7518
 *   https://mozilla.github.io/id-specs/docs/formats/keys/
 */

'use strict';

const assert = require('bsert');
const base64 = require('../encoding/base64');
const {countLeft, trimLeft} = require('../encoding/util');
const {custom} = require('./custom');

/*
 * Constants
 */

const DEFAULT_BITS = 2048;
const MIN_BITS = 512;
const MAX_BITS = 10000;

/**
 * DSA Params
 */

class DSAParams {
  constructor(p, q, g) {
    this.p = trimLeft(p);
    this.q = trimLeft(q);
    this.g = trimLeft(g);
  }

  setP(p) {
    this.p = trimLeft(p);
    return this;
  }

  setQ(q) {
    this.q = trimLeft(q);
    return this;
  }

  setG(g) {
    this.g = trimLeft(g);
    return this;
  }

  L() {
    return countLeft(this.p);
  }

  N() {
    return countLeft(this.q);
  }

  bits() {
    return this.L();
  }

  size() {
    return (this.N() + 7) >>> 3;
  }

  toParams() {
    return this;
  }

  toJSON() {
    return {
      kty: 'DSA',
      p: base64.encodeURL(this.p),
      q: base64.encodeURL(this.q),
      g: base64.encodeURL(this.g),
      ext: true
    };
  }

  fromJSON(json) {
    assert(json && typeof json === 'object');
    assert(json.kty === 'DSA');

    this.p = base64.decodeURL(json.p);
    this.q = base64.decodeURL(json.q);
    this.g = base64.decodeURL(json.g);

    return this;
  }

  [custom]() {
    return this.format();
  }

  format() {
    return {
      bits: this.bits(),
      size: this.size(),
      pbits: countLeft(this.p),
      qbits: countLeft(this.q),
      gbits: countLeft(this.g),
      p: this.p.toString('hex'),
      q: this.q.toString('hex'),
      g: this.g.toString('hex')
    };
  }

  static fromJSON(json) {
    return new this().fromJSON(json);
  }
}

/**
 * DSA Key
 */

class DSAKey extends DSAParams {
  constructor(p, q, g, y) {
    super(p, q, g);
    this.y = trimLeft(y);
  }

  setParams(params) {
    return this.fromParams(params);
  }

  setY(y) {
    this.y = trimLeft(y);
    return this;
  }

  toParams() {
    return new DSAParams(this.p, this.q, this.g);
  }

  fromParams(params) {
    assert(params instanceof DSAParams);
    this.p = params.p;
    this.q = params.q;
    this.g = params.g;
    return this;
  }

  toPublic() {
    return this;
  }

  toJSON() {
    return {
      kty: 'DSA',
      p: base64.encodeURL(this.p),
      q: base64.encodeURL(this.q),
      g: base64.encodeURL(this.g),
      y: base64.encodeURL(this.y),
      ext: true
    };
  }

  fromJSON(json) {
    assert(json && typeof json === 'object');
    assert(json.kty === 'DSA');

    this.p = base64.decodeURL(json.p);
    this.q = base64.decodeURL(json.q);
    this.g = base64.decodeURL(json.g);
    this.y = base64.decodeURL(json.y);

    return this;
  }

  static fromParams(params) {
    return new this().fromParams(params);
  }
}

/**
 * DSA Public Key
 */

class DSAPublicKey extends DSAKey {
  constructor(p, q, g, y) {
    super(p, q, g, y);
  }

  format() {
    return {
      bits: this.bits(),
      size: this.size(),
      pbits: countLeft(this.p),
      qbits: countLeft(this.q),
      gbits: countLeft(this.g),
      ybits: countLeft(this.y),
      p: this.p.toString('hex'),
      q: this.q.toString('hex'),
      g: this.g.toString('hex'),
      y: this.y.toString('hex')
    };
  }
}

/**
 * DSA Public Key
 */

class DSAPrivateKey extends DSAKey {
  constructor(p, q, g, y, x) {
    super(p, q, g, y);
    this.x = trimLeft(x);
  }

  setX(x) {
    this.x = trimLeft(x);
    return this;
  }

  toPublic() {
    const key = new DSAPublicKey();
    key.p = this.p;
    key.q = this.q;
    key.g = this.g;
    key.y = this.y;
    return key;
  }

  toJSON() {
    return {
      kty: 'DSA',
      p: base64.encodeURL(this.p),
      q: base64.encodeURL(this.q),
      g: base64.encodeURL(this.g),
      y: base64.encodeURL(this.y),
      x: base64.encodeURL(this.x),
      ext: true
    };
  }

  fromJSON(json) {
    assert(json && typeof json === 'object');
    assert(json.kty === 'DSA');

    this.p = base64.decodeURL(json.p);
    this.q = base64.decodeURL(json.q);
    this.g = base64.decodeURL(json.g);

    if (json.y != null)
      this.y = base64.decodeURL(json.y);

    this.x = base64.decodeURL(json.x);

    return this;
  }

  format() {
    return {
      bits: this.bits(),
      size: this.size(),
      pbits: countLeft(this.p),
      qbits: countLeft(this.q),
      gbits: countLeft(this.g),
      ybits: countLeft(this.y),
      xbits: countLeft(this.x),
      p: this.p.toString('hex'),
      q: this.q.toString('hex'),
      g: this.g.toString('hex'),
      y: this.y.toString('hex'),
      x: this.x.toString('hex')
    };
  }
}

/*
 * Expose
 */

exports.DEFAULT_BITS = DEFAULT_BITS;
exports.MIN_BITS = MIN_BITS;
exports.MAX_BITS = MAX_BITS;

exports.DSAKey = DSAKey;
exports.DSAParams = DSAParams;
exports.DSAPublicKey = DSAPublicKey;
exports.DSAPrivateKey = DSAPrivateKey;
