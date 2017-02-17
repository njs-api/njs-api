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

#include <new>

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
#define NJS_NONCOPYABLE(SELF) \
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

//! Runtime is mapped to the underlying VM runtime / heap. Each runtime can have
//! multiple separate contexts.
class Runtime;

//! Basic context for interacting with VM. Holds all necessary variables to make
//! the interaction simple, but doesn't offer advanced features that require
//! memory management on C++ side. See `ExecutionContext` that provides an
//! additional functionality.
class Context;

//! Handle scope if VM is using that concept, currently maps to `v8::HandleScope`.
class HandleScope;

//! Scoped context is a composition of `Context` and `HandleScope`. You should
//! prefer this type of context in case that you enter a call that was not
//! triggered by JS-VM. For example when an asynchronous task completes in node.js
//! and the `AfterWork` handler is called by libuv, it's important to create a
//! `ScopedContext` instead of just `Context`, because no handle scope exist at
//! that point.
class ScopedContext;

class ExecutionContext;
class FunctionCallContext;
class GetPropertyContext;
class SetPropertyContext;

//! Local javascript value.
class Value;

//! Persistent javascript value.
class Persistent;

// ============================================================================
// [njs::Limits]
// ============================================================================

//! NJS limits.
enum Limits {
  // NOTE: Other limits depend on VM. They should be established at the top of
  // the VM implementation file.

  //! Maximum length of a single enumeration (string).
  kMaxEnumLength = 64,
  //! Maximum length of a buffer used internally to format messages (errors).
  kMaxBufferSize = 256
};

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

// Result value - just a mark to indicate all APIs that returns `ResultCode`.
typedef unsigned int Result;

// ============================================================================
// [njs::Internal::TypeTraits]
// ============================================================================

// TODO:
// [ ] Missing wchar_t, char16_t, char32_t
// [ ] Any other types we should worry about?
namespace Internal {
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
} // Internal namespace

// ============================================================================
// [njs::IntUtils]
// ============================================================================

namespace IntUtils {
  template<typename In, typename Out, int IsNarrowing, int IsSigned>
  struct IntCastImpl {
    static NJS_INLINE Result op(In in, Out& out) NJS_NOEXCEPT {
      out = static_cast<Out>(in);
      return kResultOk;
    }
  };

  // Unsigned narrowing.
  template<typename In, typename Out>
  struct IntCastImpl<In, Out, 1, 0> {
    static NJS_INLINE Result op(In in, Out& out) NJS_NOEXCEPT {
      if (in > static_cast<In>(Internal::TypeTraits<Out>::maxValue()))
        return kResultInvalidValue;

      out = static_cast<Out>(in);
      return kResultOk;
    }
  };

  // Signed narrowing.
  template<typename In, typename Out>
  struct IntCastImpl<In, Out, 1, 1> {
    static NJS_INLINE Result op(In in, Out& out) NJS_NOEXCEPT {
      if (in < static_cast<In>(Internal::TypeTraits<Out>::minValue()) ||
          in > static_cast<In>(Internal::TypeTraits<Out>::maxValue()) )
        return kResultInvalidValue;

      out = static_cast<Out>(in);
      return kResultOk;
    }
  };

  template<typename In, typename Out>
  NJS_INLINE Result intCast(In in, Out& out) NJS_NOEXCEPT {
    return IntCastImpl<
      In, Out,
      (sizeof(In) > sizeof(Out)),
      Internal::TypeTraits<In>::kIsSigned && Internal::TypeTraits<Out>::kIsSigned>::op(in, out);
  }

  template<typename T, size_t Size, int IsSigned>
  struct IsSafeIntImpl {
    static NJS_INLINE bool op(T x) NJS_NOEXCEPT {
      // [+] 32-bit and less are safe.
      // [+] 64-bit types are specialized.
      // [-] More than 64-bit types are not supported.
      return Size <= 4;
    }
  };

  template<typename T>
  struct IsSafeIntImpl<T, 8, 0> {
    static NJS_INLINE bool op(T x) NJS_NOEXCEPT {
      return static_cast<uint64_t>(x) <= static_cast<uint64_t>(9007199254740991ull);
    }
  };

