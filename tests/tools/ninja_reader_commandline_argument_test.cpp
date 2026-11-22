// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include <array>
#include <gtest/gtest.h>
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
      "--name", "--1234cats", "--/*&^%$#@!\"\\", "--*&^2dogs", "--test=2"};
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

} // namespace
