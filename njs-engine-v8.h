// [NJS-API]
// Native JavaScript API for Bindings.
//
// [License]
// Public Domain <http://unlicense.org>

// This file implements a NJS <-> V8 engine bridge.

#ifndef NJS_ENGINE_V8_H
#define NJS_ENGINE_V8_H

#include "./njs-base.h"
#include <v8.h>

#if defined(NJS_INTEGRATE_NODE)
# include <node.h>
# include <node_buffer.h>
#endif // NJS_INTEGRATE_NODE

namespace njs {

// ============================================================================
// [njs::Globals]
// ============================================================================

namespace Globals {

static const size_t kMaxStringSize = static_cast<size_t>(v8::String::kMaxLength);

} // Globals namespace

// ============================================================================
// [njs::TypeDefs]
// ============================================================================

typedef v8::FunctionCallback NativeFunction;

// ============================================================================
// [Forward Declarations]
// ============================================================================

// These are needed to make things a bit simpler and minimize the number of
// functions that have to be forward-declared.
namespace Internal {
  // Provided after `Context`. This is just a convenience function that
  // allows to get V8's `v8::Isolate` from `njs::Context` before it's defined.
  static NJS_INLINE v8::Isolate* v8IsolateOfContext(Context& ctx) noexcept;

  // Provided after `Value`. These are just a convenience functions that allow
  // to get the V8' `v8::Local<v8::Value>` from `njs::Value` before it's defined.
  static NJS_INLINE v8::Local<v8::Value>& v8HandleOfValue(Value& value) noexcept;
  static NJS_INLINE const v8::Local<v8::Value>& v8HandleOfValue(const Value& value) noexcept;
} // {Internal}

// ============================================================================
// [njs::ResultOf]
// ============================================================================

// V8 specializations of `njs::resultOf<>`.
template<typename T>
NJS_INLINE Result resultOf(const v8::Local<T>& in) noexcept {
  return in.IsEmpty() ? Globals::kResultInvalidHandle : Globals::kResultOk;
}

template<typename T>
NJS_INLINE Result resultOf(const v8::MaybeLocal<T>& in) noexcept {
  return in.IsEmpty() ? Globals::kResultInvalidHandle : Globals::kResultOk;
}

template<typename T>
NJS_INLINE Result resultOf(const v8::Persistent<T>& in) noexcept {
  return in.IsEmpty() ? Globals::kResultInvalidHandle : Globals::kResultOk;
}

template<typename T>
NJS_INLINE Result resultOf(const v8::Global<T>& in) noexcept {
  return in.IsEmpty() ? Globals::kResultInvalidHandle : Globals::kResultOk;
}

template<typename T>
NJS_INLINE Result resultOf(const v8::Eternal<T>& in) noexcept {
  return in.IsEmpty() ? Globals::kResultInvalidHandle : Globals::kResultOk;
}

template<>
NJS_INLINE Result resultOf(const Value& in) noexcept {
  return resultOf(Internal::v8HandleOfValue(in));
}

// ============================================================================
// [njs::Internal::V8 Helpers]
// ============================================================================

namespace Internal {
  static NJS_INLINE uintptr_t nativeTagFromObjectTag(uint32_t objectTag) noexcept {
    if (sizeof(uintptr_t) > sizeof(uint32_t))
      return (uintptr_t(objectTag) << 34) | uintptr_t(0x00000002u);
    else
      return (uintptr_t(objectTag) <<  2) | uintptr_t(0x00000002u);
  }

  static NJS_INLINE uint32_t objectTagFromNativeTag(uintptr_t nativeTag) noexcept {
    if (sizeof(uintptr_t) > sizeof(uint32_t))
      return uint32_t(nativeTag >> 34);
    else
      return uint32_t(nativeTag >>  2);
  }
} // {Internal}

// ============================================================================
// [njs::Internal::V8 Helpers]
// ============================================================================

namespace Internal {
  // V8 specific destroy callback.
  typedef void (*V8WrapDestroyCallback)(const v8::WeakCallbackInfo<void>& data);

  // Helper to convert a V8's `MaybeLocal<Type>` to V8's `Local<Type>`.
  //
  // NOTE: This performs an unchecked operation. Converting an invalid handle will
  // keep the handle invalid without triggering assertion failure. The purpose is to
  // keep things simple and to check for invalid handles instead of introduction yet
  // another handle type in NJS.
  template<typename Type>
  static NJS_INLINE const v8::Local<Type>& v8LocalFromMaybe(const v8::MaybeLocal<Type>& in) noexcept {
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
  // string for all input types that match `Globals::kTraitIdStrRef`. Each
  // specialization uses a different V8 constructor. The `type` argument
  // matches V8's `NewStringType` and can be used to create internalized strings.
  template<typename StrRef>
  NJS_INLINE v8::Local<v8::Value> v8NewString(Context& ctx, const StrRef& str, v8::NewStringType type) noexcept = delete;

  template<>
  NJS_INLINE v8::Local<v8::Value> v8NewString(Context& ctx, const Latin1Ref& str, v8::NewStringType type) noexcept {
    if (str.size() > Globals::kMaxStringSize)
      return v8::Local<v8::Value>();

    return v8LocalFromMaybe<v8::String>(
      v8::String::NewFromOneByte(
        v8IsolateOfContext(ctx),
        reinterpret_cast<const uint8_t*>(str.data()), type, static_cast<int>(str.size())));
  }

  template<>
  NJS_INLINE v8::Local<v8::Value> v8NewString(Context& ctx, const Utf8Ref& str, v8::NewStringType type) noexcept {
    if (str.size() > Globals::kMaxStringSize)
      return v8::Local<v8::Value>();

    return v8LocalFromMaybe<v8::String>(
      v8::String::NewFromUtf8(
        v8IsolateOfContext(ctx),
        str.data(), type, static_cast<int>(str.size())));
  }

  template<>
  NJS_INLINE v8::Local<v8::Value> v8NewString(Context& ctx, const Utf16Ref& str, v8::NewStringType type) noexcept {
    if (str.size() > Globals::kMaxStringSize)
      return v8::Local<v8::Value>();

    return v8LocalFromMaybe<v8::String>(
      v8::String::NewFromTwoByte(
        v8IsolateOfContext(ctx),
        str.data(), type, static_cast<int>(str.size())));
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
  struct V8ConvertImpl<T, Globals::kTraitIdBool> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) noexcept {
      out = v8::Boolean::New(v8IsolateOfContext(ctx), in);
      return Globals::kResultOk;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) noexcept {
      if (!in->IsBoolean())
        return Globals::kResultInvalidValue;
      out = v8::Boolean::Cast(*in)->Value();
      return Globals::kResultOk;
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, Globals::kTraitIdSafeInt> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) noexcept {
      out = v8::Integer::New(v8IsolateOfContext(ctx), in);
      return !out.IsEmpty() ? Globals::kResultOk : Globals::kResultBypass;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) noexcept {
      if (!in->IsInt32())
        return Globals::kResultInvalidValue;

      int32_t value = v8::Int32::Cast(*in)->Value();
      return IntUtils::intCast(value, out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, Globals::kTraitIdSafeUint> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) noexcept {
      out = v8::Integer::New(v8IsolateOfContext(ctx), in);
      return !out.IsEmpty() ? Globals::kResultOk : Globals::kResultBypass;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) noexcept {
      if (!in->IsUint32())
        return Globals::kResultInvalidValue;

      uint32_t value = v8::Uint32::Cast(*in)->Value();
      return IntUtils::intCast(value, out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, Globals::kTraitIdUnsafeInt> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) noexcept {
      // Fast-case: Most of the time if we hit packing int64_t to JS value it's
      // because of `size_t` and similar types. These types will hardly contain
      // value that is only representable as `int64_t` so we prefer this path
      // that creates a JavaScript integer, which can be then optimized by V8.
      if (in >= static_cast<T>(std::numeric_limits<int32_t>::lowest()) &&
          in <= static_cast<T>(std::numeric_limits<int32_t>::max())) {
        out = v8::Integer::New(v8IsolateOfContext(ctx), static_cast<int32_t>(in));
      }
      else {
        if (!IntUtils::isSafeInt<T>(in))
          return Globals::kResultUnsafeInt64Conversion;
        out = v8::Number::New(v8IsolateOfContext(ctx), static_cast<double>(in));
      }

      return !out.IsEmpty() ? Globals::kResultOk : Globals::kResultBypass;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) noexcept {
      if (!in->IsNumber())
        return Globals::kResultInvalidValue;
      else
        return IntUtils::doubleToInt64(v8::Number::Cast(*in)->Value(), out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, Globals::kTraitIdUnsafeUint> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) noexcept {
      // The same trick as in kTraitIdUnsafeInt.
      if (in <= static_cast<T>(TypeTraits<uint32_t>::maxValue())) {
        out = v8::Integer::NewFromUnsigned(v8IsolateOfContext(ctx), static_cast<uint32_t>(in));
      }
      else {
        if (!IntUtils::isSafeInt<T>(in))
          return Globals::kResultUnsafeInt64Conversion;
        out = v8::Number::New(v8IsolateOfContext(ctx), static_cast<double>(in));
      }
      return !out.IsEmpty() ? Globals::kResultOk : Globals::kResultBypass;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) noexcept {
      if (!in->IsNumber())
        return Globals::kResultInvalidValue;
      else
        return IntUtils::doubleToUint64(v8::Number::Cast(*in)->Value(), out);
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, Globals::kTraitIdFloat> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) noexcept {
      out = v8::Number::New(v8IsolateOfContext(ctx), static_cast<double>(in));
      return Globals::kResultOk;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) noexcept {
      if (!in->IsNumber())
        return Globals::kResultInvalidValue;

      out = static_cast<T>(v8::Number::Cast(*in)->Value());
      return Globals::kResultOk;
    }
  };

  template<typename T>
  struct V8ConvertImpl<T, Globals::kTraitIdDouble> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, v8::Local<v8::Value>& out) noexcept {
      out = v8::Number::New(v8IsolateOfContext(ctx), in);
      return Globals::kResultOk;
    }

    static NJS_INLINE Result unpack(Context& ctx, const v8::Local<v8::Value>& in, T& out) noexcept {
      if (!in->IsNumber())
        return Globals::kResultInvalidValue;

      out = v8::Number::Cast(*in)->Value();
      return Globals::kResultOk;
    }
  };

  template<typename T, typename Concept, unsigned int ConceptType>
  struct V8ConvertWithConceptImpl {
  };

  template<typename T, typename Concept>
  struct V8ConvertWithConceptImpl<T, Concept, Globals::kConceptSerializer> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, Value& out, const Concept& concept) noexcept {
      return concept.serialize(ctx, in, out);
    }

