#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "systems/battlefield_capture.h"

int main(int argc, char** argv) {
  Game::BattlefieldCapture::RunnerConfig config;
  std::string output_path;
  bool verify_run = false;
  bool run_all = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto value = [&](const char* option) -> std::string {
      if (++i >= argc) {
        std::cerr << "missing value for " << option << '\n';
        std::exit(2);
      }
      return argv[i];
    };
    if (arg == "--scenario") {
      if (!Game::BattlefieldCapture::parse_scenario(value("--scenario"),
                                                    config.scenario)) {
        std::cerr << "unknown scenario\n";
        return 2;
      }
    } else if (arg == "--seed") {
      config.seed = std::stoull(value("--seed"));
    } else if (arg == "--seconds") {
      config.duration_seconds = std::stod(value("--seconds"));
    } else if (arg == "--output") {
      output_path = value("--output");
    } else if (arg == "--verify") {
      verify_run = true;
    } else if (arg == "--all") {
      run_all = true;
      verify_run = true;
    } else if (arg == "--list") {
      for (auto id : Game::BattlefieldCapture::all_scenarios()) {
        std::cout << Game::BattlefieldCapture::scenario_name(id) << '\n';
      }
      return 0;
    } else {
      std::cerr << "unknown option: " << arg << '\n';
      return 2;
    }
  }
  if (run_all) {
    bool passed = true;
    for (auto const scenario : Game::BattlefieldCapture::all_scenarios()) {
      config.scenario = scenario;
      auto const capture = Game::BattlefieldCapture::run(config);
      auto const report = Game::BattlefieldCapture::verify(capture);
      std::cout << "[" << Game::BattlefieldCapture::scenario_name(scenario) << "] "
                << "tick_ms=" << capture.performance.slowest_tick_ms
                << " ai_decisions=" << capture.performance.ai_decisions
                << " ai_commands=" << capture.performance.ai_commands << ' ';
      Game::BattlefieldCapture::write_verification_report(report, std::cout);
      passed = passed && report.passed;
    }
    return passed ? 0 : 1;
  }
  const auto capture = Game::BattlefieldCapture::run(config);
  if (verify_run) {
    auto const report = Game::BattlefieldCapture::verify(capture);
    Game::BattlefieldCapture::write_verification_report(report, std::cout);
    return report.passed ? 0 : 1;
  }
  if (output_path.empty()) {
    Game::BattlefieldCapture::write_json_lines(capture, std::cout);
  } else {
    std::ofstream stream(output_path);
    if (!stream) {
      std::cerr << "cannot open output\n";
      return 1;
    }
    Game::BattlefieldCapture::write_json_lines(capture, stream);
  }
  return 0;
}
