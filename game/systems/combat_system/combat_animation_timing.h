#pragma once

#include <cstdint>

#include "../../core/component.h"
#include "../../core/entity.h"
#include "../../units/spawn_type.h"

namespace Game::Systems::Combat {

[[nodiscard]] auto melee_contact_time_for_attack(const Engine::Core::Entity* attacker,
                                                 float cooldown) noexcept -> float;

[[nodiscard]] auto melee_contact_time_for_unit(Game::Units::SpawnType spawn_type,
                                               Engine::Core::CombatAttackFamily family,
                                               std::uint8_t attack_variant,
                                               float cooldown) noexcept -> float;

} // namespace Game::Systems::Combat
