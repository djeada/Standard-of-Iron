#include "damage_processor.h"
#include "../../core/component.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../building_collision_registry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

namespace Game::Systems::Combat {

namespace {

auto mix_hash(std::uint32_t value) -> std::uint32_t {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return value;
}

auto resolve_death_reaction(Engine::Core::Entity *target,
                            Engine::Core::Entity *attacker)
    -> Engine::Core::DeathReactionType {
  using Engine::Core::DeathReactionType;
  if (target == nullptr) {
    return DeathReactionType::Collapse;
  }

  if (attacker == nullptr) {
    return DeathReactionType::Collapse;
  }

  auto *attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr) {
    return DeathReactionType::Collapse;
  }

  if (attacker_unit->spawn_type == Game::Units::SpawnType::Elephant) {
    auto *elephant = attacker->get_component<Engine::Core::ElephantComponent>();
    // Mix both entity ids (with a simple shift+xor) so each pair yields a
    // deterministic but varied elephant impact reaction.
    std::uint32_t seed = target->get_id() ^ (attacker->get_id() << 1U);
    seed = mix_hash(seed);
    std::uint32_t const style = seed % 5U;

    bool const charging = (elephant != nullptr) &&
                          (elephant->charge_state ==
                               Engine::Core::ElephantComponent::ChargeState::
                                   Charging ||
                           elephant->charge_state ==
                               Engine::Core::ElephantComponent::ChargeState::
                                   Trampling);
    if (charging) {
      switch (style) {
      case 0U:
        return DeathReactionType::Thrown;
      case 1U:
        return DeathReactionType::SpinFall;
      case 2U:
        return DeathReactionType::Crushed;
      case 3U:
        return DeathReactionType::Knockback;
      default:
        return DeathReactionType::BackwardFall;
      }
    }
    return (style % 2U == 0U) ? DeathReactionType::Knockback
                              : DeathReactionType::BackwardFall;
  }

  auto *attack = attacker->get_component<Engine::Core::AttackComponent>();
  if ((attack != nullptr) &&
      attack->current_mode == Engine::Core::AttackComponent::CombatMode::Melee) {
    return DeathReactionType::BackwardFall;
  }

  return DeathReactionType::Collapse;
}

void apply_death_motion(Engine::Core::Entity *target,
                        Engine::Core::Entity *attacker) {
  if (target == nullptr) {
    return;
  }

  auto *death = target->get_component<Engine::Core::DeathMotionComponent>();
  if (death == nullptr) {
    death = target->add_component<Engine::Core::DeathMotionComponent>();
  }
  if (death == nullptr) {
    return;
  }

  death->reaction = resolve_death_reaction(target, attacker);
  death->elapsed_time = 0.0F;
  death->duration = 1.35F;
  death->impulse_x = 0.0F;
  death->impulse_z = 0.0F;
  death->angular_velocity = 0.0F;

  auto *target_transform = target->get_component<Engine::Core::TransformComponent>();
  auto *attacker_transform =
      (attacker != nullptr)
          ? attacker->get_component<Engine::Core::TransformComponent>()
          : nullptr;
  if (target_transform != nullptr && attacker_transform != nullptr) {
    float const dx =
        target_transform->position.x - attacker_transform->position.x;
    float const dz =
        target_transform->position.z - attacker_transform->position.z;
    float const dist = std::sqrt(dx * dx + dz * dz);
    if (dist > 0.0001F) {
      death->impulse_x = dx / dist;
      death->impulse_z = dz / dist;
    }
  }

  switch (death->reaction) {
  case Engine::Core::DeathReactionType::Thrown:
    death->duration = 1.8F;
    death->impulse_x *= 2.8F;
    death->impulse_z *= 2.8F;
    death->angular_velocity = 780.0F;
    break;
  case Engine::Core::DeathReactionType::SpinFall:
    death->duration = 1.65F;
    death->impulse_x *= 2.2F;
    death->impulse_z *= 2.2F;
    death->angular_velocity = 620.0F;
    break;
  case Engine::Core::DeathReactionType::Crushed:
    death->duration = 1.1F;
    death->impulse_x *= 0.5F;
    death->impulse_z *= 0.5F;
    death->angular_velocity = 140.0F;
    break;
  case Engine::Core::DeathReactionType::Knockback:
    death->duration = 1.45F;
    death->impulse_x *= 1.8F;
    death->impulse_z *= 1.8F;
    death->angular_velocity = 260.0F;
    break;
  case Engine::Core::DeathReactionType::BackwardFall:
    death->duration = 1.5F;
    death->impulse_x *= 1.35F;
    death->impulse_z *= 1.35F;
    death->angular_velocity = 180.0F;
    break;
  case Engine::Core::DeathReactionType::Collapse:
  default:
    death->duration = 1.35F;
    death->angular_velocity = 70.0F;
    break;
  }
}

} // namespace

