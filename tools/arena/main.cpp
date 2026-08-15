#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "arena_scenarios.h"
#include "arena_viewport.h"
#include "arena_window.h"
#include "game/map/campaign_loader.h"
#include "game/map/mission_loader.h"
#include "game/map/terrain_topology_audit.h"
#include "game/session/session_context.h"
#include "promo_runner.h"
#include "promo_spec.h"
#include "render/gl/context_requirements.h"
#include "render/graphics_settings.h"
#include "render/profiling/frame_profile.h"
#include "ui/theme.h"
#include "ui/widget_shell.h"
#include "utils/resource_utils.h"

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

auto graphics_quality_name(Render::GraphicsQuality quality) -> QString {
  switch (quality) {
  case Render::GraphicsQuality::Low:
    return QStringLiteral("Low");
  case Render::GraphicsQuality::Medium:
    return QStringLiteral("Medium");
  case Render::GraphicsQuality::High:
    return QStringLiteral("High");
  case Render::GraphicsQuality::Ultra:
    return QStringLiteral("Ultra");
  }
  return QStringLiteral("Unknown");
}

struct TerrainReviewEntry {
  QString id;
  QString map_path;
};

auto resolve_terrain_review_path(const QString& path) -> QString {
  if (path.startsWith(QStringLiteral(":/"))) {
    const QString source_candidate = QDir::current().absoluteFilePath(path.mid(2));
    if (QFileInfo::exists(source_candidate)) {
      return QDir::cleanPath(source_candidate);
    }
  }
  return Utils::Resources::resolve_resource_path(path);
}

auto campaign_terrain_review_entries(QString* error)
    -> std::vector<TerrainReviewEntry> {
  std::vector<TerrainReviewEntry> entries;
  QSet<QString> seen_maps;
  const QString campaign_root =
      resolve_terrain_review_path(QStringLiteral(":/assets/campaigns"));
  QDir const campaign_dir(campaign_root);
  const QStringList campaign_files =
      campaign_dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);

  for (const auto& campaign_file : campaign_files) {
    Game::Campaign::CampaignDefinition campaign;
    QString load_error;
    if (!Game::Campaign::CampaignLoader::load_from_json_file(
            campaign_dir.filePath(campaign_file), campaign, &load_error)) {
      if (error != nullptr) {
        *error = load_error;
      }
      return {};
    }
    std::stable_sort(campaign.missions.begin(),
                     campaign.missions.end(),
                     [](const auto& lhs, const auto& rhs) {
                       return lhs.order_index < rhs.order_index;
                     });
    for (const auto& campaign_mission : campaign.missions) {
      const QString mission_path = resolve_terrain_review_path(
          QStringLiteral(":/assets/missions/%1.json").arg(campaign_mission.mission_id));
      Game::Mission::MissionDefinition mission;
      if (!Game::Mission::MissionLoader::load_from_json_file(
              mission_path, mission, &load_error)) {
        if (error != nullptr) {
          *error = load_error;
        }
        return {};
      }
      const QString map_path = resolve_terrain_review_path(mission.map_path);
      const QString canonical_path = QFileInfo(map_path).canonicalFilePath();
      const QString identity = canonical_path.isEmpty() ? map_path : canonical_path;
      if (identity.isEmpty() || seen_maps.contains(identity)) {
        continue;
      }
      seen_maps.insert(identity);
      entries.push_back({campaign_mission.mission_id, map_path});
    }
  }
  return entries;
}

