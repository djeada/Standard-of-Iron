#include "attack_processor.h"

#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../../units/troop_config.h"
#include "../../visuals/team_colors.h"
#include "../arrow_system.h"
#include "../combat_actions/combat_action_definition.h"
#include "../combat_actions/combat_action_events.h"
#include "../combat_rules.h"
#include "../command_service.h"
#include "../formation_combat_geometry.h"
#include "../order_service.h"
#include "../owner_registry.h"
#include "../pathfinding.h"
#include "../projectile_system.h"
#include "../rpg_combat_system/rpg_commander_damage.h"
#include "../troop_profile_service.h"
#include "combat_animation_timing.h"
#include "combat_mode_processor.h"
#include "combat_random.h"
#include "combat_types.h"
#include "combat_utils.h"
#include "damage_processor.h"

namespace Game::Systems::Combat {

namespace {
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

auto melee_strike_blocked_by_commander_ring(Engine::Core::Entity* attacker,
                                            Engine::Core::Entity* target) -> bool {
  if (attacker == nullptr || target == nullptr) {
    return false;
  }
  auto const* commander = target->get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || !commander->fpv_controlled) {
    return false;
  }
  auto const* engagement =
      target->get_component<Engine::Core::RpgEngagementComponent>();
  if (engagement == nullptr || engagement->active_attackers <= 0) {
    return false;
  }
  auto const attacker_id = attacker->get_id();
  return attacker_id != engagement->front_attacker_id &&
         attacker_id != engagement->left_threat_id &&
         attacker_id != engagement->right_threat_id;
}

void begin_attack_animation(Engine::Core::Entity* attacker,
                            bool preserve_seed = false) {
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
    {
      auto const* commander =
          attacker->get_component<Engine::Core::CommanderComponent>();
      bool const is_fpv = (commander != nullptr && commander->fpv_controlled);
      if (is_fpv) {

        combat_state->swing_duration_scale = 1.0F;
        combat_state->state_duration =
            Engine::Core::CombatStateComponent::k_advance_duration *
            commander_attack_advance_scale(attacker);
      } else {

        float scale = 1.0F;
        if (attack != nullptr &&
            attack->current_mode == Engine::Core::AttackComponent::CombatMode::Melee) {
          scale = Engine::Core::CombatStateComponent::cooldown_fit_scale(
              attack->get_current_cooldown());
        }
        combat_state->swing_duration_scale = scale;
        combat_state->state_duration =
            Engine::Core::CombatStateComponent::k_advance_duration * scale;
      }
    }
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
    }
  }
}

