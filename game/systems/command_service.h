#pragma once

#include <QVector3D>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Engine {
namespace Core {
class World;
using EntityID = unsigned int;
struct MovementComponent;
} // namespace Core
} // namespace Engine

namespace Game {
namespace Systems {

class Pathfinding;
struct Point;

class CommandService {
public:
  struct MoveOptions {
    bool allowDirectFallback = true;
    bool clearAttackIntent = true;
  };

  static constexpr int DIRECT_PATH_THRESHOLD = 8;

  static constexpr float WAYPOINT_SKIP_THRESHOLD_SQ = 0.16f;

  static void initialize(int worldWidth, int worldHeight);

  static Pathfinding *getPathfinder();

  static void moveUnits(Engine::Core::World &world,
                        const std::vector<Engine::Core::EntityID> &units,
                        const std::vector<QVector3D> &targets);

  static void moveUnits(Engine::Core::World &world,
                        const std::vector<Engine::Core::EntityID> &units,
                        const std::vector<QVector3D> &targets,
                        const MoveOptions &options);

  static void processPathResults(Engine::Core::World &world);

  static void attackTarget(Engine::Core::World &world,
                           const std::vector<Engine::Core::EntityID> &units,
                           Engine::Core::EntityID targetId,
                           bool shouldChase = true);

private:
  struct PendingPathRequest {
    Engine::Core::EntityID entityId;
    QVector3D target;
    MoveOptions options;
  };

  static std::unique_ptr<Pathfinding> s_pathfinder;
  static std::unordered_map<std::uint64_t, PendingPathRequest>
      s_pendingRequests;
  static std::unordered_map<Engine::Core::EntityID, std::uint64_t>
      s_entityToRequest;
  static std::mutex s_pendingMutex;
  static std::atomic<std::uint64_t> s_nextRequestId;
  static Point worldToGrid(float worldX, float worldZ);
  static QVector3D gridToWorld(const Point &gridPos);
  static void clearPendingRequest(Engine::Core::EntityID entityId);
};

} // namespace Systems
} // namespace Game
