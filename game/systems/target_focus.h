#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../core/entity.h"
#include "../map/visibility_service.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class OwnerRegistry;

enum class TargetFocusRole : std::uint8_t {
  Inspected,
  LockedTarget,
  IncomingAttacker,
};

struct TargetFocusMarker {
  Engine::Core::EntityID entity_id{0};
  TargetFocusRole role{TargetFocusRole::LockedTarget};
  float world_x{0.0F};
  float world_y{0.0F};
  float world_z{0.0F};
  float radius{0.5F};
  bool is_building{false};
  bool hostile{true};
  int weight{1};
};

struct TargetFocusRequest {
  Engine::Core::World* world{nullptr};
  int local_owner_id{0};
  const std::vector<Engine::Core::EntityID>* selection{nullptr};
  Engine::Core::EntityID inspected{0};
  std::size_t max_locked_targets{8};
  std::size_t max_incoming_attackers{12};
  const Game::Map::VisibilityService::Snapshot* visibility{nullptr};

  const OwnerRegistry* owners{nullptr};
};

inline constexpr std::size_t k_target_focus_max_locked = 8;
inline constexpr std::size_t k_target_focus_max_incoming = 12;

[[nodiscard]] auto collect_target_focus_markers(const TargetFocusRequest& request)
    -> std::vector<TargetFocusMarker>;

} // namespace Game::Systems