    static NJS_INLINE Result unpack(Context& ctx, const Value& in, T& out, const Concept& concept) noexcept {
      return concept.deserialize(ctx, in, out);
    }
  };

  template<typename T, typename Concept>
  struct V8ConvertWithConceptImpl<T, Concept, Globals::kConceptValidator> {
    static NJS_INLINE Result pack(Context& ctx, const T& in, Value& out, const Concept& concept) noexcept {
      NJS_CHECK(concept.validate(in));
      return V8ConvertImpl<T, TypeTraits<T>::kTraitId>::pack(ctx, in, out);
    }

    static NJS_INLINE Result unpack(Context& ctx, const Value& in, T& out, const Concept& concept) noexcept {
      NJS_CHECK(V8ConvertImpl<T, TypeTraits<T>::kTraitId>::unpack(ctx, v8HandleOfValue(in), out));
      return concept.validate(out);
    }
  };

  // Helpers to unpack a primitive value of type `T` from V8's `Value` handle. All
  // specializations MUST BE forward-declared here.
  template<typename T>
  NJS_INLINE Result v8UnpackValue(Context& ctx, const v8::Local<v8::Value>& in, T& out) noexcept {
    return V8ConvertImpl<T, TypeTraits<T>::kTraitId>::unpack(ctx, in, out);
  }
  // Handle to handle (no conversion).
  template<>
  NJS_INLINE Result v8UnpackValue(Context& ctx, const v8::Local<v8::Value>& in, Value& out) noexcept {
    v8HandleOfValue(out) = in;
    return Globals::kResultOk;
  }
  // Handle to handle (no conversion).
  template<>
  NJS_INLINE Result v8UnpackValue(Context& ctx, const v8::Local<v8::Value>& in, v8::Local<v8::Value>& out) noexcept {
    out = in;
    return Globals::kResultOk;
  }

  template<typename T, typename Concept>
  NJS_INLINE Result v8UnpackWithConcept(Context& ctx, const Value& in, T& out, const Concept& concept) noexcept {
    return V8ConvertWithConceptImpl<T, Concept, Concept::kConceptType>::unpack(ctx, in, out, concept);
  }

  template<typename T>
  struct V8ReturnImpl {
    static NJS_INLINE Result set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const T& value) noexcept {
      v8::Local<v8::Value> intermediate;
      NJS_CHECK(V8ConvertImpl<T, TypeTraits<T>::kTraitId>::pack(ctx, value, intermediate));

      rv.Set(intermediate);
      return Globals::kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl< v8::Local<v8::Value> > {
    static NJS_INLINE Result set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const v8::Local<v8::Value>& value) noexcept {
      rv.Set(value);
      return Globals::kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl<Value> {
    static NJS_INLINE Result set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const Value& value) noexcept {
      rv.Set(v8HandleOfValue(value));
      return Globals::kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl<NullType> {
    static NJS_INLINE Result set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const NullType&) noexcept {
      rv.SetNull();
      return Globals::kResultOk;
    }
  };

  template<>
  struct V8ReturnImpl<UndefinedType> {
    static NJS_INLINE Result set(Context& ctx, v8::ReturnValue<v8::Value>& rv, const UndefinedType&) noexcept {
      rv.SetUndefined();
      return Globals::kResultOk;
    }
  };

  template<typename T>
  NJS_INLINE Result v8Return(Context& ctx, v8::ReturnValue<v8::Value>& rv, const T& value) noexcept {
    return V8ReturnImpl<T>::set(ctx, rv, value);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result v8ReturnWithConcept(Context& ctx, v8::ReturnValue<v8::Value>& rv, const T& value, const Concept& concept) noexcept;

  template<typename NativeT>
  NJS_INLINE Result v8WrapNative(Context& ctx, v8::Local<v8::Object> obj, NativeT* self, uint32_t objectTag) noexcept;

  template<typename NativeT>
  NJS_INLINE NativeT* v8UnwrapNativeUnsafe(Context& ctx, v8::Local<v8::Object> obj) noexcept;

  template<typename NativeT>
  NJS_INLINE Result v8UnwrapNativeChecked(Context& ctx, NativeT** pOut, v8::Local<v8::Value> obj, uint32_t objectTag) noexcept;
} // {Internal}

// ============================================================================
// [njs::Runtime]
// ============================================================================

class Runtime {
public:
  NJS_INLINE Runtime() noexcept : _isolate(nullptr) {}
  NJS_INLINE Runtime(const Runtime& other) noexcept : _isolate(other._isolate) {}
  explicit NJS_INLINE Runtime(v8::Isolate* isolate) noexcept : _isolate(isolate) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE v8::Isolate* v8Isolate() const noexcept { return _isolate; }

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

  NJS_INLINE Value() noexcept : _handle() {}
  NJS_INLINE Value(const Value& other) noexcept : _handle(other._handle) {}

  template<typename T>
  explicit NJS_INLINE Value(const v8::Local<T>& handle) noexcept : _handle(handle) {}

  // --------------------------------------------------------------------------
  // [Assignment]
  // --------------------------------------------------------------------------

  NJS_INLINE Value& operator=(const Value& other) noexcept {
    _handle = other._handle;
    return *this;
  }

  template<typename T>
  NJS_INLINE Value& operator=(const v8::Local<T>& handle) noexcept {
    _handle = handle;
    return *this;
  }

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  // Get the V8's handle.
  NJS_INLINE HandleType& v8Handle() noexcept { return _handle; }
  NJS_INLINE const HandleType& v8Handle() const noexcept { return _handle; }

  template<typename T>
  NJS_INLINE v8::Local<T> v8HandleAs() const noexcept { return v8::Local<T>::Cast(_handle); }

  template<typename T = v8::Value>
  NJS_INLINE T* v8Value() const noexcept { return T::Cast(*_handle); }

  // --------------------------------------------------------------------------
  // [Handle]
  // --------------------------------------------------------------------------

  // Get whether the handle is valid and can be used. If the handle is invalid
  // then no member function can be called on it. There are asserts that check
  // this in debug mode.
  NJS_INLINE bool isValid() const noexcept { return !_handle.IsEmpty(); }

  // Reset the handle.
  //
  // NOTE: `isValid()` will return `false` after `reset()` is called.
  NJS_INLINE void reset() noexcept { _handle.Clear(); }

  // --------------------------------------------------------------------------
  // [Type System]
  // --------------------------------------------------------------------------

  NJS_INLINE bool isUndefined() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsUndefined();
  }

  NJS_INLINE bool isNull() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsNull();
  }

  NJS_INLINE bool isBool() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsBoolean();
  }

  NJS_INLINE bool isTrue() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsTrue();
  }

  NJS_INLINE bool isFalse() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsFalse();
  }

  NJS_INLINE bool isInt32() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsInt32();
  }

  NJS_INLINE bool isUint32() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsUint32();
  }

  NJS_INLINE bool isNumber() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsNumber();
  }

  NJS_INLINE bool isString() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsString();
  }

  NJS_INLINE bool isSymbol() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsSymbol();
  }

  NJS_INLINE bool isArray() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsArray();
  }

  NJS_INLINE bool isObject() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsObject();
  }

  NJS_INLINE bool isDate() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsDate();
  }

  NJS_INLINE bool isRegExp() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsRegExp();
  }

  NJS_INLINE bool isFunction() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsFunction();
  }

  NJS_INLINE bool isExternal() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsExternal();
  }

  NJS_INLINE bool isPromise() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsPromise();
  }

  NJS_INLINE bool isArrayBuffer() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsArrayBuffer();
  }

  NJS_INLINE bool isArrayBufferView() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsArrayBufferView();
  }

  NJS_INLINE bool isDataView() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsDataView();
  }

  NJS_INLINE bool isTypedArray() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsTypedArray();
  }

  NJS_INLINE bool isInt8Array() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsInt8Array();
  }

  NJS_INLINE bool isInt16Array() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsInt16Array();
  }

  NJS_INLINE bool isInt32Array() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsInt32Array();
  }

  NJS_INLINE bool isUint8Array() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsUint8Array();
  }

  NJS_INLINE bool isUint8ClampedArray() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsUint8ClampedArray();
  }

  NJS_INLINE bool isUint16Array() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsUint16Array();
  }

  NJS_INLINE bool isUint32Array() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsUint32Array();
  }

  NJS_INLINE bool isFloat32Array() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsFloat32Array();
  }

  NJS_INLINE bool isFloat64Array() const noexcept {
    NJS_ASSERT(isValid());
    return _handle->IsFloat64Array();
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  HandleType _handle;
};

