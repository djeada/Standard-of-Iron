#pragma once

#include <vector>

#include "../ai_types.h"
#include "builder_pool.h"

namespace Game::Systems::AI {

class GatherCrew {
public:
  void manage(const AISnapshot& snapshot,
              const AIContext& context,
              bool reclaim_one,
              BuilderPool& pool,
              std::vector<AICommand>& out_commands);

private:
  const char* m_priority = nullptr;
  float m_priority_time = -1000.0F;
};

} // namespace Game::Systems::AI
