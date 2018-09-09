#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "openssl/opensslv.h"
#include "dsa.h"

#if OPENSSL_VERSION_NUMBER >= 0x1010008fL

#include "openssl/bn.h"
#include "openssl/dsa.h"
#include "openssl/objects.h"
#include "../random/random.h"

void
bcrypto_dsa_key_init(bcrypto_dsa_key_t *key) {
  assert(key);
  memset((void *)key, 0x00, sizeof(bcrypto_dsa_key_t));
}

void
bcrypto_dsa_key_free(bcrypto_dsa_key_t *key) {
  assert(key);
  free((void *)key);
}

static size_t
bcrypto_count_bits(const uint8_t *in, size_t in_len) {
  if (in == NULL)
    return 0;

  size_t i = 0;

  for (; i < in_len; i++) {
    if (in[i] != 0)
      break;
  }

  size_t bits = (in_len - i) * 8;

  if (bits == 0)
    return 0;

  bits -= 8;

  uint32_t oct = in[i];

  while (oct) {
    bits += 1;
    oct >>= 1;
  }

  return bits;
}

static bool
bcrypto_dsa_sane_params(const bcrypto_dsa_key_t *params) {
  if (params == NULL)
    return false;

  size_t pb = bcrypto_count_bits(params->pd, params->pl);
  size_t qb = bcrypto_count_bits(params->qd, params->ql);
  size_t gb = bcrypto_count_bits(params->gd, params->gl);

  if (pb < 1024 || pb > 3072)
    return false;

  if (qb != 160 && qb != 224 && qb != 256)
    return false;

  if (gb == 0 || gb > pb)
    return false;

  return true;
}

static bool
bcrypto_dsa_sane_pubkey(const bcrypto_dsa_key_t *key) {
  if (!bcrypto_dsa_sane_params(key))
    return false;

  size_t pb = bcrypto_count_bits(key->pd, key->pl);
  size_t yb = bcrypto_count_bits(key->yd, key->yl);

  if (yb == 0 || yb > pb)
    return false;

  return true;
}

static bool
bcrypto_dsa_sane_privkey(const bcrypto_dsa_key_t *key) {
  if (!bcrypto_dsa_sane_pubkey(key))
    return false;

  size_t qb = bcrypto_count_bits(key->qd, key->ql);
  size_t xb = bcrypto_count_bits(key->xd, key->xl);

  if (xb == 0 || xb > qb)
    return false;

  return true;
}

static bool
bcrypto_dsa_sane_compute(const bcrypto_dsa_key_t *key) {
  if (key == NULL)
    return false;

  size_t pb = bcrypto_count_bits(key->pd, key->pl);
  size_t qb = bcrypto_count_bits(key->qd, key->ql);
  size_t gb = bcrypto_count_bits(key->gd, key->gl);
  size_t yb = bcrypto_count_bits(key->yd, key->yl);
  size_t xb = bcrypto_count_bits(key->xd, key->xl);

  if (pb < 1024 || pb > 3072)
    return false;

  if (qb != 160 && qb != 224 && qb != 256)
    return false;

  if (gb == 0 || gb > pb)
    return false;

  if (yb > pb)
    return false;

  if (xb == 0 || xb > qb)
    return false;

  return true;
}

static bool
bcrypto_dsa_needs_compute(const bcrypto_dsa_key_t *key) {
  if (key == NULL)
    return false;

  return bcrypto_count_bits(key->yd, key->yl) == 0;
}

static size_t
bcrypto_dsa_subprime_size(const bcrypto_dsa_key_t *key) {
  if (key == NULL)
    return 0;

  return (bcrypto_count_bits(key->qd, key->ql) + 7) / 8;
}

