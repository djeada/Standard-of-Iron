#pragma once

#include <QVector3D>

#include <cstdint>
#include <string>
#include <vector>

#include "../core/entity.h"
#include "nation_id.h"
#include "wall_network_service.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

enum class WallSegmentFault : std::uint8_t {
  None,
  Occupied,
  Invalid,
  NotEnoughWood,
};

struct PlannedWallSegment {
  int grid_x = 0;
  int grid_z = 0;
  QVector3D world_position;
  std::uint8_t connection_mask = 0;
  float rotation_y = 0.0F;
  bool valid = false;
  WallSegmentFault fault = WallSegmentFault::None;
  GroundVerdict verdict = GroundVerdict::Clear;
  std::string failure_reason;
};

struct WallPlanRequest {
  int owner_id = 0;
  bool gate = false;
  WallGridPosition anchor;
  WallGridPosition target;
  float rotation_y = 0.0F;
};

struct WallPlan {
  std::vector<PlannedWallSegment> segments;
  int valid_count = 0;
  int wood_per_segment = 0;

  [[nodiscard]] auto wood_cost() const -> int { return valid_count * wood_per_segment; }
};

class WallPlanService {
public:
  static auto plan(Engine::Core::World& world,
                   const WallPlanRequest& request) -> WallPlan;

  static auto commit(Engine::Core::World& world,
                     const WallPlanRequest& request,
                     const WallPlan& plan,
                     const std::vector<Engine::Core::EntityID>& builders)
      -> std::vector<Engine::Core::EntityID>;
};

} // namespace Game::Systems
