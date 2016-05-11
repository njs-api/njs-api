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
namespace internal {
  // Provided after `Context`. This is just a convenience function that
  // allows to get V8's `v8::Isolate` from `njs::Context` before it's defined.
  static NJS_INLINE v8::Isolate* GetV8IsolateOfContext(Context& ctx) NJS_NOEXCEPT;

  // Provided after `Value`. These are just a convenience functions that allow
  // to get the V8' `v8::Local<v8::Value>` from `njs::Value` before it's defined.
  static NJS_INLINE v8::Local<v8::Value>& V8HandleOfValue(Value& value) NJS_NOEXCEPT;
  static NJS_INLINE const v8::Local<v8::Value>& V8HandleOfValue(const Value& value) NJS_NOEXCEPT;
} // internal namespace

// ============================================================================
// [njs::Limits]
// ============================================================================

static const size_t kMaxStringLength = static_cast<size_t>(v8::String::kMaxLength);

// ============================================================================
// [njs::ResultOf]
// ============================================================================

// V8 specializations of `njs::ResultOf<>`.
template<typename T>
NJS_INLINE Result ResultOf(const v8::Local<T>& in) NJS_NOEXCEPT {
  if (in.IsEmpty())
    return kResultInvalidHandle;

  return kResultOk;
}

template<typename T>
NJS_INLINE Result ResultOf(const v8::MaybeLocal<T>& in) NJS_NOEXCEPT {
  if (in.IsEmpty())
    return kResultInvalidHandle;

  return kResultOk;
}

template<typename T>
NJS_INLINE Result ResultOf(const v8::Persistent<T>& in) NJS_NOEXCEPT {
  if (in.IsEmpty())
    return kResultInvalidHandle;

  return kResultOk;
}

template<typename T>
NJS_INLINE Result ResultOf(const v8::Global<T>& in) NJS_NOEXCEPT {
  if (in.IsEmpty())
    return kResultInvalidHandle;

  return kResultOk;
}

template<typename T>
NJS_INLINE Result ResultOf(const v8::Eternal<T>& in) NJS_NOEXCEPT {
  if (in.IsEmpty())
    return kResultInvalidHandle;

  return kResultOk;
}

template<>
NJS_INLINE Result ResultOf(const Value& in) NJS_NOEXCEPT {
  return ResultOf(internal::V8HandleOfValue(in));
}

// ============================================================================
// [njs::internal::V8 Helpers]
// ============================================================================

namespace internal {
  // V8 specific destroy callback.
  typedef void (*V8WrapDestroyCallback)(const v8::WeakCallbackData<v8::Value, void>& data);

  // Helper to convert a V8's `MaybeLocal<Type>` to V8's `Local<Type>`.
  //
  // NOTE: This performs an unchecked operation. Converting an invalid handle will
  // keep the handle invalid without triggering assertion failure. The purpose is to
  // keep things simple and to check for invalid handles instead of introduction yet
  // another handle type in NJS.
  template<typename Type>
  static NJS_INLINE const v8::Local<Type>& V8LocalFromMaybe(const v8::MaybeLocal<Type>& in) NJS_NOEXCEPT {
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
  NJS_INLINE v8::Local<v8::Value> V8NewString(Context& ctx, const StrRef& str, v8::NewStringType type) NJS_NOEXCEPT;

  template<>
  NJS_INLINE v8::Local<v8::Value> V8NewString(Context& ctx, const Latin1Ref& str, v8::NewStringType type) NJS_NOEXCEPT {
    if (str.GetLength() > kMaxStringLength)
      return v8::Local<v8::Value>();

    return V8LocalFromMaybe<v8::String>(
      v8::String::NewFromOneByte(
        GetV8IsolateOfContext(ctx),
        reinterpret_cast<const uint8_t*>(str.GetData()), type, static_cast<int>(str.GetLength())));
  }

  template<>
  NJS_INLINE v8::Local<v8::Value> V8NewString(Context& ctx, const Utf8Ref& str, v8::NewStringType type) NJS_NOEXCEPT {
    if (str.GetLength() > kMaxStringLength)
      return v8::Local<v8::Value>();

    return V8LocalFromMaybe<v8::String>(
      v8::String::NewFromUtf8(
        GetV8IsolateOfContext(ctx),
        str.GetData(), type, static_cast<int>(str.GetLength())));
  }

  template<>
  NJS_INLINE v8::Local<v8::Value> V8NewString(Context& ctx, const Utf16Ref& str, v8::NewStringType type) NJS_NOEXCEPT {
    if (str.GetLength() > kMaxStringLength)
      return v8::Local<v8::Value>();

    return V8LocalFromMaybe<v8::String>(
      v8::String::NewFromTwoByte(
        GetV8IsolateOfContext(ctx),
        str.GetData(), type, static_cast<int>(str.GetLength())));
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
  struct V8ConvertImpl<T, internal::kTraitIdBool> {
    static NJS_INLINE Result Pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      out = v8::Boolean::New(GetV8IsolateOfContext(ctx), in);
      return kResultOk;
    }

    static NJS_INLINE Result Unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsBoolean())
        return kResultInvalidValueType;
      out = v8::Boolean::Cast(*in)->Value();
      return kResultOk;
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, internal::kTraitIdSafeInt> {
    static NJS_INLINE Result Pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      out = v8::Integer::New(GetV8IsolateOfContext(ctx), in);
      return !out.IsEmpty() ? kResultOk : kResultBypass;
    }

    static NJS_INLINE Result Unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsInt32())
        return kResultInvalidValueType;

      int32_t value = v8::Int32::Cast(*in)->Value();
      return internal::IntCast(value, out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, internal::kTraitIdSafeUint> {
    static NJS_INLINE Result Pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      out = v8::Integer::New(GetV8IsolateOfContext(ctx), in);
      return !out.IsEmpty() ? kResultOk : kResultBypass;
    }

    static NJS_INLINE Result Unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsUint32())
        return kResultInvalidValueType;

      uint32_t value = v8::Uint32::Cast(*in)->Value();
      return internal::IntCast(value, out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, internal::kTraitIdUnsafeInt> {
    static NJS_INLINE Result Pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      // Fast-case: Most of the time if we hit packing int64_t to JS value it's
      // because of `size_t` and similar types. These types will hardly contain
      // value that is only representable as `int64_t` so we prefer this path
      // that creates a JavaScript integer, which can be then optimized by V8.
      if (in >= static_cast<T>(internal::TypeTraits<int32_t>::minValue()) &&
          in <= static_cast<T>(internal::TypeTraits<int32_t>::maxValue())) {
        out = v8::Integer::New(GetV8IsolateOfContext(ctx), static_cast<int32_t>(in));
      }
      else {
        if (!internal::IsSafeInt<T>(in))
          return kResultUnsafeInt64Conversion;
        out = v8::Number::New(GetV8IsolateOfContext(ctx), static_cast<double>(in));
      }

      return !out.IsEmpty() ? kResultOk : kResultBypass;
    }

    static NJS_INLINE Result Unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsNumber())
        return kResultInvalidValueType;

      return internal::DoubleToInt64(v8::Number::Cast(*in)->Value(), out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, internal::kTraitIdUnsafeUint> {
    static NJS_INLINE Result Pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      // The same trick as in kTraitIdUnsafeInt.
      if (in <= static_cast<T>(internal::TypeTraits<uint32_t>::maxValue())) {
        out = v8::Integer::NewFromUnsigned(GetV8IsolateOfContext(ctx), static_cast<uint32_t>(in));
      }
      else {
        if (!internal::IsSafeInt<T>(in))
          return kResultUnsafeInt64Conversion;
        out = v8::Number::New(GetV8IsolateOfContext(ctx), static_cast<double>(in));
      }
      return !out.IsEmpty() ? kResultOk : kResultBypass;
    }

    static NJS_INLINE Result Unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsNumber())
        return kResultInvalidValueType;

      return internal::DoubleToUint64(v8::Number::Cast(*in)->Value(), out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, internal::kTraitIdFloat> {
    static NJS_INLINE Result Pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      out = v8::Number::New(GetV8IsolateOfContext(ctx), static_cast<double>(in));
      return kResultOk;
    }

    static NJS_INLINE Result Unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsNumber())
        return kResultInvalidValueType;

      out = static_cast<T>(v8::Number::Cast(*in)->Value());
      return kResultOk;
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, internal::kTraitIdDouble> {
    static NJS_INLINE Result Pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
      out = v8::Number::New(GetV8IsolateOfContext(ctx), in);
      return kResultOk;
    }

