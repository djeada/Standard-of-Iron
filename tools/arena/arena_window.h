#pragma once

#include <QMainWindow>

class ArenaViewport;
class TerrainPanel;
class UnitPanel;
class BuildingPanel;
class QLabel;

class ArenaWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit ArenaWindow(QWidget *parent = nullptr);

private:
  ArenaViewport *m_viewport = nullptr;
  TerrainPanel *m_terrain_panel = nullptr;
  UnitPanel *m_unit_panel = nullptr;
  BuildingPanel *m_building_panel = nullptr;
  QLabel *m_status_label = nullptr;
};
