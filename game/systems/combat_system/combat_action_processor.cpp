#include "combat_action_processor.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>

#include "../../audio/cue_ids.h"
#include "../../core/component.h"
#include "../../core/entity.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../combat_actions/body_impact.h"
#include "../combat_actions/combat_action_definition.h"
#include "../combat_actions/combat_action_events.h"
#include "../combat_actions/projectile_release.h"
#include "../combat_actions/weapon_trace.h"
#include "../combat_rules.h"
#include "../rpg_combat_system/rpg_bow_draw.h"
#include "../rpg_combat_system/rpg_bow_shot.h"
#include "../rpg_combat_system/rpg_targeting.h"
#include "attack_processor.h"
#include "combat_hit_resolver.h"
#include "combat_utils.h"
#include "damage_application.h"
#include "damage_processor.h"
#include "elephant_special_processor.h"
#include "melee_exchange.h"
#include "mounted_charge_processor.h"

namespace Game::Systems::Combat {

namespace {

auto is_rts_melee_action(Game::Systems::CombatActions::CombatActionId id) -> bool {
  return id == Game::Systems::CombatActions::CombatActionId::RtsSwordStrike ||
         id == Game::Systems::CombatActions::CombatActionId::RtsHeavyOverhead ||
         id == Game::Systems::CombatActions::CombatActionId::RtsSpearThrust ||
         id == Game::Systems::CombatActions::CombatActionId::RtsElephantStomp ||
         id == Game::Systems::CombatActions::CombatActionId::RtsCommanderThrust ||
         id == Game::Systems::CombatActions::CombatActionId::RtsCommanderCut;
}

auto is_rts_attack_action(Game::Systems::CombatActions::CombatActionId id) -> bool {
  return is_rts_melee_action(id) ||
         id == Game::Systems::CombatActions::CombatActionId::RtsBowShot ||
         id == Game::Systems::CombatActions::CombatActionId::RtsCommanderShot;
}

auto is_advanced_rts_commander_melee(
    const Engine::Core::Entity& attacker,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> bool {
  auto const* commander = attacker.get_component<Engine::Core::CommanderComponent>();
  return commander != nullptr && !commander->fpv_controlled &&
         commander->advanced_combat_enabled && definition.commander_only &&
         (definition.weapon_family ==
              Game::Systems::CombatActions::WeaponFamily::Sword ||
          definition.weapon_family ==
              Game::Systems::CombatActions::WeaponFamily::Spear);
}

auto target_uses_rpg_combat(Engine::Core::World& world,
                            Engine::Core::EntityID target_id) -> bool {
  auto* target = world.get_entity(target_id);
  return target != nullptr && Game::Systems::CombatRules::uses_rpg_combat_rules(target);
}

auto action_swings_a_traced_weapon(
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> bool {
  return definition.weapon_family ==
             Game::Systems::CombatActions::WeaponFamily::Sword ||
         definition.weapon_family == Game::Systems::CombatActions::WeaponFamily::Spear;
}

auto target_is_a_structure(Engine::Core::World& world,
                           Engine::Core::EntityID target_id) -> bool {
  auto const* target = world.get_entity(target_id);
  return target != nullptr && target->has_component<Engine::Core::BuildingComponent>();
}

[[nodiscard]] auto poise_capacity_for(const Engine::Core::Entity& target) -> float {
  if (target.has_component<Engine::Core::CommanderComponent>()) {
    return 300.0F;
  }
  auto const* unit = target.get_component<Engine::Core::UnitComponent>();
  return unit != nullptr
             ? std::clamp(static_cast<float>(unit->max_health) * 0.35F, 20.0F, 100.0F)
             : 20.0F;
}

[[nodiscard]] auto authored_hit_stop_seconds(
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> float {
  using Game::Systems::CombatActions::CommanderActionRole;
  switch (definition.role) {
  case CommanderActionRole::Dive:
    return 0.11F;
  case CommanderActionRole::Finisher:
  case CommanderActionRole::Special:
    return 0.085F;
  case CommanderActionRole::Launcher:
    return 0.07F;
  case CommanderActionRole::GapCloser:
  case CommanderActionRole::Aerial:
    return 0.055F;
  case CommanderActionRole::Routine:
    return 0.032F;
  }
  return 0.032F;
}

void apply_authored_action_reaction(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::Entity& target,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const CombatHitResult& result,
    const QVector3D& contact_point,
    float contact_speed) {
  if (!result.applied || target.has_component<Engine::Core::BuildingComponent>()) {
    return;
  }

  if (auto* target_combat =
          target.get_component<Engine::Core::CombatStateComponent>()) {
    target_combat->is_hit_paused = true;
    target_combat->hit_pause_remaining = std::max(
        target_combat->hit_pause_remaining, authored_hit_stop_seconds(definition));
  }

  bool poise_broken = definition.damage.posture_damage <= 0.0F;
  if (definition.damage.posture_damage > 0.0F) {
    auto* poise = Engine::Core::get_or_add_component<Engine::Core::PoiseComponent>(
        &target, poise_capacity_for(target));
    if (poise != nullptr) {
      poise->current =
          std::max(0.0F, poise->current - definition.damage.posture_damage);
      poise->regeneration_delay =
          Engine::Core::PoiseComponent::k_regeneration_delay_seconds;
      poise_broken = poise->current <= 0.0F;
      if (poise_broken) {
        poise->current = poise->maximum * 0.25F;
      }
    }
  }

  if (poise_broken && definition.reaction.stagger_seconds > 0.0F) {
    add_or_extend_stagger(
        &target, definition.reaction.stagger_seconds, definition.reaction.stagger_tier);
    apply_hit_feedback(&target,
                       attacker.get_id(),
                       &world,
                       Engine::Core::HitReactionKind::Stagger,
                       {.contact_point = contact_point,
                        .weapon_speed = std::max(0.0F, contact_speed)});
    if (auto* feedback = target.get_component<Engine::Core::HitFeedbackComponent>()) {
      feedback->stagger_tier = definition.reaction.stagger_tier;
    }
  }

  if (poise_broken && definition.reaction.launch_impulse > 0.0F) {
    auto* target_transform = target.get_component<Engine::Core::TransformComponent>();
    auto const* attacker_transform =
        attacker.get_component<Engine::Core::TransformComponent>();
    if (target_transform != nullptr && attacker_transform != nullptr) {
      float dx = target_transform->position.x - attacker_transform->position.x;
      float dz = target_transform->position.z - attacker_transform->position.z;
      float const length = std::max(0.001F, std::hypot(dx, dz));
      dx /= length;
      dz /= length;
      float const horizontal_impulse = definition.reaction.launch_impulse * 0.24F;
      auto* launch = target.get_component<Engine::Core::CombatLaunchComponent>();
      if (launch == nullptr) {
        launch = target.add_component<Engine::Core::CombatLaunchComponent>();
        if (launch != nullptr) {
          launch->ground_y = target_transform->position.y;
        }
      }
      if (launch != nullptr) {
        launch->velocity_y =
            std::max(launch->velocity_y, definition.reaction.launch_impulse);
        launch->velocity_x = dx * horizontal_impulse;
        launch->velocity_z = dz * horizontal_impulse;
      }
    }
  }

  if (definition.reaction.launch_impulse > 0.0F &&
      result.queued_soldier_casualties > 0) {
    launch_new_casualties(target,
                          attacker,
                          result.queued_soldier_casualties,
                          definition.reaction.launch_impulse);
  }
}

[[nodiscard]] auto
action_already_hit(const Engine::Core::RpgCommanderActionComponent& action,
                   Engine::Core::EntityID entity_id,
                   std::uint16_t soldier_slot) -> bool {
  for (std::uint8_t index = 0; index < action.hit_target_count; ++index) {
    if (action.hit_target_ids[index] == entity_id &&
        action.hit_target_soldier_slots[index] == soldier_slot) {
      return true;
    }
  }
  return false;
}

void deal_radial_action_damage(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const QVector3D& impact_point,
    float contact_speed) {
  if (definition.reaction.radial_radius <= 0.0F) {
    return;
  }
  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr) {
    return;
  }
  auto const capacity = std::min<std::uint8_t>(
      action.hit_target_ids.size(),
      static_cast<std::uint8_t>(std::max(0, definition.max_targets)));
  float const radius_sq =
      definition.reaction.radial_radius * definition.reaction.radial_radius;

  for (auto [candidate, candidate_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
    (void)candidate_unit;
    if (action.hit_target_count >= capacity || &candidate == &attacker ||
        !is_valid_enemy_unit(attacker_unit, &candidate, false)) {
      continue;
    }
    for (auto const& soldier :
         Game::Systems::RpgCombat::live_soldier_targets(candidate)) {
      if (action.hit_target_count >= capacity) {
        return;
      }
      float const dx = soldier.position.x() - impact_point.x();
      float const dz = soldier.position.z() - impact_point.z();
      if ((dx * dx) + (dz * dz) > radius_sq ||
          action_already_hit(action, candidate.get_id(), soldier.soldier_slot)) {
        continue;
      }

      auto const result = resolve_commander_action_hit(
          &world,
          {.contact = {.attacker_id = attacker.get_id(),
                       .target_id = candidate.get_id(),
                       .target_soldier_slot = soldier.soldier_slot,
                       .action_id = definition.id,
                       .weapon_family = definition.weapon_family,
                       .attack_family = definition.attack_family,
                       .attack_direction = definition.attack_direction,
                       .contact_point = soldier.position,
                       .distance = std::hypot(dx, dz),
                       .relative_speed = contact_speed},
           .damage_profile = definition.damage});
      if (!result.attempted) {
        continue;
      }
      apply_authored_action_reaction(world,
                                     attacker,
                                     candidate,
                                     definition,
                                     result,
                                     soldier.position,
                                     contact_speed);
      action.last_hit_target_id = candidate.get_id();
      action.last_hit_soldier_slot = soldier.soldier_slot;
      action.last_damage = result.damage.effective_damage;
      action.hit_target_ids[action.hit_target_count] = candidate.get_id();
      action.hit_target_soldier_slots[action.hit_target_count] = soldier.soldier_slot;
      ++action.hit_target_count;
    }
  }
}

auto rts_melee_target_still_stands(
    Engine::Core::World& world,
    const Engine::Core::Entity& attacker,
    const Engine::Core::RpgCommanderActionComponent& action) -> bool {
  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  auto* target = world.get_entity(action.active_target_id);
  auto const* target_unit = target != nullptr
                                ? target->get_component<Engine::Core::UnitComponent>()
                                : nullptr;
  return attacker_unit != nullptr && attacker_unit->health > 0 && target != nullptr &&
         target_unit != nullptr && target_unit->health > 0 &&
         target_unit->owner_id != attacker_unit->owner_id &&
         target->get_component<Engine::Core::TransformComponent>() != nullptr &&
         !target->has_component<Engine::Core::PendingRemovalComponent>();
}

auto melee_contact_comes_from_the_sweep(
    Engine::Core::World& world,
    const Engine::Core::Entity& attacker,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const Engine::Core::RpgCommanderActionComponent& action) -> bool {
  if (!action_swings_a_traced_weapon(definition)) {
    return false;
  }

  auto* target = Game::Systems::CombatRules::is_player_driven(&attacker)
                     ? nullptr
                     : world.get_entity(action.active_target_id);
  if (target != nullptr && !Game::Systems::CombatRules::is_player_driven(target)) {
    return false;
  }
  return !target_is_a_structure(world, action.active_target_id);
}

void record_signature_contact(
    Engine::Core::Entity& attacker,
    const Engine::Core::TransformComponent& attacker_transform,
    const Engine::Core::TransformComponent& target_transform,
    Engine::Core::CommanderSignatureForm form) {
  auto* presentation = Engine::Core::get_or_add_component<
      Engine::Core::CommanderSignaturePresentationComponent>(&attacker);
  if (presentation == nullptr) {
    return;
  }
  if (presentation->entries.size() >=
      Engine::Core::CommanderSignaturePresentationComponent::k_max_entries) {
    presentation->entries.erase(presentation->entries.begin());
  }

  float const dx = target_transform.position.x - attacker_transform.position.x;
  float const dz = target_transform.position.z - attacker_transform.position.z;
  float const length = std::max(0.0001F, std::hypot(dx, dz));

  Engine::Core::CommanderSignaturePresentationComponent::Entry entry;

  entry.x = attacker_transform.position.x + dx * 0.72F;
  entry.y = attacker_transform.position.y + 0.58F;
  entry.z = attacker_transform.position.z + dz * 0.72F;
  entry.dir_x = dx / length;
  entry.dir_z = dz / length;
  entry.form = form;
  presentation->entries.push_back(entry);
}

auto signature_form_for_action(Game::Systems::CombatActions::CombatActionId id)
    -> Engine::Core::CommanderSignatureForm {
  switch (id) {
  case Game::Systems::CombatActions::CombatActionId::RtsCommanderThrust:
    return Engine::Core::CommanderSignatureForm::Thrust;
  case Game::Systems::CombatActions::CombatActionId::RtsCommanderShot:
    return Engine::Core::CommanderSignatureForm::Shot;
  default:
    return Engine::Core::CommanderSignatureForm::Cut;
  }
}

void apply_commander_signature_effects(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::CommanderComponent& commander,
    Engine::Core::Entity& primary_target,
    const Engine::Core::TransformComponent& attacker_transform,
    float reach,
    int damage) {
  auto const* target_transform =
      primary_target.get_component<Engine::Core::TransformComponent>();
  auto const* action =
      attacker.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (target_transform != nullptr && action != nullptr) {
    record_signature_contact(
        attacker,
        attacker_transform,
        *target_transform,
        signature_form_for_action(
            static_cast<Game::Systems::CombatActions::CombatActionId>(
                action->combat_action_id)));
  }

  if (commander.signature_stagger_seconds > 0.0F) {
    Game::Systems::Combat::add_or_extend_stagger(
        &primary_target,
        commander.signature_stagger_seconds,
        Engine::Core::StaggerTier::LightFlinch);
  }

  if (commander.signature_max_targets <= 1) {
    return;
  }

  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr) {
    return;
  }
  float const sweep_radius = reach + 0.4F;
  int const sweep_damage = std::max(1, damage / 2);
  int remaining = commander.signature_max_targets - 1;

  for (auto [candidate_ref, candidate_unit_ref, candidate_transform_ref] :
       world.entity_view<Engine::Core::UnitComponent,
                         Engine::Core::TransformComponent>()) {
    if (remaining <= 0) {
      break;
    }
    Engine::Core::Entity* candidate = &candidate_ref;
    auto const* candidate_unit = &candidate_unit_ref;
    auto const* candidate_transform = &candidate_transform_ref;
    if (candidate == &attacker || candidate == &primary_target ||
        candidate->has_component<Engine::Core::PendingRemovalComponent>() ||
        candidate->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }
    if (candidate_unit->health <= 0 ||
        candidate_unit->owner_id == attacker_unit->owner_id) {
      continue;
    }
    float const sweep_dx =
        candidate_transform->position.x - attacker_transform.position.x;
    float const sweep_dz =
        candidate_transform->position.z - attacker_transform.position.z;
    if (std::hypot(sweep_dx, sweep_dz) > sweep_radius) {
      continue;
    }
    deal_damage(&world, candidate, sweep_damage, attacker.get_id());
    if (commander.signature_stagger_seconds > 0.0F) {
      Game::Systems::Combat::add_or_extend_stagger(
          candidate,
          commander.signature_stagger_seconds * 0.6F,
          Engine::Core::StaggerTier::LightFlinch);
    }
    --remaining;
  }
}

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
  bool const advanced_commander_melee =
      is_advanced_rts_commander_melee(attacker, definition);
  float const reach =
      (advanced_commander_melee
           ? std::max(attack != nullptr ? attack->melee_range : 0.0F,
                      definition.hit_shape.reach)
           : (attack != nullptr ? attack->melee_range : definition.hit_shape.reach)) +
      (signature_strike ? std::max(0.0F, commander->signature_bonus_reach) : 0.0F);

  auto const beat =
      signature_strike
          ? MeleeExchangeBeat{}
          : melee_exchange_beat_for_outcome(
                static_cast<MeleeExchangeOutcome>(action.exchange_outcome));
  int const base_damage = std::max(1, action.requested_damage);
  int const damage =
      signature_strike ? base_damage : melee_exchange_damage(base_damage, beat);
  bool const first_hit = action.hit_target_count == 0U;
  if (damage > 0) {
    deal_damage(&world, &target, damage, attacker.get_id());
  }
  action.last_hit_target_id = target.get_id();
  action.last_damage = damage;
  if (action.hit_target_count < action.hit_target_ids.size()) {
    action.hit_target_ids[action.hit_target_count++] = target.get_id();
  }

  if (damage > 0 && is_advanced_rts_commander_melee(attacker, definition)) {
    float const dx = target_transform->position.x - attacker_transform->position.x;
    float const dz = target_transform->position.z - attacker_transform->position.z;
    QVector3D const impact_point(attacker_transform->position.x + dx * 0.62F,
                                 attacker_transform->position.y + 1.0F,
                                 attacker_transform->position.z + dz * 0.62F);
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

  if (!signature_strike) {
    present_melee_exchange(
        world, attacker, target, *attacker_transform, *target_transform, beat, reach);
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
  auto* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  auto* attacker_transform = attacker.get_component<Engine::Core::TransformComponent>();
  auto* target = world.get_entity(action.active_target_id);
  auto* target_unit = target != nullptr
                          ? target->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
  auto* target_transform =
      target != nullptr ? target->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  if (attacker_unit == nullptr || attacker_transform == nullptr || target == nullptr ||
      target_unit == nullptr || target_transform == nullptr ||
      attacker_unit->health <= 0 || target_unit->health <= 0 ||
      attacker_unit->owner_id == target_unit->owner_id ||
      target->has_component<Engine::Core::PendingRemovalComponent>()) {
    action.action_running = false;
    action.action_completed = true;
    return;
  }
  float const dx = target_transform->position.x - attacker_transform->position.x;
  float const dz = target_transform->position.z - attacker_transform->position.z;
  float const distance = std::hypot(dx, dz);
  auto const* attack = attacker.get_component<Engine::Core::AttackComponent>();
  auto const* commander = attacker.get_component<Engine::Core::CommanderComponent>();
  bool const signature_strike =
      commander != nullptr && commander->signature_strike_active;
  bool const advanced_commander_melee =
      is_advanced_rts_commander_melee(attacker, definition);
  float const reach =
      (advanced_commander_melee
           ? std::max(attack != nullptr ? attack->melee_range : 0.0F,
                      definition.hit_shape.reach)
           : (attack != nullptr ? attack->melee_range : definition.hit_shape.reach)) +
      (signature_strike ? std::max(0.0F, commander->signature_bonus_reach) : 0.0F);
  float const yaw = attacker_transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const facing =
      (std::sin(yaw) * dx + std::cos(yaw) * dz) / std::max(distance, 0.0001F);

  bool in_range =
      is_in_range(&attacker,
                  target,
                  reach + Engine::Core::AttackComponent::k_melee_contact_range_grace);
  if (advanced_commander_melee && !is_building(target)) {

    float const effective_reach =
        reach + Engine::Core::AttackComponent::k_melee_contact_range_grace +
        combat_radius(target);
    bool const height_valid =
        attack == nullptr ||
        std::abs(target_transform->position.y - attacker_transform->position.y) <=
            attack->max_height_difference;
    in_range = distance <= effective_reach && height_valid &&
               !structure_separates_combatants(&attacker, target);
  }

  if (!in_range || facing < std::cos(80.0F * std::numbers::pi_v<float> / 180.0F)) {
    action.action_running = false;
    action.action_completed = true;
    return;
  }
  resolve_rts_melee_contact(world, attacker, action, definition, *target);
}

void deal_weapon_trace_damage(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::CombatStateComponent* presentation_state,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) {
  bool const sweep_decides =
      melee_contact_comes_from_the_sweep(world, attacker, definition, action);

  using Game::Systems::CombatActions::CombatActionEventType;
  float const window_start = Game::Systems::CombatActions::action_event_normalized_time(
      definition, CombatActionEventType::WeaponTraceStart, 1.0F);
  float const window_end = Game::Systems::CombatActions::action_event_normalized_time(
      definition, CombatActionEventType::WeaponTraceEnd, window_start);
  float swept_from = action.previous_normalized_action_time;
  float swept_to = action.normalized_action_time;
  if (sweep_decides) {
    if (window_end <= window_start) {
      return;
    }
    swept_from = std::max(swept_from, window_start);
    swept_to = std::min(swept_to, window_end);
    if (swept_to <= swept_from) {
      return;
    }
  } else if (!action.weapon_trace_active) {
    return;
  }
  auto const target_capacity = std::min<std::uint8_t>(
      action.hit_target_ids.size(),
      static_cast<std::uint8_t>(std::max(0, definition.max_targets)));
  if (target_capacity == 0U || action.hit_target_count >= target_capacity) {
    return;
  }

  std::span<const Engine::Core::EntityID> ignored_targets{};
  std::array<Game::Systems::CombatActions::WeaponTraceIgnoredTarget,
             Engine::Core::RpgCommanderActionComponent::k_max_action_hit_targets>
      ignored_target_slots_storage{};
  std::span<const Game::Systems::CombatActions::WeaponTraceIgnoredTarget>
      ignored_target_slots{};
  if (definition.can_hit_same_target_once) {
    if (definition.commander_only) {
      for (std::uint8_t index = 0; index < action.hit_target_count; ++index) {
        ignored_target_slots_storage[index] = {
            .entity_id = action.hit_target_ids[index],
            .soldier_slot = action.hit_target_soldier_slots[index],
        };
      }
      ignored_target_slots = {ignored_target_slots_storage.data(),
                              action.hit_target_count};
    } else {
      ignored_targets = {action.hit_target_ids.data(), action.hit_target_count};
    }
  }
  auto const contact = Game::Systems::CombatActions::find_weapon_trace_contact(
      world,
      attacker,
      definition,
      {.previous_normalized_time = swept_from, .current_normalized_time = swept_to},
      action.active_target_id,
      ignored_targets,
      ignored_target_slots);
  if (contact.target_id == 0) {
    return;
  }

  if (is_rts_melee_action(definition.id) &&
      !target_uses_rpg_combat(world, contact.target_id)) {
    if (auto* struck = world.get_entity(contact.target_id); struck != nullptr) {
      resolve_rts_melee_contact(world, attacker, action, definition, *struck);
      if (presentation_state != nullptr) {
        presentation_state->damage_dealt_this_swing = true;
      }
      action.last_hit_soldier_slot = contact.target_soldier_slot;
    }
    return;
  }

  auto const result = resolve_commander_action_hit(
      &world,
      {.contact = {.attacker_id = attacker.get_id(),
                   .target_id = contact.target_id,
                   .target_soldier_slot = contact.target_soldier_slot,
                   .action_id = definition.id,
                   .weapon_family = definition.weapon_family,
                   .attack_family = definition.attack_family,
                   .attack_direction = definition.attack_direction,
                   .contact_point = contact.contact_point,
                   .distance = contact.distance,
                   .local_forward = contact.local_forward,
                   .local_right = contact.local_right,
                   .relative_speed = contact.contact_speed},
       .damage_profile = definition.damage,

       .explicit_raw_damage = is_rts_melee_action(definition.id)
                                  ? std::max(1, action.requested_damage)
                                  : 0});
  if (!result.attempted) {
    return;
  }

  if (auto* struck = world.get_entity(contact.target_id); struck != nullptr) {
    apply_authored_action_reaction(world,
                                   attacker,
                                   *struck,
                                   definition,
                                   result,
                                   contact.contact_point,
                                   contact.contact_speed);
  }

  if (presentation_state != nullptr) {
    presentation_state->damage_dealt_this_swing = true;

    presentation_state->is_hit_paused = true;
    presentation_state->hit_pause_remaining = std::max(
        presentation_state->hit_pause_remaining,
        definition.commander_only
            ? authored_hit_stop_seconds(definition)
            : Engine::Core::CombatStateComponent::
                      k_combat_animation_hit_pause_duration *
                  std::clamp(contact.contact_speed /
                                 Game::Systems::Combat::k_reference_weapon_speed,
                             0.5F,
                             1.6F));
    presentation_state->telegraph_cue = Engine::Core::TelegraphCue::Impact;
  }
  bool const first_hit = action.hit_target_count == 0U;
  action.last_hit_target_id = contact.target_id;
  action.last_hit_soldier_slot = contact.target_soldier_slot;
  action.last_damage = result.damage.effective_damage;
  action.last_contact_speed = contact.contact_speed;
  action.hit_target_ids[action.hit_target_count] = contact.target_id;
  action.hit_target_soldier_slots[action.hit_target_count] =
      contact.target_soldier_slot;
  ++action.hit_target_count;
  if (first_hit) {
    deal_radial_action_damage(world,
                              attacker,
                              action,
                              definition,
                              contact.contact_point,
                              contact.contact_speed);
  }
}

void deal_mount_body_impact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) {
  if (!action.action_active ||
      definition.weapon_family != Game::Systems::CombatActions::WeaponFamily::Mount) {
    return;
  }
  auto const target_capacity = std::min<std::uint8_t>(
      action.hit_target_ids.size(),
      static_cast<std::uint8_t>(std::max(0, definition.max_targets)));
  if (target_capacity == 0U || action.hit_target_count >= target_capacity) {
    return;
  }

  std::span<const Engine::Core::EntityID> ignored_targets{};
  if (definition.can_hit_same_target_once) {
    ignored_targets = {action.hit_target_ids.data(), action.hit_target_count};
  }
  auto const contact = Game::Systems::CombatActions::find_body_impact_contact(
      world, attacker, definition, action.active_target_id, ignored_targets);
  if (contact.target_id == 0) {
    return;
  }

  int impact_damage = 10;
  if (auto const* attack = attacker.get_component<Engine::Core::AttackComponent>()) {
    impact_damage = std::max(1, attack->get_current_damage());
  }
  auto const result = resolve_mounted_charge_impact_hit(
      &world,
      {.contact = {.attacker_id = attacker.get_id(),
                   .target_id = contact.target_id,
                   .action_id = definition.id,
                   .weapon_family = definition.weapon_family,
                   .attack_family = definition.attack_family,
                   .attack_direction = definition.attack_direction,
                   .contact_point = contact.contact_point,
                   .distance = contact.distance,
                   .local_forward = contact.local_forward,
                   .local_right = contact.local_right},
       .damage_profile = definition.damage,
       .explicit_raw_damage = impact_damage});
  if (!result.attempted) {
    return;
  }

  action.last_hit_target_id = contact.target_id;
  action.last_damage = result.damage.effective_damage;
  action.hit_target_ids[action.hit_target_count++] = contact.target_id;
  if (auto* charge = attacker.get_component<Engine::Core::MountedChargeComponent>()) {
    charge->last_impact_target_id = contact.target_id;
  }
}

void cancel_authored_action(Engine::Core::RpgCommanderActionComponent& action,
                            Engine::Core::CombatStateComponent* presentation_state) {
  action.action_running = false;
  action.action_completed = true;
  action.action_active = false;
  action.weapon_trace_active = false;
  action.phase = Engine::Core::RpgCommanderActionPhase::None;
  if (presentation_state != nullptr) {
    presentation_state->animation_state = Engine::Core::CombatAnimationState::Idle;
    presentation_state->state_time = 0.0F;
    presentation_state->state_duration = 0.0F;
  }
}

auto update_commander_bow_draw(
    Engine::Core::Entity& entity,
    const Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    float delta_time) -> Game::Systems::RpgCombat::BowDrawTick {
  Game::Systems::RpgCombat::BowDrawTick tick;
  tick.allowed_delta = delta_time;

  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  auto* aim = entity.get_component<Engine::Core::RpgCommanderAimComponent>();
  if (aim == nullptr || commander == nullptr || !commander->fpv_controlled) {
    return tick;
  }

  float stamina_ratio = 1.0F;
  if (auto const* stamina = entity.get_component<Engine::Core::StaminaComponent>();
      stamina != nullptr && stamina->max_stamina > 0.0F) {
    stamina_ratio = std::clamp(stamina->stamina / stamina->max_stamina, 0.0F, 1.0F);
  }

  tick = Game::Systems::RpgCombat::update_bow_draw(
      *aim, action, definition, stamina_ratio, delta_time);

  if (tick.started_draw) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent(Game::Audio::Cue::k_combat_bow_draw));
  }
  if (tick.reached_full_draw) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent(Game::Audio::Cue::k_combat_bow_full_draw));
  }
  if (tick.started_straining) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent(Game::Audio::Cue::k_combat_bow_strain));
  }
  if (tick.relaxed) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent(Game::Audio::Cue::k_combat_ability_refused));
  }
  if (tick.at_full_draw) {
    if (auto* stamina = entity.get_component<Engine::Core::StaminaComponent>()) {
      constexpr float k_full_draw_stamina_drain = 6.0F;
      stamina->stamina =
          std::max(0.0F, stamina->stamina - (k_full_draw_stamina_drain * delta_time));
    }
  }
  return tick;
}

