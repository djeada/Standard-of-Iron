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

inline constexpr float k_lane_lookahead = 35.0F;

inline constexpr float k_lane_run_out_seconds = 8.0F;
inline constexpr float k_lane_run_out_distance = 30.0F;

[[nodiscard]] auto
elephant_lane_goal(Engine::Core::World& world,
                   Engine::Core::Entity& elephant,
                   const Engine::Core::Entity* target) -> std::optional<QVector3D>;

void steer_down_lane(Engine::Core::Entity& elephant,
                     const QVector3D& goal,
                     std::vector<CommandService::MoveIntent>& intents);

} // namespace Game::Systems::Combat
