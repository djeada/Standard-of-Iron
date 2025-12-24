#pragma once

#include "map_canvas.h"
#include "map_data.h"
#include "tool_panel.h"
#include <QLabel>
#include <QMainWindow>
#include <QWidget>

namespace MapEditor {

class EditorWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit EditorWindow(QWidget *parent = nullptr);
  ~EditorWindow() override;

  // Public method to load a file (for command-line support)
  bool loadFile(const QString &filePath);

private slots:
  void newMap();
  void openMap();
  void saveMap();
  void saveMapAs();
  void resizeMap();
  void onToolSelected(ToolType tool);
  void onElementDoubleClicked(int elementType, int index);
  void onModifiedChanged(bool modified);

private:
  void setupUI();
  void setupMenus();
  void updateWindowTitle();
  bool maybeSave();
  void closeEvent(QCloseEvent *event) override;

  MapData *m_mapData = nullptr;
  MapCanvas *m_canvas = nullptr;
  ToolPanel *m_toolPanel = nullptr;
  QLabel *m_statusLabel = nullptr;
  QString m_currentFilePath;
};

} // namespace MapEditor
