// [NJS]
// Neutral JS interface.
//
// [License]
// Public Domain <http://unlicense.org>

// [Guard]
#ifndef _NJS_BASE_H
#define _NJS_BASE_H

// [Dependencies]
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// [njs::#defs]
// ============================================================================

#define NJS_VERSION 0x010000

#if defined(_MSC_VER)
# define NJS_INLINE __forceinline
# define NJS_NOINLINE __declspec(noinline)
# if _MSC_FULL_VER >= 180021114
#  define NJS_NOEXCEPT noexcept
# else
#  define NJS_NOEXCEPT throw()
# endif
#elif defined(__clang__)
# define NJS_INLINE inline __attribute__((__always_inline__))
# define NJS_NOINLINE __attribute__((__noinline__))
# if __has_feature(cxx_noexcept)
#  define NJS_NOEXCEPT noexcept
# endif
#elif defined(__GNUC__)
# define NJS_INLINE inline __attribute__((__always_inline__))
# define NJS_NOINLINE __attribute__((__noinline__))
# if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100) >= 40600
#  define NJS_NOEXCEPT noexcept
# endif
#else
# define NJS_INLINE inline
# define NJS_NOINLINE
#endif

#if !defined(NJS_NOEXCEPT)
# define NJS_NOEXCEPT
#endif // !NJS_NOEXCEPT

#if !defined(NJS_ASSERT)
# include <assert.h>
# define NJS_ASSERT assert
#endif // !NJS_ASSERT

// Miscellaneous macros used by NJS.
#define NJS_NO_COPY(SELF) \
private: \
  NJS_INLINE SELF(const SELF&) NJS_NOEXCEPT; \
  NJS_INLINE SELF& operator=(const SELF&) NJS_NOEXCEPT; \
public:

namespace njs {

// ============================================================================
// [Forward Declarations]
// ============================================================================

// --- Provided here ---

class Latin1Ref;
class Utf8Ref;
class Utf16Ref;

// --- Provided by the VM engine ---

// Runtime is mapped to the underlying VM runtime / heap. Each runtime can have
// multiple separate contexts.
class Runtime;

// Basic context for interacting with VM. Hold all necessary variables to make
// the interaction simple, but doesn't offer advanced features that require
// to manage memory on the C++ side. See `ExecutionContext` for more powerful
// interface.
class Context;

// Currently maps to `v8::HandleScope`.
class HandleScope;

// Scoped context is a `Context` and `HandleScope`. You should prefer this
// context in case that you enter a call that was not triggered by the VM. For
// example when an asynchronous task completes in node.js and the `AfterWork`
// handler is called, you need to create `ScopedContext` instead of `Context`,
// because no handle scope exist at that point, however, when a native function
// is called by the VM there is already a `HandleScope`.
class ScopedContext;

class ExecutionContext;
class FunctionCallContext;
class GetPropertyContext;
class SetPropertyContext;

// VM value types (handles).
class Value;
class Persistent;

namespace internal {
  struct ResultPayload;
} // internal namespace

// ============================================================================
// [njs::Limits]
// ============================================================================

// Internal limits.
enum Limits {
  kMaxEnumLength = 64,
  kMaxBufferSize = 128
};

// Other limits depends on VM. They should be established at the top of the VM
// implementation file.

// ============================================================================
// [njs::ValueType]
// ============================================================================

// VM neutral value type.
enum ValueType {
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

// ============================================================================
// [njs::ExceptionType]
// ============================================================================

// VM neutral exception type.
enum ExceptionType {
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

enum ConceptType {
  kConceptSerializer = 0,
  kConceptValidator = 1
};

// ============================================================================
// [njs::Result]
// ============================================================================

// VM neutral result code.
enum ResultCode {
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

  // NJS errors simplifying class wrapping and arguments' extraction.
  kResultInvalidArgumentCommon,
  kResultInvalidArgumentValue,
  kResultInvalidArgumentTypeId,
  kResultInvalidArgumentTypeName,
  kResultInvalidArgumentCount,

  kResultUnsafeInt64Conversion,
  kResultUnsafeUint64Conversion,

