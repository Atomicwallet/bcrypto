'use strict';

const assert = require('bsert');
const base64 = require('../lib/encoding/base64');

// https://tools.ietf.org/html/rfc4648#section-10
const vectors = [
  ['', ''],
  ['66', 'Zg=='],
  ['666f', 'Zm8='],
  ['666f6f', 'Zm9v'],
  ['666f6f62', 'Zm9vYg=='],
  ['666f6f6261', 'Zm9vYmE='],
  ['666f6f626172', 'Zm9vYmFy'],
  ['53e9363b2962fcaf', 'U+k2Oyli/K8=']
];

const invalid = [
  ' Zg==',
  'Zg== ',
  'Z g==',
  'Zg ==',
  'Zg',
  'Zm8',
  'Zm9vYg',
  'Zm9vYmE',
  'U-k2Oyli_K8'
];

const urlVectors = [
  ['', ''],
  ['66', 'Zg'],
  ['666f', 'Zm8'],
  ['666f6f', 'Zm9v'],
  ['666f6f62', 'Zm9vYg'],
  ['666f6f6261', 'Zm9vYmE'],
  ['666f6f626172', 'Zm9vYmFy'],
  ['53e9363b2962fcaf', 'U-k2Oyli_K8']
];

const urlInvalid = [
  'Zg==',
  'Zm8=',
  'Zm9vYg==',
  'Zm9vYmE=',
  'U+k2Oyli/K8='
];

describe('Base64', function() {
  for (const [hex, b64] of vectors) {
    const data = Buffer.from(hex, 'hex');

    it(`should encode and decode base64: ${b64}`, () => {
      assert.strictEqual(base64.test(b64), true);
      assert.strictEqual(base64.encode(data), b64);
      assert.bufferEqual(base64.decode(b64), data);
    });
  }

  for (const b64 of invalid) {
    it(`should recognize invalid base64: ${b64}`, () => {
      assert.strictEqual(base64.test(b64), false);
      assert.throws(() => base64.decode(b64));
    });
  }

  for (const [hex, b64] of urlVectors) {
    const data = Buffer.from(hex, 'hex');

    it(`should encode and decode base64-url: ${b64}`, () => {
      assert.strictEqual(base64.testURL(b64), true);
      assert.strictEqual(base64.encodeURL(data), b64);
      assert.bufferEqual(base64.decodeURL(b64), data);
    });
  }

  for (const b64 of urlInvalid) {
    it(`should recognize invalid base64-url: ${b64}`, () => {
      assert.strictEqual(base64.testURL(b64), false);
      assert.throws(() => base64.decodeURL(b64));
    });
  }
});