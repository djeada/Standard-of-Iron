#include "rts_melee_contact.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../core/world.h"
#include "../formation_combat_geometry.h"
#include "action_contact_damage.h"
#include "authored_action_reaction.h"
#include "combat_action_predicates.h"
#include "combat_hit_resolver.h"
#include "combat_utils.h"
#include "commander_duel.h"
#include "commander_signature_effects.h"
#include "damage_application.h"
#include "damage_processor.h"
#include "melee_exchange.h"

namespace Game::Systems::Combat {

namespace {

void present_melee_exchange(Engine::Core::World& world,
                            Engine::Core::Entity& attacker,
                            Engine::Core::Entity& target,
                            const Engine::Core::TransformComponent& attacker_transform,
                            const Engine::Core::TransformComponent& target_transform,
                            const MeleeExchangeBeat& beat,
                            float reach) {
  auto const* target_unit = target.get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    return;
  }
  if (beat.outcome == MeleeExchangeOutcome::Clean ||
      beat.outcome == MeleeExchangeOutcome::Plain) {

    return;
  }
  apply_melee_reaction_feedback(
      &world, &target, attacker.get_id(), beat.target_reaction);

  float const dx = target_transform.position.x - attacker_transform.position.x;
  float const dz = target_transform.position.z - attacker_transform.position.z;
  float const distance = std::max(0.001F, std::hypot(dx, dz));
  float const contact_reach =
      std::min(distance * 0.72F, std::max(0.35F, reach * 0.55F));
  QVector3D const contact_point(
      attacker_transform.position.x + dx / distance * contact_reach,
      attacker_transform.position.y + 1.05F,
      attacker_transform.position.z + dz / distance * contact_reach);
  switch (beat.outcome) {
  case MeleeExchangeOutcome::Blocked:
    queue_melee_contact_burst(
        target, contact_point, Engine::Core::RpgContactOutcome::Block, 0.9F);
    break;
  case MeleeExchangeOutcome::Evaded:
    queue_melee_contact_burst(
        target, contact_point, Engine::Core::RpgContactOutcome::Dodge, 0.6F);
    break;
  case MeleeExchangeOutcome::Heavy:
    queue_melee_contact_burst(
        target, contact_point, Engine::Core::RpgContactOutcome::Damage, 1.1F);
    break;
  case MeleeExchangeOutcome::Clean:
  case MeleeExchangeOutcome::Plain:
    break;
  }
  if (beat.attacker_recoils) {
    apply_hit_feedback(
        &attacker, target.get_id(), &world, Engine::Core::HitReactionKind::Recoil);
  }
}

void record_melee_hit_on_action(Engine::Core::RpgCommanderActionComponent& action,
                                Engine::Core::EntityID target_id,
                                int damage) {
  action.last_hit_target_id = target_id;
  action.last_damage = damage;
  if (action.hit_target_count < action.hit_target_ids.size()) {
    action.hit_target_ids[action.hit_target_count++] = target_id;
  }
}

void apply_advanced_commander_hit(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::Entity& target,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const Engine::Core::TransformComponent& attacker_transform,
    const Engine::Core::TransformComponent& target_transform,
    const Engine::Core::CommanderComponent* commander,
    bool signature_strike,
    bool first_hit,
    float reach,
    int damage) {
  float const dx = target_transform.position.x - attacker_transform.position.x;
  float const dz = target_transform.position.z - attacker_transform.position.z;
  QVector3D const impact_point(attacker_transform.position.x + dx * 0.62F,
                               attacker_transform.position.y + 1.0F,
                               attacker_transform.position.z + dz * 0.62F);
  if (!signature_strike) {
    record_signature_contact(attacker,
                             attacker_transform,
                             target_transform,
                             commander_strike_form(definition, commander),
                             commander_strike_intensity(definition, false),
                             reach);
  }
  CombatHitResult authored_result;
  authored_result.attempted = true;
  authored_result.applied = true;
  authored_result.raw_damage = damage;
  apply_authored_action_reaction(world,
                                 attacker,
                                 target,
                                 definition,
                                 authored_result,
                                 impact_point,
                                 k_reference_weapon_speed);
  if (first_hit) {
    deal_radial_action_damage(
        world, attacker, action, definition, impact_point, k_reference_weapon_speed);
  }
}

[[nodiscard]] auto
swing_is_within_facing_cone(Engine::Core::World& world,
                            const Engine::Core::Entity& attacker,
                            const Engine::Core::Entity& target,
                            const Engine::Core::TransformComponent& attacker_transform,
                            float dx,
                            float dz,
                            float distance) -> bool {
  float const yaw = attacker_transform.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const facing =
      (std::sin(yaw) * dx + std::cos(yaw) * dz) / std::max(distance, 0.0001F);
  auto const* attack = world.try_get<Engine::Core::AttackComponent>(attacker.get_id());
  bool const soldiers_face_the_animal =
      world.has<Engine::Core::WildlifeComponent>(target.get_id()) &&
      FormationCombat::has_formation_slots(attacker);
  bool const soldiers_face_their_lock =
      attack != nullptr && attack->melee_locked_on(target.get_id()) &&
      FormationCombat::has_formation_slots(attacker) &&
      FormationCombat::has_formation_slots(target);
  return soldiers_face_the_animal || soldiers_face_their_lock ||
         facing >= std::cos(80.0F * std::numbers::pi_v<float> / 180.0F);
}

[[nodiscard]] auto target_is_in_reach(
    Engine::Core::Entity& attacker,
    Engine::Core::Entity& target,
    const Engine::Core::TransformComponent& attacker_transform,
    const Engine::Core::TransformComponent& target_transform,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    float distance) -> bool {
  auto const* attack = attacker.get_component<Engine::Core::AttackComponent>();
  auto const* commander = attacker.get_component<Engine::Core::CommanderComponent>();
  bool const advanced_commander_melee =
      is_advanced_rts_commander_melee(attacker, definition);
  float const reach =
      rts_melee_reach(attack, commander, definition, advanced_commander_melee);
  bool in_range =
      is_in_range(&attacker,
                  &target,
                  reach + Engine::Core::AttackComponent::k_melee_contact_range_grace);
  if (advanced_commander_melee && !is_building(&target)) {
    float const effective_reach =
        reach + Engine::Core::AttackComponent::k_melee_contact_range_grace +
        combat_radius(&target);
    bool const height_valid =
        attack == nullptr || duelling_with(attacker, target) ||
        std::abs(target_transform.position.y - attacker_transform.position.y) <=
            attack->max_height_difference;
    in_range = distance <= effective_reach && height_valid &&
               !structure_separates_combatants(&attacker, &target);
  }
  return in_range;
}

} // namespace

