#include "combat_hit_resolver.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../combat_rules.h"
#include "../rpg_combat_system/rpg_commander_damage.h"
#include "damage_application.h"
#include "mounted_charge_processor.h"

namespace Game::Systems::Combat {

namespace {

using CSC = Engine::Core::CombatStateComponent;

constexpr float k_mounted_charge_knockdown_speed = 6.0F;
constexpr float k_mounted_charge_knockback_speed = 3.5F;
constexpr float k_initial_burning_visual_age = 0.08F;
constexpr float k_min_projectile_area_radius = 0.1F;
constexpr float k_min_fire_patch_radius = 0.5F;
constexpr float k_fire_patch_ground_offset = 0.85F;

[[nodiscard]] auto commander_action_raw_damage(
    Engine::Core::Entity& attacker,
    const Game::Systems::CombatActions::DamageProfile& damage_profile) -> int {
  int damage = 10;
  if (auto const* attack = attacker.get_component<Engine::Core::AttackComponent>()) {
    damage = std::max(1, attack->get_current_damage());
  }

  auto const* stamina = attacker.get_component<Engine::Core::StaminaComponent>();
  if (stamina != nullptr && stamina->stamina < CSC::k_low_stamina_threshold) {
    damage = static_cast<int>(static_cast<float>(damage) *
                              CSC::k_low_stamina_damage_penalty);
    damage = std::max(1, damage);
  }

  return std::max(
      1,
      static_cast<int>(std::round(static_cast<float>(damage) *
                                  std::max(0.0F, damage_profile.base_multiplier))));
}

[[nodiscard]] auto commander_damage_profile(
    const Game::Systems::CombatActions::DamageProfile& damage_profile)
    -> Game::Systems::RpgCombat::CommanderDamageProfile {
  return {.posture_damage = damage_profile.posture_damage,
          .guard_pressure = damage_profile.guard_pressure};
}

[[nodiscard]] auto
motion_velocity(const Engine::Core::Entity& entity) -> std::pair<float, float> {
  auto const* motion =
      entity.get_component<Engine::Core::MotionPresentationComponent>();
  if (motion == nullptr || !motion->has_velocity) {
    return {0.0F, 0.0F};
  }
  return {motion->velocity_x, motion->velocity_z};
}

[[nodiscard]] auto relative_contact_speed(const Engine::Core::Entity& attacker,
                                          const Engine::Core::Entity& target) -> float {
  auto const [attacker_vx, attacker_vz] = motion_velocity(attacker);
  auto const [target_vx, target_vz] = motion_velocity(target);
  float const rel_x = attacker_vx - target_vx;
  float const rel_z = attacker_vz - target_vz;
  return std::sqrt(rel_x * rel_x + rel_z * rel_z);
}

[[nodiscard]] auto is_mounted_target(const Engine::Core::Entity& target) -> bool {
  auto const* unit = target.get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && Game::Units::is_cavalry(unit->spawn_type);
}

[[nodiscard]] auto resolve_special_attack(Engine::Core::World* world,
                                          Engine::Core::EntityID attacker_id)
    -> const Engine::Core::SpecialAttackComponent* {
  if (world == nullptr || attacker_id == 0) {
    return nullptr;
  }

  auto* attacker = world->get_entity(attacker_id);
  return attacker != nullptr
             ? attacker->get_component<Engine::Core::SpecialAttackComponent>()
             : nullptr;
}

[[nodiscard]] auto
projectile_attacker_owner_id(Engine::Core::World* world,
                             Engine::Core::EntityID attacker_id) -> int {
  if (world == nullptr || attacker_id == 0) {
    return 0;
  }

  auto* attacker = world->get_entity(attacker_id);
  if (attacker == nullptr) {
    return 0;
  }

  auto const* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
  return attacker_unit != nullptr ? attacker_unit->owner_id : 0;
}

[[nodiscard]] auto target_is_undead(const Engine::Core::Entity& entity) -> bool {
  return entity.has_component<Engine::Core::UndeadComponent>();
}

[[nodiscard]] auto
can_affect_with_fire(int attacker_owner_id,
                     bool friendly_fire,
                     Engine::Core::EntityID attacker_id,
                     const Engine::Core::Entity* candidate,
                     const Engine::Core::UnitComponent* candidate_unit) -> bool {
  if (candidate == nullptr || candidate_unit == nullptr ||
      candidate_unit->health <= 0) {
    return false;
  }
  if (candidate->get_id() == attacker_id) {
    return false;
  }
  if (!friendly_fire && attacker_owner_id != 0 &&
      candidate_unit->owner_id == attacker_owner_id) {
    return false;
  }
  return true;
}

void apply_cursed_projectile_status_if_needed(
    Engine::Core::Entity& target,
    const Engine::Core::SpecialAttackComponent* special_attack,
    const CombatHitContact& contact) {
  if (special_attack == nullptr ||
      contact.projectile_kind != Game::Systems::ProjectileKind::CursedArrow ||
      target_is_undead(target) || special_attack->cursed_stacks_per_hit <= 0) {
    return;
  }

  auto* target_unit = target.get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    return;
  }