auto should_queue_chase_command(Engine::Core::Entity* attacker,
                                Engine::Core::Entity* target,
                                Engine::Core::TransformComponent* attacker_transform,
                                Engine::Core::MovementComponent* movement,
                                const QVector3D& desired_pos,
                                float range) -> bool {
  if ((attacker == nullptr) || (target == nullptr) || (attacker_transform == nullptr) ||
      (movement == nullptr)) {
    return false;
  }

  if (is_in_range(attacker, target, range)) {
    return false;
  }

  QVector3D const planned_target(movement->get_goal_x(), 0.0F, movement->get_goal_y());

  float const threshold_sq =
      Constants::k_new_command_threshold * Constants::k_new_command_threshold;
  return (!movement->get_has_target() && !movement->has_waypoints()) ||
         (planned_target - desired_pos).lengthSquared() > threshold_sq;
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

auto matches_heal_affinity(const Engine::Core::Entity* target,
                           Engine::Core::HealerComponent::TargetAffinity affinity)
    -> bool {
  if (target == nullptr) {
    return false;
  }

  bool const target_is_undead = target->has_component<Engine::Core::UndeadComponent>();
  switch (affinity) {
  case Engine::Core::HealerComponent::TargetAffinity::UndeadAllies:
    return target_is_undead;
  case Engine::Core::HealerComponent::TargetAffinity::LivingAllies:
  default:
    return !target_is_undead;
  }
}

auto should_prioritize_healing(Engine::Core::Entity* healer,
                               Engine::Core::World* world) -> bool {
  if (healer == nullptr || world == nullptr) {
    return false;
  }

  auto* healer_component = healer->get_component<Engine::Core::HealerComponent>();
  auto* healer_unit = healer->get_component<Engine::Core::UnitComponent>();
  auto* healer_transform = healer->get_component<Engine::Core::TransformComponent>();
  if (healer_component == nullptr || healer_unit == nullptr ||
      healer_transform == nullptr || !healer_component->suppress_attack_while_healing ||
      healer_component->time_since_last_heal < healer_component->healing_cooldown) {
    return false;
  }

  for (auto* target : world->get_entities_with<Engine::Core::UnitComponent>()) {
    if (target == nullptr ||
        target->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
    auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
    if (target_unit == nullptr || target_transform == nullptr ||
        target_unit->owner_id != healer_unit->owner_id || target_unit->health <= 0 ||
        target_unit->health >= target_unit->max_health ||
        !matches_heal_affinity(target, healer_component->target_affinity)) {
      continue;
    }

    float const dx = target_transform->position.x - healer_transform->position.x;
    float const dz = target_transform->position.z - healer_transform->position.z;
    float const dist_sq = dx * dx + dz * dz;
    if (dist_sq <= healer_component->healing_range * healer_component->healing_range) {
      return true;
    }
  }

  return false;
}

auto projectile_color_for_kind(Game::Systems::ProjectileKind kind,
                               const QVector3D& team_color) -> QVector3D {
  switch (kind) {
  case Game::Systems::ProjectileKind::Fireball:
    return {1.0F, 0.45F, 0.10F};
  case Game::Systems::ProjectileKind::CursedArrow:
    return {0.58F, 0.20F, 0.82F};
  case Game::Systems::ProjectileKind::Arrow:
  case Game::Systems::ProjectileKind::Stone:
  default:
    return team_color;
  }
}

auto projectile_speed_for_kind(Game::Systems::ProjectileKind kind) -> float {
  switch (kind) {
  case Game::Systems::ProjectileKind::Fireball:
    return Constants::k_arrow_speed * 0.72F;
  case Game::Systems::ProjectileKind::CursedArrow:
    return Constants::k_arrow_speed * 0.92F;
  case Game::Systems::ProjectileKind::Arrow:
  case Game::Systems::ProjectileKind::Stone:
  default:
    return Constants::k_arrow_speed;
  }
}

void launch_special_projectile(Engine::Core::Entity* attacker,
                               Engine::Core::Entity* target,
                               int damage,
                               ProjectileSystem* projectile_system) {
  if (attacker == nullptr || target == nullptr || projectile_system == nullptr) {
    return;
  }

  auto* attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  auto* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
  auto* special_attack =
      attacker->get_component<Engine::Core::SpecialAttackComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr ||
      attacker_unit == nullptr || special_attack == nullptr) {
    return;
  }

  QVector3D const attacker_pos(attacker_transform->position.x,
                               attacker_transform->position.y,
                               attacker_transform->position.z);
  QVector3D const target_pos(target_transform->position.x,
                             target_transform->position.y,
                             target_transform->position.z);
  QVector3D const direction = (target_pos - attacker_pos).normalized();
  QVector3D const start = attacker_pos + QVector3D(0.0F, 1.15F, 0.0F) +
                          direction * Constants::k_arrow_start_offset;
  QVector3D const end = target_pos + QVector3D(0.0F, 0.85F, 0.0F) +
                        direction * Constants::k_arrow_target_offset;
  QVector3D const team_color =
      Game::Visuals::team_colorForOwner(attacker_unit->owner_id);

  projectile_system->spawn_arrow(
      start,
      end,
      projectile_color_for_kind(special_attack->projectile_kind, team_color),
      projectile_speed_for_kind(special_attack->projectile_kind),
      false,
      special_attack->projectile_kind,
      true,
      damage,
      attacker->get_id(),
      target->get_id(),
      special_attack->splash_radius,
      special_attack->splash_damage_multiplier,
      special_attack->friendly_fire);
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

void lock_facing(Engine::Core::TransformComponent* actor_transform,
                 Engine::Core::TransformComponent* target_transform) {
  if (actor_transform == nullptr || target_transform == nullptr) {
    return;
  }
  float const dx = target_transform->position.x - actor_transform->position.x;
  float const dz = target_transform->position.z - actor_transform->position.z;
  if (dx * dx + dz * dz > 0.000001F) {
    actor_transform->rotation.y =
        std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
  }
  actor_transform->desired_yaw = actor_transform->rotation.y;
  actor_transform->has_desired_yaw = false;
}

[[nodiscard]] auto is_multi_body_formation(Engine::Core::Entity* actor) -> bool {
  auto const* actor_unit =
      actor != nullptr ? actor->get_component<Engine::Core::UnitComponent>() : nullptr;
  bool const explicitly_single_body =
      actor_unit != nullptr && actor_unit->render_individuals_per_unit_override == 1;
  return actor != nullptr && !explicitly_single_body &&
         FormationCombat::has_formation_slots(*actor);
}

void lock_combatant_facing(Engine::Core::Entity* actor,
                           Engine::Core::TransformComponent* actor_transform,
                           Engine::Core::TransformComponent* target_transform) {
  if (is_multi_body_formation(actor)) {

    actor_transform->desired_yaw = actor_transform->rotation.y;
    actor_transform->has_desired_yaw = false;
    return;
  }
  lock_facing(actor_transform, target_transform);
}

auto chase_spread_angle(Engine::Core::EntityID attacker_id) -> float {
  std::uint32_t const seed =
      static_cast<std::uint32_t>(attacker_id * 2654435761U) ^ 0x85EBCA6BU;
  return (hash_to_unit(seed) - 0.5F) * Constants::k_chase_spread_arc;
}

auto rotate_xz(const QVector3D& vec, float angle) -> QVector3D {
  float const cos_a = std::cos(angle);
  float const sin_a = std::sin(angle);
  return {vec.x() * cos_a - vec.z() * sin_a, 0.0F, vec.x() * sin_a + vec.z() * cos_a};
}

auto chase_destination(const QVector3D& attacker_pos,
                       const QVector3D& target_pos,
                       float desired_distance,
                       float spread_angle) -> QVector3D {
  QVector3D approach = attacker_pos - target_pos;
  approach.setY(0.0F);
  float const length_sq = approach.lengthSquared();
  if (length_sq <= 0.000001F) {
    approach = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    approach /= std::sqrt(length_sq);
  }
  approach = rotate_xz(approach, spread_angle);
  return target_pos + approach * desired_distance;
}

auto has_valid_melee_lock(Engine::Core::Entity* entity,
                          Engine::Core::World* world) -> bool {
  if ((entity == nullptr) || (world == nullptr)) {
    return false;
  }

  if (!Game::Systems::CombatRules::participates_in_rts_melee_lock(entity)) {
    return false;
  }

  auto* attack = entity->get_component<Engine::Core::AttackComponent>();
  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if ((attack == nullptr) || (unit == nullptr) || !attack->in_melee_lock ||
      attack->melee_lock_target_id == 0) {
    return false;
  }

  auto* target = world->get_entity(attack->melee_lock_target_id);
  return is_valid_enemy_unit(unit, target, true);
}

auto is_large_melee_anchor(Engine::Core::Entity* entity) -> bool {
  if (entity == nullptr) {
    return false;
  }

  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  return entity->has_component<Engine::Core::BuildingComponent>() ||
         entity->has_component<Engine::Core::ElephantComponent>() ||
         ((unit != nullptr) && unit->spawn_type == Game::Units::SpawnType::Elephant);
}

auto is_ranged_mode(Engine::Core::AttackComponent* attack_comp) -> bool {
  return (attack_comp != nullptr) && attack_comp->can_ranged &&
         attack_comp->current_mode == Engine::Core::AttackComponent::CombatMode::Ranged;
}

auto get_base_max_health(const Engine::Core::UnitComponent* unit)
    -> std::optional<int> {
  if (unit == nullptr) {
    return std::nullopt;
  }
  auto troop_type_opt = Game::Units::spawn_typeToTroopType(unit->spawn_type);
  if (!troop_type_opt) {
    return std::nullopt;
  }
  auto profile = Game::Systems::TroopProfileService::instance().get_profile(
      unit->nation_id, *troop_type_opt);
  return profile.combat.max_health;
}

auto is_high_ground_advantage(const Engine::Core::TransformComponent* high_transform,
                              const Engine::Core::TransformComponent* low_transform)
    -> bool {
  if ((high_transform == nullptr) || (low_transform == nullptr)) {
    return false;
  }
  float const height_diff = high_transform->position.y - low_transform->position.y;
  return height_diff > Constants::k_high_ground_height_threshold;
}

void process_melee_lock(Engine::Core::Entity* attacker,
                        Engine::Core::AttackComponent* attack_comp,
                        Engine::Core::World* world,
                        float delta_time) {
  if (attack_comp == nullptr || !attack_comp->in_melee_lock) {
    return;
  }

  if (!Game::Systems::CombatRules::participates_in_rts_melee_lock(attacker)) {
    return;
  }

  auto* lock_target = world->get_entity(attack_comp->melee_lock_target_id);
  if ((lock_target == nullptr) ||
      lock_target->has_component<Engine::Core::PendingRemovalComponent>()) {
    attack_comp->in_melee_lock = false;
    attack_comp->melee_lock_target_id = 0;
    return;
  }

  if (!Game::Systems::CombatRules::participates_in_rts_melee_lock(lock_target)) {
    return;
  }

  auto* lock_target_unit = lock_target->get_component<Engine::Core::UnitComponent>();
  if ((lock_target_unit == nullptr) || lock_target_unit->health <= 0) {
    attack_comp->in_melee_lock = false;
    attack_comp->melee_lock_target_id = 0;
    return;
  }

  auto* att_t = attacker->get_component<Engine::Core::TransformComponent>();
  auto* tgt_t = lock_target->get_component<Engine::Core::TransformComponent>();
  if ((att_t == nullptr) || (tgt_t == nullptr)) {
    return;
  }

  lock_combatant_facing(attacker, att_t, tgt_t);
  auto* lock_target_atk = lock_target->get_component<Engine::Core::AttackComponent>();
  bool const reciprocal_lock =
      (lock_target_atk != nullptr) && lock_target_atk->in_melee_lock &&
      lock_target_atk->melee_lock_target_id == attacker->get_id();
  if (reciprocal_lock || !has_valid_melee_lock(lock_target, world)) {
    lock_combatant_facing(lock_target, tgt_t, att_t);
  }

  (void)delta_time;
}

auto locked_target_for_attack(Engine::Core::Entity* attacker,
                              Engine::Core::AttackComponent* attack_comp,
                              Engine::Core::World* world) -> Engine::Core::Entity* {
  if ((attacker == nullptr) || (attack_comp == nullptr) || (world == nullptr) ||
      !attack_comp->in_melee_lock || attack_comp->melee_lock_target_id == 0) {
    return nullptr;
  }

  if (!Game::Systems::CombatRules::participates_in_rts_melee_lock(attacker)) {
    return nullptr;
  }

  auto* target = world->get_entity(attack_comp->melee_lock_target_id);
  auto* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
  if ((target == nullptr) || (attacker_unit == nullptr) ||
      !Game::Systems::CombatRules::participates_in_rts_melee_lock(target) ||
      !is_valid_enemy_unit(attacker_unit, target, true)) {
    return nullptr;
  }

  if (target->get_component<Engine::Core::TransformComponent>() == nullptr) {
    return nullptr;
  }

  return target;
}

void sync_melee_lock_target(Engine::Core::Entity* attacker,
                            Engine::Core::AttackComponent* attack_comp) {
  if (attack_comp == nullptr || !attack_comp->in_melee_lock ||
      attack_comp->melee_lock_target_id == 0 ||
      !Game::Systems::CombatRules::participates_in_rts_melee_lock(attacker)) {
    return;
  }

  auto* attack_target = attacker->get_component<Engine::Core::AttackTargetComponent>();
  if (attack_target == nullptr) {
    attack_target = attacker->add_component<Engine::Core::AttackTargetComponent>();
  }
  if (attack_target != nullptr) {
    auto const* commitment =
        attacker->get_component<Engine::Core::TargetCommitmentComponent>();
    bool const switch_blocked =
        commitment != nullptr && commitment->committed_target_id != 0 &&
        commitment->committed_target_id != attack_comp->melee_lock_target_id &&
        (commitment->in_committed_phase || commitment->cooldown_remaining > 0.0F);
    if (switch_blocked) {
      attack_comp->in_melee_lock = false;
      attack_comp->melee_lock_target_id = 0;
      attack_target->target_id = commitment->committed_target_id;
      attack_target->should_chase = true;
      return;
    }
    attack_target->target_id = attack_comp->melee_lock_target_id;

    attack_target->should_chase = true;
  }
}

void apply_health_bonus(Engine::Core::UnitComponent* unit_comp) {
  auto base_max_health_opt = get_base_max_health(unit_comp);
  int const base_max_health =
      base_max_health_opt.value_or(std::max(1, unit_comp->max_health));
  int const max_health_bonus = static_cast<int>(static_cast<float>(base_max_health) *
                                                Constants::k_health_multiplier_hold);
  if (unit_comp->max_health < max_health_bonus) {
    int const safe_max_health = std::max(1, unit_comp->max_health);
    int const health_percentage = (unit_comp->health * 100) / safe_max_health;
    unit_comp->max_health = max_health_bonus;
    unit_comp->health = (max_health_bonus * health_percentage) / 100;
  }
}

void apply_hold_mode_bonuses(Engine::Core::Entity* attacker,
                             Engine::Core::UnitComponent* unit_comp,
                             float& range,
                             int& damage) {
  auto* hold_mode = attacker->get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode == nullptr) || !hold_mode->active) {
    return;
  }

  if (unit_comp->spawn_type == Game::Units::SpawnType::Archer) {
    range *= Constants::k_range_multiplier_hold;
    damage = static_cast<int>(static_cast<float>(damage) *
                              Constants::k_damage_multiplier_archer_hold);
    apply_health_bonus(unit_comp);
  } else if (unit_comp->spawn_type == Game::Units::SpawnType::Spearman) {
    range *= Constants::k_range_multiplier_spearman_hold;
    damage = static_cast<int>(static_cast<float>(damage) *
                              Constants::k_damage_multiplier_spearman_hold);
    apply_health_bonus(unit_comp);
  } else {
    damage = static_cast<int>(static_cast<float>(damage) *
                              Constants::k_damage_multiplier_default_hold);
  }
}

