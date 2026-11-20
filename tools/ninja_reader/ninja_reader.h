// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#ifndef NINJA_READER_H
#define NINJA_READER_H

#include <span>
#include <stdexcept>
#include <string>

namespace NinjaReader::CommandLine::Argument {

// Responsible for reading and parsing the supplied arguments vector. Consider this
// might be a temporary decision, and we'll just take an existing argument parser
// instead of implementing own parser.
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
