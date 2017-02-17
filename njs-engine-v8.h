// [NJS]
// Neutral JS interface.
//
// [License]
// Public Domain <http://unlicense.org>

// This file implements a NJS <-> V8 engine bridge.

// [Guard]
#ifndef _NJS_ENGINE_V8_H
#define _NJS_ENGINE_V8_H

// [Dependencies]
#include "./njs-base.h"
#include <v8.h>

#if defined(NJS_INTEGRATE_NODE)
# include <node.h>
# include <node_buffer.h>
#endif // NJS_INTEGRATE_NODE

namespace njs {

// ============================================================================
// [Forward Declarations]
// ============================================================================

// These are needed to make things a bit simpler and minimize the number of
// functions that have to be forward-declared.
namespace Internal {
  // Provided after `Context`. This is just a convenience function that
  // allows to get V8's `v8::Isolate` from `njs::Context` before it's defined.
  static NJS_INLINE v8::Isolate* v8GetIsolateOfContext(Context& ctx) NJS_NOEXCEPT;

  // Provided after `Value`. These are just a convenience functions that allow
  // to get the V8' `v8::Local<v8::Value>` from `njs::Value` before it's defined.
  static NJS_INLINE v8::Local<v8::Value>& v8HandleOfValue(Value& value) NJS_NOEXCEPT;
  static NJS_INLINE const v8::Local<v8::Value>& v8HandleOfValue(const Value& value) NJS_NOEXCEPT;
} // Internal namespace

// ============================================================================
// [njs::Limits]
// ============================================================================

static const size_t kMaxStringLength = static_cast<size_t>(v8::String::kMaxLength);

// ============================================================================
// [njs::ResultOf]
// ============================================================================

// V8 specializations of `njs::resultOf<>`.
template<typename T>
NJS_INLINE Result resultOf(const v8::Local<T>& in) NJS_NOEXCEPT { return in.IsEmpty() ? kResultInvalidHandle : kResultOk; }

template<typename T>
NJS_INLINE Result resultOf(const v8::MaybeLocal<T>& in) NJS_NOEXCEPT { return in.IsEmpty() ? kResultInvalidHandle : kResultOk; }

template<typename T>
NJS_INLINE Result resultOf(const v8::Persistent<T>& in) NJS_NOEXCEPT { return in.IsEmpty() ? kResultInvalidHandle : kResultOk; }

template<typename T>
NJS_INLINE Result resultOf(const v8::Global<T>& in) NJS_NOEXCEPT { return in.IsEmpty() ? kResultInvalidHandle : kResultOk; }

template<typename T>
NJS_INLINE Result resultOf(const v8::Eternal<T>& in) NJS_NOEXCEPT { return in.IsEmpty() ? kResultInvalidHandle : kResultOk; }

template<>
NJS_INLINE Result resultOf(const Value& in) NJS_NOEXCEPT { return resultOf(Internal::v8HandleOfValue(in)); }

// ============================================================================
// [njs::Internal::V8 Helpers]
// ============================================================================

namespace Internal {
  // V8 specific destroy callback.
  typedef void (*V8WrapDestroyCallback)(const v8::WeakCallbackData<v8::Value, void>& data);

  // Helper to convert a V8's `MaybeLocal<Type>` to V8's `Local<Type>`.
  //
  // NOTE: This performs an unchecked operation. Converting an invalid handle will
  // keep the handle invalid without triggering assertion failure. The purpose is to
  // keep things simple and to check for invalid handles instead of introduction yet
  // another handle type in NJS.
  template<typename Type>
  static NJS_INLINE const v8::Local<Type>& v8LocalFromMaybe(const v8::MaybeLocal<Type>& in) NJS_NOEXCEPT {
    // Implements the following pattern:
    //
    //   v8::Local<Type> out;
    //   in.ToLocal(&out);
    //   return out;
    //
    // However, this produces a warning about unused return value, which is
    // useless in our case as we always check whether the handle is valid.
    return reinterpret_cast<const v8::Local<Type>&>(in);
  }

  // Helper that creates a new string based on `StrRef` type. It can create
  // string for all input types that match `kTraitIdStrRef`. Each specialization
  // uses a different V8 constructor. The `type` argument matches V8's
  // `NewStringType` and can be used to create internalized strings.
  template<typename StrRef>
  NJS_INLINE v8::Local<v8::Value> v8NewString(Context& ctx, const StrRef& str, v8::NewStringType type) NJS_NOEXCEPT;

  template<>
  NJS_INLINE v8::Local<v8::Value> v8NewString(Context& ctx, const Latin1Ref& str, v8::NewStringType type) NJS_NOEXCEPT {
    if (str.getLength() > kMaxStringLength)
      return v8::Local<v8::Value>();

    return v8LocalFromMaybe<v8::String>(
      v8::String::NewFromOneByte(
        v8GetIsolateOfContext(ctx),
        reinterpret_cast<const uint8_t*>(str.getData()), type, static_cast<int>(str.getLength())));
  }

  template<>
  NJS_INLINE v8::Local<v8::Value> v8NewString(Context& ctx, const Utf8Ref& str, v8::NewStringType type) NJS_NOEXCEPT {
    if (str.getLength() > kMaxStringLength)
      return v8::Local<v8::Value>();

    return v8LocalFromMaybe<v8::String>(
      v8::String::NewFromUtf8(
        v8GetIsolateOfContext(ctx),
        str.getData(), type, static_cast<int>(str.getLength())));
  }

  template<>
  NJS_INLINE v8::Local<v8::Value> v8NewString(Context& ctx, const Utf16Ref& str, v8::NewStringType type) NJS_NOEXCEPT {
    if (str.getLength() > kMaxStringLength)
      return v8::Local<v8::Value>();

    return v8LocalFromMaybe<v8::String>(
      v8::String::NewFromTwoByte(
        v8GetIsolateOfContext(ctx),
        str.getData(), type, static_cast<int>(str.getLength())));
  }

  // Conversion from `T` to V8's `Local<Value>`. Conversions are only possible
  // between known types (basically all types defined by NJS's type-traits) and
  // V8 handles. These won't automatically convert from one type to another.
  // In addition, it's very important to check whether the input value can be
  // safely converted to the output type. For example int64_t to double conversion
  // is no safe and must be checked, whereas int32_t to double is always safe.
  template<typename T, int Id>
  struct V8ConvertImpl {};

