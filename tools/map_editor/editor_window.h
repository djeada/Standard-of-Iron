#pragma once

#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QWidget>

#include "command_palette.h"
#include "map_canvas.h"
#include "map_data.h"
#include "mission_data.h"
#include "mission_panel.h"
#include "tool_panel.h"

class QScrollArea;
class QStackedWidget;
class QTabWidget;
class QTextBrowser;
class QToolBar;
class QToolButton;

namespace MapEditor {

class GeneratorPanel;
class CampaignWorkspace;
class CommandPalette;

class EditorWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit EditorWindow(QWidget* parent = nullptr);
  ~EditorWindow() override;

  enum class Workspace {
    Campaign,
    Battlefield,
    Mission
  };

  bool load_file(const QString& file_path);
  auto open_campaign(const QString& path = {}) -> CampaignWorkspace*;
  void set_workspace(Workspace workspace);
  [[nodiscard]] auto workspace() const -> Workspace { return m_workspace; }
  void open_command_palette();
  void select_sidebar_tab(const QString& title);

private slots:
  void new_map();
  void new_mission();
  void open_map();
  void save_map();
  void save_map_as();
  void resize_map();
  void edit_biome();
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
  void refresh_json_preview();
  void on_mission_map_path_changed(const QString& map_path);
  void validate_mission();
  void check_road_network();
  void launch_mission_game();
  void launch_mission_arena();

private:
  void setup_ui();
  auto build_workspace_bar() -> QWidget*;
  void refresh_workspace_bar();
  void refresh_campaign_context(const QString& site);
  void ensure_campaign();
  void ensure_campaign_context_world();
  void open_battlefield_from_campaign(const QString& map_path,
                                      const QString& mission_id,
                                      const QString& site_id);
  [[nodiscard]] auto palette_entries() -> QVector<PaletteEntry>;
  void setup_menus();
  void reset_to_new_map();
  void update_window_title();
  void update_current_file_label();
  void show_action_feedback(const QString& message, bool success = true);
  void show_load_failure(const QString& file_path, const QString& error_message);
  void show_save_failure(const QString& file_path, const QString& error_message);
  [[nodiscard]] QString default_map_dialog_path(const QString& fallback_name) const;
  [[nodiscard]] static QString load_last_dialog_directory();
  static void remember_dialog_directory(const QString& file_path);
  bool save_map_to_path(const QString& file_path, bool update_current_path);
  bool save_mission_to_path(const QString& file_path, bool update_current_path);
  bool save_current_document();
  bool load_linked_map(const QString& authored_path, QString* out_error = nullptr);
  [[nodiscard]] QString resolve_authored_path(const QString& authored_path) const;
  [[nodiscard]] QString repository_root() const;
  [[nodiscard]] QString tool_executable(const QString& name) const;
  bool validate_current_mission(bool show_success);
  void set_mission_mode(bool enabled);
  bool maybe_save();
  void closeEvent(QCloseEvent* event) override;
  void refresh_status_label();

  MapData* m_map_data = nullptr;
  MissionData* m_mission_data = nullptr;
  MapCanvas* m_canvas = nullptr;
  ToolPanel* m_tool_panel = nullptr;
  MissionPanel* m_mission_panel = nullptr;
  GeneratorPanel* m_generator_panel = nullptr;
  CampaignWorkspace* m_campaign = nullptr;
  CommandPalette* m_palette = nullptr;
  QStackedWidget* m_stack = nullptr;
  QWidget* m_battlefield_page = nullptr;
  QToolBar* m_main_toolbar = nullptr;
  QToolButton* m_campaign_tab = nullptr;
  QToolButton* m_battlefield_tab = nullptr;
  QToolButton* m_mission_tab = nullptr;
  QLabel* m_breadcrumb = nullptr;
  QToolButton* m_problem_badge = nullptr;
  QTextBrowser* m_campaign_context = nullptr;
  QAction* m_save_action = nullptr;
  QString m_anchor_site;
  Workspace m_workspace = Workspace::Battlefield;
  QTabWidget* m_sidebar_tabs = nullptr;
  QScrollArea* m_mission_scroll = nullptr;
  QLabel* m_feedback_label = nullptr;
  QLabel* m_tool_label = nullptr;
  QLabel* m_dimensions_label = nullptr;
  QLabel* m_zoom_label = nullptr;
  QWidget* m_zoom_widget = nullptr;
  QLabel* m_cursor_label = nullptr;
  QLabel* m_file_label = nullptr;
  QPlainTextEdit* m_json_preview = nullptr;
  QString m_current_file_path;
  QString m_linked_map_file_path;
  QString m_tool_status_text = "Tool: Select";
  QString m_selection_status_text;
  bool m_hint_active = false;
  bool m_mission_mode = false;

  QAction* m_undo_action = nullptr;
  QAction* m_redo_action = nullptr;
};

} // namespace MapEditor
