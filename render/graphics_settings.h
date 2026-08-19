#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "i_render_backend.h"

namespace Render {

enum class GraphicsQuality : std::uint8_t {
  Low = 0,
  Medium = 1,
  High = 2,
  Ultra = 3
};

inline constexpr GraphicsQuality k_default_graphics_quality = GraphicsQuality::High;
inline constexpr std::size_t k_graphics_quality_count = 4;

[[nodiscard]] inline constexpr auto
graphics_quality_key(GraphicsQuality q) noexcept -> const char* {
  switch (q) {
  case GraphicsQuality::Low:
    return "low";
  case GraphicsQuality::Medium:
    return "medium";
  case GraphicsQuality::High:
    return "high";
  case GraphicsQuality::Ultra:
    return "ultra";
  }
  return "unknown";
}

enum class ShaderTier : std::uint8_t {
  Low = 0,
  Medium = 1,
  High = 2,
  Ultra = 3
};

inline constexpr float k_never_cull_distance = 1.0e9F;

struct CreatureLodSettings {
  bool enabled = true;
  float full_distance_scale = 1.0F;
  float cull_distance = 200.0F;

  bool visibility_budget = true;
  int max_full_detail_units = 300;
};

struct BatchingConfig {
  bool force_batching = false;
  bool never_batch = false;
  int batching_unit_threshold = 50;
  float batching_zoom_start = 80.0F;
  float batching_zoom_full = 120.0F;
};

struct ContactShadowBudget {
  int max_casters = 6000;

  float max_distance = 140.0F;
};

struct DirectionalShadowSettings {
  bool enabled = true;
  int cascade_count = 3;
  int resolution = 2048;
  float distance = 80.0F;

  float depth_bias = 0.030F;

  float normal_bias = 1.40F;
  float cascade_blend = 0.12F;
};

struct PostProcessSettings {
  bool bloom = true;
  bool godrays = true;
  bool ambient_occlusion = true;
  bool fxaa = true;
};

struct WeatherBudget {
  float particle_scale = 1.0F;
};

struct PresentationSettings {
  int msaa_samples = 4;
};

struct TemplatePrewarmBudget {
  std::size_t items_per_tick = 160;
  std::uint32_t tick_budget_us = 2000;
  std::size_t template_cache_budget = 512;
};

struct GraphicsProfile {
  GraphicsQuality quality = GraphicsQuality::High;
  ShaderTier shader_tier = ShaderTier::High;
  CreatureLodSettings creature_lod{};
  BatchingConfig batching{};
  ContactShadowBudget contact_shadows{};
  DirectionalShadowSettings directional_shadows{};
  PostProcessSettings post_process{};
  WeatherBudget weather{};
  PresentationSettings presentation{};
  TemplatePrewarmBudget prewarm{};

