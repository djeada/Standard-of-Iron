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

auto inBoundsStatic(int grid_x, int grid_z, int width, int height) -> bool {
  return grid_x >= 0 && grid_x < width && grid_z >= 0 && grid_z < height;
}

auto indexStatic(int grid_x, int grid_z, int width) -> int {
  return grid_z * width + grid_x;
}

auto worldToGridStatic(float world_coord, float half, float tile_size) -> int {
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
  resetThrottle();
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
  resetThrottle();
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
      integrateResult(std::move(m_completedResult.value()));
      m_completedResult.reset();
      integrated = true;
    }
  }

  if (shouldStartNewJob()) {
    const auto sources = gatherVisionSources(world, player_id);
    auto payload = composeJobPayload(sources);
    enqueueJob(std::move(payload));
  }

  return integrated;
}

void VisibilityService::computeImmediate(Engine::Core::World &world,
                                         int player_id) {
  if (!m_initialized) {
    return;
  }

  const auto sources = gatherVisionSources(world, player_id);
  auto payload = composeJobPayload(sources);
  auto result = executeJob(std::move(payload));

  if (result.changed) {
    std::unique_lock<std::shared_mutex> const lock(m_cellsMutex);
    m_cells = std::move(result.cells);
    m_version.fetch_add(1, std::memory_order_release);
  }
  resetThrottle();
}

auto VisibilityService::gatherVisionSources(Engine::Core::World &world,
                                            int player_id) const
    -> std::vector<VisibilityService::VisionSource> {
  std::vector<VisionSource> sources;
  const auto entities =
      world.get_entities_with<Engine::Core::TransformComponent>();
  const float range_padding = m_tile_size * k_half_cell_offset;

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();

  for (auto *entity : entities) {
    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (transform == nullptr || unit == nullptr) {
      continue;
    }

    if (Game::Core::isNeutralOwner(unit->owner_id)) {
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
    const int center_x = worldToGrid(transform->position.x, m_half_width);
    const int center_z = worldToGrid(transform->position.z, m_half_height);
    if (!inBounds(center_x, center_z)) {
      continue;
    }

    const int cell_radius =
        std::max(1, static_cast<int>(std::ceil(vision_range / m_tile_size)));
    const float expanded_range_sq =
        (vision_range + range_padding) * (vision_range + range_padding);

    sources.push_back({center_x, center_z, cell_radius, expanded_range_sq});
  }

  return sources;
}

auto VisibilityService::composeJobPayload(
    const std::vector<VisionSource> &sources) const
    -> VisibilityService::JobPayload {
  std::shared_lock<std::shared_mutex> const lock(m_cellsMutex);
  const auto generation_value =
      m_generation.fetch_add(1ULL, std::memory_order_relaxed);
  return JobPayload{m_width, m_height, m_tile_size,
                    m_cells, sources,  generation_value};
}

void VisibilityService::enqueueJob(JobPayload &&payload) {
  {
    std::lock_guard<std::mutex> const lock(m_queueMutex);
    m_pendingPayload = std::move(payload);
    m_lastJobStartTime = std::chrono::steady_clock::now();
  }
  ensureWorkerRunning();
  m_queueCv.notify_one();
}

void VisibilityService::integrateResult(JobResult &&result) {
  if (result.changed) {
    std::unique_lock<std::shared_mutex> const lock(m_cellsMutex);
    m_cells = std::move(result.cells);
    m_version.fetch_add(1, std::memory_order_release);
  }
}

void VisibilityService::ensureWorkerRunning() {
  bool expected = false;
  if (m_workerRunning.compare_exchange_strong(expected, true,
                                              std::memory_order_acq_rel)) {
    if (m_workerThread.joinable()) {
      m_workerThread.join();
    }
    m_workerThread = std::thread(&VisibilityService::workerLoop, this);
  }
}

void VisibilityService::workerLoop() {
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
      auto result = executeJob(std::move(payload_to_process.value()));
      std::lock_guard<std::mutex> const lock(m_queueMutex);
      if (!m_completedResult.has_value() || result.changed) {
        m_completedResult = std::move(result);
      }
    }
  }
}

