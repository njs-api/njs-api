// [NJS-API]
// Native JavaScript API for Bindings.
//
// [License]
// Public Domain <http://unlicense.org>

#ifndef NJS_BASE_H
#define NJS_BASE_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

// ============================================================================
// [njs::Defines]
// ============================================================================

#define NJS_VERSION 0x010000

#if defined(_MSC_VER)
# define NJS_INLINE __forceinline
# define NJS_NOINLINE __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
# define NJS_INLINE inline __attribute__((__always_inline__))
# define NJS_NOINLINE __attribute__((__noinline__))
#else
# define NJS_INLINE inline
# define NJS_NOINLINE
#endif

#if !defined(NJS_ASSERT)
# include <assert.h>
# define NJS_ASSERT assert
#endif // !NJS_ASSERT

// Miscellaneous macros used by NJS.
#define NJS_NONCOPYABLE(SELF)                                                 \
private:                                                                      \
  NJS_INLINE SELF(const SELF&) noexcept = delete;                             \
  NJS_INLINE SELF& operator=(const SELF&) noexcept = delete;                  \
public:

// ============================================================================
// [njs::]
// ============================================================================

namespace njs {

// ============================================================================
// [njs::Forward]
// ============================================================================

// --- Provided here ---

//! A tagged string reference that specifies LATIN-1 encoding.
class Latin1Ref;

//! A tagged string reference that specifies UTF-8 encoding.
class Utf8Ref;

//! A tagged string reference that specifies UTF-16 encoding.
class Utf16Ref;

// --- Provided by the VM engine ---

//! Runtime is mapped to the underlying VM runtime / heap. Each runtime can have
//! multiple separate contexts.
class Runtime;

//! Handle scope if VM is using that concept, currently maps to `v8::HandleScope`.
class HandleScope;

//! Basic context for interacting with VM. Holds all necessary variables to make
//! the interaction simple, but doesn't offer advanced features that require
//! memory management on C++ side. See `ExecutionContext` that provides an
//! additional functionality.
class Context;

//! Scoped context is a composition of `Context` and `HandleScope`. You should
//! prefer this type of context in case that you enter a call that was not
//! triggered by JS-VM. For example when an asynchronous task completes in node.js
//! and the `AfterWork` handler is called by libuv, it's important to create a
//! `ScopedContext` instead of just `Context`, because no handle scope exist at
//! that point.
class ScopedContext;

class ExecutionContext;
class GetPropertyContext;
class SetPropertyContext;
class FunctionCallContext;
class ConstructCallContext;

//! Local javascript value.
class Value;

//! Persistent javascript value.
class Persistent;

//! Result value - just a mark to indicate all APIs that returns `ResultCode`.
typedef unsigned int Result;

//! Contains additional information related to `Result`.
struct ResultPayload;

// ============================================================================
// [njs::Globals]
// ============================================================================

namespace Globals {

//! NJS limits.
enum Limits : uint32_t {
  // NOTE: Other limits depend on VM. They should be established at the top of
  // the VM implementation file.

  //! Maximum size of a single enumeration (string).
  kMaxEnumSize = 64,
  //! Maximum size of a buffer used internally to format messages (errors).
  kMaxBufferSize = 256
};

//! Type-trait ids.
enum TraitId : uint32_t {
  kTraitIdUnknown    = 0,
  kTraitIdBool       = 1,
  kTraitIdSafeInt    = 2,
  kTraitIdSafeUint   = 3,
  kTraitIdUnsafeInt  = 4,
  kTraitIdUnsafeUint = 5,
  kTraitIdFloat      = 6,
  kTraitIdDouble     = 7,
  kTraitIdStrRef     = 8
};

// Value type (VM independent).
enum ValueType : uint32_t {
  kValueNone = 0,

  kValueBool,
  kValueInt32,
  kValueUint32,
  kValueDouble,
  kValueString,
  kValueSymbol,
  kValueArray,
  kValueObject,
  kValueFunction,

  kValueDate,
  kValueError,
  kValueRegExp,

  kValueGeneratorFunction,
  kValueGeneratorObject,
  kValuePromise,

  kValueMap,
  kValueMapIterator,
  kValueSet,
  kValueSetIterator,

  kValueWeakMap,
  kValueWeakSet,

  kValueArrayBuffer,
  kValueArrayBufferView,
  kValueDataView,

