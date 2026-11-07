#include "editor_window.h"

#include <QAction>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextBrowser>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QtMath>

#include <cmath>

#include "campaign_workspace.h"
#include "element_edit_json.h"
#include "game/map/environment_lighting.h"
#include "game/map/map_definition.h"
#include "game/strategic/strategic_assets.h"
#include "game/strategic/strategic_encounter.h"
#include "game/strategic/strategic_events.h"
#include "generator_client.h"
#include "generator_panel.h"
#include "json_edit_dialog.h"
#include "json_schema.h"
#include "map_json_keys.h"
#include "resize_dialog.h"
#include "road_network.h"
#include "tool_catalog.h"
#include "world_canvas.h"
#include "world_document.h"

namespace {

constexpr auto k_last_dialog_directory_key = "map_editor/last_dialog_directory";

auto editor_settings() -> QSettings {
  return QSettings(QSettings::IniFormat,
                   QSettings::UserScope,
                   QCoreApplication::organizationName(),
                   QCoreApplication::applicationName());
}

auto createGuideSection(const QString& title,
                        const QString& body,
                        QWidget* parent) -> QGroupBox* {
  auto* group = new QGroupBox(title, parent);
  auto* layout = new QVBoxLayout(group);
  layout->setSpacing(4);

  auto* label = new QLabel(body, group);
  label->setObjectName("panelHint");
  label->setWordWrap(true);
  label->setTextFormat(Qt::PlainText);
  layout->addWidget(label);
  return group;
}

auto createGuidePanel(QWidget* parent) -> QWidget* {
  auto* panel = new QWidget(parent);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto* title = new QLabel("Editor Guide", panel);
  title->setObjectName("panelTitle");
  layout->addWidget(title);

  auto* intro =
      new QLabel("The map editor uses the same dark split-view layout as the other "
                 "Standard "
                 "of Iron tools, with the canvas on the left and editing panels on the "
                 "right. The canvas orientation matches the in-game battlefield view.",
                 panel);
  intro->setObjectName("panelIntro");
  intro->setWordWrap(true);
  intro->setTextFormat(Qt::PlainText);
  layout->addWidget(intro);

  layout->addWidget(
      createGuideSection("Workflow",
                         "1. Pick a tool on the Tools tab.\n"
                         "2. Click on the canvas to place or start drawing.\n"
                         "3. Drag placed items to refine positioning.\n"
                         "4. Double-click elements to edit JSON (hills also show a "
                         "cell grid: draw the hill shape or its entrances).\n"
                         "5. Save the map when the layout looks right.",
                         panel));

  layout->addWidget(createGuideSection(
      "Generator",
      "The Generator tab builds a whole battlefield from a preset and a seed. "
      "Generate opens a preview with a validation report; Accept replaces the "
      "map in one undo step. Lock the stages you want to keep and press Reroll "
      "Unlocked to regenerate the rest. Moving or editing a generated element "
      "makes it authored, so later rerolls leave it alone.",
      panel));

  layout->addWidget(
      createGuideSection("Mouse Controls",
                         "Left click places or selects.\n"
                         "Shift + click an element adds or removes it from the "
                         "selection.\n"
                         "Shift + drag empty space draws a selection box.\n"
                         "Drag any selected element to move the whole selection.\n"
                         "Shift + click or drag places without grid snapping.\n"
                         "Drag empty space in Select mode to pan.\n"
                         "Middle click, Space + drag, or Ctrl + left drag also pans.\n"
                         "Mouse wheel zooms in and out.\n"
                         "Right click opens the context menu (cancels line drawing).\n"
                         "Hover an element for a summary tooltip.\n"
                         "Double-click element to edit its JSON.\n"
                         "Double-click empty canvas opens Resize Map.",
                         panel));

  layout->addWidget(createGuideSection("Keyboard Shortcuts",
                                       "Ctrl+N: New map\n"
                                       "Ctrl+O: Open map\n"
                                       "Ctrl+S: Save\n"
                                       "Ctrl+Shift+S: Save As\n"
                                       "Ctrl+Z / Ctrl+Y: Undo / Redo\n"
                                       "Ctrl+A: Select all on visible layers\n"
                                       "Ctrl+D: Duplicate selected\n"
                                       "Ctrl+C / Ctrl+V: Copy / paste at cursor\n"
                                       "Arrows: Nudge selected one cell\n"
                                       "Shift+Arrows: Nudge by a quarter cell\n"
                                       "Ctrl+0: Zoom to fit    F: Frame selection\n"
                                       "Del / Backspace: Delete selected\n"
                                       "Escape: Cancel or return to Select",
                                       panel));

  layout->addWidget(
      createGuideSection("View",
                         "View ▸ Layers hides categories you are not editing;\n"
                         "hidden layers cannot be selected or deleted by accident.\n"
                         "Markers shrink as you zoom out and labels fade below 60%\n"
                         "so dense maps stay readable.\n"
                         "Hills, mountains and lakes always draw behind everything\n"
                         "else, largest first. Bring to front / send to back change\n"
                         "the view only — they are never saved to the map.",
                         panel));

  layout->addStretch(1);
  return panel;
}

auto createEnvironmentPanel(MapEditor::MapData* map_data, QWidget* parent) -> QWidget* {
  auto* panel = new QWidget(parent);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(10);

  auto* title = new QLabel("Environment Lighting", panel);
  title->setObjectName("panelTitle");
  layout->addWidget(title);

  auto* description =
      new QLabel("Author the battle clock, lighting profile, atmosphere, and weather. "
                 "Named legacy times are imported as exact clock values.",
                 panel);
  description->setObjectName("panelIntro");
  description->setWordWrap(true);
  layout->addWidget(description);

  auto* form_group = new QGroupBox("Time and profile", panel);
  auto* form = new QFormLayout(form_group);

  auto* time = new QDoubleSpinBox(form_group);
  time->setRange(0.0, 23.99);
  time->setDecimals(2);
  time->setSingleStep(0.25);
  time->setSuffix(" h");
  form->addRow("Start time", time);

  auto* mode = new QComboBox(form_group);
  mode->addItem("Locked", "locked");
  mode->addItem("Scripted", "scripted");
  mode->addItem("Continuous", "continuous");
  form->addRow("Time mode", mode);

  auto* day_length = new QDoubleSpinBox(form_group);
  day_length->setRange(1.0, 86400.0);
  day_length->setDecimals(0);
  day_length->setSingleStep(60.0);
  day_length->setSuffix(" s");
  form->addRow("Day length", day_length);

  auto* profile = new QComboBox(form_group);
  profile->setEditable(true);
  profile->addItems({"mediterranean_summer",
                     "delta_haze",
                     "canyon_dry",
                     "pine_overcast",
                     "alpine_clear",
                     "river_mist",
                     "iberian_high_sun",
                     "arena_neutral",
                     "iron_sepulcher"});
  form->addRow("Lighting profile", profile);

  auto* fog = new QDoubleSpinBox(form_group);
  fog->setRange(-1.0, 0.10);
  fog->setDecimals(4);
  fog->setSingleStep(0.001);
  fog->setSpecialValueText("Profile");
  form->addRow("Fog density", fog);

  auto* exposure = new QDoubleSpinBox(form_group);
  exposure->setRange(-1.0, 2.5);
  exposure->setDecimals(2);
  exposure->setSingleStep(0.05);
  exposure->setSpecialValueText("Profile");
  form->addRow("Exposure", exposure);
  layout->addWidget(form_group);

  auto* weather_group = new QGroupBox("Weather preview", panel);
  auto* weather_form = new QFormLayout(weather_group);
  auto* weather = new QComboBox(weather_group);
  weather->addItem("Off", "off");
  weather->addItem("Rain", "rain");
  weather->addItem("Snow", "snow");
  weather_form->addRow("Weather", weather);
  auto* weather_intensity = new QDoubleSpinBox(weather_group);
  weather_intensity->setRange(0.0, 1.0);
  weather_intensity->setDecimals(2);
  weather_intensity->setSingleStep(0.05);
  weather_form->addRow("Intensity", weather_intensity);
  auto* wind_strength = new QDoubleSpinBox(weather_group);
  wind_strength->setRange(0.0, 3.0);
  wind_strength->setDecimals(2);
  wind_strength->setSingleStep(0.05);
  weather_form->addRow("Wind strength", wind_strength);
  auto* wind_direction = new QDoubleSpinBox(weather_group);
  wind_direction->setRange(0.0, 359.0);
  wind_direction->setDecimals(0);
  wind_direction->setSingleStep(15.0);
  wind_direction->setSuffix("°");
  wind_direction->setToolTip(
      "Compass bearing the wind blows towards; drives both particles and drift.");
  weather_form->addRow("Wind direction", wind_direction);
  auto* shadow_quality = new QComboBox(weather_group);
  shadow_quality->addItems({"Low", "Medium", "High", "Ultra"});
  shadow_quality->setCurrentText("High");
  shadow_quality->setToolTip(
      "Preview choice for Arena; runtime quality remains a player graphics setting.");
  weather_form->addRow("Shadow preview", shadow_quality);
  layout->addWidget(weather_group);

  auto* sun = new QLabel(panel);
  sun->setObjectName("panelHint");
  sun->setWordWrap(true);
  layout->addWidget(sun);
  layout->addStretch(1);

  const auto refresh = [=]() {
    const QJsonObject environment = map_data->environment();
    const QSignalBlocker block_time(time);
    const QSignalBlocker block_mode(mode);
    const QSignalBlocker block_day_length(day_length);
    const QSignalBlocker block_profile(profile);
    const QSignalBlocker block_fog(fog);
    const QSignalBlocker block_exposure(exposure);
    const QSignalBlocker block_weather(weather);
    const QSignalBlocker block_weather_intensity(weather_intensity);

    const QSignalBlocker block_wind_strength(wind_strength);
    const QSignalBlocker block_wind_direction(wind_direction);

    const double hour =
        environment.value(MapEditor::MapJsonKeys::start_time).toDouble(13.0);
    time->setValue(hour);
    const QString mode_value =
        environment.value(MapEditor::MapJsonKeys::time_mode).toString("locked");
    mode->setCurrentIndex(std::max(0, mode->findData(mode_value)));
    day_length->setValue(
        environment.value(MapEditor::MapJsonKeys::day_length_seconds).toDouble(1800.0));
    profile->setCurrentText(environment.value(MapEditor::MapJsonKeys::lighting_profile)
                                .toString("mediterranean_summer"));
    fog->setValue(
        environment.contains(MapEditor::MapJsonKeys::fog_density)
            ? environment.value(MapEditor::MapJsonKeys::fog_density).toDouble()
            : -1.0);
    exposure->setValue(
        environment.contains(MapEditor::MapJsonKeys::exposure)
            ? environment.value(MapEditor::MapJsonKeys::exposure).toDouble()
            : -1.0);

    const QJsonObject rain = map_data->rain();
    const bool enabled = rain.value("enabled").toBool(false);
    const QString weather_value =
        enabled ? rain.value("type").toString("rain") : QStringLiteral("off");
    weather->setCurrentIndex(std::max(0, weather->findData(weather_value)));
    const QJsonValue intensity_value = rain.value("intensity");
    weather_intensity->setValue(
        intensity_value.isString()
            ? static_cast<double>(
                  Game::Map::parse_weather_intensity(intensity_value.toString(), 0.5F))
            : intensity_value.toDouble(0.5));
    weather_intensity->setEnabled(enabled);
    wind_strength->setValue(rain.value("wind_strength").toDouble(0.0));
    wind_strength->setEnabled(enabled);
    wind_direction->setValue(rain.value("wind_direction").toDouble(45.0));
    wind_direction->setEnabled(enabled);

    const auto lighting =
        Game::Map::lighting_for_hour(static_cast<float>(hour), profile->currentText());
    const QVector3D direction = lighting.primary_direction.normalized();
    const double elevation_degrees =
        qRadiansToDegrees(std::asin(std::clamp(direction.y(), -1.0F, 1.0F)));
    const double azimuth_degrees =
        qRadiansToDegrees(std::atan2(direction.x(), direction.z()));
    const QString phase =
        elevation_degrees <= 0.0
            ? QStringLiteral("moonlight / below horizon")
            : (elevation_degrees < 20.0 ? QStringLiteral("low-angle light")
                                        : QStringLiteral("high sun"));
    sun->setText(QString("Sun-direction preview: %1 — %2° elevation, %3° azimuth "
                         "(profile \"%4\"). Team colors and selection markers retain a "
                         "readability floor in every profile.")
                     .arg(phase)
                     .arg(elevation_degrees, 0, 'f', 0)
                     .arg(azimuth_degrees, 0, 'f', 0)
                     .arg(profile->currentText()));
  };

  const auto write_environment = [=]() {
    QJsonObject environment = map_data->environment();
    environment[MapEditor::MapJsonKeys::start_time] = time->value();
    environment[MapEditor::MapJsonKeys::time_mode] = mode->currentData().toString();
    environment[MapEditor::MapJsonKeys::day_length_seconds] = day_length->value();
    environment[MapEditor::MapJsonKeys::lighting_profile] = profile->currentText();
    if (fog->value() < 0.0) {
      environment.remove(MapEditor::MapJsonKeys::fog_density);
    } else {
      environment[MapEditor::MapJsonKeys::fog_density] = fog->value();
    }
    if (exposure->value() < 0.0) {
      environment.remove(MapEditor::MapJsonKeys::exposure);
    } else {
      environment[MapEditor::MapJsonKeys::exposure] = exposure->value();
    }
    map_data->set_environment(environment);
  };

  QObject::connect(time,
                   qOverload<double>(&QDoubleSpinBox::valueChanged),
                   panel,
                   [=](double) { write_environment(); });
  QObject::connect(mode,
                   qOverload<int>(&QComboBox::currentIndexChanged),
                   panel,
                   [=](int) { write_environment(); });
  QObject::connect(day_length,
                   qOverload<double>(&QDoubleSpinBox::valueChanged),
                   panel,
                   [=](double) { write_environment(); });
  QObject::connect(
      profile->lineEdit(), &QLineEdit::editingFinished, panel, write_environment);
  QObject::connect(fog,
                   qOverload<double>(&QDoubleSpinBox::valueChanged),
                   panel,
                   [=](double) { write_environment(); });
  QObject::connect(exposure,
                   qOverload<double>(&QDoubleSpinBox::valueChanged),
                   panel,
                   [=](double) { write_environment(); });

  const auto write_weather = [=]() {
    QJsonObject rain = map_data->rain();
    const QString selected = weather->currentData().toString();
    rain["enabled"] = selected != QStringLiteral("off");
    rain["type"] = selected == QStringLiteral("snow") ? "snow" : "rain";
    rain["intensity"] = weather_intensity->value();
    rain["wind_strength"] = wind_strength->value();
    rain["wind_direction"] = wind_direction->value();
    map_data->set_rain(rain);
  };
  QObject::connect(weather,
                   qOverload<int>(&QComboBox::currentIndexChanged),
                   panel,
                   [=](int) { write_weather(); });
  QObject::connect(weather_intensity,
                   qOverload<double>(&QDoubleSpinBox::valueChanged),
                   panel,
                   [=](double) { write_weather(); });
  QObject::connect(wind_strength,
                   qOverload<double>(&QDoubleSpinBox::valueChanged),
                   panel,
                   [=](double) { write_weather(); });
  QObject::connect(wind_direction,
                   qOverload<double>(&QDoubleSpinBox::valueChanged),
                   panel,
                   [=](double) { write_weather(); });
  QObject::connect(map_data, &MapEditor::MapData::data_changed, panel, refresh);
  refresh();
  return panel;
}

auto normalizedDisplayPath(const QString& file_path) -> QString {
  return QDir::toNativeSeparators(QFileInfo(file_path).absoluteFilePath());
}

} // namespace

