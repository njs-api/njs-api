// [NJS]
// Neutral JS interface.
//
// [License]
// Public Domain <http://unlicense.org>

// This header implements an enumeration extension. Enumeration is a map of
// sequential values into string representation and back. The `njs::Enum` type
// provided can be used to define enumeration and to act as parser or serializer.

// [Guard]
#ifndef _NJS_EXTENSION_ENUM_H
#define _NJS_EXTENSION_ENUM_H

// [Dependencies]
#include "./njs.h"

namespace njs {

/*TODO:
  template<typename Enum_>
  NJS_INLINE Value NewStringFromEnum(int value, const Enum_& enum_) NJS_NOEXCEPT {
    return internal::V8EnumToString(
      *this, value, reinterpret_cast<const Enum&>(enum_));
  }
*/

// ============================================================================
// [njs::internal::EnumUtils]
// ============================================================================

namespace internal {
  namespace EnumUtils {
    static const unsigned int kEnumNotFound = ~static_cast<unsigned int>(0);

    static NJS_INLINE bool IsIgnorableChar(unsigned int c) NJS_NOEXCEPT {
      return c == '-';
    }

    // Tries to find a string passed as `in` in `enumData` (see `Enum`). It returns
    // the index of the value found. If the enumeration doesn't start at 0 then the
    // index has to be adjusted accordingly. It returns `kEnumNotFound` on failure.
    //
    // Keep it inlined, it should expand only once if used properly.
    template<typename CharType>
    static NJS_INLINE unsigned int Parse(const CharType* in, size_t length, const char* enumData) NJS_NOEXCEPT {
      if (!length) return kEnumNotFound;

      const char* pa = enumData;
      const CharType* pbEnd = in + length;

      unsigned int ca, cb;
      unsigned int cbFirst = in[0];

      for (unsigned int index = 0; ; index++) {
        // NULL record indicates the end of the data.
        ca = static_cast<uint8_t>(*pa++);
        if (!ca) return kEnumNotFound;

        if (ca == cbFirst) {
          const CharType* pb = in;
          while (++pb != pbEnd) {
            cb = *pb;
L_Repeat:
            ca = static_cast<uint8_t>(*pa++);
            if (!ca) goto L_End;
            if (ca == cb) continue;

            if (IsIgnorableChar(ca))
              goto L_Repeat;
            else
              goto L_NoMatch;
          }

          ca = static_cast<uint8_t>(*pa++);
          if (!ca) return index;
        }

L_NoMatch:
        while (*pa++)
          continue;
L_End:
        ;
      }
    }

    // Keep it inlined, it should expand only once if used properly.
    template<typename CharType>
    static NJS_INLINE unsigned int Stringify(CharType* data, unsigned int index, const char* enumData) NJS_NOEXCEPT {
      unsigned int i = 0;
      const char* p = enumData;

      while (i != index) {
        // NULL record indicates the end of the data.
        if (!*p) return kEnumNotFound;

        while (*++p)
          continue;
        p++; // Skip the NULL terminator of this record.
        i++; // Increment the current index.
      }

      CharType* pData = data;
      for (;;) {
        unsigned int c = static_cast<uint8_t>(*p++);
        if (!c)
          break;
        *pData++ = static_cast<CharType>(c);
      }

      return static_cast<size_t>((intptr_t)(pData - data));
    }
  } // EnumUtils namespace
} // internal namespace

// ============================================================================
// [njs::Enum & NJS_ENUM]
// ============================================================================

struct Enum {
  enum { kConceptType = kConceptSerializer };

  // Returns enumeration representation defined by `EnumT<>`.
  NJS_INLINE const char* GetData() const NJS_NOEXCEPT {
    return reinterpret_cast<const char*>(this + 1);
  }

  template<typename T>
  NJS_NOINLINE Result Serialize(Context& ctx, T in, Value& out) const NJS_NOEXCEPT {
    unsigned int index = static_cast<unsigned int>(static_cast<int>(in)) - static_cast<unsigned int>(_start);
    uint16_t content[kMaxEnumLength];

    unsigned int length = internal::EnumUtils::Stringify<uint16_t>(content, index, GetData());
    if (length == internal::EnumUtils::kEnumNotFound || length == 0) {
      out = ctx.EmptyString();
      return kResultInvalidValue;
    }
    else {
      out = ctx.NewString(Utf16Ref(content, length));
      return ResultOf(out);
    }
  }

  template<typename T>
  NJS_NOINLINE Result Deserialize(Context& ctx, const Value& in, T& out) const NJS_NOEXCEPT {
    if (!in.IsString())
      return kResultInvalidValueType;

    uint16_t content[kMaxEnumLength];
    int length = in.StringLength();

    if (length <= 0 || length > kMaxEnumLength || in.ReadUtf16(content, length) < length)
      return kResultInvalidValue;

    unsigned int index = internal::EnumUtils::Parse<uint16_t>(content, length, GetData());
    if (index == internal::EnumUtils::kEnumNotFound)
      return kResultInvalidValue;

    out = static_cast<T>(index + static_cast<unsigned int>(_start));
    return kResultOk;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  int _start;
  int _end;
  int _size;
  int _flags;
};

// NOTE: This class cannot inherit from `Enum`, the intend is to have the
// instance of `EnumT<>` statically initialized and in read-only memory.
template<size_t Size>
struct EnumT {
  enum { kConceptType = kConceptSerializer };

  // Treat `EnumT` as `Enum`, if possible.
  NJS_INLINE operator Enum&() NJS_NOEXCEPT { return _base; }
  NJS_INLINE operator const Enum&() const NJS_NOEXCEPT { return _base; }
  NJS_INLINE const Enum* operator&() const NJS_NOEXCEPT { return &_base; }

  // Compatibility with `Enum`.
  NJS_INLINE const char* GetData() const NJS_NOEXCEPT { return _base.GetData(); }

  template<typename T>
  NJS_NOINLINE Result Serialize(Context& ctx, T in, Value& out) const NJS_NOEXCEPT {
    return _base.Serialize(ctx, in, out);
  }

  template<typename T>
  NJS_INLINE Result Deserialize(Context& ctx, const Value& in, T& out) const NJS_NOEXCEPT {
    return _base.Deserialize(ctx, in, out);
  }

  Enum _base;
  char _data[(Size + 3) & ~static_cast<size_t>(3)];
};

#define NJS_ENUM(NAME, FIRST_VALUE, LAST_VALUE, DATA) \
  static const ::njs::EnumT< sizeof(DATA) > NAME = { \
    { FIRST_VALUE, LAST_VALUE, static_cast<int>(sizeof(DATA) - 1), 0, }, \
    DATA \
  }

} // njs namespace

// [Guard]
#endif // _NJS_EXTENSION_ENUM_H
