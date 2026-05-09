#include "visibility_service.h"

#include "../core/component.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/owner_registry.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

namespace Game::Map {

namespace {

constexpr float k_default_vision_range = 12.0F;
constexpr float k_half_cell_offset = 0.5F;
constexpr float k_min_tile_size = 0.0001F;
constexpr std::chrono::milliseconds k_min_job_interval{50};
constexpr std::uint8_t k_current_visible_marker = 0x80U;

auto in_bounds_static(int grid_x, int grid_z, int width, int height) -> bool {
  return grid_x >= 0 && grid_x < width && grid_z >= 0 && grid_z < height;
}

auto index_static(int grid_x, int grid_z, int width) -> int {
  return grid_z * width + grid_x;
}

auto world_to_grid_static(float world_coord, float half, float tile_size) -> int {
  const float grid_coord = world_coord / tile_size + half;
  return static_cast<int>(std::floor(grid_coord + k_half_cell_offset));
}

} // namespace

auto VisibilityService::instance() -> VisibilityService & {
  static VisibilityService s_instance;
  return s_instance;
}

VisibilityService::~VisibilityService() {
  m_shutdownRequested.store(true, std::memory_order_release);
  m_queueCv.notify_all();
  if (m_workerThread.joinable()) {
    m_workerThread.join();
  }
}

void VisibilityService::initialize(int width, int height, float tile_size) {
  std::unique_lock<std::shared_mutex> const lock(m_cellsMutex);
  m_width = std::max(1, width);
  m_height = std::max(1, height);
  m_tile_size = std::max(k_min_tile_size, tile_size);
  m_half_width =
      static_cast<float>(m_width) * k_half_cell_offset - k_half_cell_offset;
  m_half_height =
      static_cast<float>(m_height) * k_half_cell_offset - k_half_cell_offset;

  const int count = m_width * m_height;
  m_cells.assign(count, static_cast<std::uint8_t>(VisibilityState::Unseen));
  m_version.store(1, std::memory_order_release);
  m_generation.store(0, std::memory_order_release);
  reset_throttle();
  m_initialized = true;
}

void VisibilityService::reset() {
  if (!m_initialized) {
    return;
  }
  std::unique_lock<std::shared_mutex> const lock(m_cellsMutex);
  std::fill(m_cells.begin(), m_cells.end(),
            static_cast<std::uint8_t>(VisibilityState::Unseen));
  m_version.fetch_add(1, std::memory_order_release);
  m_lastPositions.clear();
  m_force_full_update = true;
  reset_throttle();
}

auto VisibilityService::update(Engine::Core::World &world,
                               int player_id) -> bool {
  if (!m_initialized) {
    return false;
  }

  bool integrated = false;
  {
    std::lock_guard<std::mutex> const lock(m_queueMutex);
    if (m_completedResult.has_value()) {
      integrate_result(std::move(m_completedResult.value()));
      m_completedResult.reset();
      integrated = true;
    }
  }

  if (should_start_new_job()) {
    auto sources = gather_vision_sources(world, player_id);

    if (!sources.empty()) {
      auto payload = compose_job_payload(sources);
      enqueue_job(std::move(payload));
    }
  }

  return integrated;
}

void VisibilityService::compute_immediate(Engine::Core::World &world,
                                         int player_id) {
  if (!m_initialized) {
    return;
  }

  const auto sources = gather_vision_sources(world, player_id);
  auto payload = compose_job_payload(sources);
  auto result = execute_job(std::move(payload));

  if (result.changed) {
    std::unique_lock<std::shared_mutex> const lock(m_cellsMutex);
    m_cells = std::move(result.cells);
    m_version.fetch_add(1, std::memory_order_release);
  }
  reset_throttle();
}

auto VisibilityService::gather_vision_sources(Engine::Core::World &world,
                                            int player_id)
    -> std::vector<VisibilityService::VisionSource> {
  std::vector<VisionSource> sources;
  const auto entities =
      world.get_entities_with<Engine::Core::TransformComponent>();
  const float range_padding = m_tile_size * k_half_cell_offset;

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  const float inverse_tile_size_sq = 1.0F / (m_tile_size * m_tile_size);

  std::unordered_map<std::uint32_t, CachedPosition> current_positions;
  bool any_moved = m_force_full_update;

  for (auto *entity : entities) {
    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (transform == nullptr || unit == nullptr) {
      continue;
    }

    if (Game::Core::is_neutral_owner(unit->owner_id)) {
      continue;
    }

    if (unit->owner_id != player_id &&
        !owner_registry.are_allies(player_id, unit->owner_id)) {
      continue;
    }

    if (unit->health <= 0) {
      continue;
    }

    const float vision_range =
        std::max(unit->vision_range, k_default_vision_range);
    const int center_x = world_to_grid(transform->position.x, m_half_width);
    const int center_z = world_to_grid(transform->position.z, m_half_height);
    if (!in_bounds(center_x, center_z)) {
      continue;
    }

    std::uint32_t const entity_id = entity->get_id();
    current_positions[entity_id] = {center_x, center_z};

    if (!any_moved) {
      auto it = m_lastPositions.find(entity_id);
      if (it == m_lastPositions.end() || it->second.grid_x != center_x ||
          it->second.grid_z != center_z) {
        any_moved = true;
      }
    }

    const int cell_radius =
        std::max(1, static_cast<int>(std::ceil(vision_range / m_tile_size)));
    const float expanded_range_sq =
        (vision_range + range_padding) * (vision_range + range_padding);
    const float expanded_radius_cells_sq =
        expanded_range_sq * inverse_tile_size_sq;

    sources.push_back(
        {center_x, center_z, cell_radius, expanded_radius_cells_sq});
  }

  if (!any_moved) {
    for (const auto &[entity_id, pos] : m_lastPositions) {
      if (current_positions.find(entity_id) == current_positions.end()) {
        any_moved = true;
        break;
      }
    }
  }

  m_lastPositions = std::move(current_positions);
  m_force_full_update = false;

  if (!any_moved && !sources.empty()) {
    return {};
  }

  return sources;
}

auto VisibilityService::compose_job_payload(
    const std::vector<VisionSource> &sources) const
    -> VisibilityService::JobPayload {
  std::shared_lock<std::shared_mutex> const lock(m_cellsMutex);
  const auto generation_value =
      m_generation.fetch_add(1ULL, std::memory_order_relaxed);
  return JobPayload{m_width, m_height, m_cells, sources, generation_value};
}

void VisibilityService::enqueue_job(JobPayload &&payload) {
  {
    std::lock_guard<std::mutex> const lock(m_queueMutex);
    m_pendingPayload = std::move(payload);
    m_lastJobStartTime = std::chrono::steady_clock::now();
  }
  ensure_worker_running();
  m_queueCv.notify_one();
}

void VisibilityService::integrate_result(JobResult &&result) {
  if (result.changed) {
    std::unique_lock<std::shared_mutex> const lock(m_cellsMutex);
    m_cells = std::move(result.cells);
    m_version.fetch_add(1, std::memory_order_release);
  }
}

void VisibilityService::ensure_worker_running() {
  bool expected = false;
  if (m_workerRunning.compare_exchange_strong(expected, true,
                                              std::memory_order_acq_rel)) {
    if (m_workerThread.joinable()) {
      m_workerThread.join();
    }
    m_workerThread = std::thread(&VisibilityService::worker_loop, this);
  }
}

void VisibilityService::worker_loop() {
  while (!m_shutdownRequested.load(std::memory_order_acquire)) {
    std::optional<JobPayload> payload_to_process;
    {
      std::unique_lock<std::mutex> lock(m_queueMutex);
      m_queueCv.wait_for(lock, std::chrono::milliseconds(100), [this] {
        return m_pendingPayload.has_value() ||
               m_shutdownRequested.load(std::memory_order_acquire);
      });
      if (m_shutdownRequested.load(std::memory_order_acquire)) {
        break;
      }
      if (m_pendingPayload.has_value()) {
        payload_to_process = std::move(m_pendingPayload);
        m_pendingPayload.reset();
      } else {
        m_workerRunning.store(false, std::memory_order_release);
        break;
      }
    }

    if (payload_to_process.has_value()) {
      auto result = execute_job(std::move(payload_to_process.value()));
      std::lock_guard<std::mutex> const lock(m_queueMutex);
      if (!m_completedResult.has_value() || result.changed) {
        m_completedResult = std::move(result);
      }
    }
  }
}

auto VisibilityService::execute_job(JobPayload payload)
    -> VisibilityService::JobResult {
  const int cell_count = payload.width * payload.height;
  const auto visible_val = static_cast<std::uint8_t>(VisibilityState::Visible);
  const auto explored_val =
      static_cast<std::uint8_t>(VisibilityState::Explored);

  for (const auto &source : payload.sources) {
    const int min_z = std::max(0, source.center_z - source.cell_radius);
    const int max_z =
        std::min(payload.height - 1, source.center_z + source.cell_radius);
    const int min_x = std::max(0, source.center_x - source.cell_radius);
    const int max_x =
        std::min(payload.width - 1, source.center_x + source.cell_radius);

    for (int grid_z = min_z; grid_z <= max_z; ++grid_z) {
      const int dz = grid_z - source.center_z;
      const int dz_sq = dz * dz;
      if (static_cast<float>(dz_sq) > source.expanded_radius_cells_sq) {
        continue;
      }

      for (int grid_x = min_x; grid_x <= max_x; ++grid_x) {
        const int dx = grid_x - source.center_x;
        const int dist_cells_sq = dx * dx + dz_sq;
        if (static_cast<float>(dist_cells_sq) <=
            source.expanded_radius_cells_sq) {
          const int idx = index_static(grid_x, grid_z, payload.width);
          payload.cells[idx] |= k_current_visible_marker;
        }
      }
    }
  }

  bool changed = false;
  for (int idx = 0; idx < cell_count; ++idx) {
    const std::uint8_t previous_state =
        payload.cells[idx] & ~k_current_visible_marker;
    const bool now_visible =
        (payload.cells[idx] & k_current_visible_marker) != 0U;
    const std::uint8_t next_state =
        now_visible
            ? visible_val
            : (previous_state == visible_val ? explored_val : previous_state);
    changed = changed || (next_state != previous_state);
    payload.cells[idx] = next_state;
  }

  return JobResult{std::move(payload.cells), payload.generation, changed};
}

auto VisibilityService::Snapshot::in_bounds(int grid_x,
                                           int grid_z) const -> bool {
  return grid_x >= 0 && grid_x < width && grid_z >= 0 && grid_z < height;
}

auto VisibilityService::Snapshot::index(int grid_x, int grid_z) const -> int {
  return grid_z * width + grid_x;
}

auto VisibilityService::Snapshot::world_to_grid(float world_coord,
                                              float half) const -> int {
  const float grid_coord = world_coord / tile_size + half;
  return static_cast<int>(std::floor(grid_coord + k_half_cell_offset));
}

auto VisibilityService::Snapshot::state_at(int grid_x,
                                          int grid_z) const -> VisibilityState {
  if (!initialized || !in_bounds(grid_x, grid_z)) {
    return VisibilityState::Visible;
  }
  const int idx = index(grid_x, grid_z);
  if (idx < 0 || static_cast<std::size_t>(idx) >= cells.size()) {
    return VisibilityState::Visible;
  }
  return static_cast<VisibilityState>(cells[static_cast<std::size_t>(idx)]);
}

auto VisibilityService::Snapshot::is_visible_world(float world_x,
                                                 float world_z) const -> bool {
  if (!initialized) {
    return true;
  }
  const int grid_x = world_to_grid(world_x, half_width);
  const int grid_z = world_to_grid(world_z, half_height);
  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }
  const int idx = index(grid_x, grid_z);
  if (idx < 0 || static_cast<std::size_t>(idx) >= cells.size()) {
    return false;
  }
  return cells[static_cast<std::size_t>(idx)] ==
         static_cast<std::uint8_t>(VisibilityState::Visible);
}

