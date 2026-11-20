// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include <algorithm>
#include <format>
#include <ranges>
#include <string>
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
ArgumentValue& Argument::value() noexcept {
  return m_value;
}

// VectorParser

VectorParser::Exception::Exception(const std::string& msg)
    : std::runtime_error(
          std::string("NinjaReader::CommandLine::Argument::VectorParser: ") + msg) {
}

namespace {
// A utility function which trims leading and ending whitespace from the provided
// string in place.
void trim_whitespace_in_place(std::string& string) {
  auto not_space = [](unsigned char c) {
    return !std::isspace(c);
  };
  string.erase(std::ranges::find_if(string | std::views::reverse, not_space).base(),
               string.end());
  string.erase(string.begin(), std::ranges::find_if(string, not_space));
}
} // namespace

std::vector<Argument> VectorParser::parse_vector(const std::span<char*>& vector) const {
  if (vector.empty()) {
    throw VectorParser::Exception("provided arguments vector is empty");
  }

  std::vector<Argument> parsed_arguments = {};
  std::string argument_name;
  bool parsing_argument_value = false;
  for (char* str : vector) {
    if (str == nullptr) {
      throw VectorParser::Exception("argument string cannot be null");
    }

    // As the argument value is parsed, "parsing_argument_value" must be set to false,
    // an Argument instance is created and pushed to the "parsed_arguments" vector.
    // Then, continue the iteration over the argument vector.
    if (parsing_argument_value) {
      std::string argument_value = str;

      // Since the std::stoi function throws an exception if a conversion failure
      // occurs, we can use a try/catch block to catch an exception and immediately go
      // to the next check. It stell the parser that this value is not an integer.
      try {
        auto argument =
            Argument(argument_name, ArgumentValue::from(std::stoi(argument_value)));
        parsed_arguments.push_back(argument);
        parsing_argument_value = false;
        continue;
      } catch (const std::exception&) {
        // .clang-tidy generates a warning about an empty catch block, therefore, we
        // declare this variable to prevent this warning.
        auto _ = 0;
      }

      {
        bool is_boolean = false;
        ArgumentValue::Boolean boolean = false;
        if (argument_value == "true") {
          boolean = true;
          is_boolean = true;
        } else if (argument_value == "false") {
          boolean = false;
          is_boolean = false;
        }

        if (is_boolean) {
          auto argument = Argument(argument_name, ArgumentValue::from(boolean));
          parsed_arguments.push_back(argument);
          parsing_argument_value = false;
          continue;
        }
      }
      // By default, we do not use string quotes because it is not portable, since Bash
      // strips out these before passing final arguments to a command. As long as a
      // solution appears fixing this issue, we'll use quotes.
      auto argument = Argument(argument_name, ArgumentValue::from(argument_value));
      parsed_arguments.push_back(argument);
      parsing_argument_value = false;
      continue;
    }

    std::string argument_str = str;
    trim_whitespace_in_place(argument_str);
    if (argument_str.empty() || !argument_str.starts_with("--")) {
      continue;
    }
    size_t equal_operator_index = argument_str.find("--");
    argument_name = argument_str.substr(equal_operator_index + 2);
    if (argument_name.empty()) {
      throw VectorParser::Exception("argument name cannot be empty");
    }
    parsing_argument_value = true;
  }

  if (parsing_argument_value) {
    throw VectorParser::Exception(
        std::format("argument \"{}\" has no value", argument_name));
  }

  return parsed_arguments;
}

} // namespace NinjaReader::CommandLine::Argument
