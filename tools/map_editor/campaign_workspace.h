#pragma once

#include <QJsonObject>
#include <QMainWindow>

#include <tuple>

#include "game/strategic/strategic_types.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;
class QToolButton;
class QActionGroup;

namespace MapEditor {

class WorldCanvas;
class WorldDocument;
class ChronologyStrip;

class CampaignWorkspace : public QMainWindow {
  Q_OBJECT

public:
  explicit CampaignWorkspace(QWidget* parent = nullptr);

  auto open_world(const QString& path) -> bool;
  [[nodiscard]] auto has_world() const -> bool;
  void select_entity(const QString& id);
  void set_timeline(const QString& event_id);
  void show_path(const QString& from, const QString& to);
  void undo();
  void redo();
  auto save() -> bool;
  auto save_as() -> bool;
  auto maybe_save() -> bool;
  void show_problems();
  void validate_now();
  void complete_network();
  [[nodiscard]] auto document() const -> WorldDocument* { return m_document; }
  [[nodiscard]] auto canvas() const -> WorldCanvas* { return m_canvas; }
  [[nodiscard]] auto site_for_battlefield(const QString& map_path,
                                          const QString& mission_id) const -> QString;

signals:
  void open_battlefield_requested(const QString& map_path,
                                  const QString& mission_id,
                                  const QString& site_id);
  void generate_battlefield_requested(const QJsonObject& request,
                                      const QString& title,
                                      const QString& site_id);
  void feedback(const QString& message, bool success);
  void cursor_text(const QString& text);
  void summary_changed();

private:
  void build_toolbar();
  void build_explorer();
  void build_inspector();
  void build_timeline();
  void build_problems();
  void refresh_inspector();
  void refresh_properties();
  void refresh_timeline();
  void refresh_problems();
  void refresh_outline();
  void refresh_outline_selection();
  void refresh_query_sites();
  void apply_outline_filter(const QString& text);
  void run_path_query();
  void run_reach_query(const QString& from = {}, double days = -1.0);
  void run_route_count_query();
  void run_network_report();
  void open_battlefield_for(const QString& site);
  void generate_battlefield_for(const QString& site);
  void show_supply_for(const QString& id);
  void handle_link(const QUrl& url);
  void show_context_menu(const QString& id,
                         const Game::Strategic::LonLat& position,
                         const QPoint& global);
  void delete_entity(const QString& id);
  [[nodiscard]] auto inspector_html(const QString& id) const -> QString;
  [[nodiscard]] auto overview_html() const -> QString;
  [[nodiscard]] auto site_html(const QString& id) const -> QString;
  [[nodiscard]] auto route_html(const QString& id) const -> QString;
  [[nodiscard]] auto army_html(const QString& id) const -> QString;
  [[nodiscard]] auto region_html(const QString& id) const -> QString;
  [[nodiscard]] auto issues_html(const QString& id) const -> QString;
  [[nodiscard]] auto link(const QString& id) const -> QString;
  void add_site_form(QFormLayout* form, const QString& id);
  void add_route_form(QFormLayout* form, const QString& id);
  void add_region_form(QFormLayout* form, const QString& id);
  void add_army_form(QFormLayout* form, const QString& id);

  WorldDocument* m_document = nullptr;
  WorldCanvas* m_canvas = nullptr;
  QTextBrowser* m_inspector = nullptr;
  QWidget* m_properties = nullptr;
  QFormLayout* m_properties_form = nullptr;
  QToolButton* m_open_button = nullptr;
  QToolButton* m_generate_button = nullptr;
  QToolButton* m_supply_button = nullptr;
  ChronologyStrip* m_strip = nullptr;
  QListWidget* m_event_list = nullptr;
  QTextBrowser* m_event_detail = nullptr;
  QTreeWidget* m_issues = nullptr;
  QLabel* m_issue_summary = nullptr;
  QToolButton* m_show_errors = nullptr;
  QToolButton* m_show_warnings = nullptr;
  QToolButton* m_show_notes = nullptr;
  QTreeWidget* m_outline = nullptr;
  QLineEdit* m_outline_filter = nullptr;
  QListWidget* m_layers = nullptr;
  QComboBox* m_query_from = nullptr;
  QComboBox* m_query_to = nullptr;
  QComboBox* m_query_faction = nullptr;
  QCheckBox* m_query_safe = nullptr;
  QCheckBox* m_query_sea = nullptr;
  QDoubleSpinBox* m_query_days = nullptr;
  QTextBrowser* m_query_result = nullptr;
  QComboBox* m_site_kind = nullptr;
  QComboBox* m_route_kind = nullptr;
  QActionGroup* m_tools = nullptr;
  QWidget* m_explorer_tabs = nullptr;
  bool m_rebuilding = false;
  std::tuple<qsizetype, qsizetype, qsizetype> m_outline_counts{-1, -1, -1};
};

} // namespace MapEditor
