// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include "ninja_reader.h"

namespace NinjaReader::CommandLine::Argument {

VectorParser::Exception::Exception(const std::string& msg)
    : std::runtime_error(
          std::string("NinjaReader::CommandLine::Argument::VectorParser: ") + msg) {
}

} // namespace NinjaReader::CommandLine::Argument