static DSA *
bcrypto_dsa_key2dsa(const bcrypto_dsa_key_t *key, int mode) {
  if (key == NULL)
    return NULL;

  if (mode < 0 || mode > 2)
    return NULL;

  DSA *key_d = NULL;
  BIGNUM *p = NULL;
  BIGNUM *q = NULL;
  BIGNUM *g = NULL;
  BIGNUM *y = NULL;
  BIGNUM *x = NULL;

  key_d = DSA_new();

  if (!key_d)
    goto fail;

  p = BN_bin2bn(key->pd, key->pl, NULL);
  q = BN_bin2bn(key->qd, key->ql, NULL);
  g = BN_bin2bn(key->gd, key->gl, NULL);

  if (mode > 0)
    y = BN_bin2bn(key->yd, key->yl, NULL);

  if (mode == 2)
    x = BN_bin2bn(key->xd, key->xl, NULL);

  if (!p || !q || !g || (mode > 0 && !y) || (mode == 2 && !x))
    goto fail;

  if (!DSA_set0_pqg(key_d, p, q, g))
    goto fail;

  p = NULL;
  q = NULL;
  g = NULL;

  if (mode > 0) {
    if (!DSA_set0_key(key_d, y, x))
      goto fail;
  }

  y = NULL;
  x = NULL;

  return key_d;

fail:
  if (key_d)
    DSA_free(key_d);

  if (p)
    BN_free(p);

  if (q)
    BN_free(q);

  if (g)
    BN_free(g);

  if (y)
    BN_free(y);

  if (x)
    BN_free(x);

  return NULL;
}

static DSA *
bcrypto_dsa_key2params(const bcrypto_dsa_key_t *params) {
  return bcrypto_dsa_key2dsa(params, 0);
}

static DSA *
bcrypto_dsa_key2pub(const bcrypto_dsa_key_t *pub) {
  return bcrypto_dsa_key2dsa(pub, 1);
}

static DSA *
bcrypto_dsa_key2priv(const bcrypto_dsa_key_t *priv) {
  return bcrypto_dsa_key2dsa(priv, 2);
}

static bcrypto_dsa_key_t *
bcrypto_dsa_dsa2key(const DSA *key_d, int mode) {
  if (key_d == NULL)
    return NULL;

  if (mode < 0 || mode > 2)
    return NULL;

  uint8_t *arena = NULL;

  const BIGNUM *p = NULL;
  const BIGNUM *q = NULL;
  const BIGNUM *g = NULL;
  const BIGNUM *y = NULL;
  const BIGNUM *x = NULL;

  DSA_get0_pqg(key_d, &p, &q, &g);

  if (mode > 0)
    DSA_get0_key(key_d, &y, mode == 2 ? &x : NULL);

  if (!p || !q || !g || (mode > 0 && !y) || (mode == 2 && !x))
    goto fail;

  size_t pl = BN_num_bytes(p);
  size_t ql = BN_num_bytes(q);
  size_t gl = BN_num_bytes(g);
  size_t yl = mode > 0 ? BN_num_bytes(y) : 0;
  size_t xl = mode == 2 ? BN_num_bytes(x) : 0;

  size_t kl = sizeof(bcrypto_dsa_key_t);
  size_t size = kl + pl + ql + gl + yl + xl;

  arena = malloc(size);

  if (!arena)
    goto fail;

  size_t pos = 0;

  bcrypto_dsa_key_t *key;

  key = (bcrypto_dsa_key_t *)&arena[pos];
  bcrypto_dsa_key_init(key);
  pos += kl;

  key->pd = (uint8_t *)&arena[pos];
  key->pl = pl;
  pos += pl;

  key->qd = (uint8_t *)&arena[pos];
  key->ql = ql;
  pos += ql;

  key->gd = (uint8_t *)&arena[pos];
  key->gl = gl;
  pos += gl;

  if (mode > 0) {
    key->yd = (uint8_t *)&arena[pos];
    key->yl = yl;
    pos += yl;
  }

  if (mode == 2) {
    key->xd = (uint8_t *)&arena[pos];
    key->xl = xl;
    pos += xl;
  }

  assert(BN_bn2bin(p, key->pd) != -1);
  assert(BN_bn2bin(q, key->qd) != -1);
  assert(BN_bn2bin(g, key->gd) != -1);

  if (mode > 0)
    assert(BN_bn2bin(y, key->yd) != -1);

  if (mode == 2)
    assert(BN_bn2bin(x, key->xd) != -1);

  return key;

fail:
  if (arena)
    free(arena);

  return NULL;
}

static bcrypto_dsa_key_t *
bcrypto_dsa_params2key(const DSA *params_d) {
  return bcrypto_dsa_dsa2key(params_d, 0);
}

static bcrypto_dsa_key_t *
bcrypto_dsa_pub2key(const DSA *pub_d) {
  return bcrypto_dsa_dsa2key(pub_d, 1);
}

