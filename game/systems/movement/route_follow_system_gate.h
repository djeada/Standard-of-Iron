#pragma once

#include <QVector3D>

#include <cstdint>

#include "core/component_gameplay.h"
#include "core/world.h"

namespace Game::Systems {

enum class MovementGate : std::uint8_t {
  RouteFollowing = 0,
  Dead,
  DirectControl,
  HoldMode,
  MeleeLock,
  BuilderBypass,
  OnWall,
  OnRaft
};

[[nodiscard]] auto
classify_movement_gate(const Engine::Core::Entity& entity) -> MovementGate;

[[nodiscard]] auto
is_movement_point_allowed(const QVector3D& pos,
                          const Engine::Core::Entity& entity) -> bool;

[[nodiscard]] auto bypass_line_is_clear(const Engine::Core::Entity& entity,
                                        const QVector3D& from,
                                        const QVector3D& to) -> bool;

[[nodiscard]] auto
max_navigation_speed(const Engine::Core::UnitComponent& unit,
                     const Engine::Core::StaminaComponent* stamina) -> float;

[[nodiscard]] auto
formation_navigation_speed(const Engine::Core::Entity& entity,
                           const Engine::Core::UnitComponent& unit,
                           const Engine::Core::StaminaComponent* stamina) -> float;

} // namespace Game::Systems