  auto* morale = target.get_component<Engine::Core::MoraleComponent>();
  if (morale == nullptr) {
    morale = target.add_component<Engine::Core::MoraleComponent>();
  }

  auto* cursed = target.get_component<Engine::Core::CursedStatusComponent>();
  bool created_cursed = false;
  if (cursed == nullptr) {
    cursed = target.add_component<Engine::Core::CursedStatusComponent>();
    created_cursed = cursed != nullptr;
  }
  if (morale == nullptr || cursed == nullptr) {
    return;
  }
  if (created_cursed) {
    cursed->stacks = 0;
  }

  cursed->morale_penalty_per_hit = special_attack->cursed_morale_penalty_per_hit;
  cursed->duration = special_attack->cursed_duration;
  cursed->remaining_duration = special_attack->cursed_duration;
  cursed->stacks += special_attack->cursed_stacks_per_hit;

  morale->morale -= special_attack->cursed_morale_penalty_per_hit *
                    static_cast<float>(special_attack->cursed_stacks_per_hit);
  Engine::Core::refresh_morale_state(*morale);
}

void apply_fireball_burning_status_if_needed(
    Engine::Core::Entity& target,
    const Engine::Core::SpecialAttackComponent* special_attack,
    const CombatHitContact& contact) {
  if (special_attack == nullptr ||
      contact.projectile_kind != Game::Systems::ProjectileKind::Fireball ||
      special_attack->burn_duration <= 0.0F ||
      special_attack->burn_damage_per_tick <= 0) {
    return;
  }

  auto* target_unit = target.get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    return;
  }

  auto* burning = target.get_component<Engine::Core::BurningStatusComponent>();
  if (burning == nullptr) {
    burning = target.add_component<Engine::Core::BurningStatusComponent>();
    if (burning != nullptr) {
      burning->duration = special_attack->burn_duration;
      burning->remaining_duration = special_attack->burn_duration;
      burning->ignition_elapsed = k_initial_burning_visual_age;
      burning->tick_interval = special_attack->burn_tick_interval;
      burning->damage_per_tick = special_attack->burn_damage_per_tick;
      burning->fire_bonus_multiplier =
          special_attack->bonus_damage_multiplier_vs_fire_vulnerable;
    }
  }
  if (burning == nullptr) {
    return;
  }

  burning->duration = std::max(burning->duration, special_attack->burn_duration);
  burning->remaining_duration =
      std::max(burning->remaining_duration, special_attack->burn_duration);
  burning->tick_interval = special_attack->burn_tick_interval;
  burning->damage_per_tick =
      std::max(burning->damage_per_tick, special_attack->burn_damage_per_tick);
  burning->attacker_id = contact.attacker_id;
  burning->fire_bonus_multiplier =
      std::max(burning->fire_bonus_multiplier,
               special_attack->bonus_damage_multiplier_vs_fire_vulnerable);
}

