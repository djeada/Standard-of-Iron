#pragma once

#include "ai_types.h"
#include <vector>

namespace Engine::Core {
class World;
}

namespace Game::Systems::AI {

class AICommandApplier {
public:
  AICommandApplier() = default;
  ~AICommandApplier() = default;

  AICommandApplier(const AICommandApplier &) = delete;
  auto operator=(const AICommandApplier &) -> AICommandApplier & = delete;

  static void apply(Engine::Core::World &world, int aiOwnerId,
                    const std::vector<AICommand> &commands);
};

} 