static bcrypto_dsa_key_t *
bcrypto_dsa_priv2key(const DSA *priv_d) {
  return bcrypto_dsa_dsa2key(priv_d, 2);
}

static DSA_SIG *
bcrypto_dsa_rs2sig(
  const uint8_t *r,
  size_t r_len,
  const uint8_t *s,
  size_t s_len
) {
  DSA_SIG *sig_d = NULL;
  BIGNUM *r_bn = NULL;
  BIGNUM *s_bn = NULL;

  sig_d = DSA_SIG_new();

  if (!sig_d)
    goto fail;

  r_bn = BN_bin2bn(r, r_len, NULL);

  if (!r_bn)
    goto fail;

  s_bn = BN_bin2bn(s, s_len, NULL);

  if (!s_bn)
    goto fail;

  if (!DSA_SIG_set0(sig_d, r_bn, s_bn))
    goto fail;

  return sig_d;

fail:
  if (sig_d)
    DSA_SIG_free(sig_d);

  if (r_bn)
    BN_free(r_bn);

  if (s_bn)
    BN_free(s_bn);

  return NULL;
}

static bool
bcrypto_dsa_sig2rs(
  const DSA *priv_d,
  const DSA_SIG *sig_d,
  uint8_t **r,
  uint8_t **s
) {
  assert(r && s);

  uint8_t *r_buf = NULL;
  uint8_t *s_buf = NULL;

  const BIGNUM *r_bn = NULL;
  const BIGNUM *s_bn = NULL;

  DSA_SIG_get0(sig_d, &r_bn, &s_bn);
  assert(r_bn && s_bn);

  const BIGNUM *q_bn = NULL;

  DSA_get0_pqg(priv_d, NULL, &q_bn, NULL);
  assert(q_bn);

  int bits = BN_num_bits(q_bn);
  size_t size = (bits + 7) / 8;

  assert((size_t)BN_num_bytes(r_bn) <= size);
  assert((size_t)BN_num_bytes(s_bn) <= size);

  r_buf = malloc(size);
  s_buf = malloc(size);

  if (!r_buf || !s_buf)
    goto fail;

  assert(BN_bn2binpad(r_bn, r_buf, size) > 0);
  assert(BN_bn2binpad(s_bn, s_buf, size) > 0);

  *r = r_buf;
  *s = s_buf;

  return true;

fail:
  if (r_buf)
    free(r_buf);

  if (s_buf)
    free(s_buf);

  return false;
}

bcrypto_dsa_key_t *
bcrypto_dsa_params_generate(int bits) {
  DSA *params_d = NULL;

  if (bits < 1024 || bits > 3072)
    goto fail;

  params_d = DSA_new();

  if (!params_d)
    goto fail;

  bcrypto_poll();

  if (!DSA_generate_parameters_ex(params_d, bits, NULL, 0, NULL, NULL, NULL))
    goto fail;

  bcrypto_dsa_key_t *params = bcrypto_dsa_params2key(params_d);

  if (!params)
    goto fail;

  DSA_free(params_d);

  return params;

fail:
  if (params_d)
    DSA_free(params_d);

  return NULL;
}

bool
bcrypto_dsa_params_verify(bcrypto_dsa_key_t *params) {
  DSA *params_d = NULL;
  BN_CTX *ctx = NULL;
  BIGNUM *pm1_bn = NULL;
  BIGNUM *div_bn = NULL;
  BIGNUM *mod_bn = NULL;
  BIGNUM *x_bn = NULL;

  if (!bcrypto_dsa_sane_params(params))
    goto fail;

  params_d = bcrypto_dsa_key2params(params);

  if (!params_d)
    goto fail;

  const BIGNUM *p = NULL;
  const BIGNUM *q = NULL;
  const BIGNUM *g = NULL;

  DSA_get0_pqg(params_d, &p, &q, &g);
  assert(p && q && g);

  ctx = BN_CTX_new();
  pm1_bn = BN_new();
  div_bn = BN_new();
  mod_bn = BN_new();
  x_bn = BN_new();

  if (!ctx
      || !pm1_bn
      || !div_bn
      || !mod_bn
      || !x_bn) {
    goto fail;
  }

  // pm1 = p - 1
  if (!BN_sub(pm1_bn, p, BN_value_one()))
    goto fail;

  // [div, mod] = divmod(pm1, q)
  if (!BN_div(div_bn, mod_bn, pm1_bn, q, ctx))
    goto fail;

  // mod != 0
  if (!BN_is_zero(mod_bn))
    goto fail;

  // x = modpow(g, div, p)
  if (!BN_mod_exp(x_bn, g, div_bn, p, ctx))
    goto fail;

  // x == 1
  if (BN_is_one(x_bn))
    goto fail;

  DSA_free(params_d);
  BN_CTX_free(ctx);
  BN_free(pm1_bn);
  BN_free(div_bn);
  BN_free(mod_bn);
  BN_free(x_bn);

  return true;

fail:
  if (params_d)
    DSA_free(params_d);

  if (ctx)
    BN_CTX_free(ctx);

  if (pm1_bn)
    BN_free(pm1_bn);

  if (div_bn)
    BN_free(div_bn);

  if (mod_bn)
    BN_free(mod_bn);

  if (x_bn)
    BN_free(x_bn);

  return false;
}

