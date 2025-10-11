#pragma once

#include <atomic>
#include <cstdint>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace Engine {
namespace Core {
class World;
}
} // namespace Engine

namespace Game {
namespace Map {

enum class VisibilityState : std::uint8_t {
  Unseen = 0,
  Explored = 1,
  Visible = 2
};

class VisibilityService {
public:
  static VisibilityService &instance();

  void initialize(int width, int height, float tileSize);
  void reset();
  bool update(Engine::Core::World &world, int playerId);
  void computeImmediate(Engine::Core::World &world, int playerId);

  bool isInitialized() const { return m_initialized; }

  int getWidth() const { return m_width; }
  int getHeight() const { return m_height; }
  float getTileSize() const { return m_tileSize; }

  VisibilityState stateAt(int gridX, int gridZ) const;
  bool isVisibleWorld(float worldX, float worldZ) const;
  bool isExploredWorld(float worldX, float worldZ) const;

  std::vector<std::uint8_t> snapshotCells() const;
  std::uint64_t version() const {
    return m_version.load(std::memory_order_relaxed);
  }

private:
  bool inBounds(int x, int z) const;
  int index(int x, int z) const;
  int worldToGrid(float worldCoord, float half) const;

  struct VisionSource {
    int centerX;
    int centerZ;
    int cellRadius;
    float expandedRangeSq;
  };

  struct JobPayload {
    int width;
    int height;
    float tileSize;
    std::vector<std::uint8_t> cells;
    std::vector<VisionSource> sources;
    std::uint64_t generation;
  };

  struct JobResult {
    std::vector<std::uint8_t> cells;
    std::uint64_t generation;
    bool changed;
  };

  std::vector<VisionSource> gatherVisionSources(Engine::Core::World &world,
                                                int playerId) const;
  JobPayload composeJobPayload(const std::vector<VisionSource> &sources) const;
  void startAsyncJob(JobPayload &&payload);
  bool integrateCompletedJob();
  static JobResult executeJob(JobPayload payload);

  VisibilityService() = default;

  bool m_initialized = false;
  int m_width = 0;
  int m_height = 0;
  float m_tileSize = 1.0f;
  float m_halfWidth = 0.0f;
  float m_halfHeight = 0.0f;

  mutable std::shared_mutex m_cellsMutex;
  std::vector<std::uint8_t> m_cells;
  std::atomic<std::uint64_t> m_version{0};
  std::atomic<std::uint64_t> m_generation{0};
  std::future<JobResult> m_pendingJob;
  std::atomic<bool> m_jobActive{false};
};

} // namespace Map
} // namespace Game
