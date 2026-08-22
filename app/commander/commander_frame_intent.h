#pragma once

#include <QVector2D>

#include <cmath>
#include <cstdint>

struct CommanderFrameIntent {

  QVector2D move{0.0F, 0.0F};

  QVector2D look_delta{0.0F, 0.0F};

  float view_yaw = 0.0F;
  float view_pitch = 0.0F;

  bool guard = false;
  bool attack_held = false;
  bool run = false;
  bool dodge_pressed = false;
  bool jump_pressed = false;

  std::uint64_t frame_index = 0;

  [[nodiscard]] auto has_look_delta() const -> bool {
    return std::abs(look_delta.x()) > 1.0e-4F || std::abs(look_delta.y()) > 1.0e-4F;
  }

  [[nodiscard]] auto has_move() const -> bool { return move.lengthSquared() > 1.0e-6F; }
};
