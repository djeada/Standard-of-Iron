#include "rts_commander_attack.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/commander_catalog.h"
#include "../combat_actions/combat_action_definition.h"
#include "../combat_actions/combat_action_events.h"
#include "../combat_rules.h"
#include "attack_control.h"
#include "combat_types.h"
#include "commander_duel.h"
#include "melee_exchange.h"
#include "target_rules.h"

namespace Game::Systems::Combat {

namespace {

[[nodiscard]] auto
throws_telegraphed_heavy(Engine::Core::Entity& attacker,
                         const Engine::Core::Entity& target,
                         Engine::Core::CombatAttackFamily family) -> bool {

  if (family != Engine::Core::CombatAttackFamily::Sword ||
      !Game::Systems::CombatRules::uses_rpg_combat_rules(&target)) {
    return false;
  }

  auto* action = attacker.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr) {
    return false;
  }

  constexpr std::uint32_t k_heavy_every = 4U;
  std::uint32_t const seed =
      static_cast<std::uint32_t>(attacker.get_id() * 2654435761U) ^ 0x85EBCA6BU;
  std::uint32_t const swing = action->melee_attack_sequence + (seed % k_heavy_every);
  return (swing % k_heavy_every) == 0U;
}

auto claim_commander_signature(Engine::Core::Entity* attacker, int& damage)
    -> std::optional<Game::Systems::CombatActions::CombatActionId> {
  auto* commander = attacker->get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || commander->fpv_controlled ||
      commander->signature_move ==
          static_cast<std::uint8_t>(Game::Units::CommanderSignatureMove::None) ||
      commander->signature_cooldown_remaining > 0.0F || commander->wounded) {
    return std::nullopt;
  }

  auto const move =
      static_cast<Game::Units::CommanderSignatureMove>(commander->signature_move);
  std::optional<Game::Systems::CombatActions::CombatActionId> action_id;
  switch (move) {
  case Game::Units::CommanderSignatureMove::BracingThrust:
  case Game::Units::CommanderSignatureMove::PhalanxSweep:
    action_id = Game::Systems::CombatActions::CombatActionId::RtsCommanderThrust;
    break;
  case Game::Units::CommanderSignatureMove::ConsularRiposte:
  case Game::Units::CommanderSignatureMove::EncirclingCut:
    action_id = Game::Systems::CombatActions::CombatActionId::RtsCommanderCut;
    break;
  case Game::Units::CommanderSignatureMove::PointBlankVolley:
  case Game::Units::CommanderSignatureMove::HuntingShot:

    action_id = Game::Systems::CombatActions::CombatActionId::RtsCommanderShot;
    break;
  case Game::Units::CommanderSignatureMove::None:
    return std::nullopt;
  }

  commander->signature_cooldown_remaining = commander->signature_cooldown;
  commander->signature_strike_active = true;
  damage = std::max(
      1,
      static_cast<int>(static_cast<float>(damage) *
                       std::max(1.0F, commander->signature_damage_multiplier)));
  return action_id;
}

auto commander_weapon_family(Engine::Core::CombatAttackFamily family)
    -> Game::Systems::CombatActions::WeaponFamily {
  using Game::Systems::CombatActions::WeaponFamily;
  switch (family) {
  case Engine::Core::CombatAttackFamily::Sword:
    return WeaponFamily::Sword;
  case Engine::Core::CombatAttackFamily::Spear:
    return WeaponFamily::Spear;
  case Engine::Core::CombatAttackFamily::Bow:
    return WeaponFamily::Bow;
  case Engine::Core::CombatAttackFamily::None:
    break;
  }
  return WeaponFamily::None;
}

auto planar_distance(const Engine::Core::TransformComponent& a,
                     const Engine::Core::TransformComponent& b) -> float {
  return std::hypot(b.position.x - a.position.x, b.position.z - a.position.z);
}

