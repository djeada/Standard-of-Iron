// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>

#include "ninja_reader.h"

namespace NinjaReader::Routines {

// NinjaLogParser::Exception

NinjaLogParser::Exception::Exception(const std::string& msg)
    : std::runtime_error(
          std::format("NinjaReader::Routines::NinjaLogParser::Exception: {}", msg)) {
}

// NinjaLogParser

namespace {
// Indicates the current stage of a NinjaLog element (`NinjaLog::Element`). In general,
// it is needed to report error destination when throwing exceptions in the ninja log
// parser.
enum class ElementParsingStage : std::uint8_t {
  NONE = 0,
  START_TIME,
  END_TIME,
  END_TIMESTAMP_UNIX,
  OUTPUT_FILE
};

// Used to convert the element parsing stage into a human-readable message that is
// placed at the end of error messages. For example:
//
//    "Detected an integer overflow " +
//        element_parsing_stage_string(ElementParsingStage::START_TIME)
//    = "Detected an integer overflow when retrieving the start time".
//
// This works with the rest of stages.
const char* element_parsing_stage_string(ElementParsingStage element_parsing_stage) {
  switch (element_parsing_stage) {
  case ElementParsingStage::NONE:
    return " in an unknown stage";
  case ElementParsingStage::START_TIME:
    return " when retrieving the start time";
  case ElementParsingStage::END_TIME:
    return " when retrieving the end time";
  case ElementParsingStage::END_TIMESTAMP_UNIX:
    return " when retrieving the end timestamp (UNIX)";
  case ElementParsingStage::OUTPUT_FILE:
    return " when retrieving the output file";
  default:
    return " in an unknown stage";
  }
}

} // namespace

NinjaLog NinjaLogParser::parse(const std::filesystem::path& log_path) const {
  // Ignore the clang-tidy warning about "exceeds recommended size thresholds". That
  // warning is generated because of large comment blocks, which are for documentation
  // purposes.

  if (!std::filesystem::exists(log_path)) {
    throw Exception(std::format("{} does not exist", log_path.string()));
  }
  if (!std::filesystem::is_regular_file(log_path)) {
    throw Exception(std::format("{} is not a regular file", log_path.string()));
  }

  std::ifstream file_stream(log_path);
  if (!file_stream.is_open()) {
    throw Exception(std::format("failed to open {}", log_path.string()));
  }

  NinjaLog ninja_log;
  ninja_log.elements = {};
  std::string line;
  size_t line_number = 1;
  ElementParsingStage element_parsing_stage = ElementParsingStage::NONE;
  while (std::getline(file_stream, line)) {
    // The first line of .ninja_log contains its version, so we discard it because we do
    // not need it unless it is required.
    if (line_number == 1 || line.empty()) {
      line_number++;
      continue;
    }

    // Each line has five columns that are separated by tabs. Each column contains
    // its own information:
    //
    //    Column 1: Start time
    //
    //    Column 2: End time
    //
    //    Column 3: End timestamp in nanoseconds (UNIX)
    //
    //    Column 4: Output file
    //
    //    Column 5: Hash that belongs to the output file (not used in this class)

    try {
      NinjaLog::Element element;

      // The procedure of getting information in each column:
      //
      // 1. Identifiy and set the element parsing stage to a corresponding value. Its
      // value depends on which information the function is currently retrieving.
      //
      // 2. Initialize the starting index that is "next_tab_index + 1". We do not use
      // the starting index in the first block of code (where we get the start time),
      // because the start time is already placed at zero index.
      //
      // 3. Find the index of a next tab character that begins the next column. If it
      // was not found (next_tab_index == std::string::npos), throw an exception
      // (NinjaLogParser::Exception).
      //
      // 4. Extract a substring from "line" using:
      //
      //    "line.substr(start_index, next_tab_index - start_index)".
      //
      // 5. Provide a specific field in the "element" variable with extracted
      // information.

      element_parsing_stage = ElementParsingStage::START_TIME;
      size_t next_tab_index = line.find('\t');
      if (next_tab_index == std::string::npos) {
        throw Exception(
            std::format("Failed to get the starting tab character of the end time {}",
                        element_parsing_stage_string(element_parsing_stage)));
      }
      element.start_time =
          std::chrono::milliseconds(std::stoi(line.substr(0, next_tab_index)));

      element_parsing_stage = ElementParsingStage::END_TIME;
      size_t start_index = next_tab_index + 1;
      next_tab_index = line.find('\t', start_index);
      if (next_tab_index == std::string::npos) {
        throw Exception(std::format(
            "Failed to get the starting tab character of the end timestamp {}",
            element_parsing_stage_string(element_parsing_stage)));
      }
      element.end_time = std::chrono::milliseconds(
          std::stoi(line.substr(start_index, next_tab_index - start_index)));

      element_parsing_stage = ElementParsingStage::END_TIMESTAMP_UNIX;
      start_index = next_tab_index + 1;
      next_tab_index = line.find('\t', start_index);
      if (next_tab_index == std::string::npos) {
        throw Exception(
            std::format("Failed to get the starting tab character of output file {}",
                        element_parsing_stage_string(element_parsing_stage)));
      }
      element.end_timestamp_unix = std::chrono::nanoseconds(
          std::stoll(line.substr(start_index, next_tab_index - start_index)));

      element_parsing_stage = ElementParsingStage::OUTPUT_FILE;
      start_index = next_tab_index + 1;
      next_tab_index = line.find('\t', start_index);
      if (next_tab_index == std::string::npos) {
        throw Exception(
            std::format("Failed to get the starting tab character of hash {}",
                        element_parsing_stage_string(element_parsing_stage)));
      }
      element.output_file = line.substr(start_index, next_tab_index - start_index);

      ninja_log.elements.push_back(element);

    } catch (const std::out_of_range&) {
      // This exception (std::out_of_range) is supposed to be thrown by std::stoi and
      // std::stoll
      throw Exception(std::format("Detected integer overflow/underflow {}",
                                  element_parsing_stage_string(element_parsing_stage)));
    } catch (const std::invalid_argument&) {
      // This exception (std::invalid_argument) is supposed to be thrown by std::stoi
      // and std::stoll
      throw Exception(std::format("Detected an invalid integer {}",
                                  element_parsing_stage_string(element_parsing_stage)));
    } catch (const Exception& e) {
      throw e;
    } catch (const std::exception& ex) {
      throw Exception(std::format("Encoutered an unknown error: {}", ex.what()));
    }

    line_number++;
  }

  return ninja_log;
}

} // namespace NinjaReader::Routines
