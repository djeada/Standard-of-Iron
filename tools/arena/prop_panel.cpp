#include "prop_panel.h"

#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

auto add_prop_button(QButtonGroup* group,
                     QGridLayout* layout,
                     int row,
                     int column,
                     const QString& label,
                     const QString& prop_type) -> QToolButton* {
  auto* button = new QToolButton(layout->parentWidget());
  button->setText(label);
  button->setCheckable(true);
  button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  button->setMinimumHeight(42);
  button->setProperty("propType", prop_type);
  layout->addWidget(button, row, column);
  group->addButton(button);
  return button;
}

} // namespace

PropPanel::PropPanel(QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto* placement_group = new QGroupBox("Authored Props", this);
  auto* placement_layout = new QVBoxLayout(placement_group);
  auto* button_grid = new QGridLayout();
  button_grid->setHorizontalSpacing(6);
  button_grid->setVerticalSpacing(6);

  m_prop_group = new QButtonGroup(this);
  m_prop_group->setExclusive(true);
  add_prop_button(m_prop_group, button_grid, 0, 0, "Fire Camp", "firecamp")
      ->setChecked(true);
  add_prop_button(m_prop_group, button_grid, 0, 1, "Tent", "tent");
  add_prop_button(m_prop_group, button_grid, 1, 0, "Supply Cart", "supply_cart");
  add_prop_button(m_prop_group, button_grid, 1, 1, "Weapon Rack", "weapon_rack");
  add_prop_button(m_prop_group, button_grid, 2, 0, "Ruins", "ruins");
  add_prop_button(m_prop_group, button_grid, 2, 1, "Dead Tree", "dead_tree");
  add_prop_button(m_prop_group, button_grid, 3, 0, "Boulder", "boulder");
  add_prop_button(m_prop_group, button_grid, 3, 1, "Pine Tree", "pine_tree");
  add_prop_button(m_prop_group, button_grid, 4, 0, "Olive Tree", "olive_tree");
  add_prop_button(m_prop_group, button_grid, 4, 1, "Plant", "plant");
  placement_layout->addLayout(button_grid);

  auto* hint = new QLabel("Click the arena to set an anchor, then place the "
                          "selected authored prop.",
                          placement_group);
  hint->setWordWrap(true);
  hint->setTextFormat(Qt::PlainText);
  placement_layout->addWidget(hint);
  layout->addWidget(placement_group);

  auto* settings_group = new QGroupBox("Placement Settings", this);
  auto* settings_layout = new QFormLayout(settings_group);
  settings_layout->setSpacing(6);

  m_scale_box = new QDoubleSpinBox(settings_group);
  m_scale_box->setRange(0.1, 8.0);
  m_scale_box->setSingleStep(0.1);
  m_scale_box->setValue(1.0);

  m_rotation_box = new QDoubleSpinBox(settings_group);
  m_rotation_box->setRange(-180.0, 180.0);
  m_rotation_box->setSingleStep(5.0);
  m_rotation_box->setDecimals(1);
  m_rotation_box->setSuffix(" deg");
  m_rotation_box->setValue(0.0);

  m_fire_camp_intensity_box = new QDoubleSpinBox(settings_group);
  m_fire_camp_intensity_box->setRange(0.1, 5.0);
  m_fire_camp_intensity_box->setSingleStep(0.1);
  m_fire_camp_intensity_box->setValue(1.0);

  m_fire_camp_radius_box = new QDoubleSpinBox(settings_group);
  m_fire_camp_radius_box->setRange(0.5, 12.0);
  m_fire_camp_radius_box->setSingleStep(0.5);
  m_fire_camp_radius_box->setValue(3.0);

  settings_layout->addRow("Scale", m_scale_box);
  settings_layout->addRow("Rotation", m_rotation_box);
  settings_layout->addRow("Fire Intensity", m_fire_camp_intensity_box);
  settings_layout->addRow("Fire Radius", m_fire_camp_radius_box);
  layout->addWidget(settings_group);

  auto* actions_group = new QGroupBox("Actions", this);
  auto* actions_layout = new QVBoxLayout(actions_group);
  auto* primary_row = new QWidget(actions_group);
  auto* primary_row_layout = new QHBoxLayout(primary_row);
  primary_row_layout->setContentsMargins(0, 0, 0, 0);
  primary_row_layout->setSpacing(6);
  auto* place_button = new QPushButton("Place Prop", actions_group);
  place_button->setProperty("primary", true);
  auto* clear_selected_button = new QPushButton("Clear Selected Type", actions_group);
  primary_row_layout->addWidget(place_button, 1);
  primary_row_layout->addWidget(clear_selected_button, 1);
  actions_layout->addWidget(primary_row);

  auto* clear_all_button = new QPushButton("Clear All Props", actions_group);
  actions_layout->addWidget(clear_all_button);
  layout->addWidget(actions_group);
  layout->addStretch(1);

  connect(m_prop_group, &QButtonGroup::buttonClicked, this, [this]() {
    update_control_visibility();
    emit world_prop_type_selected(selected_prop_type_id());
  });
  connect(m_scale_box,
          qOverload<double>(&QDoubleSpinBox::valueChanged),
          this,
          [this](double value) {
            emit world_prop_scale_changed(static_cast<float>(value));
          });
  connect(m_rotation_box,
          qOverload<double>(&QDoubleSpinBox::valueChanged),
          this,
          [this](double value) {
            emit world_prop_rotation_degrees_changed(static_cast<float>(value));
          });
  connect(m_fire_camp_intensity_box,
          qOverload<double>(&QDoubleSpinBox::valueChanged),
          this,
          [this](double value) {
            emit fire_camp_intensity_changed(static_cast<float>(value));
          });
  connect(m_fire_camp_radius_box,
          qOverload<double>(&QDoubleSpinBox::valueChanged),
          this,
          [this](double value) {
            emit fire_camp_radius_changed(static_cast<float>(value));
          });
  connect(place_button,
          &QPushButton::clicked,
          this,
          &PropPanel::spawn_world_prop_requested);
  connect(clear_all_button,
          &QPushButton::clicked,
          this,
          &PropPanel::clear_world_props_requested);
  connect(clear_selected_button,
          &QPushButton::clicked,
          this,
          &PropPanel::clear_world_props_of_type_requested);

  update_control_visibility();
}

