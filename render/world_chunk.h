#pragma once

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <algorithm>
#include <cstdint>

namespace Render::GL {

class Mesh;
struct Material;

struct BoundingBox {
  QVector3D min{0.0F, 0.0F, 0.0F};
  QVector3D max{0.0F, 0.0F, 0.0F};

  [[nodiscard]] auto center() const -> QVector3D { return (min + max) * 0.5F; }
  [[nodiscard]] auto extents() const -> QVector3D { return (max - min) * 0.5F; }

  void expand(const QVector3D &p) {
    min.setX(std::min(min.x(), p.x()));
    min.setY(std::min(min.y(), p.y()));
    min.setZ(std::min(min.z(), p.z()));
    max.setX(std::max(max.x(), p.x()));
    max.setY(std::max(max.y(), p.y()));
    max.setZ(std::max(max.z(), p.z()));
  }

  [[nodiscard]] auto is_empty() const -> bool {
    return min.x() > max.x() || min.y() > max.y() || min.z() > max.z();
  }
};

struct WorldChunk {
  Mesh *mesh = nullptr;
  const Material *material = nullptr;
  QMatrix4x4 transform;
  BoundingBox aabb;
};

struct TerrainChunkParams {
  static constexpr float k_default_grass_primary_r = 0.30F;
  static constexpr float k_default_grass_primary_g = 0.60F;
  static constexpr float k_default_grass_primary_b = 0.28F;
  static constexpr float k_default_tile_size = 1.0F;
  static constexpr float k_default_grass_secondary_r = 0.44F;
  static constexpr float k_default_grass_secondary_g = 0.70F;
  static constexpr float k_default_grass_secondary_b = 0.32F;
  static constexpr float k_default_macro_noise_scale = 0.035F;
  static constexpr float k_default_grass_dry_r = 0.72F;
  static constexpr float k_default_grass_dry_g = 0.66F;
  static constexpr float k_default_grass_dry_b = 0.48F;
  static constexpr float k_default_detail_noise_scale = 0.16F;
  static constexpr float k_default_soil_color_r = 0.30F;
  static constexpr float k_default_soil_color_g = 0.26F;
  static constexpr float k_default_soil_color_b = 0.20F;
  static constexpr float k_default_slope_rock_threshold = 0.45F;
  static constexpr float k_default_rock_low_r = 0.48F;
  static constexpr float k_default_rock_low_g = 0.46F;
  static constexpr float k_default_rock_low_b = 0.44F;
  static constexpr float k_default_slope_rock_sharpness = 3.0F;
  static constexpr float k_default_rock_high_r = 0.70F;
  static constexpr float k_default_rock_high_g = 0.71F;
  static constexpr float k_default_rock_high_b = 0.75F;
  static constexpr float k_default_soil_blend_height = 0.6F;
  static constexpr float k_default_tint_component = 1.0F;
  static constexpr float k_default_soil_blend_sharpness = 3.5F;
  static constexpr float k_default_height_noise_strength = 0.05F;
  static constexpr float k_default_height_noise_frequency = 0.1F;
  static constexpr float k_default_ambient_boost = 1.05F;
  static constexpr float k_default_rock_detail_strength = 0.35F;
  static constexpr float k_default_light_dir_x = 0.35F;
  static constexpr float k_default_light_dir_y = 0.8F;
  static constexpr float k_default_light_dir_z = 0.45F;
  static constexpr float k_default_micro_bump_amp = 0.07F;
  static constexpr float k_default_micro_bump_freq = 2.2F;
  static constexpr float k_default_micro_normal_weight = 0.65F;
  static constexpr float k_default_albedo_jitter = 0.05F;