  template<typename T>
  struct V8ConvertImpl<T, kTraitIdBool> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      out = v8::Boolean::New(v8GetIsolateOfContext(ctx), in);
      return kResultOk;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsBoolean())
        return kResultInvalidValue;
      out = v8::Boolean::Cast(*in)->Value();
      return kResultOk;
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, kTraitIdSafeInt> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      out = v8::Integer::New(v8GetIsolateOfContext(ctx), in);
      return !out.IsEmpty() ? kResultOk : kResultBypass;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsInt32())
        return kResultInvalidValue;

      int32_t value = v8::Int32::Cast(*in)->Value();
      return IntUtils::intCast(value, out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, kTraitIdSafeUint> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      out = v8::Integer::New(v8GetIsolateOfContext(ctx), in);
      return !out.IsEmpty() ? kResultOk : kResultBypass;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsUint32())
        return kResultInvalidValue;

      uint32_t value = v8::Uint32::Cast(*in)->Value();
      return IntUtils::intCast(value, out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, kTraitIdUnsafeInt> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      // Fast-case: Most of the time if we hit packing int64_t to JS value it's
      // because of `size_t` and similar types. These types will hardly contain
      // value that is only representable as `int64_t` so we prefer this path
      // that creates a JavaScript integer, which can be then optimized by V8.
      if (in >= static_cast<T>(TypeTraits<int32_t>::minValue()) &&
          in <= static_cast<T>(TypeTraits<int32_t>::maxValue())) {
        out = v8::Integer::New(v8GetIsolateOfContext(ctx), static_cast<int32_t>(in));
      }
      else {
        if (!IntUtils::isSafeInt<T>(in))
          return kResultUnsafeInt64Conversion;
        out = v8::Number::New(v8GetIsolateOfContext(ctx), static_cast<double>(in));
      }

      return !out.IsEmpty() ? kResultOk : kResultBypass;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsNumber())
        return kResultInvalidValue;

      return IntUtils::doubleToInt64(v8::Number::Cast(*in)->Value(), out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, kTraitIdUnsafeUint> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      // The same trick as in kTraitIdUnsafeInt.
      if (in <= static_cast<T>(TypeTraits<uint32_t>::maxValue())) {
        out = v8::Integer::NewFromUnsigned(v8GetIsolateOfContext(ctx), static_cast<uint32_t>(in));
      }
      else {
        if (!IntUtils::isSafeInt<T>(in))
          return kResultUnsafeInt64Conversion;
        out = v8::Number::New(v8GetIsolateOfContext(ctx), static_cast<double>(in));
      }
      return !out.IsEmpty() ? kResultOk : kResultBypass;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsNumber())
        return kResultInvalidValue;

      return IntUtils::doubleToUint64(v8::Number::Cast(*in)->Value(), out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, kTraitIdFloat> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      out = v8::Number::New(v8GetIsolateOfContext(ctx), static_cast<double>(in));
      return kResultOk;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsNumber())
        return kResultInvalidValue;

      out = static_cast<T>(v8::Number::Cast(*in)->Value());
      return kResultOk;
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, kTraitIdDouble> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      out = v8::Number::New(v8GetIsolateOfContext(ctx), in);
      return kResultOk;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsNumber())
        return kResultInvalidValue;

      out = v8::Number::Cast(*in)->Value();
      return kResultOk;
    }
  };

  template<typename T, typename Concept, unsigned int ConceptType>
  struct V8ConvertWithConceptImpl {
  };

  template<typename T, typename Concept>
  struct V8ConvertWithConceptImpl<T, Concept, kConceptSerializer> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, Value& out, const Concept& concept) NJS_NOEXCEPT {
      return concept.serialize(ctx, in, out);
    }

    static NJS_INLINE Result unpack(Context& ctx, const Value& in, T& out, const Concept& concept) NJS_NOEXCEPT {
      return concept.deserialize(ctx, in, out);
    }
  };

  template<typename T, typename Concept>
  struct V8ConvertWithConceptImpl<T, Concept, kConceptValidator> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, Value& out, const Concept& concept) NJS_NOEXCEPT {
      NJS_CHECK(concept.validate(in));
      return V8ConvertImpl<T, TypeTraits<T>::kTraitId>::pack(ctx, in, out);
    }

    static NJS_INLINE Result unpack(Context& ctx, const Value& in, T& out, const Concept& concept) NJS_NOEXCEPT {
      NJS_CHECK(V8ConvertImpl<T, TypeTraits<T>::kTraitId>::unpack(ctx, v8HandleOfValue(in), out));
      return concept.validate(out);
    }
  };

  // Helpers to unpack a primitive value of type `T` from V8's `Value` handle. All
  // specializations MUST BE forward-declared here.
  template<typename T>
  NJS_INLINE Result v8UnpackValue(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
    return V8ConvertImpl<T, TypeTraits<T>::kTraitId>::unpack(ctx, in, out);
  }
  // Handle to handle (no conversion).
  template<>
  NJS_INLINE Result v8UnpackValue(Context& ctx, const v8::Local<v8::Value>& in, Value& out) NJS_NOEXCEPT {
    v8HandleOfValue(out) = in;
    return kResultOk;
  }
  // Handle to handle (no conversion).
  template<>
  NJS_INLINE Result v8UnpackValue(Context& ctx, const v8::Local<v8::Value>& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
    out = in;
    return kResultOk;
  }

  template<typename T, typename Concept>
  NJS_INLINE Result v8UnpackWithConcept(Context& ctx, const Value& in, T& out, const Concept& concept) NJS_NOEXCEPT {
    return V8ConvertWithConceptImpl<T, Concept, Concept::kConceptType>::unpack(ctx, in, out, concept);
  }

  template<typename T>
  struct V8ReturnImpl {
    static NJS_INLINE Result set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const T& value) NJS_NOEXCEPT {
      v8::Local<v8::Value> intermediate;
      NJS_CHECK(V8ConvertImpl<T, TypeTraits<T>::kTraitId>::pack(ctx, value, intermediate));

      rv.Set(intermediate);
      return kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl< v8::Local<v8::Value> > {
    static NJS_INLINE Result set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const v8::Local<v8::Value>& value) NJS_NOEXCEPT {
      rv.Set(value);
      return kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl<Value> {
    static NJS_INLINE Result set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const Value& value) NJS_NOEXCEPT {
      rv.Set(v8HandleOfValue(value));
      return kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl<NullType> {
    static NJS_INLINE Result set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const NullType&) NJS_NOEXCEPT {
      rv.SetNull();
      return kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl<UndefinedType> {
    static NJS_INLINE Result set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const UndefinedType&) NJS_NOEXCEPT {
      rv.SetUndefined();
      return kResultOk;
    }
  };

  template<typename T>
  NJS_INLINE Result V8SetReturn(Context& ctx, v8::ReturnValue<v8::Value>& rv, const T& value) NJS_NOEXCEPT {
    return V8ReturnImpl<T>::set(ctx, rv, value);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result v8SetReturnWithConcept(Context& ctx, v8::ReturnValue<v8::Value>& rv, const T& value, const Concept& concept) NJS_NOEXCEPT {
    Value intermediate;
    NJS_CHECK(V8ConvertWithConceptImpl<T, Concept, Concept::kConceptType>::pack(ctx, value, intermediate, concept));

    rv.Set(v8HandleOfValue(intermediate));
    return kResultOk;
  }

  template<typename NativeT>
  NJS_INLINE Result v8WrapNative(Context& ctx, v8::Local<v8::Object> obj, NativeT* self) NJS_NOEXCEPT;

  template<typename NativeT>
  NJS_INLINE NativeT* v8UnwrapNative(Context& ctx, v8::Local<v8::Object> obj) NJS_NOEXCEPT;

  // Propagates `result` and its `payload` into the underlying VM.
  static NJS_NOINLINE void v8ReportError(Context& ctx, Result result, const ResultPayload& payload) NJS_NOEXCEPT;
} // Internal namespace

// ============================================================================
// [njs::Runtime]
// ============================================================================

class Runtime {
public:
  NJS_INLINE Runtime() NJS_NOEXCEPT : _isolate(NULL) {}
  NJS_INLINE Runtime(const Runtime& other) NJS_NOEXCEPT : _isolate(other._isolate) {}
  explicit NJS_INLINE Runtime(v8::Isolate* isolate) NJS_NOEXCEPT : _isolate(isolate) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE v8::Isolate* v8GetIsolate() const NJS_NOEXCEPT { return _isolate; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  v8::Isolate* _isolate;
};

// ============================================================================
// [njs::Value]
// ============================================================================

class Value {
public:
  typedef v8::Local<v8::Value> HandleType;

  NJS_INLINE Value() NJS_NOEXCEPT : _handle() {}
  NJS_INLINE Value(const Value& other) NJS_NOEXCEPT : _handle(other._handle) {}

  template<typename T>
  explicit NJS_INLINE Value(const v8::Local<T>& handle) NJS_NOEXCEPT : _handle(handle) {}

  // --------------------------------------------------------------------------
  // [Assignment]
  // --------------------------------------------------------------------------

  NJS_INLINE Value& operator=(const Value& other) NJS_NOEXCEPT {
    _handle = other._handle;
    return *this;
  }

  template<typename T>
  NJS_INLINE Value& operator=(const v8::Local<T>& handle) NJS_NOEXCEPT {
    _handle = handle;
    return *this;
  }

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  // Get the V8's handle.
  NJS_INLINE HandleType& v8GetHandle() NJS_NOEXCEPT { return _handle; }
  NJS_INLINE const HandleType& v8GetHandle() const NJS_NOEXCEPT { return _handle; }

  template<typename T>
  NJS_INLINE v8::Local<T> v8GetHandleAs() const NJS_NOEXCEPT { return v8::Local<T>::Cast(_handle); }

  template<typename T = v8::Value>
  NJS_INLINE T* v8GetValue() const NJS_NOEXCEPT { return T::Cast(*_handle); }

  // --------------------------------------------------------------------------
  // [Handle]
  // --------------------------------------------------------------------------

  // Get whether the handle is valid and can be used. If the handle is invalid
  // then no member function can be called on it. There are asserts that check
  // this in debug mode.
  NJS_INLINE bool isValid() const NJS_NOEXCEPT { return !_handle.IsEmpty(); }

  // Reset the handle.
  //
  // NOTE: `isValid()` will return `false` after `reset()` is called.
  NJS_INLINE void reset() NJS_NOEXCEPT { _handle.Clear(); }

  // --------------------------------------------------------------------------
  // [Type System]
  // --------------------------------------------------------------------------

  NJS_INLINE bool isUndefined() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsUndefined();
  }

  NJS_INLINE bool isNull() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsNull();
  }

  NJS_INLINE bool isBool() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsBoolean();
  }

  NJS_INLINE bool isTrue() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsTrue();
  }

  NJS_INLINE bool isFalse() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsFalse();
  }

  NJS_INLINE bool isInt32() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsInt32();
  }

  NJS_INLINE bool isUint32() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsUint32();
  }

  NJS_INLINE bool isNumber() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsNumber();
  }

  NJS_INLINE bool isString() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsString();
  }

  NJS_INLINE bool isSymbol() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsSymbol();
  }

  NJS_INLINE bool isArray() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsArray();
  }

  NJS_INLINE bool isObject() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsObject();
  }

  NJS_INLINE bool isDate() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsDate();
  }

  NJS_INLINE bool isRegExp() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsRegExp();
  }

  NJS_INLINE bool isFunction() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsFunction();
  }

  NJS_INLINE bool isExternal() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsExternal();
  }

  NJS_INLINE bool isPromise() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsPromise();
  }

  NJS_INLINE bool isArrayBuffer() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsArrayBuffer();
  }

  NJS_INLINE bool isArrayBufferView() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsArrayBufferView();
  }

  NJS_INLINE bool isDataView() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsDataView();
  }

  NJS_INLINE bool isTypedArray() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsTypedArray();
  }

  NJS_INLINE bool isInt8Array() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsInt8Array();
  }

  NJS_INLINE bool isInt16Array() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsInt16Array();
  }

  NJS_INLINE bool isInt32Array() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsInt32Array();
  }

  NJS_INLINE bool isUint8Array() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsUint8Array();
  }

  NJS_INLINE bool isUint8ClampedArray() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsUint8ClampedArray();
  }

  NJS_INLINE bool isUint16Array() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsUint16Array();
  }

  NJS_INLINE bool isUint32Array() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsUint32Array();
  }

  NJS_INLINE bool isFloat32Array() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsFloat32Array();
  }

  NJS_INLINE bool isFloat64Array() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    return _handle->IsFloat64Array();
  }

  // --------------------------------------------------------------------------
  // [Casting]
  // --------------------------------------------------------------------------

  // IMPORTANT: These can only be called if the value can be casted to such types,
  // casting improper type will fail in debug more and cause an apocalypse in
  // release.

  NJS_INLINE bool boolValue() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isBool());
    return v8GetValue<v8::Boolean>()->BooleanValue();
  }

  NJS_INLINE int32_t int32Value() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isInt32());
    return v8GetValue<v8::Int32>()->Value();
  }

  NJS_INLINE uint32_t uint32Value() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isUint32());
    return v8GetValue<v8::Uint32>()->Value();
  }

  NJS_INLINE double doubleValue() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isNumber());
    return v8GetValue<v8::Number>()->Value();
  }

  // --------------------------------------------------------------------------
  // [String-Specific Functionality]
  // --------------------------------------------------------------------------

  NJS_INLINE bool isLatin1() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isString());
    return v8GetValue<v8::String>()->ContainsOnlyOneByte();
  }

  NJS_INLINE bool isLatin1Guess() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isString());
    return v8GetValue<v8::String>()->IsOneByte();
  }

  // Length of the string if represented as UTF-16 or LATIN-1 (if representable).
  NJS_INLINE size_t getStringLength() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isString());
    return v8GetValue<v8::String>()->Length();
  }

  // Length of the string if represented as UTF-8.
  NJS_INLINE size_t getUtf8Length() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isString());
    return v8GetValue<v8::String>()->Utf8Length();
  }

  NJS_INLINE int readLatin1(char* out, int len = -1) const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isString());
    return v8GetValue<v8::String>()->WriteOneByte(
      reinterpret_cast<uint8_t*>(out), 0, len, v8::String::NO_NULL_TERMINATION);
  }

  NJS_INLINE int readUtf8(char* out, int len = -1) const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isString());
    return v8GetValue<v8::String>()->WriteUtf8(
      out, len, NULL, v8::String::NO_NULL_TERMINATION);
  }

  NJS_INLINE int readUtf16(uint16_t* out, int len = -1) const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isString());
    return v8GetValue<v8::String>()->Write(
      out, 0, len, v8::String::NO_NULL_TERMINATION);
  }

  // --------------------------------------------------------------------------
  // [Array Functionality]
  // --------------------------------------------------------------------------

  NJS_INLINE size_t getArrayLength() const NJS_NOEXCEPT {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isArray());
    return v8GetValue<v8::Array>()->Length();
  }
  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  HandleType _handle;
};

