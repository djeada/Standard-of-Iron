#include "combat_status_effect_system.h"

#include "combat_system/combat_status_effect_processor.h"

namespace Game::Systems {

void CombatStatusEffectSystem::update(Engine::Core::World* world, float delta_time) {
  (void)Game::Systems::Combat::process_combat_status_effects(world, delta_time);
}

} // namespace Game::Systems