  template<typename T>
  struct IsSafeIntImpl<T, 8, 1> {
    static NJS_INLINE bool op(T x) NJS_NOEXCEPT {
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
  NJS_INLINE bool isSafeInt(T x) NJS_NOEXCEPT {
    return IsSafeIntImpl<T, sizeof(T), Internal::TypeTraits<T>::kIsSigned>::op(x);
  }

  static NJS_INLINE Result doubleToInt64(double in, int64_t& out) NJS_NOEXCEPT {
    if (in >= -9007199254740991.0 && in <= 9007199254740991.0) {
      int64_t x = static_cast<int64_t>(in);
      if (static_cast<double>(x) == in) {
        out = x;
        return kResultOk;
      }
    }

    return kResultInvalidValue;
  }

  static NJS_INLINE Result doubleToUint64(double in, uint64_t& out) NJS_NOEXCEPT {
    if (in >= 0.0 && in <= 9007199254740991.0) {
      int64_t x = static_cast<int64_t>(in);
      if (static_cast<double>(x) == in) {
        out = static_cast<uint64_t>(x);
        return kResultOk;
      }
    }

    return kResultInvalidValue;
  }
} // IntUtils namespace

// ============================================================================
// [njs::StrUtils]
// ============================================================================

namespace StrUtils {
  // Inlining probably doesn't matter here, `vsnprintf` is complex and one more
  // function call shouldn't make any difference here, so don't inline.
  //
  // NOTE: This function doesn't guarantee NULL termination, this means that the
  // return value has to be USED. This is no problem for NJS library as all VM
  // calls allow to specify the length of the string(s) passed.
  static NJS_NOINLINE unsigned int vsformat(char* dst, size_t maxLength, const char* fmt, va_list ap) NJS_NOEXCEPT {
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
  // NOTE: The function has the same behavior as `vsformat()`.
  static NJS_NOINLINE unsigned int sformat(char* dst, size_t maxLength, const char* fmt, ...) NJS_NOEXCEPT {
    va_list ap;
    va_start(ap, fmt);
    unsigned int result = vsformat(dst, maxLength, fmt, ap);
    va_end(ap);
    return result;
  }

  // Required mostly by `Utf16` string, generalized for any type.
  template<typename CharType>
  NJS_INLINE size_t slen(const CharType* str) NJS_NOEXCEPT {
    NJS_ASSERT(str != NULL);

    size_t i = 0;
    while (str[i] != CharType(0))
      i++;
    return i;
  }
  // Specialize a bit...
  template<>
  NJS_INLINE size_t slen(const char* str) NJS_NOEXCEPT { return ::strlen(str); }
}

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

  NJS_INLINE StrRef() NJS_NOEXCEPT { reset(); }
  explicit NJS_INLINE StrRef(T* data) NJS_NOEXCEPT { init(data); }
  NJS_INLINE StrRef(T* data, size_t length) NJS_NOEXCEPT { init(data, length); }
  NJS_INLINE StrRef(const StrRef<T>& other) NJS_NOEXCEPT { init(other); }

  NJS_INLINE bool isInitialized() const NJS_NOEXCEPT {
    return _data != NULL;
  }

  NJS_INLINE void reset() NJS_NOEXCEPT {
    _data = NULL;
    _length = 0;
  }

  NJS_INLINE void init(T* data) NJS_NOEXCEPT {
    init(data, StrUtils::slen(data));
  }

  NJS_INLINE void init(T* data, size_t length) NJS_NOEXCEPT {
    _data = data;
    _length = length;
  }

  NJS_INLINE void init(const StrRef<T>& other) NJS_NOEXCEPT {
    _data = other._data;
    _length = other._length;
  }

  NJS_INLINE bool hasData() const NJS_NOEXCEPT { return _data != NULL; }
  NJS_INLINE T* getData() const NJS_NOEXCEPT { return _data; }
  NJS_INLINE size_t getLength() const NJS_NOEXCEPT { return _length; }

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

