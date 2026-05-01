#include "arena_window.h"

#include "arena_viewport.h"
#include "terrain_panel.h"
#include "unit_panel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

ArenaWindow::ArenaWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("Standard of Iron Arena");

  auto *toolbar = addToolBar("Arena");
  toolbar->setMovable(false);
  auto *regenerate_action = toolbar->addAction("Regenerate");
  toolbar->addSeparator();
  auto *pause_action = toolbar->addAction("Pause");
  pause_action->setCheckable(true);
  toolbar->addSeparator();
  auto *reset_camera_action = toolbar->addAction("Reset Camera");

  auto *central = new QWidget(this);
  auto *main_layout = new QHBoxLayout(central);
  main_layout->setContentsMargins(8, 8, 8, 8);
  main_layout->setSpacing(8);

  m_viewport = new ArenaViewport(central);
  m_viewport->setMinimumSize(960, 720);
  main_layout->addWidget(m_viewport, 1);

  auto *tab_widget = new QTabWidget(central);
  tab_widget->setMinimumWidth(320);
  tab_widget->setMaximumWidth(400);

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

  main_layout->addWidget(tab_widget);
  setCentralWidget(central);

  m_status_label = new QLabel("Ready", this);
  statusBar()->addWidget(m_status_label);

  connect(regenerate_action, &QAction::triggered, m_viewport,
          &ArenaViewport::regenerateTerrain);
  connect(reset_camera_action, &QAction::triggered, m_viewport,
          &ArenaViewport::resetCamera);
  connect(pause_action, &QAction::toggled, m_viewport,
          &ArenaViewport::pauseSimulation);
  connect(m_viewport, &ArenaViewport::pausedChanged, pause_action,
          &QAction::setChecked);
  connect(m_viewport, &ArenaViewport::pausedChanged, m_unit_panel,
          &UnitPanel::setAnimationPaused);
  connect(m_viewport, &ArenaViewport::pausedChanged, this,
          [this](bool paused) {
            m_status_label->setText(paused ? "Paused" : "Running");
          });

  connect(m_terrain_panel, &TerrainPanel::seedChanged, m_viewport,
          &ArenaViewport::setTerrainSeed);
  connect(m_terrain_panel, &TerrainPanel::heightScaleChanged, m_viewport,
          &ArenaViewport::setTerrainHeightScale);
  connect(m_terrain_panel, &TerrainPanel::octavesChanged, m_viewport,
          &ArenaViewport::setTerrainOctaves);
  connect(m_terrain_panel, &TerrainPanel::frequencyChanged, m_viewport,
          &ArenaViewport::setTerrainFrequency);
  connect(m_terrain_panel, &TerrainPanel::regenerateRequested, m_viewport,
          &ArenaViewport::regenerateTerrain);
  connect(m_terrain_panel, &TerrainPanel::wireframeToggled, m_viewport,
          &ArenaViewport::setWireframeEnabled);
  connect(m_terrain_panel, &TerrainPanel::normalsToggled, m_viewport,
          &ArenaViewport::setNormalsOverlayEnabled);
  connect(m_terrain_panel, &TerrainPanel::groundTypeChanged, m_viewport,
          &ArenaViewport::setGroundType);
  connect(m_terrain_panel, &TerrainPanel::rainToggled, m_viewport,
          &ArenaViewport::setRainEnabled);
  connect(m_terrain_panel, &TerrainPanel::rainIntensityChanged, m_viewport,
          &ArenaViewport::setRainIntensity);

  connect(m_unit_panel, &UnitPanel::spawnUnitsRequested, m_viewport,
          &ArenaViewport::spawnUnits);
  connect(m_unit_panel, &UnitPanel::clearUnitsRequested, m_viewport,
          &ArenaViewport::clearUnits);
  connect(m_unit_panel, &UnitPanel::spawnOwnerSelected, m_viewport,
          &ArenaViewport::setSpawnOwner);
  connect(m_unit_panel, &UnitPanel::nationSelected, m_viewport,
          &ArenaViewport::setSpawnNation);
  connect(m_unit_panel, &UnitPanel::unitTypeSelected, m_viewport,
          &ArenaViewport::setSpawnUnitType);
  connect(m_unit_panel, &UnitPanel::spawnIndividualsPerUnitChanged, m_viewport,
          &ArenaViewport::setSpawnIndividualsPerUnit);
  connect(m_unit_panel, &UnitPanel::spawnRiderVisibilityChanged, m_viewport,
          &ArenaViewport::setSpawnRiderVisible);
  connect(m_unit_panel, &UnitPanel::applyVisualOverridesRequested, m_viewport,
          &ArenaViewport::applyVisualOverridesToSelection);
  connect(m_unit_panel, &UnitPanel::spawnOpposingBatchRequested, m_viewport,
          &ArenaViewport::spawnOpposingBatch);
  connect(m_unit_panel, &UnitPanel::spawnMirrorMatchRequested, m_viewport,
          &ArenaViewport::spawnMirrorMatch);
  connect(m_unit_panel, &UnitPanel::resetArenaRequested, m_viewport,
          &ArenaViewport::resetArena);
  connect(m_unit_panel, &UnitPanel::animationSelected, m_viewport,
          &ArenaViewport::setAnimationName);
  connect(m_unit_panel, &UnitPanel::playAnimationRequested, m_viewport,
          &ArenaViewport::playSelectedAnimation);
  connect(m_unit_panel, &UnitPanel::animationPausedToggled, m_viewport,
          &ArenaViewport::pauseSimulation);
  connect(m_unit_panel, &UnitPanel::moveSelectedUnitRequested, m_viewport,
          &ArenaViewport::moveSelectedUnitForward);
  connect(m_unit_panel, &UnitPanel::movementSpeedChanged, m_viewport,
          &ArenaViewport::setMovementSpeed);
  connect(m_unit_panel, &UnitPanel::skeletonDebugToggled, m_viewport,
          &ArenaViewport::setSkeletonDebugEnabled);
  connect(m_viewport, &ArenaViewport::selectionSummaryChanged, m_unit_panel,
          &UnitPanel::setSelectionSummary);

  m_viewport->setSpawnOwner(m_unit_panel->selectedOwnerId());
  m_viewport->setSpawnNation(m_unit_panel->selectedNationId());
  m_viewport->setSpawnUnitType(m_unit_panel->selectedUnitTypeId());
}