auto count_enemies_in_crowd_radius(Engine::Core::World& world,
                                   Engine::Core::Entity& attacker,
                                   const Engine::Core::TransformComponent& origin)
    -> int {
  constexpr float k_crowd_radius_sq = 12.25F;
  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr) {
    return 0;
  }
  int nearby_enemies = 0;
  for (auto [candidate_ref, candidate_unit, candidate_transform] :
       world.entity_view<Engine::Core::UnitComponent,
                         Engine::Core::TransformComponent>()) {
    if (!may_attack(
            attacker_unit,
            &candidate_ref,
            {.intent = EngagementIntent::AutoAcquired, .allow_buildings = false})) {
      continue;
    }
    float const dx = candidate_transform.position.x - origin.position.x;
    float const dz = candidate_transform.position.z - origin.position.z;
    if ((dx * dx) + (dz * dz) <= k_crowd_radius_sq) {
      ++nearby_enemies;
    }
  }
  return nearby_enemies;
}

auto pick_grounded_intent(Engine::Core::World& world,
                          Engine::Core::Entity& attacker,
                          const Engine::Core::TransformComponent& attacker_transform,
                          const Engine::Core::TransformComponent& target_transform)
    -> Engine::Core::CommanderCombatIntentType {
  auto const* attack = attacker.get_component<Engine::Core::AttackComponent>();
  int const nearby_enemies =
      count_enemies_in_crowd_radius(world, attacker, attacker_transform);
  auto const* action =
      attacker.get_component<Engine::Core::RpgCommanderActionComponent>();
  std::uint8_t const sequence = action != nullptr ? action->melee_attack_sequence : 0U;
  float const distance = planar_distance(attacker_transform, target_transform);
  float const normal_reach = attack != nullptr ? attack->melee_range : 2.0F;
  if (nearby_enemies >= 3 && (sequence % 3U) == 2U) {
    return Engine::Core::CommanderCombatIntentType::Special;
  }
  if (distance > normal_reach * 1.05F || (sequence % 4U) == 3U) {
    return Engine::Core::CommanderCombatIntentType::Heavy;
  }
  return Engine::Core::CommanderCombatIntentType::Light;
}

auto rts_commander_action(Engine::Core::World& world,
                          Engine::Core::Entity& attacker,
                          Engine::Core::Entity& target,
                          Engine::Core::CombatAttackFamily family)
    -> Game::Systems::CombatActions::CombatActionId {
  using Game::Systems::CombatActions::CombatActionId;
  using Game::Systems::CombatActions::CommanderActionRole;
  auto* commander = attacker.get_component<Engine::Core::CommanderComponent>();
  auto const* attack = attacker.get_component<Engine::Core::AttackComponent>();
  auto const* attacker_transform =
      attacker.get_component<Engine::Core::TransformComponent>();
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  if (commander == nullptr || !commander->advanced_combat_enabled ||
      attacker_transform == nullptr || target_transform == nullptr ||
      family == Engine::Core::CombatAttackFamily::None) {
    return CombatActionId::None;
  }

  auto const current_id = commander->combo_window_remaining > 0.0F
                              ? static_cast<CombatActionId>(commander->combo_action_id)
                              : CombatActionId::None;
  auto const* current =
      Game::Systems::CombatActions::find_combat_action_definition(current_id);
  Engine::Core::CommanderCombatIntentType intent =
      Engine::Core::CommanderCombatIntentType::Light;

  if (current != nullptr && current->role == CommanderActionRole::Launcher) {
    intent = Engine::Core::CommanderCombatIntentType::Jump;
    commander->jump_active = true;
    commander->jump_phase = 0.0F;
    commander->airborne_velocity = 5.8F;
  } else if (commander->jump_active) {
    intent = current != nullptr && current->role == CommanderActionRole::Aerial
                 ? Engine::Core::CommanderCombatIntentType::Heavy
                 : Engine::Core::CommanderCombatIntentType::Light;
  } else {
    intent =
        pick_grounded_intent(world, attacker, *attacker_transform, *target_transform);
  }

  float const distance = planar_distance(*attacker_transform, *target_transform);
  float const normal_reach = attack != nullptr ? attack->melee_range : 2.0F;
  return Game::Systems::CombatActions::resolve_commander_action(
      current_id,
      intent,
      commander_weapon_family(family),
      commander->jump_active,
      distance > normal_reach * 1.05F);
}

