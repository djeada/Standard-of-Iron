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
};

enum class MaterialType : uint8_t {
  Cloth = 0,
  Leather = 1,
  Metal = 2,
  Wood = 3,
  Skin = 4
};

} // namespace Render::GL
