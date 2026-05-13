#include "tool_panel.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

namespace MapEditor {

namespace {

constexpr int k_max_player_id = 4;

auto toolDescription(ToolType tool) -> QString {
  switch (tool) {
  case ToolType::Select:
    return "Select placed elements, drag them, or double-click to edit.";
  case ToolType::Hill:
    return "Place round or width/depth-based hills.";
  case ToolType::Mountain:
    return "Place high mountains with editable JSON properties.";
  case ToolType::River:
    return "Click a start point, then click an end point to draw a river.";
  case ToolType::Road:
    return "Draw roads and keep waypoint-based roads editable.";
  case ToolType::Bridge:
    return "Add bridges across rivers or other crossings.";
  case ToolType::PropFirecamp:
    return "Place authored fire camps with editable radius and intensity.";
  case ToolType::PropTent:
    return "Place authored tents as explicit world props.";
  case ToolType::PropSupplyCart:
    return "Place authored supply carts as explicit world props.";
  case ToolType::PropWeaponRack:
    return "Place authored weapon racks as explicit world props.";
  case ToolType::PropRuins:
    return "Place authored ruins as explicit world props.";
  case ToolType::PropDeadTree:
    return "Place authored dead trees as explicit world props.";
  case ToolType::PropBoulder:
    return "Place authored boulders as explicit world props.";
  case ToolType::Barracks:
    return "Place barracks and assign them to a player.";
  case ToolType::Village:
    return "Place villages and assign them to a player.";
  case ToolType::Eraser:
    return "Remove the element or segment under the cursor.";
  }

  return "Choose a tool to start editing.";
}

auto createInfoLabel(const QString &text, const QString &object_name,
                     QWidget *parent) -> QLabel * {
  auto *label = new QLabel(text, parent);
  label->setObjectName(object_name);
  label->setWordWrap(true);
  label->setTextFormat(Qt::PlainText);
  return label;
}

} // namespace

ToolPanel::ToolPanel(QWidget *parent) : QWidget(parent) { setup_ui(); }

void ToolPanel::setup_ui() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto *title = createInfoLabel("Map Tools", "panelTitle", this);
  layout->addWidget(title);

  auto *intro = createInfoLabel(
      "Choose a tool, then place or edit content directly on the canvas.",
      "panelIntro", this);
  layout->addWidget(intro);

  m_active_tool_label = createInfoLabel(QString(), "toolSummary", this);
  layout->addWidget(m_active_tool_label);

  m_tool_group = new QButtonGroup(this);
  m_tool_group->setExclusive(true);

  auto *selection_group = new QGroupBox("Selection & Cleanup", this);
  auto *selection_layout = new QGridLayout(selection_group);
  selection_layout->setHorizontalSpacing(6);
  selection_layout->setVerticalSpacing(6);
  m_select_button = add_tool_button(selection_layout, 0, 0, "Select", "⬚",
                                    "Select, drag, and double-click elements.",
                                    ToolType::Select);
  add_tool_button(selection_layout, 0, 1, "Eraser", "🗑",
                  "Remove elements, roads, rivers, and bridges.",
                  ToolType::Eraser);
  layout->addWidget(selection_group);

  auto *terrain_group = new QGroupBox("Terrain", this);
  auto *terrain_layout = new QGridLayout(terrain_group);
  terrain_layout->setHorizontalSpacing(6);
  terrain_layout->setVerticalSpacing(6);
  add_tool_button(terrain_layout, 0, 0, "Hill", "⛰", "Place terrain hills.",
                  ToolType::Hill);
  add_tool_button(terrain_layout, 0, 1, "Mountain", "🏔",
                  "Place mountain peaks.", ToolType::Mountain);
  layout->addWidget(terrain_group);

  auto *props_group = new QGroupBox("World Props", this);
  auto *props_layout = new QGridLayout(props_group);
  props_layout->setHorizontalSpacing(6);
  props_layout->setVerticalSpacing(6);
  add_tool_button(props_layout, 0, 0, "Fire Camp", "🔥",
                  "Place authored fire camps.", ToolType::PropFirecamp);
  add_tool_button(props_layout, 0, 1, "Tent", "⛺", "Place authored tents.",
                  ToolType::PropTent);
  add_tool_button(props_layout, 1, 0, "Supply Cart", "🛒",
                  "Place authored supply carts.", ToolType::PropSupplyCart);
  add_tool_button(props_layout, 1, 1, "Weapon Rack", "⚔",
                  "Place authored weapon racks.", ToolType::PropWeaponRack);
  add_tool_button(props_layout, 2, 0, "Ruins", "🏚", "Place authored ruins.",
                  ToolType::PropRuins);
  add_tool_button(props_layout, 2, 1, "Dead Tree", "🌲",
                  "Place authored dead trees.", ToolType::PropDeadTree);
  add_tool_button(props_layout, 3, 0, "Boulder", "🪨",
                  "Place authored boulders.", ToolType::PropBoulder);
  layout->addWidget(props_group);

  auto *paths_group = new QGroupBox("Paths & Bridges", this);
  auto *paths_layout = new QGridLayout(paths_group);
  paths_layout->setHorizontalSpacing(6);
  paths_layout->setVerticalSpacing(6);
  add_tool_button(paths_layout, 0, 0, "River", "〰",
                  "Draw a river between two points.", ToolType::River);
  add_tool_button(paths_layout, 0, 1, "Road", "═",
                  "Draw a road between two points.", ToolType::Road);
  add_tool_button(paths_layout, 1, 0, "Bridge", "🌉",
                  "Draw a bridge between two points.", ToolType::Bridge);
  layout->addWidget(paths_group);

  auto *structures_group = new QGroupBox("Structures", this);
  auto *structures_layout = new QGridLayout(structures_group);
  structures_layout->setHorizontalSpacing(6);
  structures_layout->setVerticalSpacing(6);
  add_tool_button(structures_layout, 0, 0, "Barracks", "🏛",
                  "Place a barracks and assign a player.", ToolType::Barracks);
  add_tool_button(structures_layout, 0, 1, "Village", "🏘",
                  "Place a village and assign a player.", ToolType::Village);

  auto *player_bar = new QWidget(structures_group);
  auto *player_bar_layout = new QHBoxLayout(player_bar);
  player_bar_layout->setContentsMargins(2, 0, 2, 0);
  player_bar_layout->setSpacing(3);
  player_bar_layout->addWidget(
      createInfoLabel("Player:", "fieldLabel", player_bar));

  m_player_group = new QButtonGroup(this);
  m_player_group->setExclusive(true);
  for (int pid = 0; pid <= k_max_player_id; ++pid) {
    auto *pb = new QToolButton(player_bar);
    pb->setText(pid == 0 ? "N" : QString::number(pid));
    pb->setToolTip(pid == 0 ? "Neutral (no player)"
                            : QString("Player %1").arg(pid));
    pb->setCheckable(true);
    pb->setChecked(pid == 0);
    pb->setMinimumWidth(28);
    pb->setMaximumWidth(36);
    pb->setProperty("playerId", pid);
    m_player_group->addButton(pb);
    player_bar_layout->addWidget(pb);
    connect(pb, &QToolButton::clicked, this, [this, pid]() {
      m_current_player_id = pid;
      emit player_id_changed(pid);
    });
  }
  player_bar_layout->addStretch(1);
  structures_layout->addWidget(player_bar, 1, 0, 1, 2);

  layout->addWidget(structures_group);

  auto *tips_group = new QGroupBox("Editing Tips", this);
  auto *tips_layout = new QVBoxLayout(tips_group);
  tips_layout->setSpacing(4);
  tips_layout->addWidget(
      createInfoLabel("Left click: place or select\n"
                      "Drag: move selected elements or endpoints\n"
                      "Middle click / Ctrl+drag: pan\n"
                      "Mouse wheel: zoom\n"
                      "Right click / Escape: return to Select\n"
                      "Del / Backspace: delete selected\n"
                      "Shift + click/drag: free placement (no snap)\n"
                      "Double-click element: edit JSON\n"
                      "Double-click empty grid: resize map",
                      "panelHint", tips_group));
  layout->addWidget(tips_group);

  layout->addStretch(1);

  set_current_tool(ToolType::Select, false);
  setMinimumWidth(260);
  setMaximumWidth(360);
}

auto ToolPanel::add_tool_button(QGridLayout *layout, int row, int column,
                                const QString &name, const QString &icon_char,
                                const QString &description,
                                ToolType tool) -> QToolButton * {
  auto *button = new QToolButton(this);
  button->setText(icon_char + "\n" + name);
  button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  button->setCheckable(true);
  button->setMinimumHeight(64);
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  button->setProperty("toolCard", true);
  button->setProperty("toolValue", static_cast<int>(tool));
  button->setToolTip(description);
  layout->addWidget(button, row, column);
  m_tool_group->addButton(button);
  connect(button, &QToolButton::clicked, this,
          [this, tool]() { set_current_tool(tool); });
  return button;
}

void ToolPanel::set_current_tool(ToolType tool, bool emit_signal) {
  m_current_tool = tool;

  if (m_tool_group != nullptr) {
    for (auto *button : m_tool_group->buttons()) {
      if (button->property("toolValue").toInt() == static_cast<int>(tool)) {
        if (!button->isChecked()) {
          button->setChecked(true);
        }
        break;
      }
    }
  }

  update_active_tool_label(toolDescription(tool));
  if (emit_signal) {
    emit tool_selected(m_current_tool);
  }
}

void ToolPanel::update_active_tool_label(const QString &description) {
  if (m_active_tool_label == nullptr) {
    return;
  }
  m_active_tool_label->setText("Active tool: " + description);
}

void ToolPanel::clear_selection() { set_current_tool(ToolType::Select); }

} // namespace MapEditor
