#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../formation/army_formation_types.h"
#include "formation_combat_geometry.h"
#include "order_service.h"

namespace Game::Map {
class TerrainService;
}

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
  using SlotPlacement = Game::Formation::SlotStatus;

  struct GroupSlot {
    Engine::Core::EntityID member{0};
    QVector3D position;
    int stable_slot_id{Game::Formation::k_invalid_slot};
    float facing_angle{0.0F};
    SlotPlacement placement{SlotPlacement::Valid};
  };

  struct GroundMovePlan {
    QVector3D resolved_target;
    std::vector<GroupSlot> member_slots;
    bool preserve_formation_mode = false;

    [[nodiscard]] auto
    matches_members(const std::vector<Engine::Core::EntityID>& units) const -> bool;
    [[nodiscard]] auto
    fully_placeable_for(const std::vector<Engine::Core::EntityID>& units) const -> bool;

    [[nodiscard]] auto anyone_can_move() const -> bool;
    [[nodiscard]] auto target_positions() const -> std::vector<QVector3D>;
    [[nodiscard]] auto facing_angles() const -> std::vector<float>;
  };

  struct MoveOptions {
    MoveOrderKind kind = MoveOrderKind::PlayerMove;
    bool preserve_formation_mode = false;
  };

  struct MoveIntent {
    Engine::Core::EntityID unit_id{};
    QVector3D target;
    std::optional<float> facing_angle;
  };

  static constexpr int DIRECT_PATH_THRESHOLD = 8;

  static constexpr float WAYPOINT_SKIP_THRESHOLD_SQ = 0.16F;
  static constexpr float k_unit_radius_threshold =
      FormationCombat::k_body_core_radius_floor;

  static auto plan_ground_move(Engine::Core::World& world,
                               const std::vector<Engine::Core::EntityID>& units,
                               const QVector3D& target,
                               bool preserve_current_shape = false) -> GroundMovePlan;
  static void issue_ground_move(Engine::Core::World& world,
                                const std::vector<Engine::Core::EntityID>& units,
                                const GroundMovePlan& plan);

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

  static auto world_prop_work_position(Game::Map::TerrainService& terrain,
                                       const QVector3D& worker_position,
                                       std::uint64_t world_prop_id,
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

  static auto
  resolve_group_slots(Engine::Core::World& world,
                      const std::vector<Engine::Core::EntityID>& units,
                      const QVector3D& center,
                      bool preserve_current_shape = false) -> std::vector<GroupSlot>;
};

} // namespace Game::Systems
