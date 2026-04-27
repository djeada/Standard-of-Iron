#include "unit_panel.h"

#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/units/troop_type.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStringList>
#include <QVBoxLayout>

namespace {

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

UnitPanel::UnitPanel(QWidget *parent) : QGroupBox("Units", parent) {
  auto *layout = new QVBoxLayout(this);

  auto *spawn_button = new QPushButton("Spawn Unit", this);
  auto *clear_button = new QPushButton("Clear Units", this);
  layout->addWidget(spawn_button);
  layout->addWidget(clear_button);

  auto *form = new QFormLayout();
  m_nation_box = new QComboBox(this);
  m_unit_box = new QComboBox(this);
  auto *animation_box = new QComboBox(this);
  animation_box->addItems(
      {QStringLiteral("Idle"), QStringLiteral("Walk"),
       QStringLiteral("Attack"), QStringLiteral("Death")});
  form->addRow("Nation", m_nation_box);
  form->addRow("Unit", m_unit_box);
  form->addRow("Animation", animation_box);

  auto *play_button = new QPushButton("Play Animation", this);
  form->addRow("", play_button);

  m_pause_checkbox = new QCheckBox("Pause Animation", this);
  form->addRow("", m_pause_checkbox);

  auto *move_button = new QPushButton("Move Selected Unit", this);
  form->addRow("", move_button);

  auto *speed_container = new QWidget(this);
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
  form->addRow("Speed", speed_container);

  auto *skeleton_debug_box = new QCheckBox("Skeleton/Pose Overlay", this);
  form->addRow("", skeleton_debug_box);
  layout->addLayout(form);
  layout->addStretch(1);

  connect(spawn_button, &QPushButton::clicked, this,
          &UnitPanel::spawnUnitRequested);
  connect(clear_button, &QPushButton::clicked, this,
          &UnitPanel::clearUnitsRequested);
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
          [this](int) {
    emit unitTypeSelected(selectedUnitTypeId());
  });
  connect(animation_box, &QComboBox::currentTextChanged, this,
          &UnitPanel::animationSelected);
  connect(play_button, &QPushButton::clicked, this,
          &UnitPanel::playAnimationRequested);
  connect(m_pause_checkbox, &QCheckBox::toggled, this,
          &UnitPanel::animationPausedToggled);
  connect(move_button, &QPushButton::clicked, this,
          &UnitPanel::moveSelectedUnitRequested);
  connect(skeleton_debug_box, &QCheckBox::toggled, this,
          &UnitPanel::skeletonDebugToggled);

  connect(speed_slider, &QSlider::valueChanged, speed_spin, [speed_spin](int value) {
    const QSignalBlocker blocker(speed_spin);
    speed_spin->setValue(static_cast<double>(value) / 10.0);
  });
  connect(speed_spin, qOverload<double>(&QDoubleSpinBox::valueChanged), speed_slider,
          [speed_slider](double value) {
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

auto UnitPanel::selectedNationId() const -> QString {
  return m_nation_box != nullptr ? m_nation_box->currentData().toString()
                                 : QString();
}

auto UnitPanel::selectedUnitTypeId() const -> QString {
  return m_unit_box != nullptr ? m_unit_box->currentData().toString() : QString();
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

  QString preferred = preferredUnitType.isEmpty() ? selectedUnitTypeId()
                                                  : preferredUnitType;

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
