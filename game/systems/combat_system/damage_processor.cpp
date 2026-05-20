#include "damage_processor.h"

#include "damage_application.h"

namespace Game::Systems::Combat {

void deal_damage(Engine::Core::World* world,
                 Engine::Core::Entity* target,
                 int damage,
                 Engine::Core::EntityID attacker_id) {
  apply_unit_damage(world, target, damage, attacker_id);
}

} // namespace Game::Systems::Combat
