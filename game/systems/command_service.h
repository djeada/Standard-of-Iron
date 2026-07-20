#pragma once

#include <QVector3D>

#include <memory>
#include <optional>
#include <vector>

#include "order_service.h"

namespace Engine::Core {
class World;
using EntityID = unsigned int;
struct MovementComponent;
} // namespace Engine::Core

namespace Game::Systems {

class Pathfinding;
struct Point;

class CommandService {
public:
  struct GroundMovePlan {
    QVector3D resolved_target;
    std::vector<QVector3D> positions;
    std::vector<float> facing_angles;
    bool preserve_formation_mode = false;
  };

  struct MoveOptions {
    MoveOrderKind kind = MoveOrderKind::PlayerMove;
    bool preserve_formation_mode = false;
  };

  struct MoveIntent {
    Engine::Core::EntityID unit_id{};
    QVector3D target;
  };

  static constexpr int DIRECT_PATH_THRESHOLD = 8;

  static constexpr float WAYPOINT_SKIP_THRESHOLD_SQ = 0.16F;
  static constexpr float k_unit_radius_threshold = 0.5F;

  static void initialize(int world_width, int world_height);

  static auto get_pathfinder() -> Pathfinding*;
  static auto world_to_grid(float world_x, float world_z) -> Point;
  static auto grid_to_world(const Point& grid_pos) -> QVector3D;
  static auto is_grid_walkable(const Point& grid_pos) -> bool;
  static auto is_world_position_walkable(const QVector3D& world_position) -> bool;
  static auto find_nearest_walkable_grid(const Point& origin,
                                         int max_search_radius) -> std::optional<Point>;
  static auto snap_to_walkable_ground(const QVector3D& world_position) -> QVector3D;
  static auto snap_to_walkable_ground(const QVector3D& world_position,
                                      int max_search_radius) -> QVector3D;
  static auto plan_ground_move(Engine::Core::World& world,
                               const std::vector<Engine::Core::EntityID>& units,
                               const QVector3D& target,
                               bool preserve_current_shape = false) -> GroundMovePlan;
  static void issue_ground_move(Engine::Core::World& world,
                                const std::vector<Engine::Core::EntityID>& units,
                                const GroundMovePlan& plan);
  static auto get_unit_radius(Engine::Core::World& world,
                              Engine::Core::EntityID entity_id) -> float;
  static void move_unit(Engine::Core::World& world,
                        Engine::Core::EntityID unit_id,
                        const QVector3D& target);

  static void move_unit(Engine::Core::World& world,
                        Engine::Core::EntityID unit_id,
                        const QVector3D& target,
                        const MoveOptions& options);

  static void move_units(Engine::Core::World& world,
                         const std::vector<Engine::Core::EntityID>& units,
                         const std::vector<QVector3D>& targets);

  static void move_units(Engine::Core::World& world,
                         const std::vector<Engine::Core::EntityID>& units,
                         const std::vector<QVector3D>& targets,
                         const MoveOptions& options);

  static void move_units(Engine::Core::World& world,
                         const std::vector<MoveIntent>& intents);

  static void move_units(Engine::Core::World& world,
                         const std::vector<MoveIntent>& intents,
                         const MoveOptions& options);

  static void attack_target(Engine::Core::World& world,
                            const std::vector<Engine::Core::EntityID>& units,
                            Engine::Core::EntityID target_id,
                            bool should_chase = true);

private:
  static std::unique_ptr<Pathfinding> s_pathfinder;
  static auto resolve_move_targets(Engine::Core::World& world,
                                   const std::vector<Engine::Core::EntityID>& units,
                                   const QVector3D& center) -> std::vector<QVector3D>;
};

} // namespace Game::Systems