void resolve_rts_melee_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    Engine::Core::Entity& target) {
  auto* attacker_transform = attacker.get_component<Engine::Core::TransformComponent>();
  auto* target_transform = target.get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr) {
    return;
  }
  auto const* attack = attacker.get_component<Engine::Core::AttackComponent>();
  auto* commander = attacker.get_component<Engine::Core::CommanderComponent>();
  bool const signature_strike =
      commander != nullptr && commander->signature_strike_active;
  bool const advanced = is_advanced_rts_commander_melee(attacker, definition);
  float const reach = rts_melee_reach(attack, commander, definition, advanced);

  auto const beat =
      signature_strike
          ? MeleeExchangeBeat{}
          : melee_exchange_beat_for_outcome(
                static_cast<MeleeExchangeOutcome>(action.exchange_outcome));
  int const base_damage = std::max(1, action.requested_damage);
  auto const duel_contact = resolve_duel_contact(attacker, target, base_damage);
  int const damage =
      duel_contact.has_value()
          ? duel_contact->damage
          : (signature_strike ? base_damage : melee_exchange_damage(base_damage, beat));
  bool const first_hit = action.hit_target_count == 0U;
  if (damage > 0) {
    deal_damage(&world, &target, damage, attacker.get_id());
  }
  record_melee_hit_on_action(action, target.get_id(), damage);

  if (damage > 0 && advanced) {
    apply_advanced_commander_hit(world,
                                 attacker,
                                 target,
                                 action,
                                 definition,
                                 *attacker_transform,
                                 *target_transform,
                                 commander,
                                 signature_strike,
                                 first_hit,
                                 reach,
                                 damage);
  }

  if (duel_contact.has_value()) {
    present_duel_contact(
        world, attacker, target, *attacker_transform, *target_transform, *duel_contact);
  }
  if (!signature_strike) {
    if (!duel_contact.has_value()) {
      present_melee_exchange(
          world, attacker, target, *attacker_transform, *target_transform, beat, reach);
    }
    return;
  }
  apply_commander_signature_effects(
      world, attacker, *commander, target, *attacker_transform, reach, damage);
  commander->signature_strike_active = false;
}

void deal_rts_melee_contact_damage(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) {
  auto* attacker_transform =
      world.try_get<Engine::Core::TransformComponent>(attacker.get_id());
  auto* target = world.get_entity(action.active_target_id);
  if (attacker_transform == nullptr ||
      !rts_melee_target_still_stands(world, attacker, action)) {
    action.action_running = false;
    action.action_completed = true;
    return;
  }
  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  float const dx = target_transform->position.x - attacker_transform->position.x;
  float const dz = target_transform->position.z - attacker_transform->position.z;
  float const distance = std::hypot(dx, dz);
  bool const in_range = target_is_in_reach(
      attacker, *target, *attacker_transform, *target_transform, definition, distance);
  if (!in_range ||
      !swing_is_within_facing_cone(
          world, attacker, *target, *attacker_transform, dx, dz, distance)) {
    action.action_running = false;
    action.action_completed = true;
    return;
  }
  resolve_rts_melee_contact(world, attacker, action, definition, *target);
}

} // namespace Game::Systems::Combat
