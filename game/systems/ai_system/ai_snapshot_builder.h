#pragma once

#include "ai_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {
class NationRegistry;
}

namespace Game::Systems::AI {

namespace AISnapshotBuilder {

[[nodiscard]] auto build(const Engine::Core::World& world,
                         int ai_owner_id) -> AISnapshot;

void attach_nation(AISnapshot& snapshot,
                   int ai_owner_id,
                   const Game::Systems::NationRegistry& nations);

} // namespace AISnapshotBuilder

} // namespace Game::Systems::AI
