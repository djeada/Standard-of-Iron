#include "arena_batch_runner.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSysInfo>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

#include "arena_scenario.h"
#include "arena_viewport.h"
#include "game/map/campaign_loader.h"
#include "game/map/mission_loader.h"
#include "game/map/terrain_topology_audit.h"
#include "render/gl/bootstrap.h"
#include "utils/resource_utils.h"

namespace Arena::Batch {

namespace {

[[nodiscard]] auto host_cpu_model() -> QString {

  QFile info(QStringLiteral("/proc/cpuinfo"));
  if (!info.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QSysInfo::currentCpuArchitecture();
  }
  const QString contents = QString::fromUtf8(info.readAll());
  for (auto const& line : contents.split(QLatin1Char('\n'))) {
    if (!line.startsWith(QStringLiteral("model name"))) {
      continue;
    }
    int const separator = line.indexOf(QLatin1Char(':'));
    if (separator >= 0) {
      return line.mid(separator + 1).trimmed();
    }
  }
  return QSysInfo::currentCpuArchitecture();
}

[[nodiscard]] auto reference_hardware(const ArenaViewport* viewport) -> QJsonObject {
  auto const& adapter = Render::GL::RenderBootstrap::adapter();
  QJsonObject hardware{
      {QStringLiteral("cpu"), host_cpu_model()},
      {QStringLiteral("gpu_vendor"),
       adapter.vendor.isEmpty() ? QStringLiteral("unknown") : adapter.vendor},
      {QStringLiteral("gpu_renderer"),
       adapter.renderer.isEmpty() ? QStringLiteral("unknown") : adapter.renderer},
      {QStringLiteral("gl_version"),
       adapter.version.isEmpty() ? QStringLiteral("unknown") : adapter.version},
      {QStringLiteral("os"), QSysInfo::prettyProductName()},
      {QStringLiteral("kernel"), QSysInfo::kernelVersion()}};
  if (viewport != nullptr) {
    hardware.insert(QStringLiteral("viewport_width"),
                    static_cast<int>(std::lround(viewport->width())));
    hardware.insert(QStringLiteral("viewport_height"),
                    static_cast<int>(std::lround(viewport->height())));
  }
  return hardware;
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

auto vector_json(const QVector3D& value) -> QJsonArray {
  return QJsonArray{value.x(), value.y(), value.z()};
}

auto build_run_config(const QString& scenario_id,
                      const ScenarioBatchSettings& settings,
                      const ArenaViewport* viewport) -> QJsonObject {
  auto const* scenario = Arena::Scenarios::find_definition(scenario_id);
  const float effective_hour =
      (scenario != nullptr && !settings.environment_hour_forced)
          ? scenario->environment.start_time
          : settings.environment_hour;
  const QString effective_profile = scenario != nullptr
                                        ? scenario->environment.lighting_profile
                                        : settings.lighting_profile;
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
  return QJsonObject{
      {QStringLiteral("scenario"), scenario_id},
      {QStringLiteral("graphics_quality"),
       settings.graphics_quality_override.has_value()
           ? graphics_quality_name(*settings.graphics_quality_override)
           : (scenario != nullptr ? graphics_quality_name(scenario->graphics_quality)
                                  : QStringLiteral("Unknown"))},
      {QStringLiteral("seed"), settings.seed},
      {QStringLiteral("time_of_day"),
       QString::fromLatin1(Game::Map::time_of_day_name(
           Game::Map::time_of_day_for_hour(effective_hour)))},
      {QStringLiteral("representative_clock_time"),
       QString::number(effective_hour, 'f', 2)},
      {QStringLiteral("lighting_profile"), effective_profile},
      {QStringLiteral("primary_direction"), vector_json(lighting.primary_direction)},
      {QStringLiteral("primary_color"), vector_json(lighting.primary_color)},
      {QStringLiteral("primary_intensity"), lighting.primary_intensity},
      {QStringLiteral("sky_color"), vector_json(lighting.sky_color)},
      {QStringLiteral("ambient_intensity"), lighting.ambient_intensity},
      {QStringLiteral("fog_density"), lighting.fog_density},
      {QStringLiteral("shadow_strength"), lighting.shadow_strength},
      {QStringLiteral("shadow_softness"), lighting.shadow_softness},
      {QStringLiteral("exposure"), lighting.exposure},
      {QStringLiteral("cloud_cover"), lighting.cloud_cover},
      {QStringLiteral("wetness"), lighting.wetness},
      {QStringLiteral("fixed_fps"), settings.fps},
      {QStringLiteral("duration_override"), settings.duration},
      {QStringLiteral("capture_interval_seconds"), settings.capture_interval},
      {QStringLiteral("detailed_profiling"), settings.detailed_profiling},
      {QStringLiteral("watchdog_multiplier"), settings.watchdog_multiplier},
      {QStringLiteral("renderer"), QStringLiteral("ArenaViewport/OpenGL")},
      {QStringLiteral("reference_hardware"), reference_hardware(viewport)}};
}

enum class ScenarioOutcome {
  Passed,
  Failed,
  TimedOut
};

auto outcome_name(ScenarioOutcome outcome) -> QString {
  switch (outcome) {
  case ScenarioOutcome::Passed:
    return QStringLiteral("passed");
  case ScenarioOutcome::Failed:
    return QStringLiteral("failed");
  case ScenarioOutcome::TimedOut:
    return QStringLiteral("timed_out");
  }
  return QStringLiteral("failed");
}

struct ScenarioResult {
  QString id;
  ScenarioOutcome outcome{ScenarioOutcome::Failed};
  QString summary;
  QString directory;
};

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
  std::vector<ScenarioResult> results;
};

void write_batch_summary(const BatchState& state, bool complete) {
  QJsonArray results;
  int passed = 0;
  int failed = 0;
  int timed_out = 0;
  for (const auto& result : state.results) {
    switch (result.outcome) {
    case ScenarioOutcome::Passed:
      ++passed;
      break;
    case ScenarioOutcome::Failed:
      ++failed;
      break;
    case ScenarioOutcome::TimedOut:
      ++timed_out;
      break;
    }
    results.push_back(QJsonObject{
        {QStringLiteral("id"), result.id},
        {QStringLiteral("status"), outcome_name(result.outcome)},
        {QStringLiteral("summary"), result.summary},
        {QStringLiteral("artifacts"), QDir(result.directory).absolutePath()}});
  }
  QJsonObject const summary{
      {QStringLiteral("complete"), complete},
      {QStringLiteral("selected"), static_cast<qint64>(state.scenarios.size())},
      {QStringLiteral("finished"), static_cast<qint64>(state.results.size())},
      {QStringLiteral("passed"), passed},
      {QStringLiteral("failed"), failed},
      {QStringLiteral("timed_out"), timed_out},
      {QStringLiteral("artifact_root"), QDir(state.artifact_root).absolutePath()},
      {QStringLiteral("results"), results}};
  if (!QDir().mkpath(state.artifact_root)) {
    return;
  }
  QFile file(QDir(state.artifact_root).filePath(QStringLiteral("batch_summary.json")));
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    file.write(QJsonDocument(summary).toJson(QJsonDocument::Indented));
  }
}

void record_result(BatchState& state, ScenarioOutcome outcome, const QString& summary) {
  if (outcome != ScenarioOutcome::Passed) {
    ++state.failed;
  }
  state.results.push_back(
      {state.current_scenario, outcome, summary, state.current_directory});
  write_batch_summary(state, false);
}

void report_batch_failures(const BatchState& state) {
  for (const auto& result : state.results) {
    if (result.outcome == ScenarioOutcome::Passed) {
      continue;
    }
    qWarning().noquote() << QStringLiteral("  %1 %2").arg(
        result.outcome == ScenarioOutcome::TimedOut ? QStringLiteral("TIMED OUT")
                                                    : QStringLiteral("FAILED   "),
        result.id);
  }
}

} // namespace

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

