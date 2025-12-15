#pragma once

#include "../../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

void deal_damage(Engine::Core::World *world, Engine::Core::Entity *target,
                 int damage, Engine::Core::EntityID attacker_id = 0);

void apply_hit_feedback(Engine::Core::Entity *target,
                        Engine::Core::EntityID attacker_id,
                        Engine::Core::World *world);

} // namespace Game::Systems::Combat
