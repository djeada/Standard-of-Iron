#pragma once

#include <QWidget>

class QButtonGroup;
class QGridLayout;
class QLabel;
class QToolButton;

namespace MapEditor {

enum class ToolType {
  Select,
  Hill,
  Mountain,
  River,
  Road,
  Bridge,
  PropFirecamp,
  PropTent,
  PropSupplyCart,
  PropWeaponRack,
  PropRuins,
  PropDeadTree,
  PropBoulder,
  Barracks,
  Village,
  Eraser
};

class ToolPanel : public QWidget {
  Q_OBJECT

public:
  explicit ToolPanel(QWidget* parent = nullptr);

  [[nodiscard]] ToolType current_tool() const { return m_current_tool; }
  [[nodiscard]] int current_player_id() const { return m_current_player_id; }
  void clear_selection();

signals:
  void tool_selected(ToolType tool);
  void player_id_changed(int player_id);

private:
  void setup_ui();
  auto add_tool_button(QGridLayout* layout,
                       int row,
                       int column,
                       const QString& name,
                       const QString& icon_char,
                       const QString& description,
                       ToolType tool) -> QToolButton*;
  void set_current_tool(ToolType tool, bool emit_signal = true);
  void update_active_tool_label(const QString& description);

  QButtonGroup* m_tool_group = nullptr;
  QButtonGroup* m_player_group = nullptr;
  QLabel* m_active_tool_label = nullptr;
  QToolButton* m_select_button = nullptr;
  ToolType m_current_tool = ToolType::Select;
  int m_current_player_id = 0;
};

} // namespace MapEditor
