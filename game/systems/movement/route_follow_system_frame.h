#pragma once

#include "core/component_combat.h"
#include "core/world.h"

namespace Game::Systems {

struct FollowFrame {
  Engine::Core::World& world;
  Engine::Core::Entity& entity;
  Engine::Core::TransformComponent& transform;
  Engine::Core::MovementComponent& movement;
  Engine::Core::UnitComponent& unit;
  Engine::Core::MovementFactsComponent& facts;
  float delta_time;
};

} // namespace Game::Systems
