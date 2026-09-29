#include "builder_pool.h"

#include <algorithm>
#include <limits>

#include "units/spawn_type.h"

namespace Game::Systems::AI {

namespace {

auto select_best_builder(const AISnapshot& snapshot,
                         const std::vector<Engine::Core::EntityID>& available_builders,
                         float target_x,
                         float target_z) -> Engine::Core::EntityID {
  Engine::Core::EntityID best_id = available_builders.front();
  float best_distance_sq = std::numeric_limits<float>::infinity();

  for (auto builder_id : available_builders) {
    auto it = std::find_if(
        snapshot.friendly_units.begin(),
        snapshot.friendly_units.end(),
        [builder_id](const EntitySnapshot& entity) { return entity.id == builder_id; });
    if (it == snapshot.friendly_units.end()) {
      continue;
    }

    const float distance_sq = (it->pos_x - target_x) * (it->pos_x - target_x) +
                              (it->pos_z - target_z) * (it->pos_z - target_z);
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best_id = builder_id;
    }
  }

  return best_id;
}

auto select_strongest_builder(
    const AISnapshot& snapshot,
    const std::vector<Engine::Core::EntityID>& available_builders,
    float target_x,
    float target_z) -> Engine::Core::EntityID {
  Engine::Core::EntityID best_id = available_builders.front();
  int best_strength = -1;
  float best_distance_sq = std::numeric_limits<float>::infinity();

  for (const auto builder_id : available_builders) {
    const auto it = std::find_if(
        snapshot.friendly_units.begin(),
        snapshot.friendly_units.end(),
        [builder_id](const EntitySnapshot& entity) { return entity.id == builder_id; });
    if (it == snapshot.friendly_units.end()) {
      continue;
    }
    const float distance_sq = (it->pos_x - target_x) * (it->pos_x - target_x) +
                              (it->pos_z - target_z) * (it->pos_z - target_z);
    if (it->squad_strength > best_strength ||
        (it->squad_strength == best_strength && distance_sq < best_distance_sq)) {
      best_strength = it->squad_strength;
      best_distance_sq = distance_sq;
      best_id = builder_id;
    }
  }

  return best_id;
}

} // namespace

auto BuilderPool::gather(const AISnapshot& snapshot,
                         const WorkerStallWatch& stalls) -> BuilderPool {
  BuilderPool pool;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type != Game::Units::SpawnType::Builder) {
      continue;
    }

    const bool stalled = stalls.is_stalled(entity.id);
    if (!stalled && entity.builder_production.has_component &&
        (entity.builder_production.has_construction_site ||
         entity.builder_production.has_task_target ||
         entity.builder_production.carrying_load)) {
      continue;
    }

    if (stalled) {
      pool.m_available.push_back(entity.id);
      continue;
    }

    if (entity.movement.has_component && !entity.movement.has_target) {
      pool.m_available.push_back(entity.id);
    }
  }
  return pool;
}

auto BuilderPool::take_nearest(const AISnapshot& snapshot,
                               float x,
                               float z) -> Engine::Core::EntityID {
  if (m_available.empty()) {
    return 0;
  }
  const auto id = select_best_builder(snapshot, m_available, x, z);
  std::erase(m_available, id);
  return id;
}

auto BuilderPool::take_strongest(const AISnapshot& snapshot,
                                 float x,
                                 float z) -> Engine::Core::EntityID {
  if (m_available.empty()) {
    return 0;
  }
  const auto id = select_strongest_builder(snapshot, m_available, x, z);
  std::erase(m_available, id);
  return id;
}

auto BuilderPool::take_last() -> Engine::Core::EntityID {
  const auto id = m_available.back();
  m_available.pop_back();
  return id;
}

} // namespace Game::Systems::AI
