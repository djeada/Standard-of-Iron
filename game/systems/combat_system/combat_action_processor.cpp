#include "combat_action_processor.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>

#include "../../core/component.h"
#include "../../core/entity.h"
#include "../../core/world.h"
#include "../combat_actions/body_impact.h"
#include "../combat_actions/combat_action_definition.h"
#include "../combat_actions/combat_action_events.h"
#include "../combat_actions/projectile_release.h"
#include "../combat_actions/weapon_trace.h"
#include "../combat_rules.h"
#include "attack_processor.h"
#include "combat_hit_resolver.h"
#include "combat_utils.h"
#include "damage_application.h"
#include "damage_processor.h"
#include "elephant_special_processor.h"
#include "mounted_charge_processor.h"

namespace Game::Systems::Combat {

namespace {

auto is_rts_melee_action(Game::Systems::CombatActions::CombatActionId id) -> bool {
  return id == Game::Systems::CombatActions::CombatActionId::RtsSwordStrike ||
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

auto target_uses_rpg_combat(Engine::Core::World& world,
                            Engine::Core::EntityID target_id) -> bool {
  auto* target = world.get_entity(target_id);
  return target != nullptr && Game::Systems::CombatRules::uses_rpg_combat_rules(target);
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

  for (auto* candidate : world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (remaining <= 0) {
      break;
    }
    if (candidate == nullptr || candidate == &attacker ||
        candidate == &primary_target ||
        candidate->has_component<Engine::Core::PendingRemovalComponent>() ||
        candidate->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }
    auto const* candidate_unit =
        candidate->get_component<Engine::Core::UnitComponent>();
    auto const* candidate_transform =
        candidate->get_component<Engine::Core::TransformComponent>();
    if (candidate_unit == nullptr || candidate_transform == nullptr ||
        candidate_unit->health <= 0 ||
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
  auto* commander = attacker.get_component<Engine::Core::CommanderComponent>();
  bool const signature_strike =
      commander != nullptr && commander->signature_strike_active;
  float const reach =
      (attack != nullptr ? attack->melee_range : definition.hit_shape.reach) +
      (signature_strike ? std::max(0.0F, commander->signature_bonus_reach) : 0.0F);
  float const yaw = attacker_transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const facing =
      (std::sin(yaw) * dx + std::cos(yaw) * dz) / std::max(distance, 0.0001F);

  if (!is_in_range(&attacker,
                   target,
                   reach +
                       Engine::Core::AttackComponent::k_melee_contact_range_grace) ||
      facing < std::cos(80.0F * std::numbers::pi_v<float> / 180.0F)) {
    action.action_running = false;
    action.action_completed = true;
    return;
  }
  int const damage = std::max(1, action.requested_damage);
  deal_damage(&world, target, damage, attacker.get_id());
  action.last_hit_target_id = target->get_id();
  action.last_damage = damage;
  action.hit_target_ids[0] = target->get_id();
  action.hit_target_count = 1U;

  if (!signature_strike) {
    return;
  }
  apply_commander_signature_effects(
      world, attacker, *commander, *target, *attacker_transform, reach, damage);
  commander->signature_strike_active = false;
}

void deal_weapon_trace_damage(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::CombatStateComponent* presentation_state,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) {
  if (!action.weapon_trace_active) {
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
  auto const contact = Game::Systems::CombatActions::find_weapon_trace_contact(
      world,
      attacker,
      definition,
      {.previous_normalized_time = action.previous_normalized_action_time,
       .current_normalized_time = action.normalized_action_time},
      action.active_target_id,
      ignored_targets);
  if (contact.target_id == 0) {
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
                   .local_right = contact.local_right},
       .damage_profile = definition.damage,

       .explicit_raw_damage = is_rts_melee_action(definition.id)
                                  ? std::max(1, action.requested_damage)
                                  : 0});
  if (!result.attempted) {
    return;
  }

  if (presentation_state != nullptr) {
    presentation_state->damage_dealt_this_swing = true;
  }
  action.last_hit_target_id = contact.target_id;
  action.last_hit_soldier_slot = contact.target_soldier_slot;
  action.last_damage = result.damage.effective_damage;
  action.hit_target_ids[action.hit_target_count++] = contact.target_id;
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

void handle_action_events(
    Engine::Core::World& world,
    Engine::Core::Entity& entity,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    std::span<const Game::Systems::CombatActions::CombatActionEvent> events) {
  handle_mounted_charge_action_events(entity, action, definition, events);
  for (auto const& event : events) {
    auto const action_id = static_cast<Game::Systems::CombatActions::CombatActionId>(
        action.combat_action_id);
    if (event.type ==
            Game::Systems::CombatActions::CombatActionEventType::ActiveStart &&
        is_rts_melee_action(action_id) && action.hit_target_count == 0U) {
      bool const requires_visible_weapon_contact =
          target_uses_rpg_combat(world, action.active_target_id) &&
          action_id != Game::Systems::CombatActions::CombatActionId::RtsElephantStomp;
      if (!requires_visible_weapon_contact) {
        deal_rts_melee_contact_damage(world, entity, action, definition);
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
      action->cancel_window_active = false;
      action->phase = Engine::Core::RpgCommanderActionPhase::None;
      if (presentation_state != nullptr) {
        presentation_state->animation_state = Engine::Core::CombatAnimationState::Idle;
        presentation_state->state_time = 0.0F;
        presentation_state->state_duration = 0.0F;
        presentation_state->input_buffered = false;
      }
      return;
    }
  }

  if (is_rts_attack_action(action_id)) {
    auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
    auto const* target = entity.get_component<Engine::Core::AttackTargetComponent>();
    bool const interrupted = unit == nullptr || unit->health <= 0 ||
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

  auto const events = Game::Systems::CombatActions::advance_combat_action_events(
      *action, delta_time, *definition);
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
