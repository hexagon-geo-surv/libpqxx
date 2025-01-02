/* Various utility definitions for libpqxx.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/util instead.
 *
 * Copyright (c) 2000-2025, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_UTIL
#define PQXX_H_UTIL

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <format>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "pqxx/except.hxx"
#include "pqxx/types.hxx"
#include "pqxx/version.hxx"


/// The home of all libpqxx classes, functions, templates, etc.
namespace pqxx
{}

#include <pqxx/internal/libpq-forward.hxx>


// C++23: Retire wrapper.
// PQXX_UNREACHABLE: equivalent to `std::unreachable()` if available.
#if !defined(__cpp_lib_unreachable)
#  define PQXX_UNREACHABLE while (false)
#elif !__cpp_lib_unreachable
#  define PQXX_UNREACHABLE while (false)
#else
#  define PQXX_UNREACHABLE std::unreachable()
#endif


/// Internal items for libpqxx' own use.  Do not use these yourself.
namespace pqxx::internal
{} // namespace pqxx::internal


namespace pqxx
{
using namespace std::literals;

/// Suppress compiler warning about an unused item.
template<typename... T> inline constexpr void ignore_unused(T &&...) noexcept
{}


/// Cast a numeric value to another type, or throw if it underflows/overflows.
/** Both types must be arithmetic types, and they must either be both integral
 * or both floating-point types.
 */
template<typename TO, typename FROM>
inline TO check_cast(FROM value, std::string_view description, sl loc)
{
  static_assert(std::is_arithmetic_v<FROM>);
  static_assert(std::is_arithmetic_v<TO>);
  static_assert(std::is_integral_v<FROM> == std::is_integral_v<TO>);

  // The rest of this code won't quite work for bool, but bool is trivially
  // convertible to other arithmetic types as far as I can see.
  if constexpr (std::is_same_v<FROM, bool>)
    return static_cast<TO>(value);

  // Depending on our "if constexpr" conditions, this parameter may not be
  // needed.  Some compilers will warn.
  ignore_unused(description);

  using from_limits = std::numeric_limits<decltype(value)>;
  using to_limits = std::numeric_limits<TO>;
  if constexpr (std::is_signed_v<FROM>)
  {
    if constexpr (std::is_signed_v<TO>)
    {
      if (value < to_limits::lowest())
        throw range_error{
          std::format("Cast underflow: {}"sv, description), loc};
    }
    else
    {
      // FROM is signed, but TO is not.  Treat this as a special case, because
      // there may not be a good broader type in which the compiler can even
      // perform our check.
      if (value < 0)
        throw range_error{
          std::format(
            "Casting negative value to unsigned type: {}"sv, description),
          loc};
    }
  }
  else
  {
    // No need to check: the value is unsigned so can't fall below the range
    // of the TO type.
  }

  if constexpr (std::is_integral_v<FROM>)
  {
    using unsigned_from = std::make_unsigned_t<FROM>;
    using unsigned_to = std::make_unsigned_t<TO>;
    constexpr auto from_max{static_cast<unsigned_from>((from_limits::max)())};
    constexpr auto to_max{static_cast<unsigned_to>((to_limits::max)())};
    if constexpr (from_max > to_max)
    {
      if (std::cmp_greater(value, to_max))
        throw range_error{
          std::format("Cast overflow: {}"sv, description), loc};
    }
  }
  else if constexpr ((from_limits::max)() > (to_limits::max)())
  {
    if (value > (to_limits::max)())
      throw range_error{std::format("Cast overflow: {}", description), loc};
  }

  return static_cast<TO>(value);
}


/** Check library version at link time.
 *
 * Ensures a failure when linking an application against a radically
 * different libpqxx version than the one against which it was compiled.
 *
 * Sometimes application builds fail in unclear ways because they compile
 * using headers from libpqxx version X, but then link against libpqxx
 * binary version Y.  A typical scenario would be one where you're building
 * against a libpqxx which you have built yourself, but a different version
 * is installed on the system.
 *
 * The check_library_version template is declared for any library version,
 * but only actually defined for the version of the libpqxx binary against
 * which the code is linked.
 *
 * If the library binary is a different version than the one declared in
 * these headers, then this call will fail to link: there will be no
 * definition for the function with these exact template parameter values.
 * There will be a definition, but the version in the parameter values will
 * be different.
 */
