#pragma once

#include <cstdint>

#include "app/commander/commander_motor.h"

namespace Engine::Core {
class World;
class Entity;
class TransformComponent;
} // namespace Engine::Core

namespace App::Core {

struct StrikeCarry {
  std::uint8_t sequence = 0xFFU;
  float requested = 0.0F;
  float delivered = 0.0F;
};

// Moves the body through a committed swing's authored travel. It never writes
// the body's yaw: during the wind-up it works out how far the swing bends from
// the view toward its target (aim_offset), within the action's authored cone
// and turn limit, and CommanderBodyFacing turns the body by it. The lunge then
// travels where the body faces, pulled onto the target only when the target
// lies inside the swing's cone.
class CommanderLunge {
public:
  [[nodiscard]] auto apply(Engine::Core::World& world,
                           Engine::Core::Entity& commander,
                           Engine::Core::TransformComponent& transform,
                           CommanderMotor& motor,
                           bool dodging,
                           bool airborne,
                           float view_yaw,
                           float dt) -> float;

  // Degrees the current swing bends from the view toward its target; zero
  // outside a swing.
  [[nodiscard]] auto aim_offset() const -> float { return m_aim_offset; }

  void reset() {
    m_carry = {};
    m_aim_offset = 0.0F;
    m_aim_sequence = 0xFFU;
  }

private:
  void advance(Engine::Core::World& world,
               Engine::Core::Entity& commander,
               Engine::Core::TransformComponent& transform,
               CommanderMotor& motor,
               bool airborne,
               float view_yaw,
               float dt);

  StrikeCarry m_carry;
  float m_aim_offset = 0.0F;
  std::uint8_t m_aim_sequence = 0xFFU;
};

} // namespace App::Core
