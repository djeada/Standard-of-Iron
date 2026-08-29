#pragma once

#include "../core/system.h"
#include "body_contact_system.h"
#include "local_avoidance_system.h"
#include "movement_system.h"
#include "route_follow_system.h"
#include "unit_traversal_layout_system.h"

namespace Game::Systems {

class MovementPipeline : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  [[nodiscard]] auto motor() -> MovementSystem& { return m_motor; }

private:
  RouteFollowSystem m_route_follow;
  LocalAvoidanceSystem m_avoidance;
  MovementSystem m_motor;
  BodyContactSystem m_contact;
  UnitTraversalLayoutSystem m_traversal_layout;
};

} // namespace Game::Systems
