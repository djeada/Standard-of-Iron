#pragma once

#include <cstdint>

#include "game/commander/commander_motor.h"

namespace Engine::Core {
class World;
class Entity;
class TransformComponent;
} // namespace Engine::Core

namespace Game::Commander {

struct StrikeCarry {
  std::uint8_t sequence = 0xFFU;
  float requested = 0.0F;
  float delivered = 0.0F;
};

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

  [[nodiscard]] auto aim_offset() const -> float { return m_aim_offset; }

  void reset() {
    m_carry = {};
    m_aim_offset = 0.0F;
    m_aim_sequence = 0xFFU;
  }

private:
  friend class CommanderRuntime;

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

} // namespace Game::Commander