  kValueInt8Array,
  kValueUint8Array,
  kValueUint8ClampedArray,
  kValueInt16Array,
  kValueUint16Array,
  kValueInt32Array,
  kValueUint32Array,
  kValueFloat32Array,
  kValueFloat64Array,

  // NJS specific.
  kValueNJSEnum,     // njs::Enum.
  kValueNodeBuffer,  // node::Buffer.

  kValueCount
};

// Exception type (VM independent).
enum ExceptionType : uint32_t {
  kExceptionNone = 0,

  kExceptionError = 1,
  kExceptionTypeError = 2,
  kExceptionRangeError = 3,
  kExceptionSyntaxError = 4,
  kExceptionReferenceError = 5
};

// ============================================================================
// [njs::ConceptType]
// ============================================================================

enum ConceptType : uint32_t {
  kConceptSerializer = 0,
  kConceptValidator = 1
};

// ============================================================================
// [njs::Result]
// ============================================================================

// VM neutral result code.
enum ResultCode : uint32_t {
  kResultOk = 0,

  // NJS errors mapped to JavaScript's native error objects.
  kResultThrowError = kExceptionError,
  kResultThrowTypeError = kExceptionTypeError,
  kResultThrowRangeError = kExceptionRangeError,
  kResultThrowSyntaxError = kExceptionSyntaxError,
  kResultThrowReferenceError = kExceptionReferenceError,

  kResultInvalidState = 10,
  kResultInvalidHandle,
  kResultOutOfMemory,

  kResultInvalidValue,
  kResultInvalidValueTypeId,
  kResultInvalidValueTypeName,
  kResultInvalidValueCustom,
  kResultInvalidValueRange,
  kResultUnsafeInt64Conversion,
  kResultUnsafeUint64Conversion,

  kResultInvalidArgumentsLength,

  kResultInvalidConstructCall,
  kResultAbstractConstructCall,

  // This is not an error, it's telling the error handler to not handle the
  // error as the caller thrown an exception or did something else and wants
  // to finish the current call-chain.
  kResultBypass,

  _kResultThrowFirst = kResultThrowError,
  _kResultThrowLast = kResultThrowReferenceError,

