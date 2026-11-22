// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include <array>
#include <gtest/gtest.h>
#include <span>
#include <string>

#include "tools/ninja_reader/commandline_argument.h"

using namespace NinjaReader::CommandLine;

namespace {

class ArgumentValidatorTest : public ::testing::Test {
protected:
  Argument::ArgumentValidator argument_validator;
};

TEST_F(ArgumentValidatorTest, ValidArgument) {
  std::array<const char*, 5> inputs = {
      "--name",
      "--1234cats",
      "--/*&^%$#@!\"\\",
      "--*&^2dogs",
      "--test=2",
  };
  for (auto& input : inputs) {
    EXPECT_TRUE(argument_validator.is_argument(input));
  }
}

TEST_F(ArgumentValidatorTest, InvalidArgument) {
  std::array<const char*, 8> inputs = {
      // Empty name
      "--",
      // Does not start with "--"
      "name",
      // Starts with hyphen, but a second one is required
      "-name",
      // Empty argument
      "",
      // Argument's integral value
      "1234567",
      // Argument's boolean values
      "true",
      "false",
      // Argument's string value
      "Hello, world!",
  };
  for (auto& input : inputs) {
    EXPECT_FALSE(argument_validator.is_argument(input));
  }
}

class VectorParserTest : public ::testing::Test {
protected:
  Argument::VectorParser argument_vector_parser;
};

struct VectorParserInputElement {
  std::vector<std::string> argument_str;
  std::string option;
  Argument::ArgumentValue value;
};

// An utility function which converts the provided `std::vector<std::string>` to
// `std::vector<char *>`. It is used in tests of `VectorParserTest`.
std::vector<char*>
convert_string_vector_to_char_vector(std::vector<std::string>& list) {
  std::vector<char*> output;
  output.reserve(list.size());
  for (auto& str : list) {
    output.push_back(str.data());
  }
  return output;
}

TEST_F(VectorParserTest, EmptyArgumentsVectorException) {
  try {
    std::vector<char*> args;

    // In subsequent VectorParserTest tests, we declare a variable with no name to
    // prevent clang-tidy from generating a "ignoring return value" warning, because in
    // some tests, we do not need output.
    auto _ = argument_vector_parser.parse_vector(args);

    FAIL() << "Expected VectorParser::Exception";
  } catch (const Argument::VectorParser::Exception& ex) {
    EXPECT_EQ(ex.code(), Argument::VectorParser::Exception::Code::EMPTY_VECTOR);
  }
}

TEST_F(VectorParserTest, NullArgumentException) {
  try {
    std::vector<char*> args = {nullptr};
    auto _ = argument_vector_parser.parse_vector(args);
    FAIL() << "Expected VectorParser::Exception";
  } catch (const Argument::VectorParser::Exception& ex) {
    EXPECT_EQ(ex.code(), Argument::VectorParser::Exception::Code::VECTOR_ARGUMENT_NULL);
  }
}

TEST_F(VectorParserTest, EmptyVectorArgumentNameException) {
  try {
    std::string argument_str = "--";
    std::vector<char*> args = {argument_str.data()};
    auto _ = argument_vector_parser.parse_vector(args);
    FAIL() << "Expected VectorParser::Exception";
  } catch (const Argument::VectorParser::Exception& ex) {
    EXPECT_EQ(ex.code(),
              Argument::VectorParser::Exception::Code::EMPTY_VECTOR_ARGUMENT_NAME);
  }
}

TEST_F(VectorParserTest, IntegralArgumentValueOutOfBoundsException) {
  try {
    std::string argument_str = "--id";
    std::string argument_value = "9223372036854775807";
    std::vector<char*> args = {argument_str.data(), argument_value.data()};
    auto _ = argument_vector_parser.parse_vector(args);
    FAIL() << "Expected VectorParser::Exception";
  } catch (const Argument::VectorParser::Exception& ex) {
    EXPECT_EQ(
        ex.code(),
        Argument::VectorParser::Exception::Code::INTEGRAL_ARGUMENT_VALUE_OUT_OF_BOUNDS);
  }
}

TEST_F(VectorParserTest, ArgumentExpectedValueException) {
  try {
    std::string argument_str = "--name";
    std::string option = "--foo";
    std::vector<char*> args = {argument_str.data(), option.data()};
    auto _ = argument_vector_parser.parse_vector(args);
    FAIL() << "Expected VectorParser::Exception";
  } catch (const Argument::VectorParser::Exception& ex) {
    EXPECT_EQ(ex.code(),
              Argument::VectorParser::Exception::Code::ARGUMENT_EXPECTED_VALUE);
  }
}

#define CREATE_ARGUMENT_VALUE_TEST_BODY(input_elements, is_method, get_value_method)   \
  try {                                                                                \
    std::vector<VectorParserInputElement> __input_elements = input_elements;           \
    for (auto& input_element : __input_elements) {                                     \
      auto argument_c_str =                                                            \
          convert_string_vector_to_char_vector(input_element.argument_str);            \
      auto parsed_arguments = argument_vector_parser.parse_vector(argument_c_str);     \
      ASSERT_FALSE(parsed_arguments.empty());                                          \
      EXPECT_EQ(parsed_arguments.at(0).name(), input_element.option);                  \
      EXPECT_EQ(parsed_arguments.at(0).value().is_method(),                            \
                input_element.value.is_method());                                      \
      EXPECT_EQ(parsed_arguments.at(0).value().get_value_method(),                     \
                input_element.value.get_value_method());                               \
    }                                                                                  \
  } catch (const Argument::VectorParser::Exception& ex) {                              \
    FAIL() << "Did not expect Argument::VectorParser::Exception: " << ex.what()        \
           << " (code: " << ex.code() << ")";                                          \
  } catch (const std::exception& ex) {                                                 \
    FAIL() << "Did not expect the following exception: " << ex.what();                 \
  } catch (...) {                                                                      \
    FAIL() << "Did not expect unknown exception";                                      \
  }

TEST_F(VectorParserTest, ArgumentBooleanValue) {
  std::vector<VectorParserInputElement> input_elements = {
      {
          .argument_str = {"--ok", "true"},
          .option = "true",
          .value = Argument::ArgumentValue::from(true),
      },
      {
          .argument_str = {"--failed", "false"},
          .option = "false",
          .value = Argument::ArgumentValue::from(false),
      },
  };
  CREATE_ARGUMENT_VALUE_TEST_BODY(input_elements, is_boolean, boolean);
}

TEST_F(VectorParserTest, ArgumentStringValue) {
  std::vector<VectorParserInputElement> input_elements = {
      {
          .argument_str = {"--name", "dywoq"},
          .option = "name",
          .value = Argument::ArgumentValue::from("dywoq"),
      },
      {
          .argument_str = {"--name", "12cats"},
          .option = "name",
          .value = Argument::ArgumentValue::from("12cats"),
      },
      {
          .argument_str = {"--name", "/*&^%$#@!\"\\"},
          .option = "name",
          .value = Argument::ArgumentValue::from("/*&^%$#@!\"\\"),
      },
  };
  CREATE_ARGUMENT_VALUE_TEST_BODY(input_elements, is_string, string);
}

TEST_F(VectorParserTest, ArgumentIntegralValue) {
  std::vector<VectorParserInputElement> input_elements = {
      {
          .argument_str = {"--id", "1234"},
          .option = "id",
          .value = Argument::ArgumentValue::from(1234),
      },
      {
          .argument_str = {"--id", "-1232"},
          .option = "id",
          .value = Argument::ArgumentValue::from(-1232),
      },
      {
          .argument_str = {"--id", "-9223372036854775807"},
          .option = "name",
          .value = Argument::ArgumentValue::from(-9223372036854775807),
      },
      {
          .argument_str = {"-id", "9223372036854775806"},
          .option = "id",
          .value = Argument::ArgumentValue::from(9223372036854775806),
      },
  };
  CREATE_ARGUMENT_VALUE_TEST_BODY(input_elements, is_integer, integer);
}

} // namespace
