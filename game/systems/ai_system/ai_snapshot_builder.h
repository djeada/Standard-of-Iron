#pragma once

#include "ai_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::AI {

class AISnapshotBuilder {
public:
  AISnapshotBuilder() = default;
  ~AISnapshotBuilder() = default;

  AISnapshotBuilder(const AISnapshotBuilder &) = delete;
  auto operator=(const AISnapshotBuilder &) -> AISnapshotBuilder & = delete;

  [[nodiscard]] static auto build(const Engine::Core::World &world,
                                  int ai_owner_id) -> AISnapshot;
};

} // namespace Game::Systems::AI