void deal_damage(Engine::Core::World *world, Engine::Core::Entity *target,
                 int damage, Engine::Core::EntityID attacker_id) {
  auto *unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return;
  }

  int const prev_health = unit->health;
  int const new_health = std::max(0, prev_health - damage);
  bool const is_killing_blow = (prev_health > 0 && prev_health <= damage);

  unit->health = new_health;

  int attacker_owner_id = 0;
  std::optional<Game::Units::SpawnType> attacker_type_opt;
  if (attacker_id != 0 && (world != nullptr)) {
    auto *attacker = world->get_entity(attacker_id);
    if (attacker != nullptr) {
      auto *attacker_unit =
          attacker->get_component<Engine::Core::UnitComponent>();
      if (attacker_unit != nullptr) {
        attacker_owner_id = attacker_unit->owner_id;
        attacker_type_opt = attacker_unit->spawn_type;
      }
    }
  }

  Game::Units::SpawnType const attacker_type =
      attacker_type_opt.value_or(Game::Units::SpawnType::Knight);

  Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
      attacker_id, target->get_id(), damage, attacker_type, is_killing_blow));

  if (unit->health > 0) {
    apply_hit_feedback(target, attacker_id, world);
  }

  if (target->has_component<Engine::Core::BuildingComponent>() &&
      unit->health > 0) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::BuildingAttackedEvent(target->get_id(), unit->owner_id,
                                            unit->spawn_type, attacker_id,
                                            attacker_owner_id, damage));
  }

  if (unit->health <= 0) {
    auto *attacker = (attacker_id != 0 && world != nullptr)
                         ? world->get_entity(attacker_id)
                         : nullptr;
    int const killer_owner_id = attacker_owner_id;

    Engine::Core::EventManager::instance().publish(Engine::Core::UnitDiedEvent(
        target->get_id(), unit->owner_id, unit->spawn_type, attacker_id,
        killer_owner_id));

    auto *target_atk = target->get_component<Engine::Core::AttackComponent>();
    if ((target_atk != nullptr) && target_atk->in_melee_lock &&
        target_atk->melee_lock_target_id != 0) {
      if (world != nullptr) {
        auto *lock_partner =
            world->get_entity(target_atk->melee_lock_target_id);
        if ((lock_partner != nullptr) &&
            !lock_partner
                 ->has_component<Engine::Core::PendingRemovalComponent>()) {
          auto *partner_atk =
              lock_partner->get_component<Engine::Core::AttackComponent>();
          if ((partner_atk != nullptr) &&
              partner_atk->melee_lock_target_id == target->get_id()) {
            partner_atk->in_melee_lock = false;
            partner_atk->melee_lock_target_id = 0;
          }
        }
      }
    }

    if (target->has_component<Engine::Core::BuildingComponent>()) {
      BuildingCollisionRegistry::instance().unregister_building(
          target->get_id());
    }

    if (auto *movement =
            target->get_component<Engine::Core::MovementComponent>()) {
      movement->has_target = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->clear_path();
      movement->path_pending = false;
    }

    auto *attack = target->get_component<Engine::Core::AttackComponent>();
    if (attack != nullptr) {
      attack->in_melee_lock = false;
      attack->melee_lock_target_id = 0;
    }
    auto *target_selector =
        target->get_component<Engine::Core::AttackTargetComponent>();
    if (target_selector != nullptr) {
      target_selector->target_id = 0;
      target_selector->should_chase = false;
    }

    if (target->has_component<Engine::Core::BuildingComponent>()) {
      if (auto *r = target->get_component<Engine::Core::RenderableComponent>()) {
        r->visible = false;
      }
      target->add_component<Engine::Core::PendingRemovalComponent>();
    } else {
      apply_death_motion(target, attacker);
    }
  }
}

