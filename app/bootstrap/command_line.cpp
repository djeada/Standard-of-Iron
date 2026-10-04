#include "app/bootstrap/command_line.h"

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <optional>

#include "render/graphics_settings.h"

namespace App::Bootstrap {

auto parse_command_line(QCoreApplication& app,
                        bool release_self_test,
                        CommandLineOptions& opts) -> int {
  QCommandLineParser parser;
  parser.setApplicationDescription("Standard of Iron");
  parser.addHelpOption();
  QCommandLineOption const force_software_opt(
      QStringList{"s", "force-software"},
      "Force the CPU software rendering backend (ShaderQuality::None).");
  QCommandLineOption const quality_opt(
      "quality", "Override shader quality: full | reduced | minimal | none.", "level");
  QCommandLineOption const renderer_self_test_opt(
      "renderer-self-test",
      "Show the gameplay view, render and present one frame, then exit.");
  QCommandLineOption const release_self_test_opt(
      "release-self-test",
      "Validate a fresh profile and campaign assets, start a real campaign "
      "mission, present frames, then exit.");
  QCommandLineOption const print_data_paths_opt(
      "print-data-paths", "Print where saves and settings are stored, then exit.");
  QCommandLineOption const graphics_preset_opt(
      "graphics-preset",
      "Override the complete graphics preset: low | medium | high | ultra.",
      "preset");
  QCommandLineOption const campaign_mission_opt(
      "campaign-mission",
      "Start a campaign mission directly (campaign_id/mission_id).",
      "path");
  QCommandLineOption const mission_file_opt(
      "mission-file",
      "Start a mission definition file directly for editor testing.",
      "path");
  QCommandLineOption const observe_opt(
      "observe",
      "Start this skirmish map with every slot under computer control and watch it "
      "as a spectator.",
      "map-path");
  QCommandLineOption const record_replay_opt(
      "record-replay",
      "Write every command the match accepts to this file, so the match can be "
      "played back with --replay.",
      "path");
  QCommandLineOption const replay_opt(
      "replay",
      "Launch the match a replay file describes and let the file drive it; local "
      "input and the computer opponent are shut out.",
      "path");
  QCommandLineOption const replay_verify_opt(
      "replay-verify",
      "With --replay: exit when the replay has played through, 0 if the "
      "simulation matched every recorded digest, 12 if it diverged.");
  QCommandLineOption const skip_briefing_opt(
      "skip-briefing",
      "Start a directly launched mission unpaused, without the objectives "
      "briefing (for scripted runs).");
  QCommandLineOption const component_gallery_opt(
      "component-gallery",
      "Open the Iron and Ember component gallery instead of the game.");
  QCommandLineOption const screenshot_opt(
      "screenshot", "Render one frame, write a PNG to this path, then exit.", "path");
  QCommandLineOption const screenshot_view_opt(
      "screenshot-view",
      "Surface to capture: menu | skirmish | missions | campaign | settings | load "
      "| save | briefing | hud | rpg | commander | tutorial.",
      "view",
      "menu");
  QCommandLineOption const screenshot_delay_opt(
      "screenshot-delay",
      "Milliseconds to let the surface settle before capturing.",
      "ms",
      "1200");
  QCommandLineOption const screenshot_size_opt(
      "screenshot-size",
      "Window size for --screenshot, e.g. 1280x800 (Steam Deck) or 1920x1080.",
      "WxH",
      "1600x900");
  QCommandLineOption const game_speed_opt(
      "game-speed",
      "Start a directly launched mission at this battle speed (0.5, 1, 2, 3 or 4).",
      "multiplier");
  QCommandLineOption const benchmark_seconds_opt(
      "benchmark-seconds",
      "Measure the directly started mission after a two-second warm-up, then exit.",
      "seconds");
  QCommandLineOption const benchmark_output_opt(
      "benchmark-output",
      "Write the runtime benchmark JSON report to this path.",
      "path");
  QCommandLineOption const action_fixture_opt(
      "action-fixture",
      "Drive a versioned battle/UI action fixture during the benchmark or the film.",
      "path");
  QCommandLineOption const film_opt(
      "film",
      "Film the directly launched mission one simulation step per frame into "
      "numbered PNGs in this directory, then exit. Wall-clock speed does not "
      "matter, so a software GL display still yields full-rate footage.",
      "dir");
  QCommandLineOption const film_fps_opt(
      "film-fps", "Frames (and simulation steps) per second of film.", "fps", "60");
  QCommandLineOption const film_seconds_opt(
      "film-seconds", "Seconds of footage to write.", "seconds", "8");
  QCommandLineOption const film_start_opt(
      "film-start",
      "Simulate this many seconds (running the fixture) before the first frame "
      "is written.",
      "seconds",
      "0");
  QCommandLineOption const film_size_opt(
      "film-size", "Frame size as WIDTHxHEIGHT.", "size", "1920x1080");
  QCommandLineOption const film_visible_opt(
      "film-visible",
      "Film in a normal window (default: a frameless window pinned to the "
      "bottom of the stack that never takes focus).");
  parser.addOption(force_software_opt);
  parser.addOption(quality_opt);
  parser.addOption(renderer_self_test_opt);
  parser.addOption(release_self_test_opt);
  parser.addOption(print_data_paths_opt);
  parser.addOption(graphics_preset_opt);
  parser.addOption(campaign_mission_opt);
  parser.addOption(mission_file_opt);
  parser.addOption(observe_opt);
  parser.addOption(record_replay_opt);
  parser.addOption(replay_opt);
  parser.addOption(replay_verify_opt);
  parser.addOption(skip_briefing_opt);
  parser.addOption(game_speed_opt);
  parser.addOption(component_gallery_opt);
  parser.addOption(screenshot_opt);
  parser.addOption(screenshot_view_opt);
  parser.addOption(screenshot_delay_opt);
  parser.addOption(screenshot_size_opt);
  parser.addOption(benchmark_seconds_opt);
  parser.addOption(benchmark_output_opt);
  parser.addOption(action_fixture_opt);
  parser.addOption(film_opt);
  parser.addOption(film_fps_opt);
  parser.addOption(film_seconds_opt);
  parser.addOption(film_start_opt);
  parser.addOption(film_size_opt);
  parser.addOption(film_visible_opt);
  QCommandLineOption const film_cursor_opt(
      "film-cursor",
      "Draw a mouse pointer that travels to each scripted click in the footage.");
  parser.addOption(film_cursor_opt);
  QCommandLineOption const film_audio_opt(
      "film-audio",
      "Mix the game's own sound offline in step with the film and write it to "
      "audio.wav beside the frames (no sound reaches the speakers).");
  parser.addOption(film_audio_opt);
  parser.process(app);

  opts.component_gallery_requested = parser.isSet(component_gallery_opt);
  if (parser.isSet(screenshot_opt)) {
    opts.screenshot_path = parser.value(screenshot_opt).trimmed();
    opts.screenshot_view = parser.value(screenshot_view_opt).trimmed().toLower();
    bool delay_ok = false;
    const int parsed_delay = parser.value(screenshot_delay_opt).toInt(&delay_ok);
    opts.screenshot_delay_ms = (delay_ok && parsed_delay >= 0) ? parsed_delay : 1200;
    const QStringList size = parser.value(screenshot_size_opt).split(QLatin1Char('x'));
    if (size.size() == 2) {
      opts.screenshot_size = QSize(std::clamp(size[0].toInt(), 320, 7680),
                                   std::clamp(size[1].toInt(), 240, 4320));
    }
  }

  if (parser.isSet(graphics_preset_opt)) {
    const QString preset = parser.value(graphics_preset_opt).trimmed().toLower();
    auto& gfx = Render::GraphicsSettings::instance();
    if (preset == QStringLiteral("low")) {
      gfx.set_quality(Render::GraphicsQuality::Low);
    } else if (preset == QStringLiteral("medium")) {
      gfx.set_quality(Render::GraphicsQuality::Medium);
    } else if (preset == QStringLiteral("high")) {
      gfx.set_quality(Render::GraphicsQuality::High);
    } else if (preset == QStringLiteral("ultra")) {
      gfx.set_quality(Render::GraphicsQuality::Ultra);
    } else {
      qWarning() << "Unknown --graphics-preset value:" << preset;
    }
  }

  opts.direct_campaign_mission = parser.value(campaign_mission_opt).trimmed();
  opts.direct_mission_file = parser.value(mission_file_opt).trimmed();
  opts.observe_map_file = parser.value(observe_opt).trimmed();
  opts.record_replay_path = parser.value(record_replay_opt).trimmed();
  opts.replay_path = parser.value(replay_opt).trimmed();
  opts.replay_verify = parser.isSet(replay_verify_opt);
  opts.skip_briefing = parser.isSet(skip_briefing_opt) || opts.replay_verify ||
                       !opts.observe_map_file.isEmpty();
  if (parser.isSet(game_speed_opt)) {
    bool speed_ok = false;
    const float requested = parser.value(game_speed_opt).toFloat(&speed_ok);
    if (!speed_ok) {
      qWarning() << "Ignoring unreadable --game-speed value:"
                 << parser.value(game_speed_opt);
    } else {
      opts.direct_game_speed = App::Core::GameSpeed::sanitize(requested);
      if (!qFuzzyCompare(opts.direct_game_speed, requested)) {
        qWarning() << "--game-speed" << requested << "is not offered; using"
                   << opts.direct_game_speed;
      }
    }
  }
  if (release_self_test) {

    opts.direct_campaign_mission.clear();
    opts.direct_mission_file =
        QStringLiteral(":/assets/missions/iron_sepulcher_watch.json");
  }

  bool benchmark_seconds_valid = false;
  opts.runtime_benchmark_seconds =
      parser.value(benchmark_seconds_opt).toDouble(&benchmark_seconds_valid);
  if (!benchmark_seconds_valid || opts.runtime_benchmark_seconds < 0.0) {
    opts.runtime_benchmark_seconds = 0.0;
  }
  opts.runtime_benchmark_output = parser.value(benchmark_output_opt).trimmed();
  if (parser.isSet(film_opt)) {
    App::Core::FilmConfig config;
    config.directory = parser.value(film_opt).trimmed();
    config.fps = std::clamp(parser.value(film_fps_opt).toInt(), 1, 240);
    config.seconds = std::max(0.1, parser.value(film_seconds_opt).toDouble());
    config.start_seconds = std::max(0.0, parser.value(film_start_opt).toDouble());
    const QStringList size = parser.value(film_size_opt).split(QLatin1Char('x'));
    if (size.size() == 2) {
      config.width = std::clamp(size[0].toInt(), 320, 7680);
      config.height = std::clamp(size[1].toInt(), 240, 4320);
    }
    config.background = !parser.isSet(film_visible_opt);
    config.draw_cursor = parser.isSet(film_cursor_opt);
    config.audio = parser.isSet(film_audio_opt);
    if (config.audio) {
      // Before the audio system starts: the mixer opens no device and is
      // pulled by the film instead.
      qputenv("SOI_AUDIO_OFFLINE", "1");
    }
    if (config.directory.isEmpty()) {
      qCritical() << "--film needs a directory";
      return 2;
    }
    qputenv("SOI_FILM_FPS", QByteArray::number(config.fps));

    qputenv("QSG_RENDER_LOOP", "basic");

    qputenv("QSG_FIXED_ANIMATION_STEP", "1");
    opts.film_config = config;
  }
  if (opts.runtime_benchmark_seconds > 0.0) {
    qputenv("SOI_RUNTIME_BENCHMARK_SECONDS",
            QByteArray::number(opts.runtime_benchmark_seconds, 'f', 3));
    if (!opts.runtime_benchmark_output.isEmpty()) {
      qputenv("SOI_RUNTIME_BENCHMARK_OUTPUT", opts.runtime_benchmark_output.toUtf8());
    }
  }

  opts.runtime_action_fixture_path = parser.value(action_fixture_opt).trimmed();
  if (!opts.runtime_action_fixture_path.isEmpty()) {
    QString fixture_error;
    opts.runtime_action_fixture = App::Core::load_benchmark_action_fixture(
        opts.runtime_action_fixture_path, &fixture_error);
    if (!opts.runtime_action_fixture.has_value()) {
      qCritical().noquote() << "Invalid --action-fixture:" << fixture_error;
      return 2;
    }
    qputenv("SOI_ACTION_FIXTURE_NAME", opts.runtime_action_fixture->name.toUtf8());
    qputenv("SOI_ACTION_FIXTURE_PATH", opts.runtime_action_fixture_path.toUtf8());
    qputenv(
        "SOI_ACTION_FIXTURE_REQUIRED",
        opts.runtime_action_fixture->required_coverage.join(QLatin1Char(',')).toUtf8());
  }

  std::optional<Render::ShaderQuality> requested;
  if (parser.isSet(quality_opt)) {
    const QString v = parser.value(quality_opt).trimmed().toLower();
    if (v == "full") {
      requested = Render::ShaderQuality::Full;
    } else if (v == "reduced") {
      requested = Render::ShaderQuality::Reduced;
    } else if (v == "minimal") {
      requested = Render::ShaderQuality::Minimal;
    } else if (v == "none" || v == "software") {
      requested = Render::ShaderQuality::None;
    } else {
      qWarning() << "Unknown --quality value:" << v
                 << "(expected full|reduced|minimal|none)";
    }
  }
  if (parser.isSet(force_software_opt)) {
    requested = Render::ShaderQuality::None;
  }
  if (requested.has_value()) {
    auto& gfx = Render::GraphicsSettings::instance();

    switch (*requested) {
    case Render::ShaderQuality::None:
      qInfo() << "[CLI] shader_quality = None (software backend)";
      break;
    case Render::ShaderQuality::Minimal:
      gfx.set_quality(Render::GraphicsQuality::Low);
      qInfo() << "[CLI] shader_quality = Minimal";
      break;
    case Render::ShaderQuality::Reduced:
      gfx.set_quality(Render::GraphicsQuality::Medium);
      qInfo() << "[CLI] shader_quality = Reduced";
      break;
    case Render::ShaderQuality::Full:
      gfx.set_quality(Render::GraphicsQuality::High);
      qInfo() << "[CLI] shader_quality = Full";
      break;
    }

    gfx.set_backend_kind(*requested);
  }
  return -1;
}

} // namespace App::Bootstrap
