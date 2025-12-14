#include "animation_inputs.h"

#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/core/world.h"
#include "../../../entity/registry.h"
#include <algorithm>
#include <cmath>

namespace Render::GL {

namespace {

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

auto sample_anim_state(const DrawContext &ctx) -> AnimationInputs {
  AnimationInputs anim{};
  anim.time = ctx.animation_time;
  anim.is_moving = false;
  anim.is_attacking = false;
  anim.is_melee = false;
  anim.is_in_hold_mode = false;
  anim.is_exiting_hold = false;
  anim.hold_exit_progress = 0.0F;
  anim.combat_phase = CombatAnimPhase::Idle;
  anim.combat_phase_progress = 0.0F;
  anim.attack_variant = 0;
  anim.is_hit_reacting = false;
  anim.hit_reaction_intensity = 0.0F;

  if (ctx.entity == nullptr) {
    return anim;
  }

  if (ctx.entity->has_component<Engine::Core::PendingRemovalComponent>()) {
    return anim;
  }

  auto *movement = ctx.entity->get_component<Engine::Core::MovementComponent>();
  auto *attack = ctx.entity->get_component<Engine::Core::AttackComponent>();
  auto *attack_target =
      ctx.entity->get_component<Engine::Core::AttackTargetComponent>();
  auto *transform =
      ctx.entity->get_component<Engine::Core::TransformComponent>();
  auto *hold_mode =
      ctx.entity->get_component<Engine::Core::HoldModeComponent>();
  auto *combat_state =
      ctx.entity->get_component<Engine::Core::CombatStateComponent>();
  auto *hit_feedback =
      ctx.entity->get_component<Engine::Core::HitFeedbackComponent>();

  anim.is_in_hold_mode = ((hold_mode != nullptr) && hold_mode->active);
  if ((hold_mode != nullptr) && !hold_mode->active &&
      hold_mode->exit_cooldown > 0.0F) {
    anim.is_exiting_hold = true;
    anim.hold_exit_progress =
        1.0F - (hold_mode->exit_cooldown / hold_mode->stand_up_duration);
  }
  anim.is_moving = ((movement != nullptr) && movement->has_target);

  auto *healer = ctx.entity->get_component<Engine::Core::HealerComponent>();
  if (healer != nullptr && healer->is_healing_active && transform != nullptr) {
    anim.is_healing = true;
    anim.healing_target_dx = healer->healing_target_x - transform->position.x;
    anim.healing_target_dz = healer->healing_target_z - transform->position.z;
  }

  if (combat_state != nullptr) {
    anim.combat_phase =
        map_combat_state_to_phase(combat_state->animation_state);
    if (combat_state->state_duration > 0.0F) {
      anim.combat_phase_progress =
          combat_state->state_time / combat_state->state_duration;
    }
    anim.attack_variant = combat_state->attack_variant;
  }

  if (hit_feedback != nullptr && hit_feedback->is_reacting) {
    anim.is_hit_reacting = true;
    float const progress =
        hit_feedback->reaction_time /
        Engine::Core::HitFeedbackComponent::kReactionDuration;
    anim.hit_reaction_intensity =
        hit_feedback->reaction_intensity * std::max(0.0F, 1.0F - progress);
  }

  if ((attack != nullptr) && (attack_target != nullptr) &&
      attack_target->target_id > 0 && (transform != nullptr)) {
    anim.is_melee = (attack->current_mode ==
                     Engine::Core::AttackComponent::CombatMode::Melee);

    bool const stationary = !anim.is_moving;
    float const current_cooldown =
        anim.is_melee ? attack->melee_cooldown : attack->cooldown;
    bool const recently_fired =
        attack->time_since_last < std::min(current_cooldown, 0.45F);
    bool target_in_range = false;

    if (ctx.world != nullptr) {
      auto *target = ctx.world->get_entity(attack_target->target_id);
      if (target != nullptr) {
        auto *target_transform =
            target->get_component<Engine::Core::TransformComponent>();
        if (target_transform != nullptr) {
          float const dx = target_transform->position.x - transform->position.x;
          float const dz = target_transform->position.z - transform->position.z;
          float const dist_squared = dx * dx + dz * dz;
          float target_radius = 0.0F;
          if (target->has_component<Engine::Core::BuildingComponent>()) {
            target_radius =
                std::max(target_transform->scale.x, target_transform->scale.z) *
                0.5F;
          } else {
            target_radius =
                std::max(target_transform->scale.x, target_transform->scale.z) *
                0.5F;
          }
          float const effective_range = attack->range + target_radius + 0.25F;
          target_in_range = (dist_squared <= effective_range * effective_range);
        }
      }
    }

    anim.is_attacking = stationary && (target_in_range || recently_fired);
  }

  return anim;
}

} // namespace Render::GL
