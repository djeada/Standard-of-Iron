#pragma once

#include "../gl/mesh.h"
#include <QVector3D>
#include <array>
#include <memory>

namespace Render::Ground {

struct LinearFeatureRibbonSegment {
  QVector3D start;
  QVector3D end;
  float width = 1.0F;
};

struct LinearFeatureRibbonSettings {
  float sample_step = 0.5F;
  int min_length_steps = 8;
  std::array<float, 3> edge_noise_frequencies{0.0F, 0.0F, 0.0F};
  std::array<float, 3> edge_noise_weights{1.0F, 0.0F, 0.0F};
  float width_variation_scale = 0.0F;
  float meander_frequency = 0.0F;
  float meander_length_scale = 0.1F;
  float meander_amplitude = 0.0F;
  float y_offset = 0.0F;
};

[[nodiscard]] auto build_linear_ribbon_mesh(
    const LinearFeatureRibbonSegment &segment, float tile_size,
    const LinearFeatureRibbonSettings &settings)
    -> std::unique_ptr<Render::GL::Mesh>;

} // namespace Render::Ground