namespace Internal {
  static NJS_INLINE v8::Local<v8::Value>& v8HandleOfValue(Value& value) noexcept { return value._handle; }
  static NJS_INLINE const v8::Local<v8::Value>& v8HandleOfValue(const Value& value) noexcept { return value._handle; }
} // Internal namsepace

// ============================================================================
// [njs::Persistent]
// ============================================================================

class Persistent {
public:
  NJS_INLINE Persistent() noexcept
    : _handle() {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  // Get the V8's handle.
  NJS_INLINE v8::Persistent<v8::Value>& v8Handle() noexcept {
    return _handle;
  }

  NJS_INLINE const v8::Persistent<v8::Value>& v8Handle() const noexcept {
    return _handle;
  }

  // Get the V8's `Persistent<T>` handle.
  template<typename T>
  NJS_INLINE v8::Persistent<T>& v8HandleAs() noexcept {
    return reinterpret_cast<v8::Persistent<T>&>(_handle);
  }

  template<typename T>
  NJS_INLINE const v8::Persistent<T>& v8HandleAs() const noexcept {
    return reinterpret_cast<const v8::Persistent<T>&>(_handle);
  }

  /*
  template<typename T = v8::Value>
  NJS_INLINE T* v8Value() const noexcept {
    return T::Cast(*_handle);
  }
  */

  // --------------------------------------------------------------------------
  // [Handle]
  // --------------------------------------------------------------------------

  // Get whether the handle is valid and can be used. If the handle is invalid
  // then no member function can be called on it. There are asserts that check
  // this in debug mode.
  NJS_INLINE bool isValid() const noexcept { return !_handle.IsEmpty(); }

  // Reset the handle to its construction state.
  //
  // NOTE: `isValid()` returns return `false` after `reset()` is called.
  NJS_INLINE void reset() noexcept { _handle.Empty(); }

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
  NJS_INLINE Context& operator=(const Context& other) noexcept = delete;

public:
  NJS_INLINE Context() noexcept {}

  NJS_INLINE Context(const Context& other) noexcept
    : _runtime(other._runtime),
      _context(other._context) {}

  explicit NJS_INLINE Context(const Runtime& runtime) noexcept
    : _runtime(runtime),
      _context(runtime.v8Isolate()->GetCurrentContext()) {}

  NJS_INLINE Context(v8::Isolate* isolate, const v8::Local<v8::Context>& context) noexcept
    : _runtime(isolate),
      _context(context) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE v8::Isolate* v8Isolate() const noexcept { return _runtime._isolate; }
  NJS_INLINE v8::Local<v8::Context>& v8Context() noexcept { return _context; }
  NJS_INLINE const v8::Local<v8::Context>& v8Context() const noexcept { return _context; }

  // --------------------------------------------------------------------------
  // [Runtime]
  // --------------------------------------------------------------------------

  NJS_INLINE const Runtime& runtime() const noexcept { return _runtime; }

  // --------------------------------------------------------------------------
  // [Built-Ins]
  // --------------------------------------------------------------------------

  NJS_INLINE Value undefined() const noexcept { return Value(v8::Undefined(v8Isolate())); }
  NJS_INLINE Value null() const noexcept { return Value(v8::Null(v8Isolate())); }
  NJS_INLINE Value true_() const noexcept { return Value(v8::True(v8Isolate())); }
  NJS_INLINE Value false_() const noexcept { return Value(v8::False(v8Isolate())); }

  // --------------------------------------------------------------------------
  // [New]
  // --------------------------------------------------------------------------

  NJS_INLINE Value newBool(bool value) noexcept { return Value(v8::Boolean::New(v8Isolate(), value)); }
  NJS_INLINE Value newInt32(int32_t value) noexcept { return Value(v8::Integer::New(v8Isolate(), value)); }
  NJS_INLINE Value newUint32(uint32_t value) noexcept { return Value(v8::Integer::New(v8Isolate(), value)); }
  NJS_INLINE Value newDouble(double value) noexcept { return Value(v8::Number::New(v8Isolate(), value)); }
  NJS_INLINE Value newArray() noexcept { return Value(v8::Array::New(v8Isolate())); }
  NJS_INLINE Value newArray(uint32_t size) noexcept { return Value(v8::Array::New(v8Isolate(), int(size))); }
  NJS_INLINE Value newObject() noexcept { return Value(v8::Object::New(v8Isolate())); }

  NJS_INLINE Value newString() const noexcept { return Value(v8::String::Empty(v8Isolate())); }

  template<typename StrRefT>
  NJS_INLINE Value newString(const StrRefT& data) noexcept {
    return Value(Internal::v8NewString<StrRefT>(*this, data, v8::NewStringType::kNormal));
  }

  template<typename StrRefT>
  NJS_INLINE Value newInternalizedString(const StrRefT& data) noexcept {
    return Value(Internal::v8NewString<StrRefT>(*this, data, v8::NewStringType::kInternalized));
  }

  template<typename T>
  NJS_INLINE Value newValue(const T& value) noexcept {
    Value result;
    Internal::V8ConvertImpl<T, Internal::TypeTraits<T>::kTraitId>::pack(*this, value, result._handle);
    return result;
  }

  NJS_INLINE Value newFunction(NativeFunction nativeFunction, const Value& data) noexcept {
    return Value(
      Internal::v8LocalFromMaybe<v8::Function>(
        v8::Function::New(v8Context(), nativeFunction, data.v8Handle(), 0, v8::ConstructorBehavior::kThrow)));

    //v8::Local<v8::FunctionTemplate> fnTemplate = v8::FunctionTemplate::New(
    //  ctx.v8Isolate(), (v8::FunctionCallback)item.data, data);
    //NJS_CHECK(fnTemplate);

    //if (name.isValid())
    //  fnTemplate->SetClassName(name.v8HandleAs<v8::String>());
  }

  // --------------------------------------------------------------------------
  // [Unpacking]
  // --------------------------------------------------------------------------

  // These don't perform any conversion, the type of the JS value has to match
  // the type of the `out` argument. Some types are internally convertible as
  // JS doesn't differentiate between integer and floating point types.

  template<typename T>
  NJS_INLINE Result unpack(const Value& in, T& out) noexcept {
    return Internal::v8UnpackValue<T>(*this, in._handle, out);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result unpack(const Value& in, T& out, const Concept& concept) noexcept {
    return Internal::v8UnpackWithConcept<T, Concept>(*this, in, out, concept);
  }

  // --------------------------------------------------------------------------
  // [Wrap / Unwrap]
  // --------------------------------------------------------------------------

  template<typename NativeT>
  NJS_INLINE Result wrap(Value obj, NativeT* native) noexcept {
    return wrap<NativeT>(obj, native, NativeT::kObjectTag);
  }

  template<typename NativeT>
  NJS_INLINE Result wrap(Value obj, NativeT* native, uint32_t objectTag) noexcept {
    NJS_ASSERT(obj.isObject());

    return Internal::v8WrapNative<NativeT>(*this, obj.v8HandleAs<v8::Object>(), native, objectTag);
  }

  template<typename NativeT, typename... ARGS>
  NJS_INLINE Result wrapNew(Value obj, ARGS&&... args) noexcept {
    NJS_ASSERT(obj.isObject());

    NativeT* native = new (std::nothrow) NativeT(std::forward<ARGS>(args)...);
    if (!native)
      return Globals::kResultOutOfMemory;

    return Internal::v8WrapNative<NativeT>(*this, obj.v8HandleAs<v8::Object>(), native, NativeT::kObjectTag);
  }

  template<typename NativeT>
  NJS_INLINE NativeT* unwrapUnsafe(Value obj) noexcept {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());
    return Internal::v8UnwrapNativeUnsafe<NativeT>(*this, obj.v8HandleAs<v8::Object>());
  }

  template<typename NativeT>
  NJS_INLINE Result unwrap(NativeT** pOut, Value obj) noexcept {
    return Internal::v8UnwrapNativeChecked(*this, pOut, obj, NativeT::kObjectTag);
  }

  template<typename NativeT>
  NJS_INLINE Result unwrap(NativeT** pOut, Value obj, uint32_t objectTag) noexcept {
    return Internal::v8UnwrapNativeChecked(*this, pOut, obj, objectTag);
  }

  NJS_INLINE bool isWrapped(Value obj, uint32_t objectTag) noexcept {
    // This must be checked before.
    NJS_ASSERT(obj.isValid());

    if (!obj.isObject())
      return false;

    v8::Local<v8::Object> handle = obj.v8HandleAs<v8::Object>();
    return handle->InternalFieldCount() > 1 &&
           (uintptr_t)handle->GetAlignedPointerFromInternalField(1) == Internal::nativeTagFromObjectTag(objectTag);
  }

  template<typename NativeT>
  NJS_INLINE bool isWrapped(Value obj) noexcept {
    return isWrapped(obj, NativeT::kObjectTag);
  }

  // --------------------------------------------------------------------------
  // [Value]
  // --------------------------------------------------------------------------

  // IMPORTANT: These can only be called if the value can be casted to such types,
  // casting improper type will fail assertion in debug mode and cause apocalypse
  // in release mode.

  NJS_INLINE bool boolValue(const Value& value) noexcept {
    NJS_ASSERT(value.isValid());
    NJS_ASSERT(value.isBool());
    return value._handle->BooleanValue(v8Isolate());
  }

  NJS_INLINE int32_t int32Value(const Value& value) const noexcept {
    NJS_ASSERT(value.isValid());
    NJS_ASSERT(value.isInt32());
    v8::Maybe<int32_t> result = value._handle->Int32Value(_context);
    return result.FromMaybe(0);
  }

  NJS_INLINE uint32_t uint32Value(const Value& value) const noexcept {
    NJS_ASSERT(value.isValid());
    NJS_ASSERT(value.isUint32());
    v8::Maybe<uint32_t> result = value._handle->Uint32Value(_context);
    return result.FromMaybe(0);
  }

  NJS_INLINE double doubleValue(const Value& value) const noexcept {
    NJS_ASSERT(value.isValid());
    NJS_ASSERT(value.isDouble());
    v8::Maybe<double> result = value._handle->NumberValue(_context);
    return result.FromMaybe(std::numeric_limits<double>::quiet_NaN());
  }

  // --------------------------------------------------------------------------
  // [Language]
  // --------------------------------------------------------------------------

  NJS_INLINE Maybe<bool> equals(const Value& aAny, const Value& bAny) noexcept {
    NJS_ASSERT(aAny.isValid());
    NJS_ASSERT(bAny.isValid());

    v8::Maybe<bool> result = aAny._handle->Equals(_context, bAny._handle);
    return Maybe<bool>(result.IsJust() ? Globals::kResultOk : Globals::kResultBypass, result.FromMaybe(false));
  }

  NJS_INLINE bool strictEquals(const Value& aAny, const Value& bAny) const noexcept {
    NJS_ASSERT(aAny.isValid());
    NJS_ASSERT(bAny.isValid());
    return aAny._handle->StrictEquals(bAny._handle);
  }

  NJS_INLINE bool isSameValue(const Value& aAny, const Value& bAny) const noexcept {
    NJS_ASSERT(aAny.isValid());
    NJS_ASSERT(bAny.isValid());
    return aAny._handle->SameValue(bAny._handle);
  }

  // --------------------------------------------------------------------------
  // [String]
  // --------------------------------------------------------------------------

  NJS_INLINE bool isLatin1(const Value& value) const noexcept {
    NJS_ASSERT(value.isValid());
    NJS_ASSERT(value.isString());
    return value.v8Value<v8::String>()->ContainsOnlyOneByte();
  }

  NJS_INLINE bool isLatin1Guess(const Value& value) const noexcept {
    NJS_ASSERT(value.isValid());
    NJS_ASSERT(value.isString());
    return value.v8Value<v8::String>()->IsOneByte();
  }

  // Length of the string if represented as UTF-16 or LATIN-1 (if representable).
  NJS_INLINE size_t stringLength(const Value& value) const noexcept {
    NJS_ASSERT(value.isValid());
    NJS_ASSERT(value.isString());
    return value.v8Value<v8::String>()->Length();
  }

  // Length of the string if represented as UTF-8.
  NJS_INLINE size_t utf8Length(const Value& value) const noexcept {
    NJS_ASSERT(value.isValid());
    NJS_ASSERT(value.isString());
    return value.v8Value<v8::String>()->Utf8Length(v8Isolate());
  }

  NJS_INLINE int readLatin1(const Value& value, char* out, int size = -1) const noexcept {
    NJS_ASSERT(value.isValid());
    NJS_ASSERT(value.isString());
    return value.v8Value<v8::String>()->WriteOneByte(v8Isolate(), reinterpret_cast<uint8_t*>(out), 0, size, v8::String::NO_NULL_TERMINATION);
  }

  NJS_INLINE int readUtf8(const Value& value, char* out, int size = -1) const noexcept {
    NJS_ASSERT(value.isValid());
    NJS_ASSERT(value.isString());
    return value.v8Value<v8::String>()->WriteUtf8(v8Isolate(), out, size, nullptr, v8::String::NO_NULL_TERMINATION);
  }

  NJS_INLINE int readUtf16(const Value& value, uint16_t* out, int size = -1) const noexcept {
    NJS_ASSERT(value.isValid());
    NJS_ASSERT(value.isString());
    return value.v8Value<v8::String>()->Write(v8Isolate(), out, 0, size, v8::String::NO_NULL_TERMINATION);
  }

  // Concatenate two strings.
  NJS_INLINE Value concatStrings(const Value& aStr, const Value& bStr) noexcept {
    NJS_ASSERT(aStr.isValid());
    NJS_ASSERT(aStr.isString());
    NJS_ASSERT(bStr.isValid());
    NJS_ASSERT(bStr.isString());

    return Value(v8::String::Concat(v8Isolate(), aStr.v8HandleAs<v8::String>(), bStr.v8HandleAs<v8::String>()));
  }

  // --------------------------------------------------------------------------
  // [Array]
  // --------------------------------------------------------------------------

  NJS_INLINE size_t arrayLength(const Value& value) const noexcept {
    NJS_ASSERT(isValid());
    NJS_ASSERT(isArray());
    return value.v8Value<v8::Array>()->Length();
  }

  // --------------------------------------------------------------------------
  // [Object]
  // --------------------------------------------------------------------------

  NJS_INLINE Maybe<bool> hasProperty(const Value& obj, const Value& key) noexcept {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());
    NJS_ASSERT(key.isValid());

    v8::Maybe<bool> result = obj.v8Value<v8::Object>()->Has(_context, key._handle);
    return Maybe<bool>(result.IsJust() ? Globals::kResultOk : Globals::kResultBypass, result.FromMaybe(false));
  }

