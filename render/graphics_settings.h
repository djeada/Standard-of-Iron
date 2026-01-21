#pragma once

#include <cstdint>

namespace Render {

enum class GraphicsQuality : uint8_t {
  Low = 0,
  Medium = 1,
  High = 2,
  Ultra = 3
};

struct LODMultipliers {
  float humanoid_full;
  float humanoid_reduced;
  float humanoid_minimal;
  float humanoid_billboard;

  float horse_full;
  float horse_reduced;
  float horse_minimal;
  float horse_billboard;

  float shadow_distance;
  bool enable_shadows;
};

struct GraphicsFeatures {
  bool enable_facial_hair;
  bool enable_mane_detail;
  bool enable_tail_detail;
  bool enable_armor_detail;
  bool enable_equipment_detail;
  bool enable_ground_shadows;
  bool enable_pose_cache;
};

struct BatchingConfig {
  bool force_batching;
  bool never_batch;
  int batching_unit_threshold;
  float batching_zoom_start;
  float batching_zoom_full;
};

struct VisibilityBudget {
  int max_full_detail_units;
  bool enabled;
};

class GraphicsSettings {
public:
  static auto instance() noexcept -> GraphicsSettings & {
    static GraphicsSettings inst;
    return inst;
  }

  [[nodiscard]] auto quality() const noexcept -> GraphicsQuality {
    return m_quality;
  }

  void set_quality(GraphicsQuality q) noexcept {
    m_quality = q;
    apply_preset(q);
  }

  [[nodiscard]] auto
  lod_multipliers() const noexcept -> const LODMultipliers & {
    return m_lod_multipliers;
  }

  [[nodiscard]] auto features() const noexcept -> const GraphicsFeatures & {
    return m_features;
  }

  [[nodiscard]] auto
  batching_config() const noexcept -> const BatchingConfig & {
    return m_batching_config;
  }

  [[nodiscard]] auto
  visibility_budget() const noexcept -> const VisibilityBudget & {
    return m_visibility_budget;
  }

  [[nodiscard]] auto
  calculate_batching_ratio(int visible_units,
                           float camera_height) const noexcept -> float {
    if (m_batching_config.never_batch) {
      return 0.0F;
    }
    if (m_batching_config.force_batching) {
      return 1.0F;
    }

    float unit_factor = 0.0F;
    if (visible_units > m_batching_config.batching_unit_threshold) {

      int excess = visible_units - m_batching_config.batching_unit_threshold;
      int range = m_batching_config.batching_unit_threshold * 3;
      unit_factor = static_cast<float>(excess) / static_cast<float>(range);
      unit_factor =
          unit_factor < 0.0F ? 0.0F : (unit_factor > 1.0F ? 1.0F : unit_factor);
    }

    float zoom_factor = 0.0F;
    if (camera_height > m_batching_config.batching_zoom_start) {
      float range = m_batching_config.batching_zoom_full -
                    m_batching_config.batching_zoom_start;
      if (range > 0.0F) {
        zoom_factor =
            (camera_height - m_batching_config.batching_zoom_start) / range;
        zoom_factor = zoom_factor < 0.0F
                          ? 0.0F
                          : (zoom_factor > 1.0F ? 1.0F : zoom_factor);
      }
    }

    return unit_factor > zoom_factor ? unit_factor : zoom_factor;
  }

  [[nodiscard]] auto humanoid_full_detail_distance() const noexcept -> float {
    return kBaseHumanoidFull * m_lod_multipliers.humanoid_full;
  }
  [[nodiscard]] auto
  humanoid_reduced_detail_distance() const noexcept -> float {
    return kBaseHumanoidReduced * m_lod_multipliers.humanoid_reduced;
  }
  [[nodiscard]] auto
  humanoid_minimal_detail_distance() const noexcept -> float {
    return kBaseHumanoidMinimal * m_lod_multipliers.humanoid_minimal;
  }
  [[nodiscard]] auto humanoid_billboard_distance() const noexcept -> float {
    return kBaseHumanoidBillboard * m_lod_multipliers.humanoid_billboard;
  }

  [[nodiscard]] auto horse_full_detail_distance() const noexcept -> float {
    return kBaseHorseFull * m_lod_multipliers.horse_full;
  }
  [[nodiscard]] auto horse_reduced_detail_distance() const noexcept -> float {
    return kBaseHorseReduced * m_lod_multipliers.horse_reduced;
  }
  [[nodiscard]] auto horse_minimal_detail_distance() const noexcept -> float {
    return kBaseHorseMinimal * m_lod_multipliers.horse_minimal;
  }
  [[nodiscard]] auto horse_billboard_distance() const noexcept -> float {
    return kBaseHorseBillboard * m_lod_multipliers.horse_billboard;
  }

  [[nodiscard]] auto shadow_max_distance() const noexcept -> float {
    return m_lod_multipliers.shadow_distance;
  }
  [[nodiscard]] auto shadows_enabled() const noexcept -> bool {
    return m_lod_multipliers.enable_shadows;
  }

private:
  GraphicsSettings() { set_quality(GraphicsQuality::Ultra); }

