#include <cstdio>
#include <cstdlib>
#include "builder_stall_watch.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "units/spawn_type.h"

namespace Game::Systems::AI {

void WorkerStallWatch::review(const AISnapshot& snapshot, float now) {

  constexpr float k_stall_seconds = 40.0F;

  constexpr float k_sour_seconds = 240.0F;

  m_stalled_builders.clear();
  std::erase_if(m_sour_nodes, [now](const auto& entry) { return now >= entry.second; });

  std::unordered_map<Engine::Core::EntityID, WorkerWatch> surviving;
  surviving.reserve(m_worker_watch.size());

  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type != Game::Units::SpawnType::Builder ||
        !entity.builder_production.has_component) {
      continue;
    }
    const auto& work = entity.builder_production;

    const bool holds_a_task = work.has_construction_site || work.has_task_target;
    const bool making_progress =
        work.in_progress || work.at_construction_site || work.carrying_load;
    if (!holds_a_task || making_progress) {
      continue;
    }

    WorkerWatch watch{.task_target_id = work.task_target_id,
                      .site_x = work.construction_site_x,
                      .site_z = work.construction_site_z,
                      .since = now};
    if (const auto previous = m_worker_watch.find(entity.id);
        previous != m_worker_watch.end() &&
        previous->second.task_target_id == watch.task_target_id &&
        std::abs(previous->second.site_x - watch.site_x) < 0.5F &&
        std::abs(previous->second.site_z - watch.site_z) < 0.5F) {
      watch.since = previous->second.since;
    }
    surviving.emplace(entity.id, watch);

    if (now - watch.since < k_stall_seconds) {
      continue;
    }

    if (std::getenv("SOI_TMP_BUILD") != nullptr) {
      std::fprintf(stderr, "STALL t=%.0f b%llu prod %d site %.1f,%.1f at %.1f,%.1f task %d moving %d path %d\n", now,
                   static_cast<unsigned long long>(entity.id), work.raising_a_building ? static_cast<int>(work.building_under_way) : -1, work.construction_site_x,
                   work.construction_site_z, entity.pos_x, entity.pos_z, work.has_task_target ? 1 : 0,
                   entity.movement.has_target ? 1 : 0, 0);
    }
    m_stalled_builders.insert(entity.id);
    if (watch.task_target_id != 0) {

      m_sour_nodes[watch.task_target_id] = now + k_sour_seconds;
    }
  }

  m_worker_watch = std::move(surviving);
}

auto node_is_sour(const SourNodes& sour, std::uint64_t node_id, float now) -> bool {
  const auto it = sour.find(node_id);
  return it != sour.end() && now < it->second;
}

} // namespace Game::Systems::AI