  kResultInvalidValue,
  kResultInvalidValueType,
  kResultInvalidValueCommon,
  kResultInvalidValueMinMax,

  kResultInvalidConstructCall,
  kResultAbstractConstructCall,

  // This is not an error, it's telling the error handler to not handle the
  // error as the caller thrown an exception or did something else and wants
  // to finish the current call-chain.
  kResultBypass
};

// Result value - just a mark to indicate all APIs that returns `ResultCode`.
typedef unsigned int Result;

// ============================================================================
// [njs::Maybe]
// ============================================================================

// Inspired in V8, however, holds `Result` instead of `bool`.
template<typename T>
class Maybe {
public:
  NJS_INLINE Maybe() NJS_NOEXCEPT
    : _error(kResultOk),
      _value() {}

  NJS_INLINE Maybe(Result error, const T& value) NJS_NOEXCEPT
    : _error(error),
      _value(value) {}

  NJS_INLINE bool IsOk() const NJS_NOEXCEPT { return _error == kResultOk; }

  NJS_INLINE njs::Result Error() const NJS_NOEXCEPT { return _error; }
  NJS_INLINE const T& Value() const NJS_NOEXCEPT { return _value; }

  Result _error;
  T _value;
};

// ============================================================================
// [njs::NullType & UndefinedType]
// ============================================================================

// NOTE: These are just to trick C++ type system to return NULL and UNDEFINED
// as some VMs have specialized methods for returning these. It's much concise
// to write `Return(njs::Undefined)` than `ReturnUndefined()` and when using
// V8 it may lead into a more optimized code.
struct NullType {};
struct UndefinedType {};

static const NullType Null = {};
static const UndefinedType Undefined = {};

// ============================================================================
// [njs::internal::TypeTraits]
// ============================================================================

// TODO:
// [ ] Missing wchar_t, char16_t, char32_t
// [ ] Any other types we should worry about?
namespace internal {
  enum TraitId {
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

  template<typename T, int Id>
  struct BaseTraits {
    enum {
      kTraitId     = Id,
      kIsPrimitive = Id >= kTraitIdBool     && Id <= kTraitIdDouble,
      kIsBool      = Id == kTraitIdBool,
      kIsInt       = Id >= kTraitIdSafeInt  && Id <= kTraitIdUnsafeUint,
      kIsSigned    = Id == kTraitIdSafeInt  || Id == kTraitIdUnsafeInt,
      kIsUnsigned  = Id == kTraitIdSafeUint || Id == kTraitIdUnsafeUint,
      kIsReal      = Id >= kTraitIdFloat    && Id <= kTraitIdDouble,
      kIsStrRef    = Id == kTraitIdStrRef
    };
  };

  // Type traits are used when converting values from C++ <-> Javascript.
  template<typename T>
  struct TypeTraits : public BaseTraits<T, kTraitIdUnknown> {};

  template<> struct TypeTraits<bool     > : public BaseTraits<bool     , kTraitIdBool  > {};
  template<> struct TypeTraits<float    > : public BaseTraits<float    , kTraitIdFloat > {};
  template<> struct TypeTraits<double   > : public BaseTraits<double   , kTraitIdDouble> {};

  template<> struct TypeTraits<Latin1Ref> : public BaseTraits<Latin1Ref, kTraitIdStrRef> {};
  template<> struct TypeTraits<Utf8Ref  > : public BaseTraits<Utf8Ref  , kTraitIdStrRef> {};
  template<> struct TypeTraits<Utf16Ref > : public BaseTraits<Utf16Ref , kTraitIdStrRef> {};

