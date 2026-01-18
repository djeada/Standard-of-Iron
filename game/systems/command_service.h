#pragma once

#include <QVector3D>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

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
  struct MoveOptions {
    bool allow_direct_fallback = true;
    bool clear_attack_intent = true;
    bool group_move = false;
  };

  static constexpr int DIRECT_PATH_THRESHOLD = 8;

  static constexpr float WAYPOINT_SKIP_THRESHOLD_SQ = 0.16F;

  static void initialize(int world_width, int world_height);

  static auto get_pathfinder() -> Pathfinding *;
  static auto world_to_grid(float world_x, float world_z) -> Point;
  static auto grid_to_world(const Point &grid_pos) -> QVector3D;
  static auto get_unit_radius(Engine::Core::World &world,
                              Engine::Core::EntityID entity_id) -> float;

  static void move_units(Engine::Core::World &world,
                         const std::vector<Engine::Core::EntityID> &units,
                         const std::vector<QVector3D> &targets);

  static void move_units(Engine::Core::World &world,
                         const std::vector<Engine::Core::EntityID> &units,
                         const std::vector<QVector3D> &targets,
                         const MoveOptions &options);

  static void process_path_results(Engine::Core::World &world);

  static void attack_target(Engine::Core::World &world,
                            const std::vector<Engine::Core::EntityID> &units,
                            Engine::Core::EntityID target_id,
                            bool should_chase = true);

private:
  struct PendingPathRequest {
    Engine::Core::EntityID entity_id{};
    QVector3D target;
    MoveOptions options;
    std::vector<Engine::Core::EntityID> group_members;
    std::vector<QVector3D> group_targets;
    float unit_radius{0.0F};
  };

  static std::unique_ptr<Pathfinding> s_pathfinder;
  static std::unordered_map<std::uint64_t, PendingPathRequest>
      s_pending_requests;
  static std::unordered_map<Engine::Core::EntityID, std::uint64_t>
      s_entity_to_request;
  static std::mutex s_pending_mutex;
  static std::atomic<std::uint64_t> s_next_request_id;
  static void clear_pending_request(Engine::Core::EntityID entity_id);
  static void move_group(Engine::Core::World &world,
                         const std::vector<Engine::Core::EntityID> &units,
                         const std::vector<QVector3D> &targets,
                         const MoveOptions &options);
};

} // namespace Game::Systems
