// [NJS]
// Neutral JS interface.
//
// [License]
// Public Domain <http://unlicense.org>

// [Guard]
#ifndef _NJS_TEST_P_H
#define _NJS_TEST_P_H

// [Dependencies]
#include <stdio.h>
#include "../njs.h"

namespace njstest {

// ============================================================================
// [njstest::Object]
// ============================================================================

// Some object to be wrapped.
struct Object {
  NJS_INLINE Object(int a, int b) NJS_NOEXCEPT
    : _a(a),
      _b(b) {
    printf("njstest::Object::Object(%d, %d)\n", _a, _b);
  }

  NJS_INLINE ~Object() NJS_NOEXCEPT {
    printf("njstest::Object::~Object()\n");
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_INLINE int getA() const NJS_NOEXCEPT {
    printf("njstest::Object::getA() -> %d\n", _a);
    return _a;
  }

  NJS_INLINE int getB() const NJS_NOEXCEPT {
    printf("njstest::Object::getB() -> %d\n", _b);
    return _b;
  }

  NJS_INLINE void setA(int a) NJS_NOEXCEPT {
    printf("njstest::Object::setA(%d)\n", a);
    _a = a;
  }

  // --------------------------------------------------------------------------
  // [Methods]
  // --------------------------------------------------------------------------

  NJS_INLINE void add(int n) NJS_NOEXCEPT {
    printf("njstest::Object::add(%d)\n", n);
    _a += n;
    _b += n;
  }

  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  static NJS_INLINE int staticMul(int a, int b) NJS_NOEXCEPT {
    printf("njstest::Object::staticMul(%d, %d)\n", a, b);
    return a * b;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  int _a;
  int _b;
};

// ============================================================================
// [njstest::ObjectWrap]
// ============================================================================

// A wrapped class.
struct ObjectWrap {
  NJS_BASE_CLASS(ObjectWrap, "Object")

  NJS_INLINE ObjectWrap(int a, int b) NJS_NOEXCEPT
    : _obj(a, b) {}
  NJS_INLINE ~ObjectWrap() NJS_NOEXCEPT {}

  Object _obj;
};

} // njstest namespace

// [Guard]
#endif // _NJS_TEST_P_H
