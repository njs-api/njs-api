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
  NJS_INLINE Value NewStringFromEnum(int value, const Enum_& enum_) noexcept {
    return Internal::V8EnumToString(
      *this, value, reinterpret_cast<const Enum&>(enum_));
  }
*/

// ============================================================================
// [njs::Internal::EnumUtils]
// ============================================================================

namespace Internal {
  namespace EnumUtils {
    static const constexpr char kAltEnumMarker = '@';
    static const constexpr unsigned int kEnumNotFound = ~static_cast<unsigned int>(0);

    static NJS_INLINE bool isIgnorableChar(unsigned int c) noexcept {
      return c == '-';
    }

    // Tries to find a string passed as `in` in `enumData` (see `Enum`). It returns
    // the index of the value found. If the enumeration doesn't start at 0 then the
    // index has to be adjusted accordingly. It returns `kEnumNotFound` on failure.
    //
    // Keep it inlined, it should expand only once if used properly.
    template<typename CharType>
    static NJS_INLINE unsigned int parse(const CharType* in, size_t size, const char* enumData) noexcept {
      if (!size) return kEnumNotFound;

      const char* pa = enumData;
      const CharType* pbEnd = in + size;

      unsigned int ca, cb;
      unsigned int cbFirst = in[0];

      for (unsigned int index = 0; ; index++) {
        // NULL record indicates end of data.
        ca = static_cast<uint8_t>(*pa++);
        if (!ca) return kEnumNotFound;

        // Enum can contain alternative strings that describe the same value,
        // so make sure we won't increment if we just parsed the notation.
        if (ca == kAltEnumMarker) {
          ca = static_cast<uint8_t>(*pa++);
          index--;
        }

        if (ca == cbFirst) {
          const CharType* pb = in;
          while (++pb != pbEnd) {
            cb = *pb;
L_Repeat:
            ca = static_cast<uint8_t>(*pa++);
            if (!ca) goto L_End;
            if (ca == cb) continue;

            if (isIgnorableChar(ca))
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
    static NJS_INLINE unsigned int stringify(CharType* data, unsigned int index, const char* enumData) noexcept {
      unsigned int i = 0;
      const char* p = enumData;

      while (i != index) {
        // NULL record indicates end of data.
        if (!*p)
          return kEnumNotFound;

        // Only increment if this is not an alternative string.
        i += *p != kAltEnumMarker;

        // Skip the current string.
        while (*++p)
          continue;

        // Skip the NULL terminator of this record.
        p++;
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
} // Internal namespace

// ============================================================================
// [njs::Enum & NJS_ENUM]
// ============================================================================

struct Enum {
  enum { kConceptType = Globals::kConceptSerializer };

  // Returns enumeration representation defined by `EnumT<>`.
  NJS_INLINE const char* data() const noexcept {
    return reinterpret_cast<const char*>(this + 1);
  }

  template<typename T>
  NJS_NOINLINE Result serialize(Context& ctx, T in, Value& out) const noexcept {
    unsigned int index = static_cast<unsigned int>(static_cast<int>(in)) - static_cast<unsigned int>(_start);
    uint16_t content[Globals::kMaxEnumSize];

    unsigned int size = Internal::EnumUtils::stringify<uint16_t>(content, index, data());
    if (size == Internal::EnumUtils::kEnumNotFound || size == 0) {
      out = ctx.newEmptyString();
      return Globals::kResultInvalidValue;
    }
    else {
      out = ctx.newString(Utf16Ref(content, size));
      return resultOf(out);
    }
  }

  template<typename T>
  NJS_NOINLINE Result deserialize(Context& ctx, const Value& in, T& out) const noexcept {
    if (!in.isString())
      return Globals::kResultInvalidValue;

    uint16_t content[Globals::kMaxEnumSize];
    int size = ctx.stringLength(in);

    if (size <= 0 || size > int(Globals::kMaxEnumSize) || ctx.readUtf16(in, content, size) < size)
      return Globals::kResultInvalidValue;

    unsigned int index = Internal::EnumUtils::parse<uint16_t>(content, size, data());
    if (index == Internal::EnumUtils::kEnumNotFound)
      return Globals::kResultInvalidValue;

    out = static_cast<T>(index + static_cast<unsigned int>(_start));
    return Globals::kResultOk;
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
  enum { kConceptType = Globals::kConceptSerializer };

  // Treat `EnumT` as `Enum`, if possible.
  NJS_INLINE operator Enum&() noexcept { return _base; }
  NJS_INLINE operator const Enum&() const noexcept { return _base; }
  NJS_INLINE const Enum* operator&() const noexcept { return &_base; }

  // Compatibility with `Enum`.
  NJS_INLINE const char* data() const noexcept { return _base.data(); }

  template<typename T>
  NJS_NOINLINE Result serialize(Context& ctx, T in, Value& out) const noexcept {
    return _base.serialize(ctx, in, out);
  }

  template<typename T>
  NJS_INLINE Result deserialize(Context& ctx, const Value& in, T& out) const noexcept {
    return _base.deserialize(ctx, in, out);
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