  _kResultValueFirst = kResultInvalidValue,
  _kResultValueLast = kResultUnsafeUint64Conversion
};

} // Globals namespace

// ============================================================================
// [njs::Internal::Pass]
// ============================================================================

namespace Internal {

template<typename T>
inline T& pass(T& arg) { return arg; }

} // {Internal}

// ============================================================================
// [njs::Internal::CastCheck]
// ============================================================================

namespace Internal {

// Won't compile if `Src` type can't be naturally casted to `Dst` type.
template<typename Dst, typename Src>
NJS_INLINE void checkNaturalCast(const Src* src) noexcept {
  const Dst* dst = src;
  (void)dst;
}

// Won't compile if `Src` type can't be statically casted to `Dst` type.
template<typename Dst, typename Src>
NJS_INLINE void checkStaticCast(const Src* src) noexcept {
  const Dst* dst = static_cast<const Src*>(src);
  (void)dst;
}

} // {Internal}

// ============================================================================
// [njs::Internal::TypeTraits]
// ============================================================================

// TODO:
// [ ] Missing wchar_t, char16_t, char32_t
// [ ] Any other types we should worry about?
namespace Internal {

template<typename T, int Id>
struct BaseTraits {
  enum {
    kTraitId     = Id,
    kIsPrimitive = Id >= Globals::kTraitIdBool     && Id <= Globals::kTraitIdDouble,
    kIsBool      = Id == Globals::kTraitIdBool,
    kIsInt       = Id >= Globals::kTraitIdSafeInt  && Id <= Globals::kTraitIdUnsafeUint,
    kIsSigned    = Id == Globals::kTraitIdSafeInt  || Id == Globals::kTraitIdUnsafeInt,
    kIsUnsigned  = Id == Globals::kTraitIdSafeUint || Id == Globals::kTraitIdUnsafeUint,
    kIsReal      = Id >= Globals::kTraitIdFloat    && Id <= Globals::kTraitIdDouble,
    kIsStrRef    = Id == Globals::kTraitIdStrRef
  };
};

template<typename T>
struct IsSignedIntType {
  enum { kValue = static_cast<T>(~static_cast<T>(0)) < static_cast<T>(0) };
};

// Type traits are used when converting values from C++ <-> Javascript.
template<typename T>
struct TypeTraits : public BaseTraits<T, Globals::kTraitIdUnknown> {};

template<> struct TypeTraits<bool     > : public BaseTraits<bool     , Globals::kTraitIdBool  > {};
template<> struct TypeTraits<float    > : public BaseTraits<float    , Globals::kTraitIdFloat > {};
template<> struct TypeTraits<double   > : public BaseTraits<double   , Globals::kTraitIdDouble> {};

template<> struct TypeTraits<Latin1Ref> : public BaseTraits<Latin1Ref, Globals::kTraitIdStrRef> {};
template<> struct TypeTraits<Utf8Ref  > : public BaseTraits<Utf8Ref  , Globals::kTraitIdStrRef> {};
template<> struct TypeTraits<Utf16Ref > : public BaseTraits<Utf16Ref , Globals::kTraitIdStrRef> {};

#define NJS_INT_TRAITS(T)                                                       \
template<>                                                                      \
struct TypeTraits<T> : public BaseTraits<T, IsSignedIntType<T>::kValue          \
  ? (sizeof(T) < 8 ? Globals::kTraitIdSafeInt  : Globals::kTraitIdUnsafeInt )   \
  : (sizeof(T) < 8 ? Globals::kTraitIdSafeUint : Globals::kTraitIdUnsafeUint) > \
{                                                                               \
  static NJS_INLINE T minValue() noexcept { return std::numeric_limits<T>::lowest(); } \
  static NJS_INLINE T maxValue() noexcept { return std::numeric_limits<T>::max(); } \
}

NJS_INT_TRAITS(char);
NJS_INT_TRAITS(signed char);
NJS_INT_TRAITS(unsigned char);
NJS_INT_TRAITS(short);
NJS_INT_TRAITS(unsigned short);
NJS_INT_TRAITS(int);
NJS_INT_TRAITS(unsigned int);
NJS_INT_TRAITS(long);
NJS_INT_TRAITS(unsigned long);
NJS_INT_TRAITS(long long);
NJS_INT_TRAITS(unsigned long long);
#undef NJS_INT_TRAITS

} // {Internal}

// ============================================================================
// [njs::IntUtils]
// ============================================================================

namespace IntUtils {

template<typename In, typename Out, int IsNarrowing, int IsSigned>
struct IntCastImpl {
  static NJS_INLINE Result op(In in, Out& out) noexcept {
    out = static_cast<Out>(in);
    return Globals::kResultOk;
  }
};

// Unsigned narrowing.
template<typename In, typename Out>
struct IntCastImpl<In, Out, 1, 0> {
  static NJS_INLINE Result op(In in, Out& out) noexcept {
    if (in > static_cast<In>(Internal::TypeTraits<Out>::maxValue()))
      return Globals::kResultInvalidValue;

    out = static_cast<Out>(in);
    return Globals::kResultOk;
  }
};

// Signed narrowing.
template<typename In, typename Out>
struct IntCastImpl<In, Out, 1, 1> {
  static NJS_INLINE Result op(In in, Out& out) noexcept {
    if (in < static_cast<In>(std::numeric_limits<Out>::lowest()) ||
        in > static_cast<In>(std::numeric_limits<Out>::max()) )
      return Globals::kResultInvalidValue;

    out = static_cast<Out>(in);
    return Globals::kResultOk;
  }
};

template<typename In, typename Out>
NJS_INLINE Result intCast(In in, Out& out) noexcept {
  return IntCastImpl<
    In, Out,
    (sizeof(In) > sizeof(Out)),
    Internal::TypeTraits<In>::kIsSigned && Internal::TypeTraits<Out>::kIsSigned>::op(in, out);
}

template<typename T, size_t Size, int IsSigned>
struct IsSafeIntImpl {
  static NJS_INLINE bool op(T x) noexcept {
    // [+] 32-bit and less are safe.
    // [+] 64-bit types are specialized.
    // [-] More than 64-bit types are not supported.
    return Size <= 4;
  }
};

template<typename T>
struct IsSafeIntImpl<T, 8, 0> {
  static NJS_INLINE bool op(T x) noexcept {
    return static_cast<uint64_t>(x) <= static_cast<uint64_t>(9007199254740991ull);
  }
};

template<typename T>
struct IsSafeIntImpl<T, 8, 1> {
  static NJS_INLINE bool op(T x) noexcept {
    return static_cast<int64_t>(x) >= -static_cast<int64_t>(9007199254740991ll) &&
           static_cast<int64_t>(x) <=  static_cast<int64_t>(9007199254740991ll) ;
  }
};

// Returns true if the given integer is representable in JS as it uses `double`
// externally to represent all types. This function is basically the same as
// JavaScript's `Number.isSafeInteger()`, which is equivalent to:
//
//   `x >= -Number.MAX_SAFE_INTEGER && x <= Number.MAX_SAFE_INTEGER`
//
// but accessible from C++. The MAX_SAFE_INTEGER value is equivalent to 2^53 - 1.
template<typename T>
NJS_INLINE bool isSafeInt(T x) noexcept {
  return IsSafeIntImpl<T, sizeof(T), Internal::TypeTraits<T>::kIsSigned>::op(x);
}

static NJS_INLINE Result doubleToInt64(double in, int64_t& out) noexcept {
  if (in >= -9007199254740991.0 && in <= 9007199254740991.0) {
    int64_t x = static_cast<int64_t>(in);
    if (static_cast<double>(x) == in) {
      out = x;
      return Globals::kResultOk;
    }
  }

  return Globals::kResultInvalidValue;
}

static NJS_INLINE Result doubleToUint64(double in, uint64_t& out) noexcept {
  if (in >= 0.0 && in <= 9007199254740991.0) {
    int64_t x = static_cast<int64_t>(in);
    if (static_cast<double>(x) == in) {
      out = static_cast<uint64_t>(x);
      return Globals::kResultOk;
    }
  }

  return Globals::kResultInvalidValue;
}

} // IntUtils namespace

// ============================================================================
// [njs::StrUtils]
// ============================================================================

namespace StrUtils {

// Inlining probably doesn't matter here, `vsnprintf` is complex and one more
// function call shouldn't make any difference, so don't inline.
static NJS_NOINLINE unsigned int vsformat(char* dst, size_t maxLength, const char* fmt, va_list ap) noexcept {
  int result = vsnprintf(dst, maxLength, fmt, ap);
  unsigned int size = static_cast<unsigned int>(result);

  if (result < 0)
    size = 0;
  else if (static_cast<unsigned int>(result) >= maxLength)
    size = maxLength - 1;

  dst[size] = 0;
  return size;
}

// Cannot be inlined, some compilers (like GCC) have problems with variable
// argument functions declared as __always_inline__.
//
// NOTE: The function has the same behavior as `vsformat()`.
static NJS_NOINLINE unsigned int sformat(char* dst, size_t maxLength, const char* fmt, ...) noexcept {
  va_list ap;
  va_start(ap, fmt);
  unsigned int result = vsformat(dst, maxLength, fmt, ap);
  va_end(ap);
  return result;
}

// Required mostly by `Utf16` string, generalized for any type.
template<typename CharType>
NJS_INLINE size_t slen(const CharType* str) noexcept {
  NJS_ASSERT(str != NULL);

  size_t i = 0;
  while (str[i] != CharType(0))
    i++;
  return i;
}
// Specialize a bit...
template<>
NJS_INLINE size_t slen(const char* str) noexcept { return strlen(str); }

} // StrUtils namespace

// ============================================================================
// [njs::StrRef]
// ============================================================================

//! Reference (const or mutable) to an array of characters representing a string.
//!
//! This is used as a base class that is inherited by:
//!   - `Latin1Ref`
//!   - `Utf8Ref`
//!   - `Utf16Ref`
//!
//! NOTE: This class is not designed to be a string container. It's rather used
//! as a wrapper of your own string that just provides string encoding. For
//! example both `Latin1Ref` and `Utf8Ref` can be used to create a string.
template<typename T>
class StrRef {
public:
  typedef T Type;

