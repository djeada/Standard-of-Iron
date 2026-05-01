#include "unit_panel.h"

#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/units/troop_type.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

namespace {

constexpr int kArenaLocalOwnerId = 1;
constexpr int kArenaOpponentOwnerId = 2;

auto prettifyIdentifier(const QString &value) -> QString {
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

UnitPanel::UnitPanel(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  // --- Spawn group ---
  auto *spawn_group = new QGroupBox("Spawn", this);
  auto *spawn_group_layout = new QVBoxLayout(spawn_group);
  spawn_group_layout->setSpacing(6);

  auto *spawn_form = new QFormLayout();
  spawn_form->setSpacing(4);
  m_owner_box = new QComboBox(spawn_group);
  m_nation_box = new QComboBox(spawn_group);
  m_unit_box = new QComboBox(spawn_group);
  m_spawn_count_box = new QSpinBox(spawn_group);
  m_individuals_per_unit_box = new QSpinBox(spawn_group);
  m_render_rider_checkbox = new QCheckBox("Render Rider On Mounted Units", spawn_group);

  m_owner_box->addItem(QStringLiteral("Local Player"), kArenaLocalOwnerId);
  m_owner_box->addItem(QStringLiteral("Arena Opponent"), kArenaOpponentOwnerId);
  m_spawn_count_box->setRange(1, 64);
  m_spawn_count_box->setValue(1);
  m_individuals_per_unit_box->setRange(0, 128);
  m_individuals_per_unit_box->setSpecialValueText(QStringLiteral("Default"));
  m_individuals_per_unit_box->setValue(0);
  m_render_rider_checkbox->setChecked(true);

  spawn_form->addRow("Side", m_owner_box);
  spawn_form->addRow("Nation", m_nation_box);
  spawn_form->addRow("Unit", m_unit_box);
  spawn_form->addRow("Spawn Count", m_spawn_count_box);
  spawn_form->addRow("Members / Unit", m_individuals_per_unit_box);
  spawn_form->addRow("", m_render_rider_checkbox);
  spawn_group_layout->addLayout(spawn_form);

  auto *spawn_buttons = new QWidget(spawn_group);
  auto *spawn_buttons_layout = new QHBoxLayout(spawn_buttons);
  spawn_buttons_layout->setContentsMargins(0, 0, 0, 0);
  spawn_buttons_layout->setSpacing(6);
  auto *spawn_button = new QPushButton("Spawn", spawn_group);
  spawn_button->setProperty("primary", true);
  auto *clear_button = new QPushButton("Clear", spawn_group);
  auto *apply_visuals_button = new QPushButton("Apply Overrides", spawn_group);
  spawn_button->setToolTip("Spawn units with selected settings");
  clear_button->setToolTip("Remove all units from the arena");
  apply_visuals_button->setToolTip("Apply visual overrides to selected units");
  spawn_buttons_layout->addWidget(spawn_button, 1);
  spawn_buttons_layout->addWidget(clear_button, 1);
  spawn_buttons_layout->addWidget(apply_visuals_button, 1);
  spawn_group_layout->addWidget(spawn_buttons);

  layout->addWidget(spawn_group);

  // --- Animation group ---
  auto *anim_group = new QGroupBox("Animation", this);
  auto *anim_group_layout = new QVBoxLayout(anim_group);
  anim_group_layout->setSpacing(6);

  auto *anim_form = new QFormLayout();
  anim_form->setSpacing(4);
  auto *animation_box = new QComboBox(anim_group);
  animation_box->addItems({QStringLiteral("Idle"), QStringLiteral("Walk"),
                           QStringLiteral("Attack"), QStringLiteral("Death")});

  auto *speed_container = new QWidget(anim_group);
  auto *speed_layout = new QHBoxLayout(speed_container);
  speed_layout->setContentsMargins(0, 0, 0, 0);
  auto *speed_slider = new QSlider(Qt::Horizontal, speed_container);
  auto *speed_spin = new QDoubleSpinBox(speed_container);
  speed_slider->setRange(5, 60);
  speed_slider->setValue(22);
  speed_spin->setRange(0.5, 6.0);
  speed_spin->setDecimals(2);
  speed_spin->setSingleStep(0.1);
  speed_spin->setValue(2.2);
  speed_layout->addWidget(speed_slider, 1);
  speed_layout->addWidget(speed_spin);

  anim_form->addRow("Animation", animation_box);
  anim_form->addRow("Speed", speed_container);
  anim_group_layout->addLayout(anim_form);

  auto *anim_buttons = new QWidget(anim_group);
  auto *anim_buttons_layout = new QHBoxLayout(anim_buttons);
  anim_buttons_layout->setContentsMargins(0, 0, 0, 0);
  anim_buttons_layout->setSpacing(6);
  auto *play_button = new QPushButton("Play", anim_group);
  auto *move_button = new QPushButton("Move", anim_group);
  anim_buttons_layout->addWidget(play_button, 1);
  anim_buttons_layout->addWidget(move_button, 1);
  anim_group_layout->addWidget(anim_buttons);

  m_pause_checkbox = new QCheckBox("Pause Simulation", anim_group);
  auto *skeleton_debug_box = new QCheckBox("Skeleton / Pose Overlay", anim_group);
  anim_group_layout->addWidget(m_pause_checkbox);
  anim_group_layout->addWidget(skeleton_debug_box);

  layout->addWidget(anim_group);

  // --- Quick Setup group ---
  auto *quick_setup_group = new QGroupBox("Quick Setup", this);
  auto *quick_setup_layout = new QVBoxLayout(quick_setup_group);
  quick_setup_layout->setSpacing(4);
  auto *spawn_opposing_button = new QPushButton("Spawn Opposing Batch", quick_setup_group);
  auto *spawn_mirror_button = new QPushButton("Spawn Mirror Match", quick_setup_group);
  auto *reset_arena_button = new QPushButton("Reset Arena", quick_setup_group);
  quick_setup_layout->addWidget(spawn_opposing_button);
  quick_setup_layout->addWidget(spawn_mirror_button);
  quick_setup_layout->addWidget(reset_arena_button);
  layout->addWidget(quick_setup_group);

  // --- Selection group ---
  auto *selection_group = new QGroupBox("Selection", this);
  auto *selection_layout = new QVBoxLayout(selection_group);
  m_selection_summary_label =
      new QLabel(QStringLiteral("No units selected."), selection_group);
  m_selection_summary_label->setTextFormat(Qt::PlainText);
  m_selection_summary_label->setWordWrap(true);
  m_selection_summary_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  selection_layout->addWidget(m_selection_summary_label);
  layout->addWidget(selection_group);

  layout->addStretch(1);

  // --- Connections ---
  connect(spawn_button, &QPushButton::clicked, this, [this]() {
    emit spawnIndividualsPerUnitChanged(
        m_individuals_per_unit_box != nullptr
            ? m_individuals_per_unit_box->value()
            : 0);
    emit spawnRiderVisibilityChanged(m_render_rider_checkbox == nullptr ||
                                     m_render_rider_checkbox->isChecked());
    emit spawnUnitsRequested(
        m_spawn_count_box != nullptr ? m_spawn_count_box->value() : 1);
  });
  connect(clear_button, &QPushButton::clicked, this,
          &UnitPanel::clearUnitsRequested);
  connect(m_owner_box, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int) { emit spawnOwnerSelected(selectedOwnerId()); });
  connect(m_nation_box, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int) {
            if (m_nation_box == nullptr) {
              return;
            }
            const QString nation_id = selectedNationId();
            populateUnitOptions(nation_id);
            emit nationSelected(nation_id);
          });
  connect(m_unit_box, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int) { emit unitTypeSelected(selectedUnitTypeId()); });
  connect(m_individuals_per_unit_box, qOverload<int>(&QSpinBox::valueChanged),
          this, &UnitPanel::spawnIndividualsPerUnitChanged);
  connect(m_render_rider_checkbox, &QCheckBox::toggled, this,
          &UnitPanel::spawnRiderVisibilityChanged);
  connect(animation_box, &QComboBox::currentTextChanged, this,
          &UnitPanel::animationSelected);
  connect(play_button, &QPushButton::clicked, this,
          &UnitPanel::playAnimationRequested);
  connect(apply_visuals_button, &QPushButton::clicked, this, [this]() {
    emit spawnIndividualsPerUnitChanged(
        m_individuals_per_unit_box != nullptr
            ? m_individuals_per_unit_box->value()
            : 0);
    emit spawnRiderVisibilityChanged(m_render_rider_checkbox == nullptr ||
                                     m_render_rider_checkbox->isChecked());
    emit applyVisualOverridesRequested();
  });
  connect(spawn_opposing_button, &QPushButton::clicked, this, [this]() {
    emit spawnOpposingBatchRequested(
        m_spawn_count_box != nullptr ? m_spawn_count_box->value() : 1);
  });
  connect(spawn_mirror_button, &QPushButton::clicked, this, [this]() {
    emit spawnMirrorMatchRequested(
        m_spawn_count_box != nullptr ? m_spawn_count_box->value() : 1);
  });
  connect(reset_arena_button, &QPushButton::clicked, this,
          &UnitPanel::resetArenaRequested);
  connect(m_pause_checkbox, &QCheckBox::toggled, this,
          &UnitPanel::animationPausedToggled);
  connect(move_button, &QPushButton::clicked, this,
          &UnitPanel::moveSelectedUnitRequested);
  connect(skeleton_debug_box, &QCheckBox::toggled, this,
          &UnitPanel::skeletonDebugToggled);

  connect(speed_slider, &QSlider::valueChanged, speed_spin,
          [speed_spin](int value) {
            const QSignalBlocker blocker(speed_spin);
            speed_spin->setValue(static_cast<double>(value) / 10.0);
          });
  connect(speed_spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
          speed_slider, [speed_slider](double value) {
            const QSignalBlocker blocker(speed_slider);
            speed_slider->setValue(static_cast<int>(value * 10.0));
          });
  connect(speed_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          [this](double value) {
            emit movementSpeedChanged(static_cast<float>(value));
          });

  populateNationOptions();
  populateUnitOptions(selectedNationId());
}

