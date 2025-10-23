#pragma once

#include <cstdint>

namespace Render::GL {

struct HumanProportions {

  static constexpr float TOTAL_HEIGHT = 1.80f;
  static constexpr float HEAD_HEIGHT = 0.23f;

  static constexpr float GROUND_Y = 0.0f;
  static constexpr float HEAD_TOP_Y = GROUND_Y + TOTAL_HEIGHT;
  static constexpr float CHIN_Y = HEAD_TOP_Y - HEAD_HEIGHT;
  static constexpr float NECK_BASE_Y = CHIN_Y - 0.08f;
  static constexpr float SHOULDER_Y = NECK_BASE_Y - 0.04f;
  static constexpr float CHEST_Y = SHOULDER_Y - 0.31f;
  static constexpr float WAIST_Y = CHEST_Y - 0.25f;

  static constexpr float UPPER_LEG_LEN = 0.35f;
  static constexpr float LOWER_LEG_LEN = 0.35f;
  static constexpr float KNEE_Y = WAIST_Y - UPPER_LEG_LEN;

  static constexpr float SHOULDER_WIDTH = HEAD_HEIGHT * 1.85f;
  static constexpr float HEAD_RADIUS = HEAD_HEIGHT * 0.42f;
  static constexpr float NECK_RADIUS = HEAD_RADIUS * 0.38f;
  static constexpr float TORSO_TOP_R = HEAD_RADIUS * 1.15f;
  static constexpr float TORSO_BOT_R = HEAD_RADIUS * 1.05f;
  static constexpr float UPPER_ARM_R = HEAD_RADIUS * 0.38f;
  static constexpr float FORE_ARM_R = HEAD_RADIUS * 0.30f;
  static constexpr float HAND_RADIUS = HEAD_RADIUS * 0.28f;
  static constexpr float UPPER_LEG_R = HEAD_RADIUS * 0.50f;
  static constexpr float LOWER_LEG_R = HEAD_RADIUS * 0.42f;

  static constexpr float UPPER_ARM_LEN = 0.28f;
  static constexpr float FORE_ARM_LEN = 0.30f;
};

enum class MaterialType : uint8_t {
  Cloth = 0,
  Leather = 1,
  Metal = 2,
  Wood = 3,
  Skin = 4
};

} 