void apply_projectile_impact_effects_if_needed(
    Engine::Core::Entity& target,
    const Engine::Core::SpecialAttackComponent* special_attack,
    const CombatHitContact& contact) {
  apply_cursed_projectile_status_if_needed(target, special_attack, contact);
  apply_fireball_burning_status_if_needed(target, special_attack, contact);
}

[[nodiscard]] auto refresh_burning_status_from_fire_patch(
    Engine::Core::Entity& target,
    const Engine::Core::FirePatchComponent& fire_patch) -> bool {
  if (fire_patch.burn_damage_per_tick <= 0 || fire_patch.burn_duration <= 0.0F) {
    return false;
  }

  auto* target_unit = target.get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    return false;
  }

  auto* burning = target.get_component<Engine::Core::BurningStatusComponent>();
  if (burning == nullptr) {
    burning = target.add_component<Engine::Core::BurningStatusComponent>();
    if (burning != nullptr) {
      burning->duration = fire_patch.burn_duration;
      burning->remaining_duration = fire_patch.burn_duration;
      burning->ignition_elapsed = k_initial_burning_visual_age;
      burning->tick_interval = fire_patch.burn_tick_interval;
      burning->damage_per_tick = fire_patch.burn_damage_per_tick;
      burning->fire_bonus_multiplier = fire_patch.fire_bonus_multiplier;
    }
  }
  if (burning == nullptr) {
    return false;
  }

  burning->duration = std::max(burning->duration, fire_patch.burn_duration);
  burning->remaining_duration =
      std::max(burning->remaining_duration, fire_patch.burn_duration);
  burning->tick_interval = fire_patch.burn_tick_interval;
  burning->damage_per_tick =
      std::max(burning->damage_per_tick, fire_patch.burn_damage_per_tick);
  burning->attacker_id = fire_patch.attacker_id;
  burning->fire_bonus_multiplier =
      std::max(burning->fire_bonus_multiplier, fire_patch.fire_bonus_multiplier);
  return true;
}

[[nodiscard]] auto spawn_fire_patch_for_projectile_impact(
    Engine::Core::World* world,
    const ProjectileAreaImpactRequest& request,
    const Engine::Core::SpecialAttackComponent* special_attack,
    int attacker_owner_id) -> bool {
  if (world == nullptr || special_attack == nullptr ||
      special_attack->fire_patch_duration <= 0.0F ||
      special_attack->burn_duration <= 0.0F ||
      special_attack->burn_damage_per_tick <= 0) {
    return false;
  }

  auto* fire_patch_entity = world->create_entity();
  if (fire_patch_entity == nullptr) {
    return false;
  }

  QVector3D const impact_pos = request.impact_point;
  fire_patch_entity->add_component<Engine::Core::TransformComponent>(
      impact_pos.x(), impact_pos.y(), impact_pos.z());
  auto* fire_patch =
      fire_patch_entity->add_component<Engine::Core::FirePatchComponent>();
  if (fire_patch == nullptr) {
    fire_patch_entity->add_component<Engine::Core::PendingRemovalComponent>();
    return false;
  }

  float const ground_y = std::max(0.0F, impact_pos.y() - k_fire_patch_ground_offset);
  auto* fire_patch_transform =
      fire_patch_entity->get_component<Engine::Core::TransformComponent>();
  if (fire_patch_transform != nullptr) {
    fire_patch_transform->position.y = ground_y;
  }

  fire_patch->radius = std::max(k_min_fire_patch_radius,
                                special_attack->fire_patch_radius > 0.0F
                                    ? special_attack->fire_patch_radius
                                    : special_attack->splash_radius);
  fire_patch->duration = special_attack->fire_patch_duration;
  fire_patch->remaining_duration = special_attack->fire_patch_duration;
  fire_patch->burn_duration = special_attack->burn_duration;
  fire_patch->burn_tick_interval = special_attack->burn_tick_interval;
  fire_patch->burn_damage_per_tick = special_attack->burn_damage_per_tick;
  fire_patch->attacker_owner_id = attacker_owner_id;
  fire_patch->attacker_id = request.attacker_id;
  fire_patch->friendly_fire = request.friendly_fire;
  fire_patch->fire_bonus_multiplier =
      special_attack->bonus_damage_multiplier_vs_fire_vulnerable;
  return true;
}

