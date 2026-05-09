#include "damage_processor.h"
#include "../../core/component.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../../units/troop_config.h"
#include "../building_collision_registry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

namespace Game::Systems::Combat {

namespace {

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

struct DeathSequenceTiming {
  float state_duration{1.0F};
  float dead_hold_duration{0.8F};
  std::uint8_t sequence_variant{0U};
};

auto resolve_death_variant(Engine::Core::Entity *target,
                           Engine::Core::Entity *attacker,
                           Engine::Core::DeathSequenceProfile profile)
    -> std::uint8_t {
  (void)target;
  (void)attacker;
  switch (profile) {
  case Engine::Core::DeathSequenceProfile::MountedRider:
  case Engine::Core::DeathSequenceProfile::Elephant:
  case Engine::Core::DeathSequenceProfile::Horse:
  case Engine::Core::DeathSequenceProfile::Infantry:
  default:
    // Death profile selects the archetype/clip family; sequence variants remain
    // local within that family instead of encoding raw cross-family offsets.
    return 0U;
  }
}

auto resolve_death_timing(Engine::Core::DeathSequenceProfile profile,
                          std::uint8_t variant) -> DeathSequenceTiming {
  DeathSequenceTiming timing{};
  timing.sequence_variant = variant;
  switch (profile) {
  case Engine::Core::DeathSequenceProfile::MountedRider:
    timing.state_duration = 1.15F;
    timing.dead_hold_duration = 0.95F;
    break;
  case Engine::Core::DeathSequenceProfile::Horse:
    timing.state_duration = 1.20F;
    timing.dead_hold_duration = 1.00F;
    timing.sequence_variant = 0U;
    break;
  case Engine::Core::DeathSequenceProfile::Elephant:
    timing.state_duration = 1.50F;
    timing.dead_hold_duration = 1.25F;
    break;
  case Engine::Core::DeathSequenceProfile::Infantry:
  default:
    break;
  }
  return timing;
}

void apply_death_sequence(Engine::Core::DeathAnimationComponent &death,
                          Engine::Core::DeathSequenceProfile profile,
                          std::uint8_t variant) {
  auto const timing = resolve_death_timing(profile, variant);
  death.profile = profile;
  death.state = Engine::Core::DeathSequenceState::Dying;
  death.state_time = 0.0F;
  death.state_duration = timing.state_duration;
  death.dead_hold_duration = timing.dead_hold_duration;
  death.sequence_variant = timing.sequence_variant;
}

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent &unit) -> int {
  if (unit.render_individuals_per_unit_override > 0) {
    return unit.render_individuals_per_unit_override;
  }
  return Game::Units::TroopConfig::instance().get_individuals_per_unit(
      unit.spawn_type);
}

void begin_soldier_casualties(Engine::Core::Entity *target,
                              Engine::Core::Entity *attacker, int prev_health,
                              int new_health) {
  if (target == nullptr) {
    return;
  }

  auto *unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return;
  }

  int const individuals_per_unit = resolved_individuals_per_unit(*unit);
  if (individuals_per_unit <= 1) {
    return;
  }

  int const prev_survivors = Engine::Core::resolve_surviving_individual_count(
      prev_health, unit->max_health, individuals_per_unit);
  int const new_survivors = Engine::Core::resolve_surviving_individual_count(
      new_health, unit->max_health, individuals_per_unit);
  int const casualty_start = (new_health <= 0) ? 1 : new_survivors;
  if (prev_survivors <= casualty_start) {
    return;
  }

  auto const profile = resolve_death_profile(unit);
  auto *casualties =
      Engine::Core::get_or_add_component<
          Engine::Core::SoldierCasualtyAnimationComponent>(target);
  if (casualties == nullptr) {
    return;
  }

  auto const variant = resolve_death_variant(target, attacker, profile);
  auto const timing = resolve_death_timing(profile, variant);
  for (int slot = casualty_start; slot < prev_survivors; ++slot) {
    Engine::Core::SoldierCasualtyAnimationComponent::Entry entry{};
    entry.slot_index = static_cast<std::uint16_t>(slot);
    entry.profile = profile;
    entry.state = Engine::Core::DeathSequenceState::Dying;
    entry.state_time = 0.0F;
    entry.state_duration = timing.state_duration;
    entry.dead_hold_duration = timing.dead_hold_duration;
    entry.sequence_variant = timing.sequence_variant;

    auto existing = std::find_if(
        casualties->entries.begin(), casualties->entries.end(),
        [slot](const auto &active) { return active.slot_index == slot; });
    if (existing != casualties->entries.end()) {
      *existing = entry;
    } else {
      casualties->entries.push_back(entry);
    }
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

  auto const profile = resolve_death_profile(unit);
  auto const variant = resolve_death_variant(target, attacker, profile);
  apply_death_sequence(*death, profile, variant);
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

  begin_soldier_casualties(target,
                           (attacker_id != 0 && world != nullptr)
                               ? world->get_entity(attacker_id)
                               : nullptr,
                           prev_health, new_health);

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
