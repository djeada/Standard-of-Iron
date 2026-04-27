#pragma once

#include <QMainWindow>

class ArenaViewport;
class TerrainPanel;
class UnitPanel;

class ArenaWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit ArenaWindow(QWidget *parent = nullptr);

private:
  ArenaViewport *m_viewport = nullptr;
  TerrainPanel *m_terrain_panel = nullptr;
  UnitPanel *m_unit_panel = nullptr;
};
