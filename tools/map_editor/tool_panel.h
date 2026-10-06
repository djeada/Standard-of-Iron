#pragma once

#include <QIcon>
#include <QVector>
#include <QWidget>

#include "tool_catalog.h"

class QButtonGroup;
class QComboBox;
class QContextMenuEvent;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QToolButton;

namespace MapEditor {

class ToolPanel : public QWidget {
  Q_OBJECT

public:
  explicit ToolPanel(QWidget* parent = nullptr);

  [[nodiscard]] ToolType current_tool() const { return m_current_tool; }
  [[nodiscard]] int current_player_id() const { return m_current_player_id; }
  [[nodiscard]] QString current_nation() const { return m_current_nation; }
  void clear_selection();

signals:
  void tool_selected(ToolType tool);
  void player_id_changed(int player_id);
  void nation_changed(const QString& nation);

protected:
  void contextMenuEvent(QContextMenuEvent* event) override;

private:
  void setup_ui();
  auto create_ownership_group() -> QGroupBox*;
  auto add_tool_button(QGridLayout* layout,
                       int row,
                       int column,
                       const ToolSpec& spec,
                       const QIcon& icon) -> QToolButton*;
  void apply_filter(const QString& filter);
  void set_current_tool(ToolType tool, bool emit_signal = true);
  void update_active_tool_label(const QString& description);

  QButtonGroup* m_tool_group = nullptr;
  QButtonGroup* m_player_group = nullptr;
  QComboBox* m_nation_box = nullptr;
  QLabel* m_active_tool_label = nullptr;
  QLineEdit* m_filter_edit = nullptr;
  struct SectionGroup {
    ToolSection section;
    QGroupBox* group;
  };
  QVector<SectionGroup> m_sections;
  ToolType m_current_tool = ToolType::Select;
  int m_current_player_id = 0;
  QString m_current_nation;
};

} // namespace MapEditor
