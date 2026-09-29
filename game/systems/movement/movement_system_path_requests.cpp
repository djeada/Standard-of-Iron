#include "movement_system_path_requests.h"

#include <utility>

#include "core/component_economy.h"
#include "game/core/nav_profile.h"

namespace Game::Systems {

void PathRequestQueue::process(Engine::Core::World& world, ApplyRoute apply) {
  std::size_t processed = 0;
  while (processed < k_requests_per_tick && !m_requests.empty()) {
    PendingPathRequest request = std::move(m_requests.front());
    m_requests.pop_front();

    const auto generation = m_generations.find(request.entity_id);
    if (generation == m_generations.end() || generation->second != request.generation) {
      continue;
    }
    m_generations.erase(generation);

    auto* entity = world.get_entity(request.entity_id);
    if (entity == nullptr) {
      ++processed;
      continue;
    }
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (transform != nullptr && movement != nullptr) {

      if (movement->get_order_sequence() != request.order_sequence) {
        ++processed;
        continue;
      }
      float const current_goal_x = movement->get_has_requested_goal()
                                       ? movement->get_requested_goal_x()
                                       : movement->get_goal_x();
      float const current_goal_z = movement->get_has_requested_goal()
                                       ? movement->get_requested_goal_z()
                                       : movement->get_goal_y();
      float const goal_dx = current_goal_x - request.target.x();
      float const goal_dz = current_goal_z - request.target.z();
      if (goal_dx * goal_dx + goal_dz * goal_dz > 0.01F) {
        ++processed;
        continue;
      }
      apply(*transform, *movement, request.target, request.precise_arrival);
    }
    ++processed;
  }
}

auto PathRequestQueue::enqueue(Engine::Core::EntityID entity_id,
                               const QVector3D& target,
                               bool precise_arrival,
                               std::uint64_t navigation_revision,
                               std::uint64_t order_sequence) -> bool {

  const std::uint64_t generation = ++m_generations[entity_id];

  if (m_requests.size() >= k_max_pending) {

    const auto evicted = m_requests.front();
    m_requests.pop_front();
    const auto current = m_generations.find(evicted.entity_id);
    if (current != m_generations.end() && current->second == evicted.generation) {
      m_generations.erase(current);
      Engine::Core::count_nav(Engine::Core::NavCounter::RequestsDropped);
    }
  }

  Engine::Core::count_nav(Engine::Core::NavCounter::RequestsQueued);
  m_requests.push_back({entity_id,
                        target,
                        navigation_revision,
                        order_sequence,
                        generation,
                        precise_arrival});
  return true;
}

void PathRequestQueue::cancel(Engine::Core::EntityID entity_id) {
  m_generations.erase(entity_id);
}

} // namespace Game::Systems
