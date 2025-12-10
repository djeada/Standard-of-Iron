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

class GraphicsSettings {
public:
  static auto instance() noexcept -> GraphicsSettings & {
    static GraphicsSettings inst;
    return inst;
  }

  [[nodiscard]] auto quality() const noexcept -> GraphicsQuality {
    return m_quality;
  }

  void setQuality(GraphicsQuality q) noexcept {
    m_quality = q;
    applyPreset(q);
  }

  [[nodiscard]] auto lodMultipliers() const noexcept -> const LODMultipliers & {
    return m_lodMultipliers;
  }

  [[nodiscard]] auto features() const noexcept -> const GraphicsFeatures & {
    return m_features;
  }

  [[nodiscard]] auto batchingConfig() const noexcept -> const BatchingConfig & {
    return m_batchingConfig;
  }

  [[nodiscard]] auto
  calculateBatchingRatio(int visibleUnits,
                         float cameraHeight) const noexcept -> float {
    if (m_batchingConfig.never_batch) {
      return 0.0F;
    }
    if (m_batchingConfig.force_batching) {
      return 1.0F;
    }

    float unitFactor = 0.0F;
    if (visibleUnits > m_batchingConfig.batching_unit_threshold) {

      int excess = visibleUnits - m_batchingConfig.batching_unit_threshold;
      int range = m_batchingConfig.batching_unit_threshold * 3;
      unitFactor = static_cast<float>(excess) / static_cast<float>(range);
      unitFactor =
          unitFactor < 0.0F ? 0.0F : (unitFactor > 1.0F ? 1.0F : unitFactor);
    }

    float zoomFactor = 0.0F;
    if (cameraHeight > m_batchingConfig.batching_zoom_start) {
      float range = m_batchingConfig.batching_zoom_full -
                    m_batchingConfig.batching_zoom_start;
      if (range > 0.0F) {
        zoomFactor =
            (cameraHeight - m_batchingConfig.batching_zoom_start) / range;
        zoomFactor =
            zoomFactor < 0.0F ? 0.0F : (zoomFactor > 1.0F ? 1.0F : zoomFactor);
      }
    }

    return unitFactor > zoomFactor ? unitFactor : zoomFactor;
  }

  [[nodiscard]] auto humanoidFullDetailDistance() const noexcept -> float {
    return kBaseHumanoidFull * m_lodMultipliers.humanoid_full;
  }
  [[nodiscard]] auto humanoidReducedDetailDistance() const noexcept -> float {
    return kBaseHumanoidReduced * m_lodMultipliers.humanoid_reduced;
  }
  [[nodiscard]] auto humanoidMinimalDetailDistance() const noexcept -> float {
    return kBaseHumanoidMinimal * m_lodMultipliers.humanoid_minimal;
  }
  [[nodiscard]] auto humanoidBillboardDistance() const noexcept -> float {
    return kBaseHumanoidBillboard * m_lodMultipliers.humanoid_billboard;
  }

  [[nodiscard]] auto horseFullDetailDistance() const noexcept -> float {
    return kBaseHorseFull * m_lodMultipliers.horse_full;
  }
  [[nodiscard]] auto horseReducedDetailDistance() const noexcept -> float {
    return kBaseHorseReduced * m_lodMultipliers.horse_reduced;
  }
  [[nodiscard]] auto horseMinimalDetailDistance() const noexcept -> float {
    return kBaseHorseMinimal * m_lodMultipliers.horse_minimal;
  }
  [[nodiscard]] auto horseBillboardDistance() const noexcept -> float {
    return kBaseHorseBillboard * m_lodMultipliers.horse_billboard;
  }

  [[nodiscard]] auto shadowMaxDistance() const noexcept -> float {
    return m_lodMultipliers.shadow_distance;
  }
  [[nodiscard]] auto shadowsEnabled() const noexcept -> bool {
    return m_lodMultipliers.enable_shadows;
  }

private:
  GraphicsSettings() { setQuality(GraphicsQuality::Ultra); }

  void applyPreset(GraphicsQuality q) noexcept {
    switch (q) {
    case GraphicsQuality::Low:

      m_lodMultipliers = {.humanoid_full = 0.8F,
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
      m_batchingConfig = {.force_batching = true,
                          .never_batch = false,
                          .batching_unit_threshold = 0,
                          .batching_zoom_start = 0.0F,
                          .batching_zoom_full = 0.0F};
      break;

    case GraphicsQuality::Medium:

      m_lodMultipliers = {.humanoid_full = 1.0F,
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

      m_batchingConfig = {.force_batching = false,
                          .never_batch = false,
                          .batching_unit_threshold = 30,
                          .batching_zoom_start = 60.0F,
                          .batching_zoom_full = 90.0F};
      break;

    case GraphicsQuality::High:

      m_lodMultipliers = {.humanoid_full = 2.0F,
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

      m_batchingConfig = {.force_batching = false,
                          .never_batch = false,
                          .batching_unit_threshold = 50,
                          .batching_zoom_start = 80.0F,
                          .batching_zoom_full = 120.0F};
      break;

    case GraphicsQuality::Ultra:

      m_lodMultipliers = {.humanoid_full = 100.0F,
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

      m_batchingConfig = {.force_batching = false,
                          .never_batch = true,
                          .batching_unit_threshold = 999999,
                          .batching_zoom_start = 999999.0F,
                          .batching_zoom_full = 999999.0F};
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
  LODMultipliers m_lodMultipliers{};
  GraphicsFeatures m_features{};
  BatchingConfig m_batchingConfig{};
};

} // namespace Render
