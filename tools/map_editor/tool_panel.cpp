#include "tool_panel.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

#include "spawn_icon_library.h"
#include "troop_tool_specs.h"

namespace MapEditor {

namespace {

constexpr int k_max_player_id = 4;

auto toolDescription(ToolType tool) -> QString {
  if (const auto* spec = troop_tool_spec(tool)) {
    return QString::fromLatin1(spec->description);
  }

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
  case ToolType::PropMagicShrine:
    return "Place authored magic shrines as explicit world props.";
  case ToolType::PropDeadTree:
    return "Place authored dead trees as explicit world props.";
  case ToolType::PropBoulder:
    return "Place authored boulders as explicit world props.";
  case ToolType::PropPineTree:
    return "Place authored pine trees as explicit world props.";
  case ToolType::PropOliveTree:
    return "Place authored olive trees as explicit world props.";
  case ToolType::PropPlant:
    return "Place authored ground plants as explicit world props.";
  case ToolType::PropIronOre:
    return "Place authored iron ore deposits as explicit world props.";
  case ToolType::Barracks:
    return "Place barracks and assign them to a player.";
  case ToolType::Village:
    return "Place villages and assign them to a player.";
  case ToolType::DefenseTower:
    return "Place a defense tower and assign it to a player.";
  case ToolType::Home:
    return "Place a home building and assign it to a player.";
  case ToolType::Marketplace:
    return "Place a marketplace building and assign it to a player.";
  case ToolType::Wall:
    return "Draw a wall segment between two points and assign it to a player.";
  case ToolType::Eraser:
    return "Remove the element or segment under the cursor.";
  case ToolType::TroopArcher:
  case ToolType::TroopSwordsman:
  case ToolType::TroopSpearman:
  case ToolType::TroopHorseSwordsman:
  case ToolType::TroopHorseArcher:
  case ToolType::TroopHorseSpearman:
  case ToolType::TroopHealer:
  case ToolType::TroopCatapult:
  case ToolType::TroopBallista:
  case ToolType::TroopElephant:
  case ToolType::TroopRomanLegionOrganizer:
  case ToolType::TroopRomanVeteranConsul:
  case ToolType::TroopRomanFieldCommander:
  case ToolType::TroopCarthageMercenaryBroker:
  case ToolType::TroopCarthageCavalryPatron:
  case ToolType::TroopCarthageElephantMaster:
  case ToolType::TroopSkeletonSwordsman:
  case ToolType::TroopSkeletonArcher:
  case ToolType::TroopGravePriest:
  case ToolType::TroopCivilian:
  case ToolType::TroopBuilder:
    break;
  case ToolType::UndeadZone:
    return "Place an undead zone (shrine or ruins anchor with skeleton wave spawns). "
           "Double-click to edit waves.";
  }

  return "Choose a tool to start editing.";
}

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

  m_tool_group = new QButtonGroup(this);
  m_tool_group->setExclusive(true);

  const auto add_troop_buttons_for_section = [this](QGridLayout* section_layout,
                                                    const char* section_name) {
    int index = 0;
    for (const auto& spec : k_troop_tool_specs) {
      if (QString::fromLatin1(spec.section) != QString::fromLatin1(section_name)) {
        continue;
      }

      add_tool_button(section_layout,
                      index / 2,
                      index % 2,
                      QString::fromLatin1(spec.name),
                      QString(),
                      QString::fromLatin1(spec.description),
                      spec.tool,
                      troop_tool_icon(QString::fromLatin1(spec.type)));
      ++index;
    }
  };

  auto* selection_group = new QGroupBox("Selection & Cleanup", this);
  auto* selection_layout = new QGridLayout(selection_group);
  selection_layout->setHorizontalSpacing(6);
  selection_layout->setVerticalSpacing(6);
  m_select_button = add_tool_button(selection_layout,
                                    0,
                                    0,
                                    "Select",
                                    "⬚",
                                    "Select, drag, and double-click elements.",
                                    ToolType::Select);
  add_tool_button(selection_layout,
                  0,
                  1,
                  "Eraser",
                  "🗑",
                  "Remove elements, roads, rivers, and bridges.",
                  ToolType::Eraser);
  layout->addWidget(selection_group);

