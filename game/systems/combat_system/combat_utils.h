#pragma once

#include "../../core/entity.h"
#include <vector>

namespace Engine::Core {
class World;
class AttackComponent;
} // namespace Engine::Core

namespace Game::Systems::Combat {

auto is_unit_in_hold_mode(Engine::Core::Entity *entity) -> bool;

auto is_unit_in_guard_mode(Engine::Core::Entity *entity) -> bool;

auto is_in_range(Engine::Core::Entity *attacker, Engine::Core::Entity *target,
                 float range) -> bool;

auto is_unit_idle(Engine::Core::Entity *unit) -> bool;

auto find_nearest_enemy(Engine::Core::Entity *unit, Engine::Core::World *world,
                        float max_range) -> Engine::Core::Entity *;

auto find_nearest_enemy_from_list(
    Engine::Core::Entity *unit,
    const std::vector<Engine::Core::Entity *> &all_units,
    Engine::Core::World *world, float max_range) -> Engine::Core::Entity *;

auto should_auto_engage_melee(Engine::Core::Entity *unit) -> bool;

// Check if a unit at blocker position is blocking the path from attacker to
// target Returns true if the blocker is between attacker and target and close
// to the path
auto is_blocking_path(float attacker_x, float attacker_z, float target_x,
                      float target_z, float blocker_x,
                      float blocker_z) -> bool;

} // namespace Game::Systems::Combat
