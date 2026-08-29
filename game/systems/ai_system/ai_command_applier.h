#pragma once

#include <vector>

#include "ai_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::AI {

namespace AICommandApplier {

struct ApplyReport {

  int refused_production = 0;

  int refused_construction = 0;
};

auto apply(Engine::Core::World& world,
           int ai_owner_id,
           const std::vector<AICommand>& commands) -> ApplyReport;

} // namespace AICommandApplier

} // namespace Game::Systems::AI
