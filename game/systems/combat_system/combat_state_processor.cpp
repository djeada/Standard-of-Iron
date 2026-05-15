#include "combat_state_processor.h"

#include "../../core/component.h"
#include "../../core/world.h"

namespace Game::Systems::Combat {

namespace {

using CS = Engine::Core::CombatAnimationState;
using CSC = Engine::Core::CombatStateComponent;

auto base_phase_duration(CS state) noexcept -> float {
  switch (state) {
  case CS::Advance:
    return CSC::k_advance_duration;
  case CS::WindUp:
    return CSC::k_wind_up_duration;
  case CS::Strike:
    return CSC::k_strike_duration;
  case CS::Impact:
    return CSC::k_impact_duration;
  case CS::Recover:
    return CSC::k_recover_duration;
  case CS::Reposition:
    return CSC::k_reposition_duration;
  case CS::Idle:
  default:
    return 0.0F;
  }
}

auto commander_phase_scale(const Engine::Core::Entity& unit,
                           const Engine::Core::CombatStateComponent& combat_state,
                           CS state) noexcept -> float {
  auto const* commander = unit.get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || !commander->fpv_controlled) {
    return 1.0F;
  }

  bool const finisher_attack =
      combat_state.attack_variant >= CSC::k_attack_variant_seed_slots - 1U;
  switch (state) {
  case CS::Advance:
    return finisher_attack ? 1.55F : 1.22F;
  case CS::WindUp:
    return finisher_attack ? 1.42F : 1.18F;
  case CS::Strike:
    return finisher_attack ? 1.26F : 1.10F;
  case CS::Impact:
    return finisher_attack ? 1.22F : 1.08F;
  case CS::Recover:
    return finisher_attack ? 1.38F : 1.18F;
  case CS::Reposition:
    return finisher_attack ? 1.18F : 1.06F;
  case CS::Idle:
  default:
    return 1.0F;
  }
}

auto phase_duration_for_state(const Engine::Core::Entity& unit,
                              const Engine::Core::CombatStateComponent& combat_state,
                              CS state) noexcept -> float {
  return base_phase_duration(state) * commander_phase_scale(unit, combat_state, state);
}

} // namespace

void process_combat_state(Engine::Core::World* world, float delta_time) {
  auto units = world->get_entities_with<Engine::Core::CombatStateComponent>();

  for (auto* unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* combat_state = unit->get_component<Engine::Core::CombatStateComponent>();
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
      switch (combat_state->animation_state) {
      case CS::Advance:
        combat_state->animation_state = CS::WindUp;
        combat_state->state_duration = phase_duration_for_state(
            *unit, *combat_state, combat_state->animation_state);
        break;
      case CS::WindUp:
        combat_state->animation_state = CS::Strike;
        combat_state->state_duration = phase_duration_for_state(
            *unit, *combat_state, combat_state->animation_state);
        break;
      case CS::Strike:
        combat_state->animation_state = CS::Impact;
        combat_state->state_duration = phase_duration_for_state(
            *unit, *combat_state, combat_state->animation_state);
        break;
      case CS::Impact:
        combat_state->animation_state = CS::Recover;
        combat_state->state_duration = phase_duration_for_state(
            *unit, *combat_state, combat_state->animation_state);
        break;
      case CS::Recover:
        combat_state->animation_state = CS::Reposition;
        combat_state->state_duration = phase_duration_for_state(
            *unit, *combat_state, combat_state->animation_state);
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