[[nodiscard]] auto is_spear_brace_contact(const CombatHitContact& contact) -> bool {
  return contact.weapon_family == Game::Systems::CombatActions::WeaponFamily::Spear &&
         contact.attack_direction == Engine::Core::AttackDirection::Thrust;
}

[[nodiscard]] auto
spear_brace_interrupt_speed(const Engine::Core::Entity& attacker) -> float {
  auto const* brace = attacker.get_component<Engine::Core::SpearBraceComponent>();
  if (brace != nullptr && brace->active) {
    return std::max(0.0F, brace->min_interrupt_speed);
  }

  return std::numeric_limits<float>::infinity();
}

auto apply_cavalry_interrupt_if_needed(const Engine::Core::Entity& attacker,
                                       Engine::Core::Entity& target,
                                       const CombatHitContact& contact) -> bool {
  float const interrupt_speed = spear_brace_interrupt_speed(attacker);
  if (!is_spear_brace_contact(contact) || !is_mounted_target(target) ||
      contact.relative_speed < interrupt_speed) {
    return false;
  }

  Game::Systems::Combat::add_or_extend_stagger(
      &target,
      Engine::Core::HitFeedbackComponent::duration_for_tier(
          Engine::Core::StaggerTier::Knockdown),
      Engine::Core::StaggerTier::Knockdown);

  auto* movement = target.get_component<Engine::Core::MovementComponent>();
  auto const* transform = target.get_component<Engine::Core::TransformComponent>();
  if (movement != nullptr) {
    movement->stop();
    if (transform != nullptr) {
      movement->set_rest_position(transform->position.x, transform->position.z);
    }
  }
  (void)cancel_mounted_charge(target,
                              Engine::Core::MountedChargeCancelReason::Interrupted);
  return true;
}

[[nodiscard]] auto is_mounted_charge_contact(const CombatHitContact& contact) -> bool {
  return contact.weapon_family == Game::Systems::CombatActions::WeaponFamily::Mount &&
         contact.action_id ==
             Game::Systems::CombatActions::CombatActionId::MountedChargeImpact;
}

auto apply_mounted_charge_stagger_if_needed(Engine::Core::Entity& target,
                                            const CombatHitContact& contact) -> bool {
  if (!is_mounted_charge_contact(contact) ||
      contact.relative_speed < k_mounted_charge_knockback_speed) {
    return false;
  }

  Engine::Core::StaggerTier tier = Engine::Core::StaggerTier::Knockback;
  if (contact.relative_speed >= k_mounted_charge_knockdown_speed) {
    tier = Engine::Core::StaggerTier::Knockdown;
  }

  Game::Systems::Combat::add_or_extend_stagger(
      &target, Engine::Core::HitFeedbackComponent::duration_for_tier(tier), tier);
  return true;
}

} // namespace

auto resolve_commander_action_hit(Engine::Core::World* world,
                                  const CombatHitRequest& request) -> CombatHitResult {
  CombatHitResult result;
  result.contact = request.contact;
  if (world == nullptr || request.contact.attacker_id == 0 ||
      request.contact.target_id == 0) {
    return result;
  }

  auto* attacker = world->get_entity(request.contact.attacker_id);
  auto* target = world->get_entity(request.contact.target_id);
  if (attacker == nullptr || target == nullptr) {
    return result;
  }

  if (result.contact.relative_speed <= 0.0F) {
    result.contact.relative_speed = relative_contact_speed(*attacker, *target);
  }
  result.attempted = true;
  result.raw_damage = commander_action_raw_damage(*attacker, request.damage_profile);
  result.damage = Game::Systems::RpgCombat::deal_commander_attack_damage(
      world,
      target,
      result.raw_damage,
      attacker->get_id(),
      commander_damage_profile(request.damage_profile));
  result.applied = result.damage.effective_damage > 0;
  result.interrupted_charge =
      apply_cavalry_interrupt_if_needed(*attacker, *target, result.contact);
  return result;
}