  float grass_density = 1.0F;
};

namespace Detail {

inline constexpr std::array<GraphicsProfile, k_graphics_quality_count>
    k_graphics_profiles{{
        GraphicsProfile{
            .quality = GraphicsQuality::Low,
            .shader_tier = ShaderTier::Low,
            .creature_lod = {.enabled = true,
                             .full_distance_scale = 0.6F,
                             .cull_distance = 120.0F,
                             .visibility_budget = true,
                             .max_full_detail_units = 120},
            .batching = {.force_batching = true,
                         .never_batch = false,
                         .batching_unit_threshold = 0,
                         .batching_zoom_start = 0.0F,
                         .batching_zoom_full = 0.0F},
            .contact_shadows = {.max_casters = 900, .max_distance = 90.0F},
            .directional_shadows = {.enabled = false,
                                    .cascade_count = 1,
                                    .resolution = 1024,
                                    .distance = 25.0F,
                                    .depth_bias = 0.040F,
                                    .normal_bias = 1.80F,
                                    .cascade_blend = 0.0F},
            .post_process = {.bloom = false,
                             .godrays = false,
                             .ambient_occlusion = false,
                             .fxaa = false},
            .weather = {.particle_scale = 0.30F},
            .presentation = {.msaa_samples = 0},
            .prewarm = {.items_per_tick = 96,
                        .tick_budget_us = 1200,
                        .template_cache_budget = 256},
            .grass_density = 0.30F,
        },
        GraphicsProfile{
            .quality = GraphicsQuality::Medium,
            .shader_tier = ShaderTier::Medium,
            .creature_lod = {.enabled = true,
                             .full_distance_scale = 1.0F,
                             .cull_distance = 200.0F,
                             .visibility_budget = true,
                             .max_full_detail_units = 300},
            .batching = {.force_batching = false,
                         .never_batch = false,
                         .batching_unit_threshold = 30,
                         .batching_zoom_start = 60.0F,
                         .batching_zoom_full = 90.0F},
            .contact_shadows = {.max_casters = 2500, .max_distance = 140.0F},
            .directional_shadows = {.enabled = true,
                                    .cascade_count = 2,
                                    .resolution = 1024,
                                    .distance = 50.0F,
                                    .depth_bias = 0.035F,
                                    .normal_bias = 1.60F,
                                    .cascade_blend = 0.08F},
            .post_process = {.bloom = true,
                             .godrays = false,
                             .ambient_occlusion = true,
                             .fxaa = true},
            .weather = {.particle_scale = 0.60F},
            .presentation = {.msaa_samples = 2},
            .prewarm = {.items_per_tick = 160,
                        .tick_budget_us = 2000,
                        .template_cache_budget = 512},
            .grass_density = 0.65F,
        },
        GraphicsProfile{
            .quality = GraphicsQuality::High,
            .shader_tier = ShaderTier::High,
            .creature_lod = {.enabled = false,
                             .full_distance_scale = 1.0F,
                             .cull_distance = k_never_cull_distance,
                             .visibility_budget = false,
                             .max_full_detail_units = 5000},
            .batching = {.force_batching = false,
                         .never_batch = true,
                         .batching_unit_threshold = 999999,
                         .batching_zoom_start = 999999.0F,
                         .batching_zoom_full = 999999.0F},
            .contact_shadows = {.max_casters = 6000, .max_distance = 200.0F},
            .directional_shadows = {.enabled = true,
                                    .cascade_count = 4,
                                    .resolution = 4096,
                                    .distance = 200.0F,
                                    .depth_bias = 0.025F,
                                    .normal_bias = 1.20F,
                                    .cascade_blend = 0.15F},
            .post_process = {.bloom = true,
                             .godrays = true,
                             .ambient_occlusion = true,
                             .fxaa = true},
            .weather = {.particle_scale = 1.00F},
            .presentation = {.msaa_samples = 4},
            .prewarm = {.items_per_tick = 320,
                        .tick_budget_us = 4000,
                        .template_cache_budget = 2048},
            .grass_density = 1.0F,
        },
        GraphicsProfile{
            .quality = GraphicsQuality::Ultra,
            .shader_tier = ShaderTier::Ultra,
            .creature_lod = {.enabled = false,
                             .full_distance_scale = 1.0F,
                             .cull_distance = k_never_cull_distance,
                             .visibility_budget = false,
                             .max_full_detail_units = 5000},
            .batching = {.force_batching = false,
                         .never_batch = true,
                         .batching_unit_threshold = 999999,
                         .batching_zoom_start = 999999.0F,
                         .batching_zoom_full = 999999.0F},
            .contact_shadows = {.max_casters = 6000, .max_distance = 200.0F},
            .directional_shadows = {.enabled = true,
                                    .cascade_count = 4,
                                    .resolution = 4096,
                                    .distance = 200.0F,
                                    .depth_bias = 0.025F,
                                    .normal_bias = 1.20F,
                                    .cascade_blend = 0.15F},
            .post_process = {.bloom = true,
                             .godrays = true,
                             .ambient_occlusion = true,
                             .fxaa = true},
            .weather = {.particle_scale = 1.00F},
            .presentation = {.msaa_samples = 8},
            .prewarm = {.items_per_tick = 320,
                        .tick_budget_us = 4000,
                        .template_cache_budget = 2048},
            .grass_density = 1.0F,
        },
    }};

} // namespace Detail

[[nodiscard]] constexpr auto
graphics_profile_for(GraphicsQuality quality) noexcept -> const GraphicsProfile& {
  return Detail::k_graphics_profiles[static_cast<std::size_t>(quality)];
}

class GraphicsSettings {
public:
  static auto instance() noexcept -> GraphicsSettings& {
    static GraphicsSettings inst;
    return inst;
  }

  [[nodiscard]] auto quality() const noexcept -> GraphicsQuality { return m_quality; }

