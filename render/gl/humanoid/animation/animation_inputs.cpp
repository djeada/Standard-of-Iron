#include "animation_inputs.h"

#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/core/world.h"
#include "../../../entity/registry.h"
#include <algorithm>
#include <cmath>

namespace Render::GL {

auto sampleAnimState(const DrawContext &ctx) -> AnimationInputs {
  AnimationInputs anim{};
  anim.time = ctx.animation_time;
  anim.is_moving = false;
  anim.is_attacking = false;
  anim.is_melee = false;
  anim.is_in_hold_mode = false;
  anim.is_exiting_hold = false;
  anim.hold_exit_progress = 0.0F;

  if (ctx.entity == nullptr) {
    return anim;
  }

  if (ctx.entity->hasComponent<Engine::Core::PendingRemovalComponent>()) {
    return anim;
  }

  auto *movement = ctx.entity->getComponent<Engine::Core::MovementComponent>();
  auto *attack = ctx.entity->getComponent<Engine::Core::AttackComponent>();
  auto *attack_target =
      ctx.entity->getComponent<Engine::Core::AttackTargetComponent>();
  auto *transform =
      ctx.entity->getComponent<Engine::Core::TransformComponent>();
  auto *hold_mode = ctx.entity->getComponent<Engine::Core::HoldModeComponent>();

  anim.is_in_hold_mode = ((hold_mode != nullptr) && hold_mode->active);
  if ((hold_mode != nullptr) && !hold_mode->active &&
      hold_mode->exitCooldown > 0.0F) {
    anim.is_exiting_hold = true;
    anim.hold_exit_progress =
        1.0F - (hold_mode->exitCooldown / hold_mode->standUpDuration);
  }
  anim.is_moving = ((movement != nullptr) && movement->hasTarget);

  if ((attack != nullptr) && (attack_target != nullptr) &&
      attack_target->target_id > 0 && (transform != nullptr)) {
    anim.is_melee = (attack->currentMode ==
                     Engine::Core::AttackComponent::CombatMode::Melee);

    bool const stationary = !anim.is_moving;
    float const current_cooldown =
        anim.is_melee ? attack->meleeCooldown : attack->cooldown;
    bool const recently_fired =
        attack->timeSinceLast < std::min(current_cooldown, 0.45F);
    bool target_in_range = false;

    if (ctx.world != nullptr) {
      auto *target = ctx.world->getEntity(attack_target->target_id);
      if (target != nullptr) {
        auto *target_transform =
            target->getComponent<Engine::Core::TransformComponent>();
        if (target_transform != nullptr) {
          float const dx = target_transform->position.x - transform->position.x;
          float const dz = target_transform->position.z - transform->position.z;
          float const dist_squared = dx * dx + dz * dz;
          float target_radius = 0.0F;
          if (target->hasComponent<Engine::Core::BuildingComponent>()) {
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