inline PQXX_PRIVATE void check_version() noexcept
{
  // There is no particular reason to do this here in @ref connection, except
  // to ensure that every meaningful libpqxx client will execute it.  The call
  // must be in the execution path somewhere or the compiler won't try to link
  // it.  We can't use it to initialise a global or class-static variable,
  // because a smart compiler might resolve it at compile time.
  //
  // On the other hand, we don't want to make a useless function call too
  // often for performance reasons.  A local static variable is initialised
  // only on the definition's first execution.  Compilers will be well
  // optimised for this behaviour, so there's a minimal one-time cost.
  static auto const version_ok{internal::PQXX_VERSION_CHECK()};
  ignore_unused(version_ok);
}


/// Descriptor of library's thread-safety model.
/** This describes what the library knows about various risks to thread-safety.
 */
struct PQXX_LIBEXPORT thread_safety_model
{
  /// Is the underlying libpq build thread-safe?
  bool safe_libpq = false;

  /// Is Kerberos thread-safe?
  /** @warning Is currently always `false`.
   *
   * If your application uses Kerberos, all accesses to libpqxx or Kerberos
   * must be serialized.  Confine their use to a single thread, or protect it
   * with a global lock.
   */
  bool safe_kerberos = false;

  /// A human-readable description of any thread-safety issues.
  std::string description;
};


/// Describe thread safety available in this build.
[[nodiscard]] PQXX_LIBEXPORT thread_safety_model describe_thread_safety();


/// Custom `std::char_trast` if the compiler does not provide one.
/** Needed if the standard library lacks a generic implementation or a
 * specialisation for std::byte.  They aren't strictly required to provide
 * either, and libc++ 19 removed its generic implementation.
 */
struct byte_char_traits : std::char_traits<char>
{
  using char_type = std::byte;

  static void assign(std::byte &a, const std::byte &b) noexcept { a = b; }
  static bool eq(std::byte a, std::byte b) { return a == b; }
  static bool lt(std::byte a, std::byte b) { return a < b; }

  static int compare(const std::byte *a, const std::byte *b, std::size_t size)
  {
    return std::memcmp(a, b, size);
  }

  /// Deliberately undefined: "guess" the length of an array of bytes.
  /* This would be nonsense: we can't determine the length of a random sequence
   * of bytes.  There is no terminating zero like there is for C strings.
   *
   * But `std::char_traits` requires us to provide this function, so we
   * declare it without defining it.
   */
  static size_t length(const std::byte *data);

  static const std::byte *
  find(const std::byte *data, std::size_t size, const std::byte &value)
  {
    return static_cast<const std::byte *>(
      std::memchr(data, static_cast<int>(value), size));
  }

  static std::byte *
  move(std::byte *dest, const std::byte *src, std::size_t size)
  {
    return static_cast<std::byte *>(std::memmove(dest, src, size));
  }

  static std::byte *
  copy(std::byte *dest, const std::byte *src, std::size_t size)
  {
    return static_cast<std::byte *>(std::memcpy(dest, src, size));
  }

  static std::byte *assign(std::byte *dest, std::size_t size, std::byte value)
  {
    return static_cast<std::byte *>(
      std::memset(dest, static_cast<int>(value), size));
  }

  /// Declared but not defined: makes no sense for binary data.
  static int_type not_eof(int_type value);

  static std::byte to_char_type(int_type value) { return std::byte(value); }

  static int_type to_int_type(std::byte value) { return int_type(value); }

  static bool eq_int_type(int_type a, int_type b) { return a == b; }

  /// Declared but not defined: makes no sense for binary data.
  static int_type eof();
};

template<typename TYPE, typename = void>
struct has_generic_char_traits : std::false_type
{};

template<typename TYPE>
struct has_generic_char_traits<
  TYPE, std::void_t<decltype(std::char_traits<TYPE>::eof)>> : std::true_type
{};

inline constexpr bool has_generic_bytes_char_traits =
  has_generic_char_traits<std::byte>::value;

// Supress warnings from potentially using a deprecated generic
// std::char_traits.
// Necessary for libc++ 18.
#include "pqxx/internal/ignore-deprecated-pre.hxx"

// XXX: Replace this type!
/// Type alias for a container containing bytes.
/* Required to support standard libraries without a generic implementation for
 * `std::char_traits<std::byte>`.
 * @warn Will change to `std::vector<std::byte>` in the next major release.
 */
