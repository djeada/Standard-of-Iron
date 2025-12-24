#include "tool_panel.h"
#include <QFont>
#include <QVBoxLayout>

namespace MapEditor {

ToolPanel::ToolPanel(QWidget *parent) : QWidget(parent) { setupUI(); }

void ToolPanel::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);

  m_toolList = new QListWidget(this);
  m_toolList->setIconSize(QSize(32, 32));
  m_toolList->setSpacing(4);
  m_toolList->setDragEnabled(true);
  m_toolList->setDragDropMode(QAbstractItemView::DragOnly);

  // Use a larger font for emoji icons
  QFont font = m_toolList->font();
  font.setPointSize(16);
  m_toolList->setFont(font);

  // Add tools with emoji-like characters as simple icons
  addToolItem("Select", "⬚", ToolType::Select);
  addToolItem("Hill", "⛰", ToolType::Hill);
  addToolItem("Mountain", "🏔", ToolType::Mountain);
  addToolItem("River", "〰", ToolType::River);
  addToolItem("Road", "═", ToolType::Road);
  addToolItem("Bridge", "🌉", ToolType::Bridge);
  addToolItem("Firecamp", "🔥", ToolType::Firecamp);
  addToolItem("Eraser", "🗑", ToolType::Eraser);

  connect(m_toolList, &QListWidget::itemClicked, this,
          &ToolPanel::onItemClicked);

  layout->addWidget(m_toolList);

  setMinimumWidth(120);
  setMaximumWidth(180);
}

void ToolPanel::addToolItem(const QString &name, const QString &iconChar,
                            ToolType tool) {
  auto *item = new QListWidgetItem(iconChar + "  " + name);
  item->setData(Qt::UserRole, static_cast<int>(tool));
  item->setToolTip(name);
  m_toolList->addItem(item);
}

void ToolPanel::onItemClicked(QListWidgetItem *item) {
  m_currentTool = static_cast<ToolType>(item->data(Qt::UserRole).toInt());
  emit toolSelected(m_currentTool);
}

} // namespace MapEditor
