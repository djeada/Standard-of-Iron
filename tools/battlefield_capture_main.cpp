#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "game/session/session_context.h"
#include "systems/battlefield_capture.h"

int main(int argc, char** argv) {

  Game::Session::SessionContext session;
  Game::Session::ScopedSession const active_session(session);
  Game::BattlefieldCapture::RunnerConfig config;
  std::string output_path;
  bool verify_run = false;
  bool run_all = false;
  int determinism_runs = 0;
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
    } else if (arg == "--determinism-runs") {

      determinism_runs = std::stoi(value("--determinism-runs"));
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
  if (determinism_runs > 1) {
    bool passed = true;
    const auto scenarios =
        run_all ? Game::BattlefieldCapture::all_scenarios()
                : std::vector<Game::BattlefieldCapture::ScenarioId>{config.scenario};
    for (auto const scenario : scenarios) {
      config.scenario = scenario;
      auto const report =
          Game::BattlefieldCapture::check_determinism(config, determinism_runs);
      std::cout << "[" << Game::BattlefieldCapture::scenario_name(scenario) << "] ";
      if (report.deterministic()) {
        std::cout << "DETERMINISTIC across " << report.runs << " runs\n";
        continue;
      }
      passed = false;
      std::cout << "DIVERGED at tick " << *report.divergent_tick << " (run "
                << report.divergent_run << " vs run 0)\n";

      std::istringstream first(report.first_state);
      std::istringstream other(report.other_state);
      std::string a;
      std::string b;
      int shown = 0;
      while (std::getline(first, a) && std::getline(other, b) && shown < 20) {
        if (a != b) {
          std::cout << "  run0: " << a << "\n  run" << report.divergent_run << ": " << b
                    << '\n';
          ++shown;
        }
      }
    }
    return passed ? 0 : 1;
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
  // A single scenario without --verify dumps telemetry and returns 0 whatever
  // it found. That is the right behaviour for inspecting a run, and a trap for
  // anyone who expects a gate, so say so.
  std::fputs("note: no verdict was computed; add --verify to check this scenario, "
             "or --all to check every one\n",
             stderr);

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
