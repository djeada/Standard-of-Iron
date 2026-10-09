#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <optional>
#include <vector>

#include "arena_batch_runner.h"
#include "arena_scenarios.h"
#include "arena_viewport.h"
#include "arena_window.h"
#include "battle_script.h"
#include "battle_script_headless.h"
#include "game/core/nav_profile.h"
#include "game/session/session_context.h"
#include "game/systems/default_content.h"
#include "matchup_short.h"
#include "promo_runner.h"
#include "promo_spec.h"
#include "render/gl/context_requirements.h"
#include "render/graphics_settings.h"
#include "render/profiling/frame_profile.h"
#include "ui/theme.h"
#include "ui/widget_shell.h"

namespace {

auto parse_time_of_day(const QString& value) -> std::optional<Game::Map::TimeOfDay> {
  QString const normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("morning")) {
    return Game::Map::TimeOfDay::Morning;
  }
  if (normalized == QStringLiteral("day")) {
    return Game::Map::TimeOfDay::Day;
  }
  if (normalized == QStringLiteral("afternoon")) {
    return Game::Map::TimeOfDay::Afternoon;
  }
  if (normalized == QStringLiteral("night")) {
    return Game::Map::TimeOfDay::Night;
  }
  return std::nullopt;
}

auto parse_graphics_quality(const QString& value)
    -> std::optional<Render::GraphicsQuality> {
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("low")) {
    return Render::GraphicsQuality::Low;
  }
  if (normalized == QStringLiteral("medium")) {
    return Render::GraphicsQuality::Medium;
  }
  if (normalized == QStringLiteral("high")) {
    return Render::GraphicsQuality::High;
  }
  if (normalized == QStringLiteral("ultra")) {
    return Render::GraphicsQuality::Ultra;
  }
  return std::nullopt;
}

} // namespace