  template<typename T>
  struct IsSignedType {
    enum { kValue = static_cast<T>(~static_cast<T>(0)) < static_cast<T>(0) };
  };

#define NJS_INT_TRAITS(T)                                                     \
  template<>                                                                  \
  struct TypeTraits<T>                                                        \
    : public BaseTraits < T, IsSignedType<T>::kValue                          \
      ? (sizeof(T) <= 4 ? kTraitIdSafeInt  : kTraitIdUnsafeInt )              \
      : (sizeof(T) <= 4 ? kTraitIdSafeUint : kTraitIdUnsafeUint) >            \
  {                                                                           \
    static NJS_INLINE T minValue() NJS_NOEXCEPT {                             \
      return kIsSigned                                                        \
        ? static_cast<T>(1) << (sizeof(T) * 8 - 1)                            \
        : static_cast<T>(0);                                                  \
    }                                                                         \
                                                                              \
    static NJS_INLINE T maxValue() NJS_NOEXCEPT {                             \
      return kIsSigned                                                        \
        ? ~static_cast<T>(0) ^ minValue()                                     \
        : ~static_cast<T>(0);                                                 \
    }                                                                         \
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
} // internal namespace

// ============================================================================
// [njs::internal::IntUtils]
// ============================================================================

namespace internal {
  template<typename In, typename Out, int IsNarrowing, int IsSigned>
  struct IntCastImpl {
    static NJS_INLINE Result Op(In in, Out& out) NJS_NOEXCEPT {
      out = static_cast<Out>(in);
      return kResultOk;
    }
  };

  // Unsigned narrowing.
  template<typename In, typename Out>
  struct IntCastImpl<In, Out, 1, 0> {
    static NJS_INLINE Result Op(In in, Out& out) NJS_NOEXCEPT {
      if (in > static_cast<In>(TypeTraits<Out>::maxValue()))
        return kResultInvalidValue;

      out = static_cast<Out>(in);
      return kResultOk;
    }
  };

  // Signed narrowing.
  template<typename In, typename Out>
  struct IntCastImpl<In, Out, 1, 1> {
    static NJS_INLINE Result Op(In in, Out& out) NJS_NOEXCEPT {
      if (in < static_cast<In>(TypeTraits<Out>::minValue()) ||
          in > static_cast<In>(TypeTraits<Out>::maxValue()) )
        return kResultInvalidValue;

      out = static_cast<Out>(in);
      return kResultOk;
    }
  };

  template<typename In, typename Out>
  NJS_INLINE Result IntCast(In in, Out& out) NJS_NOEXCEPT {
    return IntCastImpl<
      In, Out,
      (sizeof(In) > sizeof(Out)),
      TypeTraits<In>::kIsSigned && TypeTraits<Out>::kIsSigned>::Op(in, out);
  }

  template<typename T, size_t Size, int IsSigned>
  struct IsSafeIntImpl {
    static NJS_INLINE bool Op(T x) NJS_NOEXCEPT {
      // [+] 32-bit and less are safe.
      // [+] 64-bit types are specialized.
      // [-] More than 64-bit types are not supported.
      return Size <= 4;
    }
  };

  template<typename T>
  struct IsSafeIntImpl<T, 8, 0> {
    static NJS_INLINE bool Op(T x) NJS_NOEXCEPT {
      return static_cast<uint64_t>(x) <= static_cast<uint64_t>(9007199254740991ull);
    }
  };

  template<typename T>
  struct IsSafeIntImpl<T, 8, 1> {
    static NJS_INLINE bool Op(T x) NJS_NOEXCEPT {
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
  NJS_INLINE bool IsSafeInt(T x) NJS_NOEXCEPT {
    return internal::IsSafeIntImpl<T, sizeof(T), internal::TypeTraits<T>::kIsSigned>::Op(x);
  }

  static NJS_INLINE Result DoubleToInt64(double in, int64_t& out) NJS_NOEXCEPT {
    if (in >= -9007199254740991.0 && in <= 9007199254740991.0) {
      int64_t x = static_cast<int64_t>(in);
      if (static_cast<double>(x) == in) {
        out = x;
        return kResultOk;
      }
    }

    return kResultInvalidValue;
  }

  static NJS_INLINE Result DoubleToUint64(double in, uint64_t& out) NJS_NOEXCEPT {
    if (in >= 0.0 && in <= 9007199254740991.0) {
      int64_t x = static_cast<int64_t>(in);
      if (static_cast<double>(x) == in) {
        out = static_cast<uint64_t>(x);
        return kResultOk;
      }
    }

    return kResultInvalidValue;
  }
} // internal namespace

// ============================================================================
// [njs::internal::StrUtils]
// ============================================================================

namespace internal {
  // Inlining probably doesn't matter here, `vsnprintf` is complex and one more
  // function call shouldn't make any difference here, so don't inline.
  //
  // NOTE: This function doesn't guarantee NULL termination, this means that the
  // return value has to be USED. This is no problem for NJS library as all VM
  // calls allow to specify the length of the string(s) passed.
  static NJS_NOINLINE unsigned int StrVFormat(char* dst, size_t maxLength, const char* fmt, va_list ap) NJS_NOEXCEPT {
#if defined(_MSC_VER) && _MSC_VER < 1900
    int result = ::_vsnprintf(dst, maxLength, fmt, ap);
#else
    int result = ::vsnprintf(dst, maxLength, fmt, ap);
#endif

    if (result < 0)
      return 0;

    if (static_cast<unsigned int>(result) > maxLength)
      return static_cast<unsigned int>(maxLength);

    return static_cast<unsigned int>(result);
  }

