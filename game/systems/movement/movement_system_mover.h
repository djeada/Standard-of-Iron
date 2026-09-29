#pragma once

#include "core/component_combat.h"
#include "core/world.h"

namespace Game::Systems {

struct Mover {
  Engine::Core::World& world;
  Engine::Core::Entity& entity;
  Engine::Core::TransformComponent& transform;
  Engine::Core::MovementComponent& movement;
  Engine::Core::UnitComponent& unit;
  Engine::Core::MovementFactsComponent& facts;
  float delta_time;
  float previous_x;
  float previous_z;
};

} // namespace Game::Systems