namespace MapEditor {

EditorWindow::EditorWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_map_data(new MapData(this))
    , m_mission_data(new MissionData(this)) {

  setup_ui();
  setup_menus();

  connect(
      m_map_data, &MapData::modified_changed, this, &EditorWindow::on_modified_changed);
  connect(m_map_data,
          &MapData::undo_redo_changed,
          this,
          &EditorWindow::on_undo_redo_changed);
  connect(
      m_map_data, &MapData::data_changed, this, &EditorWindow::update_dimensions_label);
  connect(
      m_map_data, &MapData::data_changed, this, &EditorWindow::refresh_json_preview);
  connect(m_mission_data,
          &MissionData::modified_changed,
          this,
          &EditorWindow::on_modified_changed);
  connect(m_mission_data,
          &MissionData::data_changed,
          this,
          &EditorWindow::refresh_json_preview);

  resize(1600, 960);

  reset_to_new_map();
  refresh_workspace_bar();
  update_window_title();
}

EditorWindow::~EditorWindow() = default;

void EditorWindow::setup_ui() {
  auto* shell = new QWidget(this);
  setCentralWidget(shell);
  auto* shell_layout = new QVBoxLayout(shell);
  shell_layout->setContentsMargins(0, 0, 0, 0);
  shell_layout->setSpacing(0);
  auto* workspace_row = addToolBar(QStringLiteral("Workspaces"));
  workspace_row->setObjectName(QStringLiteral("workspace_row"));
  workspace_row->setMovable(false);
  workspace_row->setFloatable(false);
  workspace_row->setContextMenuPolicy(Qt::PreventContextMenu);
  workspace_row->setStyleSheet(
      QStringLiteral("QToolBar{border:none;padding:0;margin:0;spacing:0;}"));
  workspace_row->addWidget(build_workspace_bar());
  addToolBarBreak();
  m_stack = new QStackedWidget(shell);
  shell_layout->addWidget(m_stack, 1);

  auto* central_widget = new QWidget(m_stack);
  m_battlefield_page = central_widget;
  m_stack->addWidget(central_widget);

  auto* main_layout = new QHBoxLayout(central_widget);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  auto* splitter = new QSplitter(Qt::Horizontal, central_widget);
  splitter->setChildrenCollapsible(false);

  m_canvas = new MapCanvas(splitter);
  m_canvas->set_map_data(m_map_data);
  connect(m_canvas,
          &MapCanvas::element_double_clicked,
          this,
          &EditorWindow::on_element_double_clicked);
  connect(m_canvas,
          &MapCanvas::grid_double_clicked,
          this,
          &EditorWindow::on_grid_double_clicked);
  connect(m_canvas, &MapCanvas::tool_cleared, this, &EditorWindow::on_tool_cleared);
  connect(m_canvas, &MapCanvas::status_hint_changed, this, [this](const QString& hint) {
    m_hint_active = !hint.isEmpty();
    if (m_hint_active && m_tool_label != nullptr) {
      m_tool_label->setText(hint);
    } else {
      refresh_status_label();
    }
  });
  connect(m_canvas, &MapCanvas::zoom_changed, this, [this](float zoom) {
    m_zoom_label->setText(QString("%1%").arg(static_cast<int>(zoom * 100)));
  });
  connect(m_canvas, &MapCanvas::action_feedback, this, [this](const QString& message) {
    show_action_feedback(message);
  });
  connect(m_canvas,
          &MapCanvas::selection_changed,
          this,
          &EditorWindow::on_selection_changed);
  connect(m_canvas, &MapCanvas::cursor_moved, this, [this](int gx, int gz) {
    if (m_cursor_label != nullptr) {
      m_cursor_label->setText(QString("X:%1 Z:%2").arg(gx).arg(gz));
    }
  });
  connect(
      m_map_data, &MapData::data_changed, this, &EditorWindow::update_selection_info);

  m_sidebar_tabs = new QTabWidget(splitter);
  m_sidebar_tabs->setMinimumWidth(300);
  m_sidebar_tabs->setMaximumWidth(560);

  m_tool_panel = new ToolPanel(m_sidebar_tabs);
  connect(
      m_tool_panel, &ToolPanel::tool_selected, this, &EditorWindow::on_tool_selected);
  connect(m_tool_panel,
          &ToolPanel::player_id_changed,
          m_canvas,
          &MapCanvas::set_current_player_id);
  connect(m_tool_panel,
          &ToolPanel::nation_changed,
          m_canvas,
          &MapCanvas::set_current_nation);

  auto* tools_scroll = new QScrollArea(m_sidebar_tabs);
  tools_scroll->setWidget(m_tool_panel);
  tools_scroll->setWidgetResizable(true);
  tools_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_sidebar_tabs->addTab(tools_scroll, "Tools");

  m_mission_panel = new MissionPanel(m_mission_data, m_map_data, m_sidebar_tabs);
  connect(m_mission_panel,
          &MissionPanel::map_path_changed,
          this,
          &EditorWindow::on_mission_map_path_changed);
  connect(m_mission_panel,
          &MissionPanel::validate_requested,
          this,
          &EditorWindow::validate_mission);
  connect(m_mission_panel,
          &MissionPanel::launch_game_requested,
          this,
          &EditorWindow::launch_mission_game);
  connect(m_mission_panel,
          &MissionPanel::launch_arena_requested,
          this,
          &EditorWindow::launch_mission_arena);
  m_mission_scroll = new QScrollArea(m_sidebar_tabs);
  m_mission_scroll->setWidget(m_mission_panel);
  m_mission_scroll->setWidgetResizable(true);
  m_mission_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_sidebar_tabs->addTab(m_mission_scroll, "Mission");

  auto* environment_scroll = new QScrollArea(m_sidebar_tabs);
  environment_scroll->setWidget(createEnvironmentPanel(m_map_data, environment_scroll));
  environment_scroll->setWidgetResizable(true);
  environment_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_sidebar_tabs->addTab(environment_scroll, "Environment");

  m_generator_panel = new GeneratorPanel(m_map_data, m_sidebar_tabs);
  m_generator_panel->set_repository_root(repository_root());
  connect(m_generator_panel,
          &GeneratorPanel::feedback,
          this,
          [this](const QString& message, bool success) {
            show_action_feedback(message, success);
          });
  connect(m_generator_panel, &GeneratorPanel::document_replaced, this, [this]() {
    m_canvas->clear_selection();
    m_canvas->zoom_to_fit();
    update_dimensions_label();
  });
  auto* generator_scroll = new QScrollArea(m_sidebar_tabs);
  generator_scroll->setWidget(m_generator_panel);
  generator_scroll->setWidgetResizable(true);
  generator_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_sidebar_tabs->addTab(generator_scroll, "Generator");

  m_campaign_context = new QTextBrowser(m_sidebar_tabs);
  m_campaign_context->setOpenLinks(false);
  m_campaign_context->document()->setDefaultStyleSheet(
      QStringLiteral("body{color:#F4E7C8;} a{color:#E8C98B;text-decoration:none;} "
                     "h2{margin:0;font-size:14pt;}"
                     "h4{margin:10px 0 3px 0;color:#C7A66D;font-size:9pt;} "
                     ".dim{color:#9C8158;} td{padding:1px 8px 1px 0;}"));
  connect(
      m_campaign_context, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
        const QString link = url.toString();
        if (link.startsWith(QStringLiteral("site:"))) {
          set_workspace(Workspace::Campaign);
          m_campaign->select_entity(link.mid(5));
        }
      });
  m_sidebar_tabs->addTab(m_campaign_context, "Campaign");

  auto* guide_scroll = new QScrollArea(m_sidebar_tabs);
  guide_scroll->setWidget(createGuidePanel(guide_scroll));
  guide_scroll->setWidgetResizable(true);
  guide_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_sidebar_tabs->addTab(guide_scroll, "Guide");

  m_json_preview = new QPlainTextEdit(m_sidebar_tabs);
  m_json_preview->setReadOnly(true);
  m_json_preview->setLineWrapMode(QPlainTextEdit::NoWrap);
  QFont mono_font("Monospace");
  mono_font.setStyleHint(QFont::TypeWriter);
  mono_font.setPointSize(9);
  m_json_preview->setFont(mono_font);
  m_json_preview->setPlaceholderText("JSON preview will appear here…");
  m_sidebar_tabs->addTab(m_json_preview, "JSON");

