#include "terrain_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <functional>

namespace {

void bind_slider_to_double(QSlider *slider, QDoubleSpinBox *spin_box,
                           double scale,
                           const std::function<void(double)> &emit_value) {
  QObject::connect(slider, &QSlider::valueChanged, spin_box,
                   [spin_box, scale](int value) {
                     const QSignalBlocker blocker(spin_box);
                     spin_box->setValue(static_cast<double>(value) / scale);
                   });
  QObject::connect(slider, &QSlider::valueChanged, slider,
                   [scale, emit_value](int value) {
                     emit_value(static_cast<double>(value) / scale);
                   });
  QObject::connect(spin_box, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   slider, [slider, scale](double value) {
                     const QSignalBlocker blocker(slider);
                     slider->setValue(static_cast<int>(value * scale));
                   });
  QObject::connect(spin_box, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   spin_box, [emit_value](double value) { emit_value(value); });
}

} // namespace

TerrainPanel::TerrainPanel(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto *noise_group = new QGroupBox("Noise", this);
  auto *noise_layout = new QVBoxLayout(noise_group);
  noise_layout->setSpacing(6);
  auto *form = new QFormLayout();
  form->setSpacing(4);

  auto *seed_box = new QSpinBox(noise_group);
  seed_box->setRange(0, 999999);
  seed_box->setValue(1337);
  form->addRow("Seed", seed_box);

  auto *height_container = new QWidget(noise_group);
  auto *height_layout = new QHBoxLayout(height_container);
  height_layout->setContentsMargins(0, 0, 0, 0);
  auto *height_slider = new QSlider(Qt::Horizontal, height_container);
  auto *height_spin = new QDoubleSpinBox(height_container);
  height_slider->setRange(0, 400);
  height_slider->setValue(120);
  height_spin->setRange(0.0, 20.0);
  height_spin->setDecimals(2);
  height_spin->setSingleStep(0.1);
  height_spin->setValue(6.0);
  height_layout->addWidget(height_slider, 1);
  height_layout->addWidget(height_spin);
  form->addRow("Height Scale", height_container);

  auto *octaves_box = new QSpinBox(noise_group);
  octaves_box->setRange(1, 8);
  octaves_box->setValue(4);
  form->addRow("Octaves", octaves_box);

  auto *frequency_container = new QWidget(noise_group);
  auto *frequency_layout = new QHBoxLayout(frequency_container);
  frequency_layout->setContentsMargins(0, 0, 0, 0);
  auto *frequency_slider = new QSlider(Qt::Horizontal, frequency_container);
  auto *frequency_spin = new QDoubleSpinBox(frequency_container);
  frequency_slider->setRange(1, 200);
  frequency_slider->setValue(7);
  frequency_spin->setRange(0.01, 2.0);
  frequency_spin->setDecimals(3);
  frequency_spin->setSingleStep(0.01);
  frequency_spin->setValue(0.07);
  frequency_layout->addWidget(frequency_slider, 1);
  frequency_layout->addWidget(frequency_spin);
  form->addRow("Frequency", frequency_container);

  auto *ground_type_box = new QComboBox(noise_group);
  ground_type_box->addItem(QStringLiteral("Forest Mud"),
                           QStringLiteral("forest_mud"));
  ground_type_box->addItem(QStringLiteral("Grass Dry"),
                           QStringLiteral("grass_dry"));
  ground_type_box->addItem(QStringLiteral("Soil Rocky"),
                           QStringLiteral("soil_rocky"));
  ground_type_box->addItem(QStringLiteral("Alpine Mix"),
                           QStringLiteral("alpine_mix"));
  ground_type_box->addItem(QStringLiteral("Soil Fertile"),
                           QStringLiteral("soil_fertile"));
  form->addRow("Ground Type", ground_type_box);
  noise_layout->addLayout(form);

  auto *regenerate_button = new QPushButton("Regenerate", noise_group);
  regenerate_button->setProperty("primary", true);
  noise_layout->addWidget(regenerate_button);

  auto *debug_container = new QWidget(noise_group);
  auto *debug_layout = new QHBoxLayout(debug_container);
  debug_layout->setContentsMargins(0, 0, 0, 0);
  debug_layout->setSpacing(6);
  auto *wireframe_box = new QCheckBox("Wireframe", debug_container);
  auto *normals_box = new QCheckBox("Normals Overlay", debug_container);
  debug_layout->addWidget(wireframe_box);
  debug_layout->addWidget(normals_box);
  debug_layout->addStretch(1);
  noise_layout->addWidget(debug_container);

  layout->addWidget(noise_group);

  auto *rain_section = new QGroupBox("Weather", this);
  auto *rain_vlayout = new QVBoxLayout(rain_section);
  rain_vlayout->setSpacing(6);
  auto *rain_form = new QFormLayout();
  rain_form->setSpacing(4);
  auto *rain_box = new QCheckBox("Enable Rain", rain_section);
  auto *rain_intensity_container = new QWidget(rain_section);
  auto *rain_intensity_layout = new QHBoxLayout(rain_intensity_container);
  rain_intensity_layout->setContentsMargins(0, 0, 0, 0);
  auto *rain_intensity_slider =
      new QSlider(Qt::Horizontal, rain_intensity_container);
  auto *rain_intensity_spin = new QDoubleSpinBox(rain_intensity_container);
  rain_intensity_slider->setRange(0, 100);
  rain_intensity_slider->setValue(50);
  rain_intensity_spin->setRange(0.0, 1.0);
  rain_intensity_spin->setDecimals(2);
  rain_intensity_spin->setSingleStep(0.05);
  rain_intensity_spin->setValue(0.5);
  rain_intensity_layout->addWidget(rain_intensity_slider, 1);
  rain_intensity_layout->addWidget(rain_intensity_spin);
  rain_form->addRow("", rain_box);
  rain_form->addRow("Intensity", rain_intensity_container);
  rain_vlayout->addLayout(rain_form);
  layout->addWidget(rain_section);

  layout->addStretch(1);

  connect(seed_box, qOverload<int>(&QSpinBox::valueChanged), this,
          &TerrainPanel::seed_changed);
  connect(octaves_box, qOverload<int>(&QSpinBox::valueChanged), this,
          &TerrainPanel::octaves_changed);
  connect(regenerate_button, &QPushButton::clicked, this,
          &TerrainPanel::regenerate_requested);
  connect(wireframe_box, &QCheckBox::toggled, this,
          &TerrainPanel::wireframe_toggled);
  connect(normals_box, &QCheckBox::toggled, this,
          &TerrainPanel::normals_toggled);
  connect(ground_type_box, &QComboBox::currentIndexChanged, this,
          [this, ground_type_box](int) {
            emit ground_type_changed(ground_type_box->currentData().toString());
          });
  connect(rain_box, &QCheckBox::toggled, this, &TerrainPanel::rain_toggled);

  bind_slider_to_double(height_slider, height_spin, 20.0, [this](double value) {
    emit height_scale_changed(static_cast<float>(value));
  });
  bind_slider_to_double(frequency_slider, frequency_spin, 100.0,
                        [this](double value) {
                          emit frequency_changed(static_cast<float>(value));
                        });
  bind_slider_to_double(rain_intensity_slider, rain_intensity_spin, 100.0,
                        [this](double value) {
                          emit rain_intensity_changed(static_cast<float>(value));
                        });
}