auto PropPanel::selected_prop_type_id() const -> QString {
  auto* button = m_prop_group != nullptr ? m_prop_group->checkedButton() : nullptr;
  return button != nullptr ? button->property("propType").toString()
                           : QStringLiteral("firecamp");
}

auto PropPanel::selected_scale() const -> float {
  return m_scale_box != nullptr ? static_cast<float>(m_scale_box->value()) : 1.0F;
}

auto PropPanel::selected_rotation_degrees() const -> float {
  return m_rotation_box != nullptr ? static_cast<float>(m_rotation_box->value()) : 0.0F;
}

auto PropPanel::selected_fire_camp_intensity() const -> float {
  return m_fire_camp_intensity_box != nullptr
             ? static_cast<float>(m_fire_camp_intensity_box->value())
             : 1.0F;
}

auto PropPanel::selected_fire_camp_radius() const -> float {
  return m_fire_camp_radius_box != nullptr
             ? static_cast<float>(m_fire_camp_radius_box->value())
             : 3.0F;
}

void PropPanel::update_control_visibility() {
  bool const is_fire_camp = selected_prop_type_id() == QStringLiteral("firecamp");
  if (m_scale_box != nullptr) {
    m_scale_box->setEnabled(!is_fire_camp);
  }
  if (m_rotation_box != nullptr) {
    m_rotation_box->setEnabled(!is_fire_camp);
  }
  if (m_fire_camp_intensity_box != nullptr) {
    m_fire_camp_intensity_box->setEnabled(is_fire_camp);
  }
  if (m_fire_camp_radius_box != nullptr) {
    m_fire_camp_radius_box->setEnabled(is_fire_camp);
  }
}
