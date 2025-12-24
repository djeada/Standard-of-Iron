#pragma once

#include <QListWidget>
#include <QWidget>

namespace MapEditor {

/**
 * @brief Tool types available in the editor
 */
enum class ToolType {
  Select,
  Hill,
  Mountain,
  River,
  Road,
  Bridge,
  Firecamp,
  Barracks,
  Village,
  Eraser
};

/**
 * @brief Panel containing draggable tool elements
 */
class ToolPanel : public QWidget {
  Q_OBJECT

public:
  explicit ToolPanel(QWidget *parent = nullptr);

  [[nodiscard]] ToolType currentTool() const { return m_currentTool; }
  void clearSelection();

signals:
  void toolSelected(ToolType tool);

private slots:
  void onItemClicked(QListWidgetItem *item);

private:
  void setupUI();
  void addToolItem(const QString &name, const QString &iconChar, ToolType tool);

  QListWidget *m_toolList = nullptr;
  ToolType m_currentTool = ToolType::Select;
};

} // namespace MapEditor
