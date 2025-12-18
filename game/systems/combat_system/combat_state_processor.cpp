#include "combat_state_processor.h"
#include "../../core/component.h"
#include "../../core/world.h"

namespace Game::Systems::Combat {

void process_combat_state(Engine::Core::World *world, float delta_time) {
  auto units = world->get_entities_with<Engine::Core::CombatStateComponent>();

  for (auto *unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *combat_state =
        unit->get_component<Engine::Core::CombatStateComponent>();
    if (combat_state == nullptr) {
      continue;
    }

    if (combat_state->is_hit_paused) {
      combat_state->hit_pause_remaining -= delta_time;
      if (combat_state->hit_pause_remaining <= 0.0F) {
        combat_state->is_hit_paused = false;
        combat_state->hit_pause_remaining = 0.0F;
      }
      continue;
    }

    combat_state->state_time += delta_time;

    if (combat_state->state_time >= combat_state->state_duration) {
      using CS = Engine::Core::CombatAnimationState;
      using CSC = Engine::Core::CombatStateComponent;

      switch (combat_state->animation_state) {
      case CS::Advance:
        combat_state->animation_state = CS::WindUp;
        combat_state->state_duration = CSC::kWindUpDuration;
        break;
      case CS::WindUp:
        combat_state->animation_state = CS::Strike;
        combat_state->state_duration = CSC::kStrikeDuration;
        break;
      case CS::Strike:
        combat_state->animation_state = CS::Impact;
        combat_state->state_duration = CSC::kImpactDuration;
        break;
      case CS::Impact:
        combat_state->animation_state = CS::Recover;
        combat_state->state_duration = CSC::kRecoverDuration;
        break;
      case CS::Recover:
        combat_state->animation_state = CS::Reposition;
        combat_state->state_duration = CSC::kRepositionDuration;
        break;
      case CS::Reposition:
      case CS::Idle:
      default:
        combat_state->animation_state = CS::Idle;
        combat_state->state_duration = 0.0F;
        break;
      }
      combat_state->state_time = 0.0F;
    }
  }
}

} // namespace Game::Systems::Combat