  splitter->addWidget(m_canvas);
  splitter->addWidget(m_sidebar_tabs);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 0);
  splitter->setSizes({1180, 480});
  set_mission_mode(false);

  main_layout->addWidget(splitter);

  m_feedback_label = new QLabel("Ready", this);
  m_feedback_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_feedback_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_tool_label = new QLabel(m_tool_status_text, this);
  m_tool_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_dimensions_label = new QLabel("", this);
  m_dimensions_label->setToolTip(
      "Double-click on empty canvas area to edit dimensions");
  m_zoom_label = new QLabel("100%", this);
  m_zoom_label->setToolTip("Current zoom level (scroll to zoom)");
  m_zoom_label->setMinimumWidth(48);
  m_zoom_label->setAlignment(Qt::AlignCenter);

  m_zoom_widget = new QWidget(this);
  auto* zoom_layout = new QHBoxLayout(m_zoom_widget);
  zoom_layout->setContentsMargins(0, 0, 0, 0);
  zoom_layout->setSpacing(2);

  const auto add_zoom_button =
      [this, zoom_layout](const QString& text, const QString& tip, auto&& slot) {
        auto* button = new QToolButton(m_zoom_widget);
        button->setText(text);
        button->setToolTip(tip);
        button->setAutoRaise(true);
        connect(button, &QToolButton::clicked, m_canvas, slot);
        zoom_layout->addWidget(button);
      };

  add_zoom_button("−", "Zoom out (Ctrl+-)", &MapCanvas::zoom_out);
  zoom_layout->addWidget(m_zoom_label);
  add_zoom_button("+", "Zoom in (Ctrl++)", &MapCanvas::zoom_in);
  add_zoom_button(
      "Fit", "Fit the whole map in the canvas (Ctrl+0)", &MapCanvas::zoom_to_fit);
  m_cursor_label = new QLabel("X:0 Z:0", this);
  m_cursor_label->setToolTip("Grid cursor position");
  m_cursor_label->setMinimumWidth(80);
  m_file_label = new QLabel(this);
  m_file_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_file_label->setToolTip("Current map file path");
  update_current_file_label();

  statusBar()->addWidget(m_feedback_label, 1);
  statusBar()->addPermanentWidget(m_tool_label);
  statusBar()->addPermanentWidget(m_cursor_label);
  statusBar()->addPermanentWidget(m_file_label);
  statusBar()->addPermanentWidget(m_zoom_widget);
  statusBar()->addPermanentWidget(m_dimensions_label);
  auto* version_label = new QLabel("Standard of Iron Editor", this);
  version_label->setProperty("status", "muted");
  statusBar()->addPermanentWidget(version_label);
}

void EditorWindow::setup_menus() {

  auto* file_menu = menuBar()->addMenu("&File");

  auto* new_action = new QAction("&New", this);
  new_action->setShortcut(QKeySequence::New);
  new_action->setToolTip("Create a new map (Ctrl+N)");
  connect(new_action, &QAction::triggered, this, &EditorWindow::new_map);
  file_menu->addAction(new_action);

  auto* new_mission_action = new QAction("New &Mission", this);
  new_mission_action->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N));
  new_mission_action->setToolTip("Create a complete mission definition");
  connect(new_mission_action, &QAction::triggered, this, &EditorWindow::new_mission);
  file_menu->addAction(new_mission_action);

  auto* open_action = new QAction("&Open...", this);
  open_action->setShortcut(QKeySequence::Open);
  open_action->setToolTip("Open an existing map (Ctrl+O)");
  connect(open_action, &QAction::triggered, this, &EditorWindow::open_map);
  file_menu->addAction(open_action);

  auto* open_world_action = new QAction("Open Campaign &World…", this);
  open_world_action->setToolTip(
      "Open a strategic world file in the Campaign workspace");
  connect(open_world_action, &QAction::triggered, this, [this]() {
    ensure_campaign();
    if (!m_campaign->maybe_save()) {
      return;
    }
    const QString path =
        QFileDialog::getOpenFileName(this,
                                     "Open Campaign World",
                                     QDir(repository_root()).filePath("assets/world"),
                                     "Campaign world (*.json)");
    if (!path.isEmpty()) {
      open_campaign(path);
    }
  });
  file_menu->addAction(open_world_action);

  file_menu->addSeparator();

  auto* save_action = new QAction("&Save", this);
  m_save_action = save_action;
  save_action->setShortcut(QKeySequence::Save);
  save_action->setToolTip("Save the current map (Ctrl+S)");
  connect(save_action, &QAction::triggered, this, &EditorWindow::save_map);
  file_menu->addAction(save_action);

  auto* save_as_action = new QAction("Save &As...", this);
  save_as_action->setShortcut(QKeySequence::SaveAs);
  save_as_action->setToolTip("Save the map to a new file (Ctrl+Shift+S)");
  connect(save_as_action, &QAction::triggered, this, &EditorWindow::save_map_as);
  file_menu->addAction(save_as_action);

  file_menu->addSeparator();

  auto* exit_action = new QAction("E&xit", this);
  exit_action->setShortcut(QKeySequence::Quit);
  connect(exit_action, &QAction::triggered, this, &QWidget::close);
  file_menu->addAction(exit_action);

  auto* edit_menu = menuBar()->addMenu("&Edit");

  m_undo_action = new QAction("&Undo", this);
  m_undo_action->setShortcut(QKeySequence::Undo);
  m_undo_action->setEnabled(false);
  m_undo_action->setToolTip("Undo the last change (Ctrl+Z)");
  connect(m_undo_action, &QAction::triggered, this, &EditorWindow::undo);
  edit_menu->addAction(m_undo_action);

  m_redo_action = new QAction("&Redo", this);
  m_redo_action->setShortcut(QKeySequence::Redo);
  m_redo_action->setEnabled(false);
  m_redo_action->setToolTip("Redo the last undone change (Ctrl+Y)");
  connect(m_redo_action, &QAction::triggered, this, &EditorWindow::redo);
  edit_menu->addAction(m_redo_action);

  edit_menu->addSeparator();

  auto* resize_action = new QAction("&Resize Map...", this);
  resize_action->setToolTip("Resize the grid dimensions");
  connect(resize_action, &QAction::triggered, this, &EditorWindow::resize_map);
  edit_menu->addAction(resize_action);

  auto* biome_action = new QAction("Edit &Biome JSON...", this);
  biome_action->setToolTip("Edit ground and biome rendering settings");
  connect(biome_action, &QAction::triggered, this, &EditorWindow::edit_biome);
  edit_menu->addAction(biome_action);

  edit_menu->addSeparator();

  auto* duplicate_action = new QAction("&Duplicate Selection", this);
  duplicate_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
  duplicate_action->setToolTip("Duplicate the selected element one cell away (Ctrl+D)");
  connect(
      duplicate_action, &QAction::triggered, m_canvas, &MapCanvas::duplicate_selection);
  edit_menu->addAction(duplicate_action);

  auto* copy_action = new QAction("&Copy Selection", this);
  copy_action->setShortcut(QKeySequence::Copy);
  copy_action->setToolTip("Copy the selected element (Ctrl+C)");
  connect(copy_action, &QAction::triggered, m_canvas, &MapCanvas::copy_selection);
  edit_menu->addAction(copy_action);

  auto* paste_action = new QAction("&Paste At Cursor", this);
  paste_action->setShortcut(QKeySequence::Paste);
  paste_action->setToolTip("Paste the copied element at the cursor (Ctrl+V)");
  connect(paste_action, &QAction::triggered, m_canvas, &MapCanvas::paste_at_cursor);
  edit_menu->addAction(paste_action);

  auto* select_all_action = new QAction("Select &All", this);
  select_all_action->setShortcut(QKeySequence::SelectAll);
  select_all_action->setToolTip("Select every element on the visible layers (Ctrl+A)");
  connect(select_all_action, &QAction::triggered, m_canvas, &MapCanvas::select_all);
  edit_menu->addAction(select_all_action);

  auto* view_menu = menuBar()->addMenu("&View");

  auto* zoom_in_action = new QAction("Zoom &In", this);
  zoom_in_action->setShortcut(QKeySequence::ZoomIn);
  connect(zoom_in_action, &QAction::triggered, m_canvas, &MapCanvas::zoom_in);
  view_menu->addAction(zoom_in_action);

  auto* zoom_out_action = new QAction("Zoom &Out", this);
  zoom_out_action->setShortcut(QKeySequence::ZoomOut);
  connect(zoom_out_action, &QAction::triggered, m_canvas, &MapCanvas::zoom_out);
  view_menu->addAction(zoom_out_action);

  auto* zoom_fit_action = new QAction("Zoom To &Fit", this);
  zoom_fit_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
  zoom_fit_action->setToolTip("Fit the whole map in the canvas (Ctrl+0)");
  connect(zoom_fit_action, &QAction::triggered, m_canvas, &MapCanvas::zoom_to_fit);
  view_menu->addAction(zoom_fit_action);

  auto* frame_action = new QAction("F&rame Selection\tF", this);
  frame_action->setToolTip("Centre the view on the selected element (F)");
  connect(frame_action, &QAction::triggered, m_canvas, &MapCanvas::frame_selection);
  view_menu->addAction(frame_action);

  view_menu->addSeparator();

  auto* bring_front_action = new QAction("&Bring Selection To Front", this);
  bring_front_action->setToolTip(
      "Draw the selection above everything else (view only)");
  connect(bring_front_action,
          &QAction::triggered,
          m_canvas,
          &MapCanvas::bring_selection_to_front);
  view_menu->addAction(bring_front_action);

  auto* send_back_action = new QAction("Se&nd Selection To Back", this);
  send_back_action->setToolTip("Draw the selection below everything else (view only)");
  connect(send_back_action,
          &QAction::triggered,
          m_canvas,
          &MapCanvas::send_selection_to_back);
  view_menu->addAction(send_back_action);

  auto* reset_order_action = new QAction("Reset &Draw Order", this);
  connect(
      reset_order_action, &QAction::triggered, m_canvas, &MapCanvas::reset_draw_order);
  view_menu->addAction(reset_order_action);

  view_menu->addSeparator();

  auto* layers_menu = view_menu->addMenu("&Layers");
  for (int layer = 0; layer < MapCanvas::LayerCount; ++layer) {
    auto* action = new QAction(MapCanvas::layer_label(layer), this);
    action->setCheckable(true);
    action->setChecked(m_canvas->layer_visible(layer));
    connect(action, &QAction::toggled, this, [this, layer](bool visible) {
      m_canvas->set_layer_visible(layer, visible);
    });
    layers_menu->addAction(action);
  }

  auto* toolbar = addToolBar("Main");
  m_main_toolbar = toolbar;
  toolbar->setMovable(false);
  toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
  toolbar->addAction(new_action);
  toolbar->addAction(new_mission_action);
  toolbar->addAction(open_action);
  toolbar->addAction(save_action);
  toolbar->addSeparator();
  toolbar->addAction(m_undo_action);
  toolbar->addAction(m_redo_action);
  toolbar->addSeparator();
  toolbar->addAction(resize_action);

  auto* go_menu = menuBar()->addMenu("&Go");
  const auto add_go = [&](const QString& text, const QKeySequence& key, auto&& slot) {
    auto* action = new QAction(text, this);
    action->setShortcut(key);
    action->setShortcutContext(Qt::ApplicationShortcut);
    connect(action, &QAction::triggered, this, slot);
    go_menu->addAction(action);
    return action;
  };
  add_go("&Campaign", QKeySequence(Qt::CTRL | Qt::Key_1), [this]() {
    set_workspace(Workspace::Campaign);
  });
  add_go("&Battlefield", QKeySequence(Qt::CTRL | Qt::Key_2), [this]() {
    set_workspace(Workspace::Battlefield);
  });
  add_go("&Mission", QKeySequence(Qt::CTRL | Qt::Key_3), [this]() {
    set_workspace(Workspace::Mission);
  });
  go_menu->addSeparator();
  add_go("Command &Palette…", QKeySequence(Qt::CTRL | Qt::Key_K), [this]() {
    open_command_palette();
  });
  add_go("Back to Campaign &Site", QKeySequence(Qt::ALT | Qt::Key_Left), [this]() {
    set_workspace(Workspace::Campaign);
    if (!m_anchor_site.isEmpty() && m_campaign != nullptr) {
      m_campaign->select_entity(m_anchor_site);
    }
  });

  auto* test_menu = menuBar()->addMenu("&Test");
  auto* roads_action = new QAction("Check &Road Network…", this);
  roads_action->setToolTip("Find roads that do not join the rest of the network, and "
                           "snap near misses closed");
  connect(roads_action, &QAction::triggered, this, &EditorWindow::check_road_network);
  test_menu->addAction(roads_action);
  test_menu->addSeparator();
  auto* validate_action = new QAction("&Validate Mission", this);
  connect(validate_action, &QAction::triggered, this, &EditorWindow::validate_mission);
  test_menu->addAction(validate_action);
  auto* launch_game_action = new QAction("Validate && Launch &Game", this);
  connect(launch_game_action,
          &QAction::triggered,
          this,
          &EditorWindow::launch_mission_game);
  test_menu->addAction(launch_game_action);
  auto* launch_arena_action = new QAction("Validate && Launch &Arena", this);
  connect(launch_arena_action,
          &QAction::triggered,
          this,
          &EditorWindow::launch_mission_arena);
  test_menu->addAction(launch_arena_action);
}

