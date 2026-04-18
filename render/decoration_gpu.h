#pragma once

// Stage 18 — unified decoration GPU data types.
//
// Replaces the separate grass/plant/stone/pine/olive/firecamp _gpu.h
// headers with a single consolidated layout. Each per-kind Instance and
// Params struct lives here; the backend's InstancedDecorationPipeline
// dispatcher routes to the appropriate pipeline based on
// DecorationBatchCmd::kind. See render/draw_queue.h.

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
  static constexpr float kDefaultSoilColorR = 0.28F;
  static constexpr float kDefaultSoilColorG = 0.24F;
  static constexpr float kDefaultSoilColorB = 0.18F;
  static constexpr float kDefaultWindStrength = 0.25F;
  static constexpr float kDefaultLightDirX = 0.35F;
  static constexpr float kDefaultLightDirY = 0.8F;
  static constexpr float kDefaultLightDirZ = 0.45F;
  static constexpr float kDefaultWindSpeed = 1.4F;

  static auto default_soil_color() -> QVector3D {
    return {kDefaultSoilColorR, kDefaultSoilColorG, kDefaultSoilColorB};
  }
  static auto default_light_direction() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
  }

  QVector3D soil_color = default_soil_color();
  float wind_strength{kDefaultWindStrength};
  QVector3D light_direction = default_light_direction();
  float wind_speed{kDefaultWindSpeed};
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
  static constexpr float kDefaultLightDirX = 0.35F;
  static constexpr float kDefaultLightDirY = 0.8F;
  static constexpr float kDefaultLightDirZ = 0.45F;

  static auto default_light_direction() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
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
  static constexpr float kDefaultLightDirX = 0.35F;
  static constexpr float kDefaultLightDirY = 0.8F;
  static constexpr float kDefaultLightDirZ = 0.45F;
  static constexpr float kDefaultWindStrength = 0.25F;
  static constexpr float kDefaultWindSpeed = 1.4F;

  static auto default_light_direction() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
  }

  QVector3D light_direction = default_light_direction();
  float time = 0.0F;
  float wind_strength = kDefaultWindStrength;
  float wind_speed = kDefaultWindSpeed;
  float pad0 = 0.0F;
  float pad1 = 0.0F;
};

struct PineInstanceGpu {
  QVector4D pos_scale;
  QVector4D color_sway;
  QVector4D rotation;
};

struct PineBatchParams {
  static constexpr float kDefaultLightDirX = 0.35F;
  static constexpr float kDefaultLightDirY = 0.8F;
  static constexpr float kDefaultLightDirZ = 0.45F;
  static constexpr float kDefaultWindStrength = 0.3F;
  static constexpr float kDefaultWindSpeed = 0.5F;

  static auto default_light_direction() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
  }

  QVector3D light_direction = default_light_direction();
  float time = 0.0F;
  float wind_strength = kDefaultWindStrength;
  float wind_speed = kDefaultWindSpeed;
};

struct OliveInstanceGpu {
  QVector4D pos_scale;
  QVector4D color_sway;
  QVector4D rotation;
};

struct OliveBatchParams {
  static constexpr float kDefaultLightDirX = 0.35F;
  static constexpr float kDefaultLightDirY = 0.8F;
  static constexpr float kDefaultLightDirZ = 0.45F;
  static constexpr float kDefaultWindStrength = 0.3F;
  static constexpr float kDefaultWindSpeed = 0.5F;

  static auto default_light_direction() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
  }

  QVector3D light_direction = default_light_direction();
  float time = 0.0F;
  float wind_strength = kDefaultWindStrength;
  float wind_speed = kDefaultWindSpeed;
};

struct FireCampInstanceGpu {
  QVector4D pos_intensity;
  QVector4D radius_phase;
};

struct FireCampBatchParams {
  static constexpr float kDefaultFlickerSpeed = 5.0F;
  static constexpr float kDefaultFlickerAmount = 0.02F;
  static constexpr float kDefaultGlowStrength = 1.25F;

  float time = 0.0F;
  float flicker_speed = kDefaultFlickerSpeed;
  float flicker_amount = kDefaultFlickerAmount;
  float glow_strength = kDefaultGlowStrength;
};

} // namespace Render::GL