  auto* terrain_group = new QGroupBox("Terrain", this);
  auto* terrain_layout = new QGridLayout(terrain_group);
  terrain_layout->setHorizontalSpacing(6);
  terrain_layout->setVerticalSpacing(6);
  add_tool_button(
      terrain_layout, 0, 0, "Hill", "⛰", "Place terrain hills.", ToolType::Hill);
  add_tool_button(terrain_layout,
                  0,
                  1,
                  "Mountain",
                  "🏔",
                  "Place mountain peaks.",
                  ToolType::Mountain);
  layout->addWidget(terrain_group);

  auto* props_group = new QGroupBox("World Props", this);
  auto* props_layout = new QGridLayout(props_group);
  props_layout->setHorizontalSpacing(6);
  props_layout->setVerticalSpacing(6);
  add_tool_button(props_layout,
                  0,
                  0,
                  "Fire Camp",
                  "🔥",
                  "Place authored fire camps.",
                  ToolType::PropFirecamp);
  add_tool_button(
      props_layout, 0, 1, "Tent", "⛺", "Place authored tents.", ToolType::PropTent);
  add_tool_button(props_layout,
                  1,
                  0,
                  "Supply Cart",
                  "🛒",
                  "Place authored supply carts.",
                  ToolType::PropSupplyCart);
  add_tool_button(props_layout,
                  1,
                  1,
                  "Weapon Rack",
                  "⚔",
                  "Place authored weapon racks.",
                  ToolType::PropWeaponRack);
  add_tool_button(
      props_layout, 2, 0, "Ruins", "🏚", "Place authored ruins.", ToolType::PropRuins);
  add_tool_button(props_layout,
                  2,
                  1,
                  "Magic Shrine",
                  "✦",
                  "Place authored magic shrines.",
                  ToolType::PropMagicShrine);
  add_tool_button(props_layout,
                  3,
                  0,
                  "Dead Tree",
                  "🌲",
                  "Place authored dead trees.",
                  ToolType::PropDeadTree);
  add_tool_button(props_layout,
                  3,
                  1,
                  "Boulder",
                  "🪨",
                  "Place authored boulders.",
                  ToolType::PropBoulder);
  add_tool_button(props_layout,
                  4,
                  0,
                  "Pine Tree",
                  "♠",
                  "Place authored pine trees.",
                  ToolType::PropPineTree);
  add_tool_button(props_layout,
                  4,
                  1,
                  "Olive Tree",
                  "♣",
                  "Place authored olive trees.",
                  ToolType::PropOliveTree);
  add_tool_button(props_layout,
                  5,
                  0,
                  "Plant",
                  "❧",
                  "Place authored ground plants.",
                  ToolType::PropPlant);
  add_tool_button(props_layout,
                  5,
                  1,
                  "Iron Ore",
                  "◆",
                  "Place authored iron ore deposits.",
                  ToolType::PropIronOre);
  layout->addWidget(props_group);

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
  connect(m_nation_box,
          qOverload<int>(&QComboBox::currentIndexChanged),
          this,
          [this](int index) {
            m_current_nation = m_nation_box->itemData(index).toString();
            emit nation_changed(m_current_nation);
          });
  ownership_layout->addWidget(m_nation_box);
  ownership_layout->addStretch(1);
  layout->addWidget(ownership_group);

  auto* infantry_group = new QGroupBox("Troops: Infantry & Support", this);
  auto* infantry_layout = new QGridLayout(infantry_group);
  infantry_layout->setHorizontalSpacing(6);
  infantry_layout->setVerticalSpacing(6);
  add_troop_buttons_for_section(infantry_layout, "Infantry & Support");
  layout->addWidget(infantry_group);

  auto* mounted_group = new QGroupBox("Troops: Mounted & Siege", this);
  auto* mounted_layout = new QGridLayout(mounted_group);
  mounted_layout->setHorizontalSpacing(6);
  mounted_layout->setVerticalSpacing(6);
  add_troop_buttons_for_section(mounted_layout, "Mounted & Siege");
  layout->addWidget(mounted_group);

  auto* command_group = new QGroupBox("Troops: Command", this);
  auto* command_layout = new QGridLayout(command_group);
  command_layout->setHorizontalSpacing(6);
  command_layout->setVerticalSpacing(6);
  add_troop_buttons_for_section(command_layout, "Command");
  layout->addWidget(command_group);