namespace Internal {
  static NJS_INLINE v8::Local<v8::Value>& v8HandleOfValue(Value& value) NJS_NOEXCEPT { return value._handle; }
  static NJS_INLINE const v8::Local<v8::Value>& v8HandleOfValue(const Value& value) NJS_NOEXCEPT { return value._handle; }
} // Internal namsepace

// ============================================================================
// [njs::Persistent]
// ============================================================================

class Persistent {
public:
  NJS_INLINE Persistent() NJS_NOEXCEPT
    : _handle() {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  // Get the V8's handle.
  NJS_INLINE v8::Persistent<v8::Value>& v8GetHandle() NJS_NOEXCEPT {
    return _handle;
  }

  NJS_INLINE const v8::Persistent<v8::Value>& v8GetHandle() const NJS_NOEXCEPT {
    return _handle;
  }

  // Get the V8's `Persistent<T>` handle.
  template<typename T>
  NJS_INLINE v8::Persistent<T>& v8GetHandleAs() NJS_NOEXCEPT {
    return reinterpret_cast<v8::Persistent<T>&>(_handle);
  }

  template<typename T>
  NJS_INLINE const v8::Persistent<T>& v8GetHandleAs() const NJS_NOEXCEPT {
    return reinterpret_cast<const v8::Persistent<T>&>(_handle);
  }

  template<typename T = v8::Value>
  NJS_INLINE T* v8GetValue() const NJS_NOEXCEPT {
    return T::Cast(*_handle);
  }

  // --------------------------------------------------------------------------
  // [Handle]
  // --------------------------------------------------------------------------

  // Get whether the handle is valid and can be used. If the handle is invalid
  // then no member function can be called on it. There are asserts that check
  // this in debug mode.
  NJS_INLINE bool isValid() const NJS_NOEXCEPT { return !_handle.IsEmpty(); }

  // Reset the handle to its construction state.
  //
  // NOTE: `isValid()` returns return `false` after `reset()` is called.
  NJS_INLINE void reset() NJS_NOEXCEPT { _handle.Empty(); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  v8::Persistent<v8::Value> _handle;
};

// ============================================================================
// [njs::Context]
// ============================================================================

class Context {
  // No assignment.
  NJS_INLINE Context& operator=(const Context& other) NJS_NOEXCEPT;

public:
  NJS_INLINE Context() NJS_NOEXCEPT {}

  NJS_INLINE Context(const Context& other) NJS_NOEXCEPT
    : _runtime(other._runtime),
      _context(other._context) {}

  explicit NJS_INLINE Context(const Runtime& runtime) NJS_NOEXCEPT
    : _runtime(runtime),
      _context(runtime.v8GetIsolate()->GetCurrentContext()) {}

  NJS_INLINE Context(v8::Isolate* isolate, const v8::Local<v8::Context>& context) NJS_NOEXCEPT
    : _runtime(isolate),
      _context(context) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE v8::Isolate* v8GetIsolate() const NJS_NOEXCEPT { return _runtime._isolate; }
  NJS_INLINE v8::Local<v8::Context>& v8GetContext() NJS_NOEXCEPT { return _context; }
  NJS_INLINE const v8::Local<v8::Context>& v8GetContext() const NJS_NOEXCEPT { return _context; }

  // --------------------------------------------------------------------------
  // [Runtime]
  // --------------------------------------------------------------------------

  NJS_INLINE const Runtime& getRuntime() const NJS_NOEXCEPT { return _runtime; }

  // --------------------------------------------------------------------------
  // [Built-Ins]
  // --------------------------------------------------------------------------

  NJS_INLINE Value undefined() const NJS_NOEXCEPT { return Value(v8::Undefined(v8GetIsolate())); }
  NJS_INLINE Value null() const NJS_NOEXCEPT { return Value(v8::Null(v8GetIsolate())); }
  NJS_INLINE Value true_() const NJS_NOEXCEPT { return Value(v8::True(v8GetIsolate())); }
  NJS_INLINE Value false_() const NJS_NOEXCEPT { return Value(v8::False(v8GetIsolate())); }

  // --------------------------------------------------------------------------
  // [New]
  // --------------------------------------------------------------------------

  NJS_INLINE Value newBool(bool value) NJS_NOEXCEPT { return Value(v8::Boolean::New(v8GetIsolate(), value)); }
  NJS_INLINE Value newInt32(int32_t value) NJS_NOEXCEPT { return Value(v8::Integer::New(v8GetIsolate(), value)); }
  NJS_INLINE Value newUint32(uint32_t value) NJS_NOEXCEPT { return Value(v8::Integer::New(v8GetIsolate(), value)); }
  NJS_INLINE Value newDouble(double value) NJS_NOEXCEPT { return Value(v8::Number::New(v8GetIsolate(), value)); }
  NJS_INLINE Value newArray() NJS_NOEXCEPT { return Value(v8::Array::New(v8GetIsolate())); }
  NJS_INLINE Value newObject() NJS_NOEXCEPT { return Value(v8::Object::New(v8GetIsolate())); }

  template<typename StrRefT>
  NJS_INLINE Value newString(const StrRefT& data) NJS_NOEXCEPT {
    return Value(Internal::v8NewString<StrRefT>(*this, data, v8::NewStringType::kNormal));
  }

  template<typename StrRefT>
  NJS_INLINE Value newInternalizedString(const StrRefT& data) NJS_NOEXCEPT {
    return Value(Internal::v8NewString<StrRefT>(*this, data, v8::NewStringType::kInternalized));
  }

  NJS_INLINE Value newEmptyString() const NJS_NOEXCEPT { return Value(v8::String::Empty(v8GetIsolate())); }

  template<typename T>
  NJS_INLINE Value newValue(const T& value) NJS_NOEXCEPT {
    Value result;
    Internal::V8ConvertImpl<T, Internal::TypeTraits<T>::kTraitId>::pack(*this, value, result._handle);
    return result;
  }

  // --------------------------------------------------------------------------
  // [Unpacking]
  // --------------------------------------------------------------------------

  // These don't perform any conversion, the type of the JS value has to match
  // the type of the `out` argument. Some types are internally convertible as
  // JS doesn't differentiate between integer and floating point types.

  template<typename T>
  NJS_INLINE Result unpack(const Value& in, T& out) NJS_NOEXCEPT {
    return Internal::v8UnpackValue<T>(*this, in._handle, out);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result unpack(const Value& in, T& out, const Concept& concept) NJS_NOEXCEPT {
    return Internal::v8UnpackWithConcept<T, Concept>(*this, in, out, concept);
  }

  // --------------------------------------------------------------------------
  // [Wrap / Unwrap]
  // --------------------------------------------------------------------------

  template<typename NativeT>
  NJS_INLINE Result wrap(Value obj, NativeT* native) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isObject());

    return Internal::v8WrapNative<NativeT>(*this, obj.v8GetHandleAs<v8::Object>(), native);
  }

  template<typename NativeT, typename ...Args>
  NJS_INLINE Result wrapNew(Value obj, Args... args) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isObject());

