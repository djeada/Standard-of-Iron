#pragma once

#include <QVector3D>

#include "core/component_combat.h"
#include "core/world.h"
#include "movement_route.h"

namespace Game::Systems::RouteProgress {

[[nodiscard]] auto
order_goal_of(const Engine::Core::MovementComponent& movement) -> QVector3D;

void settle_arrival(Engine::Core::Entity& entity,
                    Engine::Core::MovementComponent& movement,
                    Engine::Core::MovementFactsComponent& facts,
                    MovementRoute& route,
                    bool short_of_the_order);

auto update_progress(Engine::Core::MovementComponent& movement,
                     Engine::Core::MovementFactsComponent& facts,
                     float remaining,
                     bool route_changed,
                     float delta_time) -> bool;

} // namespace Game::Systems::RouteProgress