auto resolve_projectile_impact_hit(Engine::Core::World* world,
                                   const CombatHitRequest& request) -> CombatHitResult {
  CombatHitResult result;
  result.contact = request.contact;
  result.contact.from_projectile = true;
  if (world == nullptr || request.contact.target_id == 0 ||
      request.explicit_raw_damage <= 0) {
    return result;
  }

  auto* attacker = world->get_entity(request.contact.attacker_id);
  auto* target = world->get_entity(request.contact.target_id);
  if (target == nullptr) {
    return result;
  }
  auto const* special_attack =
      resolve_special_attack(world, request.contact.attacker_id);

  if (result.contact.relative_speed <= 0.0F && attacker != nullptr) {
    result.contact.relative_speed = relative_contact_speed(*attacker, *target);
  }
  result.attempted = true;
  result.raw_damage = std::max(1, request.explicit_raw_damage);

  if (Game::Systems::CombatRules::uses_rpg_combat_rules(target)) {
    result.damage = Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
        world,
        target,
        result.raw_damage,
        request.contact.attacker_id,
        commander_damage_profile(request.damage_profile));
    result.applied = result.damage.effective_damage > 0;
    apply_projectile_impact_effects_if_needed(*target, special_attack, result.contact);
    return result;
  }

  auto const application = Game::Systems::Combat::apply_unit_damage(
      world, target, result.raw_damage, request.contact.attacker_id);
  result.damage.effective_damage = application.applied_damage;
  result.damage.killed = application.killed;
  result.applied = application.applied_damage > 0;
  apply_projectile_impact_effects_if_needed(*target, special_attack, result.contact);
  return result;
}

auto resolve_projectile_area_impact_hit(Engine::Core::World* world,
                                        const ProjectileAreaImpactRequest& request)
    -> ProjectileAreaImpactResult {
  ProjectileAreaImpactResult result;
  if (world == nullptr || request.attacker_id == 0 ||
      request.explicit_raw_damage <= 0 || request.radius <= 0.0F) {
    return result;
  }

  auto const* special_attack = resolve_special_attack(world, request.attacker_id);
  int const attacker_owner = projectile_attacker_owner_id(world, request.attacker_id);
  float const radius = std::max(k_min_projectile_area_radius, request.radius);
  float const radius_sq = radius * radius;
  float const splash_multiplier = std::max(0.0F, request.splash_damage_multiplier);
  float const fire_bonus =
      special_attack != nullptr
          ? special_attack->bonus_damage_multiplier_vs_fire_vulnerable
          : 1.0F;

  for (auto* candidate : world->get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == nullptr ||
        candidate->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* candidate_unit = candidate->get_component<Engine::Core::UnitComponent>();
    auto* candidate_transform =
        candidate->get_component<Engine::Core::TransformComponent>();
    if (candidate_transform == nullptr || !can_affect_with_fire(attacker_owner,
                                                                request.friendly_fire,
                                                                request.attacker_id,
                                                                candidate,
                                                                candidate_unit)) {
      continue;
    }

    float const dx = candidate_transform->position.x - request.impact_point.x();
    float const dz = candidate_transform->position.z - request.impact_point.z();
    if ((dx * dx + dz * dz) > radius_sq) {
      continue;
    }

    int damage = request.explicit_raw_damage;
    if (candidate->get_id() != request.primary_target_id) {
      damage =
          static_cast<int>(std::round(static_cast<float>(damage) * splash_multiplier));
    }
    if (auto* undead = candidate->get_component<Engine::Core::UndeadComponent>();
        undead != nullptr) {
      damage = static_cast<int>(std::round(
          static_cast<float>(damage) * undead->fire_damage_multiplier * fire_bonus));
    }

    auto hit = resolve_projectile_impact_hit(
        world,
        {.contact = {.attacker_id = request.attacker_id,
                     .target_id = candidate->get_id(),
                     .weapon_family = Game::Systems::CombatActions::WeaponFamily::Bow,
                     .attack_family = Engine::Core::CombatAttackFamily::Bow,
                     .attack_direction = Engine::Core::AttackDirection::Thrust,
                     .contact_point = request.impact_point,
                     .from_projectile = true,
                     .projectile_kind = request.projectile_kind},
         .damage_profile = request.damage_profile,
         .explicit_raw_damage = std::max(1, damage)});
    if (hit.attempted) {
      ++result.attempted_hits;
    }
    if (hit.applied) {
      ++result.applied_hits;
    }
  }

  result.fire_patch_spawned = spawn_fire_patch_for_projectile_impact(
      world, request, special_attack, attacker_owner);
  return result;
}

