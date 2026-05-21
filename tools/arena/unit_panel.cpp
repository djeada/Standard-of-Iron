#include "unit_panel.h"

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

#include <cmath>

#include "arena_scenarios.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/units/troop_type.h"
#include "unit_spawn_options.h"

namespace {

constexpr int k_arena_local_owner_id = 1;
constexpr int k_arena_opponent_owner_id = 2;

auto prettify_identifier(const QString& value) -> QString {
  QString label = value;
  label.replace(QLatin1Char('_'), QLatin1Char(' '));
  QStringList parts = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (QString& part : parts) {
    if (!part.isEmpty()) {
      part[0] = part[0].toUpper();
    }
  }
  return parts.join(QLatin1Char(' '));
}

} // namespace

UnitPanel::UnitPanel(QWidget* parent)
    : QWidget(parent)
    , m_saved_manual_individuals_per_unit(m_individuals_per_unit_box->value())
    , m_saved_manual_rider_visible(m_render_rider_checkbox->isChecked()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto* spawn_group = new QGroupBox("Spawn", this);
  auto* spawn_group_layout = new QVBoxLayout(spawn_group);
  spawn_group_layout->setSpacing(6);

  auto* spawn_form = new QFormLayout();
  spawn_form->setSpacing(4);
  m_owner_box = new QComboBox(spawn_group);
  m_nation_box = new QComboBox(spawn_group);
  m_unit_box = new QComboBox(spawn_group);
  m_spawn_count_box = new QSpinBox(spawn_group);
  m_individuals_per_unit_box = new QSpinBox(spawn_group);
  m_render_rider_checkbox = new QCheckBox("Render Rider On Mounted Units", spawn_group);

  m_owner_box->addItem(QStringLiteral("Local Player"), k_arena_local_owner_id);
  m_owner_box->addItem(QStringLiteral("Arena Opponent"), k_arena_opponent_owner_id);
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

  auto* spawn_buttons = new QWidget(spawn_group);
  auto* spawn_buttons_layout = new QHBoxLayout(spawn_buttons);
  spawn_buttons_layout->setContentsMargins(0, 0, 0, 0);
  spawn_buttons_layout->setSpacing(6);
  auto* spawn_button = new QPushButton("Spawn", spawn_group);
  spawn_button->setProperty("primary", true);
  auto* clear_button = new QPushButton("Clear", spawn_group);
  auto* apply_visuals_button = new QPushButton("Apply Overrides", spawn_group);
  spawn_button->setToolTip("Spawn units with selected settings");
  clear_button->setToolTip("Remove all units from the arena");
  apply_visuals_button->setToolTip("Apply visual overrides to selected units");
  spawn_buttons_layout->addWidget(spawn_button, 1);
  spawn_buttons_layout->addWidget(clear_button, 1);
  spawn_buttons_layout->addWidget(apply_visuals_button, 1);
  spawn_group_layout->addWidget(spawn_buttons);

  layout->addWidget(spawn_group);

  auto* anim_group = new QGroupBox("Animation", this);
  auto* anim_group_layout = new QVBoxLayout(anim_group);
  anim_group_layout->setSpacing(6);

  auto* anim_form = new QFormLayout();
  anim_form->setSpacing(4);
  auto* animation_box = new QComboBox(anim_group);
  animation_box->addItems({QStringLiteral("Idle"),
                           QStringLiteral("Walk"),
                           QStringLiteral("Attack"),
                           QStringLiteral("Death")});

  auto* speed_container = new QWidget(anim_group);
  auto* speed_layout = new QHBoxLayout(speed_container);
  speed_layout->setContentsMargins(0, 0, 0, 0);
  auto* speed_slider = new QSlider(Qt::Horizontal, speed_container);
  auto* speed_spin = new QDoubleSpinBox(speed_container);
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

  auto* anim_buttons = new QWidget(anim_group);
  auto* anim_buttons_layout = new QHBoxLayout(anim_buttons);
  anim_buttons_layout->setContentsMargins(0, 0, 0, 0);
  anim_buttons_layout->setSpacing(6);
  auto* play_button = new QPushButton("Play", anim_group);
  auto* move_button = new QPushButton("Move", anim_group);
  anim_buttons_layout->addWidget(play_button, 1);
  anim_buttons_layout->addWidget(move_button, 1);
  anim_group_layout->addWidget(anim_buttons);

  m_pause_checkbox = new QCheckBox("Pause Simulation", anim_group);
  auto* skeleton_debug_box = new QCheckBox("Skeleton / Pose Overlay", anim_group);
  m_combat_debug_checkbox = new QCheckBox("Combat Animation Debug Overlay", anim_group);
  m_attack_scrub_checkbox = new QCheckBox("Freeze Selected Attack Phase", anim_group);
  auto* scrub_container = new QWidget(anim_group);
  auto* scrub_layout = new QHBoxLayout(scrub_container);
  scrub_layout->setContentsMargins(0, 0, 0, 0);
  scrub_layout->setSpacing(6);
  m_attack_scrub_slider = new QSlider(Qt::Horizontal, scrub_container);
  m_attack_scrub_spin = new QDoubleSpinBox(scrub_container);
  m_attack_scrub_slider->setRange(0, 100);
  m_attack_scrub_slider->setValue(50);
  m_attack_scrub_spin->setRange(0.0, 1.0);
  m_attack_scrub_spin->setDecimals(2);
  m_attack_scrub_spin->setSingleStep(0.01);
  m_attack_scrub_spin->setValue(0.5);
  scrub_layout->addWidget(m_attack_scrub_slider, 1);
  scrub_layout->addWidget(m_attack_scrub_spin);
  anim_group_layout->addWidget(m_pause_checkbox);
  anim_group_layout->addWidget(skeleton_debug_box);
  anim_group_layout->addWidget(m_combat_debug_checkbox);
  anim_group_layout->addWidget(m_attack_scrub_checkbox);
  anim_form->addRow("Attack Phase", scrub_container);

  layout->addWidget(anim_group);

  auto* quick_setup_group = new QGroupBox("Quick Setup", this);
  auto* quick_setup_layout = new QVBoxLayout(quick_setup_group);
  quick_setup_layout->setSpacing(4);
  auto* scenario_form = new QFormLayout();
  scenario_form->setSpacing(4);
  m_scenario_box = new QComboBox(quick_setup_group);
  for (const Arena::Scenarios::ScenarioOption& option : Arena::Scenarios::options()) {
    m_scenario_box->addItem(option.label, option.id);
    m_scenario_box->setItemData(
        m_scenario_box->count() - 1, option.description, Qt::ToolTipRole);
  }
  auto* load_scenario_button = new QPushButton("Load Scenario", quick_setup_group);
  load_scenario_button->setToolTip(
      "Reset the arena and load a Phase 1 combat animation scenario.");
  scenario_form->addRow("Scenario", m_scenario_box);
  scenario_form->addRow("", load_scenario_button);
  quick_setup_layout->addLayout(scenario_form);
  m_scenario_description_label = new QLabel(quick_setup_group);
  m_scenario_description_label->setWordWrap(true);
  m_scenario_description_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  quick_setup_layout->addWidget(m_scenario_description_label);
  auto* spawn_opposing_button =
      new QPushButton("Spawn Opposing Batch", quick_setup_group);
  auto* spawn_mirror_button = new QPushButton("Spawn Mirror Match", quick_setup_group);
  auto* reset_arena_button = new QPushButton("Reset Arena", quick_setup_group);
  quick_setup_layout->addWidget(spawn_opposing_button);
  quick_setup_layout->addWidget(spawn_mirror_button);
  quick_setup_layout->addWidget(reset_arena_button);
  layout->addWidget(quick_setup_group);

  auto* selection_group = new QGroupBox("Selection", this);
  auto* selection_layout = new QVBoxLayout(selection_group);
  m_selection_summary_label =
      new QLabel(QStringLiteral("No units selected."), selection_group);
  m_selection_summary_label->setTextFormat(Qt::PlainText);
  m_selection_summary_label->setWordWrap(true);
  m_selection_summary_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  selection_layout->addWidget(m_selection_summary_label);
  layout->addWidget(selection_group);

  layout->addStretch(1);

  connect(spawn_button, &QPushButton::clicked, this, [this]() {
    emit spawn_individuals_per_unit_changed(m_individuals_per_unit_box != nullptr
                                                ? m_individuals_per_unit_box->value()
                                                : 0);
    emit spawn_rider_visibility_changed(m_render_rider_checkbox == nullptr ||
                                        m_render_rider_checkbox->isChecked());
    emit spawn_units_requested(m_spawn_count_box != nullptr ? m_spawn_count_box->value()
                                                            : 1);
  });
  connect(clear_button, &QPushButton::clicked, this, &UnitPanel::clear_units_requested);
  connect(m_owner_box,
          qOverload<int>(&QComboBox::currentIndexChanged),
          this,
          [this](int) { emit spawn_owner_selected(selected_owner_id()); });
  connect(
      m_nation_box, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_nation_box == nullptr) {
          return;
        }
        const QString nation_id = selected_nation_id();
        populate_unit_options(nation_id);
        emit nation_selected(nation_id);
      });
  connect(
      m_unit_box, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        apply_special_unit_defaults(selected_unit_type_id());
        emit unit_type_selected(selected_unit_type_id());
      });
  connect(m_individuals_per_unit_box,
          qOverload<int>(&QSpinBox::valueChanged),
          this,
          [this](int value) {
            if (!m_special_unit_option_active) {
              m_saved_manual_individuals_per_unit = value;
            }
            emit spawn_individuals_per_unit_changed(value);
          });
  connect(m_render_rider_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
    if (!m_special_unit_option_active) {
      m_saved_manual_rider_visible = checked;
    }
    emit spawn_rider_visibility_changed(checked);
  });
  connect(animation_box,
          &QComboBox::currentTextChanged,
          this,
          &UnitPanel::animation_selected);
  connect(
      play_button, &QPushButton::clicked, this, &UnitPanel::play_animation_requested);
  connect(apply_visuals_button, &QPushButton::clicked, this, [this]() {
    emit spawn_individuals_per_unit_changed(m_individuals_per_unit_box != nullptr
                                                ? m_individuals_per_unit_box->value()
                                                : 0);
    emit spawn_rider_visibility_changed(m_render_rider_checkbox == nullptr ||
                                        m_render_rider_checkbox->isChecked());
    emit apply_visual_overrides_requested();
  });
  connect(spawn_opposing_button, &QPushButton::clicked, this, [this]() {
    emit spawn_opposing_batch_requested(
        m_spawn_count_box != nullptr ? m_spawn_count_box->value() : 1);
  });
  connect(spawn_mirror_button, &QPushButton::clicked, this, [this]() {
    emit spawn_mirror_match_requested(
        m_spawn_count_box != nullptr ? m_spawn_count_box->value() : 1);
  });
  connect(reset_arena_button,
          &QPushButton::clicked,
          this,
          &UnitPanel::reset_arena_requested);
  connect(load_scenario_button, &QPushButton::clicked, this, [this]() {
    if (m_scenario_box == nullptr) {
      return;
    }
    emit load_scenario_requested(m_scenario_box->currentData().toString());
  });
  connect(m_scenario_box,
          qOverload<int>(&QComboBox::currentIndexChanged),
          this,
          [this](int index) {
            if (m_scenario_box == nullptr || m_scenario_description_label == nullptr ||
                index < 0) {
              return;
            }
            m_scenario_description_label->setText(
                m_scenario_box->itemData(index, Qt::ToolTipRole).toString());
          });
  connect(m_pause_checkbox,
          &QCheckBox::toggled,
          this,
          &UnitPanel::animation_paused_toggled);
  connect(move_button,
          &QPushButton::clicked,
          this,
          &UnitPanel::move_selected_unit_requested);
  connect(skeleton_debug_box,
          &QCheckBox::toggled,
          this,
          &UnitPanel::skeleton_debug_toggled);
  connect(m_combat_debug_checkbox,
          &QCheckBox::toggled,
          this,
          &UnitPanel::combat_debug_toggled);
  connect(m_attack_scrub_checkbox,
          &QCheckBox::toggled,
          this,
          &UnitPanel::attack_scrub_toggled);
  connect(m_attack_scrub_slider, &QSlider::valueChanged, this, [this](int value) {
    if (m_attack_scrub_spin != nullptr) {
      const QSignalBlocker blocker(m_attack_scrub_spin);
      m_attack_scrub_spin->setValue(static_cast<double>(value) / 100.0);
    }
    emit attack_scrub_phase_changed(static_cast<float>(value) / 100.0F);
  });
  connect(m_attack_scrub_spin,
          qOverload<double>(&QDoubleSpinBox::valueChanged),
          this,
          [this](double value) {
            if (m_attack_scrub_slider != nullptr) {
              const QSignalBlocker blocker(m_attack_scrub_slider);
              m_attack_scrub_slider->setValue(
                  static_cast<int>(std::round(value * 100.0)));
            }
            emit attack_scrub_phase_changed(static_cast<float>(value));
          });

  connect(speed_slider, &QSlider::valueChanged, speed_spin, [speed_spin](int value) {
    const QSignalBlocker blocker(speed_spin);
    speed_spin->setValue(static_cast<double>(value) / 10.0);
  });
  connect(speed_spin,
          qOverload<double>(&QDoubleSpinBox::valueChanged),
          speed_slider,
          [speed_slider](double value) {
            const QSignalBlocker blocker(speed_slider);
            speed_slider->setValue(static_cast<int>(value * 10.0));
          });
  connect(
      speed_spin,
      qOverload<double>(&QDoubleSpinBox::valueChanged),
      this,
      [this](double value) { emit movement_speed_changed(static_cast<float>(value)); });

  populate_nation_options();
  populate_unit_options(selected_nation_id());
  if (m_scenario_box != nullptr && m_scenario_description_label != nullptr &&
      m_scenario_box->count() > 0) {
    m_scenario_description_label->setText(
        m_scenario_box->itemData(m_scenario_box->currentIndex(), Qt::ToolTipRole)
            .toString());
  }
}

