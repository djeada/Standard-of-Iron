#include "combat_state_processor.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

#include "../../../render/profiling/frame_profile.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../combat_actions/body_impact.h"
#include "../combat_actions/combat_action_definition.h"
#include "../combat_actions/combat_action_events.h"
#include "../combat_actions/projectile_release.h"
#include "../combat_actions/weapon_trace.h"
#include "../rpg_combat_system/rpg_commander_damage.h"
#include "combat_action_processor.h"
#include "combat_hit_resolver.h"
#include "combat_utils.h"
#include "mounted_charge_processor.h"
#include "spear_brace_processor.h"

namespace Game::Systems::Combat {

namespace {

using CS = Engine::Core::CombatAnimationState;
using CSC = Engine::Core::CombatStateComponent;

constexpr float k_commander_contact_cone_dot = 0.25F;

auto commander_melee_reach(const Engine::Core::Entity& commander) -> float {
  auto const* attack = commander.get_component<Engine::Core::AttackComponent>();
  float const base_reach = attack != nullptr
                               ? attack->melee_range
                               : Engine::Core::Defaults::k_attack_melee_range;
  return base_reach + Engine::Core::AttackComponent::k_melee_contact_range_grace;
}

auto target_in_swing_arc(Engine::Core::Entity& commander,
                         Engine::Core::Entity& target,
                         float reach) -> bool {
  if (!is_in_range(&commander, &target, reach)) {
    return false;
  }

  auto const* commander_transform =
      commander.get_component<Engine::Core::TransformComponent>();
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  if (commander_transform == nullptr || target_transform == nullptr) {
    return false;
  }

  float const yaw =
      commander_transform->rotation.y * (std::numbers::pi_v<float> / 180.0F);
  float const forward_x = std::sin(yaw);
  float const forward_z = std::cos(yaw);
  float const dx = target_transform->position.x - commander_transform->position.x;
  float const dz = target_transform->position.z - commander_transform->position.z;
  float const dist = std::sqrt(dx * dx + dz * dz);
  if (dist <= 0.0001F) {
    return true;
  }
  float const facing = (forward_x * dx + forward_z * dz) / dist;
  return facing >= k_commander_contact_cone_dot;
}

auto resolve_commander_contact_target(Engine::Core::World* world,
                                      Engine::Core::Entity& commander,
                                      Engine::Core::EntityID locked_target_id)
    -> Engine::Core::Entity* {
  auto const* commander_unit = commander.get_component<Engine::Core::UnitComponent>();
  if (commander_unit == nullptr) {
    return nullptr;
  }

  float const reach = commander_melee_reach(commander);

  if (locked_target_id != 0) {
    auto* locked = world->get_entity(locked_target_id);
    if (locked != nullptr && is_valid_enemy_unit(commander_unit, locked, true) &&
        target_in_swing_arc(commander, *locked, reach)) {
      return locked;
    }
  }

  Engine::Core::Entity* best = nullptr;
  float best_score = -1000000.0F;
  for (auto* candidate : world->get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == nullptr || candidate == &commander ||
        !is_valid_enemy_unit(commander_unit, candidate, false) ||
        !target_in_swing_arc(commander, *candidate, reach)) {
      continue;
    }
    auto const* candidate_transform =
        candidate->get_component<Engine::Core::TransformComponent>();
    auto const* commander_transform =
        commander.get_component<Engine::Core::TransformComponent>();
    if (candidate_transform == nullptr || commander_transform == nullptr) {
      continue;
    }
    float const dx = candidate_transform->position.x - commander_transform->position.x;
    float const dz = candidate_transform->position.z - commander_transform->position.z;
    float const score = -std::sqrt(dx * dx + dz * dz);
    if (score > best_score) {
      best_score = score;
      best = candidate;
    }
  }
  return best;
}

void deal_commander_contact_damage(Engine::Core::World* world,
                                   Engine::Core::Entity& commander,
                                   Engine::Core::CombatStateComponent& combat_state) {
  auto const* action =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  Engine::Core::EntityID const locked_target_id =
      action != nullptr ? action->active_target_id : 0;

  auto* target = resolve_commander_contact_target(world, commander, locked_target_id);
  if (target == nullptr) {
    return;
  }

  int damage = 10;
  if (auto const* attack = commander.get_component<Engine::Core::AttackComponent>()) {
    damage = std::max(1, attack->get_current_damage());
  }

  auto const* stamina = commander.get_component<Engine::Core::StaminaComponent>();
  if (stamina != nullptr && stamina->stamina < CSC::k_low_stamina_threshold) {
    damage = static_cast<int>(static_cast<float>(damage) *
                              CSC::k_low_stamina_damage_penalty);
    damage = std::max(1, damage);
  }

  Game::Systems::RpgCombat::deal_commander_attack_damage(
      world, target, damage, commander.get_id());
  combat_state.damage_dealt_this_swing = true;
}

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