  void apply_preset(GraphicsQuality q) noexcept {
    switch (q) {
    case GraphicsQuality::Low:

      m_lod_multipliers = {.humanoid_full = 0.8F,
                           .humanoid_reduced = 0.8F,
                           .humanoid_minimal = 0.8F,
                           .humanoid_billboard = 0.8F,
                           .horse_full = 0.8F,
                           .horse_reduced = 0.8F,
                           .horse_minimal = 0.8F,
                           .horse_billboard = 0.8F,
                           .shadow_distance = 25.0F,
                           .enable_shadows = true};
      m_features = {.enable_facial_hair = false,
                    .enable_mane_detail = false,
                    .enable_tail_detail = false,
                    .enable_armor_detail = true,
                    .enable_equipment_detail = true,
                    .enable_ground_shadows = true,
                    .enable_pose_cache = true};
      m_batching_config = {.force_batching = true,
                           .never_batch = false,
                           .batching_unit_threshold = 0,
                           .batching_zoom_start = 0.0F,
                           .batching_zoom_full = 0.0F};
      m_visibility_budget = {.max_full_detail_units = 150, .enabled = true};
      break;

    case GraphicsQuality::Medium:

      m_lod_multipliers = {.humanoid_full = 1.0F,
                           .humanoid_reduced = 1.0F,
                           .humanoid_minimal = 1.0F,
                           .humanoid_billboard = 1.0F,
                           .horse_full = 1.0F,
                           .horse_reduced = 1.0F,
                           .horse_minimal = 1.0F,
                           .horse_billboard = 1.0F,
                           .shadow_distance = 40.0F,
                           .enable_shadows = true};
      m_features = {.enable_facial_hair = true,
                    .enable_mane_detail = true,
                    .enable_tail_detail = true,
                    .enable_armor_detail = true,
                    .enable_equipment_detail = true,
                    .enable_ground_shadows = true,
                    .enable_pose_cache = true};

      m_batching_config = {.force_batching = false,
                           .never_batch = false,
                           .batching_unit_threshold = 30,
                           .batching_zoom_start = 60.0F,
                           .batching_zoom_full = 90.0F};
      m_visibility_budget = {.max_full_detail_units = 300, .enabled = true};
      break;

    case GraphicsQuality::High:

      m_lod_multipliers = {.humanoid_full = 2.0F,
                           .humanoid_reduced = 2.0F,
                           .humanoid_minimal = 2.0F,
                           .humanoid_billboard = 2.0F,
                           .horse_full = 2.0F,
                           .horse_reduced = 2.0F,
                           .horse_minimal = 2.0F,
                           .horse_billboard = 2.0F,
                           .shadow_distance = 80.0F,
                           .enable_shadows = true};
      m_features = {.enable_facial_hair = true,
                    .enable_mane_detail = true,
                    .enable_tail_detail = true,
                    .enable_armor_detail = true,
                    .enable_equipment_detail = true,
                    .enable_ground_shadows = true,
                    .enable_pose_cache = true};

      m_batching_config = {.force_batching = false,
                           .never_batch = false,
                           .batching_unit_threshold = 50,
                           .batching_zoom_start = 80.0F,
                           .batching_zoom_full = 120.0F};
      m_visibility_budget = {.max_full_detail_units = 900, .enabled = true};
      break;

    case GraphicsQuality::Ultra:

      m_lod_multipliers = {.humanoid_full = 100.0F,
                           .humanoid_reduced = 100.0F,
                           .humanoid_minimal = 100.0F,
                           .humanoid_billboard = 100.0F,
                           .horse_full = 100.0F,
                           .horse_reduced = 100.0F,
                           .horse_minimal = 100.0F,
                           .horse_billboard = 100.0F,
                           .shadow_distance = 200.0F,
                           .enable_shadows = true};
      m_features = {.enable_facial_hair = true,
                    .enable_mane_detail = true,
                    .enable_tail_detail = true,
                    .enable_armor_detail = true,
                    .enable_equipment_detail = true,
                    .enable_ground_shadows = true,
                    .enable_pose_cache = false};

      m_batching_config = {.force_batching = false,
                           .never_batch = true,
                           .batching_unit_threshold = 999999,
                           .batching_zoom_start = 999999.0F,
                           .batching_zoom_full = 999999.0F};
      m_visibility_budget = {.max_full_detail_units = 5000, .enabled = false};
      break;
    }
  }

  static constexpr float kBaseHumanoidFull = 15.0F;
  static constexpr float kBaseHumanoidReduced = 35.0F;
  static constexpr float kBaseHumanoidMinimal = 60.0F;
  static constexpr float kBaseHumanoidBillboard = 100.0F;

  static constexpr float kBaseHorseFull = 20.0F;
  static constexpr float kBaseHorseReduced = 40.0F;
  static constexpr float kBaseHorseMinimal = 70.0F;
  static constexpr float kBaseHorseBillboard = 100.0F;

  GraphicsQuality m_quality{GraphicsQuality::Ultra};
  LODMultipliers m_lod_multipliers{};
  GraphicsFeatures m_features{};
  BatchingConfig m_batching_config{};
  VisibilityBudget m_visibility_budget{};
};

} // namespace Render
