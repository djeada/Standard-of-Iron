#include "combat_system.h"
#include "combat_system/attack_processor.h"
#include "combat_system/combat_state_processor.h"
#include "combat_system/hit_feedback_processor.h"

namespace Game::Systems {

void CombatSystem::update(Engine::Core::World *world, float delta_time) {
  Combat::process_hit_feedback(world, delta_time);
  Combat::process_combat_state(world, delta_time);
  Combat::process_attacks(world, delta_time);
  m_auto_engagement.process(world, delta_time);
}

} 
