// [NJS]
// Neutral JS interface.
//
// [License]
// Public Domain <http://unlicense.org>

// [Dependencies]
#include "./test_p.h"
#include <new>

namespace njstest {

// ============================================================================
// [njstest::ObjectWrap]
// ============================================================================

NJS_BIND_CLASS(ObjectWrap) {
  NJS_BIND_CONSTRUCTOR() {
    NJS_CHECK(ctx.VerifyArgumentsCount(2));

    // Get `a` and `b` arguments.
    int a, b;
    NJS_CHECK(ctx.UnpackArgument(0, a));
    NJS_CHECK(ctx.UnpackArgument(1, b));

    // Wrap.
    ObjectWrap* wrap = new(std::nothrow) ObjectWrap(a, b);
    NJS_CHECK(wrap);

    ctx.Wrap(ctx.This(), wrap);
    return ctx.Return(ctx.This());
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_BIND_GET(a) {
    return ctx.Return(self->_obj.getA());
  }

  NJS_BIND_SET(a) {
    int a;
    NJS_CHECK(ctx.UnpackValue(a));
    self->_obj.setA(a);
    return njs::kResultOk;
  }

  NJS_BIND_GET(b) {
    return ctx.Return(self->_obj.getB());
  }

  // --------------------------------------------------------------------------
  // [Methods]
  // --------------------------------------------------------------------------

  NJS_BIND_METHOD(add) {
    NJS_CHECK(ctx.VerifyArgumentsCount(1));

    int n;
    NJS_CHECK(ctx.UnpackArgument(0, n));
    self->_obj.add(n);

    return ctx.Return(ctx.This());
  }

  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  NJS_BIND_STATIC(staticMul) {
    NJS_CHECK(ctx.VerifyArgumentsCount(2));

    int a, b;
    NJS_CHECK(ctx.UnpackArgument(0, a));
    NJS_CHECK(ctx.UnpackArgument(1, b));

    return ctx.Return(Object::staticMul(a, b));
  }
};

NJS_MODULE(test) {
  // TODO: This depends on V8.
  typedef v8::Local<v8::FunctionTemplate> FunctionSpec;
  FunctionSpec ObjectSpec = NJS_INIT_CLASS(ObjectWrap, exports);
}

} // njstest namespace