void EditorWindow::check_road_network() {
  const RoadNetworkReport report = analyse_road_network(m_map_data->linear_elements());
  QString details = report.summary();
  for (int g = 1; g < report.groups.size(); ++g) {
    const auto& group = report.groups[g];
    const auto& first = m_map_data->linear_elements().at(group.first());
    details +=
        QStringLiteral("\n• Separate network %1: %2 element(s), starting near (%3, %4)")
            .arg(g)
            .arg(group.size())
            .arg(first.start.x(), 0, 'f', 0)
            .arg(first.start.y(), 0, 'f', 0);
  }
  for (const auto& miss : report.near_misses) {
    details += QStringLiteral("\n• Road #%1 stops %2 cells short of #%3 at (%4, %5)")
                   .arg(miss.element)
                   .arg(miss.gap, 0, 'f', 1)
                   .arg(miss.target_element)
                   .arg(miss.end.x(), 0, 'f', 0)
                   .arg(miss.end.y(), 0, 'f', 0);
  }
  if (!report.near_misses.isEmpty()) {
    m_canvas->center_on(report.near_misses.first().end);
  } else if (report.groups.size() > 1) {
    const auto& stray = m_map_data->linear_elements().at(report.groups[1].first());
    m_canvas->center_on(QPointF(stray.start.x(), stray.start.y()));
  }
  QMessageBox box(this);
  box.setWindowTitle(QStringLiteral("Road Network"));
  box.setIcon(report.connected() ? QMessageBox::Information : QMessageBox::Warning);
  box.setText(report.summary());
  box.setDetailedText(details);
  QPushButton* snap = nullptr;
  if (!report.near_misses.isEmpty()) {
    snap = box.addButton(
        QStringLiteral("Snap %1 Near Miss(es)").arg(report.near_misses.size()),
        QMessageBox::AcceptRole);
  }
  box.addButton(QMessageBox::Close);
  box.exec();
  if (snap == nullptr || box.clickedButton() != snap) {
    show_action_feedback(report.summary(), report.connected());
    return;
  }
  std::vector<std::unique_ptr<Command>> commands;
  for (const auto& miss : report.near_misses) {
    const LinearElement before = m_map_data->linear_elements().at(miss.element);
    commands.push_back(
        std::make_unique<UpdateLinearCmd>(m_map_data,
                                          miss.element,
                                          before,
                                          snapped(before, miss),
                                          QStringLiteral("Snap road end")));
  }
  m_map_data->execute_command(std::make_unique<CompositeCmd>(
      std::move(commands), QStringLiteral("Snap road near misses")));
  const RoadNetworkReport after = analyse_road_network(m_map_data->linear_elements());
  show_action_feedback(QStringLiteral("Snapped %1 road end(s). %2")
                           .arg(report.near_misses.size())
                           .arg(after.summary()),
                       after.connected());
}

auto EditorWindow::build_workspace_bar() -> QWidget* {
  auto* bar = new QWidget(this);
  bar->setObjectName(QStringLiteral("workspace_bar"));
  bar->setStyleSheet(QStringLiteral(
      "#workspace_bar{background:#1A140F;border-bottom:1px solid #4A3820;}"
      "#workspace_bar QToolButton[workspace='true']{padding:6px 16px;border:none;"
      "border-bottom:2px solid "
      "transparent;color:#C7A66D;font-weight:600;background:transparent;}"
      "#workspace_bar "
      "QToolButton[workspace='true']:checked{color:#F4E7C8;border-bottom:2px solid "
      "#D4A15A;}"
      "#workspace_bar QToolButton[workspace='true']:disabled{color:#5A4630;}"
      "#workspace_bar QToolButton[workspace='true']:hover{color:#F4E7C8;}"
      "#workspace_bar QLabel{color:#9C8158;}"
      "#workspace_bar QLabel a{color:#E8C98B;}"
      "#workspace_search{background:#231B14;border:1px solid "
      "#4A3820;border-radius:12px;"
      "padding:4px 12px;color:#8D7146;text-align:left;}"
      "#workspace_search:hover{border-color:#A7814A;color:#C7A66D;}"));
  auto* layout = new QHBoxLayout(bar);
  layout->setContentsMargins(8, 2, 8, 0);
  layout->setSpacing(2);
  auto* group = new QButtonGroup(bar);
  group->setExclusive(true);
  const auto tab = [&](const QString& text, const QString& tip, Workspace workspace) {
    auto* button = new QToolButton(bar);
    button->setText(text);
    button->setToolTip(tip);
    button->setCheckable(true);
    button->setProperty("workspace", true);
    group->addButton(button);
    connect(button, &QToolButton::clicked, this, [this, workspace]() {
      set_workspace(workspace);
    });
    layout->addWidget(button);
    return button;
  };
  m_campaign_tab = tab(QStringLiteral("Campaign"),
                       QStringLiteral("The strategic world: regions, roads, armies, "
                                      "supply and the war's timeline (Ctrl+1)"),
                       Workspace::Campaign);
  m_battlefield_tab =
      tab(QStringLiteral("Battlefield"),
          QStringLiteral(
              "Edit the battle map: terrain, roads, settlements, spawns (Ctrl+2)"),
          Workspace::Battlefield);
  m_mission_tab =
      tab(QStringLiteral("Mission"),
          QStringLiteral(
              "Objectives, forces, waves and dialogue on top of the map (Ctrl+3)"),
          Workspace::Mission);
  layout->addSpacing(18);
  m_breadcrumb = new QLabel(bar);
  m_breadcrumb->setTextFormat(Qt::RichText);
  connect(m_breadcrumb, &QLabel::linkActivated, this, [this](const QString& link) {
    if (link == QLatin1String("campaign")) {
      set_workspace(Workspace::Campaign);
    } else if (link.startsWith(QStringLiteral("site:"))) {
      set_workspace(Workspace::Campaign);
      m_campaign->select_entity(link.mid(5));
    } else if (link == QLatin1String("battlefield")) {
      set_workspace(Workspace::Battlefield);
    }
  });
  layout->addWidget(m_breadcrumb, 1);
  m_problem_badge = new QToolButton(bar);
  m_problem_badge->setAutoRaise(true);
  m_problem_badge->setToolTip(QStringLiteral("Campaign problems — click to review"));
  connect(m_problem_badge, &QToolButton::clicked, this, [this]() {
    set_workspace(Workspace::Campaign);
    m_campaign->show_problems();
  });
  layout->addWidget(m_problem_badge);
  auto* search = new QToolButton(bar);
  search->setObjectName(QStringLiteral("workspace_search"));
  search->setText(QStringLiteral("Search or run a command…     Ctrl+K"));
  search->setMinimumWidth(280);
  connect(search, &QToolButton::clicked, this, [this]() { open_command_palette(); });
  layout->addWidget(search);
  m_palette = new CommandPalette(this);
  return bar;
}

void EditorWindow::ensure_campaign() {
  if (m_campaign != nullptr) {
    return;
  }
  m_campaign = new CampaignWorkspace(m_stack);
  m_stack->addWidget(m_campaign);
  connect(m_campaign,
          &CampaignWorkspace::feedback,
          this,
          [this](const QString& message, bool success) {
            show_action_feedback(message, success);
          });
  connect(
      m_campaign, &CampaignWorkspace::cursor_text, this, [this](const QString& text) {
        if (m_cursor_label != nullptr && m_workspace == Workspace::Campaign) {
          m_cursor_label->setText(text);
        }
      });
  connect(m_campaign, &CampaignWorkspace::summary_changed, this, [this]() {
    refresh_workspace_bar();
    on_undo_redo_changed();
    update_window_title();
  });
  connect(m_campaign,
          &CampaignWorkspace::open_battlefield_requested,
          this,
          &EditorWindow::open_battlefield_from_campaign);
  connect(
      m_campaign,
      &CampaignWorkspace::generate_battlefield_requested,
      this,
      [this](const QJsonObject& request, const QString& title, const QString& site) {
        if (!maybe_save()) {
          return;
        }
        reset_to_new_map();
        m_anchor_site = site;
        set_workspace(Workspace::Battlefield);
        select_sidebar_tab(QStringLiteral("Generator"));
        m_generator_panel->prefill_from(request);
        show_action_feedback(
            QStringLiteral(
                "Generator prefilled for %1. Press Generate to build the battlefield.")
                .arg(title));
      });
}

auto EditorWindow::open_campaign(const QString& path) -> CampaignWorkspace* {
  ensure_campaign();
  const QString target =
      path.isEmpty()
          ? QDir(repository_root())
                .filePath(QString::fromLatin1(Game::Strategic::k_default_world_path))
          : path;
  if (m_campaign->document()->path() != target) {
    m_campaign->open_world(target);
  }
  set_workspace(Workspace::Campaign);
  return m_campaign;
}

void EditorWindow::open_battlefield_from_campaign(const QString& map_path,
                                                  const QString& mission_id,
                                                  const QString& site_id) {
  const QString mission_path =
      mission_id.isEmpty()
          ? QString()
          : QDir(repository_root())
                .filePath(QStringLiteral("assets/missions/%1.json").arg(mission_id));
  const QString target = !mission_path.isEmpty() && QFileInfo::exists(mission_path)
                             ? mission_path
                             : map_path;
  if (target.isEmpty()) {
    show_action_feedback(QStringLiteral("That site links no battlefield file."), false);
    return;
  }
  if (QFileInfo(target).absoluteFilePath() != m_current_file_path) {
    if (!maybe_save() || !load_file(target)) {
      return;
    }
  }
  m_anchor_site = site_id;
  set_workspace(m_mission_mode ? Workspace::Mission : Workspace::Battlefield);
}

void EditorWindow::ensure_campaign_context_world() {
  ensure_campaign();
  if (m_campaign->has_world()) {
    return;
  }
  const QString path =
      QDir(repository_root())
          .filePath(QString::fromLatin1(Game::Strategic::k_default_world_path));
  if (QFileInfo::exists(path)) {
    QString error;
    (void)m_campaign->document()->load(path, &error);
  }
}

