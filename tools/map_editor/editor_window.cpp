#include "editor_window.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include <cmath>

#include "json_edit_dialog.h"
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
                         "Drag empty space in Select mode to pan.\n"
                         "Middle click, Space + drag, or Ctrl + left drag also pans.\n"
                         "Mouse wheel zooms in and out.\n"
                         "Right click or Escape returns to Select.\n"
                         "Double-click element to edit its JSON.\n"
                         "Double-click empty canvas opens Resize Map.",
                         panel));

  layout->addWidget(createGuideSection("Keyboard Shortcuts",
                                       "Ctrl+N: New map\n"
                                       "Ctrl+O: Open map\n"
                                       "Ctrl+S: Save\n"
                                       "Ctrl+Shift+S: Save As\n"
                                       "Ctrl+Z / Ctrl+Y: Undo / Redo\n"
                                       "Del / Backspace: Delete selected\n"
                                       "Escape: Cancel or return to Select",
                                       panel));

  layout->addStretch(1);
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
    , m_map_data(new MapData(this)) {

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

  auto* sidebar_tabs = new QTabWidget(splitter);
  sidebar_tabs->setMinimumWidth(300);
  sidebar_tabs->setMaximumWidth(420);

  m_tool_panel = new ToolPanel(sidebar_tabs);
  connect(
      m_tool_panel, &ToolPanel::tool_selected, this, &EditorWindow::on_tool_selected);
  connect(m_tool_panel,
          &ToolPanel::player_id_changed,
          m_canvas,
          &MapCanvas::set_current_player_id);

  auto* tools_scroll = new QScrollArea(sidebar_tabs);
  tools_scroll->setWidget(m_tool_panel);
  tools_scroll->setWidgetResizable(true);
  tools_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  sidebar_tabs->addTab(tools_scroll, "Tools");

  auto* guide_scroll = new QScrollArea(sidebar_tabs);
  guide_scroll->setWidget(createGuidePanel(guide_scroll));
  guide_scroll->setWidgetResizable(true);
  guide_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  sidebar_tabs->addTab(guide_scroll, "Guide");

  m_json_preview = new QPlainTextEdit(sidebar_tabs);
  m_json_preview->setReadOnly(true);
  m_json_preview->setLineWrapMode(QPlainTextEdit::NoWrap);
  QFont mono_font("Monospace");
  mono_font.setStyleHint(QFont::TypeWriter);
  mono_font.setPointSize(9);
  m_json_preview->setFont(mono_font);
  m_json_preview->setPlaceholderText("JSON preview will appear here…");
  sidebar_tabs->addTab(m_json_preview, "JSON");

  splitter->addWidget(m_canvas);
  splitter->addWidget(sidebar_tabs);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 0);
  splitter->setSizes({1160, 340});

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
  statusBar()->addPermanentWidget(m_zoom_label);
  statusBar()->addPermanentWidget(m_dimensions_label);
  auto* version_label = new QLabel("Map Editor v1.0", this);
  version_label->setStyleSheet("color: #4f6a75;");
  statusBar()->addPermanentWidget(version_label);
}

void EditorWindow::setup_menus() {

  auto* file_menu = menuBar()->addMenu("&File");

  auto* new_action = new QAction("&New", this);
  new_action->setShortcut(QKeySequence::New);
  new_action->setToolTip("Create a new map (Ctrl+N)");
  connect(new_action, &QAction::triggered, this, &EditorWindow::new_map);
  file_menu->addAction(new_action);

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

  auto* toolbar = addToolBar("Main");
  toolbar->setMovable(false);
  toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
  toolbar->addAction(new_action);
  toolbar->addAction(open_action);
  toolbar->addAction(save_action);
  toolbar->addSeparator();
  toolbar->addAction(m_undo_action);
  toolbar->addAction(m_redo_action);
  toolbar->addSeparator();
  toolbar->addAction(resize_action);
}

void EditorWindow::new_map() {
  if (!maybe_save()) {
    return;
  }

  m_map_data->clear();
  m_current_file_path.clear();
  update_window_title();
  update_current_file_label();
  show_action_feedback("Created a new unsaved map.");
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

  QString error_message;
  if (m_map_data->load_from_json(file_path, &error_message)) {
    m_current_file_path = QFileInfo(file_path).absoluteFilePath();
    update_window_title();
    update_current_file_label();
    show_action_feedback(
        QString("Loaded \"%1\" from %2")
            .arg(m_map_data->name(), normalizedDisplayPath(file_path)));
  } else {
    show_load_failure(file_path, error_message);
  }
}

bool EditorWindow::load_file(const QString& file_path) {
  QString error_message;
  if (m_map_data->load_from_json(file_path, &error_message)) {
    m_current_file_path = QFileInfo(file_path).absoluteFilePath();
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
  } else {
    save_map_to_path(m_current_file_path, true);
  }
}

