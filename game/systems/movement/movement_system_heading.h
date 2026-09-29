#pragma once

#include "core/component_combat.h"
#include "core/world.h"

namespace Game::Systems::MovementHeading {

inline constexpr float k_hold_mode_turn_speed_degrees = 180.0F;

[[nodiscard]] auto formation_turn_speed_degrees(const Engine::Core::Entity& entity,
                                                const Engine::Core::UnitComponent& unit,
                                                float single_body_turn_speed) -> float;

void apply_desired_yaw(Engine::Core::TransformComponent* transform,
                       float delta_time,
                       float turn_speed_degrees);

[[nodiscard]] auto face_locked_opponent(Engine::Core::World* world,
                                        const Engine::Core::Entity& entity,
                                        Engine::Core::TransformComponent& transform,
                                        const Engine::Core::AttackComponent& attack,
                                        const Engine::Core::UnitComponent* unit,
                                        float delta_time) -> bool;

void clamp_to_map_bounds(Engine::Core::TransformComponent& transform);

[[nodiscard]] auto
finalize_orientation(Engine::Core::World& world,
                     Engine::Core::Entity* entity,
                     Engine::Core::TransformComponent* transform,
                     Engine::Core::MovementComponent* movement,
                     const Engine::Core::MovementFactsComponent* facts,
                     float delta_time) -> bool;

} // namespace Game::Systems::MovementHeading
