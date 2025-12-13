#pragma once

#include <atomic>
#include <cstdint>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace Engine::Core {
class World;
}

namespace Game::Map {

enum class VisibilityState : std::uint8_t {
  Unseen = 0,
  Explored = 1,
  Visible = 2
};

class VisibilityService {
public:
  static auto instance() -> VisibilityService &;

  void initialize(int width, int height, float tile_size);
  void reset();
  auto update(Engine::Core::World &world, int player_id) -> bool;
  void computeImmediate(Engine::Core::World &world, int player_id);

  auto is_initialized() const -> bool { return m_initialized; }

  auto getWidth() const -> int { return m_width; }
  auto getHeight() const -> int { return m_height; }
  auto getTileSize() const -> float { return m_tile_size; }

  auto stateAt(int grid_x, int grid_z) const -> VisibilityState;
  auto isVisibleWorld(float world_x, float world_z) const -> bool;
  auto isExploredWorld(float world_x, float world_z) const -> bool;

  auto snapshotCells() const -> std::vector<std::uint8_t>;
  auto version() const -> std::uint64_t {
    return m_version.load(std::memory_order_relaxed);
  }

private:
  auto inBounds(int x, int z) const -> bool;
  auto index(int x, int z) const -> int;
  auto worldToGrid(float world_coord, float half) const -> int;

  struct VisionSource {
    int center_x;
    int center_z;
    int cell_radius;
    float expanded_range_sq;
  };

  struct JobPayload {
    int width;
    int height;
    float tile_size;
    std::vector<std::uint8_t> cells;
    std::vector<VisionSource> sources;
    std::uint64_t generation;
  };

  struct JobResult {
    std::vector<std::uint8_t> cells;
    std::uint64_t generation;
    bool changed;
  };

  auto gatherVisionSources(Engine::Core::World &world,
                           int player_id) const -> std::vector<VisionSource>;
  auto composeJobPayload(const std::vector<VisionSource> &sources) const
      -> JobPayload;
  void startAsyncJob(JobPayload &&payload);
  auto integrateCompletedJob() -> bool;
  static auto executeJob(JobPayload payload) -> JobResult;

  VisibilityService() = default;

  bool m_initialized = false;
  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  float m_half_width = 0.0F;
  float m_half_height = 0.0F;

  mutable std::shared_mutex m_cellsMutex;
  std::vector<std::uint8_t> m_cells;
  std::atomic<std::uint64_t> m_version{0};
  mutable std::atomic<std::uint64_t> m_generation{0};
  std::future<JobResult> m_pendingJob;
  std::atomic<bool> m_jobActive{false};
};

} // namespace Game::Map
