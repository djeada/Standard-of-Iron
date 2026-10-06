#include "tool_panel.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

#include "spawn_icon_library.h"
#include "tool_catalog.h"

namespace MapEditor {

namespace {

constexpr int k_max_player_id = 4;

auto createInfoLabel(const QString& text,
                     const QString& object_name,
                     QWidget* parent) -> QLabel* {
  auto* label = new QLabel(text, parent);
  label->setObjectName(object_name);
  label->setWordWrap(true);
  label->setTextFormat(Qt::PlainText);
  return label;
}

} // namespace

ToolPanel::ToolPanel(QWidget* parent)
    : QWidget(parent) {
  setup_ui();
}

void ToolPanel::setup_ui() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto* title = createInfoLabel("Map Tools", "panelTitle", this);
  layout->addWidget(title);

  auto* intro = createInfoLabel(
      "Choose a tool, then place or edit content directly on the canvas.",
      "panelIntro",
      this);
  layout->addWidget(intro);

  m_active_tool_label = createInfoLabel(QString(), "toolSummary", this);
  layout->addWidget(m_active_tool_label);

  m_filter_edit = new QLineEdit(this);
  m_filter_edit->setPlaceholderText(QStringLiteral("Find a tool…"));
  m_filter_edit->setClearButtonEnabled(true);
  m_filter_edit->setToolTip(
      QStringLiteral("Type part of a tool name, element type or section to narrow "
                     "the list."));
  connect(m_filter_edit, &QLineEdit::textChanged, this, &ToolPanel::apply_filter);
  layout->addWidget(m_filter_edit);

  m_tool_group = new QButtonGroup(this);
  m_tool_group->setExclusive(true);

  for (const ToolSection section : tool_sections()) {
    auto* group = new QGroupBox(
        section_title(section).replace(QLatin1Char('&'), QStringLiteral("&&")), this);
    auto* grid = new QGridLayout(group);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    const int columns = section_columns(section);
    int index = 0;
    for (const ToolSpec* spec : tools_in_section(section)) {
      const bool has_icon = spec->placement == ToolPlacement::Troop;
      add_tool_button(grid,
                      index / columns,
                      index % columns,
                      *spec,
                      has_icon
                          ? troop_tool_icon(QString::fromLatin1(spec->element_type))
                          : QIcon());
      ++index;
    }
    m_sections.append({section, group});
    layout->addWidget(group);

    if (section == ToolSection::WorldProps) {
      layout->addWidget(create_ownership_group());
    }
  }

  auto* tips_group = new QGroupBox("Editing Tips", this);
  auto* tips_layout = new QVBoxLayout(tips_group);
  tips_layout->setSpacing(4);
  tips_layout->addWidget(
      createInfoLabel("Left click: place or select\n"
                      "Drag: move selected elements or endpoints\n"
                      "Middle click / Ctrl+drag: pan\n"
                      "Mouse wheel: zoom\n"
                      "Right click / Escape: put the tool away\n"
                      "Right click with no tool armed: context menu\n"
                      "Del / Backspace: delete selected\n"
                      "Shift + click/drag: free placement (no snap)\n"
                      "Double-click element: edit JSON (hills open a cell grid you "
                      "can draw their shape on)\n"
                      "Double-click empty grid: resize map",
                      "panelHint",
                      tips_group));
  layout->addWidget(tips_group);

  layout->addStretch(1);

  set_current_tool(ToolType::Select, false);
  setMinimumWidth(260);
  setMaximumWidth(360);
}

