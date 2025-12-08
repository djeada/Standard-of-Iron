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
  float humanoidFull;
  float humanoidReduced;
  float humanoidMinimal;
  float humanoidBillboard;

  float horseFull;
  float horseReduced;
  float horseMinimal;
  float horseBillboard;

  float shadowDistance;
  bool enableShadows;
};

struct GraphicsFeatures {
  bool enableFacialHair;
  bool enableManeDetail;
  bool enableTailDetail;
  bool enableArmorDetail;
  bool enableEquipmentDetail;
  bool enableGroundShadows;
  bool enablePoseCache;
};

struct BatchingConfig {
  bool forceBatching;
  bool neverBatch;
  int batchingUnitThreshold;
  float batchingZoomStart;
  float batchingZoomFull;
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
    if (m_batchingConfig.neverBatch) {
      return 0.0F;
    }
    if (m_batchingConfig.forceBatching) {
      return 1.0F;
    }

    float unitFactor = 0.0F;
    if (visibleUnits > m_batchingConfig.batchingUnitThreshold) {

      int excess = visibleUnits - m_batchingConfig.batchingUnitThreshold;
      int range = m_batchingConfig.batchingUnitThreshold * 3;
      unitFactor = static_cast<float>(excess) / static_cast<float>(range);
      unitFactor =
          unitFactor < 0.0F ? 0.0F : (unitFactor > 1.0F ? 1.0F : unitFactor);
    }

    float zoomFactor = 0.0F;
    if (cameraHeight > m_batchingConfig.batchingZoomStart) {
      float range = m_batchingConfig.batchingZoomFull -
                    m_batchingConfig.batchingZoomStart;
      if (range > 0.0F) {
        zoomFactor =
            (cameraHeight - m_batchingConfig.batchingZoomStart) / range;
        zoomFactor =
            zoomFactor < 0.0F ? 0.0F : (zoomFactor > 1.0F ? 1.0F : zoomFactor);
      }
    }

    return unitFactor > zoomFactor ? unitFactor : zoomFactor;
  }

  [[nodiscard]] auto humanoidFullDetailDistance() const noexcept -> float {
    return kBaseHumanoidFull * m_lodMultipliers.humanoidFull;
  }
  [[nodiscard]] auto humanoidReducedDetailDistance() const noexcept -> float {
    return kBaseHumanoidReduced * m_lodMultipliers.humanoidReduced;
  }
  [[nodiscard]] auto humanoidMinimalDetailDistance() const noexcept -> float {
    return kBaseHumanoidMinimal * m_lodMultipliers.humanoidMinimal;
  }
  [[nodiscard]] auto humanoidBillboardDistance() const noexcept -> float {
    return kBaseHumanoidBillboard * m_lodMultipliers.humanoidBillboard;
  }

  [[nodiscard]] auto horseFullDetailDistance() const noexcept -> float {
    return kBaseHorseFull * m_lodMultipliers.horseFull;
  }
  [[nodiscard]] auto horseReducedDetailDistance() const noexcept -> float {
    return kBaseHorseReduced * m_lodMultipliers.horseReduced;
  }
  [[nodiscard]] auto horseMinimalDetailDistance() const noexcept -> float {
    return kBaseHorseMinimal * m_lodMultipliers.horseMinimal;
  }
  [[nodiscard]] auto horseBillboardDistance() const noexcept -> float {
    return kBaseHorseBillboard * m_lodMultipliers.horseBillboard;
  }

  [[nodiscard]] auto shadowMaxDistance() const noexcept -> float {
    return m_lodMultipliers.shadowDistance;
  }
  [[nodiscard]] auto shadowsEnabled() const noexcept -> bool {
    return m_lodMultipliers.enableShadows;
  }

