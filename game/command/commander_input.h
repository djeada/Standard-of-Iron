#pragma once

#include <QJsonObject>
#include <QVector3D>

#include <cstdint>
#include <optional>

#include "../core/entity_id.h"

namespace Game::Command {

struct CommanderInputFrame {
  enum Button : std::uint32_t {
    Forward = 1U << 0U,
    Backward = 1U << 1U,
    Left = 1U << 2U,
    Right = 1U << 3U,
    TurnLeft = 1U << 4U,
    TurnRight = 1U << 5U,
    Run = 1U << 6U,
    PrimaryHeld = 1U << 7U,
    GuardHeld = 1U << 8U,
    PrimaryPressed = 1U << 9U,
    HeavyPressed = 1U << 10U,
    DodgePressed = 1U << 11U,
    JumpPressed = 1U << 12U,
    SpecialPressed = 1U << 13U,
    ShieldBashPressed = 1U << 14U,
    VanguardRushPressed = 1U << 15U,
    SecondWindPressed = 1U << 16U,
    HasDodgeDirection = 1U << 17U
  };

  Engine::Core::EntityID commander = 0;
  std::uint32_t buttons = 0;
  float view_yaw = 0.0F;
  QVector3D dodge_direction;
  std::uint64_t sequence = 0;

  [[nodiscard]] auto held(Button button) const -> bool {
    return (buttons & static_cast<std::uint32_t>(button)) != 0U;
  }

  void set(Button button, bool value) {
    if (value) {
      buttons |= static_cast<std::uint32_t>(button);
    } else {
      buttons &= ~static_cast<std::uint32_t>(button);
    }
  }
};

[[nodiscard]] auto to_json(const CommanderInputFrame& frame) -> QJsonObject;

[[nodiscard]] auto commander_input_from_json(const QJsonObject& object)
    -> std::optional<CommanderInputFrame>;

} // namespace Game::Command
