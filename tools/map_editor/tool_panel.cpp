#include "tool_panel.h"
#include <QFont>
#include <QVBoxLayout>

namespace MapEditor {

ToolPanel::ToolPanel(QWidget *parent) : QWidget(parent) { setupUI(); }

void ToolPanel::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);

  m_tool_list = new QListWidget(this);
  m_tool_list->setIconSize(QSize(32, 32));
  m_tool_list->setSpacing(4);
  m_tool_list->setDragEnabled(true);
  m_tool_list->setDragDropMode(QAbstractItemView::DragOnly);

  QFont font = m_tool_list->font();
  font.setPointSize(16);
  m_tool_list->setFont(font);

  addToolItem("Select", "⬚", ToolType::Select);
  addToolItem("Hill", "⛰", ToolType::Hill);
  addToolItem("Mountain", "🏔", ToolType::Mountain);
  addToolItem("River", "〰", ToolType::River);
  addToolItem("Road", "═", ToolType::Road);
  addToolItem("Bridge", "🌉", ToolType::Bridge);
  addToolItem("Firecamp", "🔥", ToolType::Firecamp);
  addToolItem("Barracks", "🏛", ToolType::Barracks);
  addToolItem("Village", "🏘", ToolType::Village);
  addToolItem("Eraser", "🗑", ToolType::Eraser);

  connect(m_tool_list, &QListWidget::itemClicked, this,
          &ToolPanel::onItemClicked);

  layout->addWidget(m_tool_list);

  setMinimumWidth(120);
  setMaximumWidth(180);
}

void ToolPanel::addToolItem(const QString &name, const QString &iconChar,
                            ToolType tool) {
  auto *item = new QListWidgetItem(iconChar + "  " + name);
  item->setData(Qt::UserRole, static_cast<int>(tool));
  item->setToolTip(name);
  m_tool_list->addItem(item);
}

void ToolPanel::onItemClicked(QListWidgetItem *item) {
  m_current_tool = static_cast<ToolType>(item->data(Qt::UserRole).toInt());
  emit toolSelected(m_current_tool);
}

void ToolPanel::clearSelection() {
  m_current_tool = ToolType::Select;
  m_tool_list->setCurrentRow(0);
  emit toolSelected(m_current_tool);
}

} // namespace MapEditor
