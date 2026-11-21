// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include "routines_sources.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

#include "routines_ninjalog_parser.h"

namespace NinjaReader::Routines {

// SourceFilesSorter::Exception

SourceFilesSorter::Exception::Exception(const std::string& msg)
    : std::runtime_error(
          std::format("NinjaReader::Routines::SourceFilesSorter::Exception: {}", msg)) {
}

namespace {

std::chrono::milliseconds
ninjalog_element_duration(const NinjaLog::Element& elem) noexcept {
  return elem.end_time - elem.start_time;
}

} // namespace

// SourceFilesSorter

std::vector<NinjaLog::Element>
SourceFilesSorter::sort(const NinjaLog& ninja_log,
                        SourceFilesSorter::Order order) const {
  if (ninja_log.elements.empty()) {
    throw Exception("Supplied elements vector of the NinjaLog is empty");
  }

  auto elements_copy = ninja_log.elements;
  switch (order) {
  case Order::FROM_SLOWEST_TO_FASTEST:
    std::ranges::sort(
        elements_copy,
        [](const NinjaLog::Element& first, const NinjaLog::Element& second) {
          return ninjalog_element_duration(first) > ninjalog_element_duration(second);
        });
    break;

  case Order::FROM_FASTEST_TO_SLOWEST:
    std::ranges::sort(
        elements_copy,
        [](const NinjaLog::Element& first, const NinjaLog::Element& second) {
          return ninjalog_element_duration(first) < ninjalog_element_duration(second);
        });
    break;

  default:
    throw Exception(
        "Unknown supplied order. Expected Order::FROM_SLOWEST_TO_FASTEST or "
        "Order::FROM_FASTEST_TO_SLOWEST");
  }
  return elements_copy;
}

// SourceFilesCompilationTimeCalculator

std::chrono::milliseconds SourceFilesCompilationTimeCalculator::calculate_average_time(
    const NinjaLog& ninja_log) const {
  auto total_time = calculate_total_time(ninja_log);
  return total_time / std::chrono::milliseconds::rep(ninja_log.elements.size());
}

std::chrono::milliseconds SourceFilesCompilationTimeCalculator::calculate_total_time(
    const NinjaLog& ninja_log) const {
  std::chrono::milliseconds calculation_result(0);
  for (const auto& elem : ninja_log.elements) {
    calculation_result += ninjalog_element_duration(elem);
  }
  return calculation_result;
}

} // namespace NinjaReader::Routines