auto write_terrain_review_report(const QString& directory,
                                 const TerrainReviewEntry& entry,
                                 const Game::Map::MapDefinition& definition,
                                 bool overview_saved,
                                 bool gameplay_saved) -> bool {
  const auto topology = Game::Map::audit_terrain_topology(definition);
  QJsonArray topology_issues;
  for (const auto& issue : topology.issues) {
    topology_issues.push_back(issue);
  }
  QJsonObject const report{
      {QStringLiteral("id"), entry.id},
      {QStringLiteral("map_path"), entry.map_path},
      {QStringLiteral("map_name"), definition.name},
      {QStringLiteral("passed"), overview_saved && gameplay_saved && topology.passed()},
      {QStringLiteral("grid"),
       QJsonObject{{QStringLiteral("width"), definition.grid.width},
                   {QStringLiteral("height"), definition.grid.height},
                   {QStringLiteral("tile_size"), definition.grid.tile_size}}},
      {QStringLiteral("terrain_features"),
       static_cast<qint64>(definition.terrain.size())},
      {QStringLiteral("roads"), static_cast<qint64>(definition.roads.size())},
      {QStringLiteral("rivers"), static_cast<qint64>(definition.rivers.size())},
      {QStringLiteral("lakes"), static_cast<qint64>(definition.lakes.size())},
      {QStringLiteral("bridges"), static_cast<qint64>(definition.bridges.size())},
      {QStringLiteral("topology"),
       QJsonObject{{QStringLiteral("passed"), topology.passed()},
                   {QStringLiteral("road_components"), topology.road_components},
                   {QStringLiteral("river_components"), topology.river_components},
                   {QStringLiteral("invalid_river_endpoints"),
                    topology.invalid_river_endpoints},
                   {QStringLiteral("hills_without_two_approaches"),
                    topology.hills_without_two_approaches},
                   {QStringLiteral("tactically_unanchored_lakes"),
                    topology.tactically_unanchored_lakes},
                   {QStringLiteral("issues"), topology_issues}}},
      {QStringLiteral("overview_saved"), overview_saved},
      {QStringLiteral("gameplay_saved"), gameplay_saved},
      {QStringLiteral("renderer"),
       QStringLiteral("ArenaViewport/OpenGL terrain review")}};
  QFile report_file(QDir(directory).filePath(QStringLiteral("report.json")));
  if (!report_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  const bool report_written =
      report_file.write(QJsonDocument(report).toJson(QJsonDocument::Indented)) >= 0;
  return report_written && topology.passed();
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
  QCommandLineOption const scenario_option(QStringList{QStringLiteral("scenario")},
                                           QStringLiteral("Scenario id to run."),
                                           QStringLiteral("id"));
  QCommandLineOption const terrain_map_option(
      QStringList{QStringLiteral("terrain-map")},
      QStringLiteral("Load a map as isolated terrain for visual review."),
      QStringLiteral("path"));
  QCommandLineOption const scenario_distance_option(
      QStringList{QStringLiteral("scenario-distance")},
      QStringLiteral("Camera distance multiplier for batch scenario capture."),
      QStringLiteral("scale"),
      QStringLiteral("1.0"));
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
  QCommandLineOption const lighting_profile_option(
      QStringList{QStringLiteral("lighting-profile")},
      QStringLiteral("Environment lighting profile."),
      QStringLiteral("profile"),
      QStringLiteral("mediterranean_summer"));
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
                     lighting_profile_option,
                     artifact_option,
                     capture_interval_option,
                     clean_capture_option,
                     capture_orbit_option,
                     prewarm_option,
                     profile_option,
                     watchdog_multiplier_option,
                     scenario_distance_option,
                     promo_distance_option,
                     promo_tilt_option,
                     fog_of_war_option,
                     promo_spec_option,
                     promo_out_option,
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
  const QString lighting_profile = parser.value(lighting_profile_option).trimmed();

  const bool include_map_preview_content = parser.isSet(map_preview_content_option);
  if (include_map_preview_content && !parser.isSet(terrain_map_option) &&
      !parser.isSet(campaign_terrain_option)) {
    qCritical() << "--map-preview-content requires --terrain-map or "
                   "--campaign-terrain";
    return 2;
  }

  ArenaWindow window;
  UiShell::prepare_tool_window(window);
  window.resize(1600, 900);
  window.show();
  window.viewport()->set_time_of_day(*parsed_time_of_day);
  window.viewport()->set_lighting_profile(lighting_profile);

  if (environment_hour_forced) {
    window.viewport()->set_environment_hour_override(environment_hour);
  } else {
    window.viewport()->set_environment_time(environment_hour);
  }
  window.viewport()->set_terrain_review_content_enabled(include_map_preview_content);
  window.viewport()->set_clean_capture(parser.isSet(clean_capture_option));
  window.viewport()->set_scenario_distance_scale(
      parser.value(scenario_distance_option).toFloat());
  window.viewport()->set_prewarm_unit_templates(parser.isSet(prewarm_option));
  window.viewport()->set_capture_orbit_speed(
      parser.value(capture_orbit_option).toFloat());
  window.viewport()->set_fog_of_war_enabled(parser.isSet(fog_of_war_option));

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
    } else if (parser.isSet(scenario_option)) {
      QString const scenario_id = parser.value(scenario_option).trimmed();
      if (Arena::Scenarios::find_definition(scenario_id) == nullptr) {
        qCritical().noquote()
            << QStringLiteral("Unknown Arena scenario '%1'; use --list-scenarios")
                   .arg(scenario_id);
        return 2;
      }
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

  const bool review_single_map = parser.isSet(terrain_map_option);
  const bool review_campaign_maps = parser.isSet(campaign_terrain_option);
  if (review_single_map || review_campaign_maps) {
    if (review_single_map && review_campaign_maps) {
      qCritical() << "Use either --terrain-map or --campaign-terrain, not both";
      return 2;
    }

    std::vector<TerrainReviewEntry> reviews;
    if (review_campaign_maps) {
      QString error;
      reviews = campaign_terrain_review_entries(&error);
      if (reviews.empty()) {
        qCritical().noquote() << QStringLiteral(
                                     "Could not discover campaign terrain maps: %1")
                                     .arg(error);
        return 2;
      }
    } else {
      const QString map_path =
          resolve_terrain_review_path(parser.value(terrain_map_option).trimmed());
      reviews.push_back(
          {QFileInfo(map_path).completeBaseName().remove(QStringLiteral("map_")),
           map_path});
    }

    struct TerrainReviewState {
      std::vector<TerrainReviewEntry> entries;
      std::size_t next_index{0};
      int failed{0};
      QString artifact_root;
    };
    auto state = std::make_shared<TerrainReviewState>();
    state->entries = std::move(reviews);
    state->artifact_root = QDir::cleanPath(parser.value(artifact_option));

    auto* viewport = window.viewport();
    viewport->set_batch_fixed_step(1.0F / static_cast<float>(fps));
    auto start_next = std::make_shared<std::function<void()>>();
    *start_next = [state,
                   viewport,
                   &app,
                   start_next,
                   capture_interval,
                   duration,
                   promo_distance_scale,
                   promo_tilt_deg,
                   time_of_day_forced,
                   forced_time_of_day]() {
      if (state->next_index >= state->entries.size()) {
        qInfo().noquote() << QStringLiteral("Campaign terrain review complete: %1 "
                                            "map(s), %2 failed; artifacts: %3")
                                 .arg(state->entries.size())
                                 .arg(state->failed)
                                 .arg(QDir(state->artifact_root).absolutePath());
        QApplication::exit(state->failed == 0 ? 0 : 1);
        return;
      }

      const TerrainReviewEntry entry = state->entries[state->next_index++];
      const QString directory = QDir(state->artifact_root).filePath(entry.id);
      QDir output_dir(directory);
      if ((output_dir.exists() && !output_dir.removeRecursively()) ||
          !QDir().mkpath(directory)) {
        qCritical().noquote() << QStringLiteral(
                                     "Could not prepare terrain review directory: %1")
                                     .arg(directory);
        ++state->failed;
        QTimer::singleShot(25, [start_next]() { (*start_next)(); });
        return;
      }

      QString error;
      if (!viewport->load_terrain_review_map(entry.map_path, &error)) {
        qCritical().noquote() << QStringLiteral("Terrain review failed to load %1: %2")
                                     .arg(entry.id, error);
        ++state->failed;
        QTimer::singleShot(25, [start_next]() { (*start_next)(); });
        return;
      }
      if (time_of_day_forced) {
        viewport->set_time_of_day(forced_time_of_day);
      }
      qInfo().noquote()
          << QStringLiteral("Reviewing campaign terrain: %1").arg(entry.id);

      if (capture_interval > 0.0F) {
        viewport->set_terrain_review_gameplay_camera();
        viewport->arm_terrain_review_orbit(promo_distance_scale, promo_tilt_deg);

        const int interval_ms =
            std::max(1, static_cast<int>(std::lround(capture_interval * 1000.0F)));
        const float shot_seconds = duration > 0.0F ? duration : 5.0F;
        const int frame_target =
            std::max(1, static_cast<int>(std::lround(shot_seconds / capture_interval)));
        auto captured = std::make_shared<int>(0);
        auto capture_next = std::make_shared<std::function<void()>>();
        *capture_next = [state,
                         viewport,
                         start_next,
                         directory,
                         interval_ms,
                         frame_target,
                         captured,
                         capture_next]() {
          const QImage frame = viewport->grabFramebuffer();
          if (!frame.isNull()) {
            frame.save(QDir(directory).filePath(
                QStringLiteral("frame_%1.png")
                    .arg(++(*captured), 4, 10, QLatin1Char('0'))));
          }
          if (*captured >= frame_target) {
            QTimer::singleShot(40, [start_next]() { (*start_next)(); });
            return;
          }
          QTimer::singleShot(interval_ms, [capture_next]() { (*capture_next)(); });
        };
        QTimer::singleShot(500, [capture_next]() { (*capture_next)(); });
        return;
      }

      viewport->set_terrain_review_overview_camera();

      QTimer::singleShot(450, [state, viewport, start_next, entry, directory]() {
        const QImage overview = viewport->grabFramebuffer();
        const bool overview_saved =
            !overview.isNull() &&
            overview.save(QDir(directory).filePath(QStringLiteral("overview.png")));
        viewport->set_terrain_review_gameplay_camera();
        QTimer::singleShot(
            350, [state, viewport, start_next, entry, directory, overview_saved]() {
              const QImage gameplay = viewport->grabFramebuffer();
              const bool gameplay_saved =
                  !gameplay.isNull() && gameplay.save(QDir(directory).filePath(
                                            QStringLiteral("gameplay.png")));
              if (gameplay_saved) {
                gameplay.save(QDir(directory).filePath(QStringLiteral("final.png")));
              }
              const auto* definition = viewport->terrain_review_definition();
              const bool report_saved =
                  definition != nullptr &&
                  write_terrain_review_report(
                      directory, entry, *definition, overview_saved, gameplay_saved);
              if (!overview_saved || !gameplay_saved || !report_saved) {
                ++state->failed;
                qWarning().noquote()
                    << QStringLiteral("Terrain review acceptance failed for %1")
                           .arg(entry.id);
              }
              QTimer::singleShot(40, [start_next]() { (*start_next)(); });
            });
      });
    };

    QTimer::singleShot(250, [start_next]() { (*start_next)(); });
    return QApplication::exec();
  }

  struct BatchState {
    QStringList scenarios;
    int next_index{0};
    int failed{0};
    QString artifact_root;
    QString current_directory;
    QString current_scenario;
    bool failure_context_started{false};
    bool finishing{false};
    int generation{0};
    int capture_index{0};
  };

  auto state = std::make_shared<BatchState>();
  state->artifact_root = QDir::cleanPath(parser.value(artifact_option));
  if (parser.isSet(all_option)) {
    for (auto const& scenario : Arena::Scenarios::definitions()) {
      state->scenarios.push_back(scenario.id);
    }
  } else {
    QString scenario_id = parser.value(scenario_option).trimmed();
    if (scenario_id.isEmpty()) {
      scenario_id =
          QString::fromLatin1(Arena::Scenarios::k_three_swords_vs_two_spears_id);
    }
    if (Arena::Scenarios::find_definition(scenario_id) == nullptr) {
      qCritical().noquote() << QStringLiteral(
                                   "Unknown Arena scenario '%1'; use --list-scenarios")
                                   .arg(scenario_id);
      return 2;
    }
    state->scenarios.push_back(scenario_id);
  }
  if (state->scenarios.isEmpty()) {
    qCritical() << "No Arena scenarios selected";
    return 2;
  }

  auto* viewport = window.viewport();
  viewport->set_terrain_seed(seed);
  viewport->set_batch_fixed_step(1.0F / static_cast<float>(fps));
  viewport->set_scenario_duration_override(duration);

  QObject::connect(
      viewport,
      &ArenaViewport::scenario_issue_detected,
      &app,
      [state, viewport](const QString& scenario_id, const QString& issue) {
        qWarning().noquote()
            << QStringLiteral("Arena failure [%1]: %2").arg(scenario_id, issue);
        if (state->failure_context_started) {
          return;
        }
        state->failure_context_started = true;
        QImage const current = viewport->grabFramebuffer();
        if (!current.isNull()) {
          current.save(QDir(state->current_directory)
                           .filePath(QStringLiteral("failure_frame.png")));
        }
      },
      Qt::QueuedConnection);

  auto start_next = std::make_shared<std::function<void()>>();
  *start_next = [state,
                 viewport,
                 &app,
                 start_next,
                 fps,
                 seed,
                 duration,
                 capture_interval,
                 detailed_profiling,
                 watchdog_multiplier,
                 environment_hour,
                 environment_hour_forced,
                 lighting_profile]() {
    if (state->next_index >= state->scenarios.size()) {
      qInfo().noquote()
          << QStringLiteral(
                 "Arena batch complete: %1 scenario(s), %2 failed; artifacts: %3")
                 .arg(state->scenarios.size())
                 .arg(state->failed)
                 .arg(QDir(state->artifact_root).absolutePath());
      QApplication::exit(state->failed == 0 ? 0 : 1);
      return;
    }
    QString const id = state->scenarios[state->next_index++];
    state->current_scenario = id;
    int const generation = ++state->generation;
    state->current_directory = QDir(state->artifact_root).filePath(id);
    state->failure_context_started = false;
    state->finishing = false;
    state->capture_index = 0;
    QDir scenario_artifacts(state->current_directory);
    if (scenario_artifacts.exists() && !scenario_artifacts.removeRecursively()) {
      qCritical().noquote() << QStringLiteral(
                                   "Could not replace stale Arena artifacts for %1: %2")
                                   .arg(id, state->current_directory);
      ++state->failed;
      QTimer::singleShot(25, [start_next]() { (*start_next)(); });
      return;
    }
    if (!QDir().mkpath(state->current_directory)) {
      qCritical().noquote()
          << QStringLiteral("Could not create Arena artifact directory for %1: %2")
                 .arg(id, state->current_directory);
      ++state->failed;
      QTimer::singleShot(25, [start_next]() { (*start_next)(); });
      return;
    }
    QFile config_file(
        QDir(state->current_directory).filePath(QStringLiteral("run_config.json")));
    if (config_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      auto const* scenario = Arena::Scenarios::find_definition(id);
      const float effective_hour = (scenario != nullptr && !environment_hour_forced)
                                       ? scenario->environment.start_time
                                       : environment_hour;
      const QString effective_profile = scenario != nullptr
                                            ? scenario->environment.lighting_profile
                                            : lighting_profile;
      const auto effective_weather =
          scenario != nullptr ? scenario->weather : Game::Map::WeatherLightingInput{};
      auto lighting = Game::Map::lighting_for_hour(
          effective_hour, effective_profile, effective_weather);

      if (scenario != nullptr) {
        if (scenario->environment.fog_density_override >= 0.0F) {
          lighting.fog_density = scenario->environment.fog_density_override;
        }
        if (scenario->environment.exposure_override >= 0.0F) {
          lighting.exposure = scenario->environment.exposure_override;
        }
      }
      QJsonObject const config{
          {QStringLiteral("scenario"), id},
          {QStringLiteral("graphics_quality"),
           scenario != nullptr ? graphics_quality_name(scenario->graphics_quality)
                               : QStringLiteral("Unknown")},
          {QStringLiteral("seed"), seed},
          {QStringLiteral("time_of_day"),
           QString::fromLatin1(Game::Map::time_of_day_name(
               Game::Map::time_of_day_for_hour(effective_hour)))},
          {QStringLiteral("representative_clock_time"),
           QString::number(effective_hour, 'f', 2)},
          {QStringLiteral("lighting_profile"), effective_profile},
          {QStringLiteral("primary_direction"),
           QJsonArray{lighting.primary_direction.x(),
                      lighting.primary_direction.y(),
                      lighting.primary_direction.z()}},
          {QStringLiteral("primary_color"),
           QJsonArray{lighting.primary_color.x(),
                      lighting.primary_color.y(),
                      lighting.primary_color.z()}},
          {QStringLiteral("primary_intensity"), lighting.primary_intensity},
          {QStringLiteral("sky_color"),
           QJsonArray{
               lighting.sky_color.x(), lighting.sky_color.y(), lighting.sky_color.z()}},
          {QStringLiteral("ambient_intensity"), lighting.ambient_intensity},
          {QStringLiteral("fog_density"), lighting.fog_density},
          {QStringLiteral("shadow_strength"), lighting.shadow_strength},
          {QStringLiteral("shadow_softness"), lighting.shadow_softness},
          {QStringLiteral("exposure"), lighting.exposure},
          {QStringLiteral("cloud_cover"), lighting.cloud_cover},
          {QStringLiteral("wetness"), lighting.wetness},
          {QStringLiteral("fixed_fps"), fps},
          {QStringLiteral("duration_override"), duration},
          {QStringLiteral("capture_interval_seconds"), capture_interval},
          {QStringLiteral("detailed_profiling"), detailed_profiling},
          {QStringLiteral("watchdog_multiplier"), watchdog_multiplier},
          {QStringLiteral("renderer"), QStringLiteral("ArenaViewport/OpenGL")}};
      config_file.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
    }
    qInfo().noquote() << QStringLiteral("Running rendered Arena scenario: %1").arg(id);
    viewport->load_scenario(id);

    if (capture_interval > 0.0F) {
      int const capture_interval_ms =
          std::max(1, static_cast<int>(std::lround(capture_interval * 1000.0F)));
      auto capture_next = std::make_shared<std::function<void()>>();
      *capture_next =
          [state, viewport, generation, capture_interval_ms, capture_next]() {
            if (state->generation != generation || state->finishing) {
              return;
            }
            QImage const frame = viewport->grabFramebuffer();
            if (!frame.isNull()) {
              frame.save(
                  QDir(state->current_directory)
                      .filePath(
                          QStringLiteral("frame_%1.png")
                              .arg(++state->capture_index, 4, 10, QLatin1Char('0'))));
            }
            QTimer::singleShot(capture_interval_ms,
                               [capture_next]() { (*capture_next)(); });
          };
      QTimer::singleShot(capture_interval_ms, [capture_next]() { (*capture_next)(); });
    }

    auto const* definition = Arena::Scenarios::find_definition(id);
    float const effective_duration =
        duration > 0.0F
            ? duration
            : (definition != nullptr ? definition->duration_seconds : 12.0F);
    int const watchdog_ms = static_cast<int>(
        std::max(15.0F, effective_duration * watchdog_multiplier) * 1000.0F);
    QTimer::singleShot(watchdog_ms, [state, viewport, start_next, generation]() {
      if (state->generation != generation || state->finishing) {
        return;
      }
      state->finishing = true;
      ++state->failed;
      QImage const frame = viewport->grabFramebuffer();
      if (!frame.isNull()) {
        frame.save(
            QDir(state->current_directory).filePath(QStringLiteral("timeout.png")));
      }
      QString ignored_error;
      (void)viewport->write_scenario_artifacts(state->current_directory,
                                               &ignored_error);
      QFile timeout_file(
          QDir(state->current_directory).filePath(QStringLiteral("timeout.txt")));
      if (timeout_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        timeout_file.write("Scenario exceeded the local wall-clock watchdog.\n");
      }
      qCritical().noquote() << QStringLiteral("Arena scenario timed out: %1")
                                   .arg(state->current_scenario);
      QTimer::singleShot(25, [start_next]() { (*start_next)(); });
    });
  };

  QObject::connect(
      viewport,
      &ArenaViewport::scenario_finished,
      &app,
      [state, viewport, start_next](
          const QString& scenario_id, bool passed, const QString& summary) {
        if (state->finishing || scenario_id != state->current_scenario) {
          return;
        }
        state->finishing = true;
        QImage const final_frame = viewport->grabFramebuffer();
        if (!final_frame.isNull()) {
          final_frame.save(
              QDir(state->current_directory).filePath(QStringLiteral("final.png")));
        }
        QString error;
        if (!viewport->write_scenario_artifacts(state->current_directory, &error)) {
          qCritical().noquote()
              << QStringLiteral("Could not write artifacts for %1: %2")
                     .arg(scenario_id, error);
          passed = false;
        }
        if (!passed) {
          ++state->failed;
        }
        qInfo().noquote() << summary;
        QTimer::singleShot(25, [start_next]() { (*start_next)(); });
      },
      Qt::QueuedConnection);

  QTimer::singleShot(250, [start_next]() { (*start_next)(); });

  return QApplication::exec();
}
