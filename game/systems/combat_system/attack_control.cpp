#include "attack_control.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../combat_actions/combat_action_definition.h"
#include "../combat_rules.h"
#include "../movement/order_service.h"
#include "combat_random.h"
#include "combat_types.h"

namespace Game::Systems::Combat {

namespace {

auto commander_attack_advance_scale(const Engine::Core::Entity* attacker) noexcept
    -> float {
  if (attacker == nullptr) {
    return 1.0F;
  }
  auto const* commander = attacker->get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || !commander->fpv_controlled) {
    return 1.0F;
  }
  return commander->combo_step >= 3 ? 1.55F : 1.22F;
}

} // namespace

auto deterministic_attack_delay(Engine::Core::EntityID attacker_id,
                                Engine::Core::EntityID target_id,
                                float cooldown) -> float {
  if (cooldown <= 0.05F) {
    return 0.0F;
  }
  float const max_delay = std::clamp(cooldown * 0.22F, 0.05F, 0.28F);
  std::uint32_t const seed = static_cast<std::uint32_t>(attacker_id * 2246822519U) ^
                             static_cast<std::uint32_t>(target_id * 3266489917U) ^
                             0x9E3779B9U;
  return hash_to_unit(seed) * max_delay;
}

void begin_attack_animation(Engine::Core::Entity* attacker, bool preserve_seed) {
  if (attacker == nullptr) {
    return;
  }

  auto* combat_state = attacker->get_component<Engine::Core::CombatStateComponent>();
  auto* unit = attacker->get_component<Engine::Core::UnitComponent>();
  auto* attack = attacker->get_component<Engine::Core::AttackComponent>();
  bool const had_combat_state = (combat_state != nullptr);
  if (combat_state == nullptr) {
    combat_state = attacker->add_component<Engine::Core::CombatStateComponent>();
  }
  if (combat_state != nullptr &&
      combat_state->animation_state == Engine::Core::CombatAnimationState::Idle) {
    combat_state->animation_state = Engine::Core::CombatAnimationState::Advance;
    combat_state->state_time = 0.0F;

    combat_state->state_duration =
        Engine::Core::CombatStateComponent::k_advance_duration *
        commander_attack_advance_scale(attacker);
    if (unit != nullptr && attack != nullptr) {
      combat_state->attack_family = Engine::Core::resolve_combat_attack_family(
          unit->spawn_type, attack->current_mode);
    } else {
      combat_state->attack_family = Engine::Core::CombatAttackFamily::None;
    }
    combat_state->finisher_attack = false;
    if (!preserve_seed || !had_combat_state) {
      auto* attack_target =
          attacker->get_component<Engine::Core::AttackTargetComponent>();
      std::uint32_t const target_id =
          attack_target != nullptr
              ? static_cast<std::uint32_t>(attack_target->target_id)
              : 0U;
      std::uint32_t const seed =
          static_cast<std::uint32_t>(attacker->get_id() * 2246822519U) ^
          (target_id * 3266489917U);
      combat_state->attack_offset = deterministic_range(seed, 1U, 0.0F, 0.15F);
      constexpr int k_variant_slots =
          Engine::Core::CombatStateComponent::k_attack_variant_seed_slots;
      combat_state->attack_variant = static_cast<std::uint8_t>(
          std::min(k_variant_slots - 1,
                   static_cast<int>(deterministic_unit_roll(seed, 2U) *
                                    static_cast<float>(k_variant_slots))));

      constexpr float k_swing_arc_radians = 1.25F;
      float const arc =
          (deterministic_unit_roll(seed, 3U) - 0.5F) * k_swing_arc_radians;
      bool const thrusting =
          combat_state->attack_family == Engine::Core::CombatAttackFamily::Spear;
      float const centre = thrusting ? Animation::k_melee_thrust_angle
                                     : Animation::k_melee_left_cut_angle;
      float const reach =
          attack != nullptr ? attack->melee_range : Engine::Core::k_melee_default_reach;
      combat_state->intent = Engine::Core::melee_intent_from_strike_angle(
          centre + arc, thrusting ? 1.0F : 0.0F, reach);
    }
  }
}

void stop_unit_movement(Engine::Core::Entity* unit,
                        Engine::Core::TransformComponent* transform) {
  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  if ((movement != nullptr) && movement->get_has_target()) {
    movement->stop();
    OrderService::clear_player_order_intent(unit);
    if (transform != nullptr) {
      movement->set_rest_position(transform->position.x, transform->position.z);
    }
  }
}

void drop_attack_target(Engine::Core::World* world, Engine::Core::Entity* attacker) {
  if (attacker == nullptr) {
    return;
  }
  attacker->remove_component<Engine::Core::AttackTargetComponent>();
  auto* movement = world->try_get<Engine::Core::MovementComponent>(attacker->get_id());
  if (movement == nullptr || !movement->get_has_target() ||
      !movement->get_issuer_retargets() || !movement->get_precise_arrival()) {
    return;
  }
  stop_unit_movement(
      attacker, world->try_get<Engine::Core::TransformComponent>(attacker->get_id()));
}

void clear_orphaned_rts_attack_presentation(Engine::Core::Entity* attacker) {
  if (attacker == nullptr ||
      !Game::Systems::CombatRules::participates_in_rts_melee_lock(attacker)) {
    return;
  }

  if (auto* combat = attacker->get_component<Engine::Core::CombatStateComponent>()) {
    combat->animation_state = Engine::Core::CombatAnimationState::Idle;
    combat->state_time = 0.0F;
    combat->state_duration = 0.0F;
    combat->damage_dealt_this_swing = false;
    combat->input_buffered = false;
  }

  auto* action = attacker->get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr) {
    return;
  }
  auto const action_id = static_cast<Game::Systems::CombatActions::CombatActionId>(
      action->combat_action_id);
  if (action_id != Game::Systems::CombatActions::CombatActionId::RtsSwordStrike &&
      action_id != Game::Systems::CombatActions::CombatActionId::RtsSpearThrust &&
      action_id != Game::Systems::CombatActions::CombatActionId::RtsBowShot &&
      action_id != Game::Systems::CombatActions::CombatActionId::RtsElephantStomp) {
    return;
  }
  action->combat_action_id = 0U;
  action->active_target_id = 0U;
  action->active_target_soldier_slot =
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;

  action->last_damage = 0;
  action->last_hit_target_id = 0U;
  action->last_hit_soldier_slot =
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  action->hit_target_ids.fill(0U);
  action->hit_target_soldier_slots.fill(
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot);
  action->hit_target_count = 0U;
  action->action_running = false;
  action->action_completed = false;
  action->action_active = false;
  action->weapon_trace_active = false;
  action->phase = Engine::Core::RpgCommanderActionPhase::None;
  action->normalized_action_time = 0.0F;
  action->previous_normalized_action_time = 0.0F;
}

void face_target(Engine::Core::TransformComponent* attacker_transform,
                 Engine::Core::TransformComponent* target_transform) {
  if ((attacker_transform == nullptr) || (target_transform == nullptr)) {
    return;
  }
  float const dx = target_transform->position.x - attacker_transform->position.x;
  float const dz = target_transform->position.z - attacker_transform->position.z;
  float const yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
  attacker_transform->desired_yaw = yaw;
  attacker_transform->has_desired_yaw = true;
}

} // namespace Game::Systems::Combat
