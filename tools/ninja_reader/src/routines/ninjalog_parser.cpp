// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include <format>

#include "ninja_reader.h"

namespace NinjaReader::Routines {

NinjaLogParser::Exception::Exception(const std::string& msg)
    : std::runtime_error(
          std::format("NinjaReader::Routines::NinjaLogParser::Exception: {}", msg)) {
}

} // namespace NinjaReader::Routines
