#pragma once

#include <vector>

#include "ai_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::AI {

namespace AICommandApplier {

struct ApplyReport {
  // Commands the world refused outright. A plan made of these is a computer
  // that looks busy and does nothing, which is how a whole match could pass
  // with the AI issuing thousands of decisions and never fielding an army.
  int refused_production = 0;
};

auto apply(Engine::Core::World& world,
           int ai_owner_id,
           const std::vector<AICommand>& commands) -> ApplyReport;

} // namespace AICommandApplier

} // namespace Game::Systems::AI
