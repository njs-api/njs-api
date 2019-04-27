// [NJS-API]
// Native JavaScript API for Bindings.
//
// [License]
// Public Domain <http://unlicense.org>

#ifndef NJS_TEST_P_H
#define NJS_TEST_P_H

#include <stdio.h>
#include "../njs-api.h"

namespace test {

// ============================================================================
// [test::Object]
// ============================================================================

// Some object to be wrapped.
class Object {
public:
  NJS_INLINE Object(int a, int b) noexcept
    : _a(a),
      _b(b) {
    printf("  [C++] test::Object::Object(%d, %d)\n", a, b);
  }

  NJS_INLINE ~Object() noexcept {
    printf("  [C++] test::Object::~Object()\n");
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_INLINE int a() const noexcept {
    printf("  [C++] test::Object::a() -> %d\n", _a);
    return _a;
  }

  NJS_INLINE int b() const noexcept {
    printf("  [C++] test::Object::b() -> %d\n", _b);
    return _b;
  }

  NJS_INLINE void setA(int a) noexcept {
    printf("  [C++] test::Object::setA(%d)\n", a);
    _a = a;
  }

  // --------------------------------------------------------------------------
  // [Methods]
  // --------------------------------------------------------------------------

  NJS_INLINE void add(int n) noexcept {
    printf("  [C++] test::Object::add(%d)\n", n);
    _a += n;
    _b += n;
  }

  NJS_INLINE bool equals(const Object& other) noexcept {
    return _a == other._a && _b == other._b;
  }

  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  static NJS_INLINE int staticMul(int a, int b) noexcept {
    printf("  [C++] test::Object::staticMul(%d, %d)\n", a, b);
    return a * b;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  int _a;
  int _b;
};

// A wrapped class.
class ObjectWrap {
public:
  NJS_BASE_CLASS(ObjectWrap, "Object", 0xFF)

  NJS_INLINE ObjectWrap(int a, int b) noexcept
    : _obj(a, b) {}
  NJS_INLINE ~ObjectWrap() noexcept {}

  Object _obj;
};

} // {test}

#endif // NJS_TEST_P_H
