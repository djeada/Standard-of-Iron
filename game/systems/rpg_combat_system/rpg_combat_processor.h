#pragma once

#include "../../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::RpgCombat {

void refresh_commander_engagement(Engine::Core::World* world,
                                  Engine::Core::EntityID commander_id);

void tick_rpg_combat(Engine::Core::World* world,
                     Engine::Core::EntityID commander_id,
                     float dt);

} // namespace Game::Systems::RpgCombat
