// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include <format>
#include <iostream>

#include "routines_ninjalog_parser.h"
#include "routines_sources.h"

int main() {

  using namespace NinjaReader::Routines;

  NinjaLogParser parser;
  auto log = parser.parse(std::filesystem::path("build/.ninja_log"));

  SourceFilesSorter sorter;
  auto elements = sorter.sort(log, SourceFilesSorter::Order::FROM_FASTEST_TO_SLOWEST);

  for (const auto& element : elements) {
    const auto duration = element.end_time - element.start_time;

    std::cout << std::format("Duration: {}ms, Output File: {}\n",
                             duration.count(),
                             element.output_file.string());
  }

  SourceFilesCompilationTimeCalculator calculator;
  std::cout << "Total time: " << calculator.calculate_total_time(log) << "\n";
  std::cout << "Average time: " << calculator.calculate_average_time(log) << "\n";

  return 0;
}
