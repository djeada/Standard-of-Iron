#pragma once

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "game/map/scatter/value_noise.h"

namespace Game::Map {

struct RibbonShape {
  std::array<float, 3> edge_noise_frequencies{0.0F, 0.0F, 0.0F};
  std::array<float, 3> edge_noise_weights{1.0F, 0.0F, 0.0F};
  float width_scale = 1.0F;
  float width_variation_scale = 0.0F;
  float meander_frequency = 0.0F;
  float meander_length_scale = 0.1F;
  float meander_amplitude = 0.0F;
};

// The shape the river water is drawn with. Bridges and the water-blocked
// navigation band read the same shape, so a deck ends where the drawn water
// ends rather than where the widest possible river would.
inline constexpr RibbonShape k_river_ribbon_shape{
    .edge_noise_frequencies = {0.015F, 0.055F, 0.14F},
    .edge_noise_weights = {0.45F, 0.36F, 0.19F},
    .width_scale = 1.08F,
    .width_variation_scale = 0.15F,
    .meander_frequency = 3.6F,
    .meander_length_scale = 0.1F,
    .meander_amplitude = 0.145F,
};

struct RibbonCrossSection {
  QVector3D center;
  float half_width = 0.5F;
};

[[nodiscard]] inline auto
sample_ribbon_cross_section(const QVector3D& start,
                            const QVector3D& end,
                            float width,
                            float t,
                            const RibbonShape& shape) -> RibbonCrossSection {
  QVector3D direction = end - start;
  const float length = direction.length();
  if (length < 0.01F) {
    return {start, std::max(width * 0.5F, 0.01F)};
  }
  direction /= length;
  QVector3D const perpendicular(-direction.z(), 0.0F, direction.x());
  QVector3D center = start + direction * (length * std::clamp(t, 0.0F, 1.0F));

  float combined_noise = 0.0F;
  float weight_sum = 0.0F;
  for (std::size_t index = 0; index < shape.edge_noise_frequencies.size(); ++index) {
    const float frequency = shape.edge_noise_frequencies[index];
    const float weight = shape.edge_noise_weights[index];
    if (frequency <= 0.0F || weight <= 0.0F) {
      continue;
    }
    combined_noise +=
        Render::Ground::value_noise(center.x() * frequency, center.z() * frequency) *
        weight;
    weight_sum += weight;
  }
  if (weight_sum > 0.0F) {
    combined_noise = combined_noise / weight_sum * 2.0F - 1.0F;
  }

  if (shape.meander_amplitude > 0.0F && shape.meander_frequency > 0.0F) {
    const float meander =
        (Render::Ground::value_noise(t * shape.meander_frequency,
                                     length * shape.meander_length_scale) *
             2.0F -
         1.0F) *
        width * shape.meander_amplitude *
        std::sin(std::numbers::pi_v<float> * std::clamp(t, 0.0F, 1.0F));
    center += perpendicular * meander;
  }

  const float half_width = width * 0.5F * shape.width_scale;
  return {
      center,
      std::max(half_width + combined_noise * half_width * shape.width_variation_scale,
               0.01F)};
}

} // namespace Game::Map