    static NJS_INLINE Result Unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
      if (!in->IsNumber())
        return kResultInvalidValueType;

      out = v8::Number::Cast(*in)->Value();
      return kResultOk;
    }
  };

  template<typename T, typename Concept, unsigned int ConceptType>
  struct V8ConvertWithConceptImpl {
  };

  template<typename T, typename Concept>
  struct V8ConvertWithConceptImpl<T, Concept, kConceptSerializer> {
    static NJS_INLINE Result Pack(Context& ctx, const T& in, Value& out, const Concept& concept) NJS_NOEXCEPT {
      return concept.Serialize(ctx, in, out);
    }

    static NJS_INLINE Result Unpack(Context& ctx, const Value& in, T& out, const Concept& concept) NJS_NOEXCEPT {
      return concept.Deserialize(ctx, in, out);
    }
  };

  template<typename T, typename Concept>
  struct V8ConvertWithConceptImpl<T, Concept, kConceptValidator> {
    static NJS_INLINE Result Pack(Context& ctx, const T& in, Value& out, const Concept& concept) NJS_NOEXCEPT {
      NJS_CHECK(concept.Validate(in));
      return V8ConvertImpl<T, TypeTraits<T>::kTraitId>::Pack(ctx, in, out);
    }

    static NJS_INLINE Result Unpack(Context& ctx, const Value& in, T& out, const Concept& concept) NJS_NOEXCEPT {
      NJS_CHECK(V8ConvertImpl<T, TypeTraits<T>::kTraitId>::Unpack(ctx, V8HandleOfValue(in), out));
      return concept.Validate(out);
    }
  };

  // Helpers to unpack a primitive value of type `T` from V8's `Value` handle. All
  // specializations MUST BE forward-declared here.
  template<typename T>
  NJS_INLINE Result V8UnpackValue(Context& ctx, const v8::Local<v8::Value>& in, T& out) NJS_NOEXCEPT {
    return V8ConvertImpl<T, TypeTraits<T>::kTraitId>::Unpack(ctx, in, out);
  }
  // Handle to handle (no conversion).
  template<>
  NJS_INLINE Result V8UnpackValue(Context& ctx, const v8::Local<v8::Value>& in, Value& out) NJS_NOEXCEPT {
    V8HandleOfValue(out) = in;
    return kResultOk;
  }
  // Handle to handle (no conversion).
  template<>
  NJS_INLINE Result V8UnpackValue(Context& ctx, const v8::Local<v8::Value>& in, v8::Local<v8::Value>& out) NJS_NOEXCEPT {
    out = in;
    return kResultOk;
  }

  template<typename T, typename Concept>
  NJS_INLINE Result V8UnpackWithConcept(Context& ctx, const Value& in, T& out, const Concept& concept) NJS_NOEXCEPT {
    return V8ConvertWithConceptImpl<T, Concept, Concept::kConceptType>::Unpack(ctx, in, out, concept);
  }

  template<typename T>
  struct V8ReturnImpl {
    static NJS_INLINE Result Set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const T& value) NJS_NOEXCEPT {
      v8::Local<v8::Value> intermediate;
      NJS_CHECK(V8ConvertImpl<T, TypeTraits<T>::kTraitId>::Pack(ctx, value, intermediate));

      rv.Set(intermediate);
      return kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl< v8::Local<v8::Value> > {
    static NJS_INLINE Result Set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const v8::Local<v8::Value>& value) NJS_NOEXCEPT {
      rv.Set(value);
      return kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl<Value> {
    static NJS_INLINE Result Set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const Value& value) NJS_NOEXCEPT {
      rv.Set(V8HandleOfValue(value));
      return kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl<NullType> {
    static NJS_INLINE Result Set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const NullType&) NJS_NOEXCEPT {
      rv.SetNull();
      return kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl<UndefinedType> {
    static NJS_INLINE Result Set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const UndefinedType&) NJS_NOEXCEPT {
      rv.SetUndefined();
      return kResultOk;
    }
  };

  template<typename T>
  NJS_INLINE Result V8SetReturn(Context& ctx, v8::ReturnValue<v8::Value>& rv, const T& value) NJS_NOEXCEPT {
    return V8ReturnImpl<T>::Set(ctx, rv, value);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result V8SetReturnWithConcept(Context& ctx, v8::ReturnValue<v8::Value>& rv, const T& value, const Concept& concept) NJS_NOEXCEPT {
    Value intermediate;
    NJS_CHECK(V8ConvertWithConceptImpl<T, Concept, Concept::kConceptType>::Pack(ctx, value, intermediate, concept));

    rv.Set(V8HandleOfValue(intermediate));
    return kResultOk;
  }

  template<typename T>
  NJS_INLINE void V8WrapNative(Context& ctx, v8::Local<v8::Object> obj, T* self) NJS_NOEXCEPT;

  template<typename T>
  NJS_INLINE T* V8UnwrapNative(Context& ctx, v8::Local<v8::Object> obj) NJS_NOEXCEPT;

  // Propagates `result` and its `payload` into the underlying VM.
  static NJS_NOINLINE void V8ReportError(Context& ctx, Result result, const internal::ResultPayload& payload) NJS_NOEXCEPT;
} // internal namespace

// ============================================================================
// [njs::Runtime]
// ============================================================================

class Runtime {
public:
  NJS_INLINE Runtime() NJS_NOEXCEPT
    : _isolate(NULL) {}

  NJS_INLINE Runtime(const Runtime& other) NJS_NOEXCEPT
    : _isolate(other._isolate) {}

  explicit NJS_INLINE Runtime(v8::Isolate* isolate) NJS_NOEXCEPT
    : _isolate(isolate) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE v8::Isolate* GetV8Isolate() const NJS_NOEXCEPT {
    return _isolate;
  }

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

  NJS_INLINE Value() NJS_NOEXCEPT
    : _handle() {}
  NJS_INLINE Value(const Value& other) NJS_NOEXCEPT
    : _handle(other._handle) {}

  template<typename T>
  explicit NJS_INLINE Value(const v8::Local<T>& handle) NJS_NOEXCEPT
    : _handle(handle) {}

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
  NJS_INLINE HandleType& GetV8Handle() NJS_NOEXCEPT { return _handle; }
  NJS_INLINE const HandleType& GetV8Handle() const NJS_NOEXCEPT { return _handle; }

  template<typename T>
  NJS_INLINE v8::Local<T> GetV8HandleAs() const NJS_NOEXCEPT {
    return v8::Local<T>::Cast(_handle);
  }

  template<typename T = v8::Value>
  NJS_INLINE T* GetV8Value() const NJS_NOEXCEPT {
    return T::Cast(*_handle);
  }

  // --------------------------------------------------------------------------
  // [Handle]
  // --------------------------------------------------------------------------

  // Get whether the handle is valid and can be used. If the handle is invalid
  // then no member function can be called on it. There are asserts that check
  // this in debug mode.
  NJS_INLINE bool IsValid() const NJS_NOEXCEPT { return !_handle.IsEmpty(); }

  // Reset the handle.
  //
  // NOTE: `IsValid()` will return `false` after `Reset()` is called.
  NJS_INLINE void Reset() NJS_NOEXCEPT { _handle.Clear(); }

  // --------------------------------------------------------------------------
  // [Type System]
  // --------------------------------------------------------------------------

  NJS_INLINE bool IsUndefined() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsUndefined();
  }

  NJS_INLINE bool IsNull() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsNull();
  }

  NJS_INLINE bool IsBool() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsBoolean();
  }

  NJS_INLINE bool IsTrue() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsTrue();
  }

  NJS_INLINE bool IsFalse() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsFalse();
  }

  NJS_INLINE bool IsInt32() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsInt32();
  }

  NJS_INLINE bool IsUint32() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsUint32();
  }

  NJS_INLINE bool IsNumber() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsNumber();
  }

  NJS_INLINE bool IsString() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsString();
  }

  NJS_INLINE bool IsSymbol() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsSymbol();
  }

  NJS_INLINE bool IsArray() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsArray();
  }

  NJS_INLINE bool IsObject() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsObject();
  }

  NJS_INLINE bool IsDate() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsDate();
  }

  NJS_INLINE bool IsRegExp() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsRegExp();
  }

  NJS_INLINE bool IsFunction() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsFunction();
  }

  NJS_INLINE bool IsExternal() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsExternal();
  }

  NJS_INLINE bool IsPromise() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsPromise();
  }

  NJS_INLINE bool IsArrayBuffer() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsArrayBuffer();
  }

  NJS_INLINE bool IsArrayBufferView() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsArrayBufferView();
  }

  NJS_INLINE bool IsDataView() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsDataView();
  }

  NJS_INLINE bool IsTypedArray() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsTypedArray();
  }

  NJS_INLINE bool IsInt8Array() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsInt8Array();
  }

  NJS_INLINE bool IsInt16Array() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsInt16Array();
  }

  NJS_INLINE bool IsInt32Array() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsInt32Array();
  }

  NJS_INLINE bool IsUint8Array() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsUint8Array();
  }

  NJS_INLINE bool IsUint8ClampedArray() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsUint8ClampedArray();
  }

  NJS_INLINE bool IsUint16Array() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsUint16Array();
  }

  NJS_INLINE bool IsUint32Array() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsUint32Array();
  }

  NJS_INLINE bool IsFloat32Array() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsFloat32Array();
  }

  NJS_INLINE bool IsFloat64Array() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    return _handle->IsFloat64Array();
  }

  // --------------------------------------------------------------------------
  // [Casting]
  // --------------------------------------------------------------------------

  // IMPORTANT: These can only be called if the value can be casted to such types,
  // casting improper type will fail in debug more and cause an apocalypse in
  // release.

  NJS_INLINE bool BoolValue() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsBool());
    return GetV8Value<v8::Boolean>()->BooleanValue();
  }

  NJS_INLINE int32_t Int32Value() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsInt32());
    return GetV8Value<v8::Int32>()->Value();
  }

  NJS_INLINE uint32_t Uint32Value() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsUint32());
    return GetV8Value<v8::Uint32>()->Value();
  }

  NJS_INLINE double DoubleValue() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsNumber());
    return GetV8Value<v8::Number>()->Value();
  }

  // --------------------------------------------------------------------------
  // [String-Specific Functionality]
  // --------------------------------------------------------------------------

  NJS_INLINE bool IsLatin1() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsString());
    return GetV8Value<v8::String>()->ContainsOnlyOneByte();
  }

  NJS_INLINE bool IsLatin1Guess() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsString());
    return GetV8Value<v8::String>()->IsOneByte();
  }

  // Length of the string if represented as UTF-16 or LATIN-1 (if representable).
  NJS_INLINE size_t StringLength() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsString());
    return GetV8Value<v8::String>()->Length();
  }

  // Length of the string if represented as UTF-8.
  NJS_INLINE size_t StringUtf8Length() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsString());
    return GetV8Value<v8::String>()->Utf8Length();
  }

  NJS_INLINE int ReadLatin1(char* out, int len = -1) const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsString());
    return GetV8Value<v8::String>()->WriteOneByte(
      reinterpret_cast<uint8_t*>(out), 0, len, v8::String::NO_NULL_TERMINATION);
  }

  NJS_INLINE int ReadUtf8(char* out, int len = -1) const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsString());
    return GetV8Value<v8::String>()->WriteUtf8(
      out, len, NULL, v8::String::NO_NULL_TERMINATION);
  }

  NJS_INLINE int ReadUtf16(uint16_t* out, int len = -1) const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsString());
    return GetV8Value<v8::String>()->Write(
      out, 0, len, v8::String::NO_NULL_TERMINATION);
  }

  // --------------------------------------------------------------------------
  // [Array Functionality]
  // --------------------------------------------------------------------------

  NJS_INLINE size_t ArrayLength() const NJS_NOEXCEPT {
    NJS_ASSERT(IsValid());
    NJS_ASSERT(IsArray());
    return GetV8Value<v8::Array>()->Length();
  }
  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  HandleType _handle;
};

