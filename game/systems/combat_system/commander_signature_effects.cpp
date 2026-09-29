#include "commander_signature_effects.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../core/world.h"
#include "combat_action_predicates.h"
#include "combat_utils.h"
#include "damage_application.h"
#include "damage_processor.h"

namespace Game::Systems::Combat {

namespace {

auto commander_strike_span(
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    Engine::Core::CommanderSignatureForm form) -> float {
  float span = 0.42F;
  switch (form) {
  case Engine::Core::CommanderSignatureForm::Sweep:
    span = 0.86F;
    break;
  case Engine::Core::CommanderSignatureForm::Slam:
    span = 0.50F;
    break;
  case Engine::Core::CommanderSignatureForm::Thrust:
  case Engine::Core::CommanderSignatureForm::Shot:
    span = 0.10F;
    break;
  case Engine::Core::CommanderSignatureForm::Cut:
    span = 0.46F;
    break;
  }
  if (definition.attack_direction == Engine::Core::AttackDirection::LeftSlash) {
    span = -span;
  }
  return span;
}

void push_commander_cue(
    Engine::Core::Entity& attacker,
    const Engine::Core::CommanderSignaturePresentationComponent::Entry& entry) {
  auto* presentation = Engine::Core::get_or_add_component<
      Engine::Core::CommanderSignaturePresentationComponent>(&attacker);
  if (presentation == nullptr) {
    return;
  }
  if (presentation->entries.size() >=
      Engine::Core::CommanderSignaturePresentationComponent::k_max_entries) {
    presentation->entries.erase(presentation->entries.begin());
  }
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

void record_contact_for_action(
    Engine::Core::Entity& attacker,
    Engine::Core::CommanderComponent& commander,
    const Engine::Core::TransformComponent& attacker_transform,
    const Engine::Core::Entity& primary_target,
    float reach) {
  auto const* target_transform =
      primary_target.get_component<Engine::Core::TransformComponent>();
  auto const* action =
      attacker.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (target_transform == nullptr || action == nullptr) {
    return;
  }
  auto const action_id = static_cast<Game::Systems::CombatActions::CombatActionId>(
      action->combat_action_id);
  auto const* definition =
      Game::Systems::CombatActions::find_combat_action_definition(action_id);
  record_signature_contact(attacker,
                           attacker_transform,
                           *target_transform,
                           definition != nullptr
                               ? commander_strike_form(*definition, &commander)
                               : signature_form_for_action(action_id),
                           1.0F,
                           reach);
}

void sweep_secondary_targets(Engine::Core::World& world,
                             Engine::Core::Entity& attacker,
                             Engine::Core::CommanderComponent& commander,
                             Engine::Core::Entity& primary_target,
                             const Engine::Core::TransformComponent& attacker_transform,
                             float reach,
                             int damage) {
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
        is_building(candidate)) {
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

} // namespace

auto commander_strike_form(
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const Engine::Core::CommanderComponent* commander)
    -> Engine::Core::CommanderSignatureForm {
  using Game::Systems::CombatActions::CombatActionId;
  using Game::Systems::CombatActions::CommanderActionRole;
  using Game::Systems::CombatActions::WeaponFamily;
  if (definition.weapon_family == WeaponFamily::Bow) {
    return Engine::Core::CommanderSignatureForm::Shot;
  }
  if (definition.id == CombatActionId::CommanderSwordSpin ||
      definition.id == CombatActionId::RpgSpearSweep) {
    return Engine::Core::CommanderSignatureForm::Sweep;
  }
  if (definition.id == CombatActionId::RtsCommanderThrust) {
    return commander != nullptr && commander->signature_max_targets > 1
               ? Engine::Core::CommanderSignatureForm::Sweep
               : Engine::Core::CommanderSignatureForm::Thrust;
  }
  if (definition.attack_direction == Engine::Core::AttackDirection::Thrust) {
    return Engine::Core::CommanderSignatureForm::Thrust;
  }
  if (definition.role == CommanderActionRole::Finisher ||
      definition.role == CommanderActionRole::Dive ||
      definition.role == CommanderActionRole::Launcher ||
      definition.attack_direction == Engine::Core::AttackDirection::Overhead ||
      definition.attack_direction == Engine::Core::AttackDirection::HeavyOverhead) {
    return Engine::Core::CommanderSignatureForm::Slam;
  }
  return Engine::Core::CommanderSignatureForm::Cut;
}

auto commander_strike_intensity(
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    bool signature_strike) -> float {
  using Game::Systems::CombatActions::CommanderActionRole;
  if (signature_strike ||
      definition.id == Game::Systems::CombatActions::CombatActionId::RtsCommanderCut ||
      definition.id ==
          Game::Systems::CombatActions::CombatActionId::RtsCommanderThrust) {
    return 1.0F;
  }
  switch (definition.role) {
  case CommanderActionRole::Finisher:
  case CommanderActionRole::Dive:
    return 1.0F;
  case CommanderActionRole::Launcher:
  case CommanderActionRole::GapCloser:
  case CommanderActionRole::Aerial:
    return 0.85F;
  case CommanderActionRole::Routine:
  case CommanderActionRole::Special:
    return 0.70F;
  }
  return 0.70F;
}

void record_signature_contact(
    Engine::Core::Entity& attacker,
    const Engine::Core::TransformComponent& attacker_transform,
    const Engine::Core::TransformComponent& target_transform,
    Engine::Core::CommanderSignatureForm form,
    float intensity,
    float reach) {
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
  entry.cue = Engine::Core::CommanderStrikeCue::Impact;
  entry.intensity = intensity;
  entry.reach = reach;
  push_commander_cue(attacker, entry);
}

void record_commander_swing(
    Engine::Core::Entity& attacker,
    const Engine::Core::TransformComponent& attacker_transform,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const Engine::Core::CommanderComponent* commander,
    float reach) {
  bool const signature_strike =
      commander != nullptr && commander->signature_strike_active;
  auto const form = commander_strike_form(definition, commander);
  float const yaw = attacker_transform.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const forward_x = std::sin(yaw);
  float const forward_z = std::cos(yaw);

  Engine::Core::CommanderSignaturePresentationComponent::Entry entry;
  entry.x = attacker_transform.position.x + forward_x * reach * 0.30F;
  entry.y = attacker_transform.position.y + 0.95F;
  entry.z = attacker_transform.position.z + forward_z * reach * 0.30F;
  entry.dir_x = forward_x;
  entry.dir_z = forward_z;
  entry.form = form;
  entry.cue = Engine::Core::CommanderStrikeCue::Swing;
  entry.lifetime =
      Engine::Core::CommanderSignaturePresentationComponent::k_swing_lifetime;
  entry.intensity = commander_strike_intensity(definition, signature_strike);
  entry.reach = reach;
  entry.span = commander_strike_span(definition, form);
  push_commander_cue(attacker, entry);
}

void record_swing_for_weapon_trace_start(
    Engine::Core::World& world,
    Engine::Core::Entity& entity,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    Game::Systems::CombatActions::CombatActionId action_id) {
  bool const advanced_rts_melee = is_advanced_rts_commander_melee(entity, definition);
  auto const* commander =
      world.try_get<Engine::Core::CommanderComponent>(entity.get_id());
  if (!(advanced_rts_melee ||
        commander_swings_under_player_control(commander, definition) ||
        action_id == Game::Systems::CombatActions::CombatActionId::RtsCommanderCut ||
        action_id ==
            Game::Systems::CombatActions::CombatActionId::RtsCommanderThrust)) {
    return;
  }
  auto const* transform =
      world.try_get<Engine::Core::TransformComponent>(entity.get_id());
  auto const* attack = entity.get_component<Engine::Core::AttackComponent>();
  if (commander == nullptr || transform == nullptr) {
    return;
  }
  float const reach = std::max(attack != nullptr ? attack->melee_range : 0.0F,
                               definition.hit_shape.reach) +
                      (commander->signature_strike_active
                           ? std::max(0.0F, commander->signature_bonus_reach)
                           : 0.0F);
  record_commander_swing(entity, *transform, definition, commander, reach);
}

void apply_commander_signature_effects(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::CommanderComponent& commander,
    Engine::Core::Entity& primary_target,
    const Engine::Core::TransformComponent& attacker_transform,
    float reach,
    int damage) {
  record_contact_for_action(
      attacker, commander, attacker_transform, primary_target, reach);
  if (commander.signature_stagger_seconds > 0.0F) {
    Game::Systems::Combat::add_or_extend_stagger(
        &primary_target,
        commander.signature_stagger_seconds,
        Engine::Core::StaggerTier::LightFlinch);
  }
  if (commander.signature_max_targets <= 1) {
    return;
  }
  sweep_secondary_targets(
      world, attacker, commander, primary_target, attacker_transform, reach, damage);
}

void apply_signature_shot_effects(Engine::Core::World& world,
                                  Engine::Core::Entity& shooter,
                                  Engine::Core::EntityID target_id) {
  auto* commander = shooter.get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || !commander->signature_strike_active) {
    return;
  }
  auto* shot_target = world.get_entity(target_id);
  if (shot_target != nullptr && commander->signature_stagger_seconds > 0.0F) {
    Game::Systems::Combat::add_or_extend_stagger(
        shot_target,
        commander->signature_stagger_seconds,
        Engine::Core::StaggerTier::LightFlinch);
  }
  auto const* shooter_transform =
      shooter.get_component<Engine::Core::TransformComponent>();
  auto const* shot_transform =
      shot_target != nullptr
          ? shot_target->get_component<Engine::Core::TransformComponent>()
          : nullptr;
  if (shooter_transform != nullptr && shot_transform != nullptr) {
    record_signature_contact(shooter,
                             *shooter_transform,
                             *shot_transform,
                             Engine::Core::CommanderSignatureForm::Shot);
  }
  commander->signature_strike_active = false;
}

} // namespace Game::Systems::Combat