  NJS_INLINE Maybe<bool> hasPropertyAt(const Value& obj, uint32_t index) noexcept {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());

    v8::Maybe<bool> result = obj.v8Value<v8::Object>()->Has(_context, index);
    return Maybe<bool>(result.IsJust() ? Globals::kResultOk : Globals::kResultBypass, result.FromMaybe(false));
  }

  NJS_INLINE Value propertyOf(const Value& obj, const Value& key) noexcept {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());
    NJS_ASSERT(key.isValid());

    return Value(
      Internal::v8LocalFromMaybe(
        obj.v8Value<v8::Object>()->Get(_context, key._handle)));
  }

  template<typename StrRefT>
  NJS_NOINLINE Value propertyOfT(Value obj, StrRefT key) noexcept {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());

    njs::Value keyValue = newString(key);
    if (!keyValue.isValid())
      return keyValue;

    return Value(
      Internal::v8LocalFromMaybe(
        obj.v8Value<v8::Object>()->Get(_context, keyValue._handle)));
  }

  NJS_INLINE Value propertyOf(const Value& obj, const Utf8Ref& key) noexcept { return propertyOfT(obj, key); }
  NJS_INLINE Value propertyOf(const Value& obj, const Utf16Ref& key) noexcept { return propertyOfT(obj, key); }
  NJS_INLINE Value propertyOf(const Value& obj, const Latin1Ref& key) noexcept { return propertyOfT(obj, key); }

  NJS_INLINE Value propertyAt(const Value& obj, uint32_t index) noexcept {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());

    return Value(
      Internal::v8LocalFromMaybe(
        obj.v8Value<v8::Object>()->Get(_context, index)));
  }

  NJS_INLINE Result setProperty(const Value& obj, const Value& key, const Value& val) noexcept {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());
    NJS_ASSERT(key.isValid());
    NJS_ASSERT(val.isValid());

    v8::Maybe<bool> result = obj.v8Value<v8::Object>()->Set(_context, key._handle, val._handle);
    return result.FromMaybe(false) ? Globals::kResultOk : Globals::kResultBypass;
  }

  template<typename StrRefT>
  NJS_NOINLINE Result setPropertyT(Value obj, StrRefT key, Value val) noexcept {
    Value keyValue = newInternalizedString(key);
    NJS_CHECK(keyValue);
    return setProperty(obj, keyValue, val);
  }

  NJS_INLINE Result setProperty(const Value& obj, const Utf8Ref& key, const Value& val) noexcept { return setPropertyT(obj, key, val); }
  NJS_INLINE Result setProperty(const Value& obj, const Utf16Ref& key, const Value& val) noexcept { return setPropertyT(obj, key, val); }
  NJS_INLINE Result setProperty(const Value& obj, const Latin1Ref& key, const Value& val) noexcept { return setPropertyT(obj, key, val); }

  NJS_INLINE Result setPropertyAt(const Value& obj, uint32_t index, const Value& val) noexcept {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(obj.isObject());
    NJS_ASSERT(val.isValid());

    v8::Maybe<bool> result = obj.v8Value<v8::Object>()->Set(_context, index, val._handle);
    return result.FromMaybe(false) ? Globals::kResultOk : Globals::kResultBypass;
  }

  NJS_INLINE Value newInstance(const Value& ctor) noexcept {
    NJS_ASSERT(ctor.isValid());
    NJS_ASSERT(ctor.isFunction());

    return Value(
      Internal::v8LocalFromMaybe(
        ctor.v8Value<v8::Function>()->NewInstance(_context)));
  }

  template<typename... ARGS>
  NJS_INLINE Value newInstance(const Value& ctor, ARGS&&... args) noexcept {
    Value argv[] = { std::forward<ARGS>(args)... };

    return Value(
      Internal::v8LocalFromMaybe(
        ctor.v8Value<v8::Function>()->NewInstance(
          _context,
          static_cast<int>(sizeof(argv) / sizeof(argv[0])),
          reinterpret_cast<v8::Local<v8::Value>*>(argv))));
  }

  NJS_INLINE Value newInstanceArgv(const Value& ctor, size_t argc, const Value* argv) noexcept {
    NJS_ASSERT(ctor.isValid());
    NJS_ASSERT(ctor.isFunction());

    return Value(
      Internal::v8LocalFromMaybe(
        ctor.v8Value<v8::Function>()->NewInstance(
          _context,
          static_cast<int>(argc),
          const_cast<v8::Local<v8::Value>*>(reinterpret_cast<const v8::Local<v8::Value>*>(argv)))));
  }

  // --------------------------------------------------------------------------
  // [Function]
  // --------------------------------------------------------------------------

  NJS_INLINE void setFunctionName(const Value& function, const Value& name) noexcept {
    NJS_ASSERT(isFunction(function));
    NJS_ASSERT(isString(name));

    function.v8HandleAs<v8::Function>()->SetName(name.v8HandleAs<v8::String>());
  }

  // --------------------------------------------------------------------------
  // [Call]
  // --------------------------------------------------------------------------

  NJS_INLINE Value call(const Value& function, const Value& recv) noexcept {
    return callArgv(function, recv, 0, nullptr);
  }

  template<typename... ARGS>
  NJS_INLINE Value call(const Value& function, const Value& recv, ARGS&&... args) noexcept {
    Value argv[] = { std::forward<ARGS>(args)... };
    return callArgv(function, recv, sizeof(argv) / sizeof(argv[0]), argv);
  }

  NJS_INLINE Value callArgv(const Value& function, const Value& recv, size_t argc, const Value* argv) noexcept {
    NJS_ASSERT(function.isValid());
    NJS_ASSERT(function.isFunction());
    NJS_ASSERT(recv.isValid());

    return Value(
      Internal::v8LocalFromMaybe(
        function.v8Value<v8::Function>()->Call(
          _context, recv._handle,
          static_cast<int>(argc),
          const_cast<v8::Local<v8::Value>*>(reinterpret_cast<const v8::Local<v8::Value>*>(argv)))));
  }

  // --------------------------------------------------------------------------
  // [Exception]
  // --------------------------------------------------------------------------

  NJS_NOINLINE Value newException(uint32_t type, Value msg) noexcept {
    NJS_ASSERT(msg.isString());
    switch (type) {
      default:
      case Globals::kExceptionError         : return Value(v8::Exception::Error(msg.v8HandleAs<v8::String>()));
      case Globals::kExceptionTypeError     : return Value(v8::Exception::TypeError(msg.v8HandleAs<v8::String>()));
      case Globals::kExceptionRangeError    : return Value(v8::Exception::RangeError(msg.v8HandleAs<v8::String>()));
      case Globals::kExceptionSyntaxError   : return Value(v8::Exception::SyntaxError(msg.v8HandleAs<v8::String>()));
      case Globals::kExceptionReferenceError: return Value(v8::Exception::ReferenceError(msg.v8HandleAs<v8::String>()));
    }
  }

  // --------------------------------------------------------------------------
  // [Throw]
  // --------------------------------------------------------------------------

  NJS_INLINE Result Throw(Value exception) noexcept {
    v8Isolate()->ThrowException(exception._handle);
    return Globals::kResultBypass;
  }

  NJS_NOINLINE Result throwNewException(uint32_t type, Value msg) noexcept {
    NJS_ASSERT(msg.isString());
    Value exception = newException(type, msg);
    v8Isolate()->ThrowException(exception._handle);
    return Globals::kResultBypass;
  }

  NJS_NOINLINE Result throwNewException(uint32_t type, const char* fmt, ...) noexcept {
    char buffer[Globals::kMaxBufferSize];

    va_list ap;
    va_start(ap, fmt);
    unsigned int size = StrUtils::vsformat(buffer, Globals::kMaxBufferSize, fmt, ap);
    va_end(ap);

    Value exception = newException(type, newString(Utf8Ref(buffer, size)));
    v8Isolate()->ThrowException(exception._handle);
    return Globals::kResultBypass;
  }

  // --------------------------------------------------------------------------
  // [Local <-> Persistent]
  // --------------------------------------------------------------------------

  NJS_INLINE Value makeLocal(const Persistent& persistent) noexcept {
    return Value(v8::Local<v8::Value>::New(v8Isolate(), persistent._handle));
  }

  NJS_INLINE Result makePersistent(const Value& local, Persistent& persistent) noexcept {
    persistent._handle.Reset(v8Isolate(), local._handle);
    return Globals::kResultOk;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! V8's isolate.
  Runtime _runtime;
  //! V8's context.
  v8::Local<v8::Context> _context;
};

