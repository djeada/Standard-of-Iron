// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include <iostream>
#include <optional>
#include <span>
#include <string>

#include "commandline_argument.h"
#include "commands.h"

using namespace NinjaReader;
using namespace NinjaReader::CommandLine;

namespace {

std::optional<std::string> get_command_name(const std::span<char*>& args) noexcept {
  Argument::ArgumentValidator argument_validator;
  for (auto it = args.begin(); it != args.end(); ++it) {
    if (!argument_validator.is_argument(*it)) {
      return std::string(*it);
    }
    if (argument_validator.is_argument(*it)) {
      // Advance because there is a value after the option.
      ++it;
    }
  }
  return std::nullopt;
}

} // namespace

int main(int argc, char* argv[]) {
  auto command_name = get_command_name(std::span(&argv[1], argc - 1));
  if (!command_name) {
    return Commands::help();
  }

  if (command_name.value() == "help") {
    return Commands::help();
  } else if (command_name.value() == "read") {
    return Commands::read(std::span(argv, argc));
  } else {
    std::cout << "Unknown command: " << command_name.value() << "\n";
  }

  return 0;
}
