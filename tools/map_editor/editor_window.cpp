#include "editor_window.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
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
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

#include <cmath>

#include "game/map/environment_lighting.h"
#include "game/map/map_definition.h"
#include "json_edit_dialog.h"
#include "json_schema.h"
#include "map_json_keys.h"
#include "resize_dialog.h"
#include "troop_tool_specs.h"

namespace {

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
                         "top-projection entrance grid).\n"
                         "5. Save the map when the layout looks right.",
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
  profile->addItems({"mediterranean_summer", "iron_sepulcher"});
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

auto prettifyIdentifier(const QString& value) -> QString {
  QString label = value;
  label.replace(QLatin1Char('_'), QLatin1Char(' '));
  QStringList parts = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (QString& part : parts) {
    if (!part.isEmpty()) {
      part[0] = part[0].toUpper();
    }
  }
  return parts.join(QLatin1Char(' '));
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

  setWindowTitle("Standard of Iron - Map Editor");
  resize(1400, 900);

  new_map();
}

EditorWindow::~EditorWindow() = default;

void EditorWindow::setup_ui() {
  auto* central_widget = new QWidget(this);
  setCentralWidget(central_widget);

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
  m_sidebar_tabs->setMaximumWidth(460);

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
  splitter->setSizes({1160, 340});
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
  auto* version_label = new QLabel("Mission Editor v2.0", this);
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

  file_menu->addSeparator();

  auto* save_action = new QAction("&Save", this);
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

  auto* test_menu = menuBar()->addMenu("&Test");
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

void EditorWindow::new_map() {
  if (!maybe_save()) {
    return;
  }

  m_map_data->clear();
  set_mission_mode(false);
  m_current_file_path.clear();
  m_linked_map_file_path.clear();
  update_window_title();
  update_current_file_label();
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
    return true;
  }
  show_load_failure(file_path, error_message);
  return false;
}

void EditorWindow::save_map() {
  if (m_current_file_path.isEmpty()) {
    save_map_as();
  } else if (m_mission_mode) {
    save_mission_to_path(m_current_file_path, true);
  } else {
    save_map_to_path(m_current_file_path, true);
  }
}

void EditorWindow::save_map_as() {
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
  const QString desc = m_map_data->undo_description();
  m_map_data->undo();
  m_canvas->clear_selection();
  show_action_feedback(desc.isEmpty() ? "Undo complete." : "Undid: " + desc);
}

void EditorWindow::redo() {
  const QString desc = m_map_data->redo_description();
  m_map_data->redo();
  m_canvas->clear_selection();
  show_action_feedback(desc.isEmpty() ? "Redo complete." : "Redid: " + desc);
}

void EditorWindow::on_tool_selected(ToolType tool) {
  m_canvas->set_current_tool(tool);

  QString tool_name;
  if (const auto* spec = troop_tool_spec(tool)) {
    tool_name = QString::fromLatin1(spec->name);
  }
  switch (tool) {
  case ToolType::Select:
    tool_name = "Select";
    break;
  case ToolType::Hill:
    tool_name = "Hill";
    break;
  case ToolType::Mountain:
    tool_name = "Mountain";
    break;
  case ToolType::River:
    tool_name = "River (click start, then end)";
    break;
  case ToolType::Road:
    tool_name = "Road (click start, then end)";
    break;
  case ToolType::Bridge:
    tool_name = "Bridge (click start, then end)";
    break;
  case ToolType::PropFirecamp:
    tool_name = "Fire Camp";
    break;
  case ToolType::PropTent:
    tool_name = "Tent";
    break;
  case ToolType::PropSupplyCart:
    tool_name = "Supply Cart";
    break;
  case ToolType::PropWeaponRack:
    tool_name = "Weapon Rack";
    break;
  case ToolType::PropRuins:
    tool_name = "Ruins";
    break;
  case ToolType::PropMagicShrine:
    tool_name = "Magic Shrine";
    break;
  case ToolType::PropDeadTree:
    tool_name = "Dead Tree";
    break;
  case ToolType::PropBoulder:
    tool_name = "Boulder";
    break;
  case ToolType::PropPineTree:
    tool_name = "Pine Tree";
    break;
  case ToolType::PropOliveTree:
    tool_name = "Olive Tree";
    break;
  case ToolType::PropPlant:
    tool_name = "Plant";
    break;
  case ToolType::PropIronOre:
    tool_name = "Iron Ore";
    break;
  case ToolType::PropAbandonedHome:
    tool_name = "Abandoned Home";
    break;
  case ToolType::PropStatue:
    tool_name = "Statue";
    break;
  case ToolType::Barracks:
    tool_name = "Barracks";
    break;
  case ToolType::Village:
    tool_name = "Village";
    break;
  case ToolType::DefenseTower:
    tool_name = "Defense Tower";
    break;
  case ToolType::Home:
    tool_name = "Home";
    break;
  case ToolType::Marketplace:
    tool_name = "Marketplace";
    break;
  case ToolType::Temple:
    tool_name = "Temple";
    break;
  case ToolType::Wall:
    tool_name = "Wall (click start, then end)";
    break;
  case ToolType::UndeadZone:
    tool_name = "Undead Zone";
    break;
  case ToolType::Eraser:
    tool_name = "Eraser";
    break;
  case ToolType::TroopArcher:
  case ToolType::TroopSwordsman:
  case ToolType::TroopSpearman:
  case ToolType::TroopHorseSwordsman:
  case ToolType::TroopHorseArcher:
  case ToolType::TroopHorseSpearman:
  case ToolType::TroopHealer:
  case ToolType::TroopCatapult:
  case ToolType::TroopBallista:
  case ToolType::TroopElephant:
  case ToolType::TroopRomanLegionOrganizer:
  case ToolType::TroopRomanVeteranConsul:
  case ToolType::TroopRomanFieldCommander:
  case ToolType::TroopCarthageSpearCommander:
  case ToolType::TroopCarthageBowCommander:
  case ToolType::TroopCarthageSwordCommander:
  case ToolType::TroopSkeletonSwordsman:
  case ToolType::TroopSkeletonArcher:
  case ToolType::TroopGravePriest:
  case ToolType::TroopCivilian:
  case ToolType::TroopBuilder:
    break;
  }

  m_tool_status_text = "Tool: " + tool_name;
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
  QJsonObject json;
  QString title;

  if (element_type == 0) {

    const auto& terrain = m_map_data->terrain_elements();
    if (index < 0 || index >= terrain.size()) {
      return;
    }
    const auto& elem = terrain[index];

    json[MapJsonKeys::type] = elem.type;
    json[MapJsonKeys::x] = static_cast<double>(elem.x);
    json[MapJsonKeys::z] = static_cast<double>(elem.z);
    json[MapJsonKeys::height] = static_cast<double>(elem.height);
    json[MapJsonKeys::rotation] = static_cast<double>(elem.rotation);
    const QString terrain_type = elem.type.trimmed().toLower();
    const bool is_mountain = terrain_type == QStringLiteral("mountain");
    if (elem.width > 0.0F) {
      json[MapJsonKeys::width] = static_cast<double>(elem.width);
    }
    if (elem.depth > 0.0F) {
      json[MapJsonKeys::depth] = static_cast<double>(elem.depth);
    }
    if (elem.radius > 0.0F && (elem.width <= 0.0F || elem.depth <= 0.0F)) {
      json[MapJsonKeys::radius] = static_cast<double>(elem.radius);
    }
    if (!is_mountain && !elem.entrances.isEmpty()) {
      json[MapJsonKeys::entrances] = elem.entrances;
    }
    for (const QString& key : elem.extra_fields.keys()) {
      json[key] = elem.extra_fields[key];
    }

    title = "Edit Terrain: " + elem.type;
  } else if (element_type == 1) {
    const auto& world_props = m_map_data->world_props();
    if (index < 0 || index >= world_props.size()) {
      return;
    }
    const auto& elem = world_props[index];

    json[MapJsonKeys::type] = elem.type;
    json[MapJsonKeys::x] = static_cast<double>(elem.x);
    json[MapJsonKeys::z] = static_cast<double>(elem.z);
    if (elem.type == QStringLiteral("firecamp")) {
      json[MapJsonKeys::intensity] = static_cast<double>(elem.intensity);
      json[MapJsonKeys::radius] = static_cast<double>(elem.radius);
      json[MapJsonKeys::persistent] = elem.persistent;
    } else {
      json[MapJsonKeys::scale] = static_cast<double>(elem.scale);
      json[MapJsonKeys::rotation] = static_cast<double>(elem.rotation);
    }
    for (const QString& key : elem.extra_fields.keys()) {
      json[key] = elem.extra_fields[key];
    }

    title = "Edit Prop: " + prettifyIdentifier(elem.type);
  } else if (element_type == 2) {

    const auto& linear = m_map_data->linear_elements();
    if (index < 0 || index >= linear.size()) {
      return;
    }
    const auto& elem = linear[index];

    json[MapJsonKeys::type] = elem.type;
    json[MapJsonKeys::start] = QJsonArray{static_cast<double>(elem.start.x()),
                                          static_cast<double>(elem.start.y())};
    json[MapJsonKeys::end] = QJsonArray{static_cast<double>(elem.end.x()),
                                        static_cast<double>(elem.end.y())};
    json[MapJsonKeys::width] = static_cast<double>(elem.width);
    if (!elem.waypoints.isEmpty()) {
      json[MapJsonKeys::waypoints] = waypoints_to_json(elem.waypoints);
    }
    if (elem.type == "bridge") {
      json[MapJsonKeys::height] = static_cast<double>(elem.height);
    }
    if (elem.type == "road" && !elem.style.isEmpty()) {
      json[MapJsonKeys::style] = elem.style;
    }
    if (elem.type == "wall") {
      json[MapJsonKeys::player_id] = elem.player_id;
      if (!elem.nation.isEmpty()) {
        json[MapJsonKeys::nation] = elem.nation;
      }
    }
    for (const QString& key : elem.extra_fields.keys()) {
      json[key] = elem.extra_fields[key];
    }

    title = "Edit " + elem.type;
  } else if (element_type == 3) {

    const auto& structures = m_map_data->structures();
    if (index < 0 || index >= structures.size()) {
      return;
    }
    const auto& elem = structures[index];

    json[MapJsonKeys::type] = elem.type;
    json[MapJsonKeys::x] = static_cast<double>(elem.x);
    json[MapJsonKeys::z] = static_cast<double>(elem.z);
    json[MapJsonKeys::player_id] = elem.player_id;
    if (elem.max_population != 100) {
      json[MapJsonKeys::max_population] = elem.max_population;
    }
    if (!elem.nation.isEmpty()) {
      json[MapJsonKeys::nation] = elem.nation;
    }
    for (const QString& key : elem.extra_fields.keys()) {
      json[key] = elem.extra_fields[key];
    }

    title = "Edit " + elem.type;
  } else if (element_type == 4) {
    const auto& troop_spawns = m_map_data->troop_spawns();
    if (index < 0 || index >= troop_spawns.size()) {
      return;
    }
    const auto& elem = troop_spawns[index];

    json[MapJsonKeys::type] = elem.type;
    json[MapJsonKeys::x] = static_cast<double>(elem.x);
    json[MapJsonKeys::z] = static_cast<double>(elem.z);
    if (elem.player_id >= 0) {
      json[MapJsonKeys::player_id] = elem.player_id;
    }
    if (elem.max_population >= 0) {
      json[MapJsonKeys::max_population] = elem.max_population;
    }
    if (!elem.nation.isEmpty()) {
      json[MapJsonKeys::nation] = elem.nation;
    }
    if (!elem.behavior.isEmpty()) {
      json[MapJsonKeys::behavior] = elem.behavior;
    }
    if (elem.guard_radius != 10.0F) {
      json[MapJsonKeys::guard_radius] = static_cast<double>(elem.guard_radius);
    }
    if (!elem.patrol_waypoints.isEmpty()) {
      json[MapJsonKeys::patrol_waypoints] = elem.patrol_waypoints;
    }
    for (const QString& key : elem.extra_fields.keys()) {
      json[key] = elem.extra_fields[key];
    }

    title = "Edit Troop: " + prettifyIdentifier(elem.type);
  } else if (element_type == 5) {
    const auto& undead_zones = m_map_data->undead_zones();
    if (index < 0 || index >= undead_zones.size()) {
      return;
    }
    const auto& elem = undead_zones[index];

    json["id"] = elem.id;
    json["anchor_type"] = elem.anchor_type;
    json[MapJsonKeys::x] = static_cast<double>(elem.x);
    json[MapJsonKeys::z] = static_cast<double>(elem.z);
    json[MapJsonKeys::radius] = static_cast<double>(elem.radius);
    json["leash_radius"] = static_cast<double>(elem.leash_radius);
    json["owner_id"] = elem.owner_id;
    json["team_id"] = elem.team_id;
    if (!elem.awaken_on.isEmpty()) {
      json["awaken_on"] = elem.awaken_on;
    }
    if (!elem.waves.isEmpty()) {
      json["waves"] = elem.waves;
    }

    title = "Edit Undead Zone: " + elem.id;
  } else {
    return;
  }

  const QString terrain_type =
      json.value(MapJsonKeys::type).toString().trimmed().toLower();
  const bool enable_hill_projection =
      (element_type == 0 && (terrain_type == "hill" || terrain_type == "mountain"));
  const QString sub_type = element_type == 5
                               ? json.value(QStringLiteral("anchor_type")).toString()
                               : json.value(MapJsonKeys::type).toString();
  const HillProjection::MapContext map_context{
      .tile_size = static_cast<double>(m_map_data->grid().tile_size),
      .map_grid_width = m_map_data->grid().width,
      .map_grid_height = m_map_data->grid().height};
  JsonEditDialog dialog(title,
                        json,
                        enable_hill_projection,
                        schema_for_element(element_type, sub_type),
                        this,
                        map_context);
  if (dialog.exec() == QDialog::Accepted && dialog.is_valid()) {
    QJsonObject new_json = dialog.get_json();

    if (element_type == 0) {
      TerrainElement elem;
      elem.type = new_json[MapJsonKeys::type].toString();
      elem.x = static_cast<float>(new_json[MapJsonKeys::x].toDouble());
      elem.z = static_cast<float>(new_json[MapJsonKeys::z].toDouble());
      elem.radius = static_cast<float>(new_json[MapJsonKeys::radius].toDouble(10.0));
      elem.width = static_cast<float>(new_json[MapJsonKeys::width].toDouble(0.0));
      elem.depth = static_cast<float>(new_json[MapJsonKeys::depth].toDouble(0.0));
      elem.height = static_cast<float>(new_json[MapJsonKeys::height].toDouble(3.0));
      elem.rotation = static_cast<float>(new_json[MapJsonKeys::rotation].toDouble(0.0));
      elem.entrances = new_json[MapJsonKeys::entrances].toArray();
      if (elem.type.trimmed().compare(QStringLiteral("mountain"),
                                      Qt::CaseInsensitive) == 0) {
        elem.entrances = QJsonArray{};
      }

      const QStringList known_keys = {MapJsonKeys::type,
                                      MapJsonKeys::x,
                                      MapJsonKeys::z,
                                      MapJsonKeys::radius,
                                      MapJsonKeys::width,
                                      MapJsonKeys::depth,
                                      MapJsonKeys::height,
                                      MapJsonKeys::rotation,
                                      MapJsonKeys::entrances};
      for (const QString& key : new_json.keys()) {
        if (!known_keys.contains(key)) {
          elem.extra_fields[key] = new_json[key];
        }
      }

      m_map_data->execute_command(
          std::make_unique<UpdateTerrainCmd>(m_map_data,
                                             index,
                                             m_map_data->terrain_elements()[index],
                                             elem,
                                             "Edit terrain"));
    } else if (element_type == 1) {
      WorldPropElement elem;
      elem.type = new_json[MapJsonKeys::type].toString(QStringLiteral("firecamp"));
      elem.x = static_cast<float>(new_json[MapJsonKeys::x].toDouble());
      elem.z = static_cast<float>(new_json[MapJsonKeys::z].toDouble());
      elem.scale = static_cast<float>(new_json[MapJsonKeys::scale].toDouble(1.0));
      elem.rotation = static_cast<float>(new_json[MapJsonKeys::rotation].toDouble(0.0));
      elem.intensity =
          static_cast<float>(new_json[MapJsonKeys::intensity].toDouble(1.0));
      elem.radius = static_cast<float>(new_json[MapJsonKeys::radius].toDouble(3.0));
      elem.persistent = new_json[MapJsonKeys::persistent].toBool(true);

      const QStringList known_keys = {MapJsonKeys::type,
                                      MapJsonKeys::x,
                                      MapJsonKeys::z,
                                      MapJsonKeys::scale,
                                      MapJsonKeys::rotation,
                                      MapJsonKeys::intensity,
                                      MapJsonKeys::radius,
                                      MapJsonKeys::persistent};
      for (const QString& key : new_json.keys()) {
        if (!known_keys.contains(key)) {
          elem.extra_fields[key] = new_json[key];
        }
      }

      m_map_data->execute_command(
          std::make_unique<UpdateWorldPropCmd>(m_map_data,
                                               index,
                                               m_map_data->world_props()[index],
                                               elem,
                                               "Edit " + elem.type));
    } else if (element_type == 2) {
      LinearElement elem;
      elem.type = new_json[MapJsonKeys::type].toString();

      QJsonArray start_arr = new_json[MapJsonKeys::start].toArray();
      QJsonArray end_arr = new_json[MapJsonKeys::end].toArray();
      if (start_arr.size() >= 2 && end_arr.size() >= 2) {
        elem.start = QVector2D(static_cast<float>(start_arr[0].toDouble()),
                               static_cast<float>(start_arr[1].toDouble()));
        elem.end = QVector2D(static_cast<float>(end_arr[0].toDouble()),
                             static_cast<float>(end_arr[1].toDouble()));
      }
      elem.width = static_cast<float>(new_json[MapJsonKeys::width].toDouble(3.0));
      elem.height = static_cast<float>(new_json[MapJsonKeys::height].toDouble(0.5));
      elem.style = new_json[MapJsonKeys::style].toString("default");
      elem.player_id = new_json[MapJsonKeys::player_id].toInt(0);
      elem.nation = new_json[MapJsonKeys::nation].toString();
      if (supports_waypoints(elem.type)) {
        elem.waypoints =
            waypoints_from_json(new_json[MapJsonKeys::waypoints].toArray());
      }

      const QStringList known_keys = {MapJsonKeys::type,
                                      MapJsonKeys::start,
                                      MapJsonKeys::end,
                                      MapJsonKeys::width,
                                      MapJsonKeys::height,
                                      MapJsonKeys::style,
                                      MapJsonKeys::player_id,
                                      MapJsonKeys::nation,
                                      MapJsonKeys::waypoints};
      for (const QString& key : new_json.keys()) {
        if (!known_keys.contains(key)) {
          elem.extra_fields[key] = new_json[key];
        }
      }

      if (elem.type == QStringLiteral("bridge")) {

        if (elem.height < k_min_bridge_height) {
          elem.height = k_min_bridge_height;
          show_action_feedback(
              QString("Bridge height raised to minimum %1.")
                  .arg(static_cast<double>(k_min_bridge_height), 0, 'f', 2),
              false);
        }

        const float required_width = compute_min_bridge_width(
            elem.start, elem.end, m_map_data->linear_elements());
        if (elem.width < required_width) {
          show_action_feedback(
              QString("Bridge width raised to %1 to span crossed river(s) from bank to "
                      "bank.")
                  .arg(static_cast<double>(required_width), 0, 'f', 2),
              false);
          elem.width = required_width;
        }
      }
      if (elem.type == QStringLiteral("wall")) {
        const QVector2D delta = elem.end - elem.start;
        if (std::abs(delta.x()) >= std::abs(delta.y())) {
          elem.end.setY(elem.start.y());
        } else {
          elem.end.setX(elem.start.x());
        }
      }

      m_map_data->execute_command(
          std::make_unique<UpdateLinearCmd>(m_map_data,
                                            index,
                                            m_map_data->linear_elements()[index],
                                            elem,
                                            "Edit " + elem.type));
    } else if (element_type == 3) {
      StructureElement elem;
      elem.type = new_json[MapJsonKeys::type].toString();
      elem.x = static_cast<float>(new_json[MapJsonKeys::x].toDouble());
      elem.z = static_cast<float>(new_json[MapJsonKeys::z].toDouble());
      elem.player_id = new_json[MapJsonKeys::player_id].toInt(0);
      elem.max_population = new_json[MapJsonKeys::max_population].toInt(100);
      elem.nation = new_json[MapJsonKeys::nation].toString();
      elem.spawn_order = m_map_data->structures()[index].spawn_order;

      const QStringList known_keys = {MapJsonKeys::type,
                                      MapJsonKeys::x,
                                      MapJsonKeys::z,
                                      MapJsonKeys::player_id,
                                      MapJsonKeys::max_population,
                                      MapJsonKeys::nation};
      for (const QString& key : new_json.keys()) {
        if (!known_keys.contains(key)) {
          elem.extra_fields[key] = new_json[key];
        }
      }

      m_map_data->execute_command(
          std::make_unique<UpdateStructureCmd>(m_map_data,
                                               index,
                                               m_map_data->structures()[index],
                                               elem,
                                               "Edit " + elem.type));
    } else if (element_type == 4) {
      TroopSpawnElement elem;
      elem.type = new_json[MapJsonKeys::type].toString();
      elem.x = static_cast<float>(new_json[MapJsonKeys::x].toDouble());
      elem.z = static_cast<float>(new_json[MapJsonKeys::z].toDouble());
      elem.player_id = new_json.contains(MapJsonKeys::player_id) &&
                               !new_json.value(MapJsonKeys::player_id).isNull()
                           ? new_json[MapJsonKeys::player_id].toInt(-1)
                           : -1;
      elem.max_population = new_json.contains(MapJsonKeys::max_population)
                                ? new_json[MapJsonKeys::max_population].toInt(100)
                                : -1;
      elem.nation = new_json[MapJsonKeys::nation].toString();
      elem.behavior = new_json[MapJsonKeys::behavior].toString();
      elem.guard_radius =
          static_cast<float>(new_json[MapJsonKeys::guard_radius].toDouble(10.0));
      elem.patrol_waypoints = new_json[MapJsonKeys::patrol_waypoints].toArray();

      const QStringList known_keys = {MapJsonKeys::type,
                                      MapJsonKeys::x,
                                      MapJsonKeys::z,
                                      MapJsonKeys::player_id,
                                      MapJsonKeys::max_population,
                                      MapJsonKeys::nation,
                                      MapJsonKeys::behavior,
                                      MapJsonKeys::guard_radius,
                                      MapJsonKeys::patrol_waypoints};
      for (const QString& key : new_json.keys()) {
        if (!known_keys.contains(key)) {
          elem.extra_fields[key] = new_json[key];
        }
      }

      m_map_data->execute_command(
          std::make_unique<UpdateTroopSpawnCmd>(m_map_data,
                                                index,
                                                m_map_data->troop_spawns()[index],
                                                elem,
                                                "Edit " + elem.type));
    } else if (element_type == 5) {
      UndeadZoneElement elem;
      elem.id = new_json["id"].toString();
      elem.anchor_type =
          new_json["anchor_type"].toString(QStringLiteral("magic_shrine"));
      elem.x = static_cast<float>(new_json[MapJsonKeys::x].toDouble());
      elem.z = static_cast<float>(new_json[MapJsonKeys::z].toDouble());
      elem.radius = static_cast<float>(new_json[MapJsonKeys::radius].toDouble(8.0));
      elem.leash_radius = static_cast<float>(new_json["leash_radius"].toDouble(14.0));
      elem.owner_id = new_json["owner_id"].toInt(99);
      elem.team_id = new_json["team_id"].toInt(99);
      elem.awaken_on = new_json["awaken_on"].toArray();
      elem.waves = new_json["waves"].toArray();

      m_map_data->execute_command(
          std::make_unique<UpdateUndeadZoneCmd>(m_map_data,
                                                index,
                                                m_map_data->undead_zones()[index],
                                                elem,
                                                "Edit undead zone"));
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

  const QDir repo_maps_dir(QDir::current().filePath("assets/maps"));
  if (repo_maps_dir.exists()) {
    return fallback_name.isEmpty() ? repo_maps_dir.absolutePath()
                                   : repo_maps_dir.filePath(fallback_name);
  }

  if (!fallback_name.isEmpty()) {
    return QDir::current().filePath(fallback_name);
  }

  return QDir::currentPath();
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
  for (const QString& start : starts) {
    QDir directory(start);
    for (int depth = 0; depth < 8; ++depth) {
      if (QFileInfo::exists(directory.filePath(QStringLiteral("CMakeLists.txt"))) &&
          QDir(directory.filePath(QStringLiteral("assets"))).exists()) {
        return directory.absolutePath();
      }
      if (!directory.cdUp()) {
        break;
      }
    }
  }
  return QDir::currentPath();
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
  QString title = m_mission_mode ? "Standard of Iron - Mission Editor"
                                 : "Standard of Iron - Map Editor";
  if (!m_current_file_path.isEmpty()) {
    title += " - " + QFileInfo(m_current_file_path).fileName();
  } else {
    title += " - " + (m_mission_mode ? m_mission_data->title() : m_map_data->name());
  }
  if (m_map_data->is_modified() || (m_mission_mode && m_mission_data->is_modified())) {
    title += " *";
  }
  setWindowTitle(title);
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
      coords = QString("(%1, %2) P%3")
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
  }

  if (!type_name.isEmpty()) {
    m_selection_status_text =
        QString("Selected: %1 at %2").arg(prettifyIdentifier(type_name), coords);
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
