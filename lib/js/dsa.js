/*!
 * dsa.js - DSA generation for javascript
 * Copyright (c) 2018, Christopher Jeffrey (MIT License).
 * https://github.com/bcoin-org/bcoin
 *
 * Parts of this software are based on golang/go:
 *   Copyright (c) 2009 The Go Authors. All rights reserved.
 *   https://github.com/golang/go
 *
 * Resources:
 *   https://github.com/golang/go/blob/master/src/crypto/dsa/dsa.go
 *   https://github.com/golang/go/blob/master/src/math/big/int.go
 */

'use strict';

const assert = require('bsert');
const BN = require('../../vendor/bn.js');
const random = require('../random');
const {probablyPrime} = require('../internal/primes');
const dsakey = require('../internal/dsakey')(exports);
const DSASignature = require('../internal/dsasig');

const {
  DSAKey,
  DSAParams,
  DSAPublicKey,
  DSAPrivateKey
} = dsakey;

/*
 * DSA
 */

function paramsGenerate(bits) {
  if (bits == null)
    bits = 2048;

  assert((bits >>> 0) === bits);

  if (bits < 1024 || bits > 3072)
    throw new RangeError('`bits` must range between 1024 and 3072.');

  // OpenSSL behavior.
  const L = bits;
  const N = bits < 2048 ? 160 : 256;

  return generate(L, N);
}

async function paramsGenerateAsync(bits) {
  return paramsGenerate(bits);
}

function privateKeyCreate(params) {
  assert(params instanceof DSAParams);

  let xn = new BN(0);

  const qn = new BN(params.q);
  const xb = Buffer.alloc(qn.bitLength() >>> 3);

  for (;;) {
    random.randomFill(xb, 0, xb.length);

    xn = new BN(xb);

    if (!xn.isZero() && xn.cmp(qn) < 0)
      break;
  }

  const pn = new BN(params.p);
  const gn = new BN(params.g);
  const yn = modPow(gn, xn, pn);

  const key = new DSAPrivateKey();
  key.setParams(params);
  key.x = toBuffer(xn);
  key.y = toBuffer(yn);
  return key;
}

function privateKeyGenerate(bits) {
  const params = paramsGenerate(bits);
  return privateKeyCreate(params);
}

async function privateKeyGenerateAsync(bits) {
  const params = await paramsGenerateAsync(bits);
  return privateKeyCreate(params);
}

function computeY(key) {
  assert(key instanceof DSAPrivateKey);

  const p = new BN(key.p);
  const g = new BN(key.g);
  const x = new BN(key.x);
  const y = modPow(g, x, p);

  return toBuffer(y);
}

function publicKeyCreate(key) {
  assert(key instanceof DSAPrivateKey);
  return key.toPublic();
}

function publicKeyVerify(key) {
  assert(key instanceof DSAPublicKey);
  return key.validate();
}

function privateKeyVerify(key) {
  assert(key instanceof DSAPrivateKey);

  if (!key.toPublic().validate())
    return false;

  return key.validate();
}

function sign(msg, key) {
  assert(Buffer.isBuffer(msg));
  assert(key instanceof DSAPrivateKey);

  const pn = new BN(key.p);
  const qn = new BN(key.q);
  const gn = new BN(key.g);
  const xn = new BN(key.x);

  let n = qn.bitLength();

  if (qn.cmpn(0) <= 0
      || pn.cmpn(0) <= 0
      || gn.cmpn(0) <= 0
      || xn.cmpn(0) <= 0
      || (n & 7) !== 0) {
    throw new Error('Invalid key.');
  }

  n >>>= 3;

  let attempts = 10;
  let r, s;

  for (; attempts > 0; attempts--) {
    let k = new BN(0);

    const buf = Buffer.allocUnsafe(n);

    for (;;) {
      random.randomFill(buf, 0, n);
      k = new BN(buf);

      if (!k.isZero() && k.cmp(qn) < 0)
        break;
    }

    const ki = fermatInverse(k, qn);

    r = modPow(gn, k, pn);
    r = r.mod(qn);

    if (r.isZero())
      continue;

    const z = new BN(msg);

    s = xn.mul(r);
    s.iadd(z);
    s = s.mod(qn);
    s.imul(ki);
    s = s.mod(qn);

    if (!s.isZero())
      break;
  }

  if (attempts === 0)
    throw new Error('Could not sign.');

  const sig = new DSASignature(key.size());

  sig.setR(toBuffer(r));
  sig.setS(toBuffer(s));

  return sig;
}