void EditorWindow::save_map_as() {
  QString suggested_name = m_map_data->name().trimmed();
  if (suggested_name.isEmpty()) {
    suggested_name = "untitled_map";
  }
  suggested_name.replace(' ', '_');
  suggested_name = suggested_name.toLower();

  QString file_path =
      QFileDialog::getSaveFileName(this,
                                   "Save Map As",
                                   default_map_dialog_path(suggested_name + ".json"),
                                   "JSON Files (*.json);;All Files (*)");

  if (file_path.isEmpty()) {
    show_action_feedback("Save As cancelled.", false);
    return;
  }

  if (!file_path.endsWith(".json", Qt::CaseInsensitive)) {
    file_path += ".json";
  }

  save_map_to_path(file_path, true);
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
  case ToolType::PropDeadTree:
    tool_name = "Dead Tree";
    break;
  case ToolType::PropBoulder:
    tool_name = "Boulder";
    break;
  case ToolType::Barracks:
    tool_name = "Barracks";
    break;
  case ToolType::Village:
    tool_name = "Village";
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
  case ToolType::TroopCarthageMercenaryBroker:
  case ToolType::TroopCarthageCavalryPatron:
  case ToolType::TroopCarthageElephantMaster:
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
    const bool is_circular_hill = terrain_type == QStringLiteral("hill") &&
                                  elem.radius > 0.0F && elem.width > 0.0F &&
                                  elem.depth > 0.0F &&
                                  std::abs(elem.width - elem.depth) <= 1e-3F &&
                                  std::abs(elem.width - elem.radius) <= 1e-3F;
    if (elem.radius > 0.0F &&
        (is_mountain || is_circular_hill || elem.width <= 0.0F || elem.depth <= 0.0F)) {
      json[MapJsonKeys::radius] = static_cast<double>(elem.radius);
    }
    if (!is_mountain && !is_circular_hill) {
      if (elem.width > 0.0F) {
        json[MapJsonKeys::width] = static_cast<double>(elem.width);
      }
      if (elem.depth > 0.0F) {
        json[MapJsonKeys::depth] = static_cast<double>(elem.depth);
      }
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
    json[MapJsonKeys::max_population] = elem.max_population;
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
  JsonEditDialog dialog(title, json, enable_hill_projection, this);
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

      const QStringList known_keys = {MapJsonKeys::type,
                                      MapJsonKeys::start,
                                      MapJsonKeys::end,
                                      MapJsonKeys::width,
                                      MapJsonKeys::height,
                                      MapJsonKeys::style,
                                      MapJsonKeys::player_id,
                                      MapJsonKeys::nation};
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
      elem.max_population = new_json[MapJsonKeys::max_population].toInt(150);
      elem.nation = new_json[MapJsonKeys::nation].toString();

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
    m_file_label->setToolTip("This map has not been saved yet.");
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
  m_feedback_label->setStyleSheet(success ? "color: #9fd9ff;" : "color: #ff9b9b;");
  m_feedback_label->setToolTip(message);
}

void EditorWindow::show_load_failure(const QString& file_path,
                                     const QString& error_message) {
  const QString display_path = normalizedDisplayPath(file_path);
  const QString detail = error_message.trimmed().isEmpty()
                             ? QString("Unable to load the map file.")
                             : error_message;
  show_action_feedback("Load failed: " + display_path, false);
  QMessageBox::critical(
      this,
      "Load Failed",
      QString("Could not load map file:\n%1\n\nReason: %2").arg(display_path, detail));
}

void EditorWindow::show_save_failure(const QString& file_path,
                                     const QString& error_message) {
  const QString display_path = normalizedDisplayPath(file_path);
  const QString detail = error_message.trimmed().isEmpty()
                             ? QString("Unable to write the map file.")
                             : error_message;
  show_action_feedback("Save failed: " + display_path, false);
  QMessageBox::critical(this,
                        "Save Failed",
                        QString("Could not save map \"%1\" to:\n%2\n\nReason: %3")
                            .arg(m_map_data->name(), display_path, detail));
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

void EditorWindow::update_window_title() {
  QString title = "Standard of Iron - Map Editor";
  if (!m_current_file_path.isEmpty()) {
    title += " - " + QFileInfo(m_current_file_path).fileName();
  } else {
    title += " - " + m_map_data->name();
  }
  if (m_map_data->is_modified()) {
    title += " *";
  }
  setWindowTitle(title);
}

bool EditorWindow::maybe_save() {
  if (!m_map_data->is_modified()) {
    return true;
  }

  QMessageBox::StandardButton const ret = QMessageBox::warning(
      this,
      "Unsaved Changes",
      "The map has been modified.\nDo you want to save your changes?",
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

  if (ret == QMessageBox::Save) {
    save_map();
    return !m_map_data->is_modified();
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
  const QString json = m_map_data->to_json_string();

  if (m_json_preview->toPlainText() != json) {
    const int scroll_pos = m_json_preview->verticalScrollBar()->value();
    m_json_preview->setPlainText(json);
    m_json_preview->verticalScrollBar()->setValue(scroll_pos);
  }
}

} // namespace MapEditor
