// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include "commands.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "commandline_argument.h"
#include "routines_ninjalog_parser.h"
#include "routines_sources.h"

namespace NinjaReader::Commands {

int help() {
  std::cout << "Available commands:\n";
  std::cout << "\thelp\tPrint this output\n";
  std::cout << "\tread\tRead the .ninja_log file and get the slowest source files\n";
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

  try {
    auto read_options = get_read_options(args);
    auto ninjalog_path = std::filesystem::path(read_options.build) / ".ninja_log";
    auto ninjalog = ninjalog_parser.parse(ninjalog_path);
    if (ninjalog.elements.empty()) {
      std::cout << "No source files found in " << ninjalog_path << "\n";
      return -1;
    }

    auto sorted_source_files = source_files_sorter.sort(
        ninjalog, Routines::SourceFilesSorter::Order::FROM_SLOWEST_TO_FASTEST);
    std::vector<std::pair<std::string, Routines::NinjaLog::Element>>
        latest_output_files;

    for (const auto& element : sorted_source_files) {
      auto it = std::ranges::find_if(latest_output_files, [&](const auto& existing) {
        return existing.first == element.output_file;
      });
      if (it == latest_output_files.end()) {
        latest_output_files.emplace_back(element.output_file, element);
      } else if (element.logged_mtime > it->second.logged_mtime) {
        it->second = element;
      }
    }

    std::ranges::sort(latest_output_files, [](const auto& lhs, const auto& rhs) {
      return Routines::duration_of_ninjalog_element(lhs.second) >
             Routines::duration_of_ninjalog_element(rhs.second);
    });

    auto final_top = std::min(static_cast<std::size_t>(read_options.top),
                              latest_output_files.size());
    if (read_options.top > latest_output_files.size()) {
      std::cout << "(there are only " << latest_output_files.size()
                << " source files)\n";
    }
    std::cout << "Top " << final_top << " slowest source files: \n\n";
    for (auto it = latest_output_files.begin();
         it != latest_output_files.end() &&
         std::cmp_less(it - latest_output_files.begin(), final_top);
         ++it) {
      const auto& output_file = *it;
      std::cout << "* " << output_file.first << ": "
                << std::chrono::hh_mm_ss(
                       Routines::duration_of_ninjalog_element(output_file.second))
                << "\n";
    }

    // Temporarily construct a vector of NinjaLog::Element instances so the functions of
    // the compilation_time_calculator could accept it. It is needed to consider latest
    // output files only during calculation process.

    std::vector<Routines::NinjaLog::Element> ninjalog_elements;
    ninjalog_elements.reserve(latest_output_files.size());
    for (auto& output_file : latest_output_files) {
      ninjalog_elements.push_back(output_file.second);
    }

    std::cout << "Total compilation time: "
              << std::chrono::hh_mm_ss(compilation_time_calculator.calculate_total_time(
                     ninjalog_elements))
              << "\n";
    std::cout << "Average compilation time: "
              << std::chrono::hh_mm_ss(
                     compilation_time_calculator.calculate_average_time(
                         ninjalog_elements))
              << "\n";

    // Additional exception classes are not required to be caught here, since all of
    // them inherit std::runtime_error. If a specific exception class does not inherit
    // std::runtime_error, add it explicitly here, or, add its base class.
  } catch (const std::runtime_error& e) {
    std::cout << e.what() << "\n";
    return -1;
  } catch (...) {
    std::cout << "Unknown error" << "\n";
    return -1;
  }

  return 0;
}

} // namespace NinjaReader::Commands
