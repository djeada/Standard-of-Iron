#pragma once

#include "../../game/core/component.h"
#include "animation/combat_manifest.h"

namespace Render::Creature {

[[nodiscard]] constexpr auto animation_attack_family(
    Engine::Core::CombatAttackFamily family) noexcept -> Animation::CombatAttackFamily {
  switch (family) {
  case Engine::Core::CombatAttackFamily::Sword:
    return Animation::CombatAttackFamily::Sword;
  case Engine::Core::CombatAttackFamily::Spear:
    return Animation::CombatAttackFamily::Spear;
  case Engine::Core::CombatAttackFamily::Bow:
    return Animation::CombatAttackFamily::Bow;
  case Engine::Core::CombatAttackFamily::None:
    break;
  }
  return Animation::CombatAttackFamily::None;
}

[[nodiscard]] constexpr auto engine_attack_family(
    Animation::CombatAttackFamily family) noexcept -> Engine::Core::CombatAttackFamily {
  switch (family) {
  case Animation::CombatAttackFamily::Sword:
    return Engine::Core::CombatAttackFamily::Sword;
  case Animation::CombatAttackFamily::Spear:
    return Engine::Core::CombatAttackFamily::Spear;
  case Animation::CombatAttackFamily::Bow:
    return Engine::Core::CombatAttackFamily::Bow;
  case Animation::CombatAttackFamily::None:
    break;
  }
  return Engine::Core::CombatAttackFamily::None;
}

[[nodiscard]] constexpr auto animation_combat_phase(
    Engine::Core::CombatAnimationState phase) noexcept -> Animation::CombatPhase {
  switch (phase) {
  case Engine::Core::CombatAnimationState::Advance:
    return Animation::CombatPhase::Advance;
  case Engine::Core::CombatAnimationState::WindUp:
    return Animation::CombatPhase::WindUp;
  case Engine::Core::CombatAnimationState::Strike:
    return Animation::CombatPhase::Strike;
  case Engine::Core::CombatAnimationState::Impact:
    return Animation::CombatPhase::Impact;
  case Engine::Core::CombatAnimationState::Recover:
    return Animation::CombatPhase::Recover;
  case Engine::Core::CombatAnimationState::Reposition:
    return Animation::CombatPhase::Reposition;
  case Engine::Core::CombatAnimationState::Idle:
    break;
  }
  return Animation::CombatPhase::Idle;
}

[[nodiscard]] constexpr auto engine_combat_phase(Animation::CombatPhase phase) noexcept
    -> Engine::Core::CombatAnimationState {
  switch (phase) {
  case Animation::CombatPhase::Advance:
    return Engine::Core::CombatAnimationState::Advance;
  case Animation::CombatPhase::WindUp:
    return Engine::Core::CombatAnimationState::WindUp;
  case Animation::CombatPhase::Strike:
    return Engine::Core::CombatAnimationState::Strike;
  case Animation::CombatPhase::Impact:
    return Engine::Core::CombatAnimationState::Impact;
  case Animation::CombatPhase::Recover:
    return Engine::Core::CombatAnimationState::Recover;
  case Animation::CombatPhase::Reposition:
    return Engine::Core::CombatAnimationState::Reposition;
  case Animation::CombatPhase::Idle:
    break;
  }
  return Engine::Core::CombatAnimationState::Idle;
}

} // namespace Render::Creature