void apply_high_ground_defense_bonuses(Engine::Core::Entity* attacker,
                                       Engine::Core::Entity* target,
                                       Engine::Core::UnitComponent* target_unit,
                                       int& damage) {
  if (target_unit == nullptr) {
    return;
  }

  if (target_unit->spawn_type != Game::Units::SpawnType::Archer &&
      target_unit->spawn_type != Game::Units::SpawnType::Spearman) {
    return;
  }

  auto* attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  if (!is_high_ground_advantage(target_transform, attacker_transform)) {
    return;
  }

  damage = std::max(1,
                    static_cast<int>(static_cast<float>(damage) *
                                     Constants::k_high_ground_armor_multiplier));

  auto base_max_health_opt = get_base_max_health(target_unit);
  if (!base_max_health_opt || *base_max_health_opt <= 0) {
    return;
  }

  int const max_health_bonus =
      static_cast<int>(static_cast<float>(*base_max_health_opt) *
                       Constants::k_high_ground_health_multiplier);
  if (target_unit->max_health < max_health_bonus) {
    int const safe_max_health = std::max(1, target_unit->max_health);
    int const health_percentage = (target_unit->health * 100) / safe_max_health;
    target_unit->max_health = max_health_bonus;
    target_unit->health = (max_health_bonus * health_percentage) / 100;
  }
}

