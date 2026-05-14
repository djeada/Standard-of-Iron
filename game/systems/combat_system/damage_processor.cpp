#include "damage_processor.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

#include "../../core/component.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../../units/troop_config.h"
#include "../building_collision_registry.h"
#include "../rpg_combat_system/rpg_damage_resolver.h"

namespace Game::Systems::Combat {

namespace {

auto is_mounted_spawn(Game::Units::SpawnType spawn_type) -> bool {
  using Game::Units::SpawnType;
  return spawn_type == SpawnType::MountedKnight ||
         spawn_type == SpawnType::HorseArcher || spawn_type == SpawnType::HorseSpearman;
}

auto resolve_death_profile(const Engine::Core::UnitComponent* unit)
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

auto resolve_death_variant(Engine::Core::Entity* target,
                           Engine::Core::Entity* attacker,
                           Engine::Core::DeathSequenceProfile profile) -> std::uint8_t {
  (void)target;
  (void)attacker;
  switch (profile) {
  case Engine::Core::DeathSequenceProfile::MountedRider:
  case Engine::Core::DeathSequenceProfile::Elephant:
  case Engine::Core::DeathSequenceProfile::Horse:
  case Engine::Core::DeathSequenceProfile::Infantry:
  default:

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

void apply_death_sequence(Engine::Core::DeathAnimationComponent& death,
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

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent& unit) -> int {
  if (unit.render_individuals_per_unit_override > 0) {
    return unit.render_individuals_per_unit_override;
  }
  return Game::Units::TroopConfig::instance().get_individuals_per_unit(unit.spawn_type);
}

auto apply_commander_guard_reduction(Engine::Core::World* world,
                                     Engine::Core::Entity* target,
                                     int damage,
                                     Engine::Core::EntityID attacker_id) -> int {
  if (world == nullptr || target == nullptr || attacker_id == 0 || damage <= 0) {
    return damage;
  }

  auto* guard = target->get_component<Engine::Core::CommanderGuardComponent>();
  if (guard == nullptr || !guard->active) {
    return damage;
  }

  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  auto* attacker = world->get_entity(attacker_id);
  auto* attacker_transform =
      attacker != nullptr ? attacker->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
  if (target_transform == nullptr || attacker_transform == nullptr) {
    return damage;
  }

  constexpr float k_degrees_to_radians = 0.017453292519943295F;
  const float yaw = target_transform->rotation.y * k_degrees_to_radians;
  const QVector3D forward(std::sin(yaw), 0.0F, std::cos(yaw));
  QVector3D to_attacker(attacker_transform->position.x - target_transform->position.x,
                        0.0F,
                        attacker_transform->position.z - target_transform->position.z);
  if (to_attacker.lengthSquared() <= 0.0001F) {
    return damage;
  }
  to_attacker.normalize();

  if (QVector3D::dotProduct(forward, to_attacker) < guard->frontal_arc_dot) {
    return damage;
  }

  return std::max(
      1,
      static_cast<int>(std::round(static_cast<float>(damage) *
                                  std::clamp(guard->damage_multiplier, 0.0F, 1.0F))));
}

auto begin_soldier_casualties(Engine::Core::Entity* target,
                              Engine::Core::Entity* attacker,
                              int prev_health,
                              int new_health) -> int {
  if (target == nullptr) {
    return 0;
  }

  auto* unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return 0;
  }

  int const individuals_per_unit = resolved_individuals_per_unit(*unit);
  if (individuals_per_unit <= 1) {
    return 0;
  }

  int const prev_survivors = Engine::Core::resolve_surviving_individual_count(
      prev_health, unit->max_health, individuals_per_unit);
  int const new_survivors = Engine::Core::resolve_surviving_individual_count(
      new_health, unit->max_health, individuals_per_unit);
  int const casualty_start = (new_health <= 0) ? 1 : new_survivors;
  if (prev_survivors <= casualty_start) {
    return 0;
  }

  auto const profile = resolve_death_profile(unit);
  auto* casualties = Engine::Core::get_or_add_component<
      Engine::Core::SoldierCasualtyAnimationComponent>(target);
  if (casualties == nullptr) {
    return 0;
  }

  auto const variant = resolve_death_variant(target, attacker, profile);
  auto const timing = resolve_death_timing(profile, variant);
  int queued_casualties = 0;
  for (int slot = casualty_start; slot < prev_survivors; ++slot) {
    Engine::Core::SoldierCasualtyAnimationComponent::Entry entry{};
    entry.slot_index = static_cast<std::uint16_t>(slot);
    entry.profile = profile;
    entry.state = Engine::Core::DeathSequenceState::Dying;
    entry.state_time = 0.0F;
    entry.state_duration = timing.state_duration;
    entry.dead_hold_duration = timing.dead_hold_duration;
    entry.sequence_variant = timing.sequence_variant;

    auto existing =
        std::find_if(casualties->entries.begin(),
                     casualties->entries.end(),
                     [slot](const auto& active) { return active.slot_index == slot; });
    if (existing != casualties->entries.end()) {
      *existing = entry;
    } else {
      casualties->entries.push_back(entry);
    }
    ++queued_casualties;
  }
  return queued_casualties;
}

void begin_death_sequence(Engine::Core::Entity* target,
                          Engine::Core::Entity* attacker) {
  if (target == nullptr) {
    return;
  }

  auto* unit = target->get_component<Engine::Core::UnitComponent>();
  auto* death = target->get_component<Engine::Core::DeathAnimationComponent>();
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

void prune_oldest_blood_stain(Engine::Core::World* world) {
  if (world == nullptr) {
    return;
  }

  while (true) {
    auto blood_stains = world->get_entities_with<Engine::Core::BloodStainComponent>();
    if (blood_stains.size() <
        static_cast<std::size_t>(Engine::Core::Defaults::k_blood_stain_max_active)) {
      return;
    }

    auto const oldest = std::min_element(
        blood_stains.begin(),
        blood_stains.end(),
        [](const Engine::Core::Entity* lhs, const Engine::Core::Entity* rhs) {
          if (lhs == nullptr || rhs == nullptr) {
            return rhs != nullptr;
          }
          return lhs->get_id() < rhs->get_id();
        });
    if (oldest == blood_stains.end() || *oldest == nullptr) {
      return;
    }

    world->destroy_entity((*oldest)->get_id());
  }
}

auto hash01(std::uint32_t value) -> float {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return static_cast<float>(value & 0x00ffffffU) / static_cast<float>(0x01000000U);
}

auto blood_stain_scale(const Engine::Core::UnitComponent* unit) -> float {
  if (unit == nullptr) {
    return 1.0F;
  }
  if (unit->spawn_type == Game::Units::SpawnType::Elephant) {
    return 1.65F;
  }
  if (is_mounted_spawn(unit->spawn_type)) {
    return 1.25F;
  }
  return 1.0F;
}

void spawn_blood_stain(Engine::Core::World* world, const Engine::Core::Entity* target) {
  if (world == nullptr || target == nullptr) {
    return;
  }

  auto const* transform = target->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  prune_oldest_blood_stain(world);

  auto* blood_stain = world->create_entity();
  if (blood_stain == nullptr) {
    return;
  }

  auto const* unit = target->get_component<Engine::Core::UnitComponent>();
  auto const id_seed = static_cast<std::uint32_t>(target->get_id());
  auto const position_seed =
      static_cast<std::uint32_t>(std::abs(transform->position.x) * 31.0F +
                                 std::abs(transform->position.z) * 131.0F);
  float const scale = blood_stain_scale(unit);
  float const radius = Engine::Core::Defaults::k_blood_stain_default_radius * scale *
                       (0.85F + hash01(id_seed * 17U + position_seed) * 0.45F);
  float const rotation =
      hash01(id_seed * 97U + position_seed * 3U) * std::numbers::pi_v<float> * 2.0F;
  float const aspect_ratio =
      0.72F + hash01(id_seed * 53U + position_seed * 11U) * 0.62F;
  float const seed = hash01(id_seed * 193U + position_seed * 29U);

  blood_stain->add_component<Engine::Core::TransformComponent>(
      transform->position.x, transform->position.y, transform->position.z);
  blood_stain->add_component<Engine::Core::BloodStainComponent>(
      radius,
      Engine::Core::Defaults::k_blood_stain_default_lifetime,
      rotation,
      aspect_ratio,
      seed);
}

} // namespace

void deal_damage(Engine::Core::World* world,
                 Engine::Core::Entity* target,
                 int damage,
                 Engine::Core::EntityID attacker_id) {
  auto* unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return;
  }

  damage = apply_commander_guard_reduction(world, target, damage, attacker_id);

  if (attacker_id != 0 && world != nullptr) {
    auto* attacker_ent = world->get_entity(attacker_id);
    if (attacker_ent != nullptr) {
      auto* cmd = attacker_ent->get_component<Engine::Core::CommanderComponent>();
      if (cmd != nullptr) {
        constexpr std::array<float, 4> k_combo_mult = {1.0F, 1.1F, 1.2F, 1.5F};
        const int step = std::min(cmd->combo_step, 3);
        damage = static_cast<int>(
            std::roundf(static_cast<float>(damage) * k_combo_mult[step]));
        if (cmd->power_strike_active) {
          damage = static_cast<int>(std::roundf(static_cast<float>(damage) * 1.8F));
          cmd->power_strike_active = false;
        }
        cmd->combo_step = (cmd->combo_step + 1) % 4;
        cmd->just_struck_enemy = true;
      }
    }
  }

  if (auto* rpg = target->get_component<Engine::Core::RpgHealthComponent>();
      rpg != nullptr && rpg->active) {
    auto const rpg_result = Game::Systems::RpgCombat::resolve_rpg_damage(
        world, target, damage, attacker_id);
    if (!rpg_result.killed) {
      if (rpg_result.effective_damage > 0) {
        apply_hit_feedback(target, attacker_id, world);
        Game::Units::SpawnType attacker_type_hit = Game::Units::SpawnType::Knight;
        if (attacker_id != 0 && world != nullptr) {
          auto* atk_ent = world->get_entity(attacker_id);
          if (atk_ent != nullptr) {
            auto* atk_unit = atk_ent->get_component<Engine::Core::UnitComponent>();
            if (atk_unit != nullptr) {
              attacker_type_hit = atk_unit->spawn_type;
            }
          }
        }
        Engine::Core::EventManager::instance().publish(
            Engine::Core::CombatHitEvent(attacker_id,
                                         target->get_id(),
                                         rpg_result.effective_damage,
                                         attacker_type_hit,
                                         false));
      }
      return;
    }

    damage = unit->health;
  }

  int const prev_health = unit->health;
  int const new_health = std::max(0, prev_health - damage);
  bool const is_killing_blow = (prev_health > 0 && prev_health <= damage);

  unit->health = new_health;

  int const queued_soldier_casualties = begin_soldier_casualties(
      target,
      (attacker_id != 0 && world != nullptr) ? world->get_entity(attacker_id) : nullptr,
      prev_health,
      new_health);
  if (queued_soldier_casualties > 0 && !is_killing_blow &&
      !target->has_component<Engine::Core::BuildingComponent>()) {
    spawn_blood_stain(world, target);
  }

  int attacker_owner_id = 0;
  std::optional<Game::Units::SpawnType> attacker_type_opt;
  if (attacker_id != 0 && (world != nullptr)) {
    auto* attacker = world->get_entity(attacker_id);
    if (attacker != nullptr) {
      auto* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
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

  if (target->has_component<Engine::Core::BuildingComponent>() && unit->health > 0) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::BuildingAttackedEvent(target->get_id(),
                                            unit->owner_id,
                                            unit->spawn_type,
                                            attacker_id,
                                            attacker_owner_id,
                                            damage));
  }

  if (unit->health <= 0) {
    auto* attacker = (attacker_id != 0 && world != nullptr)
                         ? world->get_entity(attacker_id)
                         : nullptr;
    int const killer_owner_id = attacker_owner_id;

    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitDiedEvent(target->get_id(),
                                    unit->owner_id,
                                    unit->spawn_type,
                                    attacker_id,
                                    killer_owner_id));

    auto* target_atk = target->get_component<Engine::Core::AttackComponent>();
    if ((target_atk != nullptr) && target_atk->in_melee_lock &&
        target_atk->melee_lock_target_id != 0) {
      if (world != nullptr) {
        auto* lock_partner = world->get_entity(target_atk->melee_lock_target_id);
        if ((lock_partner != nullptr) &&
            !lock_partner->has_component<Engine::Core::PendingRemovalComponent>()) {
          auto* partner_atk =
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
      BuildingCollisionRegistry::instance().unregister_building(target->get_id());
    }

    if (auto* movement = target->get_component<Engine::Core::MovementComponent>()) {
      movement->has_target = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->clear_path();
      movement->path_pending = false;
    }

    auto* attack = target->get_component<Engine::Core::AttackComponent>();
    if (attack != nullptr) {
      attack->in_melee_lock = false;
      attack->melee_lock_target_id = 0;
    }
    auto* target_selector =
        target->get_component<Engine::Core::AttackTargetComponent>();
    if (target_selector != nullptr) {
      target_selector->target_id = 0;
      target_selector->should_chase = false;
    }

    if (target->has_component<Engine::Core::BuildingComponent>()) {
      if (auto* r = target->get_component<Engine::Core::RenderableComponent>()) {
        r->visible = false;
      }
      target->add_component<Engine::Core::PendingRemovalComponent>();
    } else {
      if (is_killing_blow) {
        spawn_blood_stain(world, target);
      }
      begin_death_sequence(target, attacker);
    }
  }
}

void apply_hit_feedback(Engine::Core::Entity* target,
                        Engine::Core::EntityID attacker_id,
                        Engine::Core::World* world) {
  if (target == nullptr) {
    return;
  }

  auto* feedback = target->get_component<Engine::Core::HitFeedbackComponent>();
  if (feedback == nullptr) {
    feedback = target->add_component<Engine::Core::HitFeedbackComponent>();
  }
  if (feedback == nullptr) {
    return;
  }

  feedback->is_reacting = true;
  feedback->reaction_time = 0.0F;
  feedback->reaction_intensity = 0.85F;

  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  if (target_transform != nullptr && attacker_id != 0 && world != nullptr) {
    auto* attacker = world->get_entity(attacker_id);
    if (attacker != nullptr) {
      auto* attacker_transform =
          attacker->get_component<Engine::Core::TransformComponent>();
      if (attacker_transform != nullptr) {
        auto* attacker_attack =
            attacker->get_component<Engine::Core::AttackComponent>();
        auto* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
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

        float const dx = target_transform->position.x - attacker_transform->position.x;
        float const dz = target_transform->position.z - attacker_transform->position.z;
        float const dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 0.001F) {
          float const knockback = std::clamp(
              Engine::Core::HitFeedbackComponent::k_max_knockback * knockback_scale,
              0.0F,
              Engine::Core::HitFeedbackComponent::k_max_knockback * 2.8F);
          feedback->knockback_x = (dx / dist) * knockback;
          feedback->knockback_z = (dz / dist) * knockback;

          float const face_dx =
              attacker_transform->position.x - target_transform->position.x;
          float const face_dz =
              attacker_transform->position.z - target_transform->position.z;
          float const face_dist = std::sqrt(face_dx * face_dx + face_dz * face_dz);
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

  auto* combat_state = target->get_component<Engine::Core::CombatStateComponent>();
  if (combat_state != nullptr) {
    combat_state->is_hit_paused = true;
    combat_state->hit_pause_remaining =
        Engine::Core::CombatStateComponent::k_combat_animation_hit_pause_duration;
  }
}

} // namespace Game::Systems::Combat
