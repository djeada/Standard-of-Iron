#include "tool_panel.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

#include "spawn_icon_library.h"
#include "ui/theme.h"

namespace MapEditor {

namespace {

using Section = ToolSection;

constexpr auto k_sections = std::to_array({
    Section::Paths,
    Section::Terrain,
    Section::Structures,
    Section::Fortifications,
    Section::WorldProps,
    Section::TroopsInfantry,
    Section::TroopsMounted,
    Section::TroopsCommand,
    Section::Sepulcher,
    Section::Wildlife,
});

enum Category {
  Ground,
  Buildings,
  Props,
  Troops,
  Wildlife
};

auto category_for(Section section) -> int {
  switch (section) {
  case Section::Paths:
  case Section::Terrain:
    return Ground;
  case Section::Structures:
  case Section::Fortifications:
    return Buildings;
  case Section::WorldProps:
    return Props;
  case Section::TroopsInfantry:
  case Section::TroopsMounted:
  case Section::TroopsCommand:
  case Section::Sepulcher:
    return Troops;
  case Section::Wildlife:
    return Wildlife;
  case Section::Selection:
    return -1;
  }
  return -1;
}

auto has_placement_owner(ToolType tool) -> bool {
  const auto placement = tool_placement(tool);
  return placement == ToolPlacement::Troop || placement == ToolPlacement::Structure ||
         placement == ToolPlacement::TownBlock || placement == ToolPlacement::Gate ||
         tool == ToolType::Wall;
}

} // namespace

ToolPanel::ToolPanel(QWidget* parent)
    : QWidget(parent) {
  setup_ui();
}

void ToolPanel::setup_ui() {
  setObjectName(QStringLiteral("battlefieldTools"));
  setStyleSheet(
      QStringLiteral(
          "#battlefieldTools QToolButton{min-height:24px;padding:3px 8px;}"
          "#battlefieldTools QLineEdit,#battlefieldTools QComboBox{min-height:22px;}"
          "#battlefieldTools QLabel#toolSection{font-weight:700;color:%1;}"
          "#battlefieldTools QFrame#placementOwner{border-top:1px solid %2;}")
          .arg(Theme::accentBright().name(), Theme::borderSubtle().name()));

  auto* layout = new QVBoxLayout(this);
  const int margin = Theme::spacingMedium();
  const int spacing = Theme::spacingSmall();
  layout->setContentsMargins(margin, margin, margin, margin);
  layout->setSpacing(spacing);

  m_tool_group = new QButtonGroup(this);
  m_tool_group->setExclusive(true);
  auto* modes = new QHBoxLayout;
  modes->setSpacing(spacing);
  for (const ToolSpec* spec : tools_in_section(Section::Selection)) {
    modes->addWidget(create_tool_button(*spec, this));
  }
  layout->addLayout(modes);

  m_filter_edit = new QLineEdit(this);
  m_filter_edit->setPlaceholderText(QStringLiteral("Find a placement tool…"));
  m_filter_edit->setAccessibleName(QStringLiteral("Find a placement tool"));
  m_filter_edit->setClearButtonEnabled(true);
  layout->addWidget(m_filter_edit);

  m_category = new QComboBox(this);
  m_category->setAccessibleName(QStringLiteral("Tool category"));
  m_category->addItems({QStringLiteral("Terrain & crossings"),
                        QStringLiteral("Buildings & fortifications"),
                        QStringLiteral("World props"),
                        QStringLiteral("Troops"),
                        QStringLiteral("Wildlife")});
  layout->addWidget(m_category);

  m_tool_scroll = new QScrollArea(this);
  m_tool_scroll->setWidgetResizable(true);
  m_tool_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* contents = new QWidget(m_tool_scroll);
  auto* sections = new QVBoxLayout(contents);
  sections->setContentsMargins(0, 0, 0, 0);
  sections->setSpacing(Theme::spacingMedium());
  for (const Section section : k_sections) {
    auto* group = new QWidget(contents);
    auto* group_layout = new QVBoxLayout(group);
    group_layout->setContentsMargins(0, 0, 0, 0);
    group_layout->setSpacing(Theme::spacingTiny());
    auto* heading = new QLabel(section_title(section), group);
    heading->setObjectName(QStringLiteral("toolSection"));
    heading->setTextFormat(Qt::PlainText);
    group_layout->addWidget(heading);

    auto* grid = new QGridLayout;
    grid->setSpacing(Theme::spacingTiny());
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    group_layout->addLayout(grid);
    SectionGroup entry{section, group, grid, {}};
    for (const ToolSpec* spec : tools_in_section(section)) {
      entry.buttons.append(create_tool_button(*spec, group));
    }
    m_sections.append(std::move(entry));
    sections->addWidget(group);
  }
  m_no_matches = new QLabel(QStringLiteral("No matching placement tools"), contents);
  m_no_matches->setProperty("status", "muted");
  sections->addWidget(m_no_matches);
  sections->addStretch();
  m_tool_scroll->setWidget(contents);
  layout->addWidget(m_tool_scroll, 1);

  m_ownership = create_ownership_group();
  layout->addWidget(m_ownership);

  connect(m_filter_edit, &QLineEdit::textChanged, this, &ToolPanel::apply_filter);
  connect(m_category, &QComboBox::currentIndexChanged, this, [this]() {
    apply_filter(m_filter_edit->text());
  });
  set_current_tool(ToolType::Select, false);
  apply_filter({});
}

auto ToolPanel::create_ownership_group() -> QWidget* {
  auto* group = new QFrame(this);
  group->setObjectName(QStringLiteral("placementOwner"));
  auto* layout = new QFormLayout(group);
  layout->setContentsMargins(0, Theme::spacingSmall(), 0, 0);
  layout->setSpacing(Theme::spacingTiny());
  layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  auto* player = new QComboBox(group);
  player->addItem(QStringLiteral("Neutral"), 0);
  for (int id = 1; id <= 4; ++id) {
    player->addItem(QStringLiteral("Player %1").arg(id), id);
  }
  layout->addRow(QStringLiteral("Place for"), player);
  connect(player, &QComboBox::currentIndexChanged, this, [this, player]() {
    m_current_player_id = player->currentData().toInt();
    emit player_id_changed(m_current_player_id);
  });

  auto* nation = new QComboBox(group);
  nation->addItem(QStringLiteral("Owner default"), QString());
  nation->addItem(QStringLiteral("Roman"), QStringLiteral("roman_republic"));
  nation->addItem(QStringLiteral("Carthage"), QStringLiteral("carthage"));
  nation->addItem(QStringLiteral("Sepulcher"), QStringLiteral("iron_sepulcher"));
  nation->addItem(QStringLiteral("Gallic allies"), QStringLiteral("gauls"));
  nation->addItem(QStringLiteral("Iberian allies"), QStringLiteral("iberians"));
  layout->addRow(QStringLiteral("Nation"), nation);
  connect(nation, &QComboBox::currentIndexChanged, this, [this, nation]() {
    m_current_nation = nation->currentData().toString();
    emit nation_changed(m_current_nation);
  });
  return group;
}

auto ToolPanel::create_tool_button(const ToolSpec& spec,
                                   QWidget* parent) -> QToolButton* {
  auto* button = new QToolButton(parent);
  button->setText(tool_card_label(spec.tool));
  button->setCheckable(true);
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  button->setToolTip(tool_description(spec.tool));
  if (spec.placement == ToolPlacement::Troop) {
    button->setIcon(troop_tool_icon(QString::fromLatin1(spec.element_type)));
    button->setIconSize(QSize(20, 20));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  } else {
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  }

  const ToolType tool = spec.tool;
  m_tool_group->addButton(button, static_cast<int>(tool));
  connect(
      button, &QToolButton::clicked, this, [this, tool]() { set_current_tool(tool); });
  return button;
}

void ToolPanel::apply_filter(const QString& filter) {
  const bool searching = !filter.trimmed().isEmpty();
  m_category->setEnabled(!searching);
  bool any = false;
  for (const SectionGroup& section : m_sections) {
    const bool in_category =
        searching || m_category->currentIndex() == category_for(section.section);
    int index = 0;
    for (auto* button : section.buttons) {
      const auto tool = static_cast<ToolType>(m_tool_group->id(button));
      const bool visible = in_category && tool_matches_filter(tool_spec(tool), filter);
      section.grid->removeWidget(button);
      if (visible) {
        section.grid->addWidget(button, index / 2, index % 2);
        ++index;
      }
      button->setVisible(visible);
    }
    section.group->setVisible(index > 0);
    any |= index > 0;
  }
  m_no_matches->setVisible(!any);
  m_tool_scroll->verticalScrollBar()->setValue(0);
}

void ToolPanel::set_current_tool(ToolType tool, bool emit_signal) {
  m_current_tool = tool;
  if (auto* button = m_tool_group->button(static_cast<int>(tool))) {
    button->setChecked(true);
  }
  m_ownership->setVisible(has_placement_owner(tool));
  if (emit_signal) {
    emit tool_selected(tool);
  }
}

void ToolPanel::clear_selection(bool emit_signal) {
  set_current_tool(ToolType::Select, emit_signal);
}

} // namespace MapEditor
