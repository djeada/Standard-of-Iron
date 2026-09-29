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

class CommanderLunge {
public:
  [[nodiscard]] auto apply(Engine::Core::World& world,
                           Engine::Core::Entity& commander,
                           Engine::Core::TransformComponent& transform,
                           CommanderMotor& motor,
                           bool dodging,
                           bool airborne,
                           float dt) -> float;

private:
  void advance(Engine::Core::World& world,
               Engine::Core::Entity& commander,
               Engine::Core::TransformComponent& transform,
               CommanderMotor& motor,
               bool airborne,
               float dt);

  StrikeCarry m_carry;
};

} // namespace App::Core