auto VisibilityService::Snapshot::is_explored_world(float world_x,
                                                  float world_z) const -> bool {
  if (!initialized) {
    return true;
  }
  const int grid_x = world_to_grid(world_x, half_width);
  const int grid_z = world_to_grid(world_z, half_height);
  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }
  const int idx = index(grid_x, grid_z);
  if (idx < 0 || static_cast<std::size_t>(idx) >= cells.size()) {
    return false;
  }
  const auto state = cells[static_cast<std::size_t>(idx)];
  return state == static_cast<std::uint8_t>(VisibilityState::Visible) ||
         state == static_cast<std::uint8_t>(VisibilityState::Explored);
}

auto VisibilityService::state_at(int grid_x,
                                int grid_z) const -> VisibilityState {
  if (!m_initialized || !in_bounds(grid_x, grid_z)) {
    return VisibilityState::Visible;
  }
  std::shared_lock<std::shared_mutex> const lock(m_cellsMutex);
  return static_cast<VisibilityState>(m_cells[index(grid_x, grid_z)]);
}

auto VisibilityService::is_visible_world(float world_x,
                                       float world_z) const -> bool {
  if (!m_initialized) {
    return true;
  }
  const int grid_x = world_to_grid(world_x, m_half_width);
  const int grid_z = world_to_grid(world_z, m_half_height);
  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }
  std::shared_lock<std::shared_mutex> const lock(m_cellsMutex);
  return m_cells[index(grid_x, grid_z)] ==
         static_cast<std::uint8_t>(VisibilityState::Visible);
}

