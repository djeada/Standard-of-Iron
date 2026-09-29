#include "combat_action_processor.h"

#include <algorithm>
#include <span>

#include "../../audio/cue_ids.h"
#include "../../core/component_gameplay.h"
#include "../../core/entity.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../combat_actions/combat_action_definition.h"
#include "../combat_actions/combat_action_events.h"
#include "../combat_actions/projectile_release.h"
#include "../rpg_combat_system/rpg_bow_draw.h"
#include "../rpg_combat_system/rpg_bow_shot.h"
#include "action_contact_damage.h"
#include "attack_processor.h"
#include "combat_action_predicates.h"
#include "combat_utils.h"
#include "commander_root_motion.h"
#include "commander_signature_effects.h"
#include "elephant_special_processor.h"
#include "mounted_charge_processor.h"
#include "rts_melee_contact.h"

namespace Game::Systems::Combat {

namespace {

namespace CA = Game::Systems::CombatActions;

void cancel_authored_action(Engine::Core::RpgCommanderActionComponent& action,
                            Engine::Core::CombatStateComponent* presentation_state) {
  halt_action(action);
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

void handle_rts_melee_contact_event(Engine::Core::World& world,
                                    Engine::Core::Entity& entity,
                                    Engine::Core::RpgCommanderActionComponent& action,
                                    const CA::CombatActionDefinition& definition,
                                    CA::CombatActionId action_id) {
  if (!melee_contact_comes_from_the_sweep(world, entity, definition, action)) {
    deal_rts_melee_contact_damage(world, entity, action, definition);
  } else if (!rts_melee_target_still_stands(world, entity, action)) {
    halt_action(action);
  }
  if (action_id == CA::CombatActionId::RtsElephantStomp &&
      action.hit_target_count > 0U) {
    (void)apply_elephant_stomp_impact(&world, &entity);
  }
}

void release_rts_shot(Engine::Core::World& world,
                      Engine::Core::Entity& entity,
                      Engine::Core::RpgCommanderActionComponent& action,
                      const CA::CombatActionDefinition& definition) {
  auto const* special = entity.get_component<Engine::Core::SpecialAttackComponent>();
  bool released = false;
  if (special != nullptr && special->use_projectile_system) {
    released = CA::release_projectile_for_action(&world,
                                                 entity,
                                                 definition,
                                                 action.active_target_id,
                                                 action.requested_damage)
                   .released;
  } else {
    released = release_rts_arrow_volley(
        world, entity, action.active_target_id, action.requested_damage);
  }
  if (!released) {
    return;
  }
  action.last_hit_target_id = action.active_target_id;
  action.last_damage = std::max(1, action.requested_damage);
  apply_signature_shot_effects(world, entity, action.active_target_id);
}

void loose_commander_arrow(Engine::Core::World& world,
                           Engine::Core::Entity& entity,
                           Engine::Core::RpgCommanderActionComponent& action,
                           const CA::CombatActionDefinition& definition) {
  auto const loosed =
      Game::Systems::RpgCombat::loose_aimed_arrow(world, entity, definition);
  if (loosed.released) {
    action.active_target_id = loosed.target_id;
    action.active_target_soldier_slot = loosed.soldier_slot;
    action.last_hit_target_id = loosed.target_id;
    action.last_damage = loosed.damage;
  }
}

void handle_projectile_release_event(Engine::Core::World& world,
                                     Engine::Core::Entity& entity,
                                     Engine::Core::RpgCommanderActionComponent& action,
                                     const CA::CombatActionDefinition& definition,
                                     CA::CombatActionId action_id) {
  if (action_id == CA::CombatActionId::RtsBowShot ||
      action_id == CA::CombatActionId::RtsCommanderShot) {
    release_rts_shot(world, entity, action, definition);
    return;
  }
  if (definition.weapon_family == CA::WeaponFamily::Bow &&
      !is_rts_attack_action(action_id) &&
      entity.has_component<Engine::Core::RpgCommanderAimComponent>()) {
    loose_commander_arrow(world, entity, action, definition);
    return;
  }
  auto const release = CA::release_projectile_for_action(
      &world, entity, definition, action.active_target_id);
  if (release.released) {
    action.last_hit_target_id = release.target_id;
    action.last_damage = release.damage;
  }
}

void handle_action_events(Engine::Core::World& world,
                          Engine::Core::Entity& entity,
                          Engine::Core::RpgCommanderActionComponent& action,
                          const CA::CombatActionDefinition& definition,
                          std::span<const CA::CombatActionEvent> events) {
  handle_mounted_charge_action_events(entity, action, definition, events);
  auto const contact_event = action_contact_event(definition);
  auto const action_id = static_cast<CA::CombatActionId>(action.combat_action_id);
  for (auto const& event : events) {
    bool const advanced_rts_melee = is_advanced_rts_commander_melee(entity, definition);
    if (event.type == CA::CombatActionEventType::WeaponTraceStart) {
      record_swing_for_weapon_trace_start(world, entity, definition, action_id);
    }
    if (event.type == contact_event &&
        (is_rts_melee_action(action_id) || advanced_rts_melee) &&
        action.hit_target_count == 0U) {
      handle_rts_melee_contact_event(world, entity, action, definition, action_id);
      continue;
    }
    if (event.type == CA::CombatActionEventType::ProjectileRelease) {
      handle_projectile_release_event(world, entity, action, definition, action_id);
    }
  }
}

[[nodiscard]] auto
interrupts_rts_attack(Engine::Core::Entity& entity,
                      const Engine::Core::RpgCommanderActionComponent& action,
                      CA::CombatActionId action_id) -> bool {
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  auto const* target = entity.get_component<Engine::Core::AttackTargetComponent>();
  bool const shooting_from_a_melee =
      !is_rts_melee_action(action_id) &&
      action_id != CA::CombatActionId::RtsCommanderShot && in_rts_melee_lock(&entity);
  return unit == nullptr || unit->health <= 0 || shooting_from_a_melee ||
         entity.has_component<Engine::Core::StaggerComponent>() ||
         (target != nullptr && target->target_id != 0 &&
          target->target_id != action.active_target_id);
}

[[nodiscard]] auto fpv_commander_is_staggered_out(
    Engine::Core::Entity& entity,
    const Engine::Core::RpgCommanderActionComponent& action) -> bool {
  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || !commander->fpv_controlled || !action.action_running) {
    return false;
  }
  auto const* stagger = entity.get_component<Engine::Core::StaggerComponent>();
  return stagger != nullptr && stagger->tier != Engine::Core::StaggerTier::LightFlinch;
}

void deal_contact_damage_for_tick(
    Engine::Core::World& world,
    Engine::Core::Entity& entity,
    Engine::Core::CombatStateComponent* presentation_state,
    Engine::Core::RpgCommanderActionComponent& action,
    const CA::CombatActionDefinition& definition,
    CA::CombatActionId action_id) {
  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  bool const attacks_rpg_target =
      is_rts_melee_action(action_id) &&
      action_id != CA::CombatActionId::RtsElephantStomp &&
      target_uses_rpg_combat(world, action.active_target_id);
  if ((commander != nullptr && commander->fpv_controlled) || attacks_rpg_target) {
    deal_weapon_trace_damage(world, entity, presentation_state, action, definition);
  }
  deal_mount_body_impact(world, entity, action, definition);
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
  auto const action_id = static_cast<CA::CombatActionId>(action->combat_action_id);
  auto const* definition = CA::find_combat_action_definition(action_id);
  if (definition == nullptr) {
    return;
  }

  if (fpv_commander_is_staggered_out(entity, *action)) {
    cancel_authored_action(*action, presentation_state);
    return;
  }
  if (is_rts_attack_action(action_id) &&
      interrupts_rts_attack(entity, *action, action_id)) {
    halt_action(*action);
    return;
  }

  float action_delta = delta_time;
  if (action_id == CA::CombatActionId::RpgBowShot) {
    auto const draw =
        update_commander_bow_draw(entity, *action, *definition, delta_time);
    if (draw.relaxed) {
      cancel_authored_action(*action, presentation_state);
      return;
    }
    action_delta = draw.allowed_delta;
  }

  auto const events =
      CA::advance_combat_action_events(*action, action_delta, *definition);
  apply_rts_commander_root_motion(*world, entity, *action, *definition, delta_time);
  handle_action_events(*world, entity, *action, *definition, events);
  deal_contact_damage_for_tick(
      *world, entity, presentation_state, *action, *definition, action_id);
}

} // namespace Game::Systems::Combat