void apply_hit_feedback(Engine::Core::Entity *target,
                        Engine::Core::EntityID attacker_id,
                        Engine::Core::World *world) {
  if (target == nullptr) {
    return;
  }

  auto *feedback = target->get_component<Engine::Core::HitFeedbackComponent>();
  if (feedback == nullptr) {
    feedback = target->add_component<Engine::Core::HitFeedbackComponent>();
  }
  if (feedback == nullptr) {
    return;
  }

  feedback->is_reacting = true;
  feedback->reaction_time = 0.0F;
  feedback->reaction_intensity = 0.85F;

  auto *target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (target_transform != nullptr && attacker_id != 0 && world != nullptr) {
    auto *attacker = world->get_entity(attacker_id);
    if (attacker != nullptr) {
      auto *attacker_transform =
          attacker->get_component<Engine::Core::TransformComponent>();
      if (attacker_transform != nullptr) {
        auto *attacker_attack =
            attacker->get_component<Engine::Core::AttackComponent>();
        auto *attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
        float knockback_scale = 1.0F;
        if ((attacker_attack != nullptr) &&
            attacker_attack->current_mode ==
                Engine::Core::AttackComponent::CombatMode::Melee) {
          knockback_scale = 1.25F;
          feedback->reaction_intensity = 1.0F;
        } else {
          knockback_scale = 0.8F;
          feedback->reaction_intensity = 0.70F;
        }
        if (attacker_unit != nullptr &&
            attacker_unit->spawn_type == Game::Units::SpawnType::Elephant) {
          knockback_scale = 2.2F;
          feedback->reaction_intensity = 1.35F;
        }

        float const dx =
            target_transform->position.x - attacker_transform->position.x;
        float const dz =
            target_transform->position.z - attacker_transform->position.z;
        float const dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 0.001F) {
          float const knockback = std::clamp(
              Engine::Core::HitFeedbackComponent::k_max_knockback *
                  knockback_scale,
              0.0F,
              Engine::Core::HitFeedbackComponent::k_max_knockback * 2.8F);
          feedback->knockback_x = (dx / dist) * knockback;
          feedback->knockback_z = (dz / dist) * knockback;

          float const face_dx =
              attacker_transform->position.x - target_transform->position.x;
          float const face_dz =
              attacker_transform->position.z - target_transform->position.z;
          float const face_dist =
              std::sqrt(face_dx * face_dx + face_dz * face_dz);
          if (face_dist > 0.001F) {
            float const yaw =
                std::atan2(face_dx, face_dz) * 180.0F / std::numbers::pi_v<float>;
            target_transform->desired_yaw = yaw;
            target_transform->has_desired_yaw = true;
          }
        }
      }
    }
  }

  auto *combat_state =
      target->get_component<Engine::Core::CombatStateComponent>();
  if (combat_state != nullptr) {
    combat_state->is_hit_paused = true;
    combat_state->hit_pause_remaining = Engine::Core::CombatStateComponent::
        k_combat_animation_hit_pause_duration;
  }
}

} // namespace Game::Systems::Combat
