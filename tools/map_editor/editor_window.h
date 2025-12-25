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

  bool loadFile(const QString &filePath);

private slots:
  void newMap();
  void openMap();
  void saveMap();
  void saveMapAs();
  void resizeMap();
  void undo();
  void redo();
  void onToolSelected(ToolType tool);
  void onToolCleared();
  void onElementDoubleClicked(int elementType, int index);
  void onGridDoubleClicked();
  void onModifiedChanged(bool modified);
  void onUndoRedoChanged();
  void updateDimensionsLabel();

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
  QLabel *m_dimensionsLabel = nullptr;
  QString m_currentFilePath;

  QAction *m_undoAction = nullptr;
  QAction *m_redoAction = nullptr;
};

} 
