#pragma once

#include <QVector3D>

#include <cstdint>
#include <deque>
#include <vector>

#include "../core/component.h"
#include "../core/system.h"
#include "../core/world.h"
#include "command_service.h"

namespace Engine::Core {
class Entity;
}

namespace Game::Systems {

class MovementSystem : public Engine::Core::System {
public:
  using MoveOptions = CommandService::MoveOptions;
  using MoveIntent = CommandService::MoveIntent;

  void update(Engine::Core::World* world, float delta_time) override;

private:
  friend class CommandService;

  static auto
  assign_local_recovery_move(const QVector3D& current_position,
                             const QVector3D& goal,
                             Engine::Core::MovementComponent* movement) -> bool;
  static auto retarget_unit(Engine::Core::World& world,
                            Engine::Core::EntityID entity_id,
                            const QVector3D& goal) -> bool;
  static void assign_direct_target(Engine::Core::MovementComponent& movement,
                                   const QVector3D& target);
  static auto assign_path_to_movement(Pathfinding& pathfinder,
                                      const std::vector<Point>& path_points,
                                      const Engine::Core::TransformComponent& transform,
                                      Engine::Core::MovementComponent& movement,
                                      bool include_first_waypoint = false) -> bool;
  auto apply_duel_footwork(Engine::Core::Entity* entity,
                           Engine::Core::World* world,
                           Engine::Core::TransformComponent& transform,
                           const Engine::Core::AttackComponent& attack,
                           float delta_time) const -> bool;

  static void
  assign_navigation_target(Pathfinding* pathfinder,
                           const Engine::Core::TransformComponent& transform,
                           Engine::Core::MovementComponent& movement,
                           const QVector3D& requested_target);

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

  float m_duel_clock{0.0F};
  void process_pending_path_requests(Engine::Core::World& world);
  struct PendingPathRequest {
    Engine::Core::EntityID entity_id{0};
    QVector3D target;
    std::uint64_t navigation_revision{0};
    bool precise_arrival{false};
  };

  auto enqueue_pending_path_request(Engine::Core::EntityID entity_id,
                                    const QVector3D& target,
                                    bool precise_arrival,
                                    std::uint64_t navigation_revision) -> bool;
  void cancel_pending_path_request(Engine::Core::EntityID entity_id);
  std::deque<PendingPathRequest> m_pending_path_requests;

  static constexpr std::size_t k_path_requests_per_tick = 8U;
  static constexpr std::size_t k_max_pending_path_requests = 2048U;
};

} // namespace Game::Systems
