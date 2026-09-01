#pragma once

#include <cstdint>

#include "spawn_type.h"

namespace Game::Units {

enum class CombatRole : std::uint8_t {

  Noncombatant,

  Support,

  Fighter,

  Emplacement,

  Wildlife,
};

[[nodiscard]] constexpr auto combat_role(SpawnType type) noexcept -> CombatRole {
  switch (type) {
  case SpawnType::Archer:
  case SpawnType::Knight:
  case SpawnType::Spearman:
  case SpawnType::SkeletonSwordsman:
  case SpawnType::SkeletonArcher:
  case SpawnType::GravePriest:
  case SpawnType::MountedKnight:
  case SpawnType::HorseArcher:
  case SpawnType::HorseSpearman:
  case SpawnType::Catapult:
  case SpawnType::Ballista:
  case SpawnType::Elephant:
  case SpawnType::RomanLegionOrganizer:
  case SpawnType::RomanVeteranConsul:
  case SpawnType::RomanFieldCommander:
  case SpawnType::CarthageSpearCommander:
  case SpawnType::CarthageBowCommander:
  case SpawnType::CarthageSwordCommander:
    return CombatRole::Fighter;

  case SpawnType::Healer:
    return CombatRole::Support;

  case SpawnType::Civilian:
  case SpawnType::Builder:
    return CombatRole::Noncombatant;

  case SpawnType::DefenseTower:
  case SpawnType::Barracks:
  case SpawnType::Home:
  case SpawnType::WallSegment:
  case SpawnType::WallGate:
  case SpawnType::Marketplace:
  case SpawnType::Temple:
  case SpawnType::Farm:
    return CombatRole::Emplacement;

  case SpawnType::Sheep:
  case SpawnType::Wolf:
    return CombatRole::Wildlife;
  }

  return CombatRole::Noncombatant;
}

[[nodiscard]] constexpr auto acquires_targets(CombatRole role) noexcept -> bool {
  return role == CombatRole::Fighter || role == CombatRole::Emplacement;
}

[[nodiscard]] constexpr auto answers_threat_alerts(CombatRole role) noexcept -> bool {
  return role == CombatRole::Fighter;
}

[[nodiscard]] constexpr auto pursues_targets(CombatRole role) noexcept -> bool {
  return role == CombatRole::Fighter;
}

[[nodiscard]] constexpr auto acquires_targets(SpawnType type) noexcept -> bool {
  return acquires_targets(combat_role(type));
}

[[nodiscard]] constexpr auto answers_threat_alerts(SpawnType type) noexcept -> bool {
  return answers_threat_alerts(combat_role(type));
}

[[nodiscard]] constexpr auto pursues_targets(SpawnType type) noexcept -> bool {
  return pursues_targets(combat_role(type));
}

} // namespace Game::Units
