#pragma once

#include <QVector3D>

#include <optional>
#include <vector>

#include "../movement/command_service.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Systems::Combat {

// How far ahead of a battle line's front an elephant starts looking for a lane.
inline constexpr float k_lane_lookahead = 35.0F;
// After it leaves a lane at the rear, the beast keeps running this long.
inline constexpr float k_lane_run_out_seconds = 8.0F;
inline constexpr float k_lane_run_out_distance = 30.0F;

// When a charging elephant's target stands in a battle order with open lanes
// (a triplex acies), the point it should run for instead of the target: first
// the mouth of the nearest clear lane, then through it and out past the rear
// line. Empty when there is no clear lane, or the elephant is not charging one.
[[nodiscard]] auto elephant_lane_goal(Engine::Core::World& world,
                                      Engine::Core::Entity& elephant,
                                      const Engine::Core::Entity* target)
    -> std::optional<QVector3D>;

void steer_down_lane(Engine::Core::Entity& elephant,
                     const QVector3D& goal,
                     std::vector<CommandService::MoveIntent>& intents);

} // namespace Game::Systems::Combat
