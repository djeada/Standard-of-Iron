#pragma once

#include "animation/combat_manifest.h"
#include "game/core/component_combat.h"

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

} // namespace Render::Creature