  NJS_INLINE StrRef() noexcept { reset(); }
  explicit NJS_INLINE StrRef(T* data) noexcept { init(data); }
  NJS_INLINE StrRef(T* data, size_t size) noexcept { init(data, size); }
  NJS_INLINE StrRef(const StrRef<T>& other) noexcept { init(other); }

  NJS_INLINE bool isInitialized() const noexcept {
    return _data != NULL;
  }

  NJS_INLINE void reset() noexcept {
    _data = NULL;
    _size = 0;
  }

  NJS_INLINE void init(T* data) noexcept {
    init(data, StrUtils::slen(data));
  }

  NJS_INLINE void init(T* data, size_t size) noexcept {
    _data = data;
    _size = size;
  }

  NJS_INLINE void init(const StrRef<T>& other) noexcept {
    _data = other._data;
    _size = other._size;
  }

  NJS_INLINE bool hasData() const noexcept { return _data != NULL; }
  NJS_INLINE T* data() const noexcept { return _data; }
  NJS_INLINE size_t size() const noexcept { return _size; }

  // ------------------------------------------------------------------------
  // [Members]
  // ------------------------------------------------------------------------

  T* _data;
  size_t _size;
};

// ============================================================================
// [njs::Latin1Ref]
// ============================================================================

class Latin1Ref : public StrRef<const char> {
public:
  explicit NJS_INLINE Latin1Ref(const char* data) noexcept
    : StrRef(data) {}