private:
  GraphicsSettings() { applyPreset(GraphicsQuality::Medium); }

  void applyPreset(GraphicsQuality q) noexcept {
    switch (q) {
    case GraphicsQuality::Low:

      m_lodMultipliers = {.humanoidFull = 0.8F,
                          .humanoidReduced = 0.8F,
                          .humanoidMinimal = 0.8F,
                          .humanoidBillboard = 0.8F,
                          .horseFull = 0.8F,
                          .horseReduced = 0.8F,
                          .horseMinimal = 0.8F,
                          .horseBillboard = 0.8F,
                          .shadowDistance = 25.0F,
                          .enableShadows = true};
      m_features = {.enableFacialHair = false,
                    .enableManeDetail = false,
                    .enableTailDetail = false,
                    .enableArmorDetail = true,
                    .enableEquipmentDetail = true,
                    .enableGroundShadows = true,
                    .enablePoseCache = true};
      m_batchingConfig = {.forceBatching = true,
                          .neverBatch = false,
                          .batchingUnitThreshold = 0,
                          .batchingZoomStart = 0.0F,
                          .batchingZoomFull = 0.0F};
      break;

    case GraphicsQuality::Medium:

      m_lodMultipliers = {.humanoidFull = 1.0F,
                          .humanoidReduced = 1.0F,
                          .humanoidMinimal = 1.0F,
                          .humanoidBillboard = 1.0F,
                          .horseFull = 1.0F,
                          .horseReduced = 1.0F,
                          .horseMinimal = 1.0F,
                          .horseBillboard = 1.0F,
                          .shadowDistance = 40.0F,
                          .enableShadows = true};
      m_features = {.enableFacialHair = true,
                    .enableManeDetail = true,
                    .enableTailDetail = true,
                    .enableArmorDetail = true,
                    .enableEquipmentDetail = true,
                    .enableGroundShadows = true,
                    .enablePoseCache = true};

      m_batchingConfig = {.forceBatching = false,
                          .neverBatch = false,
                          .batchingUnitThreshold = 30,
                          .batchingZoomStart = 60.0F,
                          .batchingZoomFull = 90.0F};
      break;

    case GraphicsQuality::High:

      m_lodMultipliers = {.humanoidFull = 2.0F,
                          .humanoidReduced = 2.0F,
                          .humanoidMinimal = 2.0F,
                          .humanoidBillboard = 2.0F,
                          .horseFull = 2.0F,
                          .horseReduced = 2.0F,
                          .horseMinimal = 2.0F,
                          .horseBillboard = 2.0F,
                          .shadowDistance = 80.0F,
                          .enableShadows = true};
      m_features = {.enableFacialHair = true,
                    .enableManeDetail = true,
                    .enableTailDetail = true,
                    .enableArmorDetail = true,
                    .enableEquipmentDetail = true,
                    .enableGroundShadows = true,
                    .enablePoseCache = true};

      m_batchingConfig = {.forceBatching = false,
                          .neverBatch = false,
                          .batchingUnitThreshold = 50,
                          .batchingZoomStart = 80.0F,
                          .batchingZoomFull = 120.0F};
      break;

    case GraphicsQuality::Ultra:

      m_lodMultipliers = {.humanoidFull = 100.0F,
                          .humanoidReduced = 100.0F,
                          .humanoidMinimal = 100.0F,
                          .humanoidBillboard = 100.0F,
                          .horseFull = 100.0F,
                          .horseReduced = 100.0F,
                          .horseMinimal = 100.0F,
                          .horseBillboard = 100.0F,
                          .shadowDistance = 200.0F,
                          .enableShadows = true};
      m_features = {.enableFacialHair = true,
                    .enableManeDetail = true,
                    .enableTailDetail = true,
                    .enableArmorDetail = true,
                    .enableEquipmentDetail = true,
                    .enableGroundShadows = true,
                    .enablePoseCache = false};

      m_batchingConfig = {.forceBatching = false,
                          .neverBatch = true,
                          .batchingUnitThreshold = 999999,
                          .batchingZoomStart = 999999.0F,
                          .batchingZoomFull = 999999.0F};
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

  GraphicsQuality m_quality{GraphicsQuality::Medium};
  LODMultipliers m_lodMultipliers{};
  GraphicsFeatures m_features{};
  BatchingConfig m_batchingConfig{};
};

} // namespace Render
