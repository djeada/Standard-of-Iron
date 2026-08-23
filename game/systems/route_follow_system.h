#pragma once

#include <QVector3D>

#include <cstdint>

#include "../core/component.h"
#include "../core/system.h"
#include "../core/world.h"

namespace Game::Systems {

// What owns a troop entity's body this tick. Decided once, in one place, so the
// route follower and the motor can never disagree about who is driving.
enum class MovementGate : std::uint8_t {
  RouteFollowing = 0,
  Dead,
  DirectControl,
  HoldMode,
  MeleeLock,
  BuilderBypass
};

[[nodiscard]] auto
classify_movement_gate(const Engine::Core::Entity& entity) -> MovementGate;

// Shared passability and speed rules. The route follower and the motor must ask
// the same question of the same source, so both call these.
[[nodiscard]] auto
is_movement_point_allowed(const QVector3D& pos,
                          const Engine::Core::Entity& entity) -> bool;
[[nodiscard]] auto
max_navigation_speed(const Engine::Core::UnitComponent& unit,
                     const Engine::Core::StaminaComponent* stamina) -> float;
[[nodiscard]] auto
formation_navigation_speed(const Engine::Core::Entity& entity,
                           const Engine::Core::UnitComponent& unit,
                           const Engine::Core::StaminaComponent* stamina) -> float;

// Stage one of the Movement phase: turn the accepted order into an immutable
// desired velocity and a stable route tangent.
//
// It owns RouteIntentFacts and DesiredMotionFacts and nothing else. It never
// writes the transform and never writes an integrated velocity, so the steering
// stage below it always reads an intent rather than a partly-integrated motion.
class RouteFollowSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  // Remaining arclength along the assigned route from `position`.
  [[nodiscard]] static auto
  remaining_route_length(const Engine::Core::MovementComponent& movement,
                         float position_x,
                         float position_z) -> float;

private:
  static void
  follow(Engine::Core::Entity& entity, Engine::Core::World& world, float delta_time);
};

} // namespace Game::Systems
