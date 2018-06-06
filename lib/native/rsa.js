/*!
 * rsa.js - RSA for javascript
 * Copyright (c) 2018, Christopher Jeffrey (MIT License).
 * https://github.com/bcoin-org/bcoin
 */

'use strict';

const assert = require('assert');
const binding = require('./binding').rsa;
const rsakey = require('../internal/rsakey');
const rsa = exports;

const {
  RSAKey,
  RSAPrivateKey,
  RSAPublicKey
} = rsakey;

/**
 * RSAPrivateKey
 */

rsa.RSAPrivateKey = RSAPrivateKey;

/**
 * RSAPublicKey
 */

rsa.RSAPublicKey = RSAPublicKey;

/**
 * Generate a private key.
 * @param {Number} [bits=2048]
 * @returns {RSAPrivateKey} Private key.
 */

rsa.generateKey = function generateKey(bits = 2048) {
  assert((bits >>> 0) === bits);

  assert(bits === 512
    || bits === 1024
    || bits === 2048
    || bits === 4096
    || bits === 8192);

  const key = new RSAPrivateKey();
  const items = binding.privateKeyGenerate(bits);

  [
    key.n,
    key.e,
    key.d,
    key.p,
    key.q,
    key.dp,
    key.dq,
    key.qi
  ] = items;

  return key;
};

/**
 * Verify a public key.
 * @param {RSAKey} key
 * @returns {Boolean}
 */

rsa.publicVerify = function publicVerify(key) {
  assert(key instanceof RSAKey);
  return binding.publicKeyVerify(key.n, key.e);
};

/**
 * Verify a private key.
 * @param {RSAPrivateKey} key
 * @returns {Boolean}
 */

rsa.privateVerify = function privateVerify(key) {
  assert(key instanceof RSAPrivateKey);
  return binding.privateKeyVerify(
    key.n,
    key.e,
    key.d,
    key.p,
    key.q,
    key.dp,
    key.dq,
    key.qi
  );
};

/**
 * Generate a private key.
 * @param {Number} [bits=2048]
 * @returns {Buffer} Private key.
 */

rsa.privateKeyGenerate = function privateKeyGenerate(bits) {
  const key = rsa.generateKey(bits);
  return key.encode();
};

/**
 * Generate a private key.
 * @param {Number} [bits=2048]
 * @returns {Buffer} Private key.
 */

rsa.generatePrivateKey = rsa.privateKeyGenerate;

/**
 * Create a public key from a private key.
 * @param {Buffer} key
 * @returns {Buffer}
 */

rsa.publicKeyCreate = function publicKeyCreate(key) {
  const k = RSAPrivateKey.decode(key);
  const p = k.toPublic();
  return p.encode();
};

/**
 * Validate a public key.
 * @param {Number} bits
 * @param {Buffer} key
 * @returns {Boolean} True if buffer is a valid public key.
 */

rsa.publicKeyVerify = function publicKeyVerify(key) {
  assert(Buffer.isBuffer(key));

  let k;

  try {
    k = RSAPublicKey.decode(key);
  } catch (e) {
    return false;
  }

  return rsa.publicVerify(k);
};

/**
 * Validate a private key.
 * @param {Buffer} key
 * @returns {Boolean} True if buffer is a valid private key.
 */

rsa.privateKeyVerify = function privateKeyVerify(key) {
  assert(Buffer.isBuffer(key));

  let k;

  try {
    k = RSAPrivateKey.decode(key);
  } catch (e) {
    return false;
  }

  return rsa.privateVerify(k);
};

/**
 * Sign a message.
 * @param {Object} hash
 * @param {Buffer} msg
 * @param {Buffer} key - Private key.
 * @returns {Buffer} PKCS#1v1.5-formatted signature.
 */

rsa.sign = function sign(hash, msg, key) {
  const k = RSAPrivateKey.decode(key);
  return rsa.signKey(hash, msg, k);
};

/**
 * Sign a message.
 * @param {Object} hash
 * @param {Buffer} msg
 * @param {RSAPrivateKey} key - Private key.
 * @returns {Buffer} PKCS#1v1.5-formatted signature.
 */

rsa.signKey = function signKey(hash, msg, key) {
  assert(hash && typeof hash.id === 'string', 'No algorithm selected.');
  assert(Buffer.isBuffer(msg));
  assert(key instanceof RSAPrivateKey);

  return binding.sign(
    hash.id,
    msg,
    key.n,
    key.e,
    key.d,
    key.p,
    key.q,
    key.dp,
    key.dq,
    key.qi
  );
};

/**
 * Verify a signature.
 * @param {Object} hash
 * @param {Buffer} msg
 * @param {Buffer} sig - PKCS#1v1.5-formatted.
 * @param {Buffer} key
 * @returns {Boolean}
 */

rsa.verify = function verify(hash, msg, sig, key) {
  let k;

  try {
    k = RSAPublicKey.decode(key);
  } catch (e) {
    return false;
  }

  return rsa.verifyKey(hash, msg, sig, k);
};

/**
 * Verify a signature.
 * @param {Object} hash
 * @param {Buffer} msg
 * @param {Buffer} sig - PKCS#1v1.5-formatted.
 * @param {RSAPublicKey} key
 * @returns {Boolean}
 */

rsa.verifyKey = function verifyKey(hash, msg, sig, key) {
  assert(hash && typeof hash.id === 'string', 'No algorithm selected.');
  assert(Buffer.isBuffer(msg));
  assert(Buffer.isBuffer(sig));
  assert(key instanceof RSAPublicKey);

  return binding.verify(
    hash.id,
    msg,
    sig,
    key.n,
    key.e,
    key.d,
    key.p,
    key.q,
    key.dp,
    key.dq,
    key.qi
  );
};