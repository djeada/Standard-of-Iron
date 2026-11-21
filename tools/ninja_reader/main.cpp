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
    std::cout << std::format("Element(Start time: {}, End time: {}, Logged "
                             "modification time: {}, Output File: {})\n",
                             element.start_time,
                             element.end_time,
                             element.logged_mtime,
                             element.output_file.string());
  }

  return 0;
}