void UnitPanel::apply_special_unit_defaults(const QString& unit_id) {
  auto const special = Arena::UnitSpawnOptions::parse_special_unit_option(unit_id);
  if (!special.has_value()) {
    if (!m_special_unit_option_active) {
      return;
    }

    m_special_unit_option_active = false;
    if (m_individuals_per_unit_box != nullptr) {
      const QSignalBlocker blocker(m_individuals_per_unit_box);
      m_individuals_per_unit_box->setValue(m_saved_manual_individuals_per_unit);
    }
    if (m_render_rider_checkbox != nullptr) {
      const QSignalBlocker blocker(m_render_rider_checkbox);
      m_render_rider_checkbox->setChecked(m_saved_manual_rider_visible);
    }
    emit spawn_individuals_per_unit_changed(m_saved_manual_individuals_per_unit);
    emit spawn_rider_visibility_changed(m_saved_manual_rider_visible);
    return;
  }

  if (!m_special_unit_option_active) {
    m_saved_manual_individuals_per_unit =
        m_individuals_per_unit_box != nullptr ? m_individuals_per_unit_box->value() : 0;
    m_saved_manual_rider_visible =
        m_render_rider_checkbox == nullptr || m_render_rider_checkbox->isChecked();
  }
  m_special_unit_option_active = true;

  if (m_individuals_per_unit_box != nullptr) {
    const QSignalBlocker blocker(m_individuals_per_unit_box);
    m_individuals_per_unit_box->setValue(special->individuals_per_unit);
  }
  if (m_render_rider_checkbox != nullptr) {
    const QSignalBlocker blocker(m_render_rider_checkbox);
    m_render_rider_checkbox->setChecked(special->render_rider);
  }

  emit spawn_individuals_per_unit_changed(special->individuals_per_unit);
  emit spawn_rider_visibility_changed(special->render_rider);
}