  void set_quality(GraphicsQuality q) noexcept {
    m_quality = q;
    m_profile = &graphics_profile_for(q);
    m_generation.fetch_add(1U, std::memory_order_release);
  }

  [[nodiscard]] auto profile() const noexcept -> const GraphicsProfile& {
    return *m_profile;
  }

  [[nodiscard]] auto generation() const noexcept -> std::uint32_t {
    return m_generation.load(std::memory_order_acquire);
  }

  [[nodiscard]] auto backend_kind() const noexcept -> ShaderQuality {
    return m_backend_kind;
  }
  void set_backend_kind(ShaderQuality kind) noexcept { m_backend_kind = kind; }

  [[nodiscard]] auto creature_lod() const noexcept -> const CreatureLodSettings& {
    return m_profile->creature_lod;
  }
  [[nodiscard]] auto batching_config() const noexcept -> const BatchingConfig& {
    return m_profile->batching;
  }
  [[nodiscard]] auto
  contact_shadow_budget() const noexcept -> const ContactShadowBudget& {
    return m_profile->contact_shadows;
  }
  [[nodiscard]] auto
  directional_shadows() const noexcept -> const DirectionalShadowSettings& {
    return m_profile->directional_shadows;
  }
  [[nodiscard]] auto weather_budget() const noexcept -> const WeatherBudget& {
    return m_profile->weather;
  }
  [[nodiscard]] auto presentation() const noexcept -> const PresentationSettings& {
    return m_profile->presentation;
  }
  [[nodiscard]] auto post_process() const noexcept -> const PostProcessSettings& {
    return m_profile->post_process;
  }
  [[nodiscard]] auto prewarm_budget() const noexcept -> const TemplatePrewarmBudget& {
    return m_profile->prewarm;
  }

  [[nodiscard]] auto creature_lod_enabled() const noexcept -> bool {
    return m_profile->creature_lod.enabled;
  }

  [[nodiscard]] auto
  calculate_batching_ratio(int visible_units,
                           float camera_height) const noexcept -> float {
    const BatchingConfig& batching = m_profile->batching;
    if (batching.never_batch) {
      return 0.0F;
    }
    if (batching.force_batching) {
      return 1.0F;
    }

    float unit_factor = 0.0F;
    if (visible_units > batching.batching_unit_threshold) {
      const int excess = visible_units - batching.batching_unit_threshold;
      const int range = std::max(batching.batching_unit_threshold * 3, 1);
      unit_factor = static_cast<float>(excess) / static_cast<float>(range);
      unit_factor = std::clamp(unit_factor, 0.0F, 1.0F);
    }

    float zoom_factor = 0.0F;
    if (camera_height > batching.batching_zoom_start) {
      const float range = batching.batching_zoom_full - batching.batching_zoom_start;
      if (range > 0.0F) {
        zoom_factor = (camera_height - batching.batching_zoom_start) / range;
        zoom_factor = std::clamp(zoom_factor, 0.0F, 1.0F);
      }
    }

    return std::max(unit_factor, zoom_factor);
  }

  [[nodiscard]] auto humanoid_full_detail_distance() const noexcept -> float {
    return k_base_humanoid_full * m_profile->creature_lod.full_distance_scale;
  }
  [[nodiscard]] auto horse_full_detail_distance() const noexcept -> float {
    return k_base_horse_full * m_profile->creature_lod.full_distance_scale;
  }
  [[nodiscard]] auto elephant_full_detail_distance() const noexcept -> float {
    return k_base_elephant_full * m_profile->creature_lod.full_distance_scale;
  }
  [[nodiscard]] auto creature_cull_distance() const noexcept -> float {
    return m_profile->creature_lod.cull_distance;
  }

  [[nodiscard]] auto shadow_max_distance() const noexcept -> float {
    return m_profile->contact_shadows.max_distance;
  }

private:
  GraphicsSettings() { set_quality(k_default_graphics_quality); }

  static constexpr float k_base_humanoid_full = 10.0F;
  static constexpr float k_base_horse_full = 20.0F;
  static constexpr float k_base_elephant_full = 35.0F;

  GraphicsQuality m_quality{k_default_graphics_quality};
  const GraphicsProfile* m_profile{&graphics_profile_for(k_default_graphics_quality)};
  ShaderQuality m_backend_kind{ShaderQuality::Full};
  std::atomic<std::uint32_t> m_generation{0U};
};

} // namespace Render
