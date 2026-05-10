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

  MapData *m_map_data = nullptr;
  MapCanvas *m_canvas = nullptr;
  ToolPanel *m_tool_panel = nullptr;
  QLabel *m_status_label = nullptr;
  QLabel *m_dimensions_label = nullptr;
  QString m_current_file_path;

  QAction *m_undo_action = nullptr;
  QAction *m_redo_action = nullptr;
};

} // namespace MapEditor
