#pragma once

#include <QVector3D>

#include <vector>

#include "command_service.h"
#include "core/world.h"
#include "movement_orders_prepared.h"
#include "movement_system.h"
#include "movement_system_path_requests.h"
#include "systems/navigation/pathfinding.h"

namespace Game::Systems {

class MovementSystem::GroupIssue {
public:
  static void issue(Engine::Core::World& world,
                    const std::vector<Engine::Core::EntityID>& units,
                    const std::vector<QVector3D>& targets,
                    const CommandService::MoveOptions& options,
                    PathRequestQueue* path_requests);

private:
  struct GroupRoute;
  class MemberRouter;

  static auto prepare_all(Engine::Core::World& world,
                          const std::vector<Engine::Core::EntityID>& units,
                          const CommandService::MoveOptions& options,
                          PathRequestQueue* path_requests) -> std::vector<PreparedMove>;
  static void declare_group_pace(Engine::Core::World& world,
                                 std::vector<PreparedMove>& prepared);
  static void route_individually(Pathfinding* pathfinder,
                                 std::vector<PreparedMove>& prepared,
                                 const std::vector<QVector3D>& targets);
  static auto plan_group_route(Pathfinding& pathfinder,
                               const std::vector<PreparedMove>& prepared,
                               const std::vector<QVector3D>& targets) -> GroupRoute;
};

} // namespace Game::Systems
