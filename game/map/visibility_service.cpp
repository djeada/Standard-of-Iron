#include "visibility_service.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component_commander.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/owner_registry.h"
#include "game/core/presentation_coverage.h"
#include "terrain.h"

namespace Game::Map {

auto VisibilityService::instance() -> VisibilityService& {
  return *Game::Session::ambient_services().visibility;
}

namespace {

constexpr float k_default_vision_range =
    Engine::Core::Defaults::k_unit_default_vision_range;
constexpr float k_fog_reveal_scale = Engine::Core::Defaults::k_vision_reveal_scale;
constexpr float k_half_cell_offset = 0.5F;
constexpr float k_min_tile_size = 0.0001F;
constexpr std::chrono::milliseconds k_min_job_interval{50};

auto index_static(int grid_x, int grid_z, int width) -> int {
  return grid_z * width + grid_x;
}

} // namespace

VisibilityService::~VisibilityService() {
  m_shutdown_requested.store(true, std::memory_order_release);
  m_queue_cv.notify_all();
  if (m_worker_thread.joinable()) {
    m_worker_thread.join();
  }
}

void VisibilityService::initialize(int width, int height, float tile_size) {
  reset_worker_state();
  std::lock_guard<std::mutex> const lock(m_publish_mutex);
  m_width = std::max(1, width);
  m_height = std::max(1, height);
  m_tile_size = std::max(k_min_tile_size, tile_size);
  m_half_width = static_cast<float>(m_width) * k_half_cell_offset - k_half_cell_offset;
  m_half_height =
      static_cast<float>(m_height) * k_half_cell_offset - k_half_cell_offset;
  m_initialized = true;
  publish_snapshot_locked(std::vector<std::uint8_t>(
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height),
      static_cast<std::uint8_t>(VisibilityState::Unseen)));
}

void VisibilityService::shutdown() {
  if (!m_initialized) {
    return;
  }
  reset_worker_state();
  std::lock_guard<std::mutex> const lock(m_publish_mutex);
  m_width = 0;
  m_height = 0;
  m_half_width = 0.0F;
  m_half_height = 0.0F;
  m_initialized = false;
  publish_snapshot_locked({});
}

void VisibilityService::reset() {
  if (!m_initialized) {
    return;
  }
  reset_worker_state();
  std::lock_guard<std::mutex> const lock(m_publish_mutex);
  publish_snapshot_locked(
      std::vector<std::uint8_t>(snapshot_ptr()->cells.size(),
                                static_cast<std::uint8_t>(VisibilityState::Unseen)));
}

auto VisibilityService::update(Engine::Core::World& world, int player_id) -> bool {
  if (!m_initialized) {
    return false;
  }

  bool integrated = false;
  {
    std::lock_guard<std::mutex> const lock(m_queue_mutex);
    if (m_completed_result.has_value()) {
      integrate_result(std::move(m_completed_result.value()));
      m_completed_result.reset();
      integrated = true;
    }
  }

  if (should_start_new_job()) {
    auto sources = gather_vision_sources(world, player_id);
    if (sources != m_last_sources) {
      m_last_sources = sources;
      enqueue_job(compose_job_payload(std::move(sources)));
    }
  }

  return integrated;
}

void VisibilityService::compute_immediate(Engine::Core::World& world, int player_id) {
  if (!m_initialized) {
    return;
  }

  m_last_sources = gather_vision_sources(world, player_id);
  integrate_result(execute_job(compose_job_payload(m_last_sources)));
  reset_throttle();
}

auto VisibilityService::step(Engine::Core::World& world, int player_id) -> bool {
  if (!m_initialized) {
    return false;
  }
  auto sources = gather_vision_sources(world, player_id);
  if (sources == m_last_sources) {
    return false;
  }
  m_last_sources = std::move(sources);
  integrate_result(execute_job(compose_job_payload(m_last_sources)));
  return true;
}

void VisibilityService::step_on_tick(Engine::Core::World& world,
                                     int player_id,
                                     std::uint64_t tick,
                                     std::uint64_t interval) {
  if (m_result_tick && tick >= *m_result_tick) {
    std::unique_lock lock(m_queue_mutex);
    m_queue_cv.wait(lock, [this] { return m_completed_result.has_value(); });
    auto result = std::move(*m_completed_result);
    m_completed_result.reset();
    m_result_tick.reset();
    lock.unlock();
    integrate_result(std::move(result));
  }
  if (tick % interval != 0 || m_result_tick) {
    return;
  }
  auto sources = gather_vision_sources(world, player_id);
  if (sources != m_last_sources) {
    m_last_sources = sources;
    m_result_tick = tick + 1;
    enqueue_job(compose_job_payload(std::move(sources)));
  }
}

