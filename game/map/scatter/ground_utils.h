#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "game/map/scatter/value_noise.h"
#include "game/map/terrain.h"

namespace Render::Ground {

using std::uint32_t;

namespace MathConstants {
inline constexpr float k_two_pi = 6.28318530717958647692F;
}

inline auto rand_01(uint32_t& state) -> float {
  state = state * HashConstants::k_linear_congruential_multiplier +
          HashConstants::k_linear_congruential_increment;
  return static_cast<float>((state >> BitShift::shift_8) & BitShift::mask_24_bit) /
         BitShift::mask_24_bit_float;
}

inline auto remap(float value, float min_out, float max_out) -> float {
  return min_out + (max_out - min_out) * value;
}

inline auto noise_hash(float x, float y) -> float {
  float const n = std::sin(x * HashConstants::k_noise_frequency_x +
                           y * HashConstants::k_noise_frequency_y) *
                  HashConstants::k_noise_amplitude;
  return n - std::floor(n);
}

struct CurvatureShadingResponse {
  float curvature_emphasis = 0.0F;
  float ridge_response = 0.0F;
  float gully_response = 0.0F;
};

inline auto smoothstep(float edge0, float edge1, float x) -> float {
  float const width = std::max(1e-6F, edge1 - edge0);
  float const t = std::clamp((x - edge0) / width, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

inline auto color_luminance(QVector3D const& color) -> float {
  return color.x() * 0.2126F + color.y() * 0.7152F + color.z() * 0.0722F;
}

inline auto clamp_color(QVector3D const& color) -> QVector3D {
  return {std::clamp(color.x(), 0.0F, 1.0F),
          std::clamp(color.y(), 0.0F, 1.0F),
          std::clamp(color.z(), 0.0F, 1.0F)};
}

struct GrassBladeToning {
  QVector3D anchor;
  float anchor_weight;
  float depth;
  float saturation;
};

inline auto grass_blade_toning(Game::Map::GroundType ground_type) -> GrassBladeToning {
  switch (ground_type) {
  case Game::Map::GroundType::SoilFertile:

    return {{0.15F, 0.29F, 0.11F}, 0.26F, 0.20F, 1.24F};
  case Game::Map::GroundType::ForestMud:

    return {{0.13F, 0.25F, 0.13F}, 0.29F, 0.22F, 1.20F};
  case Game::Map::GroundType::AlpineMix:

    return {{0.21F, 0.29F, 0.17F}, 0.22F, 0.15F, 1.10F};
  case Game::Map::GroundType::SoilRocky:

    return {{0.25F, 0.29F, 0.15F}, 0.22F, 0.16F, 1.08F};
  case Game::Map::GroundType::GrassDry:
  default:

    return {{0.18F, 0.34F, 0.12F}, 0.19F, 0.15F, 1.12F};
  }
}

inline auto contrast_grass_blade_color(QVector3D const& blade_color,
                                       QVector3D const& soil_color,
                                       Game::Map::GroundType ground_type,
                                       float dryness) -> QVector3D {
  QVector3D blade = clamp_color(blade_color);
  const auto toning = grass_blade_toning(ground_type);
  float const dry_factor = std::clamp(dryness, 0.0F, 1.0F);

  blade = blade * (1.0F - toning.anchor_weight) + toning.anchor * toning.anchor_weight;
  blade = blade * (1.0F - toning.depth * (0.85F + 0.30F * dry_factor));

  float const luma = color_luminance(blade);
  blade = QVector3D(luma + (blade.x() - luma) * toning.saturation,
                    luma + (blade.y() - luma) * toning.saturation,
                    luma + (blade.z() - luma) * toning.saturation);
  QVector3D adjusted = clamp_color(blade);

  if (ground_type != Game::Map::GroundType::GrassDry) {
    return adjusted;
  }

  float const soil_luma = color_luminance(soil_color);
  float const bright_soil = smoothstep(0.38F, 0.58F, soil_luma);
  float const adjusted_luma = color_luminance(adjusted);
  float const min_gap = 0.055F + bright_soil * 0.025F;
  if (soil_luma > 0.38F && soil_luma - adjusted_luma < min_gap) {
    QVector3D const contrast_anchor{0.20F, 0.27F, 0.11F};
    float const deficit = min_gap - (soil_luma - adjusted_luma);
    float const extra = std::clamp(deficit / std::max(min_gap, 1e-4F), 0.0F, 1.0F);
    adjusted = adjusted * (1.0F - extra * 0.85F) + contrast_anchor * (extra * 0.85F);
  }

  return clamp_color(adjusted);
}

inline auto
compute_curvature_shading_response(Game::Map::TerrainType type,
                                   float avg_curvature,
                                   float avg_slope,
                                   float edge_factor,
                                   float plateau_factor,
                                   float entrance_factor) -> CurvatureShadingResponse {
  if (type != Game::Map::TerrainType::Hill &&
      type != Game::Map::TerrainType::Mountain) {
    return {};
  }

  float const terrain_scale = (type == Game::Map::TerrainType::Mountain) ? 1.0F : 0.78F;
  float const curvature_signal = smoothstep(0.01F, 0.12F, avg_curvature);
  float const slope_signal = smoothstep(0.12F, 0.45F, avg_slope);
  float const exposed_signal = std::max(edge_factor, slope_signal);
  float const sheltered_signal = 0.60F * plateau_factor + 0.40F * entrance_factor;

  CurvatureShadingResponse response;
  response.curvature_emphasis =
      std::clamp(terrain_scale * curvature_signal * (0.55F + 0.45F * exposed_signal) *
                     (1.0F - 0.45F * sheltered_signal),
                 0.0F,
                 1.0F);
  response.ridge_response =
      std::clamp(response.curvature_emphasis * (0.45F + 0.55F * edge_factor) *
                     (1.0F - 0.35F * entrance_factor),
                 0.0F,
                 1.0F);
  response.gully_response = std::clamp(response.curvature_emphasis *
                                           (0.50F + 0.50F * (1.0F - plateau_factor)) *
                                           (0.85F + 0.15F * (1.0F - entrance_factor)),
                                       0.0F,
                                       1.0F);
  return response;
}

inline auto compute_entry_shading_factor(float avg_entry_weight,
                                         float avg_slope,
                                         float plateau_factor,
                                         float concavity_hint) -> float {
  float const coverage = smoothstep(0.02F, 0.22F, avg_entry_weight);
  float const sheltered = 1.0F - smoothstep(0.20F, 0.58F, avg_slope);
  float const plateau_support = 0.35F + 0.65F * std::max(plateau_factor, sheltered);
  float const carved_support = concavity_hint * (0.30F + 0.30F * sheltered);
  return std::clamp(coverage * plateau_support + carved_support, 0.0F, 1.0F);
}

} // namespace Render::Ground
