#include "combat_system.h"
#include "../core/world.h"
#include "combat_system/attack_processor.h"
#include "combat_system/combat_state_processor.h"
#include "combat_system/combat_utils.h"
#include "combat_system/hit_feedback_processor.h"

namespace Game::Systems {

void CombatSystem::update(Engine::Core::World *world, float delta_time) {
  auto const query_context = Combat::build_combat_query_context(world);
  Combat::process_hit_feedback(world, delta_time);
  Combat::process_combat_state(world, delta_time);
  Combat::process_attacks(world, query_context, delta_time);
  m_auto_engagement.process(world, query_context, delta_time);
}

} // namespace Game::Systems