auto VisibilityService::gather_vision_sources(
    Engine::Core::World& world, int player_id) const -> std::vector<VisionSource> {
  std::vector<VisionSource> sources;
  const float range_padding = m_tile_size * k_half_cell_offset;
  const float inverse_tile_size_sq = 1.0F / (m_tile_size * m_tile_size);
  const auto& owner_registry = *Game::Session::services_for(world).owners;

  auto add_source =
      [&](float world_x, float world_z, int cell_radius, float radius_sq) {
        const int center_x = world_to_grid(world_x, m_half_width);
        const int center_z = world_to_grid(world_z, m_half_height);
        if (!in_bounds(center_x, center_z)) {
          return false;
        }
        sources.push_back({center_z, center_x, cell_radius, radius_sq});
        return true;
      };

  for (auto [entity, transform, unit] :
       world.entity_view<const Engine::Core::TransformComponent,
                         const Engine::Core::UnitComponent>()) {
    if (Game::Core::is_neutral_owner(unit.owner_id) || unit.health <= 0 ||
        (unit.owner_id != player_id &&
         !owner_registry.are_allies(player_id, unit.owner_id))) {
      continue;
    }

    const float vision_range =
        std::max(unit.vision_range, k_default_vision_range) * k_fog_reveal_scale;
    const int cell_radius =
        std::max(1, static_cast<int>(std::ceil(vision_range / m_tile_size)));
    const float expanded_radius_cells_sq = (vision_range + range_padding) *
                                           (vision_range + range_padding) *
                                           inverse_tile_size_sq;

    if (!add_source(transform.position.x,
                    transform.position.z,
                    cell_radius,
                    expanded_radius_cells_sq)) {
      continue;
    }

    const auto* commander = entity.get_component<Engine::Core::CommanderComponent>();
    if (commander != nullptr && commander->flag_rally_flag_active) {
      add_source(commander->flag_rally_flag_x,
                 commander->flag_rally_flag_z,
                 cell_radius,
                 expanded_radius_cells_sq);
    }
  }

  std::sort(sources.begin(), sources.end());
  sources.erase(std::unique(sources.begin(), sources.end()), sources.end());
  return sources;
}

auto VisibilityService::compose_job_payload(std::vector<VisionSource> sources) const
    -> VisibilityService::JobPayload {
  return JobPayload{snapshot_ptr(),
                    std::move(sources),
                    m_generation.fetch_add(1ULL, std::memory_order_relaxed) + 1ULL};
}

void VisibilityService::enqueue_job(JobPayload&& payload) {
  {
    std::lock_guard<std::mutex> const lock(m_queue_mutex);
    m_pending_payload = std::move(payload);
    m_last_job_start_time = std::chrono::steady_clock::now();
  }
  ensure_worker_running();
  m_queue_cv.notify_one();
}

void VisibilityService::integrate_result(JobResult&& result) {
  if (!result.changed ||
      result.generation != m_generation.load(std::memory_order_acquire)) {
    return;
  }
  std::lock_guard<std::mutex> const lock(m_publish_mutex);
  publish_snapshot_locked(std::move(result.cells));
  Engine::Core::note_coverage(Engine::Core::CoverageEvent::FogReveal);
}

void VisibilityService::ensure_worker_running() {
  bool expected = false;
  if (m_worker_running.compare_exchange_strong(
          expected, true, std::memory_order_acq_rel)) {
    if (m_worker_thread.joinable()) {
      m_worker_thread.join();
    }
    m_worker_thread = std::thread(&VisibilityService::worker_loop, this);
  }
}

void VisibilityService::worker_loop() {
  while (!m_shutdown_requested.load(std::memory_order_acquire)) {
    std::optional<JobPayload> payload_to_process;
    {
      std::unique_lock<std::mutex> lock(m_queue_mutex);
      m_queue_cv.wait_for(lock, std::chrono::milliseconds(100), [this] {
        return m_pending_payload.has_value() ||
               m_shutdown_requested.load(std::memory_order_acquire);
      });
      if (m_shutdown_requested.load(std::memory_order_acquire)) {
        break;
      }
      if (m_pending_payload.has_value()) {
        payload_to_process = std::move(m_pending_payload);
        m_pending_payload.reset();
      } else {
        m_worker_running.store(false, std::memory_order_release);
        break;
      }
    }

    if (payload_to_process.has_value()) {
      auto result = execute_job(payload_to_process.value());
      std::lock_guard<std::mutex> const lock(m_queue_mutex);
      if (!m_completed_result.has_value() || result.changed) {
        m_completed_result = std::move(result);
      }
      m_queue_cv.notify_all();
    }
  }
}