static NJS_INLINE v8::Isolate* Internal::v8IsolateOfContext(Context& ctx) noexcept {
  return ctx.v8Isolate();
}

// ============================================================================
// [njs::HandleScope]
// ============================================================================

class HandleScope {
public:
  NJS_NONCOPYABLE(HandleScope)

  explicit NJS_INLINE HandleScope(const Context& ctx) noexcept
    : _handleScope(ctx.v8Isolate()) {}

  explicit NJS_INLINE HandleScope(const Runtime& runtime) noexcept
    : _handleScope(runtime.v8Isolate()) {}

  explicit NJS_INLINE HandleScope(v8::Isolate* isolate) noexcept
    : _handleScope(isolate) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE v8::HandleScope& v8HandleScope() noexcept { return _handleScope; }

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
public:
  NJS_NONCOPYABLE(ScopedContext)

  NJS_INLINE ScopedContext(const Context& other) noexcept
    : HandleScope(other),
      Context(other) {}

  explicit NJS_INLINE ScopedContext(const Runtime& runtime) noexcept
    : HandleScope(runtime),
      Context(runtime) {}

  NJS_INLINE ScopedContext(v8::Isolate* isolate, const v8::Local<v8::Context>& handle) noexcept
    : HandleScope(isolate),
      Context(isolate, handle) {}
};

// ============================================================================
// [njs::ExecutionContext]
// ============================================================================

class ExecutionContext
  : public Context,
    public ResultMixin {
public:
  NJS_NONCOPYABLE(ExecutionContext)

  NJS_INLINE ExecutionContext() noexcept : Context() {}
  NJS_INLINE ExecutionContext(const Context& other) noexcept : Context(other) {}
  NJS_INLINE ExecutionContext(v8::Isolate* isolate, const v8::Local<v8::Context>& handle) noexcept : Context(isolate, handle) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  //! Handle a `result` returned from a native function (binding).
  NJS_INLINE void _handleResult(Result result) noexcept {
    if (result != Globals::kResultOk)
      Internal::reportError<Context>(*this, result, _payload);
  }
};

// ============================================================================
// [njs::GetPropertyContext]
// ============================================================================

class GetPropertyContext : public ExecutionContext {
public:
  // Creates the GetPropertyContext directly from V8's `PropertyCallbackInfo<Value>`.
  explicit NJS_INLINE GetPropertyContext(const v8::PropertyCallbackInfo<v8::Value>& info) noexcept
    : ExecutionContext(info.GetIsolate(), info.GetIsolate()->GetCurrentContext()),
      _info(info) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE const v8::PropertyCallbackInfo<v8::Value>& v8CallbackInfo() const noexcept { return _info; }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_INLINE Value This() const noexcept { return Value(_info.This()); }
  NJS_INLINE Value data() const noexcept { return Value(_info.Data()); }

  // --------------------------------------------------------------------------
  // [Return]
  // --------------------------------------------------------------------------

