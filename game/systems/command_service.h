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
    bool allowDirectFallback = true;
    bool clearAttackIntent = true;
    bool groupMove = false;
  };

  static constexpr int DIRECT_PATH_THRESHOLD = 8;

  static constexpr float WAYPOINT_SKIP_THRESHOLD_SQ = 0.16F;

  static void initialize(int worldWidth, int worldHeight);

  static auto getPathfinder() -> Pathfinding *;

  static void moveUnits(Engine::Core::World &world,
                        const std::vector<Engine::Core::EntityID> &units,
                        const std::vector<QVector3D> &targets);

  static void moveUnits(Engine::Core::World &world,
                        const std::vector<Engine::Core::EntityID> &units,
                        const std::vector<QVector3D> &targets,
                        const MoveOptions &options);

  static void processPathResults(Engine::Core::World &world);

  static void attack_target(Engine::Core::World &world,
                            const std::vector<Engine::Core::EntityID> &units,
                            Engine::Core::EntityID target_id,
                            bool shouldChase = true);

private:
  struct PendingPathRequest {
    Engine::Core::EntityID entity_id{};
    QVector3D target;
    MoveOptions options;
    std::vector<Engine::Core::EntityID> groupMembers;
    std::vector<QVector3D> groupTargets;
  };

  static std::unique_ptr<Pathfinding> s_pathfinder;
  static std::unordered_map<std::uint64_t, PendingPathRequest>
      s_pendingRequests;
  static std::unordered_map<Engine::Core::EntityID, std::uint64_t>
      s_entityToRequest;
  static std::mutex s_pendingMutex;
  static std::atomic<std::uint64_t> s_nextRequestId;
  static auto worldToGrid(float world_x, float world_z) -> Point;
  static auto gridToWorld(const Point &gridPos) -> QVector3D;
  static void clearPendingRequest(Engine::Core::EntityID entity_id);
  static void moveGroup(Engine::Core::World &world,
                        const std::vector<Engine::Core::EntityID> &units,
                        const std::vector<QVector3D> &targets,
                        const MoveOptions &options);
};

} // namespace Game::Systems