void EditorWindow::set_workspace(Workspace workspace) {
  if (workspace == Workspace::Campaign && m_campaign == nullptr) {
    open_campaign();
    return;
  }
  if (workspace == Workspace::Mission && !m_mission_mode) {
    open_command_palette();
    m_palette->open_with(QStringLiteral("mission "));
    return;
  }
  m_workspace = workspace;
  const bool campaign = workspace == Workspace::Campaign;
  m_stack->setCurrentWidget(campaign ? static_cast<QWidget*>(m_campaign)
                                     : m_battlefield_page);
  if (m_main_toolbar != nullptr) {
    m_main_toolbar->setVisible(!campaign);
  }
  if (m_cursor_label != nullptr) {
    m_cursor_label->clear();
  }
  for (QWidget* widget : {static_cast<QWidget*>(m_dimensions_label),
                          m_zoom_widget,
                          static_cast<QWidget*>(m_tool_label),
                          static_cast<QWidget*>(m_file_label)}) {
    if (widget != nullptr) {
      widget->setVisible(!campaign);
    }
  }
  if (!campaign && m_sidebar_tabs != nullptr) {
    if (workspace == Workspace::Mission && m_mission_scroll != nullptr) {
      m_sidebar_tabs->setCurrentWidget(m_mission_scroll);
    } else if (m_sidebar_tabs->currentWidget() == m_mission_scroll) {
      m_sidebar_tabs->setCurrentIndex(0);
    }
  }
  if (campaign) {
    m_campaign->canvas()->setFocus();
  } else {
    m_canvas->setFocus();
  }
  refresh_workspace_bar();
  on_undo_redo_changed();
  update_window_title();
}

void EditorWindow::refresh_workspace_bar() {
  if (m_campaign_tab == nullptr) {
    return;
  }
  m_campaign_tab->setChecked(m_workspace == Workspace::Campaign);
  m_battlefield_tab->setChecked(m_workspace == Workspace::Battlefield);
  m_mission_tab->setChecked(m_workspace == Workspace::Mission);
  m_mission_tab->setText(m_mission_mode ? QStringLiteral("Mission")
                                        : QStringLiteral("Mission…"));
  QStringList crumbs;
  const QString link_style = QStringLiteral("color:#E8C98B;text-decoration:none");
  const QString campaign_name =
      m_campaign != nullptr && m_campaign->has_world()
          ? m_campaign->document()->definition().name.section(QLatin1Char(':'), 0, 0)
          : QStringLiteral("Campaign");
  if (m_workspace == Workspace::Campaign) {
    crumbs.append(QStringLiteral("<b style='color:#F4E7C8'>%1</b>")
                      .arg(campaign_name.toHtmlEscaped()));
    if (m_campaign != nullptr) {
      const auto& doc = *m_campaign->document();
      if (const auto* event = doc.current_event()) {
        crumbs.append(event->date_label());
      } else {
        crumbs.append(doc.definition().start_label);
      }
      if (!doc.selection().isEmpty()) {
        crumbs.append(
            QStringLiteral("<span style='color:#E8C98B'>%1</span>")
                .arg(doc.definition().display_name(doc.selection()).toHtmlEscaped()));
      }
    }
  } else {
    QString site = m_anchor_site;
    if (site.isEmpty() && m_campaign != nullptr) {
      site = m_campaign->site_for_battlefield(
          m_mission_mode ? m_linked_map_file_path : m_current_file_path,
          m_mission_mode ? m_mission_data->id() : QString());
    }
    crumbs.append(QStringLiteral("<a href='campaign' style='%2'>%1</a>")
                      .arg(campaign_name.toHtmlEscaped(), link_style));
    if (!site.isEmpty() && m_campaign != nullptr) {
      crumbs.append(QStringLiteral("<a href='site:%1' style='%3'>%2</a>")
                        .arg(site,
                             m_campaign->document()
                                 ->definition()
                                 .display_name(site)
                                 .toHtmlEscaped(),
                             link_style));
    }
    refresh_campaign_context(site);
    const QString map_file =
        QFileInfo(m_mission_mode ? m_linked_map_file_path : m_current_file_path)
            .fileName();
    const QString map_label = map_file.isEmpty() ? m_map_data->name() : map_file;
    crumbs.append(m_workspace == Workspace::Mission
                      ? QStringLiteral("<a href='battlefield' style='%2'>%1</a>")
                            .arg(map_label.toHtmlEscaped(), link_style)
                      : QStringLiteral("<b style='color:#F4E7C8'>%1</b>")
                            .arg(map_label.toHtmlEscaped()));
    if (m_workspace == Workspace::Mission) {
      crumbs.append(QStringLiteral("<b style='color:#F4E7C8'>%1</b>")
                        .arg(m_mission_data->title().toHtmlEscaped()));
    }
  }
  m_breadcrumb->setText(
      crumbs.join(QStringLiteral("  <span style='color:#5A4630'>›</span>  ")));
  if (m_campaign != nullptr && m_campaign->has_world()) {
    const auto& report = m_campaign->document()->report();
    m_problem_badge->setVisible(true);
    m_problem_badge->setText(
        report.errors() + report.warnings() == 0
            ? QStringLiteral("✔ Campaign valid")
            : QStringLiteral("✖ %1  ⚠ %2").arg(report.errors()).arg(report.warnings()));
  } else {
    m_problem_badge->setVisible(false);
  }
}

void EditorWindow::refresh_campaign_context(const QString& site_id) {
  namespace S = Game::Strategic;
  if (m_campaign_context == nullptr) {
    return;
  }
  if (m_campaign == nullptr || !m_campaign->has_world() || site_id.isEmpty()) {
    m_campaign_context->setHtml(
        QStringLiteral("<h2>Campaign</h2><p class='dim'>This battlefield is not linked "
                       "to a campaign site. Link "
                       "one from the Campaign workspace (a site's <i>Battlefield "
                       "map</i> and <i>Mission</i> "
                       "fields), or open it from the campaign map.</p>"));
    return;
  }
  const auto& def = m_campaign->document()->definition();
  const auto* site = def.site(site_id);
  if (site == nullptr) {
    return;
  }
  int index = static_cast<int>(def.events.size());
  QString event_label;
  for (int i = 0; i < def.events.size(); ++i) {
    if (!site->battlefield.mission.isEmpty() &&
        def.events[i].mission == site->battlefield.mission) {
      index = i;
      event_label =
          def.events[i].name + QStringLiteral(" · ") + def.events[i].date_label();
      break;
    }
  }
  const S::WorldState state = S::state_after(def, index);
  const S::Season season =
      index < def.events.size() ? def.events[index].season : S::Season::Spring;
  const S::Encounter encounter = S::encounter_at(def, state, site_id);
  const S::EncounterContext context =
      S::encounter_context(def, state, encounter, season);
  const auto name = [&def](const QString& id) {
    return def.display_name(id).toHtmlEscaped();
  };
  QString html = QStringLiteral("<h2>%1</h2><div class='dim'>%2 · %3 · <a "
                                "href='site:%4'>show on the campaign map</a></div>")
                     .arg(site->name.toHtmlEscaped(),
                          name(site->region),
                          S::to_string(site->terrain),
                          site_id);
  if (!event_label.isEmpty()) {
    html += QStringLiteral("<h4>THE SITUATION BEFORE</h4>%1")
                .arg(event_label.toHtmlEscaped());
  }
  QStringList combatants;
  if (index < def.events.size()) {
    for (const auto& effect : def.events[index].effects) {
      const bool army_effect = effect.op == S::EffectOp::MoveArmy ||
                               effect.op == S::EffectOp::SetArmyStrength ||
                               effect.op == S::EffectOp::ScaleArmy ||
                               effect.op == S::EffectOp::DisbandArmy;
      if (army_effect && state.army(effect.target) != nullptr &&
          !combatants.contains(effect.target)) {
        combatants.append(effect.target);
      }
    }
  }
  html += QStringLiteral("<table>");
  html += QStringLiteral("<tr><td>Held by</td><td><b>%1</b></td></tr>")
              .arg(name(context.controller));
  if (!combatants.isEmpty()) {
    for (const auto& id : combatants) {
      const auto* army = state.army(id);
      html +=
          QStringLiteral("<tr><td>%1</td><td><b>%2</b> — %3 foot, %4 horse%5</td></tr>")
              .arg(name(army->faction), name(id))
              .arg(army->infantry)
              .arg(army->cavalry)
              .arg(army->elephants == 1 ? QStringLiteral(", 1 elephant")
                   : army->elephants > 1
                       ? QStringLiteral(", %1 elephants").arg(army->elephants)
                       : QString());
    }
  } else {
    html += QStringLiteral("<tr><td>Attacker</td><td><b>%1</b>%2</td></tr>")
                .arg(name(context.attacker),
                     encounter.attacker_army.isEmpty()
                         ? QString()
                         : QStringLiteral(" — %1, %2 men")
                               .arg(name(encounter.attacker_army))
                               .arg(context.attacker_soldiers));
    html += QStringLiteral("<tr><td>Defender</td><td><b>%1</b>%2</td></tr>")
                .arg(name(context.defender),
                     encounter.defender_army.isEmpty()
                         ? QString()
                         : QStringLiteral(" — %1, %2 men")
                               .arg(name(encounter.defender_army))
                               .arg(context.defender_soldiers));
  }
  html +=
      QStringLiteral("<tr><td>Season</td><td>%1</td></tr>").arg(S::to_string(season));
  html +=
      QStringLiteral("<tr><td>Walls</td><td>%1</td></tr>").arg(context.fortification);
  html += QStringLiteral("</table>");
  html += QStringLiteral("<h4>ROADS THAT SHOULD ENTER THIS MAP</h4>");
  for (const auto* route : def.routes_at(site_id)) {
    html += QStringLiteral(
                "%1 <span class='dim'>from</span> %2 <span class='dim'>(%3)</span><br>")
                .arg(name(route->id),
                     name(route->other_end(site_id)),
                     S::to_string(route->kind));
  }
  html += QStringLiteral("<h4>WHAT THE GENERATOR WOULD BUILD</h4>%1 · preset <b>%2</b> "
                         "· biome <b>%3</b>%4")
              .arg(context.kind,
                   context.preset,
                   context.biome,
                   context.coastal ? QStringLiteral(" · coast") : QString());
  m_campaign_context->setHtml(html);
}