  NJS_INLINE Latin1Ref(const char* data, size_t size) noexcept
    : StrRef(data, size) {}

  NJS_INLINE Latin1Ref(const Latin1Ref& other) noexcept
    : StrRef(other) {}
};

// ============================================================================
// [njs::Utf8Ref]
// ============================================================================

class Utf8Ref : public StrRef<const char> {
public:
  explicit NJS_INLINE Utf8Ref(const char* data) noexcept
    : StrRef(data) {}

  NJS_INLINE Utf8Ref(const char* data, size_t size) noexcept
    : StrRef(data, size) {}

  NJS_INLINE Utf8Ref(const Utf8Ref& other) noexcept
    : StrRef(other) {}
};

// ============================================================================
// [njs::Utf16Ref]
// ============================================================================

class Utf16Ref : public StrRef<const uint16_t> {
public:
  explicit NJS_INLINE Utf16Ref(const uint16_t* data) noexcept
    : StrRef(data) {}

  NJS_INLINE Utf16Ref(const uint16_t* data, size_t size) noexcept
    : StrRef(data, size) {}

  NJS_INLINE Utf16Ref(const Utf16Ref& other) noexcept
    : StrRef(other) {}
};

// ============================================================================
// [njs::Range]
// ============================================================================

template<typename T>
class Range {
public:
  typedef T Type;

  enum { kConceptType = Globals::kConceptValidator };

  NJS_INLINE Range<T>(Type minValue, Type maxValue) noexcept
    : _minValue(minValue),
      _maxValue(maxValue) {}

  NJS_INLINE Result validate(const Type& value) const noexcept {
    if (value >= _minValue && value <= _maxValue)
      return Globals::kResultOk;
    else
      return Globals::kResultInvalidValueRange;
  }

  Type _minValue;
  Type _maxValue;
};

// ============================================================================
// [njs::BindingItem]
// ============================================================================

//! Information about a single binding item used to declaratively bind a C++
//! class into JavaScript. Each `BindingItem` contains information about a
//! getter|setter, static (class) function, or a member function.
struct BindingItem {
  enum Type : uint32_t {
    kTypeInvalid = 0,
    kTypeStatic = 1,
    kTypeMethod = 2,
    kTypeGetter = 3,
    kTypeSetter = 4
  };

  NJS_INLINE BindingItem(unsigned int type, unsigned int flags, const char* name, const void* data) noexcept
    : type(type),
      flags(flags),
      name(name),
      data(data) {}

