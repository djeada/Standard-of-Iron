#include "animation_inputs.h"

#include <algorithm>
#include <cmath>

#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/core/world.h"
#include "../../../../game/systems/combat_rules.h"
#include "../../../creature/animation_state_components.h"
#include "../../../entity/registry.h"

namespace Render::GL {

namespace {

constexpr float k_builder_construct_cycles_per_second = 1.75F;

auto map_combat_state_to_phase(Engine::Core::CombatAnimationState state)
    -> CombatAnimPhase {
  switch (state) {
  case Engine::Core::CombatAnimationState::Advance:
    return CombatAnimPhase::Advance;
  case Engine::Core::CombatAnimationState::WindUp:
    return CombatAnimPhase::WindUp;
  case Engine::Core::CombatAnimationState::Strike:
    return CombatAnimPhase::Strike;
  case Engine::Core::CombatAnimationState::Impact:
    return CombatAnimPhase::Impact;
  case Engine::Core::CombatAnimationState::Recover:
    return CombatAnimPhase::Recover;
  case Engine::Core::CombatAnimationState::Reposition:
    return CombatAnimPhase::Reposition;
  case Engine::Core::CombatAnimationState::Idle:
  default:
    return CombatAnimPhase::Idle;
  }
}

} // namespace

auto sample_anim_state(const DrawContext& ctx) -> AnimationInputs {
  if (ctx.animation_override != nullptr) {
    return *ctx.animation_override;
  }
  AnimationInputs anim{};
  anim.time = ctx.animation_time;
  anim.is_moving = false;
  anim.is_running = false;
  anim.is_attacking = false;
  anim.is_melee = false;
  anim.is_in_hold_mode = false;
  anim.is_exiting_hold = false;
  anim.hold_exit_progress = 0.0F;
  anim.hold_entry_progress = 0.0F;
  anim.combat_phase = CombatAnimPhase::Idle;
  anim.combat_phase_progress = 0.0F;
  anim.attack_family = Engine::Core::CombatAttackFamily::None;
  anim.attack_variant = 0;
  anim.attack_offset = 0.0F;
  anim.has_attack_offset = false;
  anim.is_hit_reacting = false;
  anim.hit_reaction_intensity = 0.0F;
  anim.is_dying = false;
  anim.is_dead = false;
  anim.death_progress = 0.0F;
  anim.death_variant = 0;

  if (ctx.entity == nullptr) {
    return anim;
  }

  auto* death_anim = ctx.entity->get_component<Engine::Core::DeathAnimationComponent>();
  bool const has_active_death = (death_anim != nullptr);
  if (ctx.entity->has_component<Engine::Core::PendingRemovalComponent>() &&
      !has_active_death) {
    return anim;
  }

  auto* movement = ctx.entity->get_component<Engine::Core::MovementComponent>();
  auto* attack = ctx.entity->get_component<Engine::Core::AttackComponent>();
  auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
  auto* attack_target =
      ctx.entity->get_component<Engine::Core::AttackTargetComponent>();
  auto* transform = ctx.entity->get_component<Engine::Core::TransformComponent>();
  auto* hold_mode = ctx.entity->get_component<Engine::Core::HoldModeComponent>();
  auto* combat_state = ctx.entity->get_component<Engine::Core::CombatStateComponent>();
  auto* hit_feedback = ctx.entity->get_component<Engine::Core::HitFeedbackComponent>();
  const auto* stamina = ctx.entity->get_component<Engine::Core::StaminaComponent>();

  if (death_anim != nullptr) {
    anim.is_moving = false;
    anim.is_running = false;
    anim.is_attacking = false;
    anim.is_melee = false;
    anim.is_hit_reacting = false;
    anim.hit_reaction_intensity = 0.0F;
    anim.death_variant = death_anim->sequence_variant;
    if (death_anim->state == Engine::Core::DeathSequenceState::Dying) {
      anim.is_dying = true;
      if (death_anim->state_duration > 0.0F) {
        anim.death_progress =
            std::clamp(death_anim->state_time / death_anim->state_duration, 0.0F, 1.0F);
      } else {
        anim.death_progress = 1.0F;
      }
    } else {
      anim.is_dead = true;
      anim.death_progress = 1.0F;
    }
    return anim;
  }

  anim.is_in_hold_mode = ((hold_mode != nullptr) && hold_mode->active);
  if (anim.is_in_hold_mode && hold_mode != nullptr) {
    anim.hold_entry_progress = hold_mode->kneel_entry_progress;
  }
  if ((hold_mode != nullptr) && !hold_mode->active && hold_mode->exit_cooldown > 0.0F) {
    anim.is_exiting_hold = true;
    anim.hold_exit_progress =
        1.0F - (hold_mode->exit_cooldown / hold_mode->stand_up_duration);
  }
  anim.is_moving = ((movement != nullptr) && movement->has_target);
  anim.is_running = (stamina != nullptr) && stamina->is_running;

  auto* healer = ctx.entity->get_component<Engine::Core::HealerComponent>();
  if (healer != nullptr && healer->is_healing_active && transform != nullptr) {
    anim.is_healing = true;
    anim.healing_target_dx = healer->healing_target_x - transform->position.x;
    anim.healing_target_dz = healer->healing_target_z - transform->position.z;
  }

  auto* builder_prod =
      ctx.entity->get_component<Engine::Core::BuilderProductionComponent>();
  if (builder_prod != nullptr) {
    if (builder_prod->bypass_movement_active) {
      anim.is_moving = true;
    }
    if (builder_prod->in_progress) {
      anim.is_constructing = true;
      float const build_elapsed =
          std::max(0.0F, builder_prod->build_time - builder_prod->time_remaining);
      anim.construction_progress =
          std::fmod(build_elapsed * k_builder_construct_cycles_per_second, 1.0F);
      if (anim.construction_progress < 0.0F) {
        anim.construction_progress += 1.0F;
      }
    }
  }

  if (combat_state != nullptr) {
    anim.combat_phase = map_combat_state_to_phase(combat_state->animation_state);
    if (combat_state->state_duration > 0.0F) {
      anim.combat_phase_progress =
          combat_state->state_time / combat_state->state_duration;
    }
    anim.attack_variant = combat_state->attack_variant;
    anim.attack_offset = combat_state->attack_offset;
    anim.has_attack_offset = true;
    anim.attack_family = combat_state->attack_family;
    if (combat_state->animation_state != Engine::Core::CombatAnimationState::Idle) {
      anim.is_attacking = true;
      anim.is_melee =
          (anim.attack_family == Engine::Core::CombatAttackFamily::None)
              ? ((attack != nullptr) &&
                 (attack->current_mode ==
                  Engine::Core::AttackComponent::CombatMode::Melee))
              : (anim.attack_family != Engine::Core::CombatAttackFamily::Bow);
    }
  }

  if (!anim.is_attacking && (attack != nullptr) && attack->in_melee_lock &&
      Game::Systems::CombatRules::participates_in_rts_melee_lock(ctx.entity)) {
    anim.is_attacking = true;
    anim.is_melee = true;
    if (anim.attack_family == Engine::Core::CombatAttackFamily::None &&
        unit != nullptr) {
      anim.attack_family = Engine::Core::resolve_combat_attack_family(
          unit->spawn_type, Engine::Core::AttackComponent::CombatMode::Melee);
    }
  }

  if (hit_feedback != nullptr && hit_feedback->is_reacting) {
    anim.is_hit_reacting = true;
    float const progress = hit_feedback->reaction_time /
                           Engine::Core::HitFeedbackComponent::k_reaction_duration;
    anim.hit_reaction_intensity =
        hit_feedback->reaction_intensity * std::max(0.0F, 1.0F - progress);
  }

  if ((combat_state == nullptr) && (attack != nullptr) && (attack_target != nullptr) &&
      attack_target->target_id > 0 && (transform != nullptr)) {
    if (unit != nullptr) {
      anim.attack_family = Engine::Core::resolve_combat_attack_family(
          unit->spawn_type, attack->current_mode);
    }
    anim.is_melee =
        (attack->current_mode == Engine::Core::AttackComponent::CombatMode::Melee);

    bool const stationary = !anim.is_moving;
    float const current_cooldown =
        anim.is_melee ? attack->melee_cooldown : attack->cooldown;
    bool const recently_fired =
        attack->time_since_last < std::min(current_cooldown, 0.45F);
    bool target_in_range = false;

    if (ctx.world != nullptr) {
      auto* target = ctx.world->get_entity(attack_target->target_id);
      if (target != nullptr) {
        auto* target_transform =
            target->get_component<Engine::Core::TransformComponent>();
        if (target_transform != nullptr) {
          float const dx = target_transform->position.x - transform->position.x;
          float const dz = target_transform->position.z - transform->position.z;
          float const dist_squared = dx * dx + dz * dz;
          float target_radius = 0.0F;
          if (target->has_component<Engine::Core::BuildingComponent>()) {
            target_radius =
                std::max(target_transform->scale.x, target_transform->scale.z) * 0.5F;
          } else {
            target_radius =
                std::max(target_transform->scale.x, target_transform->scale.z) * 0.5F;
          }
          auto* elephant = target->get_component<Engine::Core::ElephantComponent>();
          if (elephant != nullptr) {
            target_radius = std::max(target_radius, elephant->trample_radius);
          }
          float const effective_range = attack->range + target_radius + 0.25F;
          target_in_range = (dist_squared <= effective_range * effective_range);
        }
      }
    }

    anim.is_attacking = stationary && (target_in_range || recently_fired);
  }

  if (anim.is_attacking || anim.is_hit_reacting) {
    anim.is_constructing = false;
  }

  bool const is_active = anim.is_moving || anim.is_attacking || anim.is_constructing ||
                         anim.is_healing || anim.is_hit_reacting || anim.is_dying ||
                         anim.is_dead || anim.is_in_hold_mode || anim.is_exiting_hold;

  auto* humanoid_state = Engine::Core::get_or_add_component<
      Render::Creature::HumanoidAnimationStateComponent>(ctx.entity);
  if (humanoid_state == nullptr) {
    anim.idle_duration = 0.0F;
    return anim;
  }

  float delta_time = 0.0F;
  if (humanoid_state->initialized) {
    delta_time = std::max(0.0F, anim.time - humanoid_state->last_sample_time);
    if (anim.time < humanoid_state->last_sample_time) {
      humanoid_state->idle_duration = 0.0F;
    }
  } else {
    humanoid_state->initialized = true;
    humanoid_state->idle_duration = 0.0F;
  }

  humanoid_state->last_sample_time = anim.time;
  if (is_active) {
    humanoid_state->idle_duration = 0.0F;
  } else {
    humanoid_state->idle_duration += delta_time;
  }
  anim.idle_duration = humanoid_state->idle_duration;

  return anim;
}

} // namespace Render::GL
