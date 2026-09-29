#pragma once

#include "movement_route.h"
#include "route_follow_system_frame.h"
#include "route_follow_system_steering.h"

namespace Game::Systems::RouteArrival {

[[nodiscard]] auto resolve(FollowFrame& frame,
                           MovementRoute& route,
                           const RouteFollowSystem::Steering::RouteAim& aim,
                           float arrive_radius,
                           bool current_position_allowed) -> bool;

} // namespace Game::Systems::RouteArrival