using bytes = std::conditional<
  has_generic_bytes_char_traits, std::basic_string<std::byte>,
  std::basic_string<std::byte, byte_char_traits>>::type;

#include "pqxx/internal/ignore-deprecated-post.hxx"


/// Cast binary data to a type that libpqxx will recognise as binary.
/** There are many different formats for storing binary data in memory.  You
 * may have yours as a `std::string`, or a `std::vector<uchar_t>`, or one of
 * many other types.  In libpqxx we commend a container of `std::byte`.
 *
 * For libpqxx to recognise your data as binary, we recommend using a
 * `pqxx::bytes`, or a `pqxx::bytes_view`; but any contiguous block of
 * `std::byte` should do.
 *
 * Use `binary_cast` as a convenience helper to cast your data as a
 * `pqxx::bytes_view`.
 *
 * @warning You must keep the storage holding the actual data alive for as
 * long as you might use this function's return value.
 */
template<potential_binary TYPE> inline bytes_view binary_cast(TYPE const &data)
{
  using item_t = value_type<TYPE>;
  return std::as_bytes(
    std::span<item_t const>{std::data(data), std::size(data)});
}


/// Construct a type that libpqxx will recognise as binary.
/** Takes a data pointer and a size, without being too strict about their
 * types, and constructs a `pqxx::bytes_view` pointing to the same data.
 *
 * This makes it a little easier to turn binary data, in whatever form you
 * happen to have it, into binary data as libpqxx understands it.
 */
template<char_sized CHAR, typename SIZE>
bytes_view binary_cast(CHAR const *data, SIZE size)
{
  return binary_cast(std::span<CHAR>{data, check_cast<std::size_t>(size)});
}


/// The "null" oid.
constexpr oid oid_none{0};
} // namespace pqxx


/// Private namespace for libpqxx's internal use; do not access.
/** This namespace hides definitions internal to libpqxx.  These are not
 * supposed to be used by client programs, and they may change at any time
 * without notice.
 *
 * Conversely, if you find something in this namespace tremendously useful, by
 * all means do lodge a request for its publication.
 *
 * @warning Here be dragons!
 */
