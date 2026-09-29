#pragma once

#include <cstdint>

#include "../../core/death_sequence.h"
#include "../../core/entity.h"

namespace Game::Systems::Combat {

auto resolve_death_variant(Engine::Core::Entity* target,
                           Engine::Core::Entity* attacker,
                           Engine::Core::DeathSequenceProfile profile,
                           std::uint16_t slot = 0U) -> std::uint8_t;

}
