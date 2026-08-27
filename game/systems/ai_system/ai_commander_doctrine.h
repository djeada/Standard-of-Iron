#pragma once

#include <optional>

#include "../../units/troop_type.h"
#include "ai_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::AI {

[[nodiscard]] auto doctrine_profile_for_troop(Game::Units::TroopType troop_type)
    -> std::optional<AIPlayerProfile>;

[[nodiscard]] auto doctrine_profile_for_owner(Engine::Core::World& world, int owner_id)
    -> std::optional<AIPlayerProfile>;

} // namespace Game::Systems::AI
