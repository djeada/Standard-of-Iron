#pragma once

#include "../../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::RpgCombat {

void refresh_commander_engagement(Engine::Core::World* world,
                                  Engine::Core::EntityID commander_id);

auto ideal_engage_distance(const Engine::Core::Entity& attacker,
                           Engine::Core::Entity& commander) -> float;

void tick_rpg_combat(Engine::Core::World* world,
                     Engine::Core::EntityID commander_id,
                     float dt);

} // namespace Game::Systems::RpgCombat
