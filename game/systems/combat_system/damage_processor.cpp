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

auto is_mounted_spawn(Game::Units::SpawnType spawn_type) -> bool {
  using Game::Units::SpawnType;
  return spawn_type == SpawnType::MountedKnight ||
         spawn_type == SpawnType::HorseArcher ||
         spawn_type == SpawnType::HorseSpearman;
}

auto resolve_death_profile(const Engine::Core::UnitComponent *unit)
    -> Engine::Core::DeathSequenceProfile {
  using Engine::Core::DeathSequenceProfile;
  if (unit == nullptr) {
    return DeathSequenceProfile::Infantry;
  }
  if (unit->death_sequence_override != 0xFFU &&
      unit->death_sequence_override <=
      static_cast<std::uint8_t>(DeathSequenceProfile::Elephant)) {
    return static_cast<DeathSequenceProfile>(unit->death_sequence_override);
  }
  if (unit->spawn_type == Game::Units::SpawnType::Elephant) {
    return DeathSequenceProfile::Elephant;
  }
  if (is_mounted_spawn(unit->spawn_type)) {
    return DeathSequenceProfile::MountedRider;
  }
  return DeathSequenceProfile::Infantry;
}

auto resolve_death_variant(Engine::Core::Entity *target,
                           Engine::Core::Entity *attacker,
                           Engine::Core::DeathSequenceProfile profile)
    -> std::uint8_t {
  if (target == nullptr) {
    return 0U;
  }
  std::uint32_t seed = target->get_id() * 2654435761U;
  if (attacker != nullptr) {
    seed ^= attacker->get_id() * 2246822519U;
  }
  seed = mix_hash(seed);
  switch (profile) {
  case Engine::Core::DeathSequenceProfile::MountedRider:
    return 2U;
  case Engine::Core::DeathSequenceProfile::Elephant:
    return static_cast<std::uint8_t>(seed & 1U);
  case Engine::Core::DeathSequenceProfile::Horse:
    return 0U;
  case Engine::Core::DeathSequenceProfile::Infantry:
  default:
    return static_cast<std::uint8_t>(seed & 1U);
  }
}

void begin_death_sequence(Engine::Core::Entity *target,
                          Engine::Core::Entity *attacker) {
  if (target == nullptr) {
    return;
  }

  auto *unit = target->get_component<Engine::Core::UnitComponent>();
  auto *death = target->get_component<Engine::Core::DeathAnimationComponent>();
  if (death == nullptr) {
    death = target->add_component<Engine::Core::DeathAnimationComponent>();
  }
  if (death == nullptr) {
    return;
  }

  death->profile = resolve_death_profile(unit);
  death->state = Engine::Core::DeathSequenceState::Dying;
  death->state_time = 0.0F;
  death->state_duration = 1.0F;
  death->dead_hold_duration = 0.8F;
  death->sequence_variant =
      resolve_death_variant(target, attacker, death->profile);

  switch (death->profile) {
  case Engine::Core::DeathSequenceProfile::MountedRider:
    death->state_duration = 1.15F;
    death->dead_hold_duration = 0.95F;
    break;
  case Engine::Core::DeathSequenceProfile::Horse:
    death->state_duration = 1.20F;
    death->dead_hold_duration = 1.00F;
    death->sequence_variant = 0U;
    break;
  case Engine::Core::DeathSequenceProfile::Elephant:
    death->state_duration = 1.50F;
    death->dead_hold_duration = 1.25F;
    break;
  case Engine::Core::DeathSequenceProfile::Infantry:
  default:
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
      begin_death_sequence(target, attacker);
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