  template<typename T>
  NJS_INLINE Result returnValue(const T& value) noexcept {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return Internal::v8Return<T>(static_cast<Context&>(*this), rv, value);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result returnValue(const T& value, const Concept& concept) noexcept {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return Internal::v8ReturnWithConcept<T, Concept>(*this, rv, value, concept);
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
                                const v8::Local<v8::Value>& value) noexcept
    : ExecutionContext(info.GetIsolate(), info.GetIsolate()->GetCurrentContext()),
      _info(info),
      _propertyValue(value) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE const v8::PropertyCallbackInfo<void>& v8CallbackInfo() const noexcept { return _info; }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_INLINE Value This() const noexcept { return Value(_info.This()); }
  NJS_INLINE Value data() const noexcept { return Value(_info.Data()); }

  NJS_INLINE Value propertyValue() const noexcept { return _propertyValue; }

  template<typename T>
  NJS_INLINE Result unpackValue(T& out) noexcept {
    return Internal::v8UnpackValue<T>(*this, _propertyValue._handle, out);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result unpackValue(T& out, const Concept& concept) noexcept {
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
  explicit NJS_INLINE FunctionCallContext(const v8::FunctionCallbackInfo<v8::Value>& info) noexcept
    : ExecutionContext(info.GetIsolate(), info.GetIsolate()->GetCurrentContext()),
      _info(info) {}

  // --------------------------------------------------------------------------
  // [V8-Specific]
  // --------------------------------------------------------------------------

  NJS_INLINE const v8::FunctionCallbackInfo<v8::Value>& v8CallbackInfo() const noexcept {
    return _info;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  NJS_INLINE Value This() const noexcept { return Value(_info.This()); }
  NJS_INLINE Value data() const noexcept { return Value(_info.Data()); }

  NJS_INLINE bool isConstructCall() const noexcept { return _info.IsConstructCall(); }

  // --------------------------------------------------------------------------
  // [Arguments]
  // --------------------------------------------------------------------------

  NJS_INLINE unsigned int argumentsLength() const noexcept {
    return static_cast<unsigned int>(_info.Length());
  }

  NJS_INLINE Result verifyArgumentsLength(unsigned int numArgs) noexcept {
    if (argumentsLength() != numArgs)
      return invalidArgumentsLength(numArgs);
    else
      return Globals::kResultOk;
  }

  NJS_INLINE Result verifyArgumentsLength(unsigned int minArgs, unsigned int maxArgs) noexcept {
    unsigned int numArgs = argumentsLength();
    if (numArgs < minArgs || numArgs > maxArgs)
      return invalidArgumentsLength(minArgs, maxArgs);
    else
      return Globals::kResultOk;
  }

  NJS_INLINE Value argumentAt(unsigned int index) const noexcept {
    return Value(_info.operator[](static_cast<int>(index)));
  }

  // Like `unwrap()`, but accepts an argument index instead of `Value`.
  template<typename NativeT>
  NJS_INLINE Result unwrapArgument(unsigned int index, NativeT** pOut) noexcept {
    return Internal::v8UnwrapNativeChecked<NativeT>(*this, pOut, _info[static_cast<int>(index)], NativeT::kObjectTag);
  }

  // Like `unpack()`, but accepts an argument index instead of `Value`.
  template<typename T>
  NJS_INLINE Result unpackArgument(unsigned int index, T& out) noexcept {
    return Internal::v8UnpackValue<T>(*this, _info[static_cast<int>(index)], out);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result unpackArgument(unsigned int index, T& out, const Concept& concept) noexcept {
    return Internal::v8UnpackWithConcept<T, Concept>(*this, argumentAt(index), out, concept);
  }

  // --------------------------------------------------------------------------
  // [Return]
  // --------------------------------------------------------------------------

  template<typename T>
  NJS_INLINE Result returnValue(const T& value) noexcept {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return Internal::v8Return<T>(static_cast<Context&>(*this), rv, value);
  }

  template<typename T, typename Concept>
  NJS_INLINE Result returnValue(const T& value, const Concept& concept) noexcept {
    v8::ReturnValue<v8::Value> rv = _info.GetReturnValue();
    return Internal::v8ReturnWithConcept<T, Concept>(*this, rv, value, concept);
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
  explicit NJS_INLINE ConstructCallContext(const v8::FunctionCallbackInfo<v8::Value>& info) noexcept
    : FunctionCallContext(info) {}

  // --------------------------------------------------------------------------
  // [ReturnWrap / ReturnNew]
  // --------------------------------------------------------------------------

  template<typename NativeT>
  NJS_INLINE Result returnWrap(NativeT* native) noexcept {
    Value obj = This();

    NJS_CHECK(wrap<NativeT>(obj, native));
    return returnValue(obj);
  }

  template<typename NativeT, typename... ARGS>
  NJS_INLINE Result returnNew(ARGS&&... args) noexcept {
    Value obj = This();

    NJS_CHECK(wrapNew<NativeT>(obj, std::forward<ARGS>(args)...));
    return returnValue(obj);
  }
};

// ============================================================================
// [njs::WrapData]
// ============================================================================

// A low-level C++ class wrapping functionality.
//
// Very similar to `node::ObjectWrap` with few differences:
//   - It isn't a base class, it's used as a member instead.
//   - It can be used to wrap any class, even those you can only inherit from.
//   - It doesn't have virtual functions and is not intended to be inherited.
//
// An instance of `WrapData` should always be named `_wrapData` in the class
// it's used.
class WrapData {
public:
  NJS_INLINE WrapData() noexcept
    : _refCount(0),
      _object(),
      _destroyCallback(nullptr) {}

  NJS_INLINE WrapData(Context& ctx, Value obj, Internal::V8WrapDestroyCallback destroyCallback) noexcept
    : _refCount(0),
      _destroyCallback(destroyCallback) {
    NJS_ASSERT(obj.isObject());
    _object._handle.Reset(ctx.v8Isolate(), obj._handle);
    obj.v8Value<v8::Object>()->SetAlignedPointerInInternalField(0, this);
  }

  NJS_NOINLINE ~WrapData() noexcept {
    if (_object.isValid()) {
      NJS_ASSERT(_object._handle.IsNearDeath());
      _object._handle.ClearWeak();
      _object._handle.Reset();
    }
  }

  // ------------------------------------------------------------------------
  // [V8-Specific]
  // ------------------------------------------------------------------------

  NJS_INLINE v8::Persistent<v8::Value>& v8Handle() noexcept { return _object._handle; }
  NJS_INLINE const v8::Persistent<v8::Value>& v8Handle() const noexcept { return _object._handle; }

  // ------------------------------------------------------------------------
  // [Handle]
  // ------------------------------------------------------------------------

  NJS_INLINE bool isValid() const noexcept { return _object.isValid(); }

  NJS_INLINE Persistent& object() noexcept { return _object; }
  NJS_INLINE const Persistent& object() const noexcept { return _object; }

  // ------------------------------------------------------------------------
  // [RefCount]
  // ------------------------------------------------------------------------

  NJS_INLINE size_t refCount() const noexcept {
    return _refCount;
  }

  NJS_INLINE void addRef() noexcept {
    NJS_ASSERT(isValid());

    _refCount++;
    _object._handle.ClearWeak();
  }

  NJS_INLINE void release(void* self) noexcept {
    NJS_ASSERT(_refCount > 0);
    NJS_ASSERT(isValid());
    NJS_ASSERT(!isWeak());

    if (--_refCount == 0)
      makeWeak(self);
  }

  NJS_INLINE bool isWeak() const noexcept {
    return _object._handle.IsWeak();
  }

  NJS_INLINE void makeWeak(void* self) noexcept {
    NJS_ASSERT(isValid());

    _object._handle.SetWeak<void>(self, _destroyCallback, v8::WeakCallbackType::kFinalizer);
    // TODO: Deprecated
    // _object._handle.MarkIndependent();
  }

  // ------------------------------------------------------------------------
  // [Statics]
  // ------------------------------------------------------------------------

  template<typename T>
  static void NJS_NOINLINE destroyCallbackT(const v8::WeakCallbackInfo<void>& data) noexcept {
    T* self = (T*)(data.GetParameter());

    NJS_ASSERT(self->_wrapData._refCount == 0);
    NJS_ASSERT(self->_wrapData._object._handle.IsNearDeath());

    self->_wrapData._object._handle.Reset();
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

// ============================================================================
// [njs::Internal]
// ============================================================================

namespace Internal {
  template<typename T, typename Concept>
  NJS_INLINE Result v8ReturnWithConcept(Context& ctx, v8::ReturnValue<v8::Value>& rv, const T& value, const Concept& concept) noexcept {
    Value intermediate;
    NJS_CHECK(V8ConvertWithConceptImpl<T, Concept, Concept::kConceptType>::pack(ctx, value, intermediate, concept));

    rv.Set(v8HandleOfValue(intermediate));
    return Globals::kResultOk;
  }

  template<typename NativeT>
  NJS_INLINE Result v8WrapNative(Context& ctx, v8::Local<v8::Object> obj, NativeT* native, uint32_t objectTag) noexcept {
    // Should be never called on an already initialized data.
    NJS_ASSERT(!native->_wrapData._object.isValid());
    NJS_ASSERT(!native->_wrapData._destroyCallback);

    // Ensure the object was properly configured and contains internal fields.
    NJS_ASSERT(obj->InternalFieldCount() > 1);

    native->_wrapData._object._handle.Reset(ctx.v8Isolate(), obj);
    native->_wrapData._destroyCallback =(Internal::V8WrapDestroyCallback)(WrapData::destroyCallbackT<NativeT>);

    obj->SetAlignedPointerInInternalField(0, native);
    obj->SetAlignedPointerInInternalField(1, (void*)Internal::nativeTagFromObjectTag(objectTag));

    native->_wrapData.makeWeak(native);
    return Globals::kResultOk;
  }

  template<typename NativeT>
  NJS_INLINE NativeT* v8UnwrapNativeUnsafe(Context& ctx, v8::Local<v8::Object> obj) noexcept {
    NJS_ASSERT(obj->InternalFieldCount() > 1);

    void* nativeObj = obj->GetAlignedPointerFromInternalField(0);
    return static_cast<NativeT*>(static_cast<typename NativeT::Base*>(nativeObj));
  }

  template<typename NativeT>
  NJS_INLINE Result v8UnwrapNativeChecked(Context& ctx, NativeT** pOut, v8::Local<v8::Value> value, uint32_t objectTag) noexcept {
    *pOut = nullptr;
    if (!value->IsObject())
      return Globals::kResultInvalidValue;

    v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(value);
    if (obj->InternalFieldCount() < 2)
      return Globals::kResultInvalidValue;

    void* nativeObj = obj->GetAlignedPointerFromInternalField(0);
    uintptr_t nativeTag = (uintptr_t)obj->GetAlignedPointerFromInternalField(1);

    if (nativeTag != Internal::nativeTagFromObjectTag(objectTag))
      return Globals::kResultInvalidValue;

    *pOut = static_cast<NativeT*>(static_cast<typename NativeT::Base*>(nativeObj));
    return Globals::kResultOk;
  }

  static NJS_NOINLINE Result v8BindClassHelper(
    Context& ctx,
    Value exports,
    v8::Local<v8::FunctionTemplate> classObj,
    v8::Local<v8::String> className,
    const BindingItem* items, unsigned int count) noexcept {

    v8::Local<v8::ObjectTemplate> prototype = classObj->PrototypeTemplate();
    v8::Local<v8::Signature> methodSignature;
    v8::Local<v8::AccessorSignature> accessorSignature;

    for (unsigned int i = 0; i < count; i++) {
      const BindingItem& item = items[i];

      Value name = ctx.newInternalizedString(Latin1Ref(item.name));
      NJS_CHECK(name);

      switch (item.type) {
        case BindingItem::kTypeStatic: {
          v8::Local<v8::FunctionTemplate> fnTemplate = v8::FunctionTemplate::New(
            ctx.v8Isolate(), (v8::FunctionCallback)item.data, exports.v8HandleAs<v8::Value>());
          fnTemplate->SetClassName(name.v8HandleAs<v8::String>());
          classObj->Set(name.v8HandleAs<v8::String>(), fnTemplate);
          break;
        }

        case BindingItem::kTypeMethod: {
          // Signature is only created when needed and then cached.
          if (methodSignature.IsEmpty())
            methodSignature = v8::Signature::New(ctx.v8Isolate(), classObj);

          v8::Local<v8::FunctionTemplate> fnTemplate = v8::FunctionTemplate::New(
            ctx.v8Isolate(), (v8::FunctionCallback)item.data, exports.v8HandleAs<v8::Value>(), methodSignature);

          fnTemplate->SetClassName(name.v8HandleAs<v8::String>());
          prototype->Set(name.v8HandleAs<v8::String>(), fnTemplate);
          break;
        }

        case BindingItem::kTypeGetter:
        case BindingItem::kTypeSetter: {
          unsigned int pairedType;
          int attr = v8::DontEnum | v8::DontDelete;

          v8::AccessorGetterCallback getter = nullptr;
          v8::AccessorSetterCallback setter = nullptr;

          if (item.type == BindingItem::kTypeGetter) {
            getter = (v8::AccessorGetterCallback)item.data;
            pairedType = BindingItem::kTypeSetter;
          }
          else {
            setter = (v8::AccessorSetterCallback)item.data;
            pairedType = BindingItem::kTypeGetter;
          }

          const BindingItem& nextItem = items[i + 1];
          if (count - i > 1 && nextItem.type == pairedType && strcmp(item.name, nextItem.name) == 0) {
            if (!getter)
              getter = (v8::AccessorGetterCallback)nextItem.data;
            else
              setter = (v8::AccessorSetterCallback)nextItem.data;
            i++;
          }

          if (!getter) {
            // TODO: Don't know what to do here...
            NJS_ASSERT(!"NJS_BIND_SET() used without having a getter before or after it!");
            ::abort();
          }

          // Signature is only created when needed and then cached.
          if (accessorSignature.IsEmpty())
            accessorSignature = v8::AccessorSignature::New(ctx.v8Isolate(), classObj);

          if (!setter)
            attr |= v8::ReadOnly;

          prototype->SetAccessor(
            name.v8HandleAs<v8::String>(), getter, setter, exports.v8HandleAs<v8::Value>(), v8::DEFAULT, static_cast<v8::PropertyAttribute>(attr), accessorSignature);
          break;
        }

        default: {
          NJS_ASSERT(!"Unhandled binding item type.");
          break;
        }
      }
    }

    return Globals::kResultOk;
  }

  template<typename NativeT>
  struct V8ClassBindings {
    typedef typename NativeT::Base Base;
    typedef typename NativeT::Type Type;

    static NJS_NOINLINE v8::Local<v8::FunctionTemplate> Init(
      Context& ctx,
      Value exports,
      v8::Local<v8::FunctionTemplate> superObj = v8::Local<v8::FunctionTemplate>()) noexcept {

      v8::Local<v8::FunctionTemplate> classObj = v8::FunctionTemplate::New(
        ctx.v8Isolate(), Type::Bindings::ConstructorEntry);

      v8::HandleScope (ctx.v8Isolate());
      if (!superObj.IsEmpty())
        classObj->Inherit(superObj);

      Value className = ctx.newInternalizedString(Latin1Ref(Type::staticClassName()));
      classObj->SetClassName(className.v8HandleAs<v8::String>());
      classObj->InstanceTemplate()->SetInternalFieldCount(2);

      // Type::Bindings is in fact an array of `BindingItem`s.
      typename Type::Bindings bindingItems;

      v8BindClassHelper(ctx, exports,
        classObj,
        className.v8HandleAs<v8::String>(),
        reinterpret_cast<const BindingItem*>(&bindingItems),
        sizeof(bindingItems) / sizeof(BindingItem));

      v8::MaybeLocal<v8::Function> fn = classObj->GetFunction(ctx.v8Context());
      ctx.setProperty(exports, className, Value(fn.ToLocalChecked()));
      return classObj;
    }
  };
} // {Internal}

// ============================================================================
// [njs::Wrap]
// ============================================================================

template<typename T>
class Wrap {
public:
  typedef T NativeType;

  struct Bindings;
  friend struct Bindings;

  template<typename... ARGS>
  NJS_INLINE Wrap(ARGS&&... args)
    : _native(std::forward<ARGS>(args)...) {}

  template<typename OtherT>
  NJS_INLINE Wrap<OtherT>* naturalCast() noexcept {
    Internal::checkNaturalCast<OtherT, T>(&_native);
    return reinterpret_cast<Wrap<OtherT>*>(this);
  }

  template<typename OtherT>
  NJS_INLINE const Wrap<OtherT>* naturalCast() const noexcept {
    Internal::checkNaturalCast<OtherT, T>(&_native);
    return reinterpret_cast<const Wrap<OtherT>*>(this);
  }

  template<typename OtherT>
  NJS_INLINE Wrap<OtherT>* staticCast() noexcept {
    Internal::checkStaticCast<OtherT, T>(&_native);
    return reinterpret_cast<Wrap<OtherT>*>(this);
  }

  template<typename OtherT>
  NJS_INLINE const Wrap<OtherT>* staticCast() const noexcept {
    Internal::checkStaticCast<OtherT, T>(&_native);
    return reinterpret_cast<const Wrap<OtherT>*>(this);
  }

  WrapData _wrapData;
  NativeType _native;
};

// ============================================================================
// [njs::Node]
// ============================================================================

#if defined(NJS_INTEGRATE_NODE)
namespace Node {
  static NJS_INLINE Value newBuffer(Context& ctx, size_t size) noexcept {
    return Value(
      Internal::v8LocalFromMaybe<v8::Object>(
        ::node::Buffer::New(ctx.v8Isolate(), size)));
  }

  static NJS_INLINE Value newBuffer(Context& ctx, void* data, size_t size) noexcept {
    return Value(
      Internal::v8LocalFromMaybe<v8::Object>(
        ::node::Buffer::New(ctx.v8Isolate(), static_cast<char*>(data), size)));
  }

  static NJS_INLINE Value newBuffer(Context& ctx, void* data, size_t size, node::Buffer::FreeCallback cb, void* hint) noexcept {
    return Value(
      Internal::v8LocalFromMaybe<v8::Object>(
        ::node::Buffer::New(ctx.v8Isolate(), static_cast<char*>(data), size, cb, hint)));
  }

  static NJS_INLINE bool isBuffer(const Value& obj) noexcept {
    NJS_ASSERT(obj.isValid());
    return ::node::Buffer::HasInstance(obj._handle);
  }

  static NJS_INLINE void* bufferData(const Value& obj) noexcept {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(isBuffer(obj));
    return ::node::Buffer::Data(obj.v8HandleAs<v8::Object>());
  }

  static NJS_INLINE size_t bufferSize(const Value& obj) noexcept {
    NJS_ASSERT(obj.isValid());
    NJS_ASSERT(isBuffer(obj));
    return ::node::Buffer::Length(obj.v8HandleAs<v8::Object>());
  }
} // Node namespace
#endif // NJS_INTEGRATE_NODE

// ============================================================================
// [NJS_MODULE - Declarative Interface]
// ============================================================================

#define NJS_MODULE(NAME)                                                      \
  static NJS_INLINE void InitImpl_##NAME(                                     \
    ::njs::Context& ctx,                                                      \
    ::njs::Value module,                                                      \
    ::njs::Value exports) noexcept;                                           \
                                                                              \
  extern "C" void InitEntry_##NAME(                                           \
      ::v8::Local<v8::Object> exports,                                        \
      ::v8::Local<v8::Value> module,                                          \
      ::v8::Local<v8::Context> v8ctx,                                         \
      void* priv) noexcept {                                                  \
    ::njs::Context ctx(v8ctx->GetIsolate(), v8ctx);                           \
    InitImpl_##NAME(ctx, ::njs::Value(module), ::njs::Value(exports));        \
  }                                                                           \
  NODE_MODULE_CONTEXT_AWARE(NAME, InitEntry_##NAME)                           \
                                                                              \
  static NJS_INLINE void InitImpl_##NAME(                                     \
    ::njs::Context& ctx,                                                      \
    ::njs::Value module,                                                      \
    ::njs::Value exports) noexcept

// TODO:
// (_T [, _Super])
#define NJS_INIT_CLASS(SELF, ...) \
  SELF::Bindings::Init(ctx, __VA_ARGS__)

// ============================================================================
// [NJS_CLASS - Declarative Interface]
// ============================================================================

#define NJS_BASE_CLASS(SELF, CLASS_NAME, TAG)                                 \
public:                                                                       \
  typedef SELF Type;                                                          \
  typedef SELF Base;                                                          \
                                                                              \
  enum : uint32_t { kObjectTag = TAG };                                       \
                                                                              \
  struct Bindings;                                                            \
                                                                              \
  static NJS_INLINE const char* staticClassName() noexcept {                  \
    return CLASS_NAME;                                                        \
  }                                                                           \
                                                                              \
  NJS_INLINE size_t refCount() const noexcept { return _wrapData._refCount; } \
  NJS_INLINE void addRef() noexcept { _wrapData.addRef(); }                   \
  NJS_INLINE void release() noexcept { _wrapData.release(this); }             \
  NJS_INLINE void makeWeak() noexcept { _wrapData.makeWeak(this); }           \
                                                                              \
  NJS_INLINE ::njs::Value asJSObject(::njs::Context& ctx) const noexcept {    \
    return ctx.makeLocal(_wrapData._object);                                  \
  }                                                                           \
                                                                              \
  ::njs::WrapData _wrapData;

#define NJS_INHERIT_CLASS(SELF, BASE, CLASS_NAME)                             \
public:                                                                       \
  typedef SELF Type;                                                          \
  typedef BASE Base;                                                          \
                                                                              \
  struct Bindings;                                                            \
  friend struct Bindings;                                                     \
                                                                              \
  static NJS_INLINE const char* staticClassName() noexcept {                  \
    return CLASS_NAME;                                                        \
  }

// ============================================================================
// [NJS_BIND - Bindings Interface]
// ============================================================================

// NOTE: All of these functions use V8 signatures to ensure that `This()`
// points to a correct object. This means that it's safe to directly use
// `Internal::v8UnwrapNativeUnchecked<>`.

#define NJS_BIND_CLASS(SELF) \
  struct SELF::Bindings : public ::njs::Internal::V8ClassBindings< SELF >

#define NJS_BIND_CONSTRUCTOR()                                                \
  static NJS_NOINLINE void ConstructorEntry(                                  \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) noexcept {       \
                                                                              \
    ::njs::ConstructCallContext ctx(::njs::Internal::pass(info));             \
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
    ::njs::ConstructCallContext& ctx) noexcept

#define NJS_ABSTRACT_CONSTRUCTOR()                                            \
  static NJS_NOINLINE void ConstructorEntry(                                  \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) noexcept {       \
                                                                              \
    ::njs::ConstructCallContext ctx(::njs::Internal::pass(info));             \
    ::njs::Result result;                                                     \
                                                                              \
    if (!info.IsConstructCall())                                              \
      result = ::njs::Globals::kResultInvalidConstructCall;                   \
    else                                                                      \
      result = ::njs::Globals::kResultAbstractConstructCall;                  \
                                                                              \
    ctx._payload.constructCall.className =                                    \
      const_cast<char*>(Type::staticClassName());                             \
    ctx._handleResult(result);                                                \
  }

#define NJS_BIND_STATIC(NAME)                                                 \
  static NJS_NOINLINE void StaticFunc_##NAME(                                 \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) noexcept {       \
                                                                              \
    ::njs::FunctionCallContext ctx(::njs::Internal::pass(info));              \
    ctx._handleResult(StaticImpl_##NAME(ctx));                                \
  }                                                                           \
                                                                              \
  struct StaticInfo_##NAME : public ::njs::BindingItem {                      \
    NJS_INLINE StaticInfo_##NAME() noexcept                                   \
      : BindingItem(kTypeStatic, 0, #NAME, (const void*)StaticFunc_##NAME) {} \
  } staticinfo_##NAME;                                                        \
                                                                              \
  static NJS_INLINE ::njs::Result StaticImpl_##NAME(                          \
    ::njs::FunctionCallContext& ctx) noexcept

#define NJS_BIND_METHOD(NAME)                                                 \
  static NJS_NOINLINE void MethodFunc_##NAME(                                 \
      const ::v8::FunctionCallbackInfo< ::v8::Value >& info) noexcept {       \
                                                                              \
    ::njs::FunctionCallContext ctx(::njs::Internal::pass(info));              \
    Type* self = ::njs::Internal::v8UnwrapNativeUnsafe<Type>(                 \
      ctx, ctx._info.This());                                                 \
    ctx._handleResult(MethodImpl_##NAME(ctx, self));                          \
  }                                                                           \
                                                                              \
  struct MethodInfo_##NAME : public ::njs::BindingItem {                      \
    NJS_INLINE MethodInfo_##NAME() noexcept                                   \
      : BindingItem(kTypeMethod, 0, #NAME, (const void*)MethodFunc_##NAME) {} \
  } methodinfo_##NAME;                                                        \
                                                                              \
  static NJS_INLINE ::njs::Result MethodImpl_##NAME(                          \
    ::njs::FunctionCallContext& ctx, Type* self) noexcept

#define NJS_BIND_GET(NAME)                                                    \
  static NJS_NOINLINE void GetFunc_##NAME(                                    \
      ::v8::Local< ::v8::String > property,                                   \
      const ::v8::PropertyCallbackInfo< ::v8::Value >& info) noexcept {       \
                                                                              \
    ::njs::GetPropertyContext ctx(::njs::Internal::pass(info));               \
    Type* self = ::njs::Internal::v8UnwrapNativeUnsafe<Type>(                 \
      ctx, ctx._info.This());                                                 \
    ctx._handleResult(GetImpl_##NAME(ctx, self));                             \
  }                                                                           \
                                                                              \
  struct GetInfo_##NAME : public ::njs::BindingItem {                         \
    NJS_INLINE GetInfo_##NAME() noexcept                                      \
      : BindingItem(kTypeGetter, 0, #NAME, (const void*)GetFunc_##NAME) {}    \
  } GetInfo_##NAME;                                                           \
                                                                              \
  static NJS_INLINE ::njs::Result GetImpl_##NAME(                             \
    ::njs::GetPropertyContext& ctx, Type* self) noexcept

#define NJS_BIND_SET(NAME)                                                    \
  static NJS_NOINLINE void SetFunc_##NAME(                                    \
      ::v8::Local< ::v8::String > property,                                   \
      ::v8::Local< ::v8::Value > value,                                       \
      const v8::PropertyCallbackInfo<void>& info) noexcept {                  \
                                                                              \
    ::njs::SetPropertyContext ctx(::njs::Internal::pass(info),                \
                                  ::njs::Internal::pass(value));              \
                                                                              \
    Type* self = ::njs::Internal::v8UnwrapNativeUnsafe<Type>(                 \
      ctx, ctx._info.This());                                                 \
    ctx._handleResult(SetImpl_##NAME(ctx, self));                             \
  }                                                                           \
                                                                              \
  struct SetInfo_##NAME : public ::njs::BindingItem {                         \
    NJS_INLINE SetInfo_##NAME() noexcept                                      \
      : BindingItem(kTypeSetter, 0, #NAME, (const void*)SetFunc_##NAME) {}    \
  } setinfo_##NAME;                                                           \
                                                                              \
  static NJS_INLINE ::njs::Result SetImpl_##NAME(                             \
    ::njs::SetPropertyContext& ctx, Type* self) noexcept

} // {njs}

#endif // NJS_ENGINE_V8_H