namespace internal {
  static NJS_INLINE v8::Local<v8::Value>& V8HandleOfValue(Value& value) NJS_NOEXCEPT {
    return value._handle;
  }

  static NJS_INLINE const v8::Local<v8::Value>& V8HandleOfValue(const Value& value) NJS_NOEXCEPT {
    return value._handle;
  }
} // internal namsepace

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
  NJS_INLINE v8::Persistent<v8::Value>& GetV8Handle() NJS_NOEXCEPT {
    return _handle;
  }

  NJS_INLINE const v8::Persistent<v8::Value>& GetV8Handle() const NJS_NOEXCEPT {
    return _handle;
  }

  // Get the V8's `Persistent<T>` handle.
  template<typename T>
  NJS_INLINE v8::Persistent<T>& GetV8HandleAs() NJS_NOEXCEPT {
    return reinterpret_cast<v8::Persistent<T>&>(_handle);
  }

  template<typename T>
  NJS_INLINE const v8::Persistent<T>& GetV8HandleAs() const NJS_NOEXCEPT {
    return reinterpret_cast<const v8::Persistent<T>&>(_handle);
  }

  template<typename T = v8::Value>
  NJS_INLINE T* GetV8Value() const NJS_NOEXCEPT {
    return T::Cast(*_handle);
  }

  // --------------------------------------------------------------------------
  // [Handle]
  // --------------------------------------------------------------------------

  // Get whether the handle is valid and can be used. If the handle is invalid
  // then no member function can be called on it. There are asserts that check
  // this in debug mode.
  NJS_INLINE bool IsValid() const NJS_NOEXCEPT {
    return !_handle.IsEmpty();
  }

  // Reset the handle to its construction state.
  //
  // NOTE: `IsValid()` returns return `false` after `Reset()` call.
  NJS_INLINE void Reset() NJS_NOEXCEPT {
    _handle.Empty();
  }

  // --------------------------------------------------------------------------
  // [Cast]
  // --------------------------------------------------------------------------

  // TODO: DEPRECATED?

  // Cast to `T`.
  //
  // NOTE: No checking is performed at this stage, you need to check whether
  // the value can be casted to `T` before you call `Cast()`.
  template<typename T>
  NJS_INLINE T& Cast() NJS_NOEXCEPT {
    return *static_cast<T*>(this);
  }

  template<typename T>
  NJS_INLINE const T& Cast() const NJS_NOEXCEPT {
    return *static_cast<const T*>(this);
  }

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
      _context(runtime.GetV8Isolate()->GetCurrentContext()) {}

  NJS_INLINE Context(v8::Isolate* isolate, const v8::Local<v8::Context>& context) NJS_NOEXCEPT
    : _runtime(isolate),
      _context(context) {}

  NJS_INLINE ~Context() NJS_NOEXCEPT {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE v8::Local<v8::Context>& GetV8Context() NJS_NOEXCEPT {
    return _context;
  }

  NJS_INLINE const v8::Local<v8::Context>& GetV8Context() const NJS_NOEXCEPT {
    return _context;
  }

  NJS_INLINE v8::Isolate* GetV8Isolate() const NJS_NOEXCEPT {
    return _runtime._isolate;
  }

  // --------------------------------------------------------------------------
  // [Runtime]
  // --------------------------------------------------------------------------

  NJS_INLINE const Runtime& GetRuntime() const NJS_NOEXCEPT {
    return _runtime;
  }

  // --------------------------------------------------------------------------
  // [Built-Ins]
  // --------------------------------------------------------------------------

  NJS_INLINE Value Undefined() const NJS_NOEXCEPT {
    return Value(v8::Undefined(GetV8Isolate()));
  }

  NJS_INLINE Value Null() const NJS_NOEXCEPT {
    return Value(v8::Null(GetV8Isolate()));
  }

  NJS_INLINE Value True() const NJS_NOEXCEPT {
    return Value(v8::True(GetV8Isolate()));
  }

  NJS_INLINE Value False() const NJS_NOEXCEPT {
    return Value(v8::False(GetV8Isolate()));
  }

  NJS_INLINE Value EmptyString() const NJS_NOEXCEPT {
    return Value(v8::String::Empty(GetV8Isolate()));
  }

  // --------------------------------------------------------------------------
  // [New]
  // --------------------------------------------------------------------------

  template<typename T>
  NJS_INLINE Value NewValue(const T& value) NJS_NOEXCEPT {
    Value result;
    internal::V8ConvertImpl<T, internal::TypeTraits<T>::kTraitId>::Pack(*this, value, result._handle);
    return result;
  }

  NJS_INLINE Value NewBool(bool value) NJS_NOEXCEPT {
    return Value(v8::Boolean::New(GetV8Isolate(), value));
  }

  NJS_INLINE Value NewInt32(int32_t value) NJS_NOEXCEPT {
    return Value(v8::Integer::New(GetV8Isolate(), value));
  }

  NJS_INLINE Value NewUint32(uint32_t value) NJS_NOEXCEPT {
    return Value(v8::Integer::New(GetV8Isolate(), value));
  }

  NJS_INLINE Value NewDouble(double value) NJS_NOEXCEPT {
    return Value(v8::Number::New(GetV8Isolate(), value));
  }

  template<typename StrRef>
  NJS_INLINE Value NewString(const StrRef& data) NJS_NOEXCEPT {
    return Value(internal::V8NewString<StrRef>(*this, data, v8::NewStringType::kNormal));
  }

  template<typename StrRef>
  NJS_INLINE Value NewInternalizedString(const StrRef& data) NJS_NOEXCEPT {
    return Value(internal::V8NewString<StrRef>(*this, data, v8::NewStringType::kInternalized));
  }

  NJS_INLINE Value NewObject() NJS_NOEXCEPT {
    return Value(v8::Object::New(GetV8Isolate()));
  }

  NJS_INLINE Value NewArray() NJS_NOEXCEPT {
    return Value(v8::Array::New(GetV8Isolate()));
  }

  // --------------------------------------------------------------------------
  // [Unpacking]
  // --------------------------------------------------------------------------

  // These don't perform any conversion, the type of the JS value has to match
  // the type of the `out` argument. Some types are internally convertible as
  // JS doesn't differentiate between integer and floating point types.

  template<typename T>
  NJS_INLINE Result Unpack(const Value& in, T& out) NJS_NOEXCEPT {
    return internal::V8UnpackValue<T>(*this, in._handle, out);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result Unpack(const Value& in, T& out, const Concept& concept) NJS_NOEXCEPT {
    return internal::V8UnpackWithConcept<T, Concept>(*this, in, out, concept);
  }

  // --------------------------------------------------------------------------
  // [Wrap / Unwrap]
  // --------------------------------------------------------------------------

  template<typename T>
  NJS_INLINE void Wrap(Value obj, T* self) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsObject());
    internal::V8WrapNative<T>(*this, obj.GetV8HandleAs<v8::Object>(), self);
  }

  template<typename T>
  NJS_INLINE T* Unwrap(Value obj) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsValid());
    NJS_ASSERT(obj.IsObject());
    return internal::V8UnwrapNative<T>(*this, obj.GetV8HandleAs<v8::Object>());
  }

  // --------------------------------------------------------------------------
  // [JS Features]
  // --------------------------------------------------------------------------

  NJS_INLINE Maybe<bool> Equals(const Value& aAny, const Value& bAny) NJS_NOEXCEPT {
    NJS_ASSERT(aAny.IsValid());
    NJS_ASSERT(bAny.IsValid());

    v8::Maybe<bool> result = aAny._handle->Equals(_context, bAny._handle);
    return Maybe<bool>(result.IsJust() ? kResultOk : kResultBypass, result.FromMaybe(false));
  }

  NJS_INLINE bool SameValue(const Value& aAny, const Value& bAny) const NJS_NOEXCEPT {
    NJS_ASSERT(aAny.IsValid());
    NJS_ASSERT(bAny.IsValid());
    return aAny._handle->SameValue(bAny._handle);
  }

  NJS_INLINE bool StrictEquals(const Value& aAny, const Value& bAny) const NJS_NOEXCEPT {
    NJS_ASSERT(aAny.IsValid());
    NJS_ASSERT(bAny.IsValid());
    return aAny._handle->StrictEquals(bAny._handle);
  }

  // Concatenate two strings.
  NJS_INLINE Value StringConcat(const Value& aStr, const Value& bStr) NJS_NOEXCEPT {
    NJS_ASSERT(aStr.IsValid());
    NJS_ASSERT(aStr.IsString());
    NJS_ASSERT(bStr.IsValid());
    NJS_ASSERT(bStr.IsString());

    return Value(
      v8::String::Concat(
        aStr.GetV8HandleAs<v8::String>(),
        bStr.GetV8HandleAs<v8::String>()));
  }

  NJS_INLINE Maybe<bool> HasProperty(const Value& obj, const Value& key) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsValid());
    NJS_ASSERT(obj.IsObject());
    NJS_ASSERT(key.IsValid());

    v8::Maybe<bool> result = obj.GetV8Value<v8::Object>()->Has(_context, key._handle);
    return Maybe<bool>(result.IsJust() ? kResultOk : kResultBypass, result.FromMaybe(false));
  }

  NJS_INLINE Maybe<bool> HasPropertyAt(const Value& obj, uint32_t index) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsValid());
    NJS_ASSERT(obj.IsObject());

    v8::Maybe<bool> result = obj.GetV8Value<v8::Object>()->Has(_context, index);
    return Maybe<bool>(result.IsJust() ? kResultOk : kResultBypass, result.FromMaybe(false));
  }

  NJS_INLINE Value GetProperty(const Value& obj, const Value& key) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsValid());
    NJS_ASSERT(obj.IsObject());
    NJS_ASSERT(key.IsValid());

    return Value(
      internal::V8LocalFromMaybe(
        obj.GetV8Value<v8::Object>()->Get(_context, key._handle)));
  }

  template<typename StrRef>
  NJS_NOINLINE Value GetProperty(const Value& obj, const StrRef& key) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsValid());
    NJS_ASSERT(obj.IsObject());

    njs::Value keyValue = NewString(key);
    if (!keyValue.IsValid())
      return keyValue;

    return Value(
      internal::V8LocalFromMaybe(
        obj.GetV8Value<v8::Object>()->Get(_context, keyValue._handle)));
  }

  NJS_INLINE Value GetPropertyAt(const Value& obj, uint32_t index) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsValid());
    NJS_ASSERT(obj.IsObject());

    return Value(
      internal::V8LocalFromMaybe(
        obj.GetV8Value<v8::Object>()->Get(_context, index)));
  }

  NJS_INLINE Result SetProperty(const Value& obj, const Value& key, const Value& val) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsValid());
    NJS_ASSERT(obj.IsObject());
    NJS_ASSERT(key.IsValid());
    NJS_ASSERT(val.IsValid());

    v8::Maybe<bool> result = obj.GetV8Value<v8::Object>()->Set(_context, key._handle, val._handle);
    return result.FromMaybe(false) ? kResultOk : kResultBypass;
  }

  NJS_INLINE Result SetPropertyAt(const Value& obj, uint32_t index, const Value& val) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsValid());
    NJS_ASSERT(obj.IsObject());
    NJS_ASSERT(val.IsValid());

    v8::Maybe<bool> result = obj.GetV8Value<v8::Object>()->Set(_context, index, val._handle);
    return result.FromMaybe(false) ? kResultOk : kResultBypass;
  }

  NJS_INLINE Value New(const Value& ctor) NJS_NOEXCEPT {
    NJS_ASSERT(ctor.IsValid());
    NJS_ASSERT(ctor.IsFunction());

    return Value(
      internal::V8LocalFromMaybe(
        ctor.GetV8Value<v8::Function>()->NewInstance(_context, 0, NULL)));
  }

  template<typename ...Args>
  NJS_INLINE Value New(const Value& ctor, Args... args) NJS_NOEXCEPT {
    Value argv[] = { args... };

    return Value(
      internal::V8LocalFromMaybe(
        ctor.GetV8Value<v8::Function>()->NewInstance(_context, sizeof(argv) / sizeof(argv[0]), argv)));
  }

  NJS_INLINE Value NewArgv(const Value& ctor, size_t argc, const Value* argv) NJS_NOEXCEPT {
    NJS_ASSERT(ctor.IsValid());
    NJS_ASSERT(ctor.IsFunction());

    return Value(
      internal::V8LocalFromMaybe(
        ctor.GetV8Value<v8::Function>()->NewInstance(
          _context,
          static_cast<int>(sizeof(argv) / sizeof(argv[0])),
          const_cast<v8::Local<v8::Value>*>(reinterpret_cast<const v8::Local<v8::Value>*>(argv)))));
  }

  NJS_INLINE Value Call(const Value& function, const Value& recv) NJS_NOEXCEPT {
    return CallArgv(function, recv, 0, NULL);
  }

  template<typename ...Args>
  NJS_INLINE Value Call(const Value& function, const Value& recv, Args... args) NJS_NOEXCEPT {
    Value argv[] = { args... };
    return CallArgv(function, recv, sizeof(argv) / sizeof(argv[0]), argv);
  }

  NJS_INLINE Value CallArgv(const Value& function, const Value& recv, size_t argc, const Value* argv) NJS_NOEXCEPT {
    NJS_ASSERT(function.IsValid());
    NJS_ASSERT(function.IsFunction());
    NJS_ASSERT(recv.IsValid());

    return Value(
      internal::V8LocalFromMaybe(
        function.GetV8Value<v8::Function>()->Call(
          _context, recv._handle,
          static_cast<int>(argc),
          const_cast<v8::Local<v8::Value>*>(reinterpret_cast<const v8::Local<v8::Value>*>(argv)))));
  }

  // --------------------------------------------------------------------------
  // [Exception]
  // --------------------------------------------------------------------------

  NJS_NOINLINE Value NewException(uint32_t type, Value msg) NJS_NOEXCEPT {
    NJS_ASSERT(msg.IsString());

    Value exception;
    switch (type) {
      default:
      case kExceptionError:
        exception = v8::Exception::Error(msg.GetV8HandleAs<v8::String>());
        break;
      case kExceptionTypeError:
        exception = v8::Exception::TypeError(msg.GetV8HandleAs<v8::String>());
        break;
      case kExceptionRangeError:
        exception = v8::Exception::RangeError(msg.GetV8HandleAs<v8::String>());
        break;
      case kExceptionSyntaxError:
        exception = v8::Exception::SyntaxError(msg.GetV8HandleAs<v8::String>());
        break;
      case kExceptionReferenceError:
        exception = v8::Exception::ReferenceError(msg.GetV8HandleAs<v8::String>());
        break;
    }
    return exception;
  }

  // --------------------------------------------------------------------------
  // [Throw]
  // --------------------------------------------------------------------------

  NJS_INLINE Result Throw(Value exception) NJS_NOEXCEPT {
    GetV8Isolate()->ThrowException(exception._handle);
    return kResultBypass;
  }

  NJS_NOINLINE Result ThrowNewException(uint32_t type, Value msg) NJS_NOEXCEPT {
    NJS_ASSERT(msg.IsString());
    Value exception = NewException(type, msg);
    GetV8Isolate()->ThrowException(exception._handle);
    return kResultBypass;
  }

  NJS_NOINLINE Result ThrowNewException(uint32_t type, const char* fmt, ...) NJS_NOEXCEPT {
    char buffer[kMaxBufferSize];

    va_list ap;
    va_start(ap, fmt);
    unsigned int length = internal::StrVFormat(buffer, kMaxBufferSize, fmt, ap);
    va_end(ap);

    Value exception = NewException(type, NewString(Utf8Ref(buffer, length)));
    GetV8Isolate()->ThrowException(exception._handle);
    return kResultBypass;
  }

  // --------------------------------------------------------------------------
  // [Local <-> Persistent]
  // --------------------------------------------------------------------------

  NJS_INLINE Value MakeLocal(const Persistent& persistent) NJS_NOEXCEPT {
    return Value(v8::Local<v8::Value>::New(GetV8Isolate(), persistent._handle));
  }

  NJS_INLINE Result MakePersistent(const Value& local, Persistent& persistent) NJS_NOEXCEPT {
    persistent._handle.Reset(GetV8Isolate(), local._handle);
    return kResultOk;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  // V8's isolate.
  Runtime _runtime;
  // V8's context handle.
  v8::Local<v8::Context> _context;
};