    NativeT* native = new (std::nothrow) NativeT(args...);
    if (!native)
      return kResultOutOfMemory;

    return Internal::v8WrapNative<NativeT>(*this, obj.v8GetHandleAs<v8::Object>(), native);
  }

  template<typename NativeT>
  NJS_INLINE NativeT* unwrap(Value obj) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());
    return Internal::v8UnwrapNative<NativeT>(*this, obj.v8GetHandleAs<v8::Object>());
  }

  // --------------------------------------------------------------------------
  // [JS Features]
  // --------------------------------------------------------------------------

  NJS_INLINE Maybe<bool> equals(const Value& aAny, const Value& bAny) NJS_NOEXCEPT {
    NJS_ASSERT(aAny.isValid());
    NJS_ASSERT(bAny.isValid());

    v8::Maybe<bool> result = aAny._handle->Equals(_context, bAny._handle);
    return Maybe<bool>(result.IsJust() ? kResultOk : kResultBypass, result.FromMaybe(false));
  }

  NJS_INLINE bool strictEquals(const Value& aAny, const Value& bAny) const NJS_NOEXCEPT {
    NJS_ASSERT(aAny.isValid());
    NJS_ASSERT(bAny.isValid());
    return aAny._handle->StrictEquals(bAny._handle);
  }

  NJS_INLINE bool isSameValue(const Value& aAny, const Value& bAny) const NJS_NOEXCEPT {
    NJS_ASSERT(aAny.isValid());
    NJS_ASSERT(bAny.isValid());
    return aAny._handle->SameValue(bAny._handle);
  }

  // Concatenate two strings.
  NJS_INLINE Value concatStrings(const Value& aStr, const Value& bStr) NJS_NOEXCEPT {
    NJS_ASSERT(aStr.isValid());
    NJS_ASSERT(aStr.isString());
    NJS_ASSERT(bStr.isValid());
    NJS_ASSERT(bStr.isString());

    return Value(
      v8::String::Concat(
        aStr.v8GetHandleAs<v8::String>(),
        bStr.v8GetHandleAs<v8::String>()));
  }

  NJS_INLINE Maybe<bool> hasProperty(const Value& obj, const Value& key) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());
    NJS_ASSERT(key.isValid());

    v8::Maybe<bool> result = obj.v8GetValue<v8::Object>()->Has(_context, key._handle);
    return Maybe<bool>(result.IsJust() ? kResultOk : kResultBypass, result.FromMaybe(false));
  }

  NJS_INLINE Maybe<bool> hasPropertyAt(const Value& obj, uint32_t index) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());

    v8::Maybe<bool> result = obj.v8GetValue<v8::Object>()->Has(_context, index);
    return Maybe<bool>(result.IsJust() ? kResultOk : kResultBypass, result.FromMaybe(false));
  }

  NJS_INLINE Value getProperty(const Value& obj, const Value& key) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());
    NJS_ASSERT(key.isValid());

    return Value(
      Internal::v8LocalFromMaybe(
        obj.v8GetValue<v8::Object>()->Get(_context, key._handle)));
  }

  template<typename StrRefT>
  NJS_NOINLINE Value getProperty(const Value& obj, const StrRefT& key) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());

    njs::Value keyValue = newString(key);
    if (!keyValue.isValid())
      return keyValue;

    return Value(
      Internal::v8LocalFromMaybe(
        obj.v8GetValue<v8::Object>()->Get(_context, keyValue._handle)));
  }

  NJS_INLINE Value getPropertyAt(const Value& obj, uint32_t index) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());

    return Value(
      Internal::v8LocalFromMaybe(
        obj.v8GetValue<v8::Object>()->Get(_context, index)));
  }

  NJS_INLINE Result setProperty(const Value& obj, const Value& key, const Value& val) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());
    NJS_ASSERT(key.isValid());
    NJS_ASSERT(val.isValid());

    v8::Maybe<bool> result = obj.v8GetValue<v8::Object>()->Set(_context, key._handle, val._handle);
    return result.FromMaybe(false) ? kResultOk : kResultBypass;
  }

  NJS_INLINE Result setPropertyAt(const Value& obj, uint32_t index, const Value& val) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());
    NJS_ASSERT(val.isValid());

    v8::Maybe<bool> result = obj.v8GetValue<v8::Object>()->Set(_context, index, val._handle);
    return result.FromMaybe(false) ? kResultOk : kResultBypass;
  }

  NJS_INLINE Value newInstance(const Value& ctor) NJS_NOEXCEPT {
    NJS_ASSERT(ctor.isValid());
    NJS_ASSERT(ctor.isFunction());

    return Value(
      Internal::v8LocalFromMaybe(
        ctor.v8GetValue<v8::Function>()->NewInstance(_context)));
  }

  template<typename ...Args>
  NJS_INLINE Value newInstance(const Value& ctor, Args... args) NJS_NOEXCEPT {
    Value argv[] = { args... };

    return Value(
      Internal::v8LocalFromMaybe(
        ctor.v8GetValue<v8::Function>()->NewInstance(
          _context,
          static_cast<int>(sizeof(argv) / sizeof(argv[0])),
          reinterpret_cast<v8::Local<v8::Value>*>(argv))));
  }

  NJS_INLINE Value newInstanceArgv(const Value& ctor, size_t argc, const Value* argv) NJS_NOEXCEPT {
    NJS_ASSERT(ctor.isValid());
    NJS_ASSERT(ctor.isFunction());

    return Value(
      Internal::v8LocalFromMaybe(
        ctor.v8GetValue<v8::Function>()->NewInstance(
          _context,
          static_cast<int>(argc),
          const_cast<v8::Local<v8::Value>*>(reinterpret_cast<const v8::Local<v8::Value>*>(argv)))));
  }

  NJS_INLINE Value call(const Value& function, const Value& recv) NJS_NOEXCEPT {
    return callArgv(function, recv, 0, NULL);
  }

  template<typename ...Args>
  NJS_INLINE Value call(const Value& function, const Value& recv, Args... args) NJS_NOEXCEPT {
    Value argv[] = { args... };
    return callArgv(function, recv, sizeof(argv) / sizeof(argv[0]), argv);
  }

  NJS_INLINE Value callArgv(const Value& function, const Value& recv, size_t argc, const Value* argv) NJS_NOEXCEPT {
    NJS_ASSERT(function.isValid());
    NJS_ASSERT(function.isFunction());
    NJS_ASSERT(recv.isValid());

    return Value(
      Internal::v8LocalFromMaybe(
        function.v8GetValue<v8::Function>()->Call(
          _context, recv._handle,
          static_cast<int>(argc),
          const_cast<v8::Local<v8::Value>*>(reinterpret_cast<const v8::Local<v8::Value>*>(argv)))));
  }

  // --------------------------------------------------------------------------
  // [Exception]
  // --------------------------------------------------------------------------

  NJS_NOINLINE Value newException(uint32_t type, Value msg) NJS_NOEXCEPT {
    NJS_ASSERT(msg.isString());
    switch (type) {
      default:
      case kExceptionError         : return Value(v8::Exception::Error(msg.v8GetHandleAs<v8::String>()));
      case kExceptionTypeError     : return Value(v8::Exception::TypeError(msg.v8GetHandleAs<v8::String>()));
      case kExceptionRangeError    : return Value(v8::Exception::RangeError(msg.v8GetHandleAs<v8::String>()));
      case kExceptionSyntaxError   : return Value(v8::Exception::SyntaxError(msg.v8GetHandleAs<v8::String>()));
      case kExceptionReferenceError: return Value(v8::Exception::ReferenceError(msg.v8GetHandleAs<v8::String>()));
    }
  }

  // --------------------------------------------------------------------------
  // [Throw]
  // --------------------------------------------------------------------------

  NJS_INLINE Result Throw(Value exception) NJS_NOEXCEPT {
    v8GetIsolate()->ThrowException(exception._handle);
    return kResultBypass;
  }

  NJS_NOINLINE Result throwNewException(uint32_t type, Value msg) NJS_NOEXCEPT {
    NJS_ASSERT(msg.isString());
    Value exception = newException(type, msg);
    v8GetIsolate()->ThrowException(exception._handle);
    return kResultBypass;
  }

  NJS_NOINLINE Result throwNewException(uint32_t type, const char* fmt, ...) NJS_NOEXCEPT {
    char buffer[kMaxBufferSize];

    va_list ap;
    va_start(ap, fmt);
    unsigned int length = StrUtils::vsformat(buffer, kMaxBufferSize, fmt, ap);
    va_end(ap);

    Value exception = newException(type, newString(Utf8Ref(buffer, length)));
    v8GetIsolate()->ThrowException(exception._handle);
    return kResultBypass;
  }

  // --------------------------------------------------------------------------
  // [Local <-> Persistent]
  // --------------------------------------------------------------------------

  NJS_INLINE Value makeLocal(const Persistent& persistent) NJS_NOEXCEPT {
    return Value(v8::Local<v8::Value>::New(v8GetIsolate(), persistent._handle));
  }

  NJS_INLINE Result makePersistent(const Value& local, Persistent& persistent) NJS_NOEXCEPT {
    persistent._handle.Reset(v8GetIsolate(), local._handle);
    return kResultOk;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! V8's isolate.
  Runtime _runtime;
  //! V8's context.
  v8::Local<v8::Context> _context;
};

