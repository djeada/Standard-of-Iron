// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include "ninja_reader.h"
#include <iostream>

int main(int argc, char* argv[]) {

  using namespace NinjaReader::CommandLine::Argument;

  VectorParser parser;
  auto arguments = parser.parse_vector(std::span(argv, argc));

  for (auto& arg : arguments) {
    std::cout << arg.name() << " = ";
    auto value = arg.value();
    if (value.is_string()) {
      std::cout << value.string() << " (STRING)" << std::endl;
    }
    if (value.is_integer()) {
      std::cout << value.integer() << " (INTEGER)"<< std::endl;
    }
    if (value.is_boolean()) {
      std::cout << value.boolean() << " (BOOLEAN)"<< std::endl;
    }
  }

  return 0;
}