bcrypto_dsa_key_t *
bcrypto_dsa_privkey_create(bcrypto_dsa_key_t *params) {
  DSA *priv_d = NULL;

  if (!bcrypto_dsa_sane_params(params))
    return NULL;

  priv_d = bcrypto_dsa_key2params(params);

  if (!priv_d)
    goto fail;

  bcrypto_poll();

  if (!DSA_generate_key(priv_d))
    goto fail;

  bcrypto_dsa_key_t *priv = bcrypto_dsa_priv2key(priv_d);

  if (!priv)
    goto fail;

  DSA_free(priv_d);

  return priv;

fail:
  if (priv_d)
    DSA_free(priv_d);

  return NULL;
}

bool
bcrypto_dsa_privkey_compute(
  bcrypto_dsa_key_t *priv,
  uint8_t **out,
  size_t *out_len
) {
  assert(out && out_len);

  BN_CTX *ctx = NULL;
  BIGNUM *p_bn = NULL;
  BIGNUM *g_bn = NULL;
  BIGNUM *y_bn = NULL;
  BIGNUM *x_bn = NULL;
  BIGNUM *prk_bn = NULL;
  size_t y_len;
  uint8_t *y_buf = NULL;

  if (!bcrypto_dsa_sane_compute(priv))
    goto fail;

  if (!bcrypto_dsa_needs_compute(priv)) {
    *out = NULL;
    out_len = 0;
    return true;
  }

  ctx = BN_CTX_new();
  p_bn = BN_bin2bn(priv->pd, priv->pl, NULL);
  g_bn = BN_bin2bn(priv->gd, priv->gl, NULL);
  y_bn = BN_new();
  x_bn = BN_bin2bn(priv->xd, priv->xl, NULL);
  prk_bn = BN_new();

  if (!ctx || !p_bn || !g_bn || !y_bn || !x_bn || !prk_bn)
    goto fail;

  BN_with_flags(prk_bn, x_bn, BN_FLG_CONSTTIME);

  if (!BN_mod_exp(y_bn, g_bn, prk_bn, p_bn, ctx))
    goto fail;

  y_len = BN_num_bytes(y_bn);
  y_buf = malloc(y_len);

  if (!y_buf)
    goto fail;

  assert(BN_bn2bin(y_bn, y_buf) != -1);

  BN_CTX_free(ctx);
  BN_free(p_bn);
  BN_free(g_bn);
  BN_free(y_bn);
  BN_free(x_bn);
  BN_free(prk_bn);

  *out = y_buf;
  *out_len = y_len;

  return true;

fail:
  if (ctx)
    BN_CTX_free(ctx);

  if (p_bn)
    BN_free(p_bn);

  if (g_bn)
    BN_free(g_bn);

  if (y_bn)
    BN_free(y_bn);

  if (x_bn)
    BN_free(x_bn);

  if (prk_bn)
    BN_free(prk_bn);

  return false;
}