static NJS_INLINE v8::Isolate* Internal::v8GetIsolateOfContext(Context& ctx) NJS_NOEXCEPT {
  return ctx.v8GetIsolate();
}

// ============================================================================
// [njs::HandleScope]
// ============================================================================

class HandleScope {
  NJS_NONCOPYABLE(HandleScope)

public:
  explicit NJS_INLINE HandleScope(const Context& ctx) NJS_NOEXCEPT
    : _handleScope(ctx.v8GetIsolate()) {}

  explicit NJS_INLINE HandleScope(const Runtime& runtime) NJS_NOEXCEPT
    : _handleScope(runtime.v8GetIsolate()) {}

  explicit NJS_INLINE HandleScope(v8::Isolate* isolate) NJS_NOEXCEPT
    : _handleScope(isolate) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE v8::HandleScope& v8GetHandleScope() NJS_NOEXCEPT { return _handleScope; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! V8's handle scope.
  v8::HandleScope _handleScope;
};

// ============================================================================
// [njs::ScopedContext]
// ============================================================================

class ScopedContext
  : public HandleScope,
    public Context {
  NJS_NONCOPYABLE(ScopedContext)

public:
  NJS_INLINE ScopedContext(const Context& other) NJS_NOEXCEPT
    : HandleScope(other),
      Context(other) {}

  explicit NJS_INLINE ScopedContext(const Runtime& runtime) NJS_NOEXCEPT
    : HandleScope(runtime),
      Context(runtime) {}

  NJS_INLINE ScopedContext(v8::Isolate* isolate, const v8::Local<v8::Context>& handle) NJS_NOEXCEPT
    : HandleScope(isolate),
      Context(isolate, handle) {}
};

// ============================================================================
// [njs::ExecutionContext]
// ============================================================================

class ExecutionContext
  : public Context,
    public ResultMixin {
  NJS_NONCOPYABLE(ExecutionContext)

public:
  NJS_INLINE ExecutionContext() NJS_NOEXCEPT : Context() {}
  NJS_INLINE ExecutionContext(const Context& other) NJS_NOEXCEPT : Context(other) {}
  NJS_INLINE ExecutionContext(v8::Isolate* isolate, const v8::Local<v8::Context>& handle) NJS_NOEXCEPT : Context(isolate, handle) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  //! Handle a `result` returned from a native function (binding).
  NJS_INLINE void _handleResult(Result result) NJS_NOEXCEPT {
    if (result != kResultOk)
      Internal::v8ReportError(*this, result, _payload);
  }
};

// ============================================================================
// [njs::GetPropertyContext]
// ============================================================================

class GetPropertyContext : public ExecutionContext {
public:
  // Creates the GetPropertyContext directly from V8's `PropertyCallbackInfo<Value>`.
  NJS_INLINE GetPropertyContext(const v8::PropertyCallbackInfo<v8::Value>& info) NJS_NOEXCEPT
    : ExecutionContext(info.GetIsolate(), info.GetIsolate()->GetCurrentContext()),
      _info(info) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE const v8::PropertyCallbackInfo<v8::Value>& v8GetCallbackInfo() const NJS_NOEXCEPT { return _info; }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_INLINE Value This() const NJS_NOEXCEPT { return Value(_info.This()); }

  // --------------------------------------------------------------------------
  // [Return]
  // --------------------------------------------------------------------------

