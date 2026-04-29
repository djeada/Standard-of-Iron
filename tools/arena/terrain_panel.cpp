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

TerrainPanel::TerrainPanel(QWidget *parent) : QGroupBox("Terrain", parent) {
  auto *layout = new QVBoxLayout(this);
  auto *form = new QFormLayout();

  auto *seed_box = new QSpinBox(this);
  seed_box->setRange(0, 999999);
  seed_box->setValue(1337);
  form->addRow("Seed", seed_box);

  auto *height_container = new QWidget(this);
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

  auto *octaves_box = new QSpinBox(this);
  octaves_box->setRange(1, 8);
  octaves_box->setValue(4);
  form->addRow("Octaves", octaves_box);

  auto *frequency_container = new QWidget(this);
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

  auto *ground_type_box = new QComboBox(this);
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

  layout->addLayout(form);

  auto *regenerate_button = new QPushButton("Regenerate", this);
  auto *wireframe_box = new QCheckBox("Wireframe", this);
  auto *normals_box = new QCheckBox("Normals Overlay", this);

  layout->addWidget(regenerate_button);
  layout->addWidget(wireframe_box);
  layout->addWidget(normals_box);

  auto *rain_section = new QGroupBox("Weather", this);
  auto *rain_vlayout = new QVBoxLayout(rain_section);
  auto *rain_form = new QFormLayout();
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
          &TerrainPanel::seedChanged);
  connect(octaves_box, qOverload<int>(&QSpinBox::valueChanged), this,
          &TerrainPanel::octavesChanged);
  connect(regenerate_button, &QPushButton::clicked, this,
          &TerrainPanel::regenerateRequested);
  connect(wireframe_box, &QCheckBox::toggled, this,
          &TerrainPanel::wireframeToggled);
  connect(normals_box, &QCheckBox::toggled, this,
          &TerrainPanel::normalsToggled);
  connect(ground_type_box, &QComboBox::currentIndexChanged, this,
          [this, ground_type_box](int) {
            emit groundTypeChanged(ground_type_box->currentData().toString());
          });
  connect(rain_box, &QCheckBox::toggled, this, &TerrainPanel::rainToggled);

  bind_slider_to_double(height_slider, height_spin, 20.0, [this](double value) {
    emit heightScaleChanged(static_cast<float>(value));
  });
  bind_slider_to_double(frequency_slider, frequency_spin, 100.0,
                        [this](double value) {
                          emit frequencyChanged(static_cast<float>(value));
                        });
  bind_slider_to_double(rain_intensity_slider, rain_intensity_spin, 100.0,
                        [this](double value) {
                          emit rainIntensityChanged(static_cast<float>(value));
                        });
}
