#pragma once

#include <QVector3D>

#include <cstdint>
#include <unordered_map>

#include "../core/component_gameplay.h"
#include "../core/system.h"
#include "../core/world.h"
#include "movement_route.h"

namespace Game::Systems {

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

private:
  void
  follow(Engine::Core::Entity& entity, Engine::Core::World& world, float delta_time);

  auto update_progress(Engine::Core::Entity& entity,
                       Engine::Core::World& world,
                       Engine::Core::TransformComponent& transform,
                       Engine::Core::MovementComponent& movement,
                       Engine::Core::MovementFactsComponent& facts,
                       float remaining,
                       bool route_changed,
                       float delta_time) -> bool;

  std::unordered_map<Engine::Core::EntityID, MovementRoute> m_routes;
  std::uint64_t m_prune_tick{0};
};

} // namespace Game::Systems
