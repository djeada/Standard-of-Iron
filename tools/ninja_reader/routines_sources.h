// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#ifndef ROUTINES_SOURCES_H
#define ROUTINES_SOURCES_H

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "routines_ninjalog_parser.h"

namespace NinjaReader::Routines {

class SourceFilesSorter {
public:
  class Exception : public std::runtime_error {
  public:
    Exception(const std::string& msg);
  };

  // Specifies the order source files are sorted in.
  enum class Order : std::uint8_t {
    // Indicates that source files taking the longest to compile go first in the
    // supplied vector.
    FROM_SLOWEST_TO_FASTEST,
    // Indicates that fastest source files taking fast to compile go first in the
    // supplied vector.
    FROM_FASTEST_TO_SLOWEST,
  };

  [[nodiscard]] std::vector<NinjaLog::Element> sort(const NinjaLog& ninja_log,
                                                    Order order) const;
};

// Responsible for calculating the average and total compilation time of source files
// that is achived through reading `NinjaLog` structure.
class SourceFilesCompilationTimeCalculator {
public:
  [[nodiscard]] std::chrono::milliseconds
  calculate_average_time(const NinjaLog& ninja_log) const;

  [[nodiscard]] std::chrono::milliseconds
  calculate_average_time(const std::vector<NinjaLog::Element>& elements) const;

  [[nodiscard]] std::chrono::milliseconds
  calculate_total_time(const NinjaLog& ninja_log) const;

  [[nodiscard]] std::chrono::milliseconds
  calculate_total_time(const std::vector<NinjaLog::Element>& elements) const;
};

} // namespace NinjaReader::Routines

#endif