function verify(msg, sig, key) {
  assert(Buffer.isBuffer(msg));
  assert(sig instanceof DSASignature);
  assert(key instanceof DSAKey);

  const pn = new BN(key.p);
  const qn = new BN(key.q);
  const gn = new BN(key.g);
  const yn = new BN(key.y);

  const rn = new BN(sig.r);
  const sn = new BN(sig.s);

  if (pn.isZero())
    return false;

  if (rn.isZero() || rn.cmp(qn) >= 0)
    return false;

  if (sn.isZero() || sn.cmp(qn) >= 0)
    return false;

  let w = sn.invm(qn);

  const n = qn.bitLength();

  if ((n & 7) !== 0)
    return false;

  const z = new BN(msg);

  let u1 = z.mul(w);
  u1 = u1.mod(qn);
  w = rn.mul(w);

  let u2 = w;
  u2 = u2.mod(qn);

  u1 = modPow(gn, u1, pn);
  let v = u1;

  u2 = modPow(yn, u2, pn);

  v.imul(u2);
  v = v.mod(pn);
  v = v.mod(qn);

  return v.cmp(rn) === 0;
}

/*
 * Generation
 */

function generate(L, N) {
  assert((L >>> 0) === L);
  assert((N >>> 0) === N);

  if (!(L === 1024 && N === 160)
      && !(L === 2048 && N === 224)
      && !(L === 2048 && N === 256)
      && !(L === 3072 && N === 256)) {
    throw new Error('Invalid parameter sizes.');
  }

  const qb = Buffer.alloc(N >>> 3);
  const pb = Buffer.alloc(L >>> 3);

  let qn = new BN(0);
  let pn = new BN(0);
  let rem = new BN(0);

generate:
  for (;;) {
    random.randomFill(qb, 0, N >>> 3);

    qb[qb.length - 1] |= 1;
    qb[0] |= 0x80;

    qn = new BN(qb);

    if (!probablyPrime(qn, 64))
      continue;

    for (let i = 0; i < 4 * L; i++) {
      random.randomFill(pb, 0, L >>> 3);

      pb[pb.length - 1] |= 1;
      pb[0] |= 0x80;

      pn = new BN(pb);

      rem = pn.mod(qn);
      rem.isubn(1);
      pn.isub(rem);

      if (pn.bitLength() < L)
        continue;

      if (!probablyPrime(pn, 64))
        continue;

      break generate;
    }
  }

  const h = new BN(2);
  const pm1 = pn.subn(1);
  const e = pm1.div(qn);

  for (;;) {
    const gn = modPow(h, e, pn);

    if (gn.cmpn(1) === 0) {
      h.iaddn(1);
      continue;
    }

    const params = new DSAParams();
    params.p = toBuffer(pn);
    params.q = toBuffer(qn);
    params.g = toBuffer(gn);
    return params;
  }
}

function modPow(x, y, m) {
  return x.toRed(BN.mont(m)).redPow(y).fromRed();
}

function fermatInverse(k, p) {
  return modPow(k, p.subn(2), p);
}

/*
 * Helpers
 */

function toBuffer(n) {
  return n.toArrayLike(Buffer, 'be');
}

/*
 * Expose
 */

exports.native = 0;
exports.DSAKey = DSAKey;
exports.DSAParams = DSAParams;
exports.DSAPublicKey = DSAPublicKey;
exports.DSAPrivateKey = DSAPrivateKey;
exports.DSASignature = DSASignature;

exports.paramsGenerate = paramsGenerate;
exports.paramsGenerateAsync = paramsGenerateAsync;
exports.privateKeyCreate = privateKeyCreate;
exports.privateKeyGenerate = privateKeyGenerate;
exports.privateKeyGenerateAsync = privateKeyGenerateAsync;
exports.computeY = computeY;
exports.publicKeyCreate = publicKeyCreate;
exports.publicKeyVerify = publicKeyVerify;
exports.privateKeyVerify = privateKeyVerify;
exports.sign = sign;
exports.verify = verify;