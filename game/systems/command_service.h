#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "order_service.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
class MovementComponent;
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

  static auto plan_ground_move(Engine::Core::World& world,
                               const std::vector<Engine::Core::EntityID>& units,
                               const QVector3D& target,
                               bool preserve_current_shape = false) -> GroundMovePlan;
  static void issue_ground_move(Engine::Core::World& world,
                                const std::vector<Engine::Core::EntityID>& units,
                                const GroundMovePlan& plan);
  // A troop is not one disc. `envelope` is the circle that contains its whole
  // formation -- the right radius for predicting an encounter and for planning
  // a route through a gap. `core` is the body a single soldier actually
  // occupies: two troops whose outer files touch are not interpenetrating, and
  // pushing them apart at envelope distance spreads a battle line out by
  // metres.
  struct UnitRadii {
    float core{0.5F};
    float envelope{0.5F};
  };

  static auto get_unit_radii(Engine::Core::World& world,
                             Engine::Core::EntityID entity_id) -> UnitRadii;

  static auto get_unit_radius(Engine::Core::World& world,
                              Engine::Core::EntityID entity_id) -> float;

  static auto structure_work_position(const QVector3D& worker_position,
                                      const QVector3D& structure_position,
                                      const std::string& structure_key,
                                      float unit_radius) -> QVector3D;
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
  static auto resolve_move_targets(Engine::Core::World& world,
                                   const std::vector<Engine::Core::EntityID>& units,
                                   const QVector3D& center) -> std::vector<QVector3D>;
};

} // namespace Game::Systems
