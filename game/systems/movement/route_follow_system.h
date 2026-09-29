#pragma once

#include <QVector3D>

#include <cstdint>
#include <unordered_map>

#include "core/component_gameplay.h"
#include "core/system.h"
#include "core/world.h"
#include "movement_route.h"
#include "route_follow_system_frame.h"
#include "route_follow_system_gate.h"

namespace Game::Systems {

class RouteFollowSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  [[nodiscard]] static auto
  remaining_route_length(const Engine::Core::MovementComponent& movement,
                         float position_x,
                         float position_z) -> float;

  [[nodiscard]] auto
  route_for(Engine::Core::EntityID entity_id) const -> const MovementRoute*;

  class Steering;

private:
  enum class GoalCheck : std::uint8_t {
    Fine,
    Handled,
    Abandoned,
  };

  void
  follow(Engine::Core::Entity& entity, Engine::Core::World& world, float delta_time);

  void follow_route(FollowFrame& frame);

  void refresh_clearance(FollowFrame& frame);

  [[nodiscard]] auto check_goal(FollowFrame& frame,
                                bool current_position_allowed) -> GoalCheck;

  auto sync_route(FollowFrame& frame, MovementRoute& route) -> bool;

  std::unordered_map<Engine::Core::EntityID, MovementRoute> m_routes;
  std::uint64_t m_prune_tick{0};
};

} // namespace Game::Systems