  NJS_INLINE Latin1Ref(const Latin1Ref& other) NJS_NOEXCEPT
    : StrRef(other) {}
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

  NJS_INLINE Utf8Ref(const Utf8Ref& other) NJS_NOEXCEPT
    : StrRef(other) {}
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

  NJS_INLINE Utf16Ref(const Utf16Ref& other) NJS_NOEXCEPT
    : StrRef(other) {}
};

// ============================================================================
// [njs::Range]
// ============================================================================

template<typename T>
class Range {
public:
  typedef T Type;

  enum { kConceptType = kConceptValidator };

  NJS_INLINE Range<T>(Type minValue, Type maxValue) NJS_NOEXCEPT
    : _minValue(minValue),
      _maxValue(maxValue) {}

  NJS_INLINE Result validate(const Type& value) const NJS_NOEXCEPT {
    if (value >= _minValue && value <= _maxValue)
      return kResultOk;
    else
      return kResultInvalidValueRange;
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

  unsigned int type;         //!< Type of the item.
  unsigned int flags;        //!< Flags.
  const char* name;          //!< Name (function name, property name, etc...).
  const void* data;          //!< Data (function pointer).
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
  NJS_INLINE bool isInitialized() const NJS_NOEXCEPT { return any.first != -1; }

  //! Has to be called to reset the content of payload. It resets only one
  //! member for efficiency. If this member is set then the payload is valid,
  //! otherwise it's invalid or not set.
  NJS_INLINE void reset() NJS_NOEXCEPT {
    any.first  = static_cast<intptr_t>(-1);
    any.second = static_cast<intptr_t>(-1);
  }

  // ------------------------------------------------------------------------
  // [Accessors]
  // ------------------------------------------------------------------------

  NJS_INLINE bool hasArgument() const NJS_NOEXCEPT { return value.argIndex != -1; }
  NJS_INLINE bool hasValue() const NJS_NOEXCEPT { return any.second != -1; }

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
  char staticBuffer[kMaxBufferSize];
};

// ============================================================================
// [njs::ResultMixin]
// ============================================================================

//! Result mixin, provides methods that can be used by `ExecutionContext`. The
//! purpose of this mixin is to define methods that are generic and then simply
//! reused by all VM backend.
class ResultMixin {
public:
  NJS_INLINE ResultMixin() NJS_NOEXCEPT { _payload.reset(); }

  // ------------------------------------------------------------------------
  // [Throw Exception]
  // ------------------------------------------------------------------------

  // ------------------------------------------------------------------------
  // [Invalid Value / Argument]
  // ------------------------------------------------------------------------

  NJS_INLINE Result invalidValue() NJS_NOEXCEPT {
    return kResultInvalidValue;
  }

  NJS_INLINE Result invalidValueTypeId(uint32_t typeId) NJS_NOEXCEPT {
    _payload.value.typeId = typeId;
    return kResultInvalidValueTypeId;
  }

  NJS_INLINE Result invalidValueTypeName(const char* typeName) NJS_NOEXCEPT {
    _payload.value.typeName = typeName;
    return kResultInvalidValueTypeName;
  }

  NJS_INLINE Result invalidValueCustom(const char* message) NJS_NOEXCEPT {
    _payload.value.message = message;
    return kResultInvalidValueCustom;
  }

  NJS_INLINE Result invalidArgument() NJS_NOEXCEPT {
    _payload.value.argIndex = -2;
    return kResultInvalidValue;
  }

  NJS_INLINE Result invalidArgument(unsigned int index) NJS_NOEXCEPT {
    _payload.value.argIndex = static_cast<intptr_t>(index);
    return kResultInvalidValue;
  }

  NJS_INLINE Result invalidArgumentTypeId(unsigned int index, uint32_t typeId) NJS_NOEXCEPT {
    _payload.value.argIndex = static_cast<intptr_t>(index);
    _payload.value.typeId = typeId;
    return kResultInvalidValueTypeId;
  }

