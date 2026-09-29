#include "melee_lock.h"

#include <algorithm>
#include <cmath>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/troop_config.h"
#include "../../util/planar_math.h"
#include "../combat_rules.h"
#include "../formation_combat_geometry.h"
#include "../owner_registry.h"
#include "attack_control.h"
#include "combat_types.h"
#include "combat_utils.h"
#include "structure_combat.h"
#include "target_assignment.h"
#include "target_rules.h"
#include "threat_alert.h"

namespace Game::Systems::Combat {

namespace {

constexpr float k_lock_facing_turn_degrees_per_second = 360.0F;

[[nodiscard]] auto lock_facing_turn_rate(const Engine::Core::Entity* actor) -> float {
  auto const* unit =
      actor != nullptr ? actor->get_component<Engine::Core::UnitComponent>() : nullptr;
  if (unit == nullptr) {
    return k_lock_facing_turn_degrees_per_second;
  }

  return std::min(k_lock_facing_turn_degrees_per_second,
                  Game::Units::body_turn_speed_degrees(unit->spawn_type));
}

void lock_facing(Engine::Core::TransformComponent* actor_transform,
                 Engine::Core::TransformComponent* target_transform,
                 float turn_rate_degrees,
                 float delta_time) {
  if (actor_transform == nullptr || target_transform == nullptr) {
    return;
  }
  float const dx = target_transform->position.x - actor_transform->position.x;
  float const dz = target_transform->position.z - actor_transform->position.z;
  if (dx * dx + dz * dz > 0.000001F) {
    actor_transform->rotation.y = Game::Systems::turn_yaw_toward(
        actor_transform->rotation.y,
        Game::Systems::yaw_degrees_from_direction(dx, dz),
        turn_rate_degrees * std::max(0.0F, delta_time));
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

[[nodiscard]] auto steers_its_own_heading(const Engine::Core::Entity* entity) -> bool {
  if (entity == nullptr) {
    return false;
  }
  if (auto const* transform = entity->get_component<Engine::Core::TransformComponent>();
      transform != nullptr && transform->has_desired_yaw) {
    return true;
  }
  auto const* movement = entity->get_component<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    return false;
  }
  if (movement->get_has_target()) {
    return true;
  }
  constexpr float k_under_way_speed = 0.2F;
  float const vx = movement->get_vx();
  float const vz = movement->get_vz();
  return (vx * vx) + (vz * vz) > k_under_way_speed * k_under_way_speed;
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
  return may_attack(
      unit, target, {.intent = EngagementIntent::Ordered, .allow_buildings = true});
}

void lock_combatant_facing(Engine::Core::Entity* actor,
                           Engine::Core::TransformComponent* actor_transform,
                           Engine::Core::TransformComponent* target_transform,
                           float delta_time,
                           FacingLedger& ledger) {
  if (is_building(actor)) {
    return;
  }
  if (is_multi_body_formation(actor)) {

    actor_transform->desired_yaw = actor_transform->rotation.y;
    actor_transform->has_desired_yaw = false;
    return;
  }
  if (actor != nullptr && !ledger.claim(actor->get_id())) {
    return;
  }
  lock_facing(
      actor_transform, target_transform, lock_facing_turn_rate(actor), delta_time);
}

} // namespace

void release_structure_lock_for_troop_target(Engine::Core::Entity* attacker,
                                             Engine::Core::AttackComponent* attack_comp,
                                             Engine::Core::World* world) {
  if ((attack_comp == nullptr) || (world == nullptr) || !attack_comp->in_melee_lock ||
      attack_comp->melee_lock_target_id == 0) {
    return;
  }

  auto* lock_target = world->get_entity(attack_comp->melee_lock_target_id);
  if ((lock_target == nullptr) || !is_building(lock_target)) {
    return;
  }

  auto const* attack_target =
      world->try_get<Engine::Core::AttackTargetComponent>(attacker->get_id());
  if ((attack_target == nullptr) ||
      attack_target->target_id == attack_comp->melee_lock_target_id) {
    return;
  }

  auto* ordered_target = world->get_entity(attack_target->target_id);
  auto const* attacker_unit =
      world->try_get<Engine::Core::UnitComponent>(attacker->get_id());
  if (!may_attack(attacker_unit,
                  ordered_target,
                  {.intent = EngagementIntent::Ordered, .allow_buildings = false})) {
    return;
  }

  attack_comp->release_melee_lock();
}

void process_melee_lock(Engine::Core::Entity* attacker,
                        Engine::Core::AttackComponent* attack_comp,
                        Engine::Core::World* world,
                        float delta_time,
                        FacingLedger& ledger) {
  if (attack_comp == nullptr || !attack_comp->in_melee_lock) {
    return;
  }

  if (!Game::Systems::CombatRules::participates_in_rts_melee_lock(attacker)) {
    return;
  }

  auto* lock_target = world->get_entity(attack_comp->melee_lock_target_id);
  if ((lock_target == nullptr) ||
      lock_target->has_component<Engine::Core::PendingRemovalComponent>()) {
    attack_comp->release_melee_lock();
    return;
  }

  if (!Game::Systems::CombatRules::participates_in_rts_melee_lock(lock_target)) {
    return;
  }

  auto* lock_target_unit = lock_target->get_component<Engine::Core::UnitComponent>();
  if ((lock_target_unit == nullptr) || lock_target_unit->health <= 0) {
    attack_comp->release_melee_lock();
    return;
  }

  auto* att_t = attacker->get_component<Engine::Core::TransformComponent>();
  auto* tgt_t = lock_target->get_component<Engine::Core::TransformComponent>();
  if ((att_t == nullptr) || (tgt_t == nullptr)) {
    return;
  }

  auto* lock_target_atk = lock_target->get_component<Engine::Core::AttackComponent>();
  if (structure_separates_combatants(attacker, lock_target)) {
    attack_comp->release_melee_lock();
    if (lock_target_atk != nullptr &&
        lock_target_atk->melee_lock_target_id == attacker->get_id()) {
      lock_target_atk->release_melee_lock();
    }
    return;
  }

  lock_combatant_facing(attacker, att_t, tgt_t, delta_time, ledger);
  bool const reciprocal_lock = (lock_target_atk != nullptr) &&
                               lock_target_atk->melee_locked_on(attacker->get_id());

  if (!reciprocal_lock && !has_valid_melee_lock(lock_target, world) &&
      !steers_its_own_heading(lock_target)) {
    lock_combatant_facing(lock_target, tgt_t, att_t, delta_time, ledger);
  }

  if (is_in_range(attacker,
                  lock_target,
                  attack_comp->melee_range +
                      Engine::Core::AttackComponent::k_melee_contact_range_grace)) {
    attack_comp->melee_lock_separation_time = 0.0F;
    return;
  }

  attack_comp->melee_lock_separation_time += delta_time;
  if (attack_comp->melee_lock_separation_time <
      Engine::Core::AttackComponent::k_melee_lock_separation_release) {
    return;
  }

  attack_comp->release_melee_lock();
  if (reciprocal_lock && lock_target_atk != nullptr) {
    lock_target_atk->release_melee_lock();
  }
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
  auto* attacker_unit = world->try_get<Engine::Core::UnitComponent>(attacker->get_id());
  if ((target == nullptr) || (attacker_unit == nullptr) ||
      !Game::Systems::CombatRules::participates_in_rts_melee_lock(target) ||
      !may_attack(attacker_unit,
                  target,
                  {.intent = EngagementIntent::Ordered, .allow_buildings = true})) {
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

  auto* attack_target =
      Engine::Core::get_or_add_component<Engine::Core::AttackTargetComponent>(attacker);
  if (attack_target != nullptr) {
    auto const* commitment =
        attacker->get_component<Engine::Core::TargetCommitmentComponent>();
    bool const switch_blocked =
        commitment != nullptr && commitment->committed_target_id != 0 &&
        commitment->committed_target_id != attack_comp->melee_lock_target_id &&
        (commitment->in_committed_phase || commitment->cooldown_remaining > 0.0F);
    if (switch_blocked) {
      attack_comp->release_melee_lock();
      assign_attack_target(
          attacker, commitment->committed_target_id, TargetSource::Commitment);
      return;
    }
    assign_attack_target(
        attacker, attack_comp->melee_lock_target_id, TargetSource::MeleeLock);
  }
}

void drop_target_left_by_a_finished_lock(
    Engine::Core::World* world,
    Engine::Core::Entity* attacker,
    const Engine::Core::AttackComponent* attack_comp) {
  if (pursues_targets(attacker) ||
      ((attack_comp != nullptr) && attack_comp->in_melee_lock)) {
    return;
  }

  auto const* attack_target =
      attacker->get_component<Engine::Core::AttackTargetComponent>();
  if ((attack_target == nullptr) || attack_target->is_player_command ||
      attack_target->should_chase) {
    return;
  }

  drop_attack_target(world, attacker);
}

auto bodies_have_met(Engine::Core::Entity& attacker,
                     const Engine::Core::TransformComponent& attacker_transform,
                     Engine::Core::Entity& target,
                     const Engine::Core::TransformComponent& target_transform) -> bool {
  if (attacker.has_component<Engine::Core::ElephantComponent>() ||
      target.has_component<Engine::Core::ElephantComponent>()) {
    return false;
  }
  auto const geometry = FormationCombat::contact_geometry(attacker, target);
  if (geometry.uses_formation_slots) {
    return FormationCombat::contact_is_active(attacker, target, geometry);
  }
  float const distance =
      std::hypot(target_transform.position.x - attacker_transform.position.x,
                 target_transform.position.z - attacker_transform.position.z);
  return distance <=
         FormationCombat::single_combat_strike_distance(attacker, target, geometry);
}

void reciprocate_melee_lock(Engine::Core::World* world,
                            Engine::Core::Entity* attacker,
                            Engine::Core::Entity* target,
                            bool keep_when_locked_on_attacker) {
  auto* target_atk = world->try_get<Engine::Core::AttackComponent>(target->get_id());
  if (target_atk == nullptr) {
    return;
  }
  if (world->has<Engine::Core::WildlifeComponent>(target->get_id())) {
    return;
  }
  if (target->has_component<Engine::Core::ElephantComponent>() &&
      FormationCombat::has_formation_slots(*attacker)) {
    auto const elephant_geometry =
        FormationCombat::contact_geometry(*target, *attacker);
    if (!FormationCombat::contact_is_active(*target, *attacker, elephant_geometry)) {
      return;
    }
  }
  auto* existing_target = world->get_entity(target_atk->melee_lock_target_id);
  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  bool const has_valid_existing_lock =
      target_atk->in_melee_lock && existing_target != nullptr &&
      target_unit != nullptr &&
      may_attack(target_unit,
                 existing_target,
                 {.intent = EngagementIntent::Ordered, .allow_buildings = true});
  if (!has_valid_existing_lock ||
      (keep_when_locked_on_attacker &&
       target_atk->melee_lock_target_id == attacker->get_id())) {
    target_atk->in_melee_lock = true;
    target_atk->melee_lock_target_id = attacker->get_id();
  }
}

auto enter_melee_lock(Engine::Core::Entity* attacker,
                      Engine::Core::Entity* target,
                      Engine::Core::AttackComponent* attack_comp,
                      Engine::Core::World* world,
                      float delta_time,
                      FacingLedger& ledger) -> bool {
  if ((attacker == nullptr) || (target == nullptr) || (attack_comp == nullptr)) {
    return false;
  }

  auto charge_precedes_melee = [](Engine::Core::Entity* entity) {
    auto const* charge = entity->get_component<Engine::Core::MountedChargeComponent>();
    return charge != nullptr &&
           (charge->intent_requested ||
            charge->state == Engine::Core::MountedChargeState::Charging ||
            charge->state == Engine::Core::MountedChargeState::ImpactActive);
  };
  if (charge_precedes_melee(attacker) || charge_precedes_melee(target)) {
    return false;
  }
  if (structure_separates_combatants(attacker, target)) {
    return false;
  }
  if (!Game::Systems::CombatRules::participates_in_rts_melee_lock(attacker) ||
      !Game::Systems::CombatRules::participates_in_rts_melee_lock(target)) {
    return false;
  }

  bool const already_locked = attack_comp->in_melee_lock &&
                              attack_comp->melee_lock_target_id == target->get_id();
  if (!already_locked) {
    attack_comp->in_melee_lock = true;
    attack_comp->melee_lock_target_id = target->get_id();
    attack_comp->melee_footwork_offset = 0.0F;
  }
  reciprocate_melee_lock(world, attacker, target, already_locked);
  if (already_locked) {
    return true;
  }

  answer_attacker(world, target, attacker, AnswerPolicy::TurnOnAttacker);

  auto* att_t = world->try_get<Engine::Core::TransformComponent>(attacker->get_id());
  auto* tgt_t = world->try_get<Engine::Core::TransformComponent>(target->get_id());
  if ((att_t != nullptr) && (tgt_t != nullptr)) {
    lock_combatant_facing(attacker, att_t, tgt_t, delta_time, ledger);
    auto const* target_atk =
        world->try_get<Engine::Core::AttackComponent>(target->get_id());
    bool const reciprocal_lock = (target_atk != nullptr) && target_atk->in_melee_lock &&
                                 target_atk->melee_lock_target_id == attacker->get_id();
    if ((reciprocal_lock || !has_valid_melee_lock(target, world)) &&
        !steers_its_own_heading(target)) {
      lock_combatant_facing(target, tgt_t, att_t, delta_time, ledger);
    }
  }
  return true;
}

void initiate_melee_combat(Engine::Core::Entity* attacker,
                           Engine::Core::Entity* target,
                           Engine::Core::AttackComponent* attack_comp,
                           Engine::Core::World* world,
                           float delta_time,
                           FacingLedger& ledger) {
  if ((attacker == nullptr) || (target == nullptr) || (attack_comp == nullptr)) {
    return;
  }
  bool const held_before = attack_comp->in_melee_lock &&
                           attack_comp->melee_lock_target_id == target->get_id();
  if (!enter_melee_lock(attacker, target, attack_comp, world, delta_time, ledger)) {
    auto* att_t = world->try_get<Engine::Core::TransformComponent>(attacker->get_id());
    auto* tgt_t = world->try_get<Engine::Core::TransformComponent>(target->get_id());
    if ((att_t != nullptr) && (tgt_t != nullptr)) {
      face_target(att_t, tgt_t);
    }
    begin_attack_animation(attacker);
    return;
  }
  begin_attack_animation(attacker, held_before);
}

namespace {

auto can_be_locked(const Engine::Core::World& world,
                   Engine::Core::Entity* entity) -> bool {

  if (entity == nullptr) {
    return false;
  }
  auto const id = entity->get_id();
  if (world.has<Engine::Core::PendingRemovalComponent>(id) ||
      world.has<Engine::Core::ElephantComponent>(id) ||
      world.has<Engine::Core::WildlifeComponent>(id) || is_building(entity) ||
      !Game::Systems::CombatRules::participates_in_rts_melee_lock(entity)) {
    return false;
  }
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(id);
  return unit != nullptr && unit->health > 0 &&
         world.has<Engine::Core::AttackComponent>(id);
}

} // namespace

void lock_touching_enemies(Engine::Core::World* world,
                           const CombatQueryContext& query_context,
                           float delta_time,
                           FacingLedger& ledger) {
  constexpr float k_touch_search_radius = 8.0F;
  constexpr float k_touch_slack = 0.05F;
  for (auto* unit : query_context.units) {
    if (!can_be_locked(*world, unit)) {
      continue;
    }
    auto* attack = world->try_get<Engine::Core::AttackComponent>(unit->get_id());
    if (attack->in_melee_lock) {
      continue;
    }
    auto const* own = world->try_get<Engine::Core::UnitComponent>(unit->get_id());
    auto* transform = world->try_get<Engine::Core::TransformComponent>(unit->get_id());
    if (transform == nullptr) {
      continue;
    }
    collect_unit_ids_near(*world,
                          transform->position.x,
                          transform->position.z,
                          k_touch_search_radius,
                          query_context.nearby_unit_ids);
    for (auto const other_id : query_context.nearby_unit_ids) {
      auto* other = query_context.find_entity(other_id);
      if (other == nullptr || other == unit || !can_be_locked(*world, other)) {
        continue;
      }
      auto const* theirs = world->try_get<Engine::Core::UnitComponent>(other_id);
      if (!query_context.hostile(own->owner_id, theirs->owner_id)) {
        continue;
      }
      auto const geometry = FormationCombat::contact_geometry(*unit, *other);

      constexpr float k_touching_gap = 0.001F;
      bool const touching = geometry.uses_formation_slots
                                ? geometry.surface_gap <= k_touching_gap
                                : geometry.center_distance <=
                                      std::max(geometry.contact_center_distance,
                                               geometry.body_contact_center_distance) +
                                          k_touch_slack;
      if (!touching) {
        continue;
      }
      if (enter_melee_lock(unit, other, attack, world, delta_time, ledger)) {
        assign_attack_target(unit, other->get_id(), TargetSource::MeleeLock);
        stop_unit_movement(unit, transform);
        break;
      }
    }
  }
}
} // namespace Game::Systems::Combat
