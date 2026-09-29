#pragma once

#include "army_formation_types.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Formation::Cohesion {

void refresh_shape_state(Engine::Core::World& world, ArmyFormation& formation);

void collect_stragglers(Engine::Core::World& world,
                        ArmyFormation& formation,
                        float elapsed);

[[nodiscard]] auto damage_taken_multiplier(const ArmyFormation& formation) -> float;

[[nodiscard]] auto move_speed_multiplier(const ArmyFormation& formation,
                                         const Engine::Core::Entity& entity) -> float;

} // namespace Game::Formation::Cohesion
