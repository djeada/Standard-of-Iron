#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "game/core/nav_profile.h"
#include "gate_service.h"
#include "map/forest_outline.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "pathfinding.h"
#include "systems/building_collision_registry.h"

namespace Game::Systems {

namespace {
void merge_dirty_regions(std::vector<DirtyRegion>& regions) {
  constexpr std::size_t k_max_individual_regions = 256U;
  if (regions.size() > k_max_individual_regions) {
    DirtyRegion merged = regions.front();
    for (auto const& region : regions) {
      merged.min_x = std::min(merged.min_x, region.min_x);
      merged.max_x = std::max(merged.max_x, region.max_x);
      merged.min_z = std::min(merged.min_z, region.min_z);
      merged.max_z = std::max(merged.max_z, region.max_z);
    }
    regions.assign(1U, merged);
    return;
  }
  for (std::size_t i = 0; i < regions.size(); ++i) {
    for (std::size_t j = i + 1; j < regions.size();) {
      bool const overlaps_x = regions[i].min_x <= regions[j].max_x + 1 &&
                              regions[j].min_x <= regions[i].max_x + 1;
      bool const overlaps_z = regions[i].min_z <= regions[j].max_z + 1 &&
                              regions[j].min_z <= regions[i].max_z + 1;
      if (!overlaps_x || !overlaps_z) {
        ++j;
        continue;
      }
      regions[i].min_x = std::min(regions[i].min_x, regions[j].min_x);
      regions[i].max_x = std::max(regions[i].max_x, regions[j].max_x);
      regions[i].min_z = std::min(regions[i].min_z, regions[j].min_z);
      regions[i].max_z = std::max(regions[i].max_z, regions[j].max_z);
      regions[j] = regions.back();
      regions.pop_back();
      j = i + 1;
    }
  }
}

} // namespace

void Pathfinding::mark_navigation_grid_dirty() {
  std::lock_guard<std::mutex> const lock(m_dirty_mutex);
  m_full_update_required = true;
  m_navigation_grid_dirty.store(true, std::memory_order_release);
}

void Pathfinding::mark_region_dirty(int min_x, int max_x, int min_z, int max_z) {

  if (!clamp_to_grid(min_x, max_x, min_z, max_z)) {
    return;
  }

  std::lock_guard<std::mutex> const lock(m_dirty_mutex);
  m_dirty_regions.emplace_back(min_x, max_x, min_z, max_z);
  m_navigation_grid_dirty.store(true, std::memory_order_release);
}

void Pathfinding::mark_building_region_dirty(float center_x,
                                             float center_z,
                                             float width,
                                             float depth) {
  float const padding = BuildingCollisionRegistry::get_grid_padding();
  float const half_width = width / 2.0F + padding;
  float const half_depth = depth / 2.0F + padding;

  int const min_x =
      static_cast<int>(std::floor(center_x - half_width - m_grid_offset_x));
  int const max_x =
      static_cast<int>(std::ceil(center_x + half_width - m_grid_offset_x));
  int const min_z =
      static_cast<int>(std::floor(center_z - half_depth - m_grid_offset_z));
  int const max_z =
      static_cast<int>(std::ceil(center_z + half_depth - m_grid_offset_z));

  mark_region_dirty(min_x, max_x, min_z, max_z);
}

void Pathfinding::mark_obstruction_released() {
  m_obstruction_center_located.store(false, std::memory_order_relaxed);
  m_obstruction_revision.fetch_add(1, std::memory_order_acq_rel);
}

void Pathfinding::mark_obstruction_released_at(float center_x, float center_z) {
  m_obstruction_center_x.store(center_x, std::memory_order_relaxed);
  m_obstruction_center_z.store(center_z, std::memory_order_relaxed);
  m_obstruction_center_located.store(true, std::memory_order_relaxed);
  m_obstruction_revision.fetch_add(1, std::memory_order_acq_rel);
}

auto Pathfinding::last_obstruction_release() const -> ObstructionRelease {
  if (!m_obstruction_center_located.load(std::memory_order_relaxed)) {
    return {};
  }
  return {.center = QVector3D(m_obstruction_center_x.load(std::memory_order_relaxed),
                              0.0F,
                              m_obstruction_center_z.load(std::memory_order_relaxed)),
          .located = true};
}

auto Pathfinding::obstruction_revision() const -> std::uint64_t {
  return m_obstruction_revision.load(std::memory_order_acquire);
}

auto Pathfinding::process_dirty_regions() -> DirtyRegion {
  std::vector<DirtyRegion> regions_to_process;

  {
    std::lock_guard<std::mutex> const lock(m_dirty_mutex);
    if (m_full_update_required) {

      m_dirty_regions.clear();
      m_full_update_required = false;

      m_navigation_grid.fill(CellValue::Walkable);
      update_region(0, m_width - 1, 0, m_height - 1);

      return {0, m_width - 1, 0, m_height - 1};
    }

    regions_to_process = std::move(m_dirty_regions);
    m_dirty_regions.clear();
  }

  if (regions_to_process.empty()) {
    return {0, -1, 0, -1};
  }

  merge_dirty_regions(regions_to_process);

  DirtyRegion rebuilt{regions_to_process.front().min_x,
                      regions_to_process.front().max_x,
                      regions_to_process.front().min_z,
                      regions_to_process.front().max_z};
  for (const auto& region : regions_to_process) {
    update_region(region.min_x, region.max_x, region.min_z, region.max_z);
    rebuilt.min_x = std::min(rebuilt.min_x, region.min_x);
    rebuilt.max_x = std::max(rebuilt.max_x, region.max_x);
    rebuilt.min_z = std::min(rebuilt.min_z, region.min_z);
    rebuilt.max_z = std::max(rebuilt.max_z, region.max_z);
  }
  return rebuilt;
}

void Pathfinding::note_navigation_change(int min_x, int max_x, int min_z, int max_z) {
  std::lock_guard<std::mutex> const lock(m_nav_change_mutex);
  if (m_nav_changes.size() >= k_max_tracked_nav_changes) {
    m_nav_changes.erase(m_nav_changes.begin());
  }
  m_nav_changes.push_back({.revision = navigation_revision(),
                           .min_x = min_x,
                           .max_x = max_x,
                           .min_z = min_z,
                           .max_z = max_z});
}

auto Pathfinding::navigation_changes_since(std::uint64_t from_revision,
                                           std::uint64_t to_revision,
                                           std::vector<NavChange>& changes) -> bool {
  std::lock_guard<std::mutex> const lock(m_nav_change_mutex);
  for (const NavChange& change : m_nav_changes) {
    if (change.revision > from_revision && change.revision <= to_revision) {
      changes.push_back(change);
    }
  }
  return !changes.empty() && m_nav_changes.front().revision <= from_revision + 1;
}

void Pathfinding::drop_paths_crossing_changes(std::uint64_t from_revision,
                                              std::uint64_t to_revision) {
  std::vector<NavChange> changes;
  if (!navigation_changes_since(from_revision, to_revision, changes)) {
    Engine::Core::count_nav(Engine::Core::NavCounter::RouteCacheFlushes);
    Engine::Core::count_nav(Engine::Core::NavCounter::RouteCacheEvictions,
                            static_cast<std::uint64_t>(m_path_cache.size()));
    m_path_cache.clear();
    return;
  }

  constexpr int k_margin = 1;
  std::uint64_t dropped = 0;
  for (auto it = m_path_cache.begin(); it != m_path_cache.end();) {
    const auto& [key, entry] = *it;
    const bool reached_goal =
        !entry.path.empty() && entry.path.back() == Point{key.end_x, key.end_y};
    const bool crosses =
        !reached_goal ||
        std::any_of(changes.begin(), changes.end(), [&](const NavChange& change) {
          return entry.min_x <= change.max_x + k_margin &&
                 entry.max_x >= change.min_x - k_margin &&
                 entry.min_z <= change.max_z + k_margin &&
                 entry.max_z >= change.min_z - k_margin;
        });
    if (crosses) {
      it = m_path_cache.erase(it);
      ++dropped;
    } else {
      ++it;
    }
  }
  Engine::Core::count_nav(Engine::Core::NavCounter::RouteCacheEvictions, dropped);
  Engine::Core::count_nav(Engine::Core::NavCounter::RouteCacheKept,
                          static_cast<std::uint64_t>(m_path_cache.size()));
}

void Pathfinding::evict_cold_paths() {
  if (m_path_cache.size() < k_max_cached_paths) {
    return;
  }
  const std::size_t target = k_max_cached_paths / 4U;
  std::vector<std::pair<std::uint64_t, PathCacheKey>> by_age;
  by_age.reserve(m_path_cache.size());
  for (const auto& [cache_key, entry] : m_path_cache) {
    by_age.emplace_back(entry.last_used, cache_key);
  }
  std::partial_sort(
      by_age.begin(),
      by_age.begin() + static_cast<std::ptrdiff_t>(target),
      by_age.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
  for (std::size_t i = 0; i < target; ++i) {
    m_path_cache.erase(by_age[i].second);
  }
  Engine::Core::count_nav(Engine::Core::NavCounter::RouteCacheEvictions,
                          static_cast<std::uint64_t>(target));
}

} // namespace Game::Systems