auto VisibilityService::execute_job(const JobPayload& payload)
    -> VisibilityService::JobResult {
  const auto& base = *payload.base;
  const auto visible_val = static_cast<std::uint8_t>(VisibilityState::Visible);
  const auto explored_val = static_cast<std::uint8_t>(VisibilityState::Explored);

  std::vector<std::uint8_t> cells(base.cells.size());
  std::transform(
      base.cells.begin(), base.cells.end(), cells.begin(), [&](std::uint8_t state) {
        return state == visible_val ? explored_val : state;
      });

  for (const auto& source : payload.sources) {
    const int min_z = std::max(0, source.center_z - source.cell_radius);
    const int max_z = std::min(base.height - 1, source.center_z + source.cell_radius);
    for (int grid_z = min_z; grid_z <= max_z; ++grid_z) {
      const int dz = grid_z - source.center_z;
      const double room =
          static_cast<double>(source.expanded_radius_cells_sq) - dz * dz;
      if (room < 0.0) {
        continue;
      }
      const int half = std::min(source.cell_radius, static_cast<int>(std::sqrt(room)));
      const int min_x = std::max(0, source.center_x - half);
      const int max_x = std::min(base.width - 1, source.center_x + half);
      std::fill(cells.begin() + index_static(min_x, grid_z, base.width),
                cells.begin() + index_static(max_x, grid_z, base.width) + 1,
                visible_val);
    }
  }

  const bool changed = cells != base.cells;
  return JobResult{std::move(cells), payload.generation, changed};
}