auto calculate_tactical_damage_multiplier(Engine::Core::Entity* attacker,
                                          Engine::Core::Entity* target,
                                          Engine::Core::UnitComponent* attacker_unit,
                                          Engine::Core::UnitComponent* target_unit)
    -> float {
  float multiplier = 1.0F;

  if (attacker_unit->spawn_type == Game::Units::SpawnType::Spearman) {
    if (target_unit->spawn_type == Game::Units::SpawnType::HorseArcher ||
        target_unit->spawn_type == Game::Units::SpawnType::HorseSpearman ||
        target_unit->spawn_type == Game::Units::SpawnType::MountedKnight) {
      multiplier *= Constants::k_spearman_vs_cavalry_multiplier;
    }
  }

  if (attacker_unit->spawn_type == Game::Units::SpawnType::Archer ||
      attacker_unit->spawn_type == Game::Units::SpawnType::HorseArcher) {

    if (target->has_component<Engine::Core::ElephantComponent>()) {
      multiplier *= Constants::k_archer_vs_elephant_multiplier;
    }

    auto* attacker_transform =
        attacker->get_component<Engine::Core::TransformComponent>();
    auto* target_transform = target->get_component<Engine::Core::TransformComponent>();

    if (is_high_ground_advantage(attacker_transform, target_transform)) {
      multiplier *= Constants::k_archer_high_ground_multiplier;
    }
  } else if (attacker_unit->spawn_type == Game::Units::SpawnType::Spearman) {
    auto* attacker_transform =
        attacker->get_component<Engine::Core::TransformComponent>();
    auto* target_transform = target->get_component<Engine::Core::TransformComponent>();

    if (is_high_ground_advantage(attacker_transform, target_transform)) {
      multiplier *= Constants::k_spearman_high_ground_multiplier;
    }
  }

  return multiplier;
}

void spawn_arrows(Engine::Core::Entity* attacker,
                  Engine::Core::Entity* target,
                  ArrowSystem* arrow_sys) {
  if (arrow_sys == nullptr) {
    return;
  }

  auto* att_t = attacker->get_component<Engine::Core::TransformComponent>();
  auto* tgt_t = target->get_component<Engine::Core::TransformComponent>();
  auto* att_u = attacker->get_component<Engine::Core::UnitComponent>();

  if ((att_t == nullptr) || (tgt_t == nullptr)) {
    return;
  }

  QVector3D const a_pos(att_t->position.x, att_t->position.y, att_t->position.z);
  QVector3D const t_pos(tgt_t->position.x, tgt_t->position.y, tgt_t->position.z);
  QVector3D const dir = (t_pos - a_pos).normalized();
  QVector3D const color = (att_u != nullptr)
                              ? Game::Visuals::team_colorForOwner(att_u->owner_id)
                              : QVector3D(0.8F, 0.9F, 1.0F);

  int arrow_count = 1;
  if (att_u != nullptr) {
    int const troop_size =
        Game::Units::TroopConfig::instance().get_individuals_per_unit(
            att_u->spawn_type);

    int const max_arrows = std::max(2, (troop_size * 2) / 3);

    std::uint32_t const seed =
        static_cast<std::uint32_t>(attacker->get_id() * 2246822519U) ^
        static_cast<std::uint32_t>(target->get_id() * 3266489917U) ^
        static_cast<std::uint32_t>(troop_size * 0x9E37U);
    int const min_arrows = max_arrows / 2;
    int const range = std::max(1, max_arrows - min_arrows + 1);
    arrow_count =
        min_arrows + std::min(range - 1,
                              static_cast<int>(deterministic_unit_roll(seed, 3U) *
                                               static_cast<float>(range)));
  }
  arrow_count = std::min(arrow_count, Constants::k_max_visual_arrows_per_volley);

  QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
  QVector3D const up_vector(0.0F, 1.0F, 0.0F);
  int const wave_count = std::max(1, Constants::k_arrow_volley_wave_count);
  int const rank_count = std::max(1, (arrow_count + wave_count - 1) / wave_count);

  for (int i = 0; i < arrow_count; ++i) {
    std::uint32_t const spread_seed =
        static_cast<std::uint32_t>(attacker->get_id() * 2246822519U) ^
        static_cast<std::uint32_t>(target->get_id() * 3266489917U) ^
        static_cast<std::uint32_t>((i + 1) * 0x85EBCA6BU);
    float const spread_a = deterministic_range(
        spread_seed, 11U, Constants::k_arrow_spread_min, Constants::k_arrow_spread_max);
    float const spread_b = deterministic_range(
        spread_seed, 12U, Constants::k_arrow_spread_min, Constants::k_arrow_spread_max);
    float const spread_c = deterministic_range(
        spread_seed, 13U, Constants::k_arrow_spread_min, Constants::k_arrow_spread_max);
    int const wave_index = i % wave_count;
    int const rank_index = i / wave_count;
    float const centered_wave =
        static_cast<float>(wave_index) - (static_cast<float>(wave_count - 1) * 0.5F);
    float const centered_rank =
        static_cast<float>(rank_index) - (static_cast<float>(rank_count - 1) * 0.5F);

    float const base_lateral = centered_rank * Constants::k_arrow_volley_rank_spacing;
    float const launch_lateral = base_lateral + spread_a * 0.60F;
    float const target_lateral = (base_lateral * 0.95F) + spread_b * 0.45F;
    float const wave_height = (1.0F - 0.18F * std::abs(centered_wave)) *
                              Constants::k_arrow_volley_wave_height;
    float const launch_height =
        wave_height +
        std::abs(spread_b) * (Constants::k_arrow_vertical_spread_factor * 0.48F);
    float const target_height =
        (wave_height * 0.5F) +
        std::abs(spread_c) * (Constants::k_arrow_vertical_spread_factor * 0.22F);
    float const wave_depth = centered_wave * Constants::k_arrow_volley_wave_depth;
    float const depth_offset =
        wave_depth + spread_c * (Constants::k_arrow_depth_spread_factor * 0.55F);

    QVector3D const start_offset =
        perpendicular * launch_lateral + up_vector * launch_height;
    QVector3D const end_offset =
        perpendicular * target_lateral + up_vector * target_height + dir * depth_offset;

    QVector3D const start = a_pos +
                            QVector3D(0.0F, Constants::k_arrow_start_height, 0.0F) +
                            dir * Constants::k_arrow_start_offset + start_offset;
    QVector3D const end = t_pos + dir * Constants::k_arrow_target_offset +
                          QVector3D(0.0F, Constants::k_arrow_target_offset, 0.0F) +
                          end_offset;

    arrow_sys->spawn_arrow(
        start, end, color, Constants::k_arrow_speed, ArrowVisualStyle::Volley);
  }
}