  auto* sepulcher_group = new QGroupBox("Troops: Iron Sepulcher", this);
  auto* sepulcher_layout = new QGridLayout(sepulcher_group);
  sepulcher_layout->setHorizontalSpacing(6);
  sepulcher_layout->setVerticalSpacing(6);
  add_troop_buttons_for_section(sepulcher_layout, "Sepulcher");
  add_tool_button(
      sepulcher_layout,
      (sepulcher_layout->rowCount()),
      0,
      "Undead Zone",
      "☠",
      "Place an undead zone (shrine or ruins anchor with skeleton wave spawns). "
      "Double-click to edit waves.",
      ToolType::UndeadZone);
  layout->addWidget(sepulcher_group);

  auto* paths_group = new QGroupBox("Paths & Bridges", this);
  auto* paths_layout = new QGridLayout(paths_group);
  paths_layout->setHorizontalSpacing(6);
  paths_layout->setVerticalSpacing(6);
  add_tool_button(paths_layout,
                  0,
                  0,
                  "River",
                  "〰",
                  "Draw a river between two points.",
                  ToolType::River);
  add_tool_button(paths_layout,
                  0,
                  1,
                  "Road",
                  "═",
                  "Draw a road between two points.",
                  ToolType::Road);
  add_tool_button(paths_layout,
                  1,
                  0,
                  "Bridge",
                  "🌉",
                  "Draw a bridge between two points.",
                  ToolType::Bridge);
  add_tool_button(paths_layout,
                  1,
                  1,
                  "Wall",
                  "🧱",
                  "Draw a wall segment between two points and assign a player.",
                  ToolType::Wall);
  layout->addWidget(paths_group);

  auto* structures_group = new QGroupBox("Structures", this);
  auto* structures_layout = new QGridLayout(structures_group);
  structures_layout->setHorizontalSpacing(6);
  structures_layout->setVerticalSpacing(6);
  add_tool_button(structures_layout,
                  0,
                  0,
                  "Barracks",
                  "🏛",
                  "Place a barracks and assign a player.",
                  ToolType::Barracks);
  add_tool_button(structures_layout,
                  0,
                  1,
                  "Village",
                  "🏘",
                  "Place a village and assign a player.",
                  ToolType::Village);
  add_tool_button(structures_layout,
                  1,
                  0,
                  "Tower",
                  "🗼",
                  "Place a defense tower and assign a player.",
                  ToolType::DefenseTower);
  add_tool_button(structures_layout,
                  1,
                  1,
                  "Home",
                  "🏠",
                  "Place a home building and assign a player.",
                  ToolType::Home);
  add_tool_button(structures_layout,
                  2,
                  0,
                  "Marketplace",
                  "⚖",
                  "Place a marketplace and assign a player.",
                  ToolType::Marketplace);

  layout->addWidget(structures_group);

  auto* tips_group = new QGroupBox("Editing Tips", this);
  auto* tips_layout = new QVBoxLayout(tips_group);
  tips_layout->setSpacing(4);
  tips_layout->addWidget(
      createInfoLabel("Left click: place or select\n"
                      "Drag: move selected elements or endpoints\n"
                      "Middle click / Ctrl+drag: pan\n"
                      "Mouse wheel: zoom\n"
                      "Right click / Escape: return to Select\n"
                      "Del / Backspace: delete selected\n"
                      "Shift + click/drag: free placement (no snap)\n"
                      "Double-click element: edit JSON (hills include a top-grid)\n"
                      "Double-click empty grid: resize map",
                      "panelHint",
                      tips_group));
  layout->addWidget(tips_group);

  layout->addStretch(1);

  set_current_tool(ToolType::Select, false);
  setMinimumWidth(260);
  setMaximumWidth(360);
}

auto ToolPanel::add_tool_button(QGridLayout* layout,
                                int row,
                                int column,
                                const QString& name,
                                const QString& icon_char,
                                const QString& description,
                                ToolType tool,
                                const QIcon& icon) -> QToolButton* {
  auto* button = new QToolButton(this);
  if (!icon.isNull()) {
    button->setIcon(icon);
    button->setIconSize(QSize(28, 28));
    button->setText(name);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  } else {
    button->setText(icon_char + "\n" + name);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  }
  button->setCheckable(true);
  button->setMinimumHeight(icon.isNull() ? 64 : 78);
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  button->setProperty("toolCard", true);
  button->setProperty("toolValue", static_cast<int>(tool));
  button->setToolTip(description);
  layout->addWidget(button, row, column);
  m_tool_group->addButton(button);
  connect(
      button, &QToolButton::clicked, this, [this, tool]() { set_current_tool(tool); });
  return button;
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

  update_active_tool_label(toolDescription(tool));
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

} // namespace MapEditor