auto VisibilityService::Snapshot::in_bounds(int grid_x, int grid_z) const -> bool {
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

auto VisibilityService::state_at(int grid_x, int grid_z) const -> VisibilityState {
  const auto current = snapshot_ptr();
  return current != nullptr ? current->state_at(grid_x, grid_z)
                            : Snapshot{}.state_at(grid_x, grid_z);
}

auto VisibilityService::is_visible_world(float world_x, float world_z) const -> bool {
  const auto current = snapshot_ptr();
  return current != nullptr ? current->is_visible_world(world_x, world_z)
                            : Snapshot{}.is_visible_world(world_x, world_z);
}

auto VisibilityService::is_explored_world(float world_x, float world_z) const -> bool {
  const auto current = snapshot_ptr();
  return current != nullptr ? current->is_explored_world(world_x, world_z)
                            : Snapshot{}.is_explored_world(world_x, world_z);
}

auto VisibilityService::snapshot() const -> VisibilityService::Snapshot {
  const auto snapshot = snapshot_ptr();
  return snapshot != nullptr ? *snapshot : Snapshot{};
}

auto VisibilityService::snapshot_ptr() const -> VisibilityService::SnapshotPtr {
  return std::atomic_load_explicit(&m_published_snapshot, std::memory_order_acquire);
}

auto VisibilityService::snapshot_if_newer(std::uint64_t known_version) const
    -> VisibilityService::SnapshotPtr {
  auto snapshot = snapshot_ptr();
  if (snapshot == nullptr || snapshot->version <= known_version) {
    return nullptr;
  }
  return snapshot;
}

void VisibilityService::reveal_all() {
  if (!m_initialized) {
    return;
  }
  std::lock_guard<std::mutex> const lock(m_publish_mutex);
  reset_throttle();
  publish_snapshot_locked(
      std::vector<std::uint8_t>(snapshot_ptr()->cells.size(),
                                static_cast<std::uint8_t>(VisibilityState::Visible)));
}

void VisibilityService::reveal_scouted_areas(
    std::span<const ScoutedArea> scouted_areas) {
  if (!scouted_areas.empty() && is_initialized()) {
    const int width = get_width();
    const int height = get_height();
    const float tile_size = get_tile_size();
    std::vector<std::uint8_t> known(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U);
    const float half_width = static_cast<float>(width) * 0.5F - 0.5F;
    const float half_height = static_cast<float>(height) * 0.5F - 0.5F;
    for (const auto& area : scouted_areas) {
      const float centre_x = area.x / tile_size + half_width;
      const float centre_z = area.z / tile_size + half_height;
      const float radius = area.radius / tile_size;
      const int min_z = std::max(0, static_cast<int>(std::floor(centre_z - radius)));
      const int max_z =
          std::min(height - 1, static_cast<int>(std::ceil(centre_z + radius)));
      const int min_x = std::max(0, static_cast<int>(std::floor(centre_x - radius)));
      const int max_x =
          std::min(width - 1, static_cast<int>(std::ceil(centre_x + radius)));
      for (int grid_z = min_z; grid_z <= max_z; ++grid_z) {
        for (int grid_x = min_x; grid_x <= max_x; ++grid_x) {
          const float dx = static_cast<float>(grid_x) - centre_x;
          const float dz = static_cast<float>(grid_z) - centre_z;
          if ((dx * dx) + (dz * dz) <= radius * radius) {
            known[static_cast<std::size_t>(grid_z) * static_cast<std::size_t>(width) +
                  static_cast<std::size_t>(grid_x)] = 1U;
          }
        }
      }
    }
    (void)restore_explored(known, width, height);
  }
}

auto VisibilityService::restore_explored(const std::vector<std::uint8_t>& explored,
                                         int width,
                                         int height) -> bool {
  if (!m_initialized) {
    return false;
  }
  std::lock_guard<std::mutex> const lock(m_publish_mutex);
  auto cells = snapshot_ptr()->cells;
  if (width != m_width || height != m_height || explored.size() != cells.size()) {
    return false;
  }

  const auto explored_val = static_cast<std::uint8_t>(VisibilityState::Explored);
  const auto unseen_val = static_cast<std::uint8_t>(VisibilityState::Unseen);
  bool changed = false;
  for (std::size_t idx = 0; idx < cells.size(); ++idx) {
    if (explored[idx] != 0U && cells[idx] == unseen_val) {
      cells[idx] = explored_val;
      changed = true;
    }
  }

  if (changed) {
    publish_snapshot_locked(std::move(cells));
  }
  return true;
}

auto VisibilityService::in_bounds(int grid_x, int grid_z) const -> bool {
  return grid_x >= 0 && grid_x < m_width && grid_z >= 0 && grid_z < m_height;
}

auto VisibilityService::world_to_grid(float world_coord, float half) const -> int {
  const float grid_coord = world_coord / m_tile_size + half;
  return static_cast<int>(std::floor(grid_coord + k_half_cell_offset));
}

auto VisibilityService::should_start_new_job() const -> bool {
  const auto now = std::chrono::steady_clock::now();
  return (now - m_last_job_start_time) >= k_min_job_interval;
}

void VisibilityService::reset_throttle() {
  m_last_job_start_time = {};
}

void VisibilityService::reset_worker_state() {
  std::lock_guard<std::mutex> const lock(m_queue_mutex);
  m_pending_payload.reset();
  m_completed_result.reset();
  m_generation.store(0, std::memory_order_release);
  m_last_sources.clear();
  m_result_tick.reset();
  reset_throttle();
}

auto SimulationVisibility::grid_for(int owner) -> VisibilityService& {
  return m_grids[owner];
}

auto SimulationVisibility::grid_for(int owner) const -> const VisibilityService* {
  const auto found = m_grids.find(owner);
  return found != m_grids.end() ? &found->second : nullptr;
}

void SimulationVisibility::advance(Engine::Core::World& world,
                                   const Game::Systems::OwnerRegistry& owners,
                                   const TerrainHeightMap& map,
                                   std::uint64_t tick,
                                   double tick_seconds) {
  constexpr double refresh_seconds = 0.075;
  const auto interval = static_cast<std::uint64_t>(
      std::max(1.0, std::ceil(refresh_seconds / tick_seconds)));
  for (const int owner : owners.get_player_owner_ids()) {
    auto& visibility = grid_for(owner);
    if (!visibility.is_initialized()) {
      visibility.initialize(map.get_width(), map.get_height(), map.get_tile_size());
      visibility.compute_immediate(world, owner);
      visibility.reveal_scouted_areas(m_scouted_areas);
    }
    visibility.step_on_tick(world, owner, tick, interval);
  }
}

void SimulationVisibility::set_scouted_areas(const std::vector<ScoutedArea>& areas) {
  m_scouted_areas = areas;
}

void SimulationVisibility::clear() {
  m_grids.clear();
  m_scouted_areas.clear();
}

auto SimulationVisibility::generation() const -> std::uint64_t {
  std::uint64_t result = 0;
  for (const auto& [owner, visibility] : m_grids) {
    if (const auto snapshot = visibility.snapshot_ptr()) {
      result = (result * 1099511628211ULL) ^ static_cast<std::uint64_t>(owner);
      result = (result * 1099511628211ULL) ^ snapshot->version;
    }
  }
  return result;
}

void VisibilityService::publish_snapshot_locked(std::vector<std::uint8_t> cells) {
  const auto previous = snapshot_ptr();
  auto snapshot = std::make_shared<Snapshot>();
  snapshot->version = previous != nullptr ? previous->version + 1ULL : 1ULL;
  snapshot->initialized = m_initialized;
  snapshot->width = m_width;
  snapshot->height = m_height;
  snapshot->tile_size = m_tile_size;
  snapshot->half_width = m_half_width;
  snapshot->half_height = m_half_height;
  snapshot->cells = std::move(cells);
  std::atomic_store_explicit(&m_published_snapshot,
                             std::shared_ptr<const Snapshot>(std::move(snapshot)),
                             std::memory_order_release);
}

} // namespace Game::Map