auto action_contact_event(
    const Game::Systems::CombatActions::CombatActionDefinition& definition)
    -> Game::Systems::CombatActions::CombatActionEventType {

  for (auto const& event : definition.events) {
    if (event.type ==
        Game::Systems::CombatActions::CombatActionEventType::ActiveStart) {
      return Game::Systems::CombatActions::CombatActionEventType::ActiveStart;
    }
  }
  return Game::Systems::CombatActions::CombatActionEventType::WeaponTraceStart;
}

void handle_action_events(
    Engine::Core::World& world,
    Engine::Core::Entity& entity,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    std::span<const Game::Systems::CombatActions::CombatActionEvent> events) {
  handle_mounted_charge_action_events(entity, action, definition, events);
  auto const contact_event = action_contact_event(definition);
  for (auto const& event : events) {
    auto const action_id = static_cast<Game::Systems::CombatActions::CombatActionId>(
        action.combat_action_id);
    bool const advanced_rts_melee = is_advanced_rts_commander_melee(entity, definition);
    if (event.type == contact_event &&
        (is_rts_melee_action(action_id) || advanced_rts_melee) &&
        action.hit_target_count == 0U) {
      if (!melee_contact_comes_from_the_sweep(world, entity, definition, action)) {
        deal_rts_melee_contact_damage(world, entity, action, definition);
      } else if (!rts_melee_target_still_stands(world, entity, action)) {
        action.action_running = false;
        action.action_completed = true;
        action.action_active = false;
        action.weapon_trace_active = false;
      }
      if (action_id == Game::Systems::CombatActions::CombatActionId::RtsElephantStomp &&
          action.hit_target_count > 0U) {
        (void)apply_elephant_stomp_impact(&world, &entity);
      }
      continue;
    }
    if (event.type !=
        Game::Systems::CombatActions::CombatActionEventType::ProjectileRelease) {
      continue;
    }
    if (action_id == Game::Systems::CombatActions::CombatActionId::RtsBowShot ||
        action_id == Game::Systems::CombatActions::CombatActionId::RtsCommanderShot) {
      auto const* special =
          entity.get_component<Engine::Core::SpecialAttackComponent>();
      bool released = false;
      if (special != nullptr && special->use_projectile_system) {
        auto const release =
            Game::Systems::CombatActions::release_projectile_for_action(
                &world,
                entity,
                definition,
                action.active_target_id,
                action.requested_damage);
        released = release.released;
      } else {
        released = release_rts_arrow_volley(
            world, entity, action.active_target_id, action.requested_damage);
      }
      if (released) {
        action.last_hit_target_id = action.active_target_id;
        action.last_damage = std::max(1, action.requested_damage);

        if (auto* commander = entity.get_component<Engine::Core::CommanderComponent>();
            commander != nullptr && commander->signature_strike_active) {
          auto* shot_target = world.get_entity(action.active_target_id);
          if (shot_target != nullptr && commander->signature_stagger_seconds > 0.0F) {
            Game::Systems::Combat::add_or_extend_stagger(
                shot_target,
                commander->signature_stagger_seconds,
                Engine::Core::StaggerTier::LightFlinch);
          }
          auto const* shooter_transform =
              entity.get_component<Engine::Core::TransformComponent>();
          auto const* shot_transform =
              shot_target != nullptr
                  ? shot_target->get_component<Engine::Core::TransformComponent>()
                  : nullptr;
          if (shooter_transform != nullptr && shot_transform != nullptr) {
            record_signature_contact(entity,
                                     *shooter_transform,
                                     *shot_transform,
                                     Engine::Core::CommanderSignatureForm::Shot);
          }
          commander->signature_strike_active = false;
        }
      }
      continue;
    }
    if (definition.weapon_family == Game::Systems::CombatActions::WeaponFamily::Bow &&
        !is_rts_attack_action(action_id) &&
        entity.has_component<Engine::Core::RpgCommanderAimComponent>()) {
      auto const loosed =
          Game::Systems::RpgCombat::loose_aimed_arrow(world, entity, definition);
      if (loosed.released) {
        action.active_target_id = loosed.target_id;
        action.active_target_soldier_slot = loosed.soldier_slot;
        action.last_hit_target_id = loosed.target_id;
        action.last_damage = loosed.damage;
      }
      continue;
    }
    auto const release = Game::Systems::CombatActions::release_projectile_for_action(
        &world, entity, definition, action.active_target_id);
    if (release.released) {
      action.last_hit_target_id = release.target_id;
      action.last_damage = release.damage;
    }
  }
}

} // namespace

