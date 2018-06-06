#ifndef _BCRYPTO_SHA384_H
#define _BCRYPTO_SHA384_H
#include <node.h>
#include <nan.h>
#include "openssl/sha.h"

class SHA384 : public Nan::ObjectWrap {
public:
  static NAN_METHOD(New);
  static void Init(v8::Local<v8::Object> &target);

  SHA384();
  ~SHA384();

  SHA512_CTX ctx;

private:
  static NAN_METHOD(Init);
  static NAN_METHOD(Update);
  static NAN_METHOD(Final);
  static NAN_METHOD(Digest);
  static NAN_METHOD(Root);
  static NAN_METHOD(Multi);
};
#endif