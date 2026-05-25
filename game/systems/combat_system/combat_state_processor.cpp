#include "combat_state_processor.h"

#include "../../../render/profiling/frame_profile.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../rpg_combat_system/rpg_commander_damage.h"

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

  // Amplified durations: larger wind-up for readability, faster strike for impact
  switch (state) {
  case CS::Advance:
    return combat_state.finisher_attack ? 1.65F : 1.30F;
  case CS::WindUp:
    return combat_state.finisher_attack ? 1.60F : 1.35F;
  case CS::Strike:
    return combat_state.finisher_attack ? 1.15F : 1.0F;
  case CS::Impact:
    return combat_state.finisher_attack ? 1.30F : 1.12F;
  case CS::Recover:
    return combat_state.finisher_attack ? 1.20F : 1.05F;
  case CS::Reposition:
    return combat_state.finisher_attack ? 1.10F : 0.90F;
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
  auto& profile = Render::Profiling::global_profile();
  Render::Profiling::AccumulatorScope const scope(&profile.combat_state_update_us);
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
        // Deferred hit window: deal damage when blade connects
        if (!combat_state->damage_dealt_this_swing) {
          auto const* commander =
              unit->get_component<Engine::Core::CommanderComponent>();
          if (commander != nullptr && commander->fpv_controlled) {
            auto const* action =
                unit->get_component<Engine::Core::RpgCommanderActionComponent>();
            auto const* attack =
                unit->get_component<Engine::Core::AttackComponent>();
            if (action != nullptr && action->active_target_id != 0) {
              auto* target = world->get_entity(action->active_target_id);
              if (target != nullptr) {
                int damage = 10;
                if (attack != nullptr) {
                  damage = std::max(1, attack->get_current_damage());
                }
                // Low stamina penalty
                auto const* stamina =
                    unit->get_component<Engine::Core::StaminaComponent>();
                if (stamina != nullptr &&
                    stamina->stamina < CSC::k_low_stamina_threshold) {
                  damage = static_cast<int>(
                      static_cast<float>(damage) * CSC::k_low_stamina_damage_penalty);
                  damage = std::max(1, damage);
                }
                Game::Systems::RpgCombat::deal_commander_attack_damage(
                    world, target, damage, unit->get_id());
                combat_state->damage_dealt_this_swing = true;
              }
            }
          }
        }
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
        // Input buffering: skip Reposition and go directly to Advance for next attack
        if (combat_state->input_buffered) {
          combat_state->animation_state = CS::Advance;
          combat_state->state_duration = phase_duration_for_state(
              *unit, *combat_state, combat_state->animation_state);
          combat_state->input_buffered = false;
          combat_state->damage_dealt_this_swing = false;
        } else {
          combat_state->animation_state = CS::Reposition;
          combat_state->state_duration = phase_duration_for_state(
              *unit, *combat_state, combat_state->animation_state);
        }
        break;
      case CS::Reposition:
      case CS::Idle:
      default:
        combat_state->animation_state = CS::Idle;
        combat_state->state_duration = 0.0F;
        combat_state->input_buffered = false;
        break;
      }
      combat_state->state_time = 0.0F;
    }
  }
}

} // namespace Game::Systems::Combat
