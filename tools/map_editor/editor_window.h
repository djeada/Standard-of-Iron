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

  bool load_file(const QString &file_path);

private slots:
  void new_map();
  void open_map();
  void save_map();
  void save_map_as();
  void resize_map();
  void undo();
  void redo();
  void on_tool_selected(ToolType tool);
  void on_tool_cleared();
  void on_element_double_clicked(int element_type, int index);
  void on_grid_double_clicked();
  void on_modified_changed(bool modified);
  void on_undo_redo_changed();
  void update_dimensions_label();
  void on_selection_changed(int element_type, int index);
  void update_selection_info();

private:
  void setup_ui();
  void setup_menus();
  void update_window_title();
  void update_current_file_label();
  void show_action_feedback(const QString &message, bool success = true);
  void show_load_failure(const QString &file_path,
                         const QString &error_message);
  void show_save_failure(const QString &file_path,
                         const QString &error_message);
  [[nodiscard]] QString
  default_map_dialog_path(const QString &fallback_name) const;
  bool save_map_to_path(const QString &file_path, bool update_current_path);
  bool maybe_save();
  void closeEvent(QCloseEvent *event) override;
  void refresh_status_label();

  MapData *m_map_data = nullptr;
  MapCanvas *m_canvas = nullptr;
  ToolPanel *m_tool_panel = nullptr;
  QLabel *m_feedback_label = nullptr;
  QLabel *m_tool_label = nullptr;
  QLabel *m_dimensions_label = nullptr;
  QLabel *m_zoom_label = nullptr;
  QLabel *m_cursor_label = nullptr;
  QLabel *m_file_label = nullptr;
  QString m_current_file_path;
  QString m_tool_status_text = "Tool: Select";
  QString m_selection_status_text;
  bool m_hint_active = false;

  QAction *m_undo_action = nullptr;
  QAction *m_redo_action = nullptr;
};

} // namespace MapEditor
