#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSurfaceFormat>
#include <QSet>
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
#include "utils/resource_utils.h"

namespace {

constexpr auto k_dark_stylesheet = R"(
QWidget {
    background-color: #071018;
    color: #eaf6ff;
    font-size: 12px;
}
QMainWindow {
    background-color: #071018;
}
QToolBar {
    background-color: #0c1e2a;
    border: none;
    border-bottom: 1px solid #0f2b34;
    spacing: 6px;
    padding: 3px 6px;
}
QToolBar::separator {
    background-color: #0f2b34;
    width: 1px;
    margin: 4px 3px;
}
QToolButton {
    background-color: transparent;
    color: #eaf6ff;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 5px 10px;
    min-width: 60px;
}
QToolButton:hover {
    background-color: #184c7a;
    border-color: #1f8bf5;
}
QToolButton:pressed {
    background-color: #1b74d1;
}
QToolButton:checked {
    background-color: #1054a0;
    border-color: #1f8bf5;
    color: #9fd9ff;
}
QGroupBox {
    background-color: #0c1e2a;
    border: 1px solid #0f2b34;
    border-radius: 6px;
    margin-top: 10px;
    padding-top: 10px;
    font-weight: bold;
    font-size: 12px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6px;
    left: 8px;
    color: #9fd9ff;
}
QPushButton {
    background-color: #0f2430;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    border-radius: 4px;
    padding: 5px 10px;
    min-height: 26px;
}
QPushButton:hover {
    background-color: #184c7a;
    border-color: #1f8bf5;
}
QPushButton:pressed {
    background-color: #1b74d1;
}
QPushButton:disabled {
    background-color: #1a2a32;
    color: #4f6a75;
    border-color: #0f2b34;
}
QPushButton[primary="true"] {
    background-color: #1054a0;
    color: #dff0ff;
    border: 1px solid #1f8bf5;
    font-weight: bold;
}
QPushButton[primary="true"]:hover {
    background-color: #1568c5;
    border-color: #9fd9ff;
    color: #ffffff;
}
QPushButton[primary="true"]:pressed {
    background-color: #1b74d1;
}
QComboBox {
    background-color: #0c1e2a;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    border-radius: 4px;
    padding: 4px 8px;
    min-height: 24px;
}
QComboBox:hover {
    border-color: #1f8bf5;
}
QComboBox:focus {
    border-color: #1f8bf5;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
}
QComboBox QAbstractItemView {
    background-color: #0c1e2a;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    selection-background-color: #1f8bf5;
    selection-color: #eaf6ff;
    outline: none;
}
QSpinBox, QDoubleSpinBox {
    background-color: #0c1e2a;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    border-radius: 4px;
    padding: 4px 8px;
    min-height: 24px;
}
QSpinBox:hover, QDoubleSpinBox:hover {
    border-color: #1f8bf5;
}
QSpinBox:focus, QDoubleSpinBox:focus {
    border-color: #1f8bf5;
}
QSpinBox::up-button, QDoubleSpinBox::up-button,
QSpinBox::down-button, QDoubleSpinBox::down-button {
    background-color: #0f2430;
    border: none;
    width: 18px;
}
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
    background-color: #184c7a;
}
QSlider::groove:horizontal {
    background-color: #0f2430;
    border: none;
    height: 4px;
    border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background-color: #1f8bf5;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background-color: #1f8bf5;
    border: 2px solid #071018;
    width: 14px;
    height: 14px;
    border-radius: 7px;
    margin: -5px 0;
}
QSlider::handle:horizontal:hover {
    background-color: #9fd9ff;
    border-color: #071018;
}
QCheckBox {
    color: #eaf6ff;
    spacing: 6px;
}
QCheckBox::indicator {
    width: 15px;
    height: 15px;
    border: 1px solid #1a4060;
    border-radius: 3px;
    background-color: #0c1e2a;
}
QCheckBox::indicator:checked {
    background-color: #1f8bf5;
    border-color: #1b74d1;
}
QCheckBox::indicator:hover {
    border-color: #1f8bf5;
}
QLabel {
    background-color: transparent;
    color: #eaf6ff;
}
QScrollBar:vertical {
    background-color: #071018;
    width: 8px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background-color: #1a3a4a;
    border-radius: 4px;
    min-height: 20px;
}
QScrollBar::handle:vertical:hover {
    background-color: #1f8bf5;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar:horizontal {
    background-color: #071018;
    height: 8px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background-color: #1a3a4a;
    border-radius: 4px;
    min-width: 20px;
}
QScrollBar::handle:horizontal:hover {
    background-color: #1f8bf5;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}
QScrollArea {
    border: none;
    background-color: transparent;
}
QSplitter::handle {
    background-color: #0f2b34;
}
QSplitter::handle:horizontal {
    width: 4px;
}
QSplitter::handle:hover {
    background-color: #1f8bf5;
}
QTabWidget::pane {
    border: 1px solid #0f2b34;
    background-color: #071018;
}
QTabBar::tab {
    background-color: #0c1e2a;
    color: #86a7b6;
    border: 1px solid #0f2b34;
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    padding: 6px 16px;
    margin-right: 2px;
    min-width: 60px;
}
QTabBar::tab:selected {
    background-color: #071018;
    color: #9fd9ff;
    border-bottom: 2px solid #1f8bf5;
}
QTabBar::tab:hover:!selected {
    background-color: #132030;
    color: #eaf6ff;
}
QStatusBar {
    background-color: #0c1e2a;
    color: #86a7b6;
    border-top: 1px solid #0f2b34;
    padding: 0 4px;
}
QStatusBar::item {
    border: none;
}
)";

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