static NJS_INLINE v8::Isolate* internal::GetV8IsolateOfContext(Context& ctx) NJS_NOEXCEPT {
  return ctx.GetV8Isolate();
}

// ============================================================================
// [njs::HandleScope]
// ============================================================================

class HandleScope {
  NJS_NO_COPY(HandleScope)

 public:
  explicit NJS_INLINE HandleScope(const Context& ctx) NJS_NOEXCEPT
    : _handleScope(ctx.GetV8Isolate()) {}

  explicit NJS_INLINE HandleScope(v8::Isolate* isolate) NJS_NOEXCEPT
    : _handleScope(isolate) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE v8::HandleScope& GetV8HandleScope() NJS_NOEXCEPT {
    return _handleScope;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  // V8's handle scope.
  v8::HandleScope _handleScope;
};

// ============================================================================
// [njs::ScopedContext]
// ============================================================================

class ScopedContext : public Context {
  NJS_NO_COPY(ScopedContext)

 public:
  NJS_INLINE ScopedContext(const Context& other) NJS_NOEXCEPT
    : _handleScope(other),
      Context(other) {}

  explicit NJS_INLINE ScopedContext(const Runtime& other) NJS_NOEXCEPT
    : _handleScope(other.GetV8Isolate()),
      Context(other) {}