auto EditorWindow::palette_entries() -> QVector<PaletteEntry> {
  QVector<PaletteEntry> entries;
  const auto command =
      [&](const QString& title, const QString& detail, std::function<void()> run) {
        entries.append({title, detail, QStringLiteral("Command"), std::move(run)});
      };
  command(QStringLiteral("Go to Campaign"), QStringLiteral("Ctrl+1"), [this]() {
    set_workspace(Workspace::Campaign);
  });
  command(QStringLiteral("Go to Battlefield"), QStringLiteral("Ctrl+2"), [this]() {
    set_workspace(Workspace::Battlefield);
  });
  if (m_mission_mode) {
    command(QStringLiteral("Go to Mission"), QStringLiteral("Ctrl+3"), [this]() {
      set_workspace(Workspace::Mission);
    });
  }
  command(QStringLiteral("Save"), QStringLiteral("Ctrl+S"), [this]() { save_map(); });
  command(QStringLiteral("New Map"), QStringLiteral("Battlefield"), [this]() {
    new_map();
    set_workspace(Workspace::Battlefield);
  });
  command(QStringLiteral("New Mission"), QStringLiteral("Mission"), [this]() {
    new_mission();
  });
  command(QStringLiteral("Check Road Network"),
          QStringLiteral("Battlefield roads, bridges and fords"),
          [this]() {
            set_workspace(Workspace::Battlefield);
            check_road_network();
          });
  command(QStringLiteral("Validate Mission"), QStringLiteral("Mission"), [this]() {
    validate_mission();
  });
  command(QStringLiteral("Validate Campaign"),
          QStringLiteral("Every strategic check"),
          [this]() {
            open_campaign();
            m_campaign->validate_now();
          });
  command(QStringLiteral("Complete Road Network"),
          QStringLiteral("Campaign: connect every site"),
          [this]() {
            open_campaign();
            m_campaign->complete_network();
          });
  const QDir root(repository_root());
  QHash<QString, int> campaign_order;
  {
    QFile campaign(
        root.filePath(QStringLiteral("assets/campaigns/second_punic_war.json")));
    if (campaign.open(QIODevice::ReadOnly)) {
      for (const auto value : QJsonDocument::fromJson(campaign.readAll())
                                  .object()
                                  .value(QStringLiteral("missions"))
                                  .toArray()) {
        const QJsonObject mission = value.toObject();
        campaign_order.insert(mission.value(QStringLiteral("mission_id")).toString(),
                              mission.value(QStringLiteral("order_index")).toInt());
      }
    }
  }
  static const QStringList k_numerals = {QStringLiteral("I"),
                                         QStringLiteral("II"),
                                         QStringLiteral("III"),
                                         QStringLiteral("IV"),
                                         QStringLiteral("V"),
                                         QStringLiteral("VI"),
                                         QStringLiteral("VII"),
                                         QStringLiteral("VIII"),
                                         QStringLiteral("IX"),
                                         QStringLiteral("X")};
  QStringList mission_files = QDir(root.filePath(QStringLiteral("assets/missions")))
                                  .entryList({QStringLiteral("*.json")}, QDir::Files);
  std::sort(mission_files.begin(),
            mission_files.end(),
            [&](const QString& a, const QString& b) {
              const int oa = campaign_order.value(QFileInfo(a).baseName(), 1000);
              const int ob = campaign_order.value(QFileInfo(b).baseName(), 1000);
              return oa != ob ? oa < ob : a < b;
            });
  for (const auto& file_name : mission_files) {
    const QString path =
        root.filePath(QStringLiteral("assets/missions/%1").arg(file_name));
    QFile file(path);
    QString title = QFileInfo(path).baseName();
    if (file.open(QIODevice::ReadOnly)) {
      const QString read = QJsonDocument::fromJson(file.readAll())
                               .object()
                               .value(QStringLiteral("title"))
                               .toString();
      if (!read.isEmpty()) {
        title = read;
      }
    }
    const QString id = QFileInfo(path).baseName();
    const int order = campaign_order.value(id, -1);
    const QString detail =
        order >= 0 && order < k_numerals.size()
            ? QStringLiteral("campaign %1 · %2").arg(k_numerals[order], file_name)
            : QStringLiteral("standalone · %1").arg(file_name);
    entries.append({QStringLiteral("Mission: %1").arg(title),
                    detail,
                    QStringLiteral("Mission"),
                    [this, path]() {
                      if (maybe_save() && load_file(path)) {
                        m_anchor_site.clear();
                        set_workspace(Workspace::Mission);
                      }
                    }});
  }
  QDirIterator maps(root.filePath(QStringLiteral("assets/maps")),
                    {QStringLiteral("*.json")},
                    QDir::Files);
  while (maps.hasNext()) {
    const QString path = maps.next();
    entries.append({QStringLiteral("Map: %1").arg(
                        QFileInfo(path)
                            .baseName()
                            .remove(QStringLiteral("map_"))
                            .replace(QLatin1Char('_'), QLatin1Char(' '))),
                    QFileInfo(path).fileName(),
                    QStringLiteral("Map"),
                    [this, path]() {
                      if (maybe_save() && load_file(path)) {
                        m_anchor_site.clear();
                        set_workspace(Workspace::Battlefield);
                      }
                    }});
  }
  if (m_campaign != nullptr && m_campaign->has_world()) {
    const auto& def = m_campaign->document()->definition();
    for (const auto& site : def.sites) {
      entries.append(
          {site.name,
           QStringLiteral("%1 in %2%3")
               .arg(Game::Strategic::to_string(site.kind),
                    def.display_name(site.region),
                    site.battlefield.empty() ? QString()
                                             : QStringLiteral(" · battlefield")),
           QStringLiteral("Site"),
           [this, id = site.id]() {
             set_workspace(Workspace::Campaign);
             m_campaign->select_entity(id);
           }});
    }
    for (const auto& region : def.regions) {
      entries.append({region.name,
                      QStringLiteral("region"),
                      QStringLiteral("Region"),
                      [this, id = region.id]() {
                        set_workspace(Workspace::Campaign);
                        m_campaign->select_entity(id);
                      }});
    }
    for (const auto& route : def.routes) {
      if (route.name.isEmpty()) {
        continue;
      }
      entries.append({route.name,
                      Game::Strategic::to_string(route.kind),
                      QStringLiteral("Route"),
                      [this, id = route.id]() {
                        set_workspace(Workspace::Campaign);
                        m_campaign->select_entity(id);
                      }});
    }
    for (const auto& army : m_campaign->document()->state().armies) {
      entries.append({army.name,
                      QStringLiteral("army at %1").arg(def.display_name(army.location)),
                      QStringLiteral("Army"),
                      [this, id = army.id]() {
                        set_workspace(Workspace::Campaign);
                        m_campaign->select_entity(id);
                      }});
    }
    for (const auto& event : def.events) {
      entries.append({event.name,
                      event.date_label(),
                      QStringLiteral("Event"),
                      [this, id = event.id]() {
                        set_workspace(Workspace::Campaign);
                        m_campaign->set_timeline(id);
                      }});
    }
  }
  return entries;
}

void EditorWindow::open_command_palette() {
  m_palette->set_entries(palette_entries());
  m_palette->open_with();
}

void EditorWindow::reset_to_new_map() {
  m_map_data->clear();
  set_mission_mode(false);
  m_current_file_path.clear();
  m_linked_map_file_path.clear();
  update_window_title();
  update_current_file_label();
}

void EditorWindow::new_map() {
  if (!maybe_save()) {
    return;
  }

  reset_to_new_map();
  m_anchor_site.clear();
  set_workspace(Workspace::Battlefield);
  show_action_feedback("Created a new unsaved map.");
}

void EditorWindow::new_mission() {
  if (!maybe_save()) {
    return;
  }

  m_mission_data->clear();
  m_map_data->clear();
  set_mission_mode(true);
  m_current_file_path.clear();
  m_linked_map_file_path.clear();
  QString map_error;
  load_linked_map(m_mission_data->map_path(), &map_error);
  update_window_title();
  update_current_file_label();
  m_sidebar_tabs->setCurrentWidget(m_mission_scroll);
  m_anchor_site.clear();
  set_workspace(Workspace::Mission);
  show_action_feedback(QStringLiteral("Created a new unsaved mission."));
}

void EditorWindow::open_map() {
  if (!maybe_save()) {
    return;
  }

  const QString file_path =
      QFileDialog::getOpenFileName(this,
                                   "Open Map",
                                   default_map_dialog_path(QString()),
                                   "JSON Files (*.json);;All Files (*)");

  if (file_path.isEmpty()) {
    return;
  }

  load_file(file_path);
}

bool EditorWindow::load_file(const QString& file_path) {
  QFile probe(file_path);
  if (!probe.open(QIODevice::ReadOnly)) {
    show_load_failure(file_path, probe.errorString());
    return false;
  }
  QJsonParseError parse_error;
  const QJsonDocument probe_document =
      QJsonDocument::fromJson(probe.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !probe_document.isObject()) {
    show_load_failure(file_path, parse_error.errorString());
    return false;
  }
  const QJsonObject probe_root = probe_document.object();
  const bool is_mission = probe_root.contains(QStringLiteral("map_path")) &&
                          probe_root.contains(QStringLiteral("player_setup"));

  remember_dialog_directory(file_path);

  QString error_message;
  if (is_mission) {
    if (!m_mission_data->load_from_json(file_path, &error_message)) {
      show_load_failure(file_path, error_message);
      return false;
    }
    set_mission_mode(true);
    m_current_file_path = QFileInfo(file_path).absoluteFilePath();
    QString map_error;
    if (!load_linked_map(m_mission_data->map_path(), &map_error)) {
      show_action_feedback(
          QStringLiteral("Mission loaded; linked map failed: ") + map_error, false);
      QMessageBox::warning(
          this,
          QStringLiteral("Linked Map Missing"),
          QStringLiteral(
              "The mission loaded, but its battlefield could not be opened:\n%1")
              .arg(map_error));
    } else {
      show_action_feedback(
          QStringLiteral("Loaded mission \"%1\" from %2")
              .arg(m_mission_data->title(), normalizedDisplayPath(file_path)));
    }
    update_window_title();
    update_current_file_label();
    m_sidebar_tabs->setCurrentWidget(m_mission_scroll);
    m_anchor_site.clear();
    ensure_campaign_context_world();
    set_workspace(Workspace::Mission);
    return true;
  }

  if (m_map_data->load_from_json(file_path, &error_message)) {
    set_mission_mode(false);
    m_current_file_path = QFileInfo(file_path).absoluteFilePath();
    m_linked_map_file_path.clear();
    m_canvas->clear_selection();
    m_canvas->zoom_to_fit();
    update_window_title();
    update_current_file_label();
    show_action_feedback(
        QString("Loaded \"%1\" from %2")
            .arg(m_map_data->name(), normalizedDisplayPath(file_path)));
    m_anchor_site.clear();
    ensure_campaign_context_world();
    set_workspace(Workspace::Battlefield);
    return true;
  }
  show_load_failure(file_path, error_message);
  return false;
}

void EditorWindow::save_map() {
  if (m_workspace == Workspace::Campaign && m_campaign != nullptr) {
    m_campaign->save();
    return;
  }
  if (m_current_file_path.isEmpty()) {
    save_map_as();
  } else if (m_mission_mode) {
    save_mission_to_path(m_current_file_path, true);
  } else {
    save_map_to_path(m_current_file_path, true);
  }
}

void EditorWindow::save_map_as() {
  if (m_workspace == Workspace::Campaign && m_campaign != nullptr) {
    m_campaign->save_as();
    return;
  }
  QString suggested_name =
      m_mission_mode ? m_mission_data->id().trimmed() : m_map_data->name().trimmed();
  if (suggested_name.isEmpty()) {
    suggested_name = m_mission_mode ? "untitled_mission" : "untitled_map";
  }
  suggested_name.replace(' ', '_');
  suggested_name = suggested_name.toLower();

  QString initial_path;
  if (m_mission_mode && m_current_file_path.isEmpty()) {
    const QDir mission_dir(repository_root() + QStringLiteral("/assets/missions"));
    initial_path =
        mission_dir.exists()
            ? mission_dir.filePath(suggested_name + QStringLiteral(".json"))
            : default_map_dialog_path(suggested_name + QStringLiteral(".json"));
  } else {
    initial_path = default_map_dialog_path(suggested_name + QStringLiteral(".json"));
  }
  QString file_path =
      QFileDialog::getSaveFileName(this,
                                   m_mission_mode ? QStringLiteral("Save Mission As")
                                                  : QStringLiteral("Save Map As"),
                                   initial_path,
                                   "JSON Files (*.json);;All Files (*)");

  if (file_path.isEmpty()) {
    show_action_feedback("Save As cancelled.", false);
    return;
  }

  if (!file_path.endsWith(".json", Qt::CaseInsensitive)) {
    file_path += ".json";
  }

  if (m_mission_mode) {
    save_mission_to_path(file_path, true);
  } else {
    save_map_to_path(file_path, true);
  }
}

void EditorWindow::resize_map() {
  const GridSettings& grid = m_map_data->grid();
  ResizeDialog dialog(grid.width, grid.height, this);

  if (dialog.exec() == QDialog::Accepted) {
    GridSettings new_grid = grid;
    new_grid.width = dialog.new_width();
    new_grid.height = dialog.new_height();
    m_map_data->execute_command(
        std::make_unique<ResizeMapCmd>(m_map_data, grid, new_grid));
    m_canvas->update();
    show_action_feedback(
        QString("Resized map to %1 x %2").arg(new_grid.width).arg(new_grid.height));
  }
}

void EditorWindow::edit_biome() {
  const QJsonObject before = m_map_data->biome();
  JsonEditDialog dialog("Edit Biome", before, false, schema_for_biome(), this);
  if (dialog.exec() == QDialog::Accepted && dialog.is_valid()) {
    m_map_data->execute_command(
        std::make_unique<UpdateBiomeCmd>(m_map_data, before, dialog.get_json()));
    show_action_feedback("Biome settings updated.");
  }
}

void EditorWindow::undo() {
  if (m_workspace == Workspace::Campaign && m_campaign != nullptr) {
    m_campaign->undo();
    return;
  }
  const QString desc = m_map_data->undo_description();
  m_map_data->undo();
  m_canvas->clear_selection();
  show_action_feedback(desc.isEmpty() ? "Undo complete." : "Undid: " + desc);
}

