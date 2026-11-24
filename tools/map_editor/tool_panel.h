#pragma once

#include <QIcon>
#include <QVector>
#include <QWidget>

#include "tool_catalog.h"

class QButtonGroup;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QScrollArea;
class QToolButton;

namespace MapEditor {

class ToolPanel : public QWidget {
  Q_OBJECT

public:
  explicit ToolPanel(QWidget* parent = nullptr);

  [[nodiscard]] ToolType current_tool() const { return m_current_tool; }
  [[nodiscard]] int current_player_id() const { return m_current_player_id; }
  [[nodiscard]] QString current_nation() const { return m_current_nation; }
  void clear_selection(bool emit_signal = true);

signals:
  void tool_selected(ToolType tool);
  void player_id_changed(int player_id);
  void nation_changed(const QString& nation);

private:
  void setup_ui();
  auto create_ownership_group() -> QWidget*;
  auto create_tool_button(const ToolSpec& spec, QWidget* parent) -> QToolButton*;
  void apply_filter(const QString& filter);
  void set_current_tool(ToolType tool, bool emit_signal = true);

  QButtonGroup* m_tool_group = nullptr;
  QComboBox* m_category = nullptr;
  QLineEdit* m_filter_edit = nullptr;
  QScrollArea* m_tool_scroll = nullptr;
  QLabel* m_no_matches = nullptr;
  QWidget* m_ownership = nullptr;
  struct SectionGroup {
    ToolSection section;
    QWidget* group;
    QGridLayout* grid;
    QVector<QToolButton*> buttons;
  };
  QVector<SectionGroup> m_sections;
  ToolType m_current_tool = ToolType::Select;
  int m_current_player_id = 0;
  QString m_current_nation;
};

} // namespace MapEditor