  NJS_INLINE ScopedContext(v8::Isolate* isolate, const v8::Local<v8::Context>& handle) NJS_NOEXCEPT
    : _handleScope(isolate),
      Context(isolate, handle) {}

  NJS_INLINE ~ScopedContext() NJS_NOEXCEPT {}

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  HandleScope _handleScope;
};

// ============================================================================
// [njs::internal::ResultPayload]
// ============================================================================

namespace internal {
  // Result payload.
  //
  // This data structure hold `Result` code and extra payload that provides a
  // basic description of the failure. The purpose is to bring minimal set of
  // failure information that can help users of native addons understand what
  // happened. For example a message like "Invalid argument" is not really
  // informative, especially if the function takes more arguments. However,
  // message like "Invalid argument #1, Expected Int32" is more helpful.
  //
  // To prevent unnecessary runtime overhead, this data structure shouldn't
  // contain any member that requires initialization (that have constructors).
  struct ResultPayload {
    // ------------------------------------------------------------------------
    // [Reset]
    // ------------------------------------------------------------------------

    // Has to be called to reset the content of payload. It resets only one
    // member for efficiency. If this member is set then the payload is valid,
    // otherwise it's invalid or not set.
    NJS_INLINE void Reset() NJS_NOEXCEPT {
      _data = NULL;
    }

    // ------------------------------------------------------------------------
    // [Throw Helpers]
    // ------------------------------------------------------------------------

    NJS_INLINE void setErrorMsg(const char* msg = NULL) NJS_NOEXCEPT {
      _err.msg = NULL;
    }

    NJS_NOINLINE void setErrorFmt(const char* fmt, ...) NJS_NOEXCEPT {
      va_list ap;
      va_start(ap, fmt);
      _err.msg = _buffer;
      // TODO: Set length.
      unsigned int length = internal::StrVFormat(_buffer, kMaxBufferSize, fmt, ap);
      va_end(ap);
    }

    // ------------------------------------------------------------------------
    // [InvalidArg Helpers]
    // ------------------------------------------------------------------------

    NJS_INLINE int throwInvalidArgMsg(int argIndex, const char* argName = NULL) NJS_NOEXCEPT {
      _arg.index = argIndex;
      _arg.msg = argName;
      return kResultInvalidArgumentCommon;
    }

    NJS_NOINLINE int throwInvalidArgFmt(int argIndex, const char* fmt, ...) NJS_NOEXCEPT {
      va_list ap;
      va_start(ap, fmt);
      // TODO: Set length.
      unsigned int length = internal::StrVFormat(_buffer, kMaxBufferSize, fmt, ap);
      va_end(ap);

      _arg.index = argIndex;
      _arg.msg = _buffer;
      return kResultInvalidArgumentCommon;
    }

    NJS_INLINE int throwInvalidArgTypeId(int argIndex, const char* argName, int typeId) NJS_NOEXCEPT {
      _arg.index = argIndex;
      _arg.msg = argName;
      _type.id = typeId;

      return kResultInvalidArgumentTypeId;
    }

    NJS_INLINE int throwInvalidArgTypeName(int argIndex, const char* argName, const char* typeName) NJS_NOEXCEPT {
      _arg.index = argIndex;
      _arg.msg = argName;
      _type.name = typeName;

      return kResultInvalidArgumentTypeName;
    }

    NJS_INLINE int throwInvalidArgCount(int argsRequired = -1) NJS_NOEXCEPT {
      _arg.required = argsRequired;
      return kResultInvalidArgumentCount;
    }

    // ------------------------------------------------------------------------
    // [InvalidValue]
    // ------------------------------------------------------------------------

    NJS_INLINE int throwInvalidValueMsg(const char* msg = NULL) NJS_NOEXCEPT {
      _val.msg = msg;
      return kResultInvalidValueCommon;
    }

