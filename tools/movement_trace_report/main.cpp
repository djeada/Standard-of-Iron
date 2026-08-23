// Reads a MovementTrace artifact directory and prints the analysis.
//
// The plan's rule is that a movement defect must become a machine-readable
// failure before anyone touches the code that causes it. This is the tool that
// turns a captured run into that failure: a summary table, the findings ranked
// by first occurrence, and a timeline window around the first failing tick for
// the worst entity and the worst soldier.
//
//   movement_trace_report <directory> [--timeline N] [--quiet] [--digest]
//
// Exits nonzero when the analysis finds anything, so it can be a gate.

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "core/movement_trace.h"
#include "core/movement_trace_analysis.h"

namespace {

void print_usage() {
  std::cout << "usage: movement_trace_report <trace-directory> [--timeline N] "
               "[--quiet] [--digest]\n";
}

} // namespace

auto main(int argc, char** argv) -> int {
  if (argc < 2) {
    print_usage();
    return 2;
  }

  std::string directory;
  int timeline_window = 20;
  bool quiet = false;
  bool digest_only = false;

  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--timeline" && index + 1 < argc) {
      timeline_window = std::atoi(argv[++index]);
    } else if (argument == "--quiet") {
      quiet = true;
    } else if (argument == "--digest") {
      digest_only = true;
    } else if (argument == "--help" || argument == "-h") {
      print_usage();
      return 0;
    } else if (directory.empty()) {
      directory = argument;
    }
  }

  if (directory.empty()) {
    print_usage();
    return 2;
  }

  std::vector<Engine::Core::MovementTroopSample> troops;
  std::vector<Engine::Core::MovementSoldierSample> soldiers;
  if (!Engine::Core::load_movement_trace_directory(directory, troops, soldiers)) {
    std::cerr << "could not read a trace from " << directory << '\n';
    return 2;
  }

  const std::string digest = Engine::Core::movement_digest(troops, soldiers);
  if (digest_only) {
    std::cout << digest << '\n';
    return 0;
  }

  Engine::Core::MovementGateThresholds thresholds;
  const auto analysis =
      Engine::Core::analyze_movement_trace(troops, soldiers, thresholds);

  std::cout << troops.size() << " troop samples, " << soldiers.size()
            << " soldier samples, digest " << digest << "\n\n";

  if (!quiet) {
    std::cout << Engine::Core::format_movement_summary(analysis) << '\n';
  }

  if (analysis.passed()) {
    std::cout << "no findings\n";
    return 0;
  }

  std::cout << analysis.findings.size() << " findings, first at tick "
            << analysis.first_failing_tick << "\n\n"
            << Engine::Core::format_movement_findings(analysis) << '\n';

  if (timeline_window > 0) {
    const auto worst = analysis.worst_entity();
    std::cout << "timeline for entity " << worst << " around tick "
              << analysis.first_failing_tick << ":\n"
              << Engine::Core::format_movement_timeline(
                     troops,
                     worst,
                     analysis.first_failing_tick,
                     static_cast<std::uint32_t>(timeline_window),
                     static_cast<std::uint32_t>(timeline_window))
              << '\n';

    if (const auto* soldier = analysis.worst_soldier(); soldier != nullptr) {
      std::cout << "timeline for troop " << soldier->troop_id << " slot "
                << soldier->slot << ":\n"
                << Engine::Core::format_soldier_timeline(
                       soldiers,
                       soldier->troop_id,
                       soldier->slot,
                       analysis.first_failing_frame,
                       static_cast<std::uint32_t>(timeline_window),
                       static_cast<std::uint32_t>(timeline_window))
                << '\n';
    }
  }

  return 1;
}
