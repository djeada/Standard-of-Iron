// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include "commands.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

#include "commandline_argument.h"
#include "routines_ninjalog_parser.h"
#include "routines_sources.h"

namespace NinjaReader::Commands {

int help() {
  std::cout << "Available commands:\n\n";
  std::cout << "\thelp\tPrint this output\n";
  std::cout << "\read\tRead the .ninja_log file and get the slowest source files";
  return 0;
}

namespace {

struct ReadOptions {
  CommandLine::Argument::ArgumentValue::Integer top = 0;
  std::string build;
};

ReadOptions get_read_options(const std::span<char*>& argv) {
  ReadOptions ReadOptions;
  CommandLine::Argument::VectorParser vector_parser;
  auto parsed_arguments = vector_parser.parse_vector(argv);
  if (parsed_arguments.empty()) {
    throw std::runtime_error(
        "Usage: ninja_reader read --build <build directory with .ninja_log> --top "
        "<maximum count of top results, default: 20>");
  }

  auto top = CommandLine::Argument::get_argument_value(parsed_arguments, "top");
  if (!top) {
    ReadOptions.top = 20;
  } else if (top) {
    if (!top.value().is_integer()) {
      throw std::runtime_error("--top must contain an integer");
    }
    ReadOptions.top = top.value().integer();
  }

  auto build = CommandLine::Argument::get_argument_value(parsed_arguments, "build");
  if (!build) {
    throw std::runtime_error("--build option is required");
  }
  if (!build.value().is_string()) {
    throw std::runtime_error("--build must contain a string");
  }
  ReadOptions.build = build.value().string();

  return ReadOptions;
}
} // namespace

int read(const std::span<char*>& args) {
  Routines::NinjaLogParser ninjalog_parser;
  Routines::SourceFilesSorter source_files_sorter;
  Routines::SourceFilesCompilationTimeCalculator compilation_time_calculator;

  auto read_options = get_read_options(args);
  auto ninjalog_path = std::filesystem::path(read_options.build) / ".ninja_log";
  auto ninjalog = ninjalog_parser.parse(ninjalog_path);
  if (ninjalog.elements.empty()) {
    std::cout << "No source files found in " << ninjalog_path << "\n";
    return -1;
  }

  auto sorted_source_files = source_files_sorter.sort(
      ninjalog, Routines::SourceFilesSorter::Order::FROM_SLOWEST_TO_FASTEST);
  std::vector<std::pair<std::string, Routines::NinjaLog::Element>> newest_output_files;

  for (const auto& element : sorted_source_files) {
    auto it = std::find_if(
        newest_output_files.begin(),
        newest_output_files.end(),
        [&](const auto& existing) { return existing.first == element.output_file; });

    if (it == newest_output_files.end()) {
      newest_output_files.emplace_back(element.output_file, element);
    } else if (element.logged_mtime > it->second.logged_mtime) {
      it->second = element;
    }
  }

  std::sort(newest_output_files.begin(),
            newest_output_files.end(),
            [](const auto& lhs, const auto& rhs) {
              return Routines::duration_of_ninjalog_element(lhs.second) >
                     Routines::duration_of_ninjalog_element(rhs.second);
            });

  std::cout << "Top " << read_options.top << " slowest source files: \n\n";
  for (auto& sorted_source_file : newest_output_files) {
    std::cout << "* " << sorted_source_file.first << ": "
              << std::chrono::hh_mm_ss(
                     Routines::duration_of_ninjalog_element(sorted_source_file.second))
              << "\n";
  }

  std::cout << "Total compilation time: "
            << std::chrono::hh_mm_ss(
                   compilation_time_calculator.calculate_total_time(ninjalog))
            << "\n";
  std::cout << "Average compilation time: "
            << std::chrono::hh_mm_ss(
                   compilation_time_calculator.calculate_average_time(ninjalog))
            << "\n";

  return 0;
}

} // namespace NinjaReader::Commands