    NJS_NOINLINE int throwInvalidValueFmt(const char* fmt, ...) NJS_NOEXCEPT {
      va_list ap;
      va_start(ap, fmt);
      // TODO: Set length.
      unsigned int length = internal::StrVFormat(_buffer, kMaxBufferSize, fmt, ap);
      va_end(ap);

      _val.msg = _buffer;
      return kResultInvalidValueCommon;
    }

    // --------------------------------------------------------------------------
    // [Members]
    // --------------------------------------------------------------------------

    struct ErrParams {
      const char* msg;
    };

    struct ArgParams {
      const char* msg;
      int index;
      int required;
    };

    struct ValParams {
      const char* msg;
    };

    const char* _data;

    union {
      ErrParams _err;
      ArgParams _arg;
      ValParams _val;
    };

    union {
      int id;
      const char* name;
    } _type;

    // Static buffer allocated by the function wrapper (not the implementation).
    char _buffer[kMaxBufferSize];
  };
} // internal namespace

// ============================================================================
// [njs::ExecutionContext]
// ============================================================================

class ExecutionContext : public Context {
  NJS_NO_COPY(ExecutionContext)

 public:
  NJS_INLINE ExecutionContext() NJS_NOEXCEPT
    : Context() { _payload.Reset(); }

  NJS_INLINE ExecutionContext(const Context& other) NJS_NOEXCEPT
    : Context(other) { _payload.Reset(); }

  NJS_INLINE ExecutionContext(v8::Isolate* isolate, const v8::Local<v8::Context>& handle) NJS_NOEXCEPT
    : Context(isolate, handle) { _payload.Reset(); }

  NJS_INLINE ~ExecutionContext() NJS_NOEXCEPT {}

  // --------------------------------------------------------------------------
  // [High-Level]
  // --------------------------------------------------------------------------

  NJS_INLINE Result InvalidArgument() NJS_NOEXCEPT {
    return kResultInvalidArgumentCommon;
  }

  NJS_INLINE Result InvalidArgument(unsigned int index) NJS_NOEXCEPT {
    return kResultInvalidArgumentCommon;
  }

  NJS_INLINE Result InvalidArgumentType(unsigned int index) NJS_NOEXCEPT {
    return kResultInvalidArgumentTypeId;
  }

  NJS_INLINE Result InvalidArgumentValue(unsigned int index) NJS_NOEXCEPT {
    return kResultInvalidArgumentValue;
  }

  NJS_INLINE Result InvalidArgumentsCount() NJS_NOEXCEPT {
    return kResultInvalidArgumentCount;
  }

  NJS_INLINE Result InvalidArgumentsCount(unsigned int expected) NJS_NOEXCEPT {
    return kResultInvalidArgumentCount;
  }

