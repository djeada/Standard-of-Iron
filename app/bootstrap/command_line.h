#pragma once

#include <QSize>
#include <QString>

#include <optional>

#include "app/core/benchmark_action_fixture.h"
#include "app/core/film_recorder.h"
#include "app/core/game_speed.h"

class QCoreApplication;

namespace App::Bootstrap {

struct CommandLineOptions {
  QString direct_campaign_mission;
  QString direct_mission_file;
  QString observe_map_file;
  QString record_replay_path;
  QString replay_path;
  bool replay_verify = false;
  bool skip_briefing = false;
  float direct_game_speed = App::Core::GameSpeed::k_default;
  bool component_gallery_requested = false;
  QString screenshot_path;
  QString screenshot_view;
  int screenshot_delay_ms = 0;
  QSize screenshot_size{1600, 900};
  double runtime_benchmark_seconds = 0.0;
  QString runtime_benchmark_output;
  QString runtime_action_fixture_path;
  std::optional<App::Core::BenchmarkActionFixture> runtime_action_fixture;
  std::optional<App::Core::FilmConfig> film_config;
};

auto parse_command_line(QCoreApplication& app,
                        bool release_self_test,
                        CommandLineOptions& opts) -> int;

} // namespace App::Bootstrap