struct TerrainReviewEntry {
  QString id;
  QString map_path;
};

auto resolve_terrain_review_path(const QString& path) -> QString {
  if (path.startsWith(QStringLiteral(":/"))) {
    const QString source_candidate =
        QDir::current().absoluteFilePath(path.mid(2));
    if (QFileInfo::exists(source_candidate)) {
      return QDir::cleanPath(source_candidate);
    }
  }
  return Utils::Resources::resolve_resource_path(path);
}

auto campaign_terrain_review_entries(QString* error) -> std::vector<TerrainReviewEntry> {
  std::vector<TerrainReviewEntry> entries;
  QSet<QString> seen_maps;
  const QString campaign_root = resolve_terrain_review_path(
      QStringLiteral(":/assets/campaigns"));
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
      {QStringLiteral("passed"),
       overview_saved && gameplay_saved && topology.passed()},
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
      {QStringLiteral("renderer"), QStringLiteral("ArenaViewport/OpenGL terrain review")}};
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
  QSurfaceFormat fmt;
  fmt.setVersion(3, 3);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setDepthBufferSize(24);
  fmt.setStencilBufferSize(8);
  QSurfaceFormat::setDefaultFormat(fmt);

  QApplication app(argc, argv);
  QApplication::setApplicationName("Standard of Iron Arena");
  QApplication::setApplicationVersion("1.0");
  app.setStyleSheet(QString::fromLatin1(k_dark_stylesheet));

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
  QCommandLineOption const list_option(QStringList{QStringLiteral("list-scenarios")},
                                       QStringLiteral("List scenario ids and exit."));
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
                     artifact_option,
                     capture_interval_option,
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

  const bool include_map_preview_content = parser.isSet(map_preview_content_option);
  if (include_map_preview_content && !parser.isSet(terrain_map_option) &&
      !parser.isSet(campaign_terrain_option)) {
    qCritical() << "--map-preview-content requires --terrain-map or "
                   "--campaign-terrain";
    return 2;
  }

  ArenaWindow window;
  window.resize(1600, 900);
  window.show();
  window.viewport()->set_time_of_day(*parsed_time_of_day);
  window.viewport()->set_terrain_review_content_enabled(include_map_preview_content);

  if (!parser.isSet(batch_option)) {
    if (parser.isSet(campaign_terrain_option)) {
      qCritical() << "--campaign-terrain requires --batch";
      return 2;
    }
    if (parser.isSet(terrain_map_option)) {
      const QString map_path = parser.value(terrain_map_option).trimmed();
      QTimer::singleShot(0, window.viewport(), [viewport = window.viewport(), map_path]() {
        QString error;
        if (!viewport->load_terrain_review_map(map_path, &error)) {
          qCritical().noquote()
              << QStringLiteral("Could not load terrain review map: %1").arg(error);
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
  bool capture_interval_ok = false;
  float const capture_interval =
      parser.value(capture_interval_option).toFloat(&capture_interval_ok);
  if (!fps_ok || fps < 1 || fps > 240 || !duration_ok || duration < 0.0F || !seed_ok ||
      !capture_interval_ok || capture_interval < 0.0F) {
    qCritical().noquote() << QStringLiteral(
        "Invalid --fps, --duration, --seed, or --capture-interval value");
    return 2;
  }

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
        qCritical().noquote()
            << QStringLiteral("Could not discover campaign terrain maps: %1").arg(error);
        return 2;
      }
    } else {
      const QString map_path = resolve_terrain_review_path(
          parser.value(terrain_map_option).trimmed());
      reviews.push_back(
          {QFileInfo(map_path).completeBaseName().remove(QStringLiteral("map_")), map_path});
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
    *start_next = [state, viewport, &app, start_next]() {
      if (state->next_index >= state->entries.size()) {
        qInfo().noquote()
            << QStringLiteral(
                   "Campaign terrain review complete: %1 map(s), %2 failed; artifacts: %3")
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
        qCritical().noquote()
            << QStringLiteral("Could not prepare terrain review directory: %1")
                   .arg(directory);
        ++state->failed;
        QTimer::singleShot(25, [start_next]() { (*start_next)(); });
        return;
      }

      QString error;
      if (!viewport->load_terrain_review_map(entry.map_path, &error)) {
        qCritical().noquote()
            << QStringLiteral("Terrain review failed to load %1: %2").arg(entry.id, error);
        ++state->failed;
        QTimer::singleShot(25, [start_next]() { (*start_next)(); });
        return;
      }
      qInfo().noquote()
          << QStringLiteral("Reviewing campaign terrain: %1").arg(entry.id);
      viewport->set_terrain_review_overview_camera();

      QTimer::singleShot(450, [state, viewport, start_next, entry, directory]() {
        const QImage overview = viewport->grabFramebuffer();
        const bool overview_saved =
            !overview.isNull() &&
            overview.save(QDir(directory).filePath(QStringLiteral("overview.png")));
        viewport->set_terrain_review_gameplay_camera();
        QTimer::singleShot(
            350,
            [state, viewport, start_next, entry, directory, overview_saved]() {
              const QImage gameplay = viewport->grabFramebuffer();
              const bool gameplay_saved =
                  !gameplay.isNull() &&
                  gameplay.save(
                      QDir(directory).filePath(QStringLiteral("gameplay.png")));
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
                 parsed_time_of_day]() {
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
      auto const lighting = Game::Map::lighting_for_time_of_day(*parsed_time_of_day);
      QJsonObject const config{
          {QStringLiteral("scenario"), id},
          {QStringLiteral("seed"), seed},
          {QStringLiteral("time_of_day"),
           QString::fromLatin1(Game::Map::time_of_day_name(*parsed_time_of_day))},
          {QStringLiteral("representative_clock_time"),
           QString::fromLatin1(
               Game::Map::representative_clock_time(*parsed_time_of_day))},
          {QStringLiteral("light_direction"),
           QJsonArray{lighting.light_direction.x(),
                      lighting.light_direction.y(),
                      lighting.light_direction.z()}},
          {QStringLiteral("ambient_strength"), lighting.ambient_strength},
          {QStringLiteral("fixed_fps"), fps},
          {QStringLiteral("duration_override"), duration},
          {QStringLiteral("capture_interval_seconds"), capture_interval},
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
    int const watchdog_ms =
        static_cast<int>(std::max(15.0F, effective_duration * 3.0F) * 1000.0F);
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