auto apply_fire_patch_contact_effect(Engine::Core::World* world,
                                     Engine::Core::Entity& fire_patch_entity,
                                     Engine::Core::Entity& target) -> bool {
  (void)world;
  if (fire_patch_entity.has_component<Engine::Core::PendingRemovalComponent>() ||
      target.has_component<Engine::Core::PendingRemovalComponent>()) {
    return false;
  }

  auto* fire_patch =
      fire_patch_entity.get_component<Engine::Core::FirePatchComponent>();
  auto* fire_patch_transform =
      fire_patch_entity.get_component<Engine::Core::TransformComponent>();
  auto* target_unit = target.get_component<Engine::Core::UnitComponent>();
  auto* target_transform = target.get_component<Engine::Core::TransformComponent>();
  if (fire_patch == nullptr || fire_patch_transform == nullptr ||
      target_transform == nullptr ||
      !can_affect_with_fire(fire_patch->attacker_owner_id,
                            fire_patch->friendly_fire,
                            fire_patch->attacker_id,
                            &target,
                            target_unit)) {
    return false;
  }

  float const radius_sq = fire_patch->radius * fire_patch->radius;
  float const dx = target_transform->position.x - fire_patch_transform->position.x;
  float const dz = target_transform->position.z - fire_patch_transform->position.z;
  if ((dx * dx + dz * dz) > radius_sq) {
    return false;
  }

  return refresh_burning_status_from_fire_patch(target, *fire_patch);
}

auto resolve_mounted_charge_impact_hit(
    Engine::Core::World* world, const CombatHitRequest& request) -> CombatHitResult {
  CombatHitResult result;
  result.contact = request.contact;
  if (world == nullptr || request.contact.attacker_id == 0 ||
      request.contact.target_id == 0 || request.explicit_raw_damage <= 0) {
    return result;
  }

  auto* attacker = world->get_entity(request.contact.attacker_id);
  auto* target = world->get_entity(request.contact.target_id);
  if (attacker == nullptr || target == nullptr) {
    return result;
  }

  if (result.contact.relative_speed <= 0.0F) {
    result.contact.relative_speed = relative_contact_speed(*attacker, *target);
  }
  result.attempted = true;
  result.raw_damage =
      std::max(1,
               static_cast<int>(
                   std::round(static_cast<float>(request.explicit_raw_damage) *
                              std::max(0.0F, request.damage_profile.base_multiplier))));

  if (Game::Systems::CombatRules::uses_rpg_combat_rules(target)) {
    result.damage = Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
        world,
        target,
        result.raw_damage,
        attacker->get_id(),
        commander_damage_profile(request.damage_profile));
    result.applied = result.damage.effective_damage > 0;
  } else {
    auto const application = Game::Systems::Combat::apply_unit_damage(
        world, target, result.raw_damage, attacker->get_id());
    result.damage.effective_damage = application.applied_damage;
    result.damage.killed = application.killed;
    result.applied = application.applied_damage > 0;
  }

  result.interrupted_charge =
      apply_mounted_charge_stagger_if_needed(*target, result.contact);
  return result;
}

} // namespace Game::Systems::Combat