  template<typename T>
  NJS_INLINE Result returnValue(const T& value) NJS_NOEXCEPT {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return Internal::V8SetReturn<T>(static_cast<Context&>(*this), rv, value);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result returnValue(const T& value, const Concept& concept) NJS_NOEXCEPT {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return Internal::v8SetReturnWithConcept<T, Concept>(*this, rv, value, concept);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------
  const v8::PropertyCallbackInfo<v8::Value>& _info;
};

// ============================================================================
// [njs::SetPropertyContext]
// ============================================================================

class SetPropertyContext : public ExecutionContext {
public:
  // Creates the SetPropertyContext directly from V8's `PropertyCallbackInfo<void>`.
  NJS_INLINE SetPropertyContext(const v8::PropertyCallbackInfo<void>& info,
                                const v8::Local<v8::Value>& value) NJS_NOEXCEPT
    : ExecutionContext(info.GetIsolate(), info.GetIsolate()->GetCurrentContext()),
      _info(info),
      _propertyValue(value) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE const v8::PropertyCallbackInfo<void>& v8GetCallbackInfo() const NJS_NOEXCEPT { return _info; }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_INLINE Value This() const NJS_NOEXCEPT { return Value(_info.This()); }
  NJS_INLINE Value propertyValue() const NJS_NOEXCEPT { return _propertyValue; }

  template<typename T>
  NJS_INLINE Result unpackValue(T& out) NJS_NOEXCEPT {
    return Internal::v8UnpackValue<T>(*this, _propertyValue._handle, out);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result unpackValue(T& out, const Concept& concept) NJS_NOEXCEPT {
    return Internal::v8UnpackWithConcept<T, Concept>(*this, _propertyValue, out, concept);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const v8::PropertyCallbackInfo<void>& _info;
  Value _propertyValue;
};

// ============================================================================
// [njs::FunctionCallContext]
// ============================================================================

class FunctionCallContext : public ExecutionContext {
public:
  // Creates the FunctionCallContext directly from V8's `FunctionCallbackInfo<Value>`.
  NJS_INLINE FunctionCallContext(const v8::FunctionCallbackInfo<v8::Value>& info) NJS_NOEXCEPT
    : ExecutionContext(info.GetIsolate(), info.GetIsolate()->GetCurrentContext()),
      _info(info) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE const v8::FunctionCallbackInfo<v8::Value>& v8GetCallbackInfo() const NJS_NOEXCEPT {
    return _info;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_INLINE Value This() const NJS_NOEXCEPT { return Value(_info.This()); }
  NJS_INLINE bool isConstructCall() const NJS_NOEXCEPT { return _info.IsConstructCall(); }

  // --------------------------------------------------------------------------
  // [Arguments]
  // --------------------------------------------------------------------------

  NJS_INLINE unsigned int getArgumentsLength() const NJS_NOEXCEPT {
    return static_cast<unsigned int>(_info.Length());
  }

  NJS_INLINE Result verifyArgumentsLength(unsigned int n) NJS_NOEXCEPT {
    return getArgumentsLength() != n ? invalidArgumentsLength(n)
                                     : static_cast<Result>(kResultOk);
  }

  NJS_INLINE Value getArgument(unsigned int index) const NJS_NOEXCEPT {
    return Value(_info.operator[](static_cast<int>(index)));
  }

  // Like `unpack()`, but accepts the argument index instead of the `Value`.
  template<typename T>
  NJS_INLINE Result unpackArgument(unsigned int index, T& out) NJS_NOEXCEPT {
    return Internal::v8UnpackValue<T>(*this, _info[static_cast<int>(index)], out);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result unpackArgument(unsigned int index, T& out, const Concept& concept) NJS_NOEXCEPT {
    return Internal::v8UnpackWithConcept<T, Concept>(*this, getArgument(index), out, concept);
  }

  // --------------------------------------------------------------------------
  // [Return]
  // --------------------------------------------------------------------------

  template<typename T>
  NJS_INLINE Result returnValue(const T& value) NJS_NOEXCEPT {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return Internal::V8SetReturn<T>(static_cast<Context&>(*this), rv, value);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result returnValue(const T& value, const Concept& concept) NJS_NOEXCEPT {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return Internal::v8SetReturnWithConcept<T, Concept>(*this, rv, value, concept);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const v8::FunctionCallbackInfo<v8::Value>& _info;
};

// ============================================================================
// [njs::ConstructCallContext]
// ============================================================================

class ConstructCallContext : public FunctionCallContext {
public:
  // Creates the `ConstructCallContext` directly from V8's `FunctionCallbackInfo<Value>`.
  NJS_INLINE ConstructCallContext(const v8::FunctionCallbackInfo<v8::Value>& info) NJS_NOEXCEPT
    : FunctionCallContext(info) {}

  // --------------------------------------------------------------------------
  // [ReturnWrap / ReturnNew]
  // --------------------------------------------------------------------------

  template<typename NativeT>
  NJS_INLINE Result returnWrap(NativeT* native) NJS_NOEXCEPT {
    Value obj = This();

    NJS_CHECK(wrap<NativeT>(obj, native));
    return returnValue(obj);
  }

  template<typename NativeT, typename ...Args>
  NJS_INLINE Result returnNew(Args... args) NJS_NOEXCEPT {
    Value obj = This();

    NJS_CHECK(wrapNew<NativeT>(obj, args...));
    return returnValue(obj);
  }
};

// ============================================================================
// [njs::Internal::WrapData]
// ============================================================================

namespace Internal {
  // A low-level C++ class wrapping functionality.
  //
  // Very similar to `node::ObjectWrap` with few differences:
  //   - It isn't a base class, it's used as a member instead.
  //   - It can be used to wrap any class, even those you can only inherit from.
  //   - It doesn't have virtual functions and is not intended to be inherited.
  //
  // An instance of `WrapData` should always be named `_njsData` in the class
  // it's used.
  class WrapData {
  public:
    NJS_INLINE WrapData() NJS_NOEXCEPT
      : _refCount(0),
        _object(),
        _destroyCallback(NULL) {}

    NJS_INLINE WrapData(Context& ctx, Value obj, Internal::V8WrapDestroyCallback destroyCallback) NJS_NOEXCEPT
      : _refCount(0),
        _destroyCallback(destroyCallback) {
      NJS_ASSERT(obj.isObject());
      _object._handle.Reset(ctx.v8GetIsolate(), obj._handle);
      obj.v8GetValue<v8::Object>()->SetAlignedPointerInInternalField(0, this);
    }

    NJS_NOINLINE ~WrapData() NJS_NOEXCEPT {
      if (_object.isValid()) {
        NJS_ASSERT(_object._handle.IsNearDeath());
        _object._handle.ClearWeak();
        _object._handle.Reset();
      }
    }

    // ------------------------------------------------------------------------
    // [V8-Specific]
    // ------------------------------------------------------------------------

    NJS_INLINE v8::Persistent<v8::Value>& v8GetHandle() NJS_NOEXCEPT { return _object._handle; }
    NJS_INLINE const v8::Persistent<v8::Value>& v8GetHandle() const NJS_NOEXCEPT { return _object._handle; }

    // ------------------------------------------------------------------------
    // [Handle]
    // ------------------------------------------------------------------------

    NJS_INLINE bool isValid() const NJS_NOEXCEPT { return _object.isValid(); }

    NJS_INLINE Persistent& getObject() NJS_NOEXCEPT { return _object; }
    NJS_INLINE const Persistent& getObject() const NJS_NOEXCEPT { return _object; }

    // ------------------------------------------------------------------------
    // [RefCount]
    // ------------------------------------------------------------------------

    NJS_INLINE size_t refCount() const NJS_NOEXCEPT {
      return _refCount;
    }

    NJS_INLINE void addRef() NJS_NOEXCEPT {
      NJS_ASSERT(isValid());

      _refCount++;
      _object._handle.ClearWeak();
    }

    NJS_INLINE void release(void* self) NJS_NOEXCEPT {
      NJS_ASSERT(_refCount > 0);
      NJS_ASSERT(isValid());
      NJS_ASSERT(!isWeak());

      if (--_refCount == 0)
        makeWeak(self);
    }

    NJS_INLINE bool isWeak() const NJS_NOEXCEPT {
      return _object._handle.IsWeak();
    }

    NJS_INLINE void makeWeak(void* self) NJS_NOEXCEPT {
      NJS_ASSERT(isValid());

      _object._handle.SetWeak<void>(self, _destroyCallback);
      _object._handle.MarkIndependent();
    }

    // ------------------------------------------------------------------------
    // [Statics]
    // ------------------------------------------------------------------------

    template<typename T>
    static void NJS_NOINLINE destroyCallbackT(const v8::WeakCallbackData<v8::Object, T>& data) NJS_NOEXCEPT {
      T* self = data.GetParameter();

      NJS_ASSERT(self->_njsData._refCount == 0);
      NJS_ASSERT(self->_njsData._object._handle.IsNearDeath());

      self->_njsData._object._handle.Reset();
      delete self;
    }

    // ------------------------------------------------------------------------
    // [Members]
    // ------------------------------------------------------------------------

    // Reference count.
    //
    // Value greater than zero counts the number of references on C++ side. If
    // the reference count is zero it means that the object has no references
    // on C++ side and can be garbage collected if there are no more references
    // on JS side as well.
    size_t _refCount;

    // Persistent handle, valid if `_refCount` is non-zero.
    Persistent _object;

    // Destroy callback set by calling `wrap()`.
    Internal::V8WrapDestroyCallback _destroyCallback;
  };
} // Internal namespace

// ============================================================================
// [njs::Internal]
// ============================================================================

namespace Internal {
  template<typename NativeT>
  NJS_INLINE Result v8WrapNative(Context& ctx, v8::Local<v8::Object> obj, NativeT* native) NJS_NOEXCEPT {
    // Should be never called on an already initialized data.
    NJS_ASSERT(!native->_njsData._object.isValid());
    NJS_ASSERT(!native->_njsData._destroyCallback);

    // Ensure the object was properly configured and contains internal fields.
    NJS_ASSERT(obj->InternalFieldCount() > 0);

    native->_njsData._object._handle.Reset(ctx.v8GetIsolate(), obj);
    native->_njsData._destroyCallback =(Internal::V8WrapDestroyCallback)(Internal::WrapData::destroyCallbackT<NativeT>);

    obj->SetAlignedPointerInInternalField(0, native);
    native->_njsData.makeWeak(native);

    return kResultOk;
  }

  template<typename NativeT>
  NJS_INLINE NativeT* v8UnwrapNative(Context& ctx, v8::Local<v8::Object> obj) NJS_NOEXCEPT {
    NJS_ASSERT(obj->InternalFieldCount() > 0);

    // Cast to `NativeT::Base` before casting to `NativeT*`. A direct cast from
    // `void*` to `NativeT` won't work right when `NativeT` has more than one
    // base class.
    void* native = obj->GetAlignedPointerFromInternalField(0);
    return static_cast<NativeT*>(static_cast<typename NativeT::Base*>(native));
  }

  static NJS_NOINLINE Result v8BindClassHelper(
    Context& ctx,
    v8::Local<v8::Object> exports,
    v8::Local<v8::FunctionTemplate> classObj,
    v8::Local<v8::String> className,
    const BindingItem* items, unsigned int count) NJS_NOEXCEPT {

    v8::Local<v8::ObjectTemplate> prototype = classObj->PrototypeTemplate();
    v8::Local<v8::Signature> methodSignature;
    v8::Local<v8::AccessorSignature> accessorSignature;

    for (unsigned int i = 0; i < count; i++) {
      const BindingItem& item = items[i];

      v8::Local<v8::String> name = Internal::v8LocalFromMaybe(
        v8::String::NewFromOneByte(
          ctx.v8GetIsolate(), reinterpret_cast<const uint8_t*>(item.name), v8::NewStringType::kInternalized));

      switch (item.type) {
        case BindingItem::kTypeStatic: {
          v8::Local<v8::FunctionTemplate> fnTemplate = v8::FunctionTemplate::New(
            ctx.v8GetIsolate(), (v8::FunctionCallback)item.data, exports);

          fnTemplate->SetClassName(name);
          classObj->Set(name, fnTemplate);
          break;
        }

        case BindingItem::kTypeMethod: {
          // Signature is only created when needed and then cached.
          if (methodSignature.IsEmpty())
            methodSignature = v8::Signature::New(ctx.v8GetIsolate(), classObj);

          v8::Local<v8::FunctionTemplate> fnTemplate = v8::FunctionTemplate::New(
            ctx.v8GetIsolate(), (v8::FunctionCallback)item.data, exports, methodSignature);

          fnTemplate->SetClassName(name);
          prototype->Set(name, fnTemplate);
          break;
        }

        case BindingItem::kTypeGetter:
        case BindingItem::kTypeSetter: {
          unsigned int pairedType;
          int attr = v8::DontEnum | v8::DontDelete;

          v8::AccessorGetterCallback getter = NULL;
          v8::AccessorSetterCallback setter = NULL;

          if (item.type == BindingItem::kTypeGetter) {
            getter = (v8::AccessorGetterCallback)item.data;
            pairedType = BindingItem::kTypeSetter;
          }
          else {
            setter = (v8::AccessorSetterCallback)item.data;
            pairedType = BindingItem::kTypeGetter;
          }

          const BindingItem& nextItem = items[i + 1];
          if (count - i > 1 && nextItem.type == pairedType && ::strcmp(item.name, nextItem.name) == 0) {
            if (getter == NULL)
              getter = (v8::AccessorGetterCallback)nextItem.data;
            else
              setter = (v8::AccessorSetterCallback)nextItem.data;
            i++;
          }

          if (getter == NULL) {
            // TODO: Don't know what to do here...
            NJS_ASSERT(!"NJS_BIND_SET() used without having a getter before or after it!");
            ::abort();
          }

          // Signature is only created when needed and then cached.
          if (accessorSignature.IsEmpty())
            accessorSignature = v8::AccessorSignature::New(ctx.v8GetIsolate(), classObj);

          if (setter == NULL)
            attr |= v8::ReadOnly;

          prototype->SetAccessor(
            name, getter, setter, exports, v8::DEFAULT, static_cast<v8::PropertyAttribute>(attr), accessorSignature);
          break;
        }

        default: {
          NJS_ASSERT(!"Unhandled binding item type.");
          break;
        }
      }
    }

    return kResultOk;
  }

  template<typename NativeT>
  struct V8ClassBindings {
    typedef typename NativeT::Base Base;
    typedef typename NativeT::Type Type;

    static NJS_NOINLINE v8::Local<v8::FunctionTemplate> Init(
      Context& ctx,
      Value exports,
      v8::Local<v8::FunctionTemplate> superObj = v8::Local<v8::FunctionTemplate>()) NJS_NOEXCEPT {

      v8::Local<v8::FunctionTemplate> classObj = v8::FunctionTemplate::New(
        ctx.v8GetIsolate(), Type::Bindings::ConstructorEntry);

      v8::HandleScope (ctx.v8GetIsolate());
      if (!superObj.IsEmpty())
        classObj->Inherit(superObj);

      Value className = ctx.newInternalizedString(Latin1Ref(Type::staticClassName()));
      classObj->SetClassName(className.v8GetHandleAs<v8::String>());
      classObj->InstanceTemplate()->SetInternalFieldCount(1);

      // Type::Bindings is in fact an array of `BindingItem`s.
      typename Type::Bindings bindingItems;
      NJS_ASSERT((sizeof(bindingItems) % sizeof(BindingItem)) == 0);

      v8BindClassHelper(ctx, exports.v8GetHandleAs<v8::Object>(),
        classObj,
        className.v8GetHandleAs<v8::String>(),
        reinterpret_cast<const BindingItem*>(&bindingItems),
        sizeof(bindingItems) / sizeof(BindingItem));

      exports.v8GetHandleAs<v8::Object>()->Set(className.v8GetHandleAs<v8::String>(), classObj->GetFunction());
      return classObj;
    }
  };
} // Internal namespace

// ============================================================================
// [njs::Internal - Implementation]
// ============================================================================

static NJS_NOINLINE void Internal::v8ReportError(Context& ctx, Result result, const ResultPayload& payload) NJS_NOEXCEPT {
  unsigned int exceptionType = kExceptionError;
  const StaticData& staticData = _staticData;

  char tmpBuffer[kMaxBufferSize];
  char* msg = tmpBuffer;

  if (result >= _kResultThrowFirst && result <= _kResultThrowLast) {
    exceptionType = result;

    if (!payload.isInitialized())
      msg = "";
    else
      msg = const_cast<char*>(payload.error.message);
  }
  else if (result >= _kResultValueFirst && result <= _kResultValueLast) {
    exceptionType = kExceptionTypeError;

    char baseBuffer[64];
    char* base = baseBuffer;

    intptr_t argIndex = payload.value.argIndex;
    if (argIndex == -1)
      base = "Invalid value";
    else if (argIndex == -2)
      base = "Invalid argument";
    else
      snprintf(base, 64, "Invalid argument [%u]", static_cast<unsigned int>(argIndex));

    if (result == kResultInvalidValueTypeId) {
      const char* typeName = staticData.getTypeName(payload.value.typeId);
      snprintf(msg, kMaxBufferSize, "%s: Expected Type '%s'", base, typeName);
    }
    else if (result == kResultInvalidValueTypeName) {
      const char* typeName = payload.value.typeName;
      snprintf(msg, kMaxBufferSize, "%s: Expected Type '%s'", base, typeName);
    }
    else if (result == kResultInvalidValueCustom) {
      snprintf(msg, kMaxBufferSize, "%s: %s", base, payload.value.message);
    }
    else {
      msg = base;
    }
  }
  else if (result == kResultInvalidArgumentsLength) {
    exceptionType = kExceptionTypeError;

    int minArgs = payload.arguments.minArgs;
    int maxArgs = payload.arguments.maxArgs;

    if (minArgs == -1 || maxArgs == -1) {
      msg = "Invalid number of arguments: (unspecified)";
    }
    else if (minArgs == maxArgs) {
      snprintf(msg, kMaxBufferSize,
        "Invalid number of arguments: Required exactly %d",
        static_cast<unsigned int>(minArgs));
    }
    else {
      snprintf(msg, kMaxBufferSize,
        "Invalid number of arguments: Required between %d..%d",
        static_cast<unsigned int>(minArgs),
        static_cast<unsigned int>(maxArgs));
    }
  }
  else if (result == kResultInvalidConstructCall || result == kResultAbstractConstructCall) {
    exceptionType = kExceptionTypeError;

    const char* className = "(anonymous)";
    if (payload.isInitialized())
      className = payload.constructCall.className;

    const char* reason =
      (result == kResultInvalidConstructCall)
        ? "Use new operator"
        : "Class is abstract";

    snprintf(msg, kMaxBufferSize,
      "Cannot instantiate '%s': %s",
      className,
      reason);
  }
  else {
    msg = "Unknown error";
  }

  ctx.throwNewException(exceptionType, ctx.newString(Utf8Ref(msg)));
}

// ============================================================================
// [njs::Node]
// ============================================================================

#if defined(NJS_INTEGRATE_NODE)
namespace Node {
  static NJS_INLINE Value newBuffer(Context& ctx, size_t size) NJS_NOEXCEPT {
    return Value(
      Internal::v8LocalFromMaybe<v8::Object>(
        ::node::Buffer::New(ctx.v8GetIsolate(), size)));
  }

  static NJS_INLINE Value NewBuffer(Context& ctx, void* data, size_t size) NJS_NOEXCEPT {
    return Value(
      Internal::v8LocalFromMaybe<v8::Object>(
        ::node::Buffer::New(ctx.v8GetIsolate(), static_cast<char*>(data), size)));
  }

  static NJS_INLINE Value NewBuffer(Context& ctx, void* data, size_t size, node::Buffer::FreeCallback cb, void* hint) NJS_NOEXCEPT {
    return Value(
      Internal::v8LocalFromMaybe<v8::Object>(
        ::node::Buffer::New(ctx.v8GetIsolate(), static_cast<char*>(data), size, cb, hint)));
  }

  static NJS_INLINE bool isBuffer(const Value& obj) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isValid());
    return ::node::Buffer::HasInstance(obj._handle);
  }

  static NJS_INLINE void* getBufferData(const Value& obj) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(isBuffer(obj));
    return ::node::Buffer::Data(obj.v8GetHandleAs<v8::Object>());
  }

  static NJS_INLINE size_t getBufferSize(const Value& obj) NJS_NOEXCEPT {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(isBuffer(obj));
    return ::node::Buffer::Length(obj.v8GetHandleAs<v8::Object>());
  }
} // Node namespace
#endif // NJS_INTEGRATE_NODE

// ============================================================================
// [NJS_MODULE - Declarative Interface]
// ============================================================================

#define NJS_MODULE(NAME)                                                      \
  static NJS_INLINE void InitImpl##NAME(                                      \
    ::njs::Context& ctx,                                                      \
    ::njs::Value module,                                                      \
    ::njs::Value exports) NJS_NOEXCEPT;                                       \
                                                                              \
  extern "C" void InitEntry_##NAME(                                           \
      ::v8::Local<v8::Object> exports,                                        \
      ::v8::Local<v8::Value> module,                                          \
      ::v8::Local<v8::Context> v8ctx,                                         \
      void* priv) NJS_NOEXCEPT {                                              \
    ::njs::Context ctx(v8ctx->GetIsolate(), v8ctx);                           \
    InitImpl##NAME(ctx, ::njs::Value(module), ::njs::Value(exports));         \
  }                                                                           \
  NODE_MODULE_CONTEXT_AWARE(NAME, InitEntry_##NAME)                           \
                                                                              \
  static NJS_INLINE void InitImpl##NAME(                                      \
    ::njs::Context& ctx,                                                      \
    ::njs::Value module,                                                      \
    ::njs::Value exports) NJS_NOEXCEPT

// TODO:
// (_T [, _Super])
#define NJS_INIT_CLASS(SELF, ...) \
  SELF::Bindings::Init(ctx, __VA_ARGS__)

// ============================================================================
// [NJS_CLASS - Declarative Interface]
// ============================================================================

#define NJS_BASE_CLASS(SELF, CLASS_NAME)                                      \
public:                                                                       \
  typedef SELF Type;                                                          \
  typedef SELF Base;                                                          \
                                                                              \
  struct Bindings;                                                            \
  friend struct Bindings;                                                     \
                                                                              \
  static NJS_INLINE const char* staticClassName() NJS_NOEXCEPT {              \
    return CLASS_NAME;                                                        \
  }                                                                           \
                                                                              \
  NJS_INLINE size_t refCount() const NJS_NOEXCEPT { return _njsData._refCount; } \
  NJS_INLINE void addRef() NJS_NOEXCEPT { _njsData.addRef(); }                \
  NJS_INLINE void release() NJS_NOEXCEPT { _njsData.release(this); }          \
  NJS_INLINE void makeWeak() NJS_NOEXCEPT { _njsData.makeWeak(this); }        \
                                                                              \
  NJS_INLINE ::njs::Value asJSObject(::njs::Context& ctx) const NJS_NOEXCEPT { \
    return ctx.makeLocal(_njsData._object);                                   \
  }                                                                           \
                                                                              \
  ::njs::Internal::WrapData _njsData;

#define NJS_INHERIT_CLASS(SELF, BASE, CLASS_NAME)                             \
public:                                                                       \
  typedef SELF Type;                                                          \
  typedef BASE Base;                                                          \
                                                                              \
  struct Bindings;                                                            \
  friend struct Bindings;                                                     \
                                                                              \
  static NJS_INLINE const char* staticClassName() NJS_NOEXCEPT {              \
    return CLASS_NAME;                                                        \
  }

// ============================================================================
// [NJS_BIND - Bindings Interface]
// ============================================================================

// NOTE: All of these functions use V8 signatures to ensure that `This()`
// points to a correct object. This means that it's safe to directly use
// `Internal::v8UnwrapNative<>`.

#define NJS_BIND_CLASS(SELF) \
  struct SELF::Bindings : public ::njs::Internal::V8ClassBindings< SELF >

#define NJS_BIND_CONSTRUCTOR()                                                \
  static NJS_NOINLINE void ConstructorEntry(                                  \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) NJS_NOEXCEPT {   \
                                                                              \
    ::njs::ConstructCallContext ctx(info);                                    \
    ::njs::Result result;                                                     \
                                                                              \
    if (!info.IsConstructCall())                                              \
      result = ctx.invalidConstructCall(Type::staticClassName());             \
    else                                                                      \
      result = ConstructorImpl(ctx);                                          \
                                                                              \
    ctx._handleResult(result);                                                \
  }                                                                           \
                                                                              \
  static NJS_INLINE int ConstructorImpl(                                      \
    ::njs::ConstructCallContext& ctx) NJS_NOEXCEPT

#define NJS_ABSTRACT_CONSTRUCTOR()                                            \
  static NJS_NOINLINE void ConstructorEntry(                                  \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) NJS_NOEXCEPT {   \
                                                                              \
    ::njs::ConstructCallContext ctx(info);                                    \
    ::njs::Result result;                                                     \
                                                                              \
    if (!info.IsConstructCall())                                              \
      result = ::njs::kResultInvalidConstructCall;                            \
    else                                                                      \
      result = ::njs::kResultAbstractConstructCall;                           \
                                                                              \
    ctx._payload.constructCall.className =                                    \
      const_cast<char*>(Type::staticClassName());                             \
    ctx._handleResult(result);                                                \
  }

#define NJS_BIND_STATIC(NAME)                                                 \
  static NJS_NOINLINE void StaticEntry_##NAME(                                \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) NJS_NOEXCEPT {   \
                                                                              \
    ::njs::FunctionCallContext ctx(info);                                     \
    ctx._handleResult(StaticImpl_##NAME(ctx));                                \
  }                                                                           \
                                                                              \
  struct StaticInfo_##NAME : public ::njs::BindingItem {                      \
    NJS_INLINE StaticInfo_##NAME() NJS_NOEXCEPT                               \
      : BindingItem(kTypeStatic, 0, #NAME, (const void*)StaticEntry_##NAME) {}\
  } staticinfo_##NAME;                                                        \
                                                                              \
  static NJS_INLINE ::njs::Result StaticImpl_##NAME(                          \
    ::njs::FunctionCallContext& ctx) NJS_NOEXCEPT

#define NJS_BIND_METHOD(NAME)                                                 \
  static NJS_NOINLINE void MethodEntry_##NAME(                                \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) NJS_NOEXCEPT {   \
                                                                              \
    ::njs::FunctionCallContext ctx(info);                                     \
    Type* self = ::njs::Internal::v8UnwrapNative<Type>(ctx, ctx._info.This());\
    ctx._handleResult(MethodImpl_##NAME(ctx, self));                          \
  }                                                                           \
                                                                              \
  struct MethodInfo_##NAME : public ::njs::BindingItem {                      \
    NJS_INLINE MethodInfo_##NAME() NJS_NOEXCEPT                               \
      : BindingItem(kTypeMethod, 0, #NAME, (const void*)MethodEntry_##NAME) {}\
  } methodinfo_##NAME;                                                        \
                                                                              \
  static NJS_INLINE ::njs::Result MethodImpl_##NAME(                          \
    ::njs::FunctionCallContext& ctx, Type* self) NJS_NOEXCEPT

#define NJS_BIND_GET(NAME)                                                    \
  static NJS_NOINLINE void GetEntry_##NAME(                                   \
      ::v8::Local< ::v8::String > property,                                   \
      const ::v8::PropertyCallbackInfo< ::v8::Value >& info) NJS_NOEXCEPT {   \
                                                                              \
    ::njs::GetPropertyContext ctx(info);                                      \
    Type* self = ::njs::Internal::v8UnwrapNative<Type>(ctx, ctx._info.This());\
    ctx._handleResult(GetImpl_##NAME(ctx, self));                             \
  }                                                                           \
                                                                              \
  struct GetInfo_##NAME : public ::njs::BindingItem {                         \
    NJS_INLINE GetInfo_##NAME() NJS_NOEXCEPT                                  \
      : BindingItem(kTypeGetter, 0, #NAME, (const void*)GetEntry_##NAME) {}   \
  } GetInfo_##NAME;                                                           \
                                                                              \
  static NJS_INLINE ::njs::Result GetImpl_##NAME(                             \
    ::njs::GetPropertyContext& ctx, Type* self) NJS_NOEXCEPT

#define NJS_BIND_SET(NAME)                                                    \
  static NJS_NOINLINE void SetEntry_##NAME(                                   \
      ::v8::Local< ::v8::String > property,                                   \
      ::v8::Local< ::v8::Value > value,                                       \
      const v8::PropertyCallbackInfo<void>& info) NJS_NOEXCEPT {              \
                                                                              \
    ::njs::SetPropertyContext ctx(info, value);                               \
    Type* self = ::njs::Internal::v8UnwrapNative<Type>(ctx, ctx._info.This());\
    ctx._handleResult(SetImpl_##NAME(ctx, self));                             \
  }                                                                           \
                                                                              \
  struct SetInfo_##NAME : public ::njs::BindingItem {                         \
    NJS_INLINE SetInfo_##NAME() NJS_NOEXCEPT                                  \
      : BindingItem(kTypeSetter, 0, #NAME, (const void*)SetEntry_##NAME) {}   \
  } setinfo_##NAME;                                                           \
                                                                              \
  static NJS_INLINE ::njs::Result SetImpl_##NAME(                             \
    ::njs::SetPropertyContext& ctx, Type* self) NJS_NOEXCEPT

} // njs namespace

// [Guard]
#endif // _NJS_ENGINE_V8_H