void initiate_melee_combat(Engine::Core::Entity* attacker,
                           Engine::Core::Entity* target,
                           Engine::Core::AttackComponent* attack_comp,
                           Engine::Core::World* world) {
  if ((attacker == nullptr) || (target == nullptr) || (attack_comp == nullptr)) {
    return;
  }

  auto charge_precedes_melee = [](Engine::Core::Entity* entity) {
    auto const* charge = entity->get_component<Engine::Core::MountedChargeComponent>();
    return charge != nullptr &&
           (charge->intent_requested ||
            charge->state == Engine::Core::MountedChargeState::Charging ||
            charge->state == Engine::Core::MountedChargeState::ImpactActive);
  };
  if (charge_precedes_melee(attacker) || charge_precedes_melee(target)) {
    return;
  }

  bool const attacker_uses_rts_lock =
      Game::Systems::CombatRules::participates_in_rts_melee_lock(attacker);
  bool const target_uses_rts_lock =
      Game::Systems::CombatRules::participates_in_rts_melee_lock(target);
  auto* att_t = attacker->get_component<Engine::Core::TransformComponent>();
  auto* tgt_t = target->get_component<Engine::Core::TransformComponent>();

  if (!attacker_uses_rts_lock || !target_uses_rts_lock) {
    if ((att_t != nullptr) && (tgt_t != nullptr)) {
      face_target(att_t, tgt_t);
    }
    begin_attack_animation(attacker);
    return;
  }

  auto* target_atk = target->get_component<Engine::Core::AttackComponent>();
  if (attack_comp->in_melee_lock &&
      attack_comp->melee_lock_target_id == target->get_id()) {
    begin_attack_animation(attacker, true);
    if (target_atk != nullptr) {
      auto* existing_target = world->get_entity(target_atk->melee_lock_target_id);
      auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
      bool const has_valid_existing_lock =
          target_atk->in_melee_lock && existing_target != nullptr &&
          target_unit != nullptr &&
          is_valid_enemy_unit(target_unit, existing_target, true);
      if (!has_valid_existing_lock ||
          target_atk->melee_lock_target_id == attacker->get_id()) {
        target_atk->in_melee_lock = true;
        target_atk->melee_lock_target_id = attacker->get_id();
      }
    }
    if (att_t != nullptr && tgt_t != nullptr) {
      lock_combatant_facing(attacker, att_t, tgt_t);
    }
    return;
  }

  attack_comp->in_melee_lock = true;
  attack_comp->melee_lock_target_id = target->get_id();
  begin_attack_animation(attacker);

  if (target_atk != nullptr) {
    auto* existing_target = world->get_entity(target_atk->melee_lock_target_id);
    auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
    bool const has_valid_existing_lock =
        target_atk->in_melee_lock && existing_target != nullptr &&
        target_unit != nullptr &&
        is_valid_enemy_unit(target_unit, existing_target, true);
    if (!has_valid_existing_lock) {
      target_atk->in_melee_lock = true;
      target_atk->melee_lock_target_id = attacker->get_id();
    }
  }

  if ((att_t != nullptr) && (tgt_t != nullptr)) {
    lock_combatant_facing(attacker, att_t, tgt_t);
    auto* target_atk_after = target->get_component<Engine::Core::AttackComponent>();
    bool const reciprocal_lock =
        (target_atk_after != nullptr) && target_atk_after->in_melee_lock &&
        target_atk_after->melee_lock_target_id == attacker->get_id();
    if (reciprocal_lock || !has_valid_melee_lock(target, world)) {
      lock_combatant_facing(target, tgt_t, att_t);
    }
  }
}

auto is_formation_reserve(Engine::Core::Entity* entity,
                          Engine::Core::World* world) -> bool {
  auto const* mode = entity->get_component<Engine::Core::FormationModeComponent>();
  if (mode == nullptr || !mode->active || mode->formation_id == 0 ||
      mode->stable_rank < 0) {
    return false;
  }
  int front_rank = mode->stable_rank;
  for (auto* member :
       world->get_entities_with<Engine::Core::FormationModeComponent>()) {
    auto const* member_mode =
        member->get_component<Engine::Core::FormationModeComponent>();
    auto const* member_unit = member->get_component<Engine::Core::UnitComponent>();
    if (member_mode != nullptr && member_unit != nullptr && member_unit->health > 0 &&
        member_mode->active && member_mode->formation_id == mode->formation_id &&
        member_mode->stable_rank >= 0) {
      front_rank = std::min(front_rank, member_mode->stable_rank);
    }
  }
  return mode->stable_rank > front_rank;
}