  float direction_scale = 1.0F;
  switch (combat_state.attack_direction) {
  case Engine::Core::AttackDirection::Thrust:
    direction_scale = 0.80F;
    break;
  case Engine::Core::AttackDirection::Overhead:
    direction_scale = 1.15F;
    break;
  case Engine::Core::AttackDirection::HeavyOverhead:
    direction_scale = 1.30F;
    break;
  default:
    direction_scale = 1.0F;
    break;
  }

  switch (state) {
  case CS::Advance:
    return (combat_state.finisher_attack ? 1.90F : 1.50F) * direction_scale;
  case CS::WindUp:
    return (combat_state.finisher_attack ? 2.10F : 1.65F) * direction_scale;
  case CS::Strike:
    return (combat_state.finisher_attack ? 1.40F : 1.15F) * direction_scale;
  case CS::Impact:
    return (combat_state.finisher_attack ? 1.50F : 1.25F) * direction_scale;
  case CS::Recover:
    return (combat_state.finisher_attack ? 1.35F : 1.15F) * direction_scale;
  case CS::Reposition:
    return (combat_state.finisher_attack ? 1.20F : 1.0F) * direction_scale;
  case CS::Idle:
  default:
    return 1.0F;
  }
}

auto phase_duration_for_state(const Engine::Core::Entity& unit,
                              const Engine::Core::CombatStateComponent& combat_state,
                              CS state) noexcept -> float {
  auto const* commander = unit.get_component<Engine::Core::CommanderComponent>();
  if (commander != nullptr && commander->fpv_controlled) {
    return base_phase_duration(state) *
           commander_phase_scale(unit, combat_state, state);
  }

  return base_phase_duration(state) * combat_state.swing_duration_scale;
}

void reset_action_events_if_present(Engine::Core::Entity& unit) {
  auto* action = unit.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr) {
    return;
  }
  Game::Systems::CombatActions::reset_combat_action_event_runtime(*action);
}

} // namespace

void process_combat_state(Engine::Core::World* world, float delta_time) {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  auto& profile = Render::Profiling::global_profile();
  Render::Profiling::AccumulatorScope const scope(&profile.combat_state_update_us);
#endif
  process_spear_brace_state(world, delta_time);
  process_mounted_charge_intents(world, delta_time);
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

    int transitions = 0;
    while (combat_state->state_duration > 0.0F &&
           combat_state->state_time >= combat_state->state_duration &&
           transitions < 8) {
      ++transitions;
      float const carry = combat_state->state_time - combat_state->state_duration;
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

        if (!combat_state->damage_dealt_this_swing) {
          auto const* commander =
              unit->get_component<Engine::Core::CommanderComponent>();
          auto const* action =
              unit->get_component<Engine::Core::RpgCommanderActionComponent>();
          if (commander != nullptr && commander->fpv_controlled &&
              (action == nullptr || action->combat_action_id == 0U)) {
            deal_commander_contact_damage(world, *unit, *combat_state);
          }
        }
        break;
      case CS::Impact:
        combat_state->animation_state = CS::Recover;
        combat_state->state_duration = phase_duration_for_state(
            *unit, *combat_state, combat_state->animation_state);
        break;
      case CS::Recover:

        if (combat_state->input_buffered) {
          combat_state->animation_state = CS::Advance;
          combat_state->state_duration = phase_duration_for_state(
              *unit, *combat_state, combat_state->animation_state);
          combat_state->input_buffered = false;
          combat_state->damage_dealt_this_swing = false;
          reset_action_events_if_present(*unit);
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
      combat_state->state_time = carry;
    }
  }

  for (auto* unit :
       world->get_entities_with<Engine::Core::RpgCommanderActionComponent>()) {
    if (unit == nullptr ||
        unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto* presentation_state =
        unit->get_component<Engine::Core::CombatStateComponent>();
    if (presentation_state != nullptr && presentation_state->is_hit_paused) {
      continue;
    }
    process_authored_combat_action(world, *unit, presentation_state, delta_time);
  }
}

} // namespace Game::Systems::Combat
