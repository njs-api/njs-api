// [NJS]
// Neutral JS interface.
//
// [License]
// Public Domain <http://unlicense.org>

// [Dependencies]
#include "./test_p.h"

namespace test {

// ============================================================================
// [test::NJSObject]
// ============================================================================

NJS_BIND_CLASS(NJSObject) {
  NJS_BIND_CONSTRUCTOR() {
    NJS_CHECK(ctx.verifyArgumentsLength(2));

    // Unpack `a` and `b` arguments.
    int a, b;
    NJS_CHECK(ctx.unpackArgument(0, a));
    NJS_CHECK(ctx.unpackArgument(1, b));

    return ctx.returnNew<NJSObject>(a, b);
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_BIND_GET(a) {
    return ctx.returnValue(self->_obj.a());
  }

  NJS_BIND_SET(a) {
    int a;
    NJS_CHECK(ctx.unpackValue(a));
    self->_obj.setA(a);
    return njs::Globals::kResultOk;
  }

  NJS_BIND_GET(b) {
    return ctx.returnValue(self->_obj.b());
  }

  // --------------------------------------------------------------------------
  // [Methods]
  // --------------------------------------------------------------------------

  NJS_BIND_METHOD(add) {
    NJS_CHECK(ctx.verifyArgumentsLength(1));

    int n;
    NJS_CHECK(ctx.unpackArgument(0, n));
    self->_obj.add(n);

    return ctx.returnValue(ctx.This());
  }

  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  NJS_BIND_STATIC(staticMul) {
    NJS_CHECK(ctx.verifyArgumentsLength(2));

    int a, b;
    NJS_CHECK(ctx.unpackArgument(0, a));
    NJS_CHECK(ctx.unpackArgument(1, b));

    return ctx.returnValue(Object::staticMul(a, b));
  }
};

NJS_MODULE(test) {
  // TODO: This depends on V8.
  typedef v8::Local<v8::FunctionTemplate> FunctionSpec;
  FunctionSpec ObjectSpec = NJS_INIT_CLASS(NJSObject, exports);
}

} // test namespace