void EditorWindow::redo() {
  if (m_workspace == Workspace::Campaign && m_campaign != nullptr) {
    m_campaign->redo();
    return;
  }
  const QString desc = m_map_data->redo_description();
  m_map_data->redo();
  m_canvas->clear_selection();
  show_action_feedback(desc.isEmpty() ? "Redo complete." : "Redid: " + desc);
}

void EditorWindow::on_tool_selected(ToolType tool) {
  m_canvas->set_current_tool(tool);
  m_tool_status_text = "Tool: " + tool_status_label(tool);
  m_selection_status_text.clear();
  refresh_status_label();
}

void EditorWindow::on_tool_cleared() {
  m_tool_panel->clear_selection();
  m_tool_status_text = "Tool: Select";
  m_selection_status_text.clear();
  refresh_status_label();
}

void EditorWindow::on_grid_double_clicked() {
  resize_map();
}

void EditorWindow::on_undo_redo_changed() {
  if (m_undo_action == nullptr) {
    return;
  }
  if (m_workspace == Workspace::Campaign && m_campaign != nullptr) {
    m_undo_action->setEnabled(m_campaign->document()->can_undo());
    m_redo_action->setEnabled(m_campaign->document()->can_redo());
    m_undo_action->setText("&Undo");
    m_redo_action->setText("&Redo");
    return;
  }
  const bool can_undo = m_map_data->can_undo();
  const bool can_redo = m_map_data->can_redo();

  m_undo_action->setEnabled(can_undo);
  m_redo_action->setEnabled(can_redo);

  const QString undo_desc = m_map_data->undo_description();
  m_undo_action->setText(undo_desc.isEmpty() ? "&Undo" : "&Undo " + undo_desc);

  const QString redo_desc = m_map_data->redo_description();
  m_redo_action->setText(redo_desc.isEmpty() ? "&Redo" : "&Redo " + redo_desc);
}

void EditorWindow::update_dimensions_label() {
  const GridSettings& grid = m_map_data->grid();
  m_dimensions_label->setText(QString("Map: %1 x %2").arg(grid.width).arg(grid.height));
}

void EditorWindow::on_element_double_clicked(int element_type, int index) {
  const ElementSnapshot before = ElementOps::snapshot(*m_map_data, element_type, index);
  const auto document = ElementEditJson::to_document(before);
  if (!document.has_value()) {
    return;
  }

  const HillProjection::MapContext map_context{
      .tile_size = static_cast<double>(m_map_data->grid().tile_size),
      .map_grid_width = m_map_data->grid().width,
      .map_grid_height = m_map_data->grid().height};
  JsonEditDialog dialog(document->title,
                        document->json,
                        document->hill_projection,
                        schema_for_element(element_type, document->schema_sub_type),
                        this,
                        map_context);
  if (dialog.exec() != QDialog::Accepted || !dialog.is_valid()) {
    return;
  }

  const ElementEditJson::EditResult result = ElementEditJson::from_json(
      before, dialog.get_json(), m_map_data->linear_elements());
  for (const QString& note : result.notes) {
    show_action_feedback(note, false);
  }
  if (auto command = ElementOps::make_update(
          *m_map_data, index, before, result.element, result.description)) {
    m_map_data->execute_command(std::move(command));
    if (ElementOps::is_generated(before)) {
      show_action_feedback(QStringLiteral(
          "Converted the generated element to authored; a reroll will keep it."));
    }
  }
}

void EditorWindow::select_sidebar_tab(const QString& title) {
  for (int index = 0; index < m_sidebar_tabs->count(); ++index) {
    if (m_sidebar_tabs->tabText(index).compare(title, Qt::CaseInsensitive) == 0) {
      m_sidebar_tabs->setCurrentIndex(index);
      return;
    }
  }
}

void EditorWindow::on_modified_changed(bool modified) {
  Q_UNUSED(modified)
  update_window_title();
}

void EditorWindow::update_current_file_label() {
  if (m_file_label == nullptr) {
    return;
  }

  if (m_current_file_path.isEmpty()) {
    m_file_label->setText("File: unsaved");
    m_file_label->setToolTip(m_mission_mode ? "This mission has not been saved yet."
                                            : "This map has not been saved yet.");
    return;
  }

  const QString display_path = normalizedDisplayPath(m_current_file_path);
  m_file_label->setText("File: " + display_path);
  m_file_label->setToolTip(display_path);
}

void EditorWindow::show_action_feedback(const QString& message, bool success) {
  if (m_feedback_label == nullptr) {
    return;
  }

  m_feedback_label->setText(message);
  m_feedback_label->setProperty("status", success ? "success" : "error");
  m_feedback_label->style()->unpolish(m_feedback_label);
  m_feedback_label->style()->polish(m_feedback_label);
  m_feedback_label->setToolTip(message);
}

void EditorWindow::show_load_failure(const QString& file_path,
                                     const QString& error_message) {
  const QString display_path = normalizedDisplayPath(file_path);
  const QString detail = error_message.trimmed().isEmpty()
                             ? QString("Unable to load the JSON file.")
                             : error_message;
  show_action_feedback("Load failed: " + display_path, false);
  QMessageBox::critical(
      this,
      "Load Failed",
      QString("Could not load file:\n%1\n\nReason: %2").arg(display_path, detail));
}

void EditorWindow::show_save_failure(const QString& file_path,
                                     const QString& error_message) {
  const QString display_path = normalizedDisplayPath(file_path);
  const QString detail = error_message.trimmed().isEmpty()
                             ? QString("Unable to write the map file.")
                             : error_message;
  show_action_feedback("Save failed: " + display_path, false);
  QMessageBox::critical(
      this,
      "Save Failed",
      QString("Could not save \"%1\" to:\n%2\n\nReason: %3")
          .arg(m_mission_mode ? m_mission_data->title() : m_map_data->name(),
               display_path,
               detail));
}

QString EditorWindow::default_map_dialog_path(const QString& fallback_name) const {
  if (!m_current_file_path.isEmpty()) {
    return m_current_file_path;
  }

  const QString remembered = load_last_dialog_directory();
  if (!remembered.isEmpty() && QDir(remembered).exists()) {
    return fallback_name.isEmpty() ? remembered
                                   : QDir(remembered).filePath(fallback_name);
  }

  const QDir repo_maps_dir(
      QDir(repository_root()).filePath(QStringLiteral("assets/maps")));
  if (repo_maps_dir.exists()) {
    return fallback_name.isEmpty() ? repo_maps_dir.absolutePath()
                                   : repo_maps_dir.filePath(fallback_name);
  }

  if (!fallback_name.isEmpty()) {
    return QDir::current().filePath(fallback_name);
  }

  return QDir::currentPath();
}

QString EditorWindow::load_last_dialog_directory() {
  return editor_settings().value(k_last_dialog_directory_key).toString();
}

void EditorWindow::remember_dialog_directory(const QString& file_path) {
  const QString directory = QFileInfo(file_path).absolutePath();
  if (directory.isEmpty()) {
    return;
  }
  editor_settings().setValue(k_last_dialog_directory_key, directory);
}

bool EditorWindow::save_map_to_path(const QString& file_path,
                                    bool update_current_path) {
  const QString absolute_path = QFileInfo(file_path).absoluteFilePath();
  QString error_message;
  if (!m_map_data->save_to_json(absolute_path, &error_message)) {
    show_save_failure(absolute_path, error_message);
    return false;
  }

  if (update_current_path) {
    m_current_file_path = absolute_path;
  }

  remember_dialog_directory(absolute_path);
  m_map_data->set_modified(false);
  update_window_title();
  update_current_file_label();
  show_action_feedback(
      QString("Saved \"%1\" to %2")
          .arg(m_map_data->name(), normalizedDisplayPath(absolute_path)));
  return true;
}

bool EditorWindow::save_mission_to_path(const QString& file_path,
                                        bool update_current_path) {
  const QString absolute_path = QFileInfo(file_path).absoluteFilePath();
  QString error_message;

  if (m_map_data->is_modified()) {
    if (m_linked_map_file_path.isEmpty()) {
      show_save_failure(
          absolute_path,
          QStringLiteral("The linked battlefield has changes but no writable path."));
      return false;
    }
    if (!m_map_data->save_to_json(m_linked_map_file_path, &error_message)) {
      show_save_failure(m_linked_map_file_path, error_message);
      return false;
    }
    m_map_data->set_modified(false);
  }

  if (!m_mission_data->save_to_json(absolute_path, &error_message)) {
    show_save_failure(absolute_path, error_message);
    return false;
  }
  if (update_current_path) {
    m_current_file_path = absolute_path;
  }
  remember_dialog_directory(absolute_path);
  m_mission_data->set_modified(false);
  update_window_title();
  update_current_file_label();
  show_action_feedback(
      QStringLiteral("Saved mission \"%1\" to %2")
          .arg(m_mission_data->title(), normalizedDisplayPath(absolute_path)));
  return true;
}

bool EditorWindow::save_current_document() {
  save_map();
  return m_mission_mode
             ? !m_mission_data->is_modified() && !m_map_data->is_modified() &&
                   !m_current_file_path.isEmpty()
             : !m_map_data->is_modified() && !m_current_file_path.isEmpty();
}

QString EditorWindow::repository_root() const {
  QStringList starts = {QDir::currentPath(), QCoreApplication::applicationDirPath()};
  if (!m_current_file_path.isEmpty()) {
    starts.prepend(QFileInfo(m_current_file_path).absolutePath());
  }
  const QString root = Generator::find_repository_root(starts);
  return root.isEmpty() ? QDir::currentPath() : root;
}

QString EditorWindow::resolve_authored_path(const QString& authored_path) const {
  const QString trimmed = authored_path.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }
  if (QFileInfo(trimmed).isAbsolute() && !trimmed.startsWith(":/")) {
    return QFileInfo(trimmed).absoluteFilePath();
  }

  QString relative = trimmed;
  if (relative.startsWith(":/")) {
    relative = relative.mid(2);
  }
  const QString root_candidate = QDir(repository_root()).filePath(relative);
  if (QFileInfo::exists(root_candidate)) {
    return QFileInfo(root_candidate).absoluteFilePath();
  }
  if (!m_current_file_path.isEmpty()) {
    const QString mission_relative =
        QDir(QFileInfo(m_current_file_path).absolutePath()).filePath(relative);
    if (QFileInfo::exists(mission_relative)) {
      return QFileInfo(mission_relative).absoluteFilePath();
    }
  }
  return QFileInfo(root_candidate).absoluteFilePath();
}

bool EditorWindow::load_linked_map(const QString& authored_path, QString* out_error) {
  const QString resolved = resolve_authored_path(authored_path);
  if (resolved.isEmpty() || !QFileInfo::exists(resolved)) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Map '%1' does not exist.").arg(authored_path);
    }
    return false;
  }
  QString error;
  if (!m_map_data->load_from_json(resolved, &error)) {
    if (out_error != nullptr) {
      *out_error = error;
    }
    return false;
  }
  m_linked_map_file_path = resolved;
  m_canvas->clear_selection();
  m_canvas->zoom_to_fit();
  m_mission_panel->refresh();
  return true;
}

QString EditorWindow::tool_executable(const QString& name) const {
#ifdef Q_OS_WIN
  const QString executable_name = name + QStringLiteral(".exe");
#else
  const QString& executable_name = name;
#endif
  const QString alongside =
      QDir(QCoreApplication::applicationDirPath()).filePath(executable_name);
  if (QFileInfo::exists(alongside)) {
    return alongside;
  }
  const QString root = repository_root();
  const QStringList build_dirs = {QStringLiteral("build-debug/bin"),
                                  QStringLiteral("build/bin"),
                                  QStringLiteral("build-release/bin")};
  for (const QString& build_dir : build_dirs) {
    const QString candidate =
        QDir(root).filePath(build_dir + QLatin1Char('/') + executable_name);
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
  }
  return {};
}