  // Processes a result returned mostly from a native function.
  NJS_INLINE void ProcessResult(Result result) NJS_NOEXCEPT {
    if (result != kResultOk)
      internal::V8ReportError(*this, result, _payload);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  internal::ResultPayload _payload;
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

  NJS_INLINE const v8::FunctionCallbackInfo<v8::Value>& GetV8CallbackInfo() const NJS_NOEXCEPT {
    return _info;
  }

  // --------------------------------------------------------------------------
  // [Arguments]
  // --------------------------------------------------------------------------

  NJS_INLINE unsigned int ArgumentsCount() const NJS_NOEXCEPT {
    return static_cast<unsigned int>(_info.Length());
  }

  NJS_INLINE Result VerifyArgumentsCount(unsigned int n) NJS_NOEXCEPT {
    return ArgumentsCount() == n ? static_cast<Result>(kResultOk) : InvalidArgumentsCount(n);
  }

  NJS_INLINE Value Argument(unsigned int index) const NJS_NOEXCEPT {
    return Value(_info.operator[](static_cast<int>(index)));
  }

  // Like `Unpack()`, but accepts the argument index instead of the `Value`.
  template<typename T>
  NJS_INLINE Result UnpackArgument(unsigned int index, T& out) NJS_NOEXCEPT {
    return internal::V8UnpackValue<T>(*this, _info[static_cast<int>(index)], out);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result UnpackArgument(unsigned int index, T& out, const Concept& concept) NJS_NOEXCEPT {
    return internal::V8UnpackWithConcept<T, Concept>(*this, Argument(index), out, concept);
  }

  // --------------------------------------------------------------------------
  // [Object System]
  // --------------------------------------------------------------------------

  NJS_INLINE Value This() const NJS_NOEXCEPT {
    return Value(_info.This());
  }

  NJS_INLINE bool IsConstructCall() const NJS_NOEXCEPT {
    return _info.IsConstructCall();
  }

  // --------------------------------------------------------------------------
  // [Return]
  // --------------------------------------------------------------------------

  template<typename T>
  NJS_INLINE Result Return(const T& value) NJS_NOEXCEPT {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return internal::V8SetReturn<T>(static_cast<Context&>(*this), rv, value);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result Return(const T& value, const Concept& concept) NJS_NOEXCEPT {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return internal::V8SetReturnWithConcept<T, Concept>(*this, rv, value, concept);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const v8::FunctionCallbackInfo<v8::Value>& _info;
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

  NJS_INLINE const v8::PropertyCallbackInfo<v8::Value>& GetV8CallbackInfo() const NJS_NOEXCEPT {
    return _info;
  }

  // --------------------------------------------------------------------------
  // [Object System]
  // --------------------------------------------------------------------------

  NJS_INLINE Value This() const NJS_NOEXCEPT {
    return Value(_info.This());
  }

  // --------------------------------------------------------------------------
  // [Return]
  // --------------------------------------------------------------------------

  template<typename T>
  NJS_INLINE Result Return(const T& value) NJS_NOEXCEPT {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return internal::V8SetReturn<T>(static_cast<Context&>(*this), rv, value);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result Return(const T& value, const Concept& concept) NJS_NOEXCEPT {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return internal::V8SetReturnWithConcept<T, Concept>(*this, rv, value, concept);
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

  NJS_INLINE const v8::PropertyCallbackInfo<void>& GetV8CallbackInfo() const NJS_NOEXCEPT {
    return _info;
  }

  // --------------------------------------------------------------------------
  // [Object System]
  // --------------------------------------------------------------------------

  NJS_INLINE Value This() const NJS_NOEXCEPT {
    return Value(_info.This());
  }

  // --------------------------------------------------------------------------
  // [Property]
  // --------------------------------------------------------------------------

  template<typename T>
  NJS_INLINE Result UnpackValue(T& out) NJS_NOEXCEPT {
    return internal::V8UnpackValue<T>(*this, _propertyValue._handle, out);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result UnpackValue(T& out, const Concept& concept) NJS_NOEXCEPT {
    return internal::V8UnpackWithConcept<T, Concept>(*this, _propertyValue, out, concept);
  }

  NJS_INLINE Value PropertyValue() const NJS_NOEXCEPT {
    return _propertyValue;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const v8::PropertyCallbackInfo<void>& _info;
  Value _propertyValue;
};

// ============================================================================
// [njs::internal::WrapData]
// ============================================================================

namespace internal {
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

    NJS_INLINE WrapData(Context& ctx, Value obj, internal::V8WrapDestroyCallback destroyCallback) NJS_NOEXCEPT
      : _refCount(0),
        _destroyCallback(destroyCallback) {
      NJS_ASSERT(obj.IsObject());
      _object._handle.Reset(ctx.GetV8Isolate(), obj._handle);
      obj.GetV8Value<v8::Object>()->SetAlignedPointerInInternalField(0, this);
    }

    NJS_NOINLINE ~WrapData() NJS_NOEXCEPT {
      if (_object.IsValid()) {
        NJS_ASSERT(_object._handle.IsNearDeath());
        _object._handle.ClearWeak();
        _object._handle.Reset();
      }
    }

    // ------------------------------------------------------------------------
    // [V8-Specific]
    // ------------------------------------------------------------------------

    NJS_INLINE v8::Persistent<v8::Value>& GetV8Handle() NJS_NOEXCEPT {
      return _object._handle;
    }

    NJS_INLINE const v8::Persistent<v8::Value>& GetV8Handle() const NJS_NOEXCEPT {
      return _object._handle;
    }

    // ------------------------------------------------------------------------
    // [Handle]
    // ------------------------------------------------------------------------

    NJS_INLINE bool IsValid() const NJS_NOEXCEPT {
      return _object.IsValid();
    }

    NJS_INLINE Persistent& GetObject() NJS_NOEXCEPT {
      return _object;
    }

    NJS_INLINE const Persistent& GetObject() const NJS_NOEXCEPT {
      return _object;
    }

    // ------------------------------------------------------------------------
    // [RefCount]
    // ------------------------------------------------------------------------

    NJS_INLINE size_t RefCount() const NJS_NOEXCEPT {
      return _refCount;
    }

    NJS_INLINE void AddRef() NJS_NOEXCEPT {
      NJS_ASSERT(IsValid());

      _refCount++;
      _object._handle.ClearWeak();
    }

    NJS_INLINE void Release(void* self) NJS_NOEXCEPT {
      NJS_ASSERT(_refCount > 0);
      NJS_ASSERT(IsValid());
      NJS_ASSERT(!IsWeak());

      if (--_refCount == 0)
        MakeWeak(self);
    }

    NJS_INLINE bool IsWeak() const NJS_NOEXCEPT {
      return _object._handle.IsWeak();
    }

    NJS_INLINE void MakeWeak(void* self) NJS_NOEXCEPT {
      NJS_ASSERT(IsValid());

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
    internal::V8WrapDestroyCallback _destroyCallback;
  };
} // internal namespace

// ============================================================================
// [njs::internal]
// ============================================================================

namespace internal {
  template<typename T>
  NJS_INLINE void V8WrapNative(Context& ctx, v8::Local<v8::Object> obj, T* self) NJS_NOEXCEPT {
    // Should be never called on an already initialized data.
    NJS_ASSERT(!self->_njsData._object.IsValid());
    NJS_ASSERT(!self->_njsData._destroyCallback);

    // Ensure the object was properly configured and contains internal fields.
    NJS_ASSERT(obj->InternalFieldCount() > 0);

    self->_njsData._object._handle.Reset(ctx.GetV8Isolate(), obj);
    self->_njsData._destroyCallback =(internal::V8WrapDestroyCallback)(internal::WrapData::destroyCallbackT<T>);

    obj->SetAlignedPointerInInternalField(0, self);
    self->_njsData.MakeWeak(self);
  }

  template<typename T>
  NJS_INLINE T* V8UnwrapNative(Context& ctx, v8::Local<v8::Object> obj) NJS_NOEXCEPT {
    NJS_ASSERT(obj->InternalFieldCount() > 0);

    // Cast to `T::Base` before casting to `T`. A direct cast from `void` to
    // `T` won't work right when `T` has more than one base class.
    void* self = obj->GetAlignedPointerFromInternalField(0);
    return static_cast<T*>(static_cast<typename T::Base*>(self));
  }

  static NJS_NOINLINE Result V8BindClassHelper(
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

      v8::Local<v8::String> name = internal::V8LocalFromMaybe(
        v8::String::NewFromOneByte(
          ctx.GetV8Isolate(), reinterpret_cast<const uint8_t*>(item.name), v8::NewStringType::kInternalized));

      switch (item.type) {
        case BindingItem::kTypeStatic: {
          v8::Local<v8::FunctionTemplate> fnTemplate = v8::FunctionTemplate::New(
            ctx.GetV8Isolate(), (v8::FunctionCallback)item.data, exports);

          fnTemplate->SetClassName(name);
          classObj->Set(name, fnTemplate);
          break;
        }

        case BindingItem::kTypeMethod: {
          // Signature is only created when needed and then cached.
          if (methodSignature.IsEmpty())
            methodSignature = v8::Signature::New(ctx.GetV8Isolate(), classObj);

          v8::Local<v8::FunctionTemplate> fnTemplate = v8::FunctionTemplate::New(
            ctx.GetV8Isolate(), (v8::FunctionCallback)item.data, exports, methodSignature);

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
            accessorSignature = v8::AccessorSignature::New(ctx.GetV8Isolate(), classObj);

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

  template<typename T>
  struct V8ClassBindings {
    typedef typename T::Base Base;
    typedef typename T::Type Type;

    static NJS_NOINLINE v8::Local<v8::FunctionTemplate> Init(
      Context& ctx,
      Value exports,
      v8::Local<v8::FunctionTemplate> superObj = v8::Local<v8::FunctionTemplate>()) NJS_NOEXCEPT {

      v8::Local<v8::FunctionTemplate> classObj = v8::FunctionTemplate::New(
        ctx.GetV8Isolate(), Type::Bindings::ConstructorEntry);

      v8::HandleScope (ctx.GetV8Isolate());
      if (!superObj.IsEmpty())
        classObj->Inherit(superObj);

      Value className = ctx.NewInternalizedString(Latin1Ref(Type::StaticClassName()));
      classObj->SetClassName(className.GetV8HandleAs<v8::String>());
      classObj->InstanceTemplate()->SetInternalFieldCount(1);

      // Type::Bindings is in fact an array of `BindingItem`s.
      typename Type::Bindings bindingItems;
      NJS_ASSERT((sizeof(bindingItems) % sizeof(BindingItem)) == 0);

      V8BindClassHelper(ctx, exports.GetV8HandleAs<v8::Object>(),
        classObj,
        className.GetV8HandleAs<v8::String>(),
        reinterpret_cast<const BindingItem*>(&bindingItems),
        sizeof(bindingItems) / sizeof(BindingItem));

      exports.GetV8HandleAs<v8::Object>()->Set(className.GetV8HandleAs<v8::String>(), classObj->GetFunction());
      return classObj;
    }
  };
} // internal namespace

// ============================================================================
// [njs::internal - Implementation]
// ============================================================================

static NJS_NOINLINE void internal::V8ReportError(Context& ctx, Result result, const internal::ResultPayload& payload) NJS_NOEXCEPT {
  const StaticData& staticData = _staticData;
  char tmpBuffer[kMaxBufferSize];
  char* msg = tmpBuffer;

  int exceptionType = kExceptionError;

  static const char fmtInvalidArgumentCommon[] = "Invalid argument %d (%s)";
  static const char fmtInvalidArgumentType[] = "Invalid argument %d (%s must be of type %s)";

  static const char fmtInvalidArgumentsCount[] = "Invalid count of arguments";
  static const char fmtInvalidArgumentsCountReq[] = "Invalid count of arguments (%d required)";

  static const char fmtInvalidValueCommon[] = "Invalid value (%s)";

  static const char fmtInvalidConstructCall[] = "Cannot instantiate class %s without new";
  static const char fmtAbstractConstructCall[] = "Cannot instantiate abstract class %s";

  switch (result) {
    // ----------------------------------------------------------------------
    // [Throw]
    // ----------------------------------------------------------------------

    case kResultThrowError:
    case kResultThrowTypeError:
    case kResultThrowRangeError:
    case kResultThrowSyntaxError:
    case kResultThrowReferenceError:
      exceptionType = result;
      msg = const_cast<char*>(payload._err.msg);
      break;

    // ----------------------------------------------------------------------
    // [Invalid Argument]
    // ----------------------------------------------------------------------

    case kResultInvalidArgumentCommon:
      exceptionType = kExceptionTypeError;
      snprintf(msg, kMaxBufferSize, fmtInvalidArgumentCommon,
        payload._arg.index,
        payload._arg.msg);
      break;

    case kResultInvalidArgumentTypeId:
      exceptionType = kExceptionTypeError;
      snprintf(msg, kMaxBufferSize, fmtInvalidArgumentType,
        payload._arg.index,
        payload._arg.msg,
        staticData.getTypeName(payload._type.id));
      break;

    case kResultInvalidArgumentTypeName:
      exceptionType = kExceptionTypeError;
      snprintf(msg, kMaxBufferSize, fmtInvalidArgumentType,
        payload._arg.index,
        payload._arg.msg,
        payload._type.name);
      break;

    case kResultInvalidArgumentCount:
      exceptionType = kExceptionTypeError;
      if (payload._arg.required == -1)
        snprintf(msg, kMaxBufferSize, fmtInvalidArgumentsCount);
      else
        snprintf(msg, kMaxBufferSize, fmtInvalidArgumentsCountReq, payload._arg.required);
      break;

    case kResultInvalidValueCommon:
      exceptionType = kExceptionTypeError;
      snprintf(msg, kMaxBufferSize, fmtInvalidValueCommon, payload._val.msg);
      break;

    case kResultInvalidConstructCall:
      exceptionType = kExceptionTypeError;
      snprintf(msg, kMaxBufferSize, fmtInvalidConstructCall, payload._data);
      break;

    case kResultAbstractConstructCall:
      exceptionType = kExceptionTypeError;
      snprintf(msg, kMaxBufferSize, fmtAbstractConstructCall, payload._data);
      break;
  }

  ctx.ThrowNewException(exceptionType, ctx.NewString(Utf8Ref(msg)));
}

// ============================================================================
// [njs::Node]
// ============================================================================

#if defined(NJS_INTEGRATE_NODE)
namespace Node {
  static NJS_INLINE Value NewBuffer(Context& ctx, size_t size) NJS_NOEXCEPT {
    return Value(
      internal::V8LocalFromMaybe<v8::Object>(
        ::node::Buffer::New(ctx.GetV8Isolate(), size)));
  }

  static NJS_INLINE Value NewBuffer(Context& ctx, void* data, size_t size) NJS_NOEXCEPT {
    return Value(
      internal::V8LocalFromMaybe<v8::Object>(
        ::node::Buffer::New(ctx.GetV8Isolate(), static_cast<char*>(data), size)));
  }

  static NJS_INLINE Value NewBuffer(Context& ctx, void* data, size_t size, node::Buffer::FreeCallback cb, void* hint) NJS_NOEXCEPT {
    return Value(
      internal::V8LocalFromMaybe<v8::Object>(
        ::node::Buffer::New(ctx.GetV8Isolate(), static_cast<char*>(data), size, cb, hint)));
  }

  static NJS_INLINE bool IsBuffer(const Value& obj) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsValid());
    return ::node::Buffer::HasInstance(obj._handle);
  }

  static NJS_INLINE void* GetBufferData(const Value& obj) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsValid());
    NJS_ASSERT(IsBuffer(obj));
    return ::node::Buffer::Data(obj.GetV8HandleAs<v8::Object>());
  }

  static NJS_INLINE size_t GetBufferSize(const Value& obj) NJS_NOEXCEPT {
    NJS_ASSERT(obj.IsValid());
    NJS_ASSERT(IsBuffer(obj));
    return ::node::Buffer::Length(obj.GetV8HandleAs<v8::Object>());
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
 public:                                                                      \
  typedef SELF Type;                                                          \
  typedef SELF Base;                                                          \
                                                                              \
  struct Bindings;                                                            \
  friend struct Bindings;                                                     \
                                                                              \
  static NJS_INLINE const char* StaticClassName() NJS_NOEXCEPT {              \
    return CLASS_NAME;                                                        \
  }                                                                           \
                                                                              \
  NJS_INLINE size_t RefCount() const NJS_NOEXCEPT { return _njsData._refCount; } \
  NJS_INLINE void AddRef() NJS_NOEXCEPT { _njsData.AddRef(); }                \
  NJS_INLINE void Release() NJS_NOEXCEPT { _njsData.Release(this); }          \
  NJS_INLINE void MakeWeak() NJS_NOEXCEPT { _njsData.MakeWeak(this); }        \
                                                                              \
  NJS_INLINE ::njs::Value AsJSObject(::njs::Context& ctx) const NJS_NOEXCEPT { \
    return ctx.MakeLocal(_njsData._object);                                   \
  }                                                                           \
                                                                              \
  ::njs::internal::WrapData _njsData;

#define NJS_INHERIT_CLASS(SELF, BASE, CLASS_NAME)                             \
 public:                                                                      \
  typedef SELF Type;                                                          \
  typedef BASE Base;                                                          \
                                                                              \
  struct Bindings;                                                            \
  friend struct Bindings;                                                     \
                                                                              \
  static NJS_INLINE const char* StaticClassName() NJS_NOEXCEPT {              \
    return CLASS_NAME;                                                        \
  }

// ============================================================================
// [NJS_BIND - Bindings Interface]
// ============================================================================

// NOTE: All of these functions use V8 signatures to ensure that `This()`
// points to a correct object. This means that it's safe to directly use
// `internal::V8UnwrapNative<>`.

#define NJS_BIND_CLASS(SELF) \
  struct SELF::Bindings : public ::njs::internal::V8ClassBindings< SELF >

#define NJS_BIND_CONSTRUCTOR()                                                \
  static NJS_NOINLINE void ConstructorEntry(                                  \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) NJS_NOEXCEPT {   \
                                                                              \
    ::njs::FunctionCallContext ctx(info);                                     \
    ::njs::Result result;                                                     \
                                                                              \
    if (!info.IsConstructCall()) {                                            \
      result = ::njs::kResultInvalidConstructCall;                            \
      ctx._payload._data = const_cast<char*>(Type::StaticClassName());        \
    }                                                                         \
    else {                                                                    \
      result = ConstructorImpl(ctx);                                          \
    }                                                                         \
                                                                              \
    ctx.ProcessResult(result);                                                \
  }                                                                           \
                                                                              \
  static NJS_INLINE int ConstructorImpl(                                      \
    ::njs::FunctionCallContext& ctx) NJS_NOEXCEPT

#define NJS_ABSTRACT_CONSTRUCTOR()                                            \
  static NJS_NOINLINE void ConstructorEntry(                                  \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) NJS_NOEXCEPT {   \
                                                                              \
    ::njs::FunctionCallContext ctx(info);                                     \
    ::njs::Result result;                                                     \
                                                                              \
    if (!info.IsConstructCall())                                              \
      result = ::njs::kResultInvalidConstructCall;                            \
    else                                                                      \
      result = ::njs::kResultAbstractConstructCall;                           \
    ctx._payload._data = const_cast<char*>(Type::StaticClassName());          \
                                                                              \
    ctx.ProcessResult(result);                                                \
  }

#define NJS_BIND_STATIC(NAME)                                                 \
  static NJS_NOINLINE void StaticEntry_##NAME(                                \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) NJS_NOEXCEPT {   \
                                                                              \
    ::njs::FunctionCallContext ctx(info);                                     \
    ctx.ProcessResult(StaticImpl_##NAME(ctx));                                \
  }                                                                           \
                                                                              \
  struct StaticInfo_##NAME : public ::njs::internal::BindingItem {            \
    NJS_INLINE StaticInfo_##NAME() NJS_NOEXCEPT                               \
      : BindingItem(                                                          \
          kTypeStatic, 0, #NAME, (const void*)StaticEntry_##NAME) {}          \
  } staticinfo_##NAME;                                                        \
                                                                              \
  static NJS_INLINE ::njs::Result StaticImpl_##NAME(                          \
    ::njs::FunctionCallContext& ctx) NJS_NOEXCEPT

#define NJS_BIND_METHOD(NAME)                                                 \
  static NJS_NOINLINE void MethodEntry_##NAME(                                \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) NJS_NOEXCEPT {   \
                                                                              \
    ::njs::FunctionCallContext ctx(info);                                     \
    Type* self = ::njs::internal::V8UnwrapNative<Type>(ctx, ctx._info.This());\
    ctx.ProcessResult(MethodImpl_##NAME(ctx, self));                          \
  }                                                                           \
                                                                              \
  struct MethodInfo_##NAME : public ::njs::internal::BindingItem {            \
    NJS_INLINE MethodInfo_##NAME() NJS_NOEXCEPT                               \
      : BindingItem(                                                          \
          kTypeMethod, 0, #NAME, (const void*)MethodEntry_##NAME) {}          \
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
    Type* self = ::njs::internal::V8UnwrapNative<Type>(ctx, ctx._info.This());\
    ctx.ProcessResult(GetImpl_##NAME(ctx, self));                             \
  }                                                                           \
                                                                              \
  struct GetInfo_##NAME : public ::njs::internal::BindingItem {               \
    NJS_INLINE GetInfo_##NAME() NJS_NOEXCEPT                                  \
      : BindingItem(                                                          \
          kTypeGetter, 0, #NAME, (const void*)GetEntry_##NAME) {}             \
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
    Type* self = ::njs::internal::V8UnwrapNative<Type>(ctx, ctx._info.This());\
    ctx.ProcessResult(SetImpl_##NAME(ctx, self));                             \
  }                                                                           \
                                                                              \
  struct SetInfo_##NAME : public ::njs::internal::BindingItem {               \
    NJS_INLINE SetInfo_##NAME() NJS_NOEXCEPT                                  \
      : BindingItem(                                                          \
          kTypeSetter, 0, #NAME, (const void*)SetEntry_##NAME) {}             \
  } setinfo_##NAME;                                                           \
                                                                              \
  static NJS_INLINE ::njs::Result SetImpl_##NAME(                             \
    ::njs::SetPropertyContext& ctx, Type* self) NJS_NOEXCEPT

} // njs namespace

// [Guard]
#endif // _NJS_ENGINE_V8_H