void record_commander_combo(
    Engine::Core::Entity* attacker,
    Game::Systems::CombatActions::CombatActionId id,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) {
  auto* commander = attacker->get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || !definition.commander_only) {
    return;
  }
  commander->combo_action_id = static_cast<std::uint8_t>(id);
  commander->combo_window_remaining =
      definition.duration_seconds +
      Engine::Core::CommanderBodyControlComponent::k_chain_window_seconds;
  commander->dive_attack_active =
      definition.role == Game::Systems::CombatActions::CommanderActionRole::Dive;
  if (commander->dive_attack_active) {
    commander->airborne_velocity = -11.0F;
  }
}

auto routine_melee_action(Engine::Core::Entity* attacker,
                          Engine::Core::Entity* target,
                          Engine::Core::CombatAttackFamily family)
    -> Game::Systems::CombatActions::CombatActionId {
  using Game::Systems::CombatActions::CombatActionId;
  if (attacker->has_component<Engine::Core::ElephantComponent>()) {
    return CombatActionId::RtsElephantStomp;
  }
  if (family == Engine::Core::CombatAttackFamily::Spear) {
    return CombatActionId::RtsSpearThrust;
  }
  return throws_telegraphed_heavy(*attacker, *target, family)
             ? CombatActionId::RtsHeavyOverhead
             : CombatActionId::RtsSwordStrike;
}

auto is_chaining_from_commander_link(
    const Engine::Core::RpgCommanderActionComponent& action) -> bool {
  if (!action.action_running || action.combat_action_id == 0U) {
    return false;
  }
  auto const* running = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action.combat_action_id));
  return running != nullptr && running->commander_only;
}

} // namespace

auto commander_link_still_swinging(Engine::Core::Entity* attacker) -> bool {
  auto const* commander = attacker->get_component<Engine::Core::CommanderComponent>();
  auto const* action =
      attacker->get_component<Engine::Core::RpgCommanderActionComponent>();
  if (commander == nullptr || commander->fpv_controlled ||
      !commander->advanced_combat_enabled || action == nullptr ||
      !action->action_running || action->combat_action_id == 0U) {
    return false;
  }
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action->combat_action_id));
  if (definition == nullptr || !definition->commander_only) {
    return false;
  }
  float const exit_safe = Game::Systems::CombatActions::action_event_normalized_time(
      *definition,
      Game::Systems::CombatActions::CombatActionEventType::ExitSafe,
      0.92F);
  return action->normalized_action_time < exit_safe;
}

void begin_rts_melee_action(Engine::Core::World& world,
                            Engine::Core::Entity* attacker,
                            Engine::Core::Entity* target,
                            int damage) {
  using Game::Systems::CombatActions::CombatActionId;
  auto* unit = attacker->get_component<Engine::Core::UnitComponent>();
  auto* action =
      Engine::Core::get_or_add_component<Engine::Core::RpgCommanderActionComponent>(
          attacker);
  if (unit == nullptr || action == nullptr) {
    return;
  }
  auto const family = Engine::Core::resolve_combat_attack_family(
      unit->spawn_type, Engine::Core::AttackComponent::CombatMode::Melee);
  bool const chaining_from_link = is_chaining_from_commander_link(*action);
  auto const duel_link = claim_duel_link(*attacker, *target);
  auto const signature = duel_permits_signature(*attacker)
                             ? claim_commander_signature(attacker, damage)
                             : std::nullopt;
  auto const routine_id = routine_melee_action(attacker, target, family);
  auto const commander_id =
      duel_link.has_value() ? *duel_link
                            : rts_commander_action(world, *attacker, *target, family);
  auto const id =
      signature.has_value()
          ? *signature
          : (commander_id != CombatActionId::None ? commander_id : routine_id);
  action->phase = Engine::Core::RpgCommanderActionPhase::Strike;
  action->combat_action_id = static_cast<std::uint8_t>(id);
  action->active_target_id = target->get_id();
  action->active_target_soldier_slot =
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  action->requested_damage = damage;
  auto const* definition =
      Game::Systems::CombatActions::find_combat_action_definition(id);
  action->action_duration =
      definition != nullptr ? std::max(0.001F, definition->duration_seconds) : 1.0F;
  action->melee_attack_sequence =
      static_cast<std::uint8_t>((action->melee_attack_sequence + 1U) % 250U);
  if (definition != nullptr) {
    record_commander_combo(attacker, id, *definition);
  }

  bool const exchange_applies =
      !signature.has_value() && commander_id == CombatActionId::None &&
      !attacker->has_component<Engine::Core::ElephantComponent>();
  auto const beat =
      exchange_applies
          ? resolve_melee_exchange_beat(attacker->get_id(),
                                        target->get_id(),
                                        action->melee_attack_sequence,
                                        melee_target_can_defend(attacker, target))
          : MeleeExchangeBeat{};
  action->exchange_outcome = static_cast<std::uint8_t>(beat.outcome);
  float entry_time = 0.0F;
  if (chaining_from_link && definition != nullptr && definition->commander_only) {
    entry_time = Game::Systems::CombatActions::action_event_normalized_time(
        *definition,
        Game::Systems::CombatActions::CombatActionEventType::WindupStart,
        0.0F);
  }
  Game::Systems::CombatActions::reset_combat_action_event_runtime(*action, entry_time);
}