void begin_rts_melee_action(Engine::Core::Entity* attacker,
                            Engine::Core::Entity* target,
                            int damage,
                            float duration) {
  auto* unit = attacker->get_component<Engine::Core::UnitComponent>();
  auto* action =
      Engine::Core::get_or_add_component<Engine::Core::RpgCommanderActionComponent>(
          attacker);
  if (unit == nullptr || action == nullptr) {
    return;
  }
  auto const family = Engine::Core::resolve_combat_attack_family(
      unit->spawn_type, Engine::Core::AttackComponent::CombatMode::Melee);
  auto const id = family == Engine::Core::CombatAttackFamily::Spear
                      ? Game::Systems::CombatActions::CombatActionId::RtsSpearThrust
                      : Game::Systems::CombatActions::CombatActionId::RtsSwordStrike;
  action->phase = Engine::Core::RpgCommanderActionPhase::Strike;
  action->combat_action_id = static_cast<std::uint8_t>(id);
  action->active_target_id = target->get_id();
  action->requested_damage = damage;
  action->action_duration = std::max(0.001F, duration);
  Game::Systems::CombatActions::reset_combat_action_event_runtime(*action);
}

void begin_rts_bow_action(Engine::Core::Entity* attacker,
                          Engine::Core::Entity* target,
                          int damage,
                          float duration) {
  auto* action =
      Engine::Core::get_or_add_component<Engine::Core::RpgCommanderActionComponent>(
          attacker);
  if (action == nullptr) {
    return;
  }
  action->phase = Engine::Core::RpgCommanderActionPhase::Strike;
  action->combat_action_id = static_cast<std::uint8_t>(
      Game::Systems::CombatActions::CombatActionId::RtsBowShot);
  action->active_target_id = target->get_id();
  action->requested_damage = damage;
  action->action_duration = std::max(0.001F, duration);
  Game::Systems::CombatActions::reset_combat_action_event_runtime(*action);
}

} // namespace

bool release_rts_arrow_volley(Engine::Core::World& world,
                              Engine::Core::Entity& attacker,
                              Engine::Core::EntityID target_id,
                              int damage) {
  auto* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  auto* target = world.get_entity(target_id);
  if (attacker_unit == nullptr || attacker_unit->health <= 0 ||
      !is_valid_enemy_unit(attacker_unit, target, true)) {
    return false;
  }
  if (auto* arrow_system = world.get_system<ArrowSystem>()) {
    spawn_arrows(&attacker, target, arrow_system);
  }
  deal_damage(&world, target, std::max(1, damage), attacker.get_id());
  return true;
}

