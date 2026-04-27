#include "terrain_panel.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QCheckBox>
#include <functional>

namespace {

void bind_slider_to_double(QSlider *slider, QDoubleSpinBox *spin_box, double scale,
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

  layout->addLayout(form);

  auto *regenerate_button = new QPushButton("Regenerate", this);
  auto *wireframe_box = new QCheckBox("Wireframe", this);
  auto *normals_box = new QCheckBox("Normals Overlay", this);

  layout->addWidget(regenerate_button);
  layout->addWidget(wireframe_box);
  layout->addWidget(normals_box);
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

  bind_slider_to_double(height_slider, height_spin, 20.0,
                        [this](double value) {
                          emit heightScaleChanged(static_cast<float>(value));
                        });
  bind_slider_to_double(frequency_slider, frequency_spin, 100.0,
                        [this](double value) {
                          emit frequencyChanged(static_cast<float>(value));
                        });
}
