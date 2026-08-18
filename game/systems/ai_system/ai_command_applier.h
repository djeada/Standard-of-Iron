#pragma once

#include <vector>

#include "ai_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::AI {

namespace AICommandApplier {

void apply(Engine::Core::World& world,
           int ai_owner_id,
           const std::vector<AICommand>& commands);

} // namespace AICommandApplier

} // namespace Game::Systems::AI