void UnitPanel::set_animation_paused(bool paused) {
  if (m_pause_checkbox == nullptr || m_pause_checkbox->isChecked() == paused) {
    return;
  }
  const QSignalBlocker blocker(m_pause_checkbox);
  m_pause_checkbox->setChecked(paused);
}

void UnitPanel::set_selection_summary(const QString& summary) {
  if (m_selection_summary_label == nullptr) {
    return;
  }
  m_selection_summary_label->setText(
      summary.trimmed().isEmpty() ? QStringLiteral("No units selected.") : summary);
}

auto UnitPanel::selected_owner_id() const -> int {
  return m_owner_box != nullptr ? m_owner_box->currentData().toInt()
                                : k_arena_local_owner_id;
}

auto UnitPanel::selected_nation_id() const -> QString {
  return m_nation_box != nullptr ? m_nation_box->currentData().toString() : QString();
}

auto UnitPanel::selected_unit_type_id() const -> QString {
  return m_unit_box != nullptr ? m_unit_box->currentData().toString() : QString();
}

void UnitPanel::populate_nation_options() {
  if (m_nation_box == nullptr) {
    return;
  }

  QString const preferred = selected_nation_id();
  const auto& registry = Game::Systems::NationRegistry::instance();
  const auto& nations = registry.get_all_nations();

  m_nation_box->clear();
  for (const auto& nation : nations) {
    if (nation.available_troops.empty()) {
      continue;
    }
    QString const nation_id = Game::Systems::nation_id_to_qstring(nation.id);
    QString label = QString::fromStdString(nation.display_name);
    if (label.trimmed().isEmpty()) {
      label = prettify_identifier(nation_id);
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

void UnitPanel::populate_unit_options(const QString& nation_id,
                                      const QString& preferred_unit_type) {
  if (m_unit_box == nullptr) {
    return;
  }

  Game::Systems::NationID parsed_nation_id{};
  if (!Game::Systems::try_parse_nation_id(nation_id, parsed_nation_id)) {
    m_unit_box->clear();
    return;
  }

  const auto* nation =
      Game::Systems::NationRegistry::instance().get_nation(parsed_nation_id);
  if (nation == nullptr) {
    m_unit_box->clear();
    return;
  }

  QString const preferred =
      preferred_unit_type.isEmpty() ? selected_unit_type_id() : preferred_unit_type;

  m_unit_box->clear();
  std::optional<Game::Units::TroopType> cavalry_option;
  bool has_elephant_option = false;
  for (const auto& troop : nation->available_troops) {
    QString const troop_id = Game::Units::troop_typeToQString(troop.unit_type);
    QString label = QString::fromStdString(troop.display_name);
    if (label.trimmed().isEmpty()) {
      label = prettify_identifier(troop_id);
    }
    m_unit_box->addItem(label, troop_id);

    if (!cavalry_option.has_value() &&
        (troop.unit_type == Game::Units::TroopType::MountedKnight ||
         troop.unit_type == Game::Units::TroopType::HorseArcher ||
         troop.unit_type == Game::Units::TroopType::HorseSpearman)) {
      cavalry_option = troop.unit_type;
    }
    if (troop.unit_type == Game::Units::TroopType::Elephant) {
      has_elephant_option = true;
    }
  }

  if (cavalry_option.has_value() || has_elephant_option) {
    m_unit_box->insertSeparator(m_unit_box->count());
  }
  if (cavalry_option.has_value()) {
    auto const option = Arena::UnitSpawnOptions::make_special_unit_option(
        Arena::UnitSpawnOptions::Kind::SingleHorse, *cavalry_option);
    m_unit_box->addItem(option.label, option.id);
  }
  if (has_elephant_option) {
    auto const option = Arena::UnitSpawnOptions::make_special_unit_option(
        Arena::UnitSpawnOptions::Kind::SingleElephant,
        Game::Units::TroopType::Elephant);
    m_unit_box->addItem(option.label, option.id);
  }

  if (m_unit_box->count() == 0) {
    return;
  }

  int const index = preferred.isEmpty() ? -1 : m_unit_box->findData(preferred);
  m_unit_box->setCurrentIndex(index >= 0 ? index : 0);
}
