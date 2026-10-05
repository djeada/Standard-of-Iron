#pragma once

#include <vector>

#include "../movement/command_service.h"

namespace Engine::Core {
class Entity;
class TransformComponent;
} // namespace Engine::Core

namespace Game::Systems::Combat {

struct ChaseInputs {
  Engine::Core::Entity* attacker = nullptr;
  Engine::Core::TransformComponent* attacker_transform = nullptr;
  Engine::Core::Entity* target = nullptr;
  Engine::Core::TransformComponent* target_transform = nullptr;
  float range = 0.0F;
  bool ranged_unit = false;
  float delta_time = 0.0F;
};

void steer_toward_target(const ChaseInputs& inputs,
                         std::vector<CommandService::MoveIntent>& chase_move_intents);

} // namespace Game::Systems::Combat
