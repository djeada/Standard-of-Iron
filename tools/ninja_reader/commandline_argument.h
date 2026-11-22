// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#ifndef COMMANDLINE_ARGUMENT_H
#define COMMANDLINE_ARGUMENT_H

#include <concepts>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace NinjaReader::CommandLine::Argument {

class ArgumentValue {
public:
  using Integer = std::int64_t;
  using String = std::string;
  using Boolean = bool;
  using Type = std::variant<Integer, String, Boolean>;

private:
  Type m_value;

public:
  ArgumentValue(Integer integer) noexcept;
  ArgumentValue(String string) noexcept;
  ArgumentValue(Boolean boolean) noexcept;

  // Returns true if this argument value is an integer.
  [[nodiscard]] bool is_integer() const noexcept;

  // Provides the underlying integral value. See the "std::get<T>" ("std::variant"
  // overload) C++ standard library for thrown exceptions.
  [[nodiscard]] Integer integer() const;

  // Returns true if this argument value is a string.
  [[nodiscard]] bool is_string() const noexcept;

  // Provides readonly reference to the underlying string. See the "std::get<T>"
  // ("std::variant" overload) C++ standard library for thrown exceptions.
  [[nodiscard]] const String& string() const;

  // Returns true if this argument value is a boolean.
  [[nodiscard]] bool is_boolean() const noexcept;

  // Provides the underlying boolean value. See the "std::get<T>" ("std::variant"
  // overload) C++ standard library for thrown exceptions.
  [[nodiscard]] bool boolean() const;

  // Constructs an ArgumentValue instance with the provided integer. Causes a
  // compilation error if `IntegralT` is not integral. Use it to convert integral
  // types that are incompatible with `ArgumentValue::Integer`.
  template <std::integral IntegralT>
  static auto from(IntegralT integer) noexcept -> ArgumentValue {
    return ArgumentValue(static_cast<Integer>(integer));
  }

  // Constructs an ArgumentValue instance with the provided string.
  static auto from(const std::string& string) noexcept { return ArgumentValue(string); }

  // Constructs an ArgumentValue instance with the provided readonly string.
  static auto from(const char* string) noexcept {
    return ArgumentValue(std::string(string));
  }

  // Constructs an ArgumentValue instance with the provided boolean.
  static auto from(bool boolean) noexcept { return ArgumentValue(boolean); }
};

class Argument {
private:
  std::string m_name;
  ArgumentValue m_value;

public:
  explicit Argument(std::string name, ArgumentValue value) noexcept;

  // Provides reference to the underlying argument's value.
  [[nodiscard]] ArgumentValue& value() noexcept;

  // Provides the argument's name.
  [[nodiscard]] const std::string& name() const noexcept;
};

// Responsible for checking whether a specific string is a valid command-line argument
// or not.
class ArgumentValidator {
public:
  // Checks whether `argument_str` is not empty, starts with the `--` prefix and has
  // non-empty name: `--<name>`. Consider that it trims whitespace before checking.
  [[nodiscard]] bool is_argument(std::string_view argument_str) const;
};

// Responsible for reading and parsing the supplied arguments vector.
class VectorParser {
public:
  // An exception class that is thrown by the methods of VectorParser.
  class Exception : public std::runtime_error {
  public:
    enum class Code : std::uint8_t {
      NONE = 0,
      EMPTY_VECTOR,
      VECTOR_ARGUMENT_NULL,
      EMPTY_VECTOR_ARGUMENT_NAME,
      INTEGRAL_ARGUMENT_VALUE_OUT_OF_BOUNDS,
      ARGUMENT_NO_VALUE,
      ARGUMENT_EXPECTED_VALUE,
    };

  private:
    Code m_code = Code::NONE;

  public:
    Exception(const std::string& msg, Code code);

    [[nodiscard]] Code code() const noexcept;
  };

  // Parses the provided argument vector (a `vector` parameter).
  //
  // Command-line arguments separate their name and value. For example, one of them
  // shall break its name and value into two argument strings, like:
  //
  // `{ "--<name>", "<value>" }
  //
  // Discards argument argument strings that are empty and do not start with `--`.
  //
  // Throws `VectorParser::Exception` if:
  // - `vector` is empty;
  // - one of the arguments string from `vector` is null;
  // - a name of one of the arguments is empty;
  // - iteration ends, but one of the arguments has no value;
  // - the argument's integral value is out of bounds (overflow/underflow) of
  //   `ArgumentValue::Integer`;
  // - the argument's value is declaration of a next argument. Example:
  //
  //   { "--<argA>", "--<argB>" } (Expected a value after "--<argA>", but got argument
  //   declaration)
  [[nodiscard]] std::vector<Argument>
  parse_vector(const std::span<char*>& vector) const;
};

} // namespace NinjaReader::CommandLine::Argument

#endif