auto resolve_melee_swing_cadence(Engine::Core::Entity* attacker,
                                 Engine::Core::Entity* target,
                                 float cooldown) -> float {
  float const base_delay =
      deterministic_attack_delay(attacker->get_id(), target->get_id(), cooldown);
  auto const* action =
      attacker->get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr || attacker->has_component<Engine::Core::ElephantComponent>() ||
      !melee_target_can_defend(attacker, target)) {
    return -base_delay;
  }

  auto const* commander = attacker->get_component<Engine::Core::CommanderComponent>();
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action->combat_action_id));
  float link_length = 0.0F;
  if (commander != nullptr && !commander->fpv_controlled &&
      commander->advanced_combat_enabled && definition != nullptr &&
      definition->commander_only) {
    float const exit_safe = Game::Systems::CombatActions::action_event_normalized_time(
        *definition,
        Game::Systems::CombatActions::CombatActionEventType::ExitSafe,
        0.92F);
    link_length = std::max(0.05F, action->action_duration * exit_safe);
  }

  auto const next_beat = resolve_melee_exchange_beat(
      attacker->get_id(),
      target->get_id(),
      static_cast<std::uint8_t>((action->melee_attack_sequence + 1U) % 250U),
      true);
  if (auto const duel_cadence = duel_swing_cadence(*attacker, cooldown, link_length)) {
    return *duel_cadence;
  }
  float const interval = cooldown * next_beat.interval_weight;
  float const delay = base_delay * next_beat.delay_weight;
  return cooldown - std::max(interval + delay, link_length);
}

void begin_rts_bow_action(Engine::Core::World& world,
                          Engine::Core::Entity* attacker,
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
  auto const advanced = rts_commander_action(
      world, *attacker, *target, Engine::Core::CombatAttackFamily::Bow);
  int authored_damage = damage;
  auto const signature = claim_commander_signature(attacker, authored_damage);
  auto const id =
      signature.has_value()
          ? *signature
          : (advanced != Game::Systems::CombatActions::CombatActionId::None
                 ? advanced
                 : Game::Systems::CombatActions::CombatActionId::RtsBowShot);
  action->combat_action_id = static_cast<std::uint8_t>(id);
  action->active_target_id = target->get_id();
  action->active_target_soldier_slot =
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  action->requested_damage = authored_damage;
  auto const* definition =
      Game::Systems::CombatActions::find_combat_action_definition(id);
  action->action_duration = definition != nullptr
                                ? std::max(0.001F, definition->duration_seconds)
                                : std::max(0.001F, duration);
  if (auto* commander = attacker->get_component<Engine::Core::CommanderComponent>();
      commander != nullptr && definition != nullptr && definition->commander_only) {
    commander->combo_action_id = static_cast<std::uint8_t>(id);
    commander->combo_window_remaining =
        definition->duration_seconds +
        Engine::Core::CommanderBodyControlComponent::k_chain_window_seconds;
  }
  Game::Systems::CombatActions::reset_combat_action_event_runtime(*action);
}
} // namespace Game::Systems::Combat
