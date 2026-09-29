#pragma once

#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>

#include "core/component_combat.h"
#include "core/world.h"

namespace Game::Systems {

class PathRequestQueue {
public:
  static constexpr std::size_t k_requests_per_tick = 8U;
  static constexpr std::size_t k_max_pending = 2048U;

  auto enqueue(Engine::Core::EntityID entity_id,
               const QVector3D& target,
               bool precise_arrival,
               std::uint64_t navigation_revision,
               std::uint64_t order_sequence) -> bool;

  void cancel(Engine::Core::EntityID entity_id);

  using ApplyRoute = void (*)(Engine::Core::TransformComponent& transform,
                              Engine::Core::MovementComponent& movement,
                              const QVector3D& target,
                              bool precise_arrival);
  void process(Engine::Core::World& world, ApplyRoute apply);

private:
  struct PendingPathRequest {
    Engine::Core::EntityID entity_id{0};
    QVector3D target;
    std::uint64_t navigation_revision{0};
    std::uint64_t order_sequence{0};

    std::uint64_t generation{0};
    bool precise_arrival{false};
  };

  std::deque<PendingPathRequest> m_requests;
  std::unordered_map<Engine::Core::EntityID, std::uint64_t> m_generations;
};

} // namespace Game::Systems