  //! Type of the item.
  unsigned int type;
  //! Flags.
  unsigned int flags;
  //! Name (function name, property name, etc...).
  const char* name;
  //! Data (native function pointer).
  const void* data;
};

// ============================================================================
// [njs::StaticData]
// ============================================================================

//! Minimal read-only data that is used to improve and unify error handling.
//!
//! NOTE: Do not add useless members here, a copy will be embedded in every
//! executable and shared object that uses this data, so keep it lightweight.
struct StaticData {
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_INLINE const char* typeNameOf(int type) const noexcept {
    NJS_ASSERT(type >= 0 && type < Globals::kValueCount);
    return _typeName[type];
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  char _typeName[Globals::kValueCount][20];
};

static const StaticData _staticData = {
  // _typeName.
  {
    "?",

    "Boolean",
    "Int32",
    "Uint32",
    "Number",
    "String",
    "Symbol",
    "Array",
    "Object",
    "Function",

    "Date",
    "Error",
    "RegExp",

    "GeneratorFunction",
    "GeneratorObject",
    "Promise",

    "Map",
    "MapIterator",
    "Set",
    "SetIterator",

    "WeakMap",
    "WeakSet",

    "ArrayBuffer",
    "ArrayBufferView",
    "DataView",

    "Int8Array",
    "Uint8Array",
    "Uint8ClampedArray",
    "Int16Array",
    "Uint16Array",
    "Int32Array",
    "Uint32Array",
    "Float32Array",
    "Float64Array",

    "njs::Enum",
    "node::Buffer"
  }
};

// ============================================================================
// [njs::ResultPayload]
// ============================================================================

//! Result payload.
//!
//! This data structure holds extra payload associated with `Result` code that
//! can be used to produce a more meaningful message than "Invalid argument"
//! and similar. The purpose is to bring a minimal set of failure information
//! to users of native addons. It should help with API misuse and with finding
//! where the error happened and why.
//!
//! NOTE: Only first two members of this data is initialized (or reset) by
//! `reset()` method for performance reasons. Based on the error type and
//! content of these values other members can be used, but not without
//! checking `isInitialized()` first.
struct ResultPayload {
  // ------------------------------------------------------------------------
  // [Reset]
  // ------------------------------------------------------------------------

  //! Get if this `ResultPayload` data is initialized.
  NJS_INLINE bool isInitialized() const noexcept { return any.first != -1; }

  //! Has to be called to reset the content of payload. It resets only one
  //! member for efficiency. If this member is set then the payload is valid,
  //! otherwise it's invalid or not set.
  NJS_INLINE void reset() noexcept {
    any.first  = static_cast<intptr_t>(-1);
    any.second = static_cast<intptr_t>(-1);
  }

  // ------------------------------------------------------------------------
  // [Accessors]
  // ------------------------------------------------------------------------

  NJS_INLINE bool hasArgument() const noexcept { return value.argIndex != -1; }
  NJS_INLINE bool hasValue() const noexcept { return any.second != -1; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  struct AnyData {
    intptr_t first;
    intptr_t second;
  };

  struct ErrorData {
    const char* message;
  };

  struct ValueData {
    intptr_t argIndex;
    union {
      uintptr_t typeId;
      const char* typeName;
      const char* message;
    };
  };

  struct ArgumentsData {
    intptr_t minArgs;
    intptr_t maxArgs;
  };

  struct ConstructCallData {
    const char* className;
  };

  union {
    AnyData any;
    ErrorData error;
    ValueData value;
    ArgumentsData arguments;
    ConstructCallData constructCall;
  };

  //! Static buffer for temporary strings.
  char staticBuffer[Globals::kMaxBufferSize];
};

// ============================================================================
// [njs::ResultMixin]
// ============================================================================

//! Result mixin, provides methods that can be used by `ExecutionContext`. The
//! purpose of this mixin is to define methods that are generic and then simply
//! reused by all VM backends.
class ResultMixin {
public:
  NJS_INLINE ResultMixin() noexcept { _payload.reset(); }

  // ------------------------------------------------------------------------
  // [Throw Exception]
  // ------------------------------------------------------------------------

  // ------------------------------------------------------------------------
  // [Invalid Value / Argument]
  // ------------------------------------------------------------------------

  NJS_INLINE Result invalidValue() noexcept {
    return Globals::kResultInvalidValue;
  }

  NJS_INLINE Result invalidValueTypeId(uint32_t typeId) noexcept {
    _payload.value.typeId = typeId;
    return Globals::kResultInvalidValueTypeId;
  }

  NJS_INLINE Result invalidValueTypeName(const char* typeName) noexcept {
    _payload.value.typeName = typeName;
    return Globals::kResultInvalidValueTypeName;
  }

  NJS_INLINE Result invalidValueCustom(const char* message) noexcept {
    _payload.value.message = message;
    return Globals::kResultInvalidValueCustom;
  }

  NJS_INLINE Result invalidArgument() noexcept {
    _payload.value.argIndex = -2;
    return Globals::kResultInvalidValue;
  }

  NJS_INLINE Result invalidArgument(unsigned int index) noexcept {
    _payload.value.argIndex = static_cast<intptr_t>(index);
    return Globals::kResultInvalidValue;
  }

