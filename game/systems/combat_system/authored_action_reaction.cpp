#include "authored_action_reaction.h"

#include <algorithm>
#include <cmath>

#include "../../core/component_gameplay.h"
#include "../../core/world.h"
#include "combat_hit_resolver.h"
#include "combat_utils.h"
#include "damage_application.h"

namespace Game::Systems::Combat {

namespace {

[[nodiscard]] auto poise_capacity_for(const Engine::Core::Entity& target) -> float {
  if (target.has_component<Engine::Core::CommanderComponent>()) {
    return 300.0F;
  }
  auto const* unit = target.get_component<Engine::Core::UnitComponent>();
  return unit != nullptr
             ? std::clamp(static_cast<float>(unit->max_health) * 0.35F, 20.0F, 100.0F)
             : 20.0F;
}

void apply_hit_stop(
    Engine::Core::Entity& target,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) {
  if (auto* target_combat =
          target.get_component<Engine::Core::CombatStateComponent>()) {
    target_combat->is_hit_paused = true;
    target_combat->hit_pause_remaining = std::max(
        target_combat->hit_pause_remaining, authored_hit_stop_seconds(definition));
  }
}

[[nodiscard]] auto drain_poise(
    Engine::Core::Entity& target,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> bool {
  bool poise_broken = definition.damage.posture_damage <= 0.0F;
  if (definition.damage.posture_damage <= 0.0F) {
    return poise_broken;
  }
  auto* poise = Engine::Core::get_or_add_component<Engine::Core::PoiseComponent>(
      &target, poise_capacity_for(target));
  if (poise == nullptr) {
    return poise_broken;
  }
  poise->current = std::max(0.0F, poise->current - definition.damage.posture_damage);
  poise->regeneration_delay =
      Engine::Core::PoiseComponent::k_regeneration_delay_seconds;
  poise_broken = poise->current <= 0.0F;
  if (poise_broken) {
    poise->current = poise->maximum * 0.25F;
  }
  return poise_broken;
}

void stagger_target(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    Engine::Core::Entity& target,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const QVector3D& contact_point,
    float contact_speed) {
  add_or_extend_stagger(
      &target, definition.reaction.stagger_seconds, definition.reaction.stagger_tier);
  apply_hit_feedback(
      &target,
      attacker.get_id(),
      &world,
      Engine::Core::HitReactionKind::Stagger,
      {.contact_point = contact_point, .weapon_speed = std::max(0.0F, contact_speed)});
  if (auto* feedback = target.get_component<Engine::Core::HitFeedbackComponent>()) {
    feedback->stagger_tier = definition.reaction.stagger_tier;
  }
}

void launch_target(
    Engine::Core::Entity& attacker,
    Engine::Core::Entity& target,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) {
  auto* target_transform = target.get_component<Engine::Core::TransformComponent>();
  auto const* attacker_transform =
      attacker.get_component<Engine::Core::TransformComponent>();
  if (target_transform == nullptr || attacker_transform == nullptr) {
    return;
  }
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

} // namespace

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
  if (!result.applied || is_building(&target)) {
    return;
  }
  apply_hit_stop(target, definition);
  bool const poise_broken = drain_poise(target, definition);
  if (poise_broken && definition.reaction.stagger_seconds > 0.0F) {
    stagger_target(world, attacker, target, definition, contact_point, contact_speed);
  }
  if (poise_broken && definition.reaction.launch_impulse > 0.0F) {
    launch_target(attacker, target, definition);
  }
  if (definition.reaction.launch_impulse > 0.0F &&
      result.queued_soldier_casualties > 0) {
    launch_new_casualties(target,
                          attacker,
                          result.queued_soldier_casualties,
                          definition.reaction.launch_impulse);
  }
}

} // namespace Game::Systems::Combat