void process_authored_combat_action(
    Engine::Core::World* world,
    Engine::Core::Entity& entity,
    Engine::Core::CombatStateComponent* presentation_state,
    float delta_time) {
  if (world == nullptr) {
    return;
  }
  auto* action = entity.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr || action->combat_action_id == 0U) {
    return;
  }
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action->combat_action_id));
  if (definition == nullptr) {
    return;
  }
  auto const action_id = static_cast<Game::Systems::CombatActions::CombatActionId>(
      action->combat_action_id);

  if (auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
      commander != nullptr && commander->fpv_controlled && action->action_running) {
    auto const* stagger = entity.get_component<Engine::Core::StaggerComponent>();
    if (stagger != nullptr && stagger->tier != Engine::Core::StaggerTier::LightFlinch) {
      action->action_running = false;
      action->action_completed = true;
      action->action_active = false;
      action->weapon_trace_active = false;
      action->phase = Engine::Core::RpgCommanderActionPhase::None;
      if (presentation_state != nullptr) {
        presentation_state->animation_state = Engine::Core::CombatAnimationState::Idle;
        presentation_state->state_time = 0.0F;
        presentation_state->state_duration = 0.0F;
      }
      return;
    }
  }

  if (is_rts_attack_action(action_id)) {
    auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
    auto const* target = entity.get_component<Engine::Core::AttackTargetComponent>();

    auto const* attack = entity.get_component<Engine::Core::AttackComponent>();
    bool const shooting_from_a_melee =
        !is_rts_melee_action(action_id) && attack != nullptr && attack->in_melee_lock &&
        Game::Systems::CombatRules::participates_in_rts_melee_lock(&entity);

    bool const interrupted = unit == nullptr || unit->health <= 0 ||
                             shooting_from_a_melee ||
                             entity.has_component<Engine::Core::StaggerComponent>() ||
                             (target != nullptr && target->target_id != 0 &&
                              target->target_id != action->active_target_id);
    if (interrupted) {
      action->action_running = false;
      action->action_completed = true;
      action->action_active = false;
      action->weapon_trace_active = false;
      return;
    }
  }

  float action_delta = delta_time;
  if (action_id == Game::Systems::CombatActions::CombatActionId::RpgBowShot) {
    auto const draw =
        update_commander_bow_draw(entity, *action, *definition, delta_time);
    if (draw.relaxed) {
      cancel_authored_action(*action, presentation_state);
      return;
    }
    action_delta = draw.allowed_delta;
  }

  auto const events = Game::Systems::CombatActions::advance_combat_action_events(
      *action, action_delta, *definition);
  handle_action_events(*world, entity, *action, *definition, events);

  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  bool const attacks_rpg_target =
      is_rts_melee_action(action_id) &&
      action_id != Game::Systems::CombatActions::CombatActionId::RtsElephantStomp &&
      target_uses_rpg_combat(*world, action->active_target_id);
  if ((commander != nullptr && commander->fpv_controlled) || attacks_rpg_target) {
    deal_weapon_trace_damage(*world, entity, presentation_state, *action, *definition);
  }
  deal_mount_body_impact(*world, entity, *action, *definition);
}

} // namespace Game::Systems::Combat