void process_attacks(Engine::Core::World* world,
                     const CombatQueryContext& query_context,
                     float delta_time) {
  auto const& units = query_context.units;
  auto* arrow_sys = world->get_system<ArrowSystem>();
  auto* projectile_sys = world->get_system<ProjectileSystem>();
  std::vector<CommandService::MoveIntent> chase_move_intents;
  chase_move_intents.reserve(units.size());

  for (auto* attacker : units) {
    if (attacker->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    if (attacker->has_component<Engine::Core::StaggerComponent>()) {
      continue;
    }

    auto* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
    auto* attacker_transform =
        attacker->get_component<Engine::Core::TransformComponent>();
    auto* attacker_atk = attacker->get_component<Engine::Core::AttackComponent>();

    if ((attacker_unit == nullptr) || (attacker_transform == nullptr)) {
      continue;
    }

    if (attacker_unit->health <= 0) {
      continue;
    }

    if (is_formation_reserve(attacker, world)) {
      Game::Systems::CombatRules::clear_rts_combat_tracking(attacker);
      continue;
    }

    auto const* pending_charge =
        attacker->get_component<Engine::Core::MountedChargeComponent>();
    bool const resolving_charge_impact =
        pending_charge != nullptr &&
        (pending_charge->state == Engine::Core::MountedChargeState::Charging ||
         pending_charge->state == Engine::Core::MountedChargeState::ImpactActive);
    if (!resolving_charge_impact) {
      process_melee_lock(attacker, attacker_atk, world, delta_time);
    }
    sync_melee_lock_target(attacker, attacker_atk);

    float range = 2.0F;
    int damage = 10;
    float cooldown = 1.0F;
    float* t_accum = nullptr;
    float tmp_accum = 0.0F;

    if (attacker_atk != nullptr) {
      update_combat_mode(attacker, world, attacker_atk);

      range = attacker_atk->get_current_range();
      damage = attacker_atk->get_current_damage();
      cooldown = attacker_atk->get_current_cooldown();

      apply_hold_mode_bonuses(attacker, attacker_unit, range, damage);

      attacker_atk->time_since_last += delta_time;
      t_accum = &attacker_atk->time_since_last;
    } else {
      tmp_accum += delta_time;
      t_accum = &tmp_accum;
    }

    bool const attack_ready = *t_accum >= cooldown;

    if (attack_ready && should_prioritize_healing(attacker, world)) {
      continue;
    }

    bool const in_melee_lock =
        (attacker_atk != nullptr) && attacker_atk->in_melee_lock &&
        Game::Systems::CombatRules::participates_in_rts_melee_lock(attacker);

    auto* attack_target =
        !Game::Systems::CombatRules::uses_rpg_combat_rules(attacker)
            ? attacker->get_component<Engine::Core::AttackTargetComponent>()
            : nullptr;
    bool const suppress_opportunistic_combat =
        suppresses_opportunistic_combat(attacker);
    if (suppress_opportunistic_combat && (attack_target != nullptr)) {
      attacker->remove_component<Engine::Core::AttackTargetComponent>();
      attack_target = nullptr;
    }
    Engine::Core::Entity* best_target = nullptr;
    Engine::Core::UnitComponent* best_target_unit = nullptr;
    Engine::Core::TransformComponent* best_target_transform = nullptr;

    if (in_melee_lock) {
      best_target = locked_target_for_attack(attacker, attacker_atk, world);
      if (best_target != nullptr) {
        best_target_unit = best_target->get_component<Engine::Core::UnitComponent>();
        best_target_transform =
            best_target->get_component<Engine::Core::TransformComponent>();
      }
    }

    if ((best_target == nullptr) && (attack_target != nullptr) &&
        attack_target->target_id != 0) {
      auto* target = world->get_entity(attack_target->target_id);
      auto* target_unit = is_valid_enemy_unit(attacker_unit, target, true)
                              ? target->get_component<Engine::Core::UnitComponent>()
                              : nullptr;
      auto* target_transform =
          (target_unit != nullptr)
              ? target->get_component<Engine::Core::TransformComponent>()
              : nullptr;
      bool const target_is_building = (target != nullptr) && is_building(target);

      if ((target_unit != nullptr) && (target_transform != nullptr)) {
        bool const ranged_unit = is_ranged_mode(attacker_atk);

        if (is_in_range(attacker, target, range)) {
          best_target = target;
          best_target_unit = target_unit;
          best_target_transform = target_transform;
          stop_unit_movement(attacker, attacker_transform);
          if (!in_melee_lock) {
            face_target(attacker_transform, target_transform);
          }
        } else {
          if (!attack_target->should_chase) {
            attacker->remove_component<Engine::Core::AttackTargetComponent>();
            continue;
          }

          auto* hold_mode = attacker->get_component<Engine::Core::HoldModeComponent>();
          if ((hold_mode != nullptr) && hold_mode->active) {
            attacker->remove_component<Engine::Core::AttackTargetComponent>();
            continue;
          }

          auto* guard_mode =
              attacker->get_component<Engine::Core::GuardModeComponent>();
          if ((guard_mode != nullptr) && guard_mode->active) {
            float guard_x = guard_mode->guard_position_x;
            float guard_z = guard_mode->guard_position_z;
            if (guard_mode->guarded_entity_id != 0) {
              auto* guarded_entity = world->get_entity(guard_mode->guarded_entity_id);
              if (guarded_entity != nullptr) {
                auto* guarded_transform =
                    guarded_entity->get_component<Engine::Core::TransformComponent>();
                if (guarded_transform != nullptr) {
                  guard_x = guarded_transform->position.x;
                  guard_z = guarded_transform->position.z;
                }
              }
            }

            float const dx = target_transform->position.x - guard_x;
            float const dz = target_transform->position.z - guard_z;
            float const dist_sq = dx * dx + dz * dz;
            float const guard_radius_sq =
                guard_mode->guard_radius * guard_mode->guard_radius;
            if (dist_sq > guard_radius_sq) {
              attacker->remove_component<Engine::Core::AttackTargetComponent>();
              continue;
            }
          }

          QVector3D const attacker_pos(
              attacker_transform->position.x, 0.0F, attacker_transform->position.z);
          QVector3D const target_pos(
              target_transform->position.x, 0.0F, target_transform->position.z);
          QVector3D desired_pos = target_pos;
          bool hold_position = false;

          float const spread_angle = chase_spread_angle(attacker->get_id());
          QVector3D const direction = target_pos - attacker_pos;
          float const distance_sq = direction.lengthSquared();
          float const distance =
              distance_sq > 0.000001F ? std::sqrt(distance_sq) : 0.0F;

          if (target_is_building ||
              target->has_component<Engine::Core::ElephantComponent>()) {
            float const target_radius = combat_radius(target);
            if (distance > 0.0F) {
              float const desired_distance =
                  target_radius + std::max(range - 0.2F, 0.2F);
              if (distance > desired_distance + 0.15F) {
                desired_pos = chase_destination(
                    attacker_pos, target_pos, desired_distance, spread_angle);
              } else {
                hold_position = true;
              }
            }
          } else if (ranged_unit) {
            if (distance > 0.0F) {
              float const optimal_range = range * Constants::k_optimal_range_factor;
              if (distance > optimal_range + Constants::k_optimal_range_buffer) {

                float const ranged_approach_angle =
                    FormationCombat::has_formation_slots(*attacker) ? 0.0F
                                                                    : spread_angle;
                desired_pos = chase_destination(
                    attacker_pos, target_pos, optimal_range, ranged_approach_angle);
              } else {
                hold_position = true;
              }
            }
          } else {
            if (distance > 0.0F) {
              auto const geometry =
                  FormationCombat::contact_geometry(*attacker, *target);
              float const desired_distance = geometry.uses_formation_slots
                                                 ? geometry.engagement_center_distance
                                                 : std::max(range - 0.2F, 0.2F);
              if (geometry.uses_formation_slots ? !FormationCombat::contact_is_active(
                                                      *attacker, *target, geometry)
                                                : distance > desired_distance + 0.15F) {
                desired_pos = chase_destination(
                    attacker_pos,
                    target_pos,
                    desired_distance,
                    geometry.uses_formation_slots ? 0.0F : spread_angle);
              } else {
                hold_position = true;
              }
            }
          }

          auto* movement = attacker->get_component<Engine::Core::MovementComponent>();
          if (movement == nullptr) {
            movement = attacker->add_component<Engine::Core::MovementComponent>();
          }

          if (movement != nullptr) {
            if (hold_position) {
              movement->stop();
              movement->set_rest_position(attacker_transform->position.x,
                                          attacker_transform->position.z);
            } else if (should_queue_chase_command(attacker,
                                                  target,
                                                  attacker_transform,
                                                  movement,
                                                  desired_pos,
                                                  range)) {
              chase_move_intents.push_back({attacker->get_id(), desired_pos});
            }
          }

          if (is_in_range(attacker, target, range)) {
            best_target = target;
            best_target_unit = target_unit;
            best_target_transform = target_transform;
          }
        }
      } else {
        attacker->remove_component<Engine::Core::AttackTargetComponent>();
      }
    }

    bool const has_attack_target =
        attacker->has_component<Engine::Core::AttackTargetComponent>();
    if ((best_target == nullptr) && !has_attack_target &&
        !suppress_opportunistic_combat) {
      if (Game::Systems::CombatRules::participates_in_rts_melee_lock(attacker)) {
        best_target = find_nearest_enemy(attacker, query_context, range);
        if (best_target != nullptr) {
          best_target_unit = best_target->get_component<Engine::Core::UnitComponent>();
          best_target_transform =
              best_target->get_component<Engine::Core::TransformComponent>();
        }
      }
    }

    if ((best_target != nullptr) && (best_target_unit != nullptr) &&
        (best_target_transform != nullptr)) {
      if (auto* movement = attacker->get_component<Engine::Core::MovementComponent>();
          movement != nullptr) {
        OrderService::clear_player_order_intent(attacker);
      }
      if (!attacker->has_component<Engine::Core::AttackTargetComponent>()) {
        auto* new_target =
            attacker->add_component<Engine::Core::AttackTargetComponent>();
        new_target->target_id = best_target->get_id();
        new_target->should_chase = false;
      } else {
        auto* existing_target =
            attacker->get_component<Engine::Core::AttackTargetComponent>();
        if (existing_target->target_id != best_target->get_id()) {
          existing_target->target_id = best_target->get_id();
          existing_target->should_chase = false;
        }
      }

      bool const ranged_unit = is_ranged_mode(attacker_atk);

      if (ranged_unit) {
        stop_unit_movement(attacker, attacker_transform);
      }

      if (!in_melee_lock) {
        face_target(attacker_transform, best_target_transform);
      }

      auto const* mounted_charge =
          attacker->get_component<Engine::Core::MountedChargeComponent>();
      bool const charge_impact_pending =
          mounted_charge != nullptr &&
          (mounted_charge->state == Engine::Core::MountedChargeState::Charging ||
           mounted_charge->state == Engine::Core::MountedChargeState::ImpactActive);
      if (charge_impact_pending) {

        continue;
      }

      if (!attack_ready) {
        continue;
      }

      if (ranged_unit) {
        begin_attack_animation(attacker);
      }

      bool const should_show_arrow_vfx =
          attacker_unit->spawn_type != Game::Units::SpawnType::Catapult &&
          attacker_unit->spawn_type != Game::Units::SpawnType::Ballista;

      if ((attacker_atk != nullptr) &&
          attacker_atk->current_mode ==
              Engine::Core::AttackComponent::CombatMode::Melee) {

        if (melee_strike_blocked_by_commander_ring(attacker, best_target)) {
          continue;
        }

        initiate_melee_combat(attacker, best_target, attacker_atk, world);
      }

      float const tactical_multiplier = calculate_tactical_damage_multiplier(
          attacker, best_target, attacker_unit, best_target_unit);
      damage = static_cast<int>(static_cast<float>(damage) * tactical_multiplier);
      apply_high_ground_defense_bonuses(
          attacker, best_target, best_target_unit, damage);

      auto* special_attack =
          attacker->get_component<Engine::Core::SpecialAttackComponent>();
      bool const use_special_projectile = ranged_unit && special_attack != nullptr &&
                                          special_attack->use_projectile_system &&
                                          projectile_sys != nullptr;
      bool const use_rts_bow_action =
          ranged_unit && !use_special_projectile &&
          (attacker_unit->spawn_type == Game::Units::SpawnType::Archer ||
           attacker_unit->spawn_type == Game::Units::SpawnType::HorseArcher);

      if (should_show_arrow_vfx &&
          ((attacker_atk == nullptr) ||
           attacker_atk->current_mode !=
               Engine::Core::AttackComponent::CombatMode::Melee) &&
          !use_special_projectile && !use_rts_bow_action) {
        spawn_arrows(attacker, best_target, arrow_sys);
      }

      bool const is_melee_attack = (attacker_atk != nullptr) &&
                                   attacker_atk->current_mode ==
                                       Engine::Core::AttackComponent::CombatMode::Melee;
      auto const* attacker_commander =
          attacker->get_component<Engine::Core::CommanderComponent>();
      bool const fpv_commander =
          (attacker_commander != nullptr) && attacker_commander->fpv_controlled;
      bool const defer_melee_strike =
          is_melee_attack && !fpv_commander && cooldown > 0.001F;

      if (use_special_projectile) {
        launch_special_projectile(attacker, best_target, damage, projectile_sys);
      } else if (use_rts_bow_action) {
        begin_rts_bow_action(attacker, best_target, damage, cooldown);
      } else if (defer_melee_strike) {
        begin_rts_melee_action(attacker, best_target, damage, cooldown);
      } else {
        if (Game::Systems::CombatRules::uses_rpg_combat_rules(best_target)) {
          Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
              world, best_target, damage, attacker->get_id());
        } else {
          deal_damage(world, best_target, damage, attacker->get_id());
        }
      }
      *t_accum = -deterministic_attack_delay(
          attacker->get_id(), best_target->get_id(), cooldown);

      auto* guard_mode = attacker->get_component<Engine::Core::GuardModeComponent>();
      if ((guard_mode != nullptr) && guard_mode->active) {
        guard_mode->returning_to_guard_position = false;
      }
    } else {
      if (Game::Systems::CombatRules::participates_in_rts_melee_lock(attacker) &&
          (attack_target == nullptr) &&
          attacker->has_component<Engine::Core::AttackTargetComponent>()) {
        attacker->remove_component<Engine::Core::AttackTargetComponent>();
      }

      auto* guard_mode = attacker->get_component<Engine::Core::GuardModeComponent>();
      if ((guard_mode != nullptr) && guard_mode->active &&
          !guard_mode->returning_to_guard_position) {
        float guard_x = guard_mode->guard_position_x;
        float guard_z = guard_mode->guard_position_z;

        if (guard_mode->guarded_entity_id != 0) {
          auto* guarded_entity = world->get_entity(guard_mode->guarded_entity_id);
          if (guarded_entity != nullptr) {
            auto* guarded_transform =
                guarded_entity->get_component<Engine::Core::TransformComponent>();
            if (guarded_transform != nullptr) {
              guard_x = guarded_transform->position.x;
              guard_z = guarded_transform->position.z;
            }
          }
        }

        float const dx = guard_x - attacker_transform->position.x;
        float const dz = guard_z - attacker_transform->position.z;
        float const dist_sq = dx * dx + dz * dz;

        float const k_return_threshold_sq =
            Engine::Core::Defaults::k_guard_return_threshold *
            Engine::Core::Defaults::k_guard_return_threshold;
        if (dist_sq > k_return_threshold_sq) {
          guard_mode->returning_to_guard_position = true;
          CommandService::MoveOptions options;
          options.kind = MoveOrderKind::GuardReturn;
          CommandService::move_unit(
              *world, attacker->get_id(), QVector3D(guard_x, 0.0F, guard_z), options);
        }
      }
    }
  }

  if (!chase_move_intents.empty()) {
    CommandService::MoveOptions options;
    options.kind = MoveOrderKind::AttackChase;
    CommandService::move_units(*world, chase_move_intents, options);
  }
}

} // namespace Game::Systems::Combat
