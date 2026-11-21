// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#ifndef NINJALOG_PARSER_H
#define NINJALOG_PARSER_H

#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace NinjaReader::Routines {

// Contains the parsed informaiton of a `.ninja_log` file. It is supplied by the
// `NinjaLogParser` class.
struct NinjaLog {
  struct Element {
    std::chrono::milliseconds start_time;
    std::chrono::milliseconds end_time;
    std::chrono::nanoseconds logged_mtime;
    std::filesystem::path output_file;
  };
  std::filesystem::path path;
  std::vector<Element> elements;
};

// Responsible for reading the provided `.ninja_log` and parsing it, producing a
// `NinjaLog` instance.
class NinjaLogParser {
public:
  class Exception : public std::runtime_error {
  public:
    Exception(const std::string& msg);
  };

  [[nodiscard]] NinjaLog parse(const std::filesystem::path& log_path) const;
};

} // namespace NinjaReader::Routines

#endif
