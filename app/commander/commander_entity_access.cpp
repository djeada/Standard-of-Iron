#include "app/commander/commander_entity_access.h"

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_actions/melee_intent_solver.h"

namespace App::Core {

auto controlled_commander(Engine::Core::World& world,
                          Engine::Core::EntityID commander_id,
                          int local_owner_id) -> Engine::Core::Entity* {
  auto* entity = world.get_entity(commander_id);
  if (entity == nullptr) {
    return nullptr;
  }

  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
      unit->owner_id != local_owner_id ||
      entity->get_component<Engine::Core::CommanderComponent>() == nullptr) {
    return nullptr;
  }
  return entity;
}

auto body_allows_now(const Engine::Core::Entity& commander)
    -> Game::Systems::CombatActions::MeleeInterruption {
  auto const* action =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr || !action->action_running || action->combat_action_id == 0U) {
    return {};
  }
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action->combat_action_id));
  if (definition == nullptr) {
    return {};
  }
  return Game::Systems::CombatActions::melee_interruption_at(
      *definition, action->normalized_action_time);
}

void cancel_current_attack(Engine::Core::Entity& commander) {
  auto* action = commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action != nullptr && action->action_running) {
    action->action_running = false;
    action->action_active = false;
    action->weapon_trace_active = false;
    action->action_completed = true;
    action->phase = Engine::Core::RpgCommanderActionPhase::None;
  }
  if (auto* combat = commander.get_component<Engine::Core::CombatStateComponent>()) {
    combat->animation_state = Engine::Core::CombatAnimationState::Idle;
    combat->state_time = 0.0F;
    combat->state_duration = 0.0F;
    combat->is_hit_paused = false;
    combat->hit_pause_remaining = 0.0F;
  }
}

} // namespace App::Core
