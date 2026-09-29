#pragma once

#include "movement_route.h"
#include "route_follow_system.h"
#include "route_follow_system_frame.h"

namespace Game::Systems {

class RouteFollowSystem::Steering {
public:
  struct RouteAim {
    float aim_x{0.0F};
    float aim_z{0.0F};
    float endpoint_x{0.0F};
    float endpoint_z{0.0F};
    float remaining{0.0F};
    float tangent_x{0.0F};
    float tangent_z{0.0F};
  };

  static void publish_route_facts(FollowFrame& frame);

  static void
  publish_direct_control_intent(const Engine::Core::Entity& entity,
                                const Engine::Core::TransformComponent& transform,
                                Engine::Core::MovementFactsComponent& facts);

  [[nodiscard]] static auto arrive_radius_for(const FollowFrame& frame,
                                              float max_speed) -> float;

  [[nodiscard]] static auto aim_along_route(FollowFrame& frame,
                                            MovementRoute& route,
                                            float max_speed) -> RouteAim;

  static void publish_desired_motion(FollowFrame& frame,
                                     RouteAim aim,
                                     float arrive_radius,
                                     float max_speed);

private:
  static void aim_on_route(FollowFrame& frame,
                           MovementRoute& route,
                           float max_speed,
                           RouteAim& result);
};

} // namespace Game::Systems
