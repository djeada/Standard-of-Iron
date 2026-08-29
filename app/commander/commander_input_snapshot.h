#pragma once

#include <QVector3D>

#include <cstdint>

struct CommanderInputSnapshot {

  bool forward = false;
  bool backward = false;
  bool left = false;
  bool right = false;
  bool turn_left = false;
  bool turn_right = false;
  bool run = false;
  bool primary_held = false;
  bool guard_held = false;

  bool primary_pressed = false;
  bool dodge_pressed = false;
  bool jump_pressed = false;
  bool shield_bash_pressed = false;
  bool vanguard_rush_pressed = false;
  bool second_wind_pressed = false;

  bool has_dodge_direction = false;
  QVector3D dodge_direction{0.0F, 0.0F, 0.0F};

  std::uint64_t sequence = 0;

  [[nodiscard]] auto has_pending_edge() const -> bool {
    return primary_pressed || dodge_pressed || jump_pressed || shield_bash_pressed ||
           vanguard_rush_pressed || second_wind_pressed;
  }

  [[nodiscard]] auto forward_axis() const -> int {
    return (forward ? 1 : 0) - (backward ? 1 : 0);
  }

  [[nodiscard]] auto right_axis() const -> int {
    return (right ? 1 : 0) - (left ? 1 : 0);
  }
};
