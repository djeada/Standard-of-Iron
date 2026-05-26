#include "target_commitment_system.h"

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"

namespace Game::Systems {

namespace {

auto is_committed_phase(Engine::Core::CombatAnimationState state) -> bool {
  switch (state) {
  case Engine::Core::CombatAnimationState::WindUp:
  case Engine::Core::CombatAnimationState::Strike:
  case Engine::Core::CombatAnimationState::Impact:
    return true;
  default:
    return false;
  }
}

auto is_target_valid(Engine::Core::World* world,
                     Engine::Core::EntityID target_id) -> bool {
  if (target_id == 0) {
    return false;
  }
  auto* target = world->get_entity(target_id);
  if (target == nullptr) {
    return false;
  }
  if (target->has_component<Engine::Core::PendingRemovalComponent>()) {
    return false;
  }
  auto* unit = target->get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && unit->health > 0;
}

} // namespace

void TargetCommitmentSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr || delta_time <= 0.0F) {
    return;
  }

  m_diagnostics = {};

  auto entities = world->get_entities_with<Engine::Core::AttackComponent>();

  for (auto* entity : entities) {
    auto* atk = entity->get_component<Engine::Core::AttackComponent>();
    if (atk == nullptr) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }

    // Ensure commitment component exists for attackers with melee lock.
    auto* commitment = entity->get_component<Engine::Core::TargetCommitmentComponent>();

    auto* combat_state = entity->get_component<Engine::Core::CombatStateComponent>();
    auto* attack_target = entity->get_component<Engine::Core::AttackTargetComponent>();

    if (commitment == nullptr) {
      if (atk->in_melee_lock && attack_target != nullptr) {
        commitment = entity->add_component<Engine::Core::TargetCommitmentComponent>();
        if (commitment != nullptr) {
          commitment->committed_target_id = attack_target->target_id;
        }
      }
      continue;
    }

    // Tick cooldown.
    if (commitment->cooldown_remaining > 0.0F) {
      commitment->cooldown_remaining =
          std::max(0.0F, commitment->cooldown_remaining - delta_time);
    }

    // Update committed phase status.
    bool in_committed =
        combat_state != nullptr && is_committed_phase(combat_state->animation_state);
    commitment->in_committed_phase = in_committed;

    // Check if current committed target is still valid.
    if (!is_target_valid(world, commitment->committed_target_id)) {
      // Force release.
      commitment->committed_target_id = 0;
      commitment->cooldown_remaining = 0.0F;
      commitment->in_committed_phase = false;
      ++m_diagnostics.forced_releases;
      continue;
    }

    // If attack target is trying to change, enforce commitment.
    if (attack_target != nullptr &&
        attack_target->target_id != commitment->committed_target_id) {

      if (in_committed || commitment->cooldown_remaining > 0.0F) {
        // Block the switch.
        attack_target->target_id = commitment->committed_target_id;
        ++m_diagnostics.switches_blocked;
      } else {
        // Allow the switch and update commitment.
        commitment->committed_target_id = attack_target->target_id;
        commitment->cooldown_remaining =
            Engine::Core::TargetCommitmentComponent::k_switch_cooldown;
        ++m_diagnostics.switches_allowed;
      }
    }
  }
}

} // namespace Game::Systems
