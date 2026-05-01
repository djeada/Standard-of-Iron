#include "building_panel.h"

#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

namespace {

constexpr int kArenaBuildingLocalOwnerId = 1;
constexpr int kArenaBuildingOpponentOwnerId = 2;

auto prettify_building_identifier(const QString &value) -> QString {
  QString label = value;
  label.replace(QLatin1Char('_'), QLatin1Char(' '));
  QStringList parts = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (QString &part : parts) {
    if (!part.isEmpty()) {
      part[0] = part[0].toUpper();
    }
  }
  return parts.join(QLatin1Char(' '));
}

} // namespace

BuildingPanel::BuildingPanel(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto *spawn_group = new QGroupBox("Spawn Building", this);
  auto *spawn_group_layout = new QVBoxLayout(spawn_group);
  spawn_group_layout->setSpacing(6);

  auto *spawn_form = new QFormLayout();
  spawn_form->setSpacing(4);
  m_owner_box = new QComboBox(spawn_group);
  m_nation_box = new QComboBox(spawn_group);
  m_building_box = new QComboBox(spawn_group);
  m_spawn_count_box = new QSpinBox(spawn_group);

  m_owner_box->addItem(QStringLiteral("Local Player"),
                       kArenaBuildingLocalOwnerId);
  m_owner_box->addItem(QStringLiteral("Arena Opponent"),
                       kArenaBuildingOpponentOwnerId);
  m_spawn_count_box->setRange(1, 16);
  m_spawn_count_box->setValue(1);

  m_building_box->addItem(QStringLiteral("Barracks"),
                          QStringLiteral("barracks"));
  m_building_box->addItem(QStringLiteral("Defense Tower"),
                          QStringLiteral("defense_tower"));
  m_building_box->addItem(QStringLiteral("Home"), QStringLiteral("home"));

  spawn_form->addRow("Side", m_owner_box);
  spawn_form->addRow("Nation", m_nation_box);
  spawn_form->addRow("Building", m_building_box);
  spawn_form->addRow("Spawn Count", m_spawn_count_box);
  spawn_group_layout->addLayout(spawn_form);

  auto *spawn_buttons = new QWidget(spawn_group);
  auto *spawn_buttons_layout = new QHBoxLayout(spawn_buttons);
  spawn_buttons_layout->setContentsMargins(0, 0, 0, 0);
  spawn_buttons_layout->setSpacing(6);
  auto *spawn_button = new QPushButton("Spawn", spawn_group);
  spawn_button->setProperty("primary", true);
  auto *clear_button = new QPushButton("Clear", spawn_group);
  spawn_button->setToolTip("Spawn buildings with selected settings");
  clear_button->setToolTip("Remove all buildings from the arena");
  spawn_buttons_layout->addWidget(spawn_button, 1);
  spawn_buttons_layout->addWidget(clear_button, 1);
  spawn_group_layout->addWidget(spawn_buttons);

  layout->addWidget(spawn_group);

  auto *selection_group = new QGroupBox("Selection", this);
  auto *selection_layout = new QVBoxLayout(selection_group);
  m_selection_summary_label =
      new QLabel(QStringLiteral("No buildings selected."), selection_group);
  m_selection_summary_label->setTextFormat(Qt::PlainText);
  m_selection_summary_label->setWordWrap(true);
  m_selection_summary_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  selection_layout->addWidget(m_selection_summary_label);
  layout->addWidget(selection_group);

  layout->addStretch(1);

  connect(spawn_button, &QPushButton::clicked, this, [this]() {
    emit spawn_buildings_requested(
        m_spawn_count_box != nullptr ? m_spawn_count_box->value() : 1);
  });
  connect(clear_button, &QPushButton::clicked, this,
          &BuildingPanel::clear_buildings_requested);
  connect(m_owner_box, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int) { emit building_owner_selected(selected_owner_id()); });
  connect(m_nation_box, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int) { emit building_nation_selected(selected_nation_id()); });
  connect(m_building_box, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int) {
            emit building_type_selected(selected_building_type_id());
          });

  populate_nation_options();
}

void BuildingPanel::set_selection_summary(const QString &summary) {
  if (m_selection_summary_label == nullptr) {
    return;
  }
  m_selection_summary_label->setText(
      summary.trimmed().isEmpty() ? QStringLiteral("No buildings selected.")
                                  : summary);
}

auto BuildingPanel::selected_owner_id() const -> int {
  return m_owner_box != nullptr ? m_owner_box->currentData().toInt()
                                : kArenaBuildingLocalOwnerId;
}

auto BuildingPanel::selected_nation_id() const -> QString {
  return m_nation_box != nullptr ? m_nation_box->currentData().toString()
                                 : QString();
}

auto BuildingPanel::selected_building_type_id() const -> QString {
  return m_building_box != nullptr ? m_building_box->currentData().toString()
                                   : QStringLiteral("barracks");
}

void BuildingPanel::populate_nation_options() {
  if (m_nation_box == nullptr) {
    return;
  }

  const auto &registry = Game::Systems::NationRegistry::instance();
  const auto &nations = registry.get_all_nations();

  m_nation_box->clear();
  for (const auto &nation : nations) {
    QString const nation_id = Game::Systems::nation_id_to_qstring(nation.id);
    QString label = QString::fromStdString(nation.display_name);
    if (label.trimmed().isEmpty()) {
      label = prettify_building_identifier(nation_id);
    }
    m_nation_box->addItem(label, nation_id);
  }

  if (m_nation_box->count() == 0) {
    return;
  }

  int index = m_nation_box->findData(
      Game::Systems::nation_id_to_qstring(registry.default_nation_id()));
  m_nation_box->setCurrentIndex(index >= 0 ? index : 0);
}
