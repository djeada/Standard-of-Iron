#pragma once

#include <QVector3D>
#include <QVector4D>
#include <cstdint>

namespace Render::GL {

struct GrassInstanceGpu {
  QVector4D pos_height{0.0F, 0.0F, 0.0F, 0.0F};
  QVector4D color_width{0.0F, 0.0F, 0.0F, 0.0F};
  QVector4D sway_params{0.0F, 0.0F, 0.0F, 0.0F};
};

struct GrassBatchParams {
  static constexpr float k_default_soil_color_r = 0.28F;
  static constexpr float k_default_soil_color_g = 0.24F;
  static constexpr float k_default_soil_color_b = 0.18F;
  static constexpr float k_default_wind_strength = 0.25F;
  static constexpr float k_default_light_dir_x = 0.35F;
  static constexpr float k_default_light_dir_y = 0.8F;
  static constexpr float k_default_light_dir_z = 0.45F;
  static constexpr float k_default_wind_speed = 1.4F;

  static auto default_soil_color() -> QVector3D {
    return {k_default_soil_color_r, k_default_soil_color_g,
            k_default_soil_color_b};
  }
  static auto default_light_direction() -> QVector3D {
    return {k_default_light_dir_x, k_default_light_dir_y,
            k_default_light_dir_z};
  }

  QVector3D soil_color = default_soil_color();
  float wind_strength{k_default_wind_strength};
  QVector3D light_direction = default_light_direction();
  float wind_speed{k_default_wind_speed};
  float time{0.0F};
  float pad0{0.0F};
  float pad1{0.0F};
  float pad2{0.0F};
};

struct StoneInstanceGpu {
  QVector4D pos_scale;
  QVector4D color_rot;
};

struct StoneBatchParams {
  static constexpr float k_default_light_dir_x = 0.35F;
  static constexpr float k_default_light_dir_y = 0.8F;
  static constexpr float k_default_light_dir_z = 0.45F;

  static auto default_light_direction() -> QVector3D {
    return {k_default_light_dir_x, k_default_light_dir_y,
            k_default_light_dir_z};
  }

  QVector3D light_direction = default_light_direction();
  float time = 0.0F;
};

struct PlantInstanceGpu {
  QVector4D pos_scale;
  QVector4D color_sway;
  QVector4D type_params;
};

struct PlantBatchParams {
  static constexpr float k_default_light_dir_x = 0.35F;
  static constexpr float k_default_light_dir_y = 0.8F;
  static constexpr float k_default_light_dir_z = 0.45F;
  static constexpr float k_default_wind_strength = 0.25F;
  static constexpr float k_default_wind_speed = 1.4F;

  static auto default_light_direction() -> QVector3D {
    return {k_default_light_dir_x, k_default_light_dir_y,
            k_default_light_dir_z};
  }

  QVector3D light_direction = default_light_direction();
  float time = 0.0F;
  float wind_strength = k_default_wind_strength;
  float wind_speed = k_default_wind_speed;
  float pad0 = 0.0F;
  float pad1 = 0.0F;
};

struct PineInstanceGpu {
  QVector4D pos_scale;
  QVector4D color_sway;
  QVector4D rotation;
};

struct PineBatchParams {
  static constexpr float k_default_light_dir_x = 0.35F;
  static constexpr float k_default_light_dir_y = 0.8F;
  static constexpr float k_default_light_dir_z = 0.45F;
  static constexpr float k_default_wind_strength = 0.3F;
  static constexpr float k_default_wind_speed = 0.5F;

  static auto default_light_direction() -> QVector3D {
    return {k_default_light_dir_x, k_default_light_dir_y,
            k_default_light_dir_z};
  }

  QVector3D light_direction = default_light_direction();
  float time = 0.0F;
  float wind_strength = k_default_wind_strength;
  float wind_speed = k_default_wind_speed;
};

struct OliveInstanceGpu {
  QVector4D pos_scale;
  QVector4D color_sway;
  QVector4D rotation;
};

struct OliveBatchParams {
  static constexpr float k_default_light_dir_x = 0.35F;
  static constexpr float k_default_light_dir_y = 0.8F;
  static constexpr float k_default_light_dir_z = 0.45F;
  static constexpr float k_default_wind_strength = 0.3F;
  static constexpr float k_default_wind_speed = 0.5F;

  static auto default_light_direction() -> QVector3D {
    return {k_default_light_dir_x, k_default_light_dir_y,
            k_default_light_dir_z};
  }

  QVector3D light_direction = default_light_direction();
  float time = 0.0F;
  float wind_strength = k_default_wind_strength;
  float wind_speed = k_default_wind_speed;
};

struct FireCampInstanceGpu {
  QVector4D pos_intensity;
  QVector4D radius_phase;
};

struct FireCampBatchParams {
  static constexpr float k_default_flicker_speed = 5.0F;
  static constexpr float k_default_flicker_amount = 0.02F;
  static constexpr float k_default_glow_strength = 1.25F;

  float time = 0.0F;
  float flicker_speed = k_default_flicker_speed;
  float flicker_amount = k_default_flicker_amount;
  float glow_strength = k_default_glow_strength;
};

} // namespace Render::GL
