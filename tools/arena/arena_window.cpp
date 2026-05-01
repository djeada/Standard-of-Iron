#include "arena_window.h"

#include "arena_viewport.h"
#include "building_panel.h"
#include "terrain_panel.h"
#include "unit_panel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

ArenaWindow::ArenaWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("Standard of Iron Arena");

  auto *toolbar = addToolBar("Arena");
  toolbar->setMovable(false);
  toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
  auto *regenerate_action = toolbar->addAction("⟳  Regenerate");
  regenerate_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
  regenerate_action->setToolTip("Regenerate terrain (Ctrl+R)");
  toolbar->addSeparator();
  auto *pause_action = toolbar->addAction("⏸  Pause");
  pause_action->setCheckable(true);
  pause_action->setShortcut(QKeySequence(QStringLiteral("Space")));
  pause_action->setToolTip("Pause / resume simulation (Space)");
  toolbar->addSeparator();
  auto *reset_camera_action = toolbar->addAction("⌖  Reset Camera");
  reset_camera_action->setShortcut(QKeySequence(QStringLiteral("F")));
  reset_camera_action->setToolTip("Reset camera to default position (F)");

  auto *central = new QWidget(this);
  auto *main_layout = new QHBoxLayout(central);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  auto *splitter = new QSplitter(Qt::Horizontal, central);
  splitter->setChildrenCollapsible(false);

  m_viewport = new ArenaViewport(splitter);
  m_viewport->setMinimumSize(480, 400);
  splitter->addWidget(m_viewport);

  auto *tab_widget = new QTabWidget(splitter);
  tab_widget->setMinimumWidth(280);
  tab_widget->setMaximumWidth(480);

  m_terrain_panel = new TerrainPanel();
  auto *terrain_scroll = new QScrollArea();
  terrain_scroll->setWidget(m_terrain_panel);
  terrain_scroll->setWidgetResizable(true);
  terrain_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  tab_widget->addTab(terrain_scroll, "Terrain");

  m_unit_panel = new UnitPanel();
  auto *unit_scroll = new QScrollArea();
  unit_scroll->setWidget(m_unit_panel);
  unit_scroll->setWidgetResizable(true);
  unit_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  tab_widget->addTab(unit_scroll, "Units");

  m_building_panel = new BuildingPanel();
  auto *building_scroll = new QScrollArea();
  building_scroll->setWidget(m_building_panel);
  building_scroll->setWidgetResizable(true);
  building_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  tab_widget->addTab(building_scroll, "Buildings");

  splitter->addWidget(tab_widget);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 0);
  splitter->setSizes({1200, 360});

  main_layout->addWidget(splitter);
  setCentralWidget(central);

  m_status_label = new QLabel("Ready", this);
  statusBar()->addWidget(m_status_label);
  auto *version_label = new QLabel("Standard of Iron Arena v1.0", this);
  version_label->setStyleSheet("color: #4f6a75;");
  statusBar()->addPermanentWidget(version_label);

  connect(regenerate_action, &QAction::triggered, m_viewport,
          &ArenaViewport::regenerate_terrain);
  connect(reset_camera_action, &QAction::triggered, m_viewport,
          &ArenaViewport::reset_camera);
  connect(pause_action, &QAction::toggled, m_viewport,
          &ArenaViewport::pause_simulation);
  connect(m_viewport, &ArenaViewport::paused_changed, pause_action,
          &QAction::setChecked);
  connect(m_viewport, &ArenaViewport::paused_changed, m_unit_panel,
          &UnitPanel::set_animation_paused);
  connect(m_viewport, &ArenaViewport::paused_changed, this, [this](bool paused) {
    m_status_label->setText(paused ? "Paused" : "Running");
  });

  connect(m_terrain_panel, &TerrainPanel::seed_changed, m_viewport,
          &ArenaViewport::set_terrain_seed);
  connect(m_terrain_panel, &TerrainPanel::height_scale_changed, m_viewport,
          &ArenaViewport::set_terrain_height_scale);
  connect(m_terrain_panel, &TerrainPanel::octaves_changed, m_viewport,
          &ArenaViewport::set_terrain_octaves);
  connect(m_terrain_panel, &TerrainPanel::frequency_changed, m_viewport,
          &ArenaViewport::set_terrain_frequency);
  connect(m_terrain_panel, &TerrainPanel::regenerate_requested, m_viewport,
          &ArenaViewport::regenerate_terrain);
  connect(m_terrain_panel, &TerrainPanel::wireframe_toggled, m_viewport,
          &ArenaViewport::set_wireframe_enabled);
  connect(m_terrain_panel, &TerrainPanel::normals_toggled, m_viewport,
          &ArenaViewport::set_normals_overlay_enabled);
  connect(m_terrain_panel, &TerrainPanel::ground_type_changed, m_viewport,
          &ArenaViewport::set_ground_type);
  connect(m_terrain_panel, &TerrainPanel::rain_toggled, m_viewport,
          &ArenaViewport::set_rain_enabled);
  connect(m_terrain_panel, &TerrainPanel::rain_intensity_changed, m_viewport,
          &ArenaViewport::set_rain_intensity);

  connect(m_unit_panel, &UnitPanel::spawn_units_requested, m_viewport,
          &ArenaViewport::spawn_units);
  connect(m_unit_panel, &UnitPanel::clear_units_requested, m_viewport,
          &ArenaViewport::clear_units);
  connect(m_unit_panel, &UnitPanel::spawn_owner_selected, m_viewport,
          &ArenaViewport::set_spawn_owner);
  connect(m_unit_panel, &UnitPanel::nation_selected, m_viewport,
          &ArenaViewport::set_spawn_nation);
  connect(m_unit_panel, &UnitPanel::unit_type_selected, m_viewport,
          &ArenaViewport::set_spawn_unit_type);
  connect(m_unit_panel, &UnitPanel::spawn_individuals_per_unit_changed, m_viewport,
          &ArenaViewport::set_spawn_individuals_per_unit);
  connect(m_unit_panel, &UnitPanel::spawn_rider_visibility_changed, m_viewport,
          &ArenaViewport::set_spawn_rider_visible);
  connect(m_unit_panel, &UnitPanel::apply_visual_overrides_requested, m_viewport,
          &ArenaViewport::apply_visual_overrides_to_selection);
  connect(m_unit_panel, &UnitPanel::spawn_opposing_batch_requested, m_viewport,
          &ArenaViewport::spawn_opposing_batch);
  connect(m_unit_panel, &UnitPanel::spawn_mirror_match_requested, m_viewport,
          &ArenaViewport::spawn_mirror_match);
  connect(m_unit_panel, &UnitPanel::reset_arena_requested, m_viewport,
          &ArenaViewport::reset_arena);
  connect(m_unit_panel, &UnitPanel::animation_selected, m_viewport,
          &ArenaViewport::set_animation_name);
  connect(m_unit_panel, &UnitPanel::play_animation_requested, m_viewport,
          &ArenaViewport::play_selected_animation);
  connect(m_unit_panel, &UnitPanel::animation_paused_toggled, m_viewport,
          &ArenaViewport::pause_simulation);
  connect(m_unit_panel, &UnitPanel::move_selected_unit_requested, m_viewport,
          &ArenaViewport::move_selected_unit_forward);
  connect(m_unit_panel, &UnitPanel::movement_speed_changed, m_viewport,
          &ArenaViewport::set_movement_speed);
  connect(m_unit_panel, &UnitPanel::skeleton_debug_toggled, m_viewport,
          &ArenaViewport::set_skeleton_debug_enabled);
  connect(m_viewport, &ArenaViewport::selection_summary_changed, m_unit_panel,
          &UnitPanel::set_selection_summary);

  connect(m_building_panel, &BuildingPanel::spawn_buildings_requested,
          m_viewport, &ArenaViewport::spawn_buildings);
  connect(m_building_panel, &BuildingPanel::clear_buildings_requested,
          m_viewport, &ArenaViewport::clear_buildings);
  connect(m_building_panel, &BuildingPanel::building_owner_selected, m_viewport,
          &ArenaViewport::set_spawn_building_owner);
  connect(m_building_panel, &BuildingPanel::building_nation_selected, m_viewport,
          &ArenaViewport::set_spawn_building_nation);
  connect(m_building_panel, &BuildingPanel::building_type_selected, m_viewport,
          &ArenaViewport::set_spawn_building_type);
  connect(m_viewport, &ArenaViewport::selection_summary_changed,
          m_building_panel, &BuildingPanel::set_selection_summary);

  m_viewport->set_spawn_owner(m_unit_panel->selected_owner_id());
  m_viewport->set_spawn_nation(m_unit_panel->selected_nation_id());
  m_viewport->set_spawn_unit_type(m_unit_panel->selected_unit_type_id());

  m_viewport->set_spawn_building_owner(m_building_panel->selected_owner_id());
  m_viewport->set_spawn_building_nation(m_building_panel->selected_nation_id());
  m_viewport->set_spawn_building_type(m_building_panel->selected_building_type_id());
}
