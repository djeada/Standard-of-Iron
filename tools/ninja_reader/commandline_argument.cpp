// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include <variant>

#include "ninja_reader.h"

namespace NinjaReader::CommandLine::Argument {

// ArgumentValue

ArgumentValue::ArgumentValue(Integer integer) noexcept
    : m_value(integer) {
}
ArgumentValue::ArgumentValue(String string) noexcept
    : m_value(string) {
}
ArgumentValue::ArgumentValue(Boolean boolean) noexcept
    : m_value(boolean) {
}

bool ArgumentValue::is_integer() const noexcept {
  return std::holds_alternative<Integer>(m_value);
}
ArgumentValue::Integer ArgumentValue::integer() const {
  return std::get<Integer>(m_value);
}

bool ArgumentValue::is_string() const noexcept {
  return std::holds_alternative<String>(m_value);
}
const ArgumentValue::String& ArgumentValue::string() const {
  return std::get<String>(m_value);
}

bool ArgumentValue::is_boolean() const noexcept {
  return std::holds_alternative<Boolean>(m_value);
}
ArgumentValue::Boolean ArgumentValue::boolean() const {
  return std::get<Boolean>(m_value);
}

// Argument

Argument::Argument(std::string name, ArgumentValue value) noexcept
    : m_name(std::move(name))
    , m_value(std::move(value)) {
}

const std::string& Argument::name() const noexcept {
  return m_name;
}
ArgumentValue &Argument::value() noexcept {
  return m_value;
}

// VectorParser

VectorParser::Exception::Exception(const std::string& msg)
    : std::runtime_error(
          std::string("NinjaReader::CommandLine::Argument::VectorParser: ") + msg) {
}

} // namespace NinjaReader::CommandLine::Argument
