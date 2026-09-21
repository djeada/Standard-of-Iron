#pragma once

#include "game/mission/difficulty_profile.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {
class OwnerRegistry;
}

namespace Game::Mission {

struct DifficultyForceResult {

  int owners_scaled = 0;

  int units_added = 0;

  int units_withdrawn = 0;

  int units_capped = 0;
};

[[nodiscard]] auto difficulty_applies_to(const MatchDifficulty& difficulty,
                                         const Game::Systems::OwnerRegistry& owners,
                                         int owner_id,
                                         int local_owner_id) -> bool;

[[nodiscard]] auto
apply_starting_force_difficulty(Engine::Core::World& world,
                                const MatchDifficulty& difficulty,
                                int local_owner_id) -> DifficultyForceResult;

} // namespace Game::Mission