  NJS_INLINE Result invalidArgumentTypeId(unsigned int index, uint32_t typeId) noexcept {
    _payload.value.argIndex = static_cast<intptr_t>(index);
    _payload.value.typeId = typeId;
    return Globals::kResultInvalidValueTypeId;
  }

  NJS_INLINE Result invalidArgumentTypeName(unsigned int index, const char* typeName) noexcept {
    _payload.value.argIndex = static_cast<intptr_t>(index);
    _payload.value.typeName = typeName;
    return Globals::kResultInvalidValueTypeName;
  }

  NJS_INLINE Result invalidArgumentCustom(unsigned int index, const char* message) noexcept {
    _payload.value.argIndex = static_cast<intptr_t>(index);
    _payload.value.message = message;
    return Globals::kResultInvalidValueCustom;
  }

  // ------------------------------------------------------------------------
  // [Invalid Arguments Length]
  // ------------------------------------------------------------------------

  NJS_INLINE Result invalidArgumentsLength() noexcept {
    return Globals::kResultInvalidArgumentsLength;
  }

  NJS_INLINE Result invalidArgumentsLength(unsigned int numArgs) noexcept {
    _payload.arguments.minArgs = static_cast<intptr_t>(numArgs);
    _payload.arguments.maxArgs = static_cast<intptr_t>(numArgs);
    return Globals::kResultInvalidArgumentsLength;
  }

  NJS_INLINE Result invalidArgumentsLength(unsigned int minArgs, unsigned int maxArgs) noexcept {
    _payload.arguments.minArgs = static_cast<intptr_t>(minArgs);
    _payload.arguments.maxArgs = static_cast<intptr_t>(maxArgs);
    return Globals::kResultInvalidArgumentsLength;
  }

  // ------------------------------------------------------------------------
  // [Invalid Construct-Call]
  // ------------------------------------------------------------------------

  NJS_INLINE Result invalidConstructCall() noexcept {
    return Globals::kResultInvalidConstructCall;
  }

  NJS_INLINE Result invalidConstructCall(const char* className) noexcept {
    _payload.constructCall.className = className;
    return Globals::kResultInvalidConstructCall;
  }

  NJS_INLINE Result abstractConstructCall() noexcept {
    return Globals::kResultAbstractConstructCall;
  }

  NJS_INLINE Result abstractConstructCall(const char* className) noexcept {
    _payload.constructCall.className = className;
    return Globals::kResultAbstractConstructCall;
  }

  // ------------------------------------------------------------------------
  // [Members]
  // ------------------------------------------------------------------------

  ResultPayload _payload;
};

// ============================================================================
// [njs::Maybe]
// ============================================================================

// Inspired in V8, however, holds `Result` instead of `bool`.
template<typename T>
class Maybe {
public:
  NJS_INLINE Maybe() noexcept
    : _value(),
      _result(Globals::kResultOk) {}

  NJS_INLINE Maybe(Result result, const T& value) noexcept
    : _value(value),
      _result(result) {}

  NJS_INLINE bool isOk() const noexcept { return _result == Globals::kResultOk; }
  NJS_INLINE const T& value() const noexcept { return _value; }
  NJS_INLINE njs::Result result() const noexcept { return _result; }

