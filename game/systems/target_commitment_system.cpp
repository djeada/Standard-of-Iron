#include "target_commitment_system.h"

#include <algorithm>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/system_context.h"
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

auto is_target_valid(Engine::Core::SystemContext& context,
                     Engine::Core::EntityID target_id) -> bool {
  if (target_id == 0 || !context.is_alive(target_id) ||
      context.has<Engine::Core::PendingRemovalComponent>(target_id)) {
    return false;
  }
  const auto* unit = context.try_get<Engine::Core::UnitComponent>(target_id);
  return unit != nullptr && unit->health > 0;
}

} // namespace

void TargetCommitmentSystem::run(Engine::Core::SystemContext& context) {
  const float delta_time = context.delta_time();
  if (delta_time < 0.0F) {
    return;
  }

  m_diagnostics = {};

  for (auto [entity_id, atk] : context.view<Engine::Core::AttackComponent>()) {
    const auto* unit = context.try_get<Engine::Core::UnitComponent>(entity_id);
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }

    auto* commitment =
        context.try_get<Engine::Core::TargetCommitmentComponent>(entity_id);
    const auto* combat_state =
        context.try_get<Engine::Core::CombatStateComponent>(entity_id);
    auto* attack_target =
        context.try_get<Engine::Core::AttackTargetComponent>(entity_id);

    if (commitment == nullptr) {
      if (atk.in_melee_lock && attack_target != nullptr) {
        commitment =
            context.emplace<Engine::Core::TargetCommitmentComponent>(entity_id);
        if (commitment != nullptr) {
          commitment->committed_target_id = attack_target->target_id;
          commitment->cooldown_remaining =
              Engine::Core::TargetCommitmentComponent::k_switch_cooldown;
        }
      }
      continue;
    }

    if (commitment->cooldown_remaining > 0.0F) {
      commitment->cooldown_remaining =
          std::max(0.0F, commitment->cooldown_remaining - delta_time);
    }

    bool const in_committed =
        combat_state != nullptr && is_committed_phase(combat_state->animation_state);
    commitment->in_committed_phase = in_committed;

    if (!is_target_valid(context, commitment->committed_target_id)) {

      commitment->committed_target_id = 0;
      commitment->cooldown_remaining = 0.0F;
      commitment->in_committed_phase = false;
      ++m_diagnostics.forced_releases;
      continue;
    }

    if (attack_target != nullptr &&
        attack_target->target_id != commitment->committed_target_id) {

      if (in_committed || commitment->cooldown_remaining > 0.0F) {

        attack_target->target_id = commitment->committed_target_id;
        attack_target->should_chase = true;
        if (atk.in_melee_lock &&
            atk.melee_lock_target_id != commitment->committed_target_id) {

          atk.in_melee_lock = false;
          atk.melee_lock_target_id = 0;
        }
        ++m_diagnostics.switches_blocked;
      } else {

        commitment->committed_target_id = attack_target->target_id;
        commitment->cooldown_remaining =
            Engine::Core::TargetCommitmentComponent::k_switch_cooldown;
        ++m_diagnostics.switches_allowed;
      }
    }
  }
}

auto TargetCommitmentSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent, CombatStateComponent, PendingRemovalComponent>{},
      Writes<AttackComponent, AttackTargetComponent, TargetCommitmentComponent>{});
}

} // namespace Game::Systems