auto main(int argc, char** argv) -> int {

  Game::Session::SessionContext session;
  Game::Session::ScopedSession const active_session(session);
  QSurfaceFormat fmt;
#if defined(Q_OS_MACOS)
  constexpr auto gl_version = Render::GL::ContextRequirements::apple_maximum;
#else
  constexpr auto gl_version = Render::GL::ContextRequirements::preferred;
#endif
  fmt.setVersion(gl_version.major, gl_version.minor);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setDepthBufferSize(24);
  fmt.setStencilBufferSize(8);
  if (qEnvironmentVariableIsSet("SOI_GL_DEBUG")) {
    fmt.setOption(QSurfaceFormat::DebugContext);
  }
  QSurfaceFormat::setDefaultFormat(fmt);

  QApplication app(argc, argv);
  QApplication::setApplicationName("Standard of Iron Arena");
  QApplication::setApplicationVersion("1.0");
  UiShell::apply(app);

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Interactive and automated rendered-gameplay Arena"));
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption const batch_option(
      QStringList{QStringLiteral("batch")},
      QStringLiteral("Run scenarios automatically through the real Arena renderer."));
  QCommandLineOption const all_option(
      QStringList{QStringLiteral("all")},
      QStringLiteral("Run every registered Arena scenario."));
  QCommandLineOption const scenario_option(
      QStringList{QStringLiteral("scenario")},
      QStringLiteral("Scenario ids to run: a comma-separated list that may use shell "
                     "wildcards, e.g. 'trailer_*,gate_destroyed_breach'."),
      QStringLiteral("ids"));
  QCommandLineOption const terrain_map_option(
      QStringList{QStringLiteral("terrain-map")},
      QStringLiteral("Load a map as isolated terrain for visual review."),
      QStringLiteral("path"));
  QCommandLineOption const scenario_distance_option(
      QStringList{QStringLiteral("scenario-distance")},
      QStringLiteral("Camera distance multiplier for batch scenario capture."),
      QStringLiteral("scale"),
      QStringLiteral("1.0"));
  QCommandLineOption const scenario_tilt_option(
      QStringList{QStringLiteral("scenario-tilt")},
      QStringLiteral("Camera tilt in degrees for batch scenario capture, replacing "
                     "the scenario's own."),
      QStringLiteral("degrees"));
  QCommandLineOption const scenario_yaw_option(
      QStringList{QStringLiteral("scenario-yaw")},
      QStringLiteral("Degrees added to the scenario's camera yaw for batch capture."),
      QStringLiteral("degrees"),
      QStringLiteral("0"));
  QCommandLineOption const promo_distance_option(
      QStringList{QStringLiteral("promo-distance")},
      QStringLiteral("Camera distance multiplier for campaign promo capture."),
      QStringLiteral("scale"),
      QStringLiteral("1.0"));
  QCommandLineOption const promo_tilt_option(
      QStringList{QStringLiteral("promo-tilt")},
      QStringLiteral("Camera tilt in degrees for campaign promo capture."),
      QStringLiteral("degrees"),
      QStringLiteral("0"));
  QCommandLineOption const campaign_terrain_option(
      QStringList{QStringLiteral("campaign-terrain")},
      QStringLiteral("Review every campaign mission map in campaign order."));
  QCommandLineOption const map_preview_content_option(
      QStringList{QStringLiteral("map-preview-content")},
      QStringLiteral("Include biome scatter, world props, authored buildings, and "
                     "walls in map previews."));
  QCommandLineOption const duration_option(
      QStringList{QStringLiteral("duration")},
      QStringLiteral("Override scenario duration in simulated seconds."),
      QStringLiteral("seconds"),
      QStringLiteral("0"));
  QCommandLineOption const fps_option(
      QStringList{QStringLiteral("fps")},
      QStringLiteral("Fixed simulation/render sampling rate for batch mode."),
      QStringLiteral("fps"),
      QStringLiteral("60"));
  QCommandLineOption const seed_option(
      QStringList{QStringLiteral("seed")},
      QStringLiteral("Deterministic Arena terrain seed."),
      QStringLiteral("seed"),
      QStringLiteral("1337"));
  QCommandLineOption const time_of_day_option(
      QStringList{QStringLiteral("time-of-day")},
      QStringLiteral("Lighting preset: morning, day, afternoon, or night."),
      QStringLiteral("preset"),
      QStringLiteral("day"));
  QCommandLineOption const environment_time_option(
      QStringList{QStringLiteral("time")},
      QStringLiteral("Exact decimal environment hour (0-24); overrides "
                     "--time-of-day and any hour a scenario locks."),
      QStringLiteral("hour"));
  QCommandLineOption const graphics_quality_option(
      QStringList{QStringLiteral("graphics-quality")},
      QStringLiteral("Override the scenario's graphics preset: low, medium, high, "
                     "or ultra."),
      QStringLiteral("preset"));
  QCommandLineOption const lighting_profile_option(
      QStringList{QStringLiteral("lighting-profile")},
      QStringLiteral("Environment lighting profile."),
      QStringLiteral("profile"),
      QStringLiteral("arena_neutral"));
  QCommandLineOption const artifact_option(
      QStringList{QStringLiteral("artifact-dir")},
      QStringLiteral("Directory for reports, JSONL traces, and frame captures."),
      QStringLiteral("directory"),
      QStringLiteral("artifacts/arena"));
  QCommandLineOption const capture_interval_option(
      QStringList{QStringLiteral("capture-interval")},
      QStringLiteral("Seconds between batch frame captures; zero disables them."),
      QStringLiteral("seconds"),
      QStringLiteral("1"));
  QCommandLineOption const capture_orbit_option(
      QStringList{QStringLiteral("capture-orbit")},
      QStringLiteral("Degrees per second to orbit the scenario camera while "
                     "capturing; zero keeps the authored view."),
      QStringLiteral("degrees"),
      QStringLiteral("0"));
  QCommandLineOption const clean_capture_option(
      QStringList{QStringLiteral("clean-capture")},
      QStringLiteral("Hide stats, controls and spawn markers so captured frames "
                     "record only the scene."));
  QCommandLineOption const prewarm_option(
      QStringList{QStringLiteral("prewarm")},
      QStringLiteral("Prewarm unit templates after the scenario loads and then "
                     "forbid render-time baking, matching the campaign path."));
  QCommandLineOption const animation_diagnostics_option(
      QStringList{QStringLiteral("animation-diagnostics")},
      QStringLiteral("Collect per-soldier animation diagnostics (root yaw, pose, "
                     "cull reason) into the trace even for scenarios that "
                     "normally leave them off for speed."));
  QCommandLineOption const profile_option(
      QStringList{QStringLiteral("profile")},
      QStringLiteral("Record detailed renderer phase timings in batch traces."));
  QCommandLineOption const watchdog_multiplier_option(
      QStringList{QStringLiteral("watchdog-multiplier")},
      QStringLiteral("Wall-clock watchdog as a multiple of simulated duration."),
      QStringLiteral("multiplier"),
      QStringLiteral("3"));
  QCommandLineOption const fog_of_war_option(
      QStringList{QStringLiteral("fog-of-war")},
      QStringLiteral("Run the match's fog of war instead of revealing the whole "
                     "arena, so reviews can check remembered terrain and the "
                     "fog over unexplored ground."));
  QCommandLineOption const matchup_option(
      QStringList{QStringLiteral("matchup")},
      QStringLiteral("Record a vertical matchup short, e.g. \"20 swordsman vs 20 "
                     "archer\"; writes a Shorts-shaped clip that closes on the "
                     "battle report."),
      QStringLiteral("a vs b"));
  QCommandLineOption const matchup_seconds_option(
      QStringList{QStringLiteral("matchup-seconds")},
      QStringLiteral("Ceiling on the fighting in a matchup short before the "
                     "report; the short ends earlier if one side is wiped out."),
      QStringLiteral("seconds"),
      QStringLiteral("45"));
  QCommandLineOption const matchup_report_option(
      QStringList{QStringLiteral("matchup-report-seconds")},
      QStringLiteral("Seconds the closing battle report is held on screen."),
      QStringLiteral("seconds"),
      QStringLiteral("4"));
  QCommandLineOption const matchup_preview_option(
      QStringList{QStringLiteral("matchup-preview")},
      QStringLiteral("Render a matchup short at quarter size for a few seconds, "
                     "without audio, to check its framing quickly."));
  QCommandLineOption const list_option(QStringList{QStringLiteral("list-scenarios")},
                                       QStringLiteral("List scenario ids and exit."));
  QCommandLineOption const promo_spec_option(
      QStringList{QStringLiteral("promo-spec")},
      QStringLiteral("Record the cinematic shot list in this promo spec JSON."),
      QStringLiteral("file"));
  QCommandLineOption const promo_out_option(
      QStringList{QStringLiteral("promo-out")},
      QStringLiteral("Directory for recorded promo clips, posters, and manifest."),
      QStringLiteral("directory"),
      QStringLiteral("artifacts/promo"));
  QCommandLineOption const promo_precheck_option(
      QStringList{QStringLiteral("promo-precheck")},
      QStringLiteral("Play every scenario the promo spec names through once without "
                     "rendering first, and write its match timeline."));
  QCommandLineOption const promo_precheck_only_option(
      QStringList{QStringLiteral("promo-precheck-only")},
      QStringLiteral("Only the dry run: write the match timeline and verdict, record "
                     "nothing."));
  QCommandLineOption const battle_script_option(
      QStringList{QStringLiteral("battle-script")},
      QStringLiteral("Compile a battle-script JSON (an order of battle with phases) "
                     "into a scenario and run it; repeat for several files."),
      QStringLiteral("file"));
  QCommandLineOption const battle_script_scale_option(
      QStringList{QStringLiteral("battle-script-scale")},
      QStringLiteral("Replace the script's historical-to-game scale (default: the "
                     "file's own, 0.1 if unset). Counts and map geometry follow."),
      QStringLiteral("scale"));
  QCommandLineOption const battle_script_check_option(
      QStringList{QStringLiteral("battle-script-check")},
      QStringLiteral("Validate the battle scripts, print their order of battle and "
                     "phases, and exit."));
  QCommandLineOption const headless_option(
      QStringList{QStringLiteral("headless")},
      QStringLiteral("Play the battle scripts on the real simulation without a "
                     "window; prints the phase timeline and world digest and "
                     "writes <artifact-dir>/<id>/timeline.json."));
  QCommandLineOption const determinism_check_option(
      QStringList{QStringLiteral("determinism-check")},
      QStringLiteral("With --headless: play each script twice and fail unless "
                     "every world digest and phase event matches."));
  parser.addOptions({battle_script_option,
                     battle_script_scale_option,
                     battle_script_check_option,
                     headless_option,
                     determinism_check_option});
  parser.addOptions({batch_option,
                     all_option,
                     scenario_option,
                     terrain_map_option,
                     campaign_terrain_option,
                     map_preview_content_option,
                     duration_option,
                     fps_option,
                     seed_option,
                     time_of_day_option,
                     environment_time_option,
                     graphics_quality_option,
                     lighting_profile_option,
                     artifact_option,
                     capture_interval_option,
                     clean_capture_option,
                     capture_orbit_option,
                     prewarm_option,
                     animation_diagnostics_option,
                     profile_option,
                     watchdog_multiplier_option,
                     scenario_distance_option,
                     scenario_tilt_option,
                     scenario_yaw_option,
                     promo_distance_option,
                     promo_tilt_option,
                     fog_of_war_option,
                     promo_spec_option,
                     promo_out_option,
                     promo_precheck_option,
                     promo_precheck_only_option,
                     matchup_option,
                     matchup_seconds_option,
                     matchup_report_option,
                     matchup_preview_option,
                     list_option});
  parser.process(app);

  if (parser.isSet(list_option)) {
    QTextStream out(stdout);
    for (auto const& scenario : Arena::Scenarios::definitions()) {
      out << scenario.id << "\t" << scenario.label << "\t" << scenario.description
          << "\n";
    }
    return 0;
  }

  auto const parsed_time_of_day = parse_time_of_day(parser.value(time_of_day_option));
  if (!parsed_time_of_day.has_value()) {
    qCritical().noquote() << QStringLiteral(
        "Invalid --time-of-day value; expected morning, day, "
        "afternoon, or night");
    return 2;
  }

  const bool time_of_day_forced = parser.isSet(time_of_day_option);
  const bool environment_hour_forced = parser.isSet(environment_time_option);
  const auto forced_time_of_day = *parsed_time_of_day;
  float environment_hour = Game::Map::hour_for_time_of_day(*parsed_time_of_day);
  if (parser.isSet(environment_time_option)) {
    bool valid_time = false;
    environment_hour = parser.value(environment_time_option).toFloat(&valid_time);
    if (!valid_time || environment_hour < 0.0F || environment_hour >= 24.0F) {
      qCritical() << "Invalid --time value; expected a decimal hour in [0, 24)";
      return 2;
    }
  }
  std::optional<Render::GraphicsQuality> graphics_quality_override;
  if (parser.isSet(graphics_quality_option)) {
    graphics_quality_override =
        parse_graphics_quality(parser.value(graphics_quality_option));
    if (!graphics_quality_override.has_value()) {
      qCritical().noquote() << QStringLiteral(
          "Invalid --graphics-quality value; expected low, medium, high, or ultra");
      return 2;
    }
  }

  const QString lighting_profile = parser.value(lighting_profile_option).trimmed();

  Arena::BattleScript::LoadOptions battle_options;
  if (parser.isSet(battle_script_scale_option)) {
    bool scale_ok = false;
    battle_options.scale_override =
        parser.value(battle_script_scale_option).toFloat(&scale_ok);
    if (!scale_ok) {
      qCritical() << "Invalid --battle-script-scale value";
      return 2;
    }
  }
  const QStringList battle_scripts = parser.values(battle_script_option);
  QStringList battle_script_ids;
  const auto register_battle_scripts = [&]() -> bool {
    for (const QString& script : battle_scripts) {
      QString error;
      const auto id =
          Arena::BattleScript::register_file(script, battle_options, &error);
      if (!id.has_value()) {
        qCritical().noquote() << error;
        return false;
      }
      battle_script_ids.push_back(*id);
    }
    return true;
  };

  if (parser.isSet(battle_script_check_option) || parser.isSet(headless_option)) {
    if (battle_scripts.isEmpty()) {
      qCritical() << "--battle-script-check and --headless need --battle-script";
      return 2;
    }
    Game::Systems::initialize_default_content(session.nations());
    if (parser.isSet(battle_script_check_option)) {
      bool all_ok = true;
      for (const QString& script : battle_scripts) {
        const auto result = Arena::BattleScript::load_file(script, battle_options);
        QTextStream out(stdout);
        if (!result.errors.empty() || !result.warnings.empty()) {
          out << Arena::BattleScript::format_diagnostics(result) << "\n";
        }
        if (result.ok()) {
          out << Arena::BattleScript::summary_text(result) << "\n";
        } else {
          all_ok = false;
        }
      }
      return all_ok ? 0 : 1;
    }
    if (!register_battle_scripts()) {
      return 2;
    }
    Arena::Headless::Options headless;
    headless.duration_override = parser.value(duration_option).toFloat();
    int status = 0;
    for (const QString& id : battle_script_ids) {
      const auto* definition = Arena::Scenarios::find_definition(id);
      std::vector<Arena::Headless::Result> runs;
      const int passes = parser.isSet(determinism_check_option) ? 2 : 1;
      for (int pass = 0; pass < passes; ++pass) {
        runs.push_back(Arena::Headless::run(*definition, headless));
        const auto& run = runs.back();
        if (!run.started) {
          qCritical().noquote()
              << QStringLiteral("Headless run of %1 failed: %2").arg(id, run.error);
          return 1;
        }
        qInfo().noquote()
            << QStringLiteral("Headless %1 pass %2: %3 units, %4 s simulated in %5 s, "
                              "digest %6")
                   .arg(id)
                   .arg(pass + 1)
                   .arg(run.spawned_units)
                   .arg(QString::number(run.elapsed_seconds, 'f', 1))
                   .arg(QString::number(run.wall_seconds, 'f', 1))
                   .arg(run.final_digest, 16, 16, QLatin1Char('0'));
        for (const auto& event : run.events) {
          qInfo().noquote() << QStringLiteral("  %1 s  %2")
                                   .arg(QString::number(event.time_seconds, 'f', 1), 7)
                                   .arg(event.name);
        }
        for (const auto& side : run.report.battle.sides) {
          qInfo().noquote() << QStringLiteral("  side %1: %2 of %3 units standing%4")
                                   .arg(side.label)
                                   .arg(side.living_units)
                                   .arg(side.peak_units)
                                   .arg(side.eliminated_at >= 0.0F
                                            ? QStringLiteral(", broken at %1 s")
                                                  .arg(side.eliminated_at)
                                            : QString());
        }
      }
      const QString directory =
          QDir(QDir::cleanPath(parser.value(artifact_option))).filePath(id);
      QDir().mkpath(directory);
      QFile timeline(QDir(directory).filePath(QStringLiteral("timeline.json")));
      if (timeline.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        timeline.write(Arena::Headless::timeline_json(runs, id));
        qInfo().noquote() << QStringLiteral("  timeline: %1").arg(timeline.fileName());
      }
      if (passes == 2) {
        const auto verdict = Arena::Headless::compare(runs[0], runs[1]);
        qInfo().noquote() << QStringLiteral("Determinism %1: %2")
                                 .arg(verdict.deterministic ? QStringLiteral("PASS")
                                                            : QStringLiteral("FAIL"))
                                 .arg(verdict.detail);
        if (!verdict.deterministic) {
          status = 1;
        }
      }
    }
    return status;
  }

  const bool include_map_preview_content = parser.isSet(map_preview_content_option);
  if (include_map_preview_content && !parser.isSet(terrain_map_option) &&
      !parser.isSet(campaign_terrain_option)) {
    qCritical() << "--map-preview-content requires --terrain-map or "
                   "--campaign-terrain";
    return 2;
  }

  ArenaWindow window(session);
  UiShell::prepare_tool_window(window);
  window.resize(1600, 900);
  window.show();
  window.viewport()->set_time_of_day(*parsed_time_of_day);
  window.viewport()->set_lighting_profile(lighting_profile);

  if (environment_hour_forced || time_of_day_forced) {
    window.viewport()->set_environment_hour_override(environment_hour);
  } else {
    window.viewport()->set_environment_time(environment_hour);
  }
  if (graphics_quality_override.has_value()) {
    window.viewport()->set_graphics_quality_override(*graphics_quality_override);
  }
  window.viewport()->set_terrain_review_content_enabled(include_map_preview_content);
  window.viewport()->set_clean_capture(parser.isSet(clean_capture_option));
  window.viewport()->set_scenario_distance_scale(
      parser.value(scenario_distance_option).toFloat());
  if (parser.isSet(scenario_tilt_option)) {
    window.viewport()->set_scenario_tilt_override(
        parser.value(scenario_tilt_option).toFloat());
  }
  window.viewport()->set_scenario_yaw_offset(
      parser.value(scenario_yaw_option).toFloat());
  window.viewport()->set_prewarm_unit_templates(parser.isSet(prewarm_option));
  window.viewport()->set_force_animation_diagnostics(
      parser.isSet(animation_diagnostics_option));
  window.viewport()->set_capture_orbit_speed(
      parser.value(capture_orbit_option).toFloat());
  window.viewport()->set_fog_of_war_enabled(parser.isSet(fog_of_war_option));

  if (!register_battle_scripts()) {
    return 2;
  }

  if (parser.isSet(matchup_option)) {
    if (parser.isSet(batch_option) || parser.isSet(promo_spec_option) ||
        parser.isSet(campaign_terrain_option) || parser.isSet(terrain_map_option)) {
      qCritical() << "--matchup cannot be combined with --batch, --promo-spec, "
                     "--campaign-terrain, or --terrain-map";
      return 2;
    }
    QString matchup_error;
    auto matchup =
        Arena::Matchup::parse(parser.value(matchup_option).trimmed(), &matchup_error);
    if (!matchup.has_value()) {
      qCritical().noquote() << matchup_error;
      return 2;
    }
    matchup->seed = parser.value(seed_option).toInt();
    matchup->fight_seconds =
        std::clamp(parser.value(matchup_seconds_option).toFloat(), 4.0F, 170.0F);
    matchup->report_seconds =
        std::clamp(parser.value(matchup_report_option).toFloat(), 0.0F, 10.0F);
    matchup->preview = parser.isSet(matchup_preview_option);

    Arena::Scenarios::register_runtime_definition(
        Arena::Matchup::build_scenario(*matchup));
    const auto spec = Arena::Matchup::build_spec(*matchup);

    Arena::Promo::RunOptions matchup_options;
    matchup_options.output_directory =
        QDir(QDir::cleanPath(parser.value(promo_out_option))).filePath(spec.id);
    qInfo().noquote() << QStringLiteral("Recording matchup short: %1")
                             .arg(Arena::Matchup::title(*matchup));
    const int matchup_status =
        Arena::Promo::run(*window.viewport(), spec, matchup_options, &matchup_error);
    if (matchup_status == 2 && !matchup_error.isEmpty()) {
      qCritical().noquote() << matchup_error;
    }
    return matchup_status;
  }

  if (parser.isSet(promo_spec_option)) {
    if (parser.isSet(batch_option) || parser.isSet(campaign_terrain_option) ||
        parser.isSet(terrain_map_option)) {
      qCritical() << "--promo-spec cannot be combined with --batch, "
                     "--campaign-terrain, or --terrain-map";
      return 2;
    }
    QString promo_error;
    const auto spec =
        Arena::Promo::load(parser.value(promo_spec_option).trimmed(), &promo_error);
    if (!spec.has_value()) {
      qCritical().noquote() << promo_error;
      return 2;
    }
    Arena::Promo::RunOptions promo_options;
    promo_options.output_directory =
        QDir(QDir::cleanPath(parser.value(promo_out_option))).filePath(spec->id);
    promo_options.force_precheck = parser.isSet(promo_precheck_option);
    promo_options.precheck_only = parser.isSet(promo_precheck_only_option);
    const int promo_status =
        Arena::Promo::run(*window.viewport(), *spec, promo_options, &promo_error);
    if (promo_status == 2 && !promo_error.isEmpty()) {
      qCritical().noquote() << promo_error;
    }
    return promo_status;
  }

  if (!parser.isSet(batch_option)) {
    if (parser.isSet(campaign_terrain_option)) {
      qCritical() << "--campaign-terrain requires --batch";
      return 2;
    }
    if (parser.isSet(terrain_map_option)) {
      const QString map_path = parser.value(terrain_map_option).trimmed();
      QTimer::singleShot(
          0,
          window.viewport(),
          [viewport = window.viewport(),
           map_path,
           time_of_day_forced,
           forced_time_of_day]() {
            QString error;
            if (!viewport->load_terrain_review_map(map_path, &error)) {
              qCritical().noquote()
                  << QStringLiteral("Could not load terrain review map: %1").arg(error);
              return;
            }
            if (time_of_day_forced) {
              viewport->set_time_of_day(forced_time_of_day);
            }
          });
    } else if (parser.isSet(scenario_option) || !battle_script_ids.isEmpty()) {
      QString selection_error;
      QStringList const selected = Arena::Scenarios::select_definition_ids(
          parser.isSet(scenario_option) ? parser.value(scenario_option)
                                        : battle_script_ids.join(QLatin1Char(',')),
          &selection_error);
      if (selected.isEmpty()) {
        qCritical().noquote() << selection_error;
        return 2;
      }
      if (selected.size() > 1) {
        qWarning().noquote()
            << QStringLiteral("--scenario matched %1 scenarios; the interactive "
                              "Arena loads the first, %2. Add --batch to run them all.")
                   .arg(selected.size())
                   .arg(selected.front());
      }
      QString const scenario_id = selected.front();
      bool seed_ok = false;
      int const seed = parser.value(seed_option).toInt(&seed_ok);
      if (!seed_ok) {
        qCritical() << "Invalid --seed value";
        return 2;
      }
      window.viewport()->set_terrain_seed(seed);
      QTimer::singleShot(
          0, window.viewport(), [viewport = window.viewport(), scenario_id]() {
            viewport->load_scenario(scenario_id);
          });
    }
    return QApplication::exec();
  }

  bool fps_ok = false;
  int const fps = parser.value(fps_option).toInt(&fps_ok);
  bool duration_ok = false;
  float const duration = parser.value(duration_option).toFloat(&duration_ok);
  bool seed_ok = false;
  int const seed = parser.value(seed_option).toInt(&seed_ok);
  float const promo_distance_scale = parser.value(promo_distance_option).toFloat();
  float const promo_tilt_deg = parser.value(promo_tilt_option).toFloat();
  bool capture_interval_ok = false;
  float const capture_interval =
      parser.value(capture_interval_option).toFloat(&capture_interval_ok);
  bool watchdog_multiplier_ok = false;
  float const watchdog_multiplier =
      parser.value(watchdog_multiplier_option).toFloat(&watchdog_multiplier_ok);
  if (!fps_ok || fps < 1 || fps > 240 || !duration_ok || duration < 0.0F || !seed_ok ||
      !capture_interval_ok || capture_interval < 0.0F || !watchdog_multiplier_ok ||
      watchdog_multiplier < 1.0F) {
    qCritical().noquote() << QStringLiteral(
        "Invalid --fps, --duration, --seed, --capture-interval, or "
        "--watchdog-multiplier value");
    return 2;
  }
  bool const detailed_profiling = parser.isSet(profile_option);
  Render::Profiling::global_profile().enabled = detailed_profiling;
  Engine::Core::nav_profile().set_enabled(detailed_profiling);

  const bool review_single_map = parser.isSet(terrain_map_option);
  const bool review_campaign_maps = parser.isSet(campaign_terrain_option);
  if (review_single_map || review_campaign_maps) {
    if (review_single_map && review_campaign_maps) {
      qCritical() << "Use either --terrain-map or --campaign-terrain, not both";
      return 2;
    }

    std::vector<Arena::Batch::TerrainReviewEntry> reviews;
    if (review_campaign_maps) {
      QString error;
      reviews = Arena::Batch::campaign_terrain_review_entries(&error);
      if (reviews.empty()) {
        qCritical().noquote() << QStringLiteral(
                                     "Could not discover campaign terrain maps: %1")
                                     .arg(error);
        return 2;
      }
    } else {
      const QString map_path = Arena::Batch::resolve_terrain_review_path(
          parser.value(terrain_map_option).trimmed());
      reviews.push_back(
          {QFileInfo(map_path).completeBaseName().remove(QStringLiteral("map_")),
           map_path});
    }

    Arena::Batch::TerrainReviewSettings review_settings;
    review_settings.artifact_root = QDir::cleanPath(parser.value(artifact_option));
    review_settings.fps = fps;
    review_settings.duration = duration;
    review_settings.capture_interval = capture_interval;
    review_settings.promo_distance_scale = promo_distance_scale;
    review_settings.promo_tilt_deg = promo_tilt_deg;
    if (time_of_day_forced) {
      review_settings.forced_time_of_day = forced_time_of_day;
    }
    Arena::Batch::start_terrain_review(
        *window.viewport(), std::move(reviews), review_settings);
    return QApplication::exec();
  }

  QStringList scenario_ids;
  if (parser.isSet(all_option)) {
    for (auto const& scenario : Arena::Scenarios::definitions()) {
      scenario_ids.push_back(scenario.id);
    }
  } else {
    QString selection = parser.value(scenario_option).trimmed();
    if (selection.isEmpty() && !battle_script_ids.isEmpty()) {
      selection = battle_script_ids.join(QLatin1Char(','));
    }
    if (selection.isEmpty()) {
      selection =
          QString::fromLatin1(Arena::Scenarios::k_three_swords_vs_two_spears_id);
    }
    QString selection_error;
    scenario_ids = Arena::Scenarios::select_definition_ids(selection, &selection_error);
    if (scenario_ids.isEmpty()) {
      qCritical().noquote() << selection_error;
      return 2;
    }
  }
  if (scenario_ids.isEmpty()) {
    qCritical() << "No Arena scenarios selected";
    return 2;
  }

  Arena::Batch::ScenarioBatchSettings batch_settings;
  batch_settings.artifact_root = QDir::cleanPath(parser.value(artifact_option));
  batch_settings.fps = fps;
  batch_settings.seed = seed;
  batch_settings.duration = duration;
  batch_settings.capture_interval = capture_interval;
  batch_settings.watchdog_multiplier = watchdog_multiplier;
  batch_settings.detailed_profiling = detailed_profiling;
  batch_settings.environment_hour = environment_hour;
  batch_settings.environment_hour_forced = environment_hour_forced;
  batch_settings.lighting_profile = lighting_profile;
  batch_settings.graphics_quality_override = graphics_quality_override;
  Arena::Batch::start_scenario_batch(*window.viewport(), scenario_ids, batch_settings);

  return QApplication::exec();
}
