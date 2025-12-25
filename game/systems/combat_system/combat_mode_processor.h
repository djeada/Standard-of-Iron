#pragma once

#include "../../core/entity.h"

namespace Engine::Core {
class World;
class AttackComponent;
} 

namespace Game::Systems::Combat {

void update_combat_mode(Engine::Core::Entity *attacker,
                        Engine::Core::World *world,
                        Engine::Core::AttackComponent *attack_comp);

}
