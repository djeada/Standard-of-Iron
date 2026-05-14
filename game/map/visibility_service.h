#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
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
  struct Snapshot {
    bool initialized = false;
    int width = 0;
    int height = 0;
    float tile_size = 1.0F;
    float half_width = 0.0F;
    float half_height = 0.0F;
    std::vector<std::uint8_t> cells;

    auto state_at(int grid_x, int grid_z) const -> VisibilityState;
    auto is_visible_world(float world_x, float world_z) const -> bool;
    auto is_explored_world(float world_x, float world_z) const -> bool;

  private:
    auto in_bounds(int grid_x, int grid_z) const -> bool;
    auto index(int grid_x, int grid_z) const -> int;
    auto world_to_grid(float world_coord, float half) const -> int;
  };

  static auto instance() -> VisibilityService&;

  void initialize(int width, int height, float tile_size);
  void reset();
  auto update(Engine::Core::World& world, int player_id) -> bool;
  void compute_immediate(Engine::Core::World& world, int player_id);

  auto is_initialized() const -> bool { return m_initialized; }

  auto get_width() const -> int { return m_width; }
  auto get_height() const -> int { return m_height; }
  auto get_tile_size() const -> float { return m_tile_size; }

  auto state_at(int grid_x, int grid_z) const -> VisibilityState;
  auto is_visible_world(float world_x, float world_z) const -> bool;
  auto is_explored_world(float world_x, float world_z) const -> bool;
  auto snapshot() const -> Snapshot;

  auto snapshot_cells() const -> std::vector<std::uint8_t>;
  auto version() const -> std::uint64_t {
    return m_version.load(std::memory_order_relaxed);
  }

  void reveal_all();

  ~VisibilityService();

private:
  auto in_bounds(int x, int z) const -> bool;
  auto index(int x, int z) const -> int;
  auto world_to_grid(float world_coord, float half) const -> int;

  struct VisionSource {
    int center_x;
    int center_z;
    int cell_radius;
    float expanded_radius_cells_sq;
  };

  struct JobPayload {
    int width;
    int height;
    std::vector<std::uint8_t> cells;
    std::vector<VisionSource> sources;
    std::uint64_t generation;
  };

  struct JobResult {
    std::vector<std::uint8_t> cells;
    std::uint64_t generation;
    bool changed;
  };

  auto gather_vision_sources(Engine::Core::World& world,
                             int player_id) -> std::vector<VisionSource>;
  auto
  compose_job_payload(const std::vector<VisionSource>& sources) const -> JobPayload;
  void enqueue_job(JobPayload&& payload);
  void integrate_result(JobResult&& result);
  auto should_start_new_job() const -> bool;
  void reset_throttle();
  void worker_loop();
  void ensure_worker_running();
  static auto execute_job(JobPayload payload) -> JobResult;

  VisibilityService() = default;

  bool m_initialized = false;
  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  float m_half_width = 0.0F;
  float m_half_height = 0.0F;

  mutable std::shared_mutex m_cells_mutex;
  std::vector<std::uint8_t> m_cells;
  std::atomic<std::uint64_t> m_version{0};
  mutable std::atomic<std::uint64_t> m_generation{0};
  std::chrono::steady_clock::time_point m_last_job_start_time{};

  std::mutex m_queue_mutex;
  std::condition_variable m_queue_cv;
  std::optional<JobPayload> m_pending_payload;
  std::optional<JobResult> m_completed_result;
  std::thread m_worker_thread;
  std::atomic<bool> m_worker_running{false};
  std::atomic<bool> m_shutdown_requested{false};

  struct CachedPosition {
    int grid_x;
    int grid_z;
  };
  std::unordered_map<std::uint32_t, CachedPosition> m_last_positions;
  bool m_force_full_update{true};
};

} // namespace Game::Map
