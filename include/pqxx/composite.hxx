#ifndef PQXX_H_COMPOSITE
#define PQXX_H_COMPOSITE

#include "pqxx/internal/array-composite.hxx"
#include "pqxx/util.hxx"

namespace pqxx
{
/// Parse a string representation of a value of a composite type.
/** @warning This code is still experimental.  Use with care.
 *
 * You may use this as a helper while implementing your own @c string_traits
 * for a composite type.
 *
 * This function nterprets @c text as the string representation of a value of
 * some composite type, and sets each of @c fields to the respective values of
 * its fields.  The field types must be copy-assignable.
 *
 * The number of fields must match the number of fields in the composite type,
 * and there must not be any other text in the input.  The function is meant to
 * handle any value string that the backend can produce, but not necessarily
 * every valid alternative spelling.
 *
 * Fields in composite types can be null.  When this happens, the C++ type of
 * the corresponding field reference must be of a type that can handle nulls.
 * If you are working with a type that does not have an inherent null value,
 * such as e.g. @c int, consider using @c std::optional.
 */
template<typename... T>
inline void parse_composite(
  pqxx::internal::encoding_group enc, std::string_view text, T &... fields)
{
  static_assert(sizeof...(fields) > 0);

  auto const scan{pqxx::internal::get_glyph_scanner(enc)};
  auto const data{text.data()};
  auto const size{text.size()};
  if (size == 0)
    throw conversion_error{"Cannot parse composite value from empty string."};

  std::size_t here{0}, next{scan(data, size, here)};
  if (next != 1 or data[here] != '(')
    throw conversion_error{
      "Invalid composite value string: " + std::string{data}};

  here = next;

  constexpr auto num_fields{sizeof...(fields)};
  std::size_t index{0};
  (pqxx::internal::parse_composite_field(
     index, text, here, fields, scan, num_fields - 1),
   ...);
}


/// Parse a string representation of a value of a composite type.
/** @warning This version only works for UTF-8 and single-byte encodings.
 *
 * For proper encoding support, pass an @c encoding_group as the first
 * argument.
 */
template<typename... T>
inline void parse_composite(std::string_view text, T &... fields)
{
  parse_composite(pqxx::internal::encoding_group::MONOBYTE, text, fields...);
}
} // namespace pqxx


namespace pqxx::internal
{
char const empty_composite_str[]{"()"};
} // namespace pqxx::internal


namespace pqxx
{
/// Estimate the buffer size needed to represent a value of a composite type.
/** Returns a conservative estimate.
 */
template<typename... T>
inline std::size_t composite_size_buffer(T const &... fields) noexcept
{
  constexpr auto num{sizeof...(fields)};

  // Size for a multi-field composite includes room for...
  //  + opening parenthesis
  //  + opening quote
  //  + field budget
  //  + worst-case one backslash per byte in the field budget
  //  - field terminating zero
  //  - escaping for field terminating zero
  //  + closing quote
  //  + separating comma per field
  //  - comma after final field
  //  + closing parenthesis
  //  + terminating zero

  if constexpr (sizeof...(fields) == 0)
    return sizeof(pqxx::internal::empty_composite_str);
  else
    return 1 + 3 * num + ((2 * size_buffer(fields) - 2) + ...) + 1;
}


/// Render a series of values as a single composite SQL value.
/** @warning This code is still experimental.  Use with care.
 *
 * You may use this as a helper while implementing your own @c string_traits
 * for a composite type.
 */
template<typename... T>
inline char *composite_into_buf(char *begin, char *end, T const &... fields)
{
  // TODO: Define a trait for "type does need quoting or escaping."
  if (std::size_t(end - begin) < composite_size_buffer(fields...))
    throw conversion_error{
      "Buffer space may not be enough to represent composite value."};

  constexpr auto num_fields{sizeof...(fields)};
  if constexpr (num_fields == 0)
  {
    char const empty[]{"()"};
    std::memcpy(begin, empty, sizeof(empty));
    return begin + sizeof(empty);
  }

  char *pos{begin};
  *pos++ = '(';

  (pqxx::internal::write_composite_field<T>(pos, end, fields), ...);

  // If we've got multiple fields, "backspace" that last comma.
  if constexpr (num_fields > 1)
    --pos;
  *pos++ = ')';
  *pos++ = '\0';
  return pos;
}
} // namespace pqxx
#endif