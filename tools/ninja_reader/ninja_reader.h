// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#ifndef NINJA_READER_H
#define NINJA_READER_H

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>

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

// Responsible for reading and parsing the supplied arguments vector.
class VectorParser {
public:
  // An exception class that is thrown by the methods of VectorParser.
  class Exception : public std::runtime_error {
  public:
    Exception(const std::string& msg);
  };

  void parse_vector(const std::span<char*>& vector) const;
};

} // namespace NinjaReader::CommandLine::Argument

#endif
