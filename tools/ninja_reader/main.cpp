// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include <format>
#include <iostream>

#include "routines_ninjalog_parser.h"

int main() {

  using namespace NinjaReader::Routines;

  NinjaLogParser parser;
  auto log = parser.parse(std::filesystem::path("build/.ninja_log"));

  for (auto& element : log.elements) {
    std::cout << std::format(
        "Element(Start time: {}, End time: {}, Timestamp: {}, Output File: {})\n",
        element.start_time,
        element.end_time,
        element.end_timestamp_unix,
        element.output_file.string());
  }

  return 0;
}
