#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "game/core/entity.h"

namespace Engine::Core {
class World;
}

namespace App::Core::WallDeck {

struct Node {
  Engine::Core::EntityID id{0};
  float x{0.0F};
  float z{0.0F};
  std::int8_t inner_x{0};
  std::int8_t inner_z{0};
  bool stair{false};
  bool ladder{false};
};

struct Surface {
  float elevation{0.0F};
  Engine::Core::EntityID node_id{0};
  bool on_access{false};
};

inline constexpr float k_board_elevation = 0.45F;

[[nodiscard]] auto
surface_at(const std::vector<Node>& nodes, float x, float z) -> std::optional<Surface>;

struct Placement {
  float x{0.0F};
  float z{0.0F};
  Surface surface;
};

[[nodiscard]] auto nearest_deck_point(const std::vector<Node>& nodes,
                                      float x,
                                      float z) -> std::optional<Placement>;

[[nodiscard]] auto gather(Engine::Core::World& world,
                          int owner_id,
                          float x,
                          float z,
                          float reach) -> std::vector<Node>;

} // namespace App::Core::WallDeck