  T _value;
  Result _result;
};

// ============================================================================
// [njs::NullType & UndefinedType]
// ============================================================================

// NOTE: These are just to trick C++ type system to return NULL and UNDEFINED
// as some VMs have specialized methods for returning these. It's much more
// convenient to write `returnValue(njs::Undefined)` rather than `returnUndefined()`.
struct NullType {};
struct UndefinedType {};

static const NullType Null = {};
static const UndefinedType Undefined = {};

// ============================================================================
// [njs::ResultOf & NJS_CHECK]
// ============================================================================

//! Used internally to get a result of an operation that can fail. There are
//! many areas where an invalid handle can be returned instead of a `Result`.
//! VM bridges may overload it and propagate a specific error in such case.
//!
//! By default no implementation for `T` is provided, thus it would be compile
//! time error if used with non-implemented type.
template<typename T>
NJS_INLINE Result resultOf(const T& in) noexcept;

template<>
NJS_INLINE Result resultOf(const Result& in) noexcept {
  // Provided for convenience.
  return in;
}

template<typename T>
NJS_INLINE Result resultOf(T* in) noexcept {
  // Null-pointer check, used after memory allocation or `new(std::nothrow)`.
  return in ? Globals::kResultOk : Globals::kResultOutOfMemory;
}

template<typename T>
NJS_INLINE Result resultOf(const T* in) noexcept {
  // The same as above, specialized for `const T*`.
  return in ? Globals::kResultOk : Globals::kResultOutOfMemory;
}

template<typename T>
NJS_INLINE Result resultOf(const Maybe<T>& in) noexcept {
  // Just pass the result of Maybe<T> handle.
  return in.result();
}

// Part of the NJS-API - these must be provided by the engine and return
// `Globals::kResultInvalidHandle` in case the handle is not valid.
template<>
NJS_INLINE Result resultOf(const Value& in) noexcept;

#define NJS_CHECK(...) \
  do { \
    ::njs::Result result_ = ::njs::resultOf(__VA_ARGS__); \
    if (result_ != 0) { \
      return result_; \
    } \
  } while (0)

// ============================================================================
// [njs::Internal::reportError]
// ============================================================================

namespace Internal {

// NOTE: This is templated as to make it possible to be declared without knowing the `Context`.
template<typename Context>
static NJS_NOINLINE void reportError(Context& ctx, Result result, const ResultPayload& payload) noexcept {
  unsigned int exceptionType = Globals::kExceptionError;
  const StaticData& staticData = _staticData;

  enum { kMsgSize = Globals::kMaxBufferSize };
  char msgBuf[kMsgSize];
  const char* msg = msgBuf;

  if (result >= Globals::_kResultThrowFirst &&
      result <= Globals::_kResultThrowLast) {
    exceptionType = result;

    if (!payload.isInitialized())
      msg = "";
    else
      msg = const_cast<char*>(payload.error.message);
  }
  else if (result >= Globals::_kResultValueFirst &&
           result <= Globals::_kResultValueLast) {
    exceptionType = Globals::kExceptionTypeError;

    char baseBuf[64];
    const char* base = baseBuf;

    intptr_t argIndex = payload.value.argIndex;
    if (argIndex == -1)
      base = "Invalid value";
    else if (argIndex == -2)
      base = "Invalid argument";
    else
      StrUtils::sformat(baseBuf, 64, "Invalid argument [%u]", static_cast<unsigned int>(argIndex));

    if (result == Globals::kResultInvalidValueTypeId) {
      const char* typeName = staticData.typeNameOf(payload.value.typeId);
      StrUtils::sformat(msgBuf, kMsgSize, "%s: Expected Type '%s'", base, typeName);
    }
    else if (result == Globals::kResultInvalidValueTypeName) {
      const char* typeName = payload.value.typeName;
      StrUtils::sformat(msgBuf, kMsgSize, "%s: Expected Type '%s'", base, typeName);
    }
    else if (result == Globals::kResultInvalidValueCustom) {
      StrUtils::sformat(msgBuf, kMsgSize, "%s: %s", base, payload.value.message);
    }
    else {
      msg = base;
    }
  }
  else if (result == Globals::kResultInvalidArgumentsLength) {
    exceptionType = Globals::kExceptionTypeError;

    int minArgs = static_cast<int>(payload.arguments.minArgs);
    int maxArgs = static_cast<int>(payload.arguments.maxArgs);

    if (minArgs == -1 || maxArgs == -1)
      msg = "Invalid number of arguments: (unspecified)";
    else if (minArgs == maxArgs)
      StrUtils::sformat(msgBuf, kMsgSize, "Invalid number of arguments: Required exactly %d", minArgs);
    else
      StrUtils::sformat(msgBuf, kMsgSize, "Invalid number of arguments: Required between %d..%d", minArgs, maxArgs);
  }
  else if (result == Globals::kResultInvalidConstructCall ||
           result == Globals::kResultAbstractConstructCall) {
    exceptionType = Globals::kExceptionTypeError;

    const char* className = "(anonymous)";
    if (payload.isInitialized())
      className = payload.constructCall.className;

    const char* reason =
      (result == Globals::kResultInvalidConstructCall)
        ? "Use new operator"
        : "Class is abstract";
    StrUtils::sformat(msgBuf, kMsgSize, "Cannot instantiate '%s': %s", className, reason);
  }
  else {
    msg = "Unknown error";
  }

  ctx.throwNewException(exceptionType, ctx.newString(Utf8Ref(msg)));
}

} // {Internal}
} // {njs}

#endif // NJS_BASE_H