  static constexpr float k_default_snow_coverage = 0.0F;
  static constexpr float k_default_moisture_level = 0.5F;
  static constexpr float k_default_crack_intensity = 0.0F;
  static constexpr float k_default_rock_exposure = 0.3F;
  static constexpr float k_default_grass_saturation = 1.0F;
  static constexpr float k_default_soil_roughness = 0.5F;
  static constexpr float k_default_curvature_response = 0.0F;
  static constexpr float k_default_ridge_response = 0.0F;
  static constexpr float k_default_gully_response = 0.0F;
  static constexpr float k_default_snow_color_r = 0.92F;
  static constexpr float k_default_snow_color_g = 0.94F;
  static constexpr float k_default_snow_color_b = 0.98F;
  static constexpr float k_default_soil_foot_height = 0.0F;
  static constexpr float k_default_screen_toe_mul = 0.0F;
  static constexpr float k_default_screen_toe_clamp = 0.0F;

  static auto default_grass_primary() -> QVector3D {
    return {k_default_grass_primary_r, k_default_grass_primary_g,
            k_default_grass_primary_b};
  }
  static auto default_grass_secondary() -> QVector3D {
    return {k_default_grass_secondary_r, k_default_grass_secondary_g,
            k_default_grass_secondary_b};
  }
  static auto default_grass_dry() -> QVector3D {
    return {k_default_grass_dry_r, k_default_grass_dry_g, k_default_grass_dry_b};
  }
  static auto default_soil_color() -> QVector3D {
    return {k_default_soil_color_r, k_default_soil_color_g, k_default_soil_color_b};
  }
  static auto default_rock_low() -> QVector3D {
    return {k_default_rock_low_r, k_default_rock_low_g, k_default_rock_low_b};
  }
  static auto default_rock_high() -> QVector3D {
    return {k_default_rock_high_r, k_default_rock_high_g, k_default_rock_high_b};
  }
  static auto default_tint() -> QVector3D {
    return {k_default_tint_component, k_default_tint_component,
            k_default_tint_component};
  }
  static auto default_light_direction() -> QVector3D {
    return {k_default_light_dir_x, k_default_light_dir_y, k_default_light_dir_z};
  }
  static auto default_snow_color() -> QVector3D {
    return {k_default_snow_color_r, k_default_snow_color_g, k_default_snow_color_b};
  }

  QVector3D grass_primary = default_grass_primary();
  float tile_size = k_default_tile_size;
  QVector3D grass_secondary = default_grass_secondary();
  float macro_noise_scale = k_default_macro_noise_scale;
  QVector3D grass_dry = default_grass_dry();
  float detail_noise_scale = k_default_detail_noise_scale;
  QVector3D soil_color = default_soil_color();
  float slope_rock_threshold = k_default_slope_rock_threshold;
  QVector3D rock_low = default_rock_low();
  float slope_rock_sharpness = k_default_slope_rock_sharpness;
  QVector3D rock_high = default_rock_high();
  float soil_blend_height = k_default_soil_blend_height;
  QVector3D tint = default_tint();
  float soil_blend_sharpness = k_default_soil_blend_sharpness;

  QVector2D noise_offset{0.0F, 0.0F};
  float height_noise_strength = k_default_height_noise_strength;
  float height_noise_frequency = k_default_height_noise_frequency;
  float ambient_boost = k_default_ambient_boost;
  float rock_detail_strength = k_default_rock_detail_strength;
  QVector3D light_direction = default_light_direction();

  float noise_angle = 0.0F;

  float micro_bump_amp = k_default_micro_bump_amp;
  float micro_bump_freq = k_default_micro_bump_freq;
  float micro_normal_weight = k_default_micro_normal_weight;

  float albedo_jitter = k_default_albedo_jitter;

  bool is_ground_plane = false;

  float snow_coverage = k_default_snow_coverage;
  float moisture_level = k_default_moisture_level;
  float crack_intensity = k_default_crack_intensity;
  float rock_exposure = k_default_rock_exposure;
  float grass_saturation = k_default_grass_saturation;
  float soil_roughness = k_default_soil_roughness;
  float curvature_response = k_default_curvature_response;
  float ridge_response = k_default_ridge_response;
  float gully_response = k_default_gully_response;
  QVector3D snow_color = default_snow_color();
  float soil_foot_height = k_default_soil_foot_height;
  float screen_toe_mul = k_default_screen_toe_mul;
  float screen_toe_clamp = k_default_screen_toe_clamp;
};

} // namespace Render::GL