auto VisibilityService::is_explored_world(float world_x,
                                        float world_z) const -> bool {
  if (!m_initialized) {
    return true;
  }
  const int grid_x = world_to_grid(world_x, m_half_width);
  const int grid_z = world_to_grid(world_z, m_half_height);
  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }
  std::shared_lock<std::shared_mutex> const lock(m_cellsMutex);
  const auto state = m_cells[index(grid_x, grid_z)];
  return state == static_cast<std::uint8_t>(VisibilityState::Visible) ||
         state == static_cast<std::uint8_t>(VisibilityState::Explored);
}

auto VisibilityService::snapshot() const -> VisibilityService::Snapshot {
  std::shared_lock<std::shared_mutex> const lock(m_cellsMutex);
  Snapshot shot;
  shot.initialized = m_initialized;
  shot.width = m_width;
  shot.height = m_height;
  shot.tile_size = m_tile_size;
  shot.half_width = m_half_width;
  shot.half_height = m_half_height;
  shot.cells = m_cells;
  return shot;
}

auto VisibilityService::snapshot_cells() const -> std::vector<std::uint8_t> {
  std::shared_lock<std::shared_mutex> const lock(m_cellsMutex);
  return m_cells;
}

void VisibilityService::reveal_all() {
  if (!m_initialized) {
    return;
  }
  std::unique_lock<std::shared_mutex> const lock(m_cellsMutex);
  std::fill(m_cells.begin(), m_cells.end(),
            static_cast<std::uint8_t>(VisibilityState::Visible));
  m_version.fetch_add(1, std::memory_order_release);
  reset_throttle();
}

auto VisibilityService::in_bounds(int grid_x, int grid_z) const -> bool {
  return grid_x >= 0 && grid_x < m_width && grid_z >= 0 && grid_z < m_height;
}

auto VisibilityService::index(int grid_x, int grid_z) const -> int {
  return grid_z * m_width + grid_x;
}

auto VisibilityService::world_to_grid(float world_coord,
                                    float half) const -> int {
  const float grid_coord = world_coord / m_tile_size + half;
  return static_cast<int>(std::floor(grid_coord + k_half_cell_offset));
}

auto VisibilityService::should_start_new_job() const -> bool {
  const auto now = std::chrono::steady_clock::now();
  return (now - m_lastJobStartTime) >= k_min_job_interval;
}

void VisibilityService::reset_throttle() { m_lastJobStartTime = {}; }

} // namespace Game::Map