  // Cannot be inlined, some compilers (like GCC) have problems with variable
  // argument functions declared as __always_inline__.
  //
  // NOTE: The function has the same behavior as `StrVFormat()`.
  static NJS_NOINLINE unsigned int StrFormat(char* dst, size_t maxLength, const char* fmt, ...) NJS_NOEXCEPT {
    va_list ap;
    va_start(ap, fmt);
    unsigned int result = StrVFormat(dst, maxLength, fmt, ap);
    va_end(ap);
    return result;
  }

  // Required mostly by `Utf16` string, generalized for any type.
  template<typename CharType>
  NJS_INLINE size_t StrLen(const CharType* str) NJS_NOEXCEPT {
    NJS_ASSERT(str != NULL);

    size_t i = 0;
    while (str[i] != CharType(0))
      i++;
    return i;
  }
  // Specialize a bit...
  template<>
  NJS_INLINE size_t StrLen(const char* str) NJS_NOEXCEPT {
    return ::strlen(str);
  }
} // internal namespace

// ============================================================================
// [njs::StrRef]
// ============================================================================

template<typename T>
class StrRef {
  NJS_NO_COPY(StrRef<T>)

public:
  typedef T Type;

  explicit NJS_INLINE StrRef() NJS_NOEXCEPT { Reset(); }
  explicit NJS_INLINE StrRef(T* data) NJS_NOEXCEPT { SetData(data); }
  NJS_INLINE StrRef(T* data, size_t length) NJS_NOEXCEPT { SetData(data, length); }

  NJS_INLINE void Reset() NJS_NOEXCEPT {
    _data = NULL;
    _length = 0;
  }

  NJS_INLINE void SetData(T* data) NJS_NOEXCEPT {
    _data = data;
    _length = internal::StrLen(data);
  }

  NJS_INLINE void SetData(T* data, size_t length) NJS_NOEXCEPT {
    _data = data;
    _length = length;
  }

  NJS_INLINE bool HasData() const NJS_NOEXCEPT { return _data != NULL; }
  NJS_INLINE T* GetData() const NJS_NOEXCEPT { return _data; }
  NJS_INLINE size_t GetLength() const NJS_NOEXCEPT { return _length; }

  // ------------------------------------------------------------------------
  // [Members]
  // ------------------------------------------------------------------------

  T* _data;
  size_t _length;
};

// ============================================================================
// [njs::Latin1Ref]
// ============================================================================

class Latin1Ref : public StrRef<const char> {
public:
  explicit NJS_INLINE Latin1Ref(const char* data) NJS_NOEXCEPT
    : StrRef(data) {}

  NJS_INLINE Latin1Ref(const char* data, size_t length) NJS_NOEXCEPT
    : StrRef(data, length) {}
};

// ============================================================================
// [njs::Utf8Ref]
// ============================================================================

class Utf8Ref : public StrRef<const char> {
public:
  explicit NJS_INLINE Utf8Ref(const char* data) NJS_NOEXCEPT
    : StrRef(data) {}