bool
bcrypto_dsa_privkey_verify(bcrypto_dsa_key_t *key) {
  DSA *priv_d = NULL;
  BN_CTX *ctx = NULL;
  BIGNUM *y_bn = NULL;

  if (!bcrypto_dsa_sane_privkey(key))
    goto fail;

  if (!bcrypto_dsa_pubkey_verify(key))
    goto fail;

  priv_d = bcrypto_dsa_key2priv(key);

  if (!priv_d)
    goto fail;

  const BIGNUM *p = NULL;
  const BIGNUM *q = NULL;
  const BIGNUM *g = NULL;
  const BIGNUM *x = NULL;
  const BIGNUM *y = NULL;

  DSA_get0_pqg(priv_d, &p, &q, &g);
  assert(p && g);

  DSA_get0_key(priv_d, &y, &x);
  assert(y && x);

  // x >= y
  if (BN_ucmp(x, y) >= 0)
    goto fail;

  ctx = BN_CTX_new();
  y_bn = BN_new();

  if (!ctx || !y_bn)
    goto fail;

  // y = modpow(g, x, p)
  if (!BN_mod_exp(y_bn, g, x, p, ctx))
    goto fail;

  // y2 == y1
  if (BN_ucmp(y_bn, y) != 0)
    goto fail;

  DSA_free(priv_d);
  BN_CTX_free(ctx);
  BN_free(y_bn);

  return true;

fail:
  if (priv_d)
    DSA_free(priv_d);

  if (ctx)
    BN_CTX_free(ctx);

  if (y_bn)
    BN_free(y_bn);

  return false;
}

bool
bcrypto_dsa_privkey_export(
  const bcrypto_dsa_key_t *priv,
  uint8_t **out,
  size_t *out_len
) {
  assert(out && out_len);

  if (!bcrypto_dsa_sane_privkey(priv))
    return false;

  DSA *priv_d = bcrypto_dsa_key2priv(priv);

  if (!priv_d)
    return false;

  uint8_t *buf = NULL;
  int len = i2d_DSAPrivateKey(priv_d, &buf);

  DSA_free(priv_d);

  if (len <= 0)
    return false;

  *out = buf;
  *out_len = (size_t)len;

  return true;
}

bcrypto_dsa_key_t *
bcrypto_dsa_privkey_import(
  const uint8_t *raw,
  size_t raw_len
) {
  DSA *priv_d = NULL;
  const uint8_t *p = raw;

  if (!d2i_DSAPrivateKey(&priv_d, &p, raw_len))
    return false;

  bcrypto_dsa_key_t *k = bcrypto_dsa_priv2key(priv_d);

  DSA_free(priv_d);

  return k;
}

bool
bcrypto_dsa_pubkey_verify(bcrypto_dsa_key_t *key) {
  if (!bcrypto_dsa_params_verify(key))
    return false;

  return bcrypto_dsa_sane_pubkey(key);
}

bool
bcrypto_dsa_pubkey_export(
  const bcrypto_dsa_key_t *pub,
  uint8_t **out,
  size_t *out_len
) {
  assert(out && out_len);

  if (!bcrypto_dsa_sane_pubkey(pub))
    return false;

  DSA *pub_d = bcrypto_dsa_key2pub(pub);

  if (!pub_d)
    return false;

  uint8_t *buf = NULL;
  int len = i2d_DSAPublicKey(pub_d, &buf);

  DSA_free(pub_d);

  if (len <= 0)
    return false;

  *out = buf;
  *out_len = (size_t)len;

  return true;
}

bcrypto_dsa_key_t *
bcrypto_dsa_pubkey_import(
  const uint8_t *raw,
  size_t raw_len
) {
  DSA *pub_d = NULL;
  const uint8_t *p = raw;

  if (!d2i_DSAPublicKey(&pub_d, &p, raw_len))
    return false;

  bcrypto_dsa_key_t *k = bcrypto_dsa_pub2key(pub_d);

  DSA_free(pub_d);

  return k;
}

