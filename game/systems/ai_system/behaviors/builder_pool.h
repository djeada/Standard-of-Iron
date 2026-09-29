#pragma once

#include <vector>

#include "../ai_types.h"
#include "builder_stall_watch.h"

namespace Game::Systems::AI {

class BuilderPool {
public:
  static auto gather(const AISnapshot& snapshot,
                     const WorkerStallWatch& stalls) -> BuilderPool;

  auto
  take_nearest(const AISnapshot& snapshot, float x, float z) -> Engine::Core::EntityID;

  auto take_strongest(const AISnapshot& snapshot,
                      float x,
                      float z) -> Engine::Core::EntityID;

  auto take_last() -> Engine::Core::EntityID;

  [[nodiscard]] auto size() const noexcept -> int {
    return static_cast<int>(m_available.size());
  }
  [[nodiscard]] auto empty() const noexcept -> bool { return m_available.empty(); }

private:
  std::vector<Engine::Core::EntityID> m_available;
};

} // namespace Game::Systems::AI
