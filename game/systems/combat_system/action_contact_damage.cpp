#include "action_contact_damage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

#include "../../core/world.h"
#include "../combat_actions/body_impact.h"
#include "../combat_actions/combat_action_events.h"
#include "../combat_actions/weapon_trace.h"
#include "../rpg_combat_system/rpg_targeting.h"
#include "authored_action_reaction.h"
#include "combat_action_predicates.h"
#include "combat_hit_resolver.h"
#include "combat_utils.h"
#include "commander_signature_effects.h"
#include "damage_application.h"
#include "rts_melee_contact.h"
#include "target_rules.h"

namespace Game::Systems::Combat {

namespace {

namespace CA = Game::Systems::CombatActions;

using ActionRef = Engine::Core::RpgCommanderActionComponent;

[[nodiscard]] auto
hit_capacity(const ActionRef& action,
             const CA::CombatActionDefinition& definition) -> std::uint8_t {
  return std::min<std::uint8_t>(
      action.hit_target_ids.size(),
      static_cast<std::uint8_t>(std::max(0, definition.max_targets)));
}

void record_hit_on_action(ActionRef& action,
                          Engine::Core::EntityID target_id,
                          std::uint16_t soldier_slot,
                          int damage) {
  action.last_hit_target_id = target_id;
  action.last_hit_soldier_slot = soldier_slot;
  action.last_damage = damage;
  action.hit_target_ids[action.hit_target_count] = target_id;
  action.hit_target_soldier_slots[action.hit_target_count] = soldier_slot;
  ++action.hit_target_count;
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

struct SweptWindow {
  float from{0.0F};
  float to{0.0F};
};

[[nodiscard]] auto swept_window(const ActionRef& action,
                                const CA::CombatActionDefinition& definition,
                                bool sweep_decides) -> std::optional<SweptWindow> {
  using CA::CombatActionEventType;
  float const window_start = CA::action_event_normalized_time(
      definition, CombatActionEventType::WeaponTraceStart, 1.0F);
  float const window_end = CA::action_event_normalized_time(
      definition, CombatActionEventType::WeaponTraceEnd, window_start);
  SweptWindow window{action.previous_normalized_action_time,
                     action.normalized_action_time};
  if (!sweep_decides) {
    if (!action.weapon_trace_active) {
      return std::nullopt;
    }
    return window;
  }
  if (window_end <= window_start) {
    return std::nullopt;
  }
  window.from = std::max(window.from, window_start);
  window.to = std::min(window.to, window_end);
  if (window.to <= window.from) {
    return std::nullopt;
  }
  return window;
}

struct IgnoredTargetLists {
  std::array<CA::WeaponTraceIgnoredTarget, ActionRef::k_max_action_hit_targets>
      entries{};
  std::span<const Engine::Core::EntityID> ids{};
  std::span<const CA::WeaponTraceIgnoredTarget> slot_span{};

  IgnoredTargetLists() = default;
  IgnoredTargetLists(const IgnoredTargetLists&) = delete;
  auto operator=(const IgnoredTargetLists&) -> IgnoredTargetLists& = delete;
};

void collect_ignored_targets(IgnoredTargetLists& lists,
                             const ActionRef& action,
                             const CA::CombatActionDefinition& definition) {
  if (!definition.can_hit_same_target_once) {
    return;
  }
  if (!definition.commander_only) {
    lists.ids = {action.hit_target_ids.data(), action.hit_target_count};
    return;
  }
  for (std::uint8_t index = 0; index < action.hit_target_count; ++index) {
    lists.entries[index] = {
        .entity_id = action.hit_target_ids[index],
        .soldier_slot = action.hit_target_soldier_slots[index],
    };
  }
  lists.slot_span = {lists.entries.data(), action.hit_target_count};
}

[[nodiscard]] auto
resolve_trace_hit(Engine::Core::World& world,
                  Engine::Core::Entity& attacker,
                  const ActionRef& action,
                  const CA::CombatActionDefinition& definition,
                  const CA::WeaponTraceContact& contact) -> CombatHitResult {
  return resolve_commander_action_hit(
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
}

void record_player_swing_contact(Engine::Core::World& world,
                                 Engine::Core::Entity& attacker,
                                 const Engine::Core::Entity& struck,
                                 const CA::CombatActionDefinition& definition,
                                 const CombatHitResult& result) {
  auto const* commander =
      world.try_get<Engine::Core::CommanderComponent>(attacker.get_id());
  if (!result.applied ||
      !commander_swings_under_player_control(commander, definition)) {
    return;
  }
  auto const* attacker_transform =
      world.try_get<Engine::Core::TransformComponent>(attacker.get_id());
  auto const* target_transform =
      world.try_get<Engine::Core::TransformComponent>(struck.get_id());
  if (attacker_transform == nullptr || target_transform == nullptr) {
    return;
  }
  bool const signature_strike = commander->signature_strike_active;
  record_signature_contact(attacker,
                           *attacker_transform,
                           *target_transform,
                           commander_strike_form(definition, commander),
                           commander_strike_intensity(definition, signature_strike),
                           std::max(definition.hit_shape.reach, 1.2F));
}

void present_trace_impact(Engine::Core::CombatStateComponent& presentation_state,
                          const CA::CombatActionDefinition& definition,
                          float contact_speed) {
  presentation_state.damage_dealt_this_swing = true;
  presentation_state.is_hit_paused = true;
  presentation_state.hit_pause_remaining = std::max(
      presentation_state.hit_pause_remaining,
      definition.commander_only
          ? authored_hit_stop_seconds(definition)
          : Engine::Core::CombatStateComponent::k_combat_animation_hit_pause_duration *
                std::clamp(contact_speed /
                               Game::Systems::Combat::k_reference_weapon_speed,
                           0.5F,
                           1.6F));
  presentation_state.telegraph_cue = Engine::Core::TelegraphCue::Impact;
}

[[nodiscard]] auto
resolve_rts_swing_contact(Engine::Core::World& world,
                          Engine::Core::Entity& attacker,
                          Engine::Core::CombatStateComponent* presentation_state,
                          ActionRef& action,
                          const CA::CombatActionDefinition& definition,
                          const CA::WeaponTraceContact& contact) -> bool {
  if (!is_rts_melee_action(definition.id) ||
      target_uses_rpg_combat(world, contact.target_id)) {
    return false;
  }
  if (auto* struck = world.get_entity(contact.target_id); struck != nullptr) {
    resolve_rts_melee_contact(world, attacker, action, definition, *struck);
    if (presentation_state != nullptr) {
      presentation_state->damage_dealt_this_swing = true;
    }
    action.last_hit_soldier_slot = contact.target_soldier_slot;
  }
  return true;
}

} // namespace

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
        !may_attack(attacker_unit,
                    &candidate,
                    {.intent = EngagementIntent::Ordered, .allow_buildings = false})) {
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

void deal_weapon_trace_damage(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::CombatStateComponent* presentation_state,
    Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) {
  bool const sweep_decides =
      melee_contact_comes_from_the_sweep(world, attacker, definition, action);
  auto const window = swept_window(action, definition, sweep_decides);
  if (!window.has_value()) {
    return;
  }
  auto const capacity = hit_capacity(action, definition);
  if (capacity == 0U || action.hit_target_count >= capacity) {
    return;
  }

  IgnoredTargetLists ignored;
  collect_ignored_targets(ignored, action, definition);
  auto const contact = CA::find_weapon_trace_contact(
      world,
      attacker,
      definition,
      {.previous_normalized_time = window->from, .current_normalized_time = window->to},
      action.active_target_id,
      ignored.ids,
      ignored.slot_span);
  if (contact.target_id == 0) {
    return;
  }
  if (resolve_rts_swing_contact(
          world, attacker, presentation_state, action, definition, contact)) {
    return;
  }

  auto const result = resolve_trace_hit(world, attacker, action, definition, contact);
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
    record_player_swing_contact(world, attacker, *struck, definition, result);
  }
  if (presentation_state != nullptr) {
    present_trace_impact(*presentation_state, definition, contact.contact_speed);
  }
  bool const first_hit = action.hit_target_count == 0U;
  action.last_contact_speed = contact.contact_speed;
  record_hit_on_action(action,
                       contact.target_id,
                       contact.target_soldier_slot,
                       result.damage.effective_damage);
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

} // namespace Game::Systems::Combat
