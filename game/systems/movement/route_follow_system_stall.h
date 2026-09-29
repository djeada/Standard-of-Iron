#pragma once

#include <QVector3D>

#include "route_follow_system_frame.h"

namespace Game::Systems::ObjectiveStall {

void abandon(Engine::Core::Entity& entity,
             Engine::Core::MovementComponent& movement,
             Engine::Core::MovementFactsComponent& facts,
             const QVector3D& objective);

[[nodiscard]] auto track(FollowFrame& frame, float max_speed) -> bool;

} // namespace Game::Systems::ObjectiveStall