bool
bcrypto_dsa_sign(
  const uint8_t *msg,
  size_t msg_len,
  const bcrypto_dsa_key_t *priv,
  uint8_t **r,
  size_t *r_len,
  uint8_t **s,
  size_t *s_len
) {
  assert(r && r_len && s && s_len);

  DSA *priv_d = NULL;
  DSA_SIG *sig_d = NULL;

  if (msg == NULL || msg_len < 1 || msg_len > 64)
    goto fail;

  if (!bcrypto_dsa_sane_privkey(priv))
    goto fail;

  priv_d = bcrypto_dsa_key2priv(priv);

  if (!priv_d)
    goto fail;

  bcrypto_poll();

  sig_d = DSA_do_sign(msg, msg_len, priv_d);

  if (!sig_d)
    goto fail;

  if (!bcrypto_dsa_sig2rs(priv_d, sig_d, r, s))
    goto fail;

  const BIGNUM *q_bn = NULL;

  DSA_get0_pqg(priv_d, NULL, &q_bn, NULL);
  assert(q_bn);

  int bits = BN_num_bits(q_bn);
  size_t size = (bits + 7) / 8;

  *r_len = size;
  *s_len = size;

  DSA_free(priv_d);
  DSA_SIG_free(sig_d);

  return true;

fail:
  if (priv_d)
    DSA_free(priv_d);

  if (sig_d)
    DSA_SIG_free(sig_d);

  return false;
}

bool
bcrypto_dsa_verify(
  const uint8_t *msg,
  size_t msg_len,
  const uint8_t *r,
  size_t r_len,
  const uint8_t *s,
  size_t s_len,
  const bcrypto_dsa_key_t *pub
) {
  size_t qsize = 0;
  DSA *pub_d = NULL;
  DSA_SIG *sig_d = NULL;

  if (msg == NULL || msg_len < 1 || msg_len > 64)
    goto fail;

  qsize = bcrypto_dsa_subprime_size(pub);

  if (r == NULL || r_len != qsize)
    goto fail;

  if (s == NULL || s_len != qsize)
    goto fail;

  if (!bcrypto_dsa_sane_pubkey(pub))
    goto fail;

  pub_d = bcrypto_dsa_key2pub(pub);

  if (!pub_d)
    goto fail;

  sig_d = bcrypto_dsa_rs2sig(r, r_len, s, s_len);

  if (!sig_d)
    goto fail;

  if (DSA_do_verify(msg, msg_len, sig_d, pub_d) <= 0)
    goto fail;

  DSA_free(pub_d);
  DSA_SIG_free(sig_d);

  return true;

fail:
  if (pub_d)
    DSA_free(pub_d);

  if (sig_d)
    DSA_SIG_free(sig_d);

  return false;
}

#else

void
bcrypto_dsa_key_init(bcrypto_dsa_key_t *key) {}

void
bcrypto_dsa_key_free(bcrypto_dsa_key_t *key) {}

bcrypto_dsa_key_t *
bcrypto_dsa_params_generate(int bits) {
  return NULL;
}

bool
bcrypto_dsa_params_verify(bcrypto_dsa_key_t *params) {
  return false;
}

bcrypto_dsa_key_t *
bcrypto_dsa_privkey_create(bcrypto_dsa_key_t *params) {
  return NULL;
}

bool
bcrypto_dsa_privkey_compute(
  bcrypto_dsa_key_t *priv,
  uint8_t **out,
  size_t *out_len
) {
  return false;
}

bool
bcrypto_dsa_privkey_verify(bcrypto_dsa_key_t *key) {
  return false;
}

bool
bcrypto_dsa_privkey_export(
  const bcrypto_dsa_key_t *priv,
  uint8_t **out,
  size_t *out_len
) {
  return false;
}

bcrypto_dsa_key_t *
bcrypto_dsa_privkey_import(
  const uint8_t *raw,
  size_t raw_len
) {
  return NULL;
}

bool
bcrypto_dsa_pubkey_verify(bcrypto_dsa_key_t *key) {
  return false;
}

bool
bcrypto_dsa_pubkey_export(
  const bcrypto_dsa_key_t *pub,
  uint8_t **out,
  size_t *out_len
) {
  return false;
}

bcrypto_dsa_key_t *
bcrypto_dsa_pubkey_import(
  const uint8_t *raw,
  size_t raw_len
) {
  return NULL;
}

bool
bcrypto_dsa_sign(
  const uint8_t *msg,
  size_t msg_len,
  const bcrypto_dsa_key_t *priv,
  uint8_t **r,
  size_t *r_len,
  uint8_t **s,
  size_t *s_len
) {
  return false;
}

bool
bcrypto_dsa_verify(
  const uint8_t *msg,
  size_t msg_len,
  const uint8_t *r,
  size_t r_len,
  const uint8_t *s,
  size_t s_len,
  const bcrypto_dsa_key_t *pub
) {
  return false;
}

#endif
