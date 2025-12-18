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

/**
 * @brief Find the nearest enemy to a unit using spatial partitioning.
 *
 * This version uses a spatial grid for O(k) lookup where k is the number
 * of nearby entities, instead of O(n) iteration over all entities.
 *
 * @param unit The unit searching for enemies
 * @param world The game world
 * @param max_range Maximum search range
 * @return Pointer to nearest enemy, or nullptr if none found
 */
auto find_nearest_enemy(Engine::Core::Entity *unit, Engine::Core::World *world,
                        float max_range) -> Engine::Core::Entity *;

/**
 * @brief Find the nearest enemy from a pre-queried list of units.
 *
 * This is more efficient when the caller already has a list of units,
 * avoiding a redundant get_entities_with call.
 *
 * @param unit The unit searching for enemies
 * @param all_units Pre-queried list of all units
 * @param world The game world (for entity lookups)
 * @param max_range Maximum search range
 * @return Pointer to nearest enemy, or nullptr if none found
 */
auto find_nearest_enemy_from_list(
    Engine::Core::Entity *unit,
    const std::vector<Engine::Core::Entity *> &all_units,
    Engine::Core::World *world, float max_range) -> Engine::Core::Entity *;

auto should_auto_engage_melee(Engine::Core::Entity *unit) -> bool;

} // namespace Game::Systems::Combat