auto VisibilityService::executeJob(JobPayload payload)
    -> VisibilityService::JobResult {
  const int cell_count = payload.width * payload.height;
  std::vector<std::uint8_t> current_visible(cell_count, 0U);

  for (const auto &source : payload.sources) {
    for (int dz = -source.cell_radius; dz <= source.cell_radius; ++dz) {
      const int grid_z = source.center_z + dz;
      if (!inBoundsStatic(source.center_x, grid_z, payload.width,
                          payload.height)) {
        continue;
      }
      const float world_dz = static_cast<float>(dz) * payload.tile_size;
      for (int dx = -source.cell_radius; dx <= source.cell_radius; ++dx) {
        const int grid_x = source.center_x + dx;
        if (!inBoundsStatic(grid_x, grid_z, payload.width, payload.height)) {
          continue;
        }
        const float world_dx = static_cast<float>(dx) * payload.tile_size;
        const float dist_sq = world_dx * world_dx + world_dz * world_dz;
        if (dist_sq <= source.expanded_range_sq) {
          const int idx = indexStatic(grid_x, grid_z, payload.width);
          current_visible[idx] = 1U;
        }
      }
    }
  }

  bool changed = false;
  for (int idx = 0; idx < cell_count; ++idx) {
    const std::uint8_t now_visible = current_visible[idx];
    const auto visible_val =
        static_cast<std::uint8_t>(VisibilityState::Visible);
    const auto explored_val =
        static_cast<std::uint8_t>(VisibilityState::Explored);

    if (now_visible != 0U) {
      if (payload.cells[idx] != visible_val) {
        payload.cells[idx] = visible_val;
        changed = true;
      }
    } else if (payload.cells[idx] == visible_val) {
      payload.cells[idx] = explored_val;
      changed = true;
    }
  }

  return JobResult{std::move(payload.cells), payload.generation, changed};
}

auto VisibilityService::stateAt(int grid_x,
                                int grid_z) const -> VisibilityState {
  if (!m_initialized || !inBounds(grid_x, grid_z)) {
    return VisibilityState::Visible;
  }
  std::shared_lock<std::shared_mutex> const lock(m_cellsMutex);
  return static_cast<VisibilityState>(m_cells[index(grid_x, grid_z)]);
}

auto VisibilityService::isVisibleWorld(float world_x,
                                       float world_z) const -> bool {
  if (!m_initialized) {
    return true;
  }
  const int grid_x = worldToGrid(world_x, m_half_width);
  const int grid_z = worldToGrid(world_z, m_half_height);
  if (!inBounds(grid_x, grid_z)) {
    return false;
  }
  std::shared_lock<std::shared_mutex> const lock(m_cellsMutex);
  return m_cells[index(grid_x, grid_z)] ==
         static_cast<std::uint8_t>(VisibilityState::Visible);
}

auto VisibilityService::isExploredWorld(float world_x,
                                        float world_z) const -> bool {
  if (!m_initialized) {
    return true;
  }
  const int grid_x = worldToGrid(world_x, m_half_width);
  const int grid_z = worldToGrid(world_z, m_half_height);
  if (!inBounds(grid_x, grid_z)) {
    return false;
  }
  std::shared_lock<std::shared_mutex> const lock(m_cellsMutex);
  const auto state = m_cells[index(grid_x, grid_z)];
  return state == static_cast<std::uint8_t>(VisibilityState::Visible) ||
         state == static_cast<std::uint8_t>(VisibilityState::Explored);
}

auto VisibilityService::snapshotCells() const -> std::vector<std::uint8_t> {
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
  resetThrottle();
}

auto VisibilityService::inBounds(int grid_x, int grid_z) const -> bool {
  return grid_x >= 0 && grid_x < m_width && grid_z >= 0 && grid_z < m_height;
}

auto VisibilityService::index(int grid_x, int grid_z) const -> int {
  return grid_z * m_width + grid_x;
}

auto VisibilityService::worldToGrid(float world_coord,
                                    float half) const -> int {
  const float grid_coord = world_coord / m_tile_size + half;
  return static_cast<int>(std::floor(grid_coord + k_half_cell_offset));
}

auto VisibilityService::shouldStartNewJob() const -> bool {
  const auto now = std::chrono::steady_clock::now();
  return (now - m_lastJobStartTime) >= k_min_job_interval;
}

void VisibilityService::resetThrottle() { m_lastJobStartTime = {}; }

} // namespace Game::Map