bool EditorWindow::validate_current_mission(bool show_success) {
  if (!m_mission_mode) {
    show_action_feedback(QStringLiteral("Open or create a mission first."), false);
    return false;
  }
  QStringList errors = m_mission_data->validate();
  const QString resolved_map = resolve_authored_path(m_mission_data->map_path());
  if (!QFileInfo::exists(resolved_map)) {
    errors.append(QStringLiteral("Linked battlefield map does not exist: %1")
                      .arg(m_mission_data->map_path()));
  }
  if (!errors.isEmpty()) {
    show_action_feedback(QStringLiteral("Mission validation failed with %1 issue(s).")
                             .arg(errors.size()),
                         false);
    QMessageBox::critical(
        this,
        QStringLiteral("Mission Validation Failed"),
        errors.join(QStringLiteral("\n• ")).prepend(QStringLiteral("• ")));
    return false;
  }

  if (!m_current_file_path.isEmpty()) {
    const QString validator = tool_executable(QStringLiteral("content_validator"));
    if (!validator.isEmpty()) {
      QProcess process;
      process.setWorkingDirectory(repository_root());
      process.start(validator, {QStringLiteral("--mission"), m_current_file_path});
      if (!process.waitForFinished(30000) ||
          process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString output = QString::fromUtf8(process.readAllStandardError()) +
                               QString::fromUtf8(process.readAllStandardOutput());
        show_action_feedback(QStringLiteral("content_validator rejected the mission."),
                             false);
        QMessageBox::critical(
            this, QStringLiteral("Content Validation Failed"), output.trimmed());
        return false;
      }
    }
  }

  if (show_success) {
    show_action_feedback(QStringLiteral("Mission and linked battlefield are valid."));
    QMessageBox::information(
        this,
        QStringLiteral("Mission Valid"),
        QStringLiteral(
            "The mission uses supported runtime options and passed validation."));
  }
  return true;
}

void EditorWindow::validate_mission() {
  validate_current_mission(true);
}

void EditorWindow::launch_mission_game() {
  if (!m_mission_mode || !save_current_document() || !validate_current_mission(false)) {
    return;
  }
  const QString game = tool_executable(QStringLiteral("standard_of_iron"));
  if (game.isEmpty()) {
    QMessageBox::warning(
        this,
        QStringLiteral("Game Not Built"),
        QStringLiteral("Build standard_of_iron before launching a mission preview."));
    return;
  }
  if (!QProcess::startDetached(game,
                               {QStringLiteral("--mission-file"), m_current_file_path},
                               repository_root())) {
    show_action_feedback(QStringLiteral("Could not launch the game."), false);
    return;
  }
  show_action_feedback(QStringLiteral("Mission launched in the game."));
}

void EditorWindow::launch_mission_arena() {
  if (!m_mission_mode || !save_current_document() || !validate_current_mission(false)) {
    return;
  }
  const QString arena = tool_executable(QStringLiteral("arena_app"));
  if (arena.isEmpty()) {
    QMessageBox::warning(
        this,
        QStringLiteral("Arena Not Built"),
        QStringLiteral("Build arena_app before launching a battlefield preview."));
    return;
  }
  const QString map_path = resolve_authored_path(m_mission_data->map_path());
  if (!QProcess::startDetached(
          arena, {QStringLiteral("--terrain-map"), map_path}, repository_root())) {
    show_action_feedback(QStringLiteral("Could not launch Arena."), false);
    return;
  }
  show_action_feedback(QStringLiteral("Linked battlefield launched in Arena."));
}

void EditorWindow::on_mission_map_path_changed(const QString& map_path) {
  if (!m_mission_mode) {
    return;
  }
  const QString resolved = resolve_authored_path(map_path);
  if (resolved == m_linked_map_file_path) {
    return;
  }
  if (m_map_data->is_modified() && !m_linked_map_file_path.isEmpty()) {
    const auto choice = QMessageBox::question(
        this,
        QStringLiteral("Save Battlefield Changes"),
        QStringLiteral(
            "Save changes to the current linked battlefield before switching?"),
        QMessageBox::Save | QMessageBox::Discard,
        QMessageBox::Save);
    if (choice == QMessageBox::Save) {
      QString error;
      if (!m_map_data->save_to_json(m_linked_map_file_path, &error)) {
        show_save_failure(m_linked_map_file_path, error);
        return;
      }
    }
  }
  QString error;
  if (!load_linked_map(map_path, &error)) {
    show_action_feedback(QStringLiteral("Could not load linked map: ") + error, false);
  } else {
    show_action_feedback(QStringLiteral("Loaded linked battlefield %1")
                             .arg(normalizedDisplayPath(m_linked_map_file_path)));
  }
}

void EditorWindow::set_mission_mode(bool enabled) {
  m_mission_mode = enabled;
  if (m_canvas != nullptr) {
    m_canvas->set_mission_data(enabled ? m_mission_data : nullptr);
  }
  if (m_sidebar_tabs != nullptr && m_mission_scroll != nullptr) {
    const int index = m_sidebar_tabs->indexOf(m_mission_scroll);
    if (index >= 0) {
      m_sidebar_tabs->setTabVisible(index, enabled);
    }
  }
  refresh_json_preview();
  update_window_title();
}

void EditorWindow::update_window_title() {
  QString title;
  if (m_workspace == Workspace::Campaign && m_campaign != nullptr) {
    const auto* doc = m_campaign->document();
    title = QStringLiteral("%1%2 — Campaign — Standard of Iron Editor")
                .arg(doc->path().isEmpty() ? doc->definition().name
                                           : QFileInfo(doc->path()).fileName(),
                     doc->is_modified() ? QStringLiteral(" •") : QString());
  } else {
    const QString name =
        !m_current_file_path.isEmpty()
            ? QFileInfo(m_current_file_path).fileName()
            : (m_mission_mode ? m_mission_data->title() : m_map_data->name());
    const bool modified =
        m_map_data->is_modified() || (m_mission_mode && m_mission_data->is_modified());
    title = QStringLiteral("%1%2 — %3 — Standard of Iron Editor")
                .arg(name,
                     modified ? QStringLiteral(" •") : QString(),
                     m_workspace == Workspace::Mission ? QStringLiteral("Mission")
                                                       : QStringLiteral("Battlefield"));
  }
  setWindowTitle(title);
  refresh_workspace_bar();
}

bool EditorWindow::maybe_save() {
  if (!m_map_data->is_modified() &&
      (!m_mission_mode || !m_mission_data->is_modified())) {
    return true;
  }

  QMessageBox::StandardButton const ret = QMessageBox::warning(
      this,
      "Unsaved Changes",
      m_mission_mode ? "The mission or linked battlefield has been modified.\n"
                       "Do you want to save your changes?"
                     : "The map has been modified.\nDo you want to save your changes?",
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

  if (ret == QMessageBox::Save) {
    return save_current_document();
  }
  if (ret == QMessageBox::Cancel) {
    return false;
  }
  return true;
}

void EditorWindow::closeEvent(QCloseEvent* event) {
  if (m_campaign != nullptr && !m_campaign->maybe_save()) {
    event->ignore();
    return;
  }
  if (maybe_save()) {
    event->accept();
  } else {
    event->ignore();
  }
}

void EditorWindow::on_selection_changed(int element_type, int index) {
  if (element_type < 0 || index < 0 || (m_map_data == nullptr)) {
    m_selection_status_text.clear();
    refresh_status_label();
    return;
  }

  QString type_name;
  QString coords;
  if (element_type == 0) {
    const auto& terrain = m_map_data->terrain_elements();
    if (index < terrain.size()) {
      const auto& e = terrain[index];
      type_name = e.type;
      coords =
          QString("(%1, %2)").arg(static_cast<int>(e.x)).arg(static_cast<int>(e.z));
    }
  } else if (element_type == 1) {
    const auto& world_props = m_map_data->world_props();
    if (index < world_props.size()) {
      const auto& e = world_props[index];
      type_name = e.type;
      coords =
          QString("(%1, %2)").arg(static_cast<int>(e.x)).arg(static_cast<int>(e.z));
    }
  } else if (element_type == 2) {
    const auto& linear = m_map_data->linear_elements();
    if (index < linear.size()) {
      const auto& e = linear[index];
      type_name = e.type;
      coords = QString("(%1,%2)→(%3,%4)")
                   .arg(static_cast<int>(e.start.x()))
                   .arg(static_cast<int>(e.start.y()))
                   .arg(static_cast<int>(e.end.x()))
                   .arg(static_cast<int>(e.end.y()));
    }
  } else if (element_type == 3) {
    const auto& structures = m_map_data->structures();
    if (index < structures.size()) {
      const auto& e = structures[index];
      type_name = e.type;
      coords = e.type == QStringLiteral("wall_gate")
                   ? QString("(%1, %2) P%3 %4°")
                         .arg(static_cast<int>(e.x))
                         .arg(static_cast<int>(e.z))
                         .arg(e.player_id)
                         .arg(static_cast<int>(e.rotation))
                   : QString("(%1, %2) P%3")
                         .arg(static_cast<int>(e.x))
                         .arg(static_cast<int>(e.z))
                         .arg(e.player_id);
    }
  } else if (element_type == 4) {
    const auto& troop_spawns = m_map_data->troop_spawns();
    if (index < troop_spawns.size()) {
      const auto& e = troop_spawns[index];
      type_name = e.type;
      coords = e.player_id >= 0 ? QString("(%1, %2) P%3")
                                      .arg(static_cast<int>(e.x))
                                      .arg(static_cast<int>(e.z))
                                      .arg(e.player_id)
                                : QString("(%1, %2)")
                                      .arg(static_cast<int>(e.x))
                                      .arg(static_cast<int>(e.z));
    }
  } else if (element_type == 5) {
    const auto& undead_zones = m_map_data->undead_zones();
    if (index < undead_zones.size()) {
      const auto& e = undead_zones[index];
      type_name = "undead_zone";
      coords = QString("(%1, %2) r=%3")
                   .arg(static_cast<int>(e.x))
                   .arg(static_cast<int>(e.z))
                   .arg(static_cast<int>(e.radius));
    }
  } else if (element_type == static_cast<int>(ElementKind::WildlifeArea)) {
    const auto& areas = m_map_data->wildlife_areas();
    if (index < areas.size()) {
      const auto& e = areas[index];
      type_name = wildlife_species_label(e.species);
      coords = QString("(%1, %2) r=%3")
                   .arg(static_cast<int>(e.x))
                   .arg(static_cast<int>(e.z))
                   .arg(static_cast<int>(e.radius));
    }
  } else if (element_type == static_cast<int>(ElementKind::Forest)) {
    const auto& forests = m_map_data->forests();
    if (index < forests.size()) {
      const auto& e = forests[index];
      type_name = QStringLiteral("forest");
      coords = QString("(%1, %2) r=%3")
                   .arg(static_cast<int>(e.x))
                   .arg(static_cast<int>(e.z))
                   .arg(static_cast<int>(e.radius));
    }
  }

  if (!type_name.isEmpty()) {
    m_selection_status_text =
        QString("Selected: %1 at %2")
            .arg(ElementEditJson::prettify_identifier(type_name), coords);
    const int extra = m_canvas->selection_count() - 1;
    if (extra > 0) {
      m_selection_status_text += QString(" (+%1 more)").arg(extra);
    }
  } else {
    m_selection_status_text.clear();
  }
  refresh_status_label();
}

void EditorWindow::update_selection_info() {
  on_selection_changed(m_canvas->selected_element_type(),
                       m_canvas->selected_element_index());
}

void EditorWindow::refresh_status_label() {
  if (m_tool_label == nullptr) {
    return;
  }
  if (m_hint_active) {
    return;
  }
  if (!m_selection_status_text.isEmpty()) {
    m_tool_label->setText(m_selection_status_text);
  } else {
    m_tool_label->setText(m_tool_status_text);
  }
}

void EditorWindow::refresh_json_preview() {
  if (m_json_preview == nullptr) {
    return;
  }
  const QString json =
      m_mission_mode ? m_mission_data->to_json_string() : m_map_data->to_json_string();

  if (m_json_preview->toPlainText() != json) {
    const int scroll_pos = m_json_preview->verticalScrollBar()->value();
    m_json_preview->setPlainText(json);
    m_json_preview->verticalScrollBar()->setValue(scroll_pos);
  }
}

} // namespace MapEditor
