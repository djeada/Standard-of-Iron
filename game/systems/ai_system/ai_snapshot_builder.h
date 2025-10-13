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
  AISnapshotBuilder &operator=(const AISnapshotBuilder &) = delete;

  AISnapshot build(const Engine::Core::World &world, int aiOwnerId) const;
};

} // namespace Game::Systems::AI