void start_terrain_review(ArenaViewport& viewport_ref,
                          std::vector<TerrainReviewEntry> entries,
                          const TerrainReviewSettings& settings) {
  struct TerrainReviewState {
    std::vector<TerrainReviewEntry> entries;
    std::size_t next_index{0};
    int failed{0};
    QString artifact_root;
  };
  auto state = std::make_shared<TerrainReviewState>();
  state->entries = std::move(entries);
  state->artifact_root = settings.artifact_root;

  auto* viewport = &viewport_ref;
  viewport->set_batch_fixed_step(1.0F / static_cast<float>(settings.fps));
  auto start_next = std::make_shared<std::function<void()>>();
  *start_next = [state, viewport, start_next, settings]() {
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
    if (settings.forced_time_of_day.has_value()) {
      viewport->set_time_of_day(*settings.forced_time_of_day);
    }
    qInfo().noquote() << QStringLiteral("Reviewing campaign terrain: %1").arg(entry.id);

    if (settings.capture_interval > 0.0F) {
      viewport->set_terrain_review_gameplay_camera();
      viewport->arm_terrain_review_orbit(settings.promo_distance_scale,
                                         settings.promo_tilt_deg);

      const int interval_ms = std::max(
          1, static_cast<int>(std::lround(settings.capture_interval * 1000.0F)));
      const float shot_seconds = settings.duration > 0.0F ? settings.duration : 5.0F;
      const int frame_target = std::max(
          1, static_cast<int>(std::lround(shot_seconds / settings.capture_interval)));
      auto captured = std::make_shared<int>(0);
      auto capture_next = std::make_shared<std::function<void()>>();
      *capture_next = [viewport,
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
                !gameplay.isNull() &&
                gameplay.save(QDir(directory).filePath(QStringLiteral("gameplay.png")));
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
}

void start_scenario_batch(ArenaViewport& viewport_ref,
                          const QStringList& scenario_ids,
                          const ScenarioBatchSettings& settings) {
  auto state = std::make_shared<BatchState>();
  state->artifact_root = settings.artifact_root;
  state->scenarios = scenario_ids;

  auto* viewport = &viewport_ref;
  viewport->set_terrain_seed(settings.seed);
  viewport->set_batch_fixed_step(1.0F / static_cast<float>(settings.fps));
  viewport->set_scenario_duration_override(settings.duration);

  QObject::connect(
      viewport,
      &ArenaViewport::scenario_issue_detected,
      viewport,
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
  *start_next = [state, viewport, start_next, settings]() {
    if (state->next_index >= state->scenarios.size()) {
      write_batch_summary(*state, true);
      qInfo().noquote()
          << QStringLiteral(
                 "Arena batch complete: %1 scenario(s), %2 failed; artifacts: %3")
                 .arg(state->scenarios.size())
                 .arg(state->failed)
                 .arg(QDir(state->artifact_root).absolutePath());
      if (state->failed > 0) {
        qWarning().noquote()
            << QStringLiteral("Arena batch failures (%1):").arg(state->failed);
        report_batch_failures(*state);
      }
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
      QString const message =
          QStringLiteral("Could not replace stale Arena artifacts for %1: %2")
              .arg(id, state->current_directory);
      qCritical().noquote() << message;
      record_result(*state, ScenarioOutcome::Failed, message);
      QTimer::singleShot(25, [start_next]() { (*start_next)(); });
      return;
    }
    if (!QDir().mkpath(state->current_directory)) {
      QString const message =
          QStringLiteral("Could not create Arena artifact directory for %1: %2")
              .arg(id, state->current_directory);
      qCritical().noquote() << message;
      record_result(*state, ScenarioOutcome::Failed, message);
      QTimer::singleShot(25, [start_next]() { (*start_next)(); });
      return;
    }
    QFile config_file(
        QDir(state->current_directory).filePath(QStringLiteral("run_config.json")));
    if (config_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      config_file.write(QJsonDocument(build_run_config(id, settings, viewport))
                            .toJson(QJsonDocument::Indented));
    }
    qInfo().noquote() << QStringLiteral("Running rendered Arena scenario: %1").arg(id);
    viewport->load_scenario(id);

    if (settings.capture_interval > 0.0F) {
      int const capture_interval_ms = std::max(
          1, static_cast<int>(std::lround(settings.capture_interval * 1000.0F)));
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
        settings.duration > 0.0F
            ? settings.duration
            : (definition != nullptr ? definition->duration_seconds : 12.0F);
    int const watchdog_ms = static_cast<int>(
        std::max(15.0F, effective_duration * settings.watchdog_multiplier) * 1000.0F);
    QTimer::singleShot(watchdog_ms, [state, viewport, start_next, generation]() {
      if (state->generation != generation || state->finishing) {
        return;
      }
      state->finishing = true;
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
      record_result(*state,
                    ScenarioOutcome::TimedOut,
                    QStringLiteral("Scenario exceeded the local wall-clock watchdog."));
      QTimer::singleShot(25, [start_next]() { (*start_next)(); });
    });
  };

  QObject::connect(
      viewport,
      &ArenaViewport::scenario_finished,
      viewport,
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
        QString recorded_summary = summary;
        if (!viewport->write_scenario_artifacts(state->current_directory, &error)) {
          qCritical().noquote()
              << QStringLiteral("Could not write artifacts for %1: %2")
                     .arg(scenario_id, error);
          passed = false;
          recorded_summary +=
              QStringLiteral(" (could not write artifacts: %1)").arg(error);
        }
        record_result(*state,
                      passed ? ScenarioOutcome::Passed : ScenarioOutcome::Failed,
                      recorded_summary);
        qInfo().noquote() << summary;
        QTimer::singleShot(25, [start_next]() { (*start_next)(); });
      },
      Qt::QueuedConnection);

  QTimer::singleShot(250, [start_next]() { (*start_next)(); });
}

} // namespace Arena::Batch
