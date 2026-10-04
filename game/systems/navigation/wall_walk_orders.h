#pragma once

#include <optional>

namespace Engine::Core {
class World;
}

namespace Game::Systems::WallWalk {

struct WallWalkOrder {
  float x{0.0F};
  float z{0.0F};
  float along_x{1.0F};
  float along_z{0.0F};
};

[[nodiscard]] auto wall_walk_order_at(Engine::Core::World& world,
                                      int owner_id,
                                      float x,
                                      float z) -> std::optional<WallWalkOrder>;

} // namespace Game::Systems::WallWalk
