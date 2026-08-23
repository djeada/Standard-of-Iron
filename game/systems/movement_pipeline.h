#pragma once

#include "../core/system.h"
#include "local_avoidance_system.h"
#include "movement_system.h"
#include "route_follow_system.h"

namespace Game::Systems {

// The Movement phase in its declared order: route following, then steering,
// then the motor.
//
// The runtime registry adds the three stages separately so the profiler and the
// access checker see them individually. Everything that drives movement outside
// the registry -- tools and tests -- uses this composite instead, so no caller
// can invent a different order and get a plausible-looking wrong answer.
class MovementPipeline : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  [[nodiscard]] auto motor() -> MovementSystem& { return m_motor; }

private:
  RouteFollowSystem m_route_follow;
  LocalAvoidanceSystem m_avoidance;
  MovementSystem m_motor;
};

} // namespace Game::Systems
