#pragma once

#include "../../core/entity.h"
#include "../spatial_grid.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Engine::Core {
class World;
class AttackComponent;
class UnitComponent;
} // namespace Engine::Core

namespace Game::Systems::Combat {

struct CombatQueryContext {
  CombatQueryContext();

  std::vector<Engine::Core::Entity *> units;
  std::unordered_map<Engine::Core::EntityID, Engine::Core::Entity *>
      entities_by_id;
  Game::Systems::SpatialGrid unit_grid;
};

auto build_combat_query_context(Engine::Core::World *world)
    -> CombatQueryContext;

auto is_unit_in_hold_mode(Engine::Core::Entity *entity) -> bool;

auto is_unit_in_guard_mode(Engine::Core::Entity *entity) -> bool;

auto is_building(Engine::Core::Entity *entity) -> bool;

auto is_valid_enemy_unit(const Engine::Core::UnitComponent *attacker_unit,
                         Engine::Core::Entity *target,
                         bool allow_buildings) -> bool;

auto is_in_range(Engine::Core::Entity *attacker, Engine::Core::Entity *target,
                 float range) -> bool;

auto is_unit_idle(Engine::Core::Entity *unit) -> bool;

auto find_nearest_enemy(
    Engine::Core::Entity *unit, const CombatQueryContext &query_context,
    float max_range,
    std::uint64_t *scan_iterations = nullptr) -> Engine::Core::Entity *;

auto should_auto_engage_melee(Engine::Core::Entity *unit) -> bool;

} // namespace Game::Systems::Combat