  NJS_INLINE Utf8Ref(const char* data, size_t length) NJS_NOEXCEPT
    : StrRef(data, length) {}
};

// ============================================================================
// [njs::Utf16Ref]
// ============================================================================

class Utf16Ref : public StrRef<const uint16_t> {
public:
  explicit NJS_INLINE Utf16Ref(const uint16_t* data) NJS_NOEXCEPT
    : StrRef(data) {}

  NJS_INLINE Utf16Ref(const uint16_t* data, size_t length) NJS_NOEXCEPT
    : StrRef(data, length) {}
};

// ============================================================================
// [njs::Range]
// ============================================================================

template<typename T>
struct Range {
  enum { kConceptType = kConceptValidator };
  typedef T Type;

  NJS_INLINE Range<T>(Type minValue, Type maxValue) NJS_NOEXCEPT
    : _minValue(minValue),
      _maxValue(maxValue) {}

  NJS_INLINE Result Validate(const Type& value) const NJS_NOEXCEPT {
    if (value >= _minValue && value <= _maxValue)
      return kResultOk;
    else
      return kResultInvalidValueMinMax;
  }

  Type _minValue;
  Type _maxValue;
};

// ============================================================================
// [njs::StaticData]
// ============================================================================

// Minimal const data that is used to improve and unify error handling.
//
// NOTE: Do not add useless members here, a copy will be embedded in every
// executable and shared object that accesses it (which is likely to be true).
struct StaticData {
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_INLINE const char* getTypeName(int type) const NJS_NOEXCEPT {
    NJS_ASSERT(type >= 0 && type < kValueCount);
    return _typeName[type];
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  char _typeName[kValueCount][20];
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
// [njs::internal::BindingItem]
// ============================================================================

namespace internal {
  // Information about a single binding item used to declaratively bind a C++
  // class into JavaScript. Each `BindingItem` contains an information about
  // a getter/setter, static (class) function, or a member function.
  struct BindingItem {
    NJS_INLINE BindingItem(int type, int flags, const char* name, const void* data) NJS_NOEXCEPT
      : type(type),
        flags(flags),
        name(name),
        data(data) {}

    enum Type {
      kTypeInvalid = 0,
      kTypeStatic = 1,
      kTypeMethod = 2,
      kTypeGetter = 3,
      kTypeSetter = 4
    };

    // Type of the item.
    unsigned int type;
    // Flags.
    unsigned int flags;
    // Name (function name, property name, etc...).
    const char* name;
    // Data (function pointer).
    const void* data;
  };
} // internal namespace

// ============================================================================
// [njs::ResultOf & NJS_CHECK]
// ============================================================================

// Used internally to get a result of an operation that can fail. There are
// many areas where an invalid handle can be returned instead of a `Result`.
// VM bridges may overload it and propagate a specific error in such case.
template<typename T>
NJS_INLINE Result ResultOf(const T& in) NJS_NOEXCEPT;
// TODO: Doesn't have to be implemented, right? {
//  NJS_ASSERT(!"njs::ResultOf<> has no overload for a given type");
//  return kResultInvalidState;
//}

template<>
NJS_INLINE Result ResultOf(const Result& in) NJS_NOEXCEPT {
  return in;
}

template<typename T>
NJS_INLINE Result ResultOf(T* in) NJS_NOEXCEPT {
  return in ? kResultOk : kResultOutOfMemory;
}

template<typename T>
NJS_INLINE Result ResultOf(const T* in) NJS_NOEXCEPT {
  return in ? kResultOk : kResultOutOfMemory;
}

template<typename T>
NJS_INLINE Result ResultOf(const Maybe<T>& in) NJS_NOEXCEPT {
  return in.Result();
}

// Part of the NJS-API - these must be provided by the engine and
// returns `kResultInvalidHandle` in case the handle is invalid.
template<>
NJS_INLINE Result ResultOf(const Value& in) NJS_NOEXCEPT;

#define NJS_CHECK(...) \
  do { \
    ::njs::Result result_ = ::njs::ResultOf(__VA_ARGS__); \
    if (result_ != 0) { \
      return result_; \
    } \
  } while (0)

} // njs namespace

// [Guard]
#endif // _NJS_BASE_H