namespace pqxx::internal
{
using namespace std::literals;


/// A safer and more generic replacement for `std::isdigit`.
/** Turns out `std::isdigit` isn't as easy to use as it sounds.  It takes an
 * `int`, but requires it to be nonnegative.  Which means it's an outright
 * liability on systems where `char` is signed.
 */
template<typename CHAR> inline constexpr bool is_digit(CHAR c) noexcept
{
  return (c >= '0') and (c <= '9');
}


/// Describe an object for humans, based on class name and optional name.
/** Interprets an empty name as "no name given."
 */
[[nodiscard]] std::string
describe_object(std::string_view class_name, std::string_view name);


/// Check validity of registering a new "guest" in a "host."
/** The host might be e.g. a connection, and the guest a transaction.  The
 * host can only have one guest at a time, so it is an error to register a new
 * guest while the host already has a guest.
 *
 * If the new registration is an error, this function throws a descriptive
 * exception.
 *
 * Pass the old guest (if any) and the new guest (if any), for both, a type
 * name (at least if the guest is not null), and optionally an object name
 * (but which may be omitted if the caller did not assign one).
 */
void check_unique_register(
  void const *old_guest, std::string_view old_class, std::string_view old_name,
  void const *new_guest, std::string_view new_class,
  std::string_view new_name);


/// Like @ref check_unique_register, but for un-registering a guest.
/** Pass the guest which was registered, as well as the guest which is being
 * unregistered, so that the function can check that they are the same one.
 */
void check_unique_unregister(
  void const *old_guest, std::string_view old_class, std::string_view old_name,
  void const *new_guest, std::string_view new_class,
  std::string_view new_name);


/// Compute buffer size needed to escape binary data for use as a BYTEA.
/** This uses the hex-escaping format.  The return value includes room for the
 * "\x" prefix.
 */
inline constexpr std::size_t size_esc_bin(std::size_t binary_bytes) noexcept
{
  return 2 + (2 * binary_bytes) + 1;
}


/// Compute binary size from the size of its escaped version.
/** Do not include a terminating zero in `escaped_bytes`.
 */
inline constexpr std::size_t size_unesc_bin(std::size_t escaped_bytes) noexcept
{
  return (escaped_bytes - 2) / 2;
}


/// Hex-escape binary data into a buffer.
/** The buffer must have room for `size_esc_bin(std::size(binary_data))` bytes,
 * and the function will write exactly that number of bytes into the buffer.
 * This includes a trailing zero.
 */
void PQXX_LIBEXPORT
esc_bin(bytes_view binary_data, std::span<char> buffer) noexcept;


/// Hex-escape binary data into a buffer.
/** The buffer must be able to accommodate
 * `size_esc_bin(std::size(binary_data))` bytes, and the function will write
 * exactly that number of bytes into the buffer.  This includes a trailing
 * zero.
 */
template<binary T>
inline void esc_bin(T &&binary_data, std::span<char> buffer) noexcept
{
  esc_bin(binary_cast(binary_data), buffer);
}


/// Hex-escape binary data into a std::string.
std::string PQXX_LIBEXPORT esc_bin(bytes_view binary_data);


/// Reconstitute binary data from its escaped version.
void PQXX_LIBEXPORT
unesc_bin(std::string_view escaped_data, std::span<std::byte> buffer, sl loc);


/// Reconstitute binary data from its escaped version.
bytes PQXX_LIBEXPORT unesc_bin(std::string_view escaped_data, sl loc);


/// Helper for determining a function's parameter types.
/** This function has no definition.  It's not meant to be actually called.
 * It's just there for pattern-matching in the compiler, so we can use its
 * hypothetical return value.
 */
template<typename RETURN, typename... ARGS>
std::tuple<ARGS...> args_f(RETURN (&func)(ARGS...));


/// Helper for determining a `std::function`'s parameter types.
/** This function has no definition.  It's not meant to be actually called.
 * It's just there for pattern-matching in the compiler, so we can use its
 * hypothetical return value.
 */
template<typename RETURN, typename... ARGS>
std::tuple<ARGS...> args_f(std::function<RETURN(ARGS...)> const &);


/// Helper for determining a member function's parameter types.
/** This function has no definition.  It's not meant to be actually called.
 * It's just there for pattern-matching in the compiler, so we can use its
 * hypothetical return value.
 */
template<typename CLASS, typename RETURN, typename... ARGS>
std::tuple<ARGS...> member_args_f(RETURN (CLASS::*)(ARGS...));


/// Helper for determining a const member function's parameter types.
/** This function has no definition.  It's not meant to be actually called.
 * It's just there for pattern-matching in the compiler, so we can use its
 * hypothetical return value.
 */
template<typename CLASS, typename RETURN, typename... ARGS>
std::tuple<ARGS...> member_args_f(RETURN (CLASS::*)(ARGS...) const);


/// Helper for determining a callable type's parameter types.
/** This specialisation should work for lambdas.
 *
 * This function has no definition.  It's not meant to be actually called.
 * It's just there for pattern-matching in the compiler, so we can use its
 * hypothetical return value.
 */
template<typename CALLABLE>
auto args_f(CALLABLE const &f)
  -> decltype(member_args_f(&CALLABLE::operator()));


/// A callable's parameter types, as a tuple.
template<typename CALLABLE>
using args_t = decltype(args_f(std::declval<CALLABLE>()));


/// Apply `std::remove_cvref_t` to each of a tuple type's component types.
/** This function has no definition.  It is not meant to be called, only to be
 * used to deduce the right types.
 */
template<typename... TYPES>
std::tuple<std::remove_cvref_t<TYPES>...>
strip_types(std::tuple<TYPES...> const &);


/// Take a tuple type and apply std::remove_cvref_t to its component types.
template<typename... TYPES>
using strip_types_t = decltype(strip_types(std::declval<TYPES...>()));


/// Return original byte for escaped character.
inline constexpr char unescape_char(char escaped) noexcept
{
  switch (escaped)
  {
  case 'b': // Backspace.
    [[unlikely]] return '\b';
  case 'f': // Form feed
    [[unlikely]] return '\f';
  case 'n': // Line feed.
    return '\n';
  case 'r': // Carriage return.
    return '\r';
  case 't': // Horizontal tab.
    return '\t';
  case 'v': // Vertical tab.
    return '\v';
  default: break;
  }
  // Regular character ("self-escaped").
  return escaped;
}


/// Helper for avoiding type trouble with `strerror_r()`/`strerror_s()`.
/** Extracts the error string from a `strerror_s()` or a POSIX-style
 * `streror_r()` outcome.
 *
 * The problem is with `strerror_r()`, really.  There's a GNU version which
 * returns the error string as a `char *`; and there's a POSIX version which
 * writes the error string into `buffer` and returns a status code.
 *
 * Not all compilers will let us handle that with a "if constexpr" on the
 * return type.  In particular, clang 17 on a Mac complains.  it insists on
 * even the non-applicable branch returning the right type.  So, instead of
 * having an `if constexpr` with an `else`, we _overload_ functions for the two
 * alternatives.
 */
[[maybe_unused]] inline char const *PQXX_COLD
make_strerror_rs_result(int err_result, std::span<char> buffer)
{
  if (err_result == 0)
    return std::data(buffer);
  else
    return "Unknown error; could not retrieve error string.";
}


/// Helper for avoiding type trouble with `strerror_r()`/`strerror_s()`.
/** Extracts the error string from a GNU-style `strerror_r()` outcome.
 *
 * There's another overload for th `strerror_s()` and POSIX-style
 * `strerror_r()` case.
 */
[[maybe_unused]] inline char const *PQXX_COLD
make_strerror_rs_result(char const *err_result, std::span<char>)
{
  return err_result;
}


/// Get error string for a given @c errno value.
[[nodiscard]] inline char const *PQXX_COLD
error_string(int err_num, std::span<char> buffer)
{
  // Not entirely clear whether strerror_s will be in std or global namespace.
  using namespace std;

#if defined(PQXX_HAVE_STERROR_S) || defined(PQXX_HAVE_STRERROR_R)
#  if defined(PQXX_HAVE_STRERROR_S)
  auto const err_result{
    strerror_s(std::data(buffer), std::size(buffer), err_num)};
#  else
  auto const err_result{
    strerror_r(err_num, std::data(buffer), std::size(buffer))};
#  endif
  return make_strerror_rs_result(err_result, buffer);
#else
  // Fallback case, hopefully for no actual platforms out there.
  pqxx::ignore_unused(err_num, buffer);
  return "(No error information available.)";
#endif
}


/// Represent a std::source_location as human-readable text.
inline std::string source_loc(sl loc)
{
  // TODO: Rewrite to guarantee Return Value Optimisation.
  char const *const func{loc.function_name()};
  // (The standard says this can't be null, but let's be conservative.)
  bool const have_func{func != nullptr and *func != '\0'},
    have_line{loc.line() > 0};

  if (have_func and have_line)
  {
    return std::format("{} in {}:{}", func, loc.file_name(), loc.line());
  }
  else if (have_func)
  {
    return std::format("{} in {}", func, loc.file_name());
  }
  else if (have_line)
  {
    return std::format("{}:{}", loc.file_name(), loc.line());
  }
  else
  {
    return std::string{loc.file_name()};
  }
}


/// Copy text from `src` into `buf` at offset `dst_offset`.
/** This is a wrapper for `std::string_view::copy()` with a few changes.
 *
 * First, it checks for overruns and throws @ref pqxx::conversion_overrun if
 * needed.  (To that end, the destination is a `std::span`, not a raw pointer.)
 *
 * Second, it takes an offset _into the destination buffer,_ i.e. you can tell
 * it where in the destination buffer the copy should write, but there's no
 * parameter to influence which part of `src` you want to copy.  You always
 * copy the whole thing.
 *
 * Third, it returns not the number of bytes it copied, but rather, the offset
 * into `dst` that's right behind the last copied byte.
 *
 * If `terminate` is true, also writes a terminating zero.
 */
template<bool terminate>
inline std::size_t copy_chars(
  std::string_view src, std::span<char> dst, std::size_t dst_offset, sl loc)
{
  auto const sz{std::size(src)};
  if (std::cmp_greater(
        dst_offset + sz + std::size_t(terminate), std::size(dst)))
    throw conversion_overrun{
      std::format(
        "Text copy exceeded buffer space: tried to copy {} bytes '{}' into a "
        "buffer of {} bytes, at offset {}.",
        sz, src, std::size(dst), dst_offset),
      loc};
  auto at{dst_offset + src.copy(std::data(dst) + dst_offset, sz)};
  if constexpr (terminate)
    dst[at++] = '\0';
  return at;
}
} // namespace pqxx::internal


namespace pqxx::internal::pq
{
/// Wrapper for `PQfreemem()`, with C++ linkage.
PQXX_LIBEXPORT void pqfreemem(void const *) noexcept;
} // namespace pqxx::internal::pq
#endif