  NJS_INLINE Result invalidArgumentTypeName(unsigned int index, const char* typeName) NJS_NOEXCEPT {
    _payload.value.argIndex = static_cast<intptr_t>(index);
    _payload.value.typeName = typeName;
    return kResultInvalidValueTypeName;
  }

  NJS_INLINE Result invalidArgumentCustom(unsigned int index, const char* message) NJS_NOEXCEPT {
    _payload.value.argIndex = static_cast<intptr_t>(index);
    _payload.value.message = message;
    return kResultInvalidValueCustom;
  }

  // ------------------------------------------------------------------------
  // [Invalid Arguments Length]
  // ------------------------------------------------------------------------

  NJS_INLINE Result invalidArgumentsLength() NJS_NOEXCEPT {
    return kResultInvalidArgumentsLength;
  }

  NJS_INLINE Result invalidArgumentsLength(unsigned int numArgs) NJS_NOEXCEPT {
    _payload.arguments.minArgs = static_cast<intptr_t>(numArgs);
    _payload.arguments.maxArgs = static_cast<intptr_t>(numArgs);
    return kResultInvalidArgumentsLength;
  }

  NJS_INLINE Result invalidArgumentsRange(unsigned int minArgs, unsigned int maxArgs) NJS_NOEXCEPT {
    _payload.arguments.minArgs = static_cast<intptr_t>(minArgs);
    _payload.arguments.maxArgs = static_cast<intptr_t>(maxArgs);
    return kResultInvalidArgumentsLength;
  }

  // ------------------------------------------------------------------------
  // [Invalid Construct-Call]
  // ------------------------------------------------------------------------

  NJS_INLINE Result invalidConstructCall() NJS_NOEXCEPT {
    return kResultInvalidConstructCall;
  }

  NJS_INLINE Result invalidConstructCall(const char* className) NJS_NOEXCEPT {
    _payload.constructCall.className = className;
    return kResultInvalidConstructCall;
  }

  NJS_INLINE Result abstractConstructCall() NJS_NOEXCEPT {
    return kResultAbstractConstructCall;
  }

  NJS_INLINE Result abstractConstructCall(const char* className) NJS_NOEXCEPT {
    _payload.constructCall.className = className;
    return kResultAbstractConstructCall;
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
  NJS_INLINE Maybe() NJS_NOEXCEPT
    : _value(),
      _result(kResultOk) {}

  NJS_INLINE Maybe(Result result, const T& value) NJS_NOEXCEPT
    : _value(value),
      _result(result) {}

  NJS_INLINE bool isOk() const NJS_NOEXCEPT { return _result == kResultOk; }
  NJS_INLINE const T& value() const NJS_NOEXCEPT { return _value; }
  NJS_INLINE njs::Result result() const NJS_NOEXCEPT { return _result; }

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
NJS_INLINE Result resultOf(const T& in) NJS_NOEXCEPT;

template<>
NJS_INLINE Result resultOf(const Result& in) NJS_NOEXCEPT {
  // Provided for convenience.
  return in;
}

template<typename T>
NJS_INLINE Result resultOf(T* in) NJS_NOEXCEPT {
  // Null-pointer check, used after memory allocation or `new(std::nothrow)`.
  return in ? kResultOk : kResultOutOfMemory;
}

template<typename T>
NJS_INLINE Result resultOf(const T* in) NJS_NOEXCEPT {
  // The same as above, specialized for `const T*`.
  return in ? kResultOk : kResultOutOfMemory;
}

template<typename T>
NJS_INLINE Result resultOf(const Maybe<T>& in) NJS_NOEXCEPT {
  // Just pass the result of Maybe<T> handle.
  return in.result();
}

// Part of the NJS-API - these must be provided by the engine and return
// `kResultInvalidHandle` in case the handle is not valid.
template<>
NJS_INLINE Result resultOf(const Value& in) NJS_NOEXCEPT;

#define NJS_CHECK(...) \
  do { \
    ::njs::Result result_ = ::njs::resultOf(__VA_ARGS__); \
    if (result_ != 0) { \
      return result_; \
    } \
  } while (0)

} // njs namespace

// [Guard]
#endif // _NJS_BASE_H