auto ToolPanel::create_ownership_group() -> QGroupBox* {
  auto* ownership_group = new QGroupBox("Ownership", this);
  auto* ownership_layout = new QHBoxLayout(ownership_group);
  ownership_layout->setContentsMargins(8, 4, 8, 8);
  ownership_layout->setSpacing(3);
  ownership_layout->addWidget(
      createInfoLabel("Player:", "fieldLabel", ownership_group));

  m_player_group = new QButtonGroup(this);
  m_player_group->setExclusive(true);
  for (int pid = 0; pid <= k_max_player_id; ++pid) {
    auto* pb = new QToolButton(ownership_group);
    pb->setText(pid == 0 ? "N" : QString::number(pid));
    pb->setToolTip(pid == 0 ? "Neutral / unassigned" : QString("Player %1").arg(pid));
    pb->setCheckable(true);
    pb->setChecked(pid == 0);
    pb->setMinimumWidth(28);
    pb->setMaximumWidth(36);
    pb->setProperty("playerId", pid);
    m_player_group->addButton(pb);
    ownership_layout->addWidget(pb);
    connect(pb, &QToolButton::clicked, this, [this, pid]() {
      m_current_player_id = pid;
      emit player_id_changed(pid);
    });
  }
  ownership_layout->addWidget(
      createInfoLabel("Nation:", "fieldLabel", ownership_group));
  m_nation_box = new QComboBox(ownership_group);
  m_nation_box->addItem("Owner default", QString());
  m_nation_box->addItem("Roman", QStringLiteral("roman_republic"));
  m_nation_box->addItem("Carthage", QStringLiteral("carthage"));
  m_nation_box->addItem("Sepulcher", QStringLiteral("iron_sepulcher"));
  m_nation_box->addItem("Gallic allies", QStringLiteral("gauls"));
  m_nation_box->addItem("Iberian allies", QStringLiteral("iberians"));
  connect(m_nation_box,
          qOverload<int>(&QComboBox::currentIndexChanged),
          this,
          [this](int index) {
            m_current_nation = m_nation_box->itemData(index).toString();
            emit nation_changed(m_current_nation);
          });
  ownership_layout->addWidget(m_nation_box);
  ownership_layout->addStretch(1);
  return ownership_group;
}

auto ToolPanel::add_tool_button(QGridLayout* layout,
                                int row,
                                int column,
                                const ToolSpec& spec,
                                const QIcon& icon) -> QToolButton* {
  auto* button = new QToolButton(this);
  const QString label = tool_card_label(spec.tool);
  if (!icon.isNull()) {
    button->setIcon(icon);
    button->setIconSize(QSize(28, 28));
    button->setText(label);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  } else {
    button->setText(QString::fromUtf8(spec.glyph) + "\n" + label);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  }
  button->setCheckable(true);
  button->setMinimumHeight(icon.isNull() ? 64 : 78);
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  button->setProperty("toolCard", true);
  button->setProperty("toolValue", static_cast<int>(spec.tool));
  button->setToolTip(QString::fromUtf8(spec.description));
  layout->addWidget(button, row, column);
  m_tool_group->addButton(button);
  const ToolType tool = spec.tool;
  connect(
      button, &QToolButton::clicked, this, [this, tool]() { set_current_tool(tool); });
  return button;
}

void ToolPanel::apply_filter(const QString& filter) {
  for (const auto& [section, group] : m_sections) {
    bool any_visible = false;
    for (auto* button : group->findChildren<QToolButton*>()) {
      const auto tool = static_cast<ToolType>(button->property("toolValue").toInt());
      const bool visible = tool_matches_filter(tool_spec(tool), filter);
      button->setVisible(visible);
      any_visible = any_visible || visible;
    }
    group->setVisible(any_visible);
  }
}

void ToolPanel::set_current_tool(ToolType tool, bool emit_signal) {
  m_current_tool = tool;

  if (m_tool_group != nullptr) {
    for (auto* button : m_tool_group->buttons()) {
      if (button->property("toolValue").toInt() == static_cast<int>(tool)) {
        if (!button->isChecked()) {
          button->setChecked(true);
        }
        break;
      }
    }
  }

  update_active_tool_label(tool_description(tool));
  if (emit_signal) {
    emit tool_selected(m_current_tool);
  }
}

void ToolPanel::update_active_tool_label(const QString& description) {
  if (m_active_tool_label == nullptr) {
    return;
  }
  m_active_tool_label->setText("Active tool: " + description);
}

void ToolPanel::clear_selection() {
  set_current_tool(ToolType::Select);
}

void ToolPanel::contextMenuEvent(QContextMenuEvent* event) {
  if (m_current_tool == ToolType::Select) {
    QWidget::contextMenuEvent(event);
    return;
  }
  clear_selection();
  event->accept();
}

} // namespace MapEditor
