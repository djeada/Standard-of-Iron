#pragma once

#include <cstdint>

namespace Render::GL {

struct HumanProportions {

  static constexpr float TOTAL_HEIGHT = 1.80F;
  static constexpr float HEADS_TALL = 7.5F;
  static constexpr float HEAD_HEIGHT = TOTAL_HEIGHT / HEADS_TALL;

  static constexpr float GROUND_Y = 0.0F;
  static constexpr float HEAD_TOP_Y = GROUND_Y + TOTAL_HEIGHT;
  static constexpr float CHIN_Y = HEAD_TOP_Y - HEAD_HEIGHT;
  static constexpr float HEAD_NECK_OVERLAP = 0.025F;
  static constexpr float HEAD_CENTER_Y =
      (HEAD_TOP_Y + CHIN_Y) * 0.5F - HEAD_NECK_OVERLAP;
  static constexpr float NECK_BASE_Y = CHIN_Y - 0.045F;
  static constexpr float SHOULDER_Y = NECK_BASE_Y - 0.09F;
  static constexpr float CHEST_Y = SHOULDER_Y - 0.27F;
  static constexpr float WAIST_Y = CHEST_Y - 0.18F;

  static constexpr float UPPER_LEG_LEN = 0.50F;
  static constexpr float LOWER_LEG_LEN = 0.47F;
  static constexpr float KNEE_Y = WAIST_Y - UPPER_LEG_LEN;

  static constexpr float SHOULDER_WIDTH = HEAD_HEIGHT * 1.82F;
  static constexpr float HEAD_RADIUS = HEAD_HEIGHT * 0.41F;
  static constexpr float NECK_RADIUS = HEAD_RADIUS * 0.39F;
  static constexpr float TORSO_TOP_R = HEAD_RADIUS * 1.25F;
  static constexpr float TORSO_BOT_R = HEAD_RADIUS * 1.12F;
  static constexpr float UPPER_ARM_R = HEAD_RADIUS * 0.45F;
  static constexpr float FORE_ARM_R = HEAD_RADIUS * 0.32F;
  static constexpr float HAND_RADIUS = HEAD_RADIUS * 0.28F;
  static constexpr float UPPER_LEG_R = HEAD_RADIUS * 0.64F;
  static constexpr float LOWER_LEG_R = HEAD_RADIUS * 0.44F;

  static constexpr float UPPER_ARM_LEN = 0.32F;
  static constexpr float FORE_ARM_LEN = 0.27F;

  // Shoulder and trapezius geometry for rounded upper body
  static constexpr float TRAP_WIDTH_SCALE = 0.42F;
  static constexpr float TRAP_HEIGHT_SCALE = 0.55F;
  static constexpr float SHOULDER_CAP_RADIUS_SCALE = 0.85F;
  static constexpr float DELTOID_RADIUS_SCALE = 0.75F;

  // Body structure offsets
  static constexpr float HIP_LATERAL_OFFSET = 0.10F;
  static constexpr float HIP_VERTICAL_OFFSET = -0.02F;
  static constexpr float FOOT_Y_OFFSET_DEFAULT = 0.022F;

  // Torso depth and shape constants
  static constexpr float TORSO_DEPTH_FACTOR_BASE = 0.55F;
  static constexpr float TORSO_DEPTH_FACTOR_MIN = 0.40F;
  static constexpr float TORSO_DEPTH_FACTOR_MAX = 0.85F;
  static constexpr float TORSO_TOP_COVER_OFFSET = -0.03F;

  // Rendering thresholds and tolerances
  static constexpr float EPSILON_SMALL = 1e-5F;
  static constexpr float EPSILON_TINY = 1e-6F;
  static constexpr float EPSILON_VECTOR = 1e-8F;
};

enum class MaterialType : uint8_t {
  Cloth = 0,
  Leather = 1,
  Metal = 2,
  Wood = 3,
  Skin = 4
};

} // namespace Render::GL
