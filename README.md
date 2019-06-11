NJS API
=======

Native JavaScript API for Bindings.

  * [Official Repository (njs-api/njs-api)](https://github.com/njs-api/njs-api)
  * [Public Domain](./LICENSE.md)

Introduction
------------

NJS-API is a header-only C++ library that abstracts APIs of JavaScript virtual machines (VMs) into a single API that can be used by embedders to write bindings compatible with multiple JS engines. The motivation to start the project were breaking changes of V8 engine and the way V8 bindings are written (too verbose and too annoying to handle all possible error conditions). In addition, attempt to port node.js to use Chakra javascript engine instead of V8 contributed to the idea of creating a single API that can target multiple JS engines, not just V8.

Features
--------

  - Source-level abstraction layer on top of native VM APIs - JavaScript engine cannot be replaced at runtime, recompilation is needed as NJS wraps VM dependent data structures and classes
  - High-performance interface that maps to native VMs as closely as hand-written bindings - the compiled code should be very close to a code you would have normally written for a particular VM
  - High-level interface to decrease the amount of code you need to write and to decrease the size of the resulting compiled code - no more boilerplate for each function you want to bind
  - Security is part of the contract - JavaScript code is not allowed to corrupt internals that the C++ side expects (function signatures, ...)
  - Comfortable and meaningful error handling - let users of your bindings know where the problem happened. NJS contains many helpers to make error handling less verbose yet still powerful on C++ side (it turns the most annoying type checks to one-liners)
  - Foundation to build extensions that can be used to implement concepts - data serialization and deserialization of custom user types

Disclaimer
----------

NJS is a WORK-IN-PROGRESS that has been published to validate the initial ideas and to design the API with more people. The library started as a separate project when developing [blend2d-js](https://github.com/blend2d/blend2d-js) to make bindings easier to write, easier to maintain, and also more secure.

Base API
--------

Classes provided by NJS-API:

  * `njs::Result` - 32-bit unsigned integer representing result-code
  * `njs::Utf8Ref` - Tagged reference to UTF-8  string
  * `njs::Utf16Ref` - Tagged reference to UTF-16 string
  * `njs::Latin1Ref` - Tagged reference to LATIN1 string
  * `njs::Range<T>` - Minimum and maximum value, that can be used to restrict JS to C++ value conversion
  * `njs::Value` - Local javascript value (wraps `v8::Local<v8::Value>`)
  * `njs::Persistent` - A persistent javascript value (wraps `v8::Persistent<v8::Value>`)
  * `njs::HandleScope` - Handle scope, currently maps to `v8::HandleScope`
  * `njs::Runtime` - Runtime is mapped to the underlying VM runtime / heap
  * `njs::Context` - Javascript context
    * `njs::ScopedContext` - Composition of `Context` and `HandleScope`
    * `njs::ExecutionContext` - Javascript context constructed during execution
    * `njs::FunctionCallContext` - Function call context
    * `njs::ConstructCallContext` - Constructor call context
    * `njs::GetPropertyContext` - Property getter context
    * `njs::SetPropertyContext` - Property setter context

Namespaces provided by NJS-API:

  * `njs::Globals` - Provides constants and limits
  * `njs::Internal` - Private namespace used by the implementation

Minimum Example
---------------

```c++
// A struct (or class) we want to wrap.
struct Point {
  double x, y;

  bool operator==(const Point& other) const noexcept { return x == other.x && y == other.y; }
  bool operator!=(const Point& other) const noexcept { return x == other.x && y == other.y; }

  Point& translate(double tx, double ty) noexcept {
    x += tx;
    y += ty;
    return *this;
  }

  static Point vectorOf(const Point& a, const Point& b) noexcept {
    return Point { b.x - a.x, b.y - a.y };
  }
};

// Wrapped object.
//
// This can actually be in header if you plan to share the interface
// with other objects or want to provide it for other libraries.
class PointWrap {
public:
  // Adds some data into `PointWrap` that will be used and implemented
  // in the next step. The last argument is signature, which should be
  // unique.
  NJS_BASE_CLASS(PointWrap, "Point", 0x01)

  // Native data.
  Point data;

  // You should always provide constructors that should match JS side.
  inline PointWrap() noexcept
    : data {} {}
  inline PointWrap(double x, double y) noexcept
    : data { x, y} {}
};

// Wrapped object bindings - should be always in `.cpp` file.
NJS_BIND_CLASS(PointWrap) {
  // Every function has an implicit context - use it to access stuff.
  NJS_BIND_CONSTURCTOR() {
    unsigned int argc = ctx.argumentsLength();
    if (argc == 0) {
      return ctx.returnNew<PointWrap>();
    }

    if (argc == 2) {
      // Unpack arguments from the calling context
      double x, y;

      // NJS_CHECK returns error immediately if something fails.
      NJS_CHECK(ctx.unpackArgument(0, x);
      NJS_CHECK(ctx.unpackArgument(1, y);

      return ctx.returnNew<PointWrap>(x, y);
    }

    return ctx.invalidArgumentsLength();
  }

  // Bindings methods, getters, and setters is easy. The bound method has
  // already setup CallContext and can access `self` that is `PointerWrap*`.
  // Unpacking the pointer to `PointWrap*` is done automatically by NJS-API.

  // If you plan to bind both getters and setters of the same property they
  // must be next to each other (a limitation that maybe we can relax in the
  // future).
  NJS_BIND_GET(x) {
    return ctx.returnValue(self->data.x);
  }

  NJS_BIND_SET(x) {
    double x;
    NJS_CHECK(ctx.unpackValue(x));
    self->data.x = x;

    // Setters return no value, you must use njs::ResultCode.
    return njs::Globals::kResultOk;
  }

  NJS_BIND_GET(y) {
    return ctx.returnValue(self->data.y);
  }

  NJS_BIND_SET(y) {
    double y;
    NJS_CHECK(ctx.unpackValue(y));
    self->data.y = y;
    return njs::Globals::kResultOk;
  }

  // You can bind methods too...
  NJS_BIND_METHOD(translate) {
    double tx, ty;

    NJS_CHECK(ctx.verifyArgumentsLength(2));
    NJS_CHECK(ctx.unpackArgument(0, tx));
    NJS_CHECK(ctx.unpackArgument(1, ty));

    // Perform the operation on native data.
    self->data.translate(tx, ty);

    // Return `this`.
    return ctx.returnValue(ctx.This());
  }

  // This shows how to unpack a wrapped object.
  NJS_BIND_METHOD(equals) {
    PointWrap* other;

    NJS_CHECK(ctx.verifyArgumentsLength(1));
    NJS_CHECK(ctx.unwrapArgument<PointWrap>(0, &other));

    return ctx.returnValue(self->data == other->data);
  }

  // Static functions can be bound too.
  NJS_BIND_STATIC(vectorOf) {
    PointWrap* a;
    PointWrap* b;

    NJS_CHECK(ctx.verifyArgumentsLength(2));
    NJS_CHECK(ctx.unwrapArgument<PointWrap>(0, &a));
    NJS_CHECK(ctx.unwrapArgument<PointWrap>(1, &b));

    // There is no reference to the class in the call context, however,
    // NJS-API wrapper always adds the module into the function data field.
    njs::Value PointClass = ctx.propertyOf(ctx.data(), njs::Latin1Ref("Point"));
    NJS_CHECK(PointClass);

    njs::Value instance = ctx.newInstance(PointClass);
    NJS_CHECK(instance);

    PointWrap* instObj = ctx.unwrapUnsafe<PointWrap>(instance);
    instObj->data = Point::vectorOf(a->data, b->data);
    return instance;
  }
};

// This would create a module that is context-aware by default.
NJS_MODULE(mylib) {
  // Each class must be added to `exports`.
  NJS_INIT_CLASS(PointWrap, exports);
}
```

TODO
----

Write more documentation and examples.
