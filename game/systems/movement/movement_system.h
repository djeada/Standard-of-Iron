#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "command_service.h"
#include "core/component_combat.h"
#include "core/system.h"
#include "core/world.h"
#include "movement_system_duel_footwork.h"
#include "movement_system_path_requests.h"

namespace Engine::Core {
class Entity;
}

namespace Game::Systems {

class MovementSystem : public Engine::Core::System {
public:
  using MoveOptions = CommandService::MoveOptions;
  using MoveIntent = CommandService::MoveIntent;

  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  class Assignment;
  class GroupIssue;
  class Motor;
  class Gates;

private:
  friend class CommandService;
  friend class RouteFollowSystem;

  static void apply_deferred_route(Engine::Core::TransformComponent& transform,
                                   Engine::Core::MovementComponent& movement,
                                   const QVector3D& target,
                                   bool precise_arrival);

  static auto path_requests_of(Engine::Core::World& world) -> PathRequestQueue*;

  static void follow_formation_slot(Engine::Core::World& world,
                                    const MoveIntent& intent,
                                    const MoveOptions& options);

  static void issue_move(Engine::Core::World& world,
                         Engine::Core::EntityID unit_id,
                         const QVector3D& target);
  static void issue_move(Engine::Core::World& world,
                         Engine::Core::EntityID unit_id,
                         const QVector3D& target,
                         const MoveOptions& options);
  static void issue_move_units(Engine::Core::World& world,
                               const std::vector<Engine::Core::EntityID>& units,
                               const std::vector<QVector3D>& targets);
  static void issue_move_units(Engine::Core::World& world,
                               const std::vector<Engine::Core::EntityID>& units,
                               const std::vector<QVector3D>& targets,
                               const MoveOptions& options);
  static void issue_move_units(Engine::Core::World& world,
                               const std::vector<MoveIntent>& intents);
  static void issue_move_units(Engine::Core::World& world,
                               const std::vector<MoveIntent>& intents,
                               const MoveOptions& options);

  void
  move_unit(Engine::Core::Entity* entity, Engine::Core::World* world, float delta_time);

  void
  repath_after_obstruction_release(Engine::Core::World& world,
                                   const std::vector<Engine::Core::Entity*>& movers);

  std::uint64_t m_obstruction_revision{0};
  DuelFootwork m_duel_footwork;
  PathRequestQueue m_path_requests;
};

} // namespace Game::Systems