void UnitPanel::setAnimationPaused(bool paused) {
  if (m_pause_checkbox == nullptr || m_pause_checkbox->isChecked() == paused) {
    return;
  }
  const QSignalBlocker blocker(m_pause_checkbox);
  m_pause_checkbox->setChecked(paused);
}

void UnitPanel::setSelectionSummary(const QString &summary) {
  if (m_selection_summary_label == nullptr) {
    return;
  }
  m_selection_summary_label->setText(summary.trimmed().isEmpty()
                                         ? QStringLiteral("No units selected.")
                                         : summary);
}

auto UnitPanel::selectedOwnerId() const -> int {
  return m_owner_box != nullptr ? m_owner_box->currentData().toInt()
                                : kArenaLocalOwnerId;
}

auto UnitPanel::selectedNationId() const -> QString {
  return m_nation_box != nullptr ? m_nation_box->currentData().toString()
                                 : QString();
}

auto UnitPanel::selectedUnitTypeId() const -> QString {
  return m_unit_box != nullptr ? m_unit_box->currentData().toString()
                               : QString();
}

void UnitPanel::populateNationOptions() {
  if (m_nation_box == nullptr) {
    return;
  }

  QString const preferred = selectedNationId();
  const auto &registry = Game::Systems::NationRegistry::instance();
  const auto &nations = registry.get_all_nations();

  m_nation_box->clear();
  for (const auto &nation : nations) {
    QString const nation_id = Game::Systems::nation_id_to_qstring(nation.id);
    QString label = QString::fromStdString(nation.display_name);
    if (label.trimmed().isEmpty()) {
      label = prettifyIdentifier(nation_id);
    }
    m_nation_box->addItem(label, nation_id);
  }

  if (m_nation_box->count() == 0) {
    return;
  }

  int index = preferred.isEmpty() ? -1 : m_nation_box->findData(preferred);
  if (index < 0) {
    index = m_nation_box->findData(
        Game::Systems::nation_id_to_qstring(registry.default_nation_id()));
  }
  m_nation_box->setCurrentIndex(index >= 0 ? index : 0);
}

void UnitPanel::populateUnitOptions(const QString &nationId,
                                    const QString &preferredUnitType) {
  if (m_unit_box == nullptr) {
    return;
  }

  Game::Systems::NationID parsed_nation_id{};
  if (!Game::Systems::try_parse_nation_id(nationId, parsed_nation_id)) {
    m_unit_box->clear();
    return;
  }

  const auto *nation =
      Game::Systems::NationRegistry::instance().get_nation(parsed_nation_id);
  if (nation == nullptr) {
    m_unit_box->clear();
    return;
  }

  QString preferred =
      preferredUnitType.isEmpty() ? selectedUnitTypeId() : preferredUnitType;

  m_unit_box->clear();
  for (const auto &troop : nation->available_troops) {
    QString const troop_id = Game::Units::troop_typeToQString(troop.unit_type);
    QString label = QString::fromStdString(troop.display_name);
    if (label.trimmed().isEmpty()) {
      label = prettifyIdentifier(troop_id);
    }
    m_unit_box->addItem(label, troop_id);
  }

  if (m_unit_box->count() == 0) {
    return;
  }

  int index = preferred.isEmpty() ? -1 : m_unit_box->findData(preferred);
  m_unit_box->setCurrentIndex(index >= 0 ? index : 0);
}
