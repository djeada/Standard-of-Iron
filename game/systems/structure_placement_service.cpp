#include "structure_placement_service.h"

#include <algorithm>

#include "../core/ambient_session.h"
#include "../core/world.h"
#include "../map/map_transformer.h"
#include "../units/factory.h"
#include "../units/spawn_type.h"
#include "../units/unit.h"
#include "building_collision_registry.h"
#include "construction_cost_catalog.h"
#include "nation_registry.h"
#include "nav_grid.h"
#include "player_resource_registry.h"
#include "wall_network_service.h"

namespace Game::Systems {

auto StructurePlacementService::footprint_is_clear(const Engine::Core::World& world,
                                                   float x,
                                                   float z,
                                                   const std::string& building_type)
    -> bool {
  const auto size = BuildingCollisionRegistry::get_building_size(building_type);
  if (Game::Session::services_for(world)
          .building_collision->is_circle_overlapping_building(
              x, z, std::max(size.width, size.depth) * 0.5F, 0)) {
    return false;
  }
  return NavGrid::is_grid_walkable(NavGrid::world_to_grid(x, z));
}

auto StructurePlacementService::ruling(Engine::Core::World& world,
                                       int owner_id,
                                       const std::string& building_type,
                                       const QVector3D& position) -> PlacementRuling {
  const auto spawn_type = Game::Units::spawn_typeFromString(building_type);
  if (!spawn_type.has_value() || !Game::Units::is_building_spawn(*spawn_type)) {
    return PlacementRuling::UnknownStructure;
  }
  if (!footprint_is_clear(world, position.x(), position.z(), building_type)) {
    return PlacementRuling::Blocked;
  }
  const auto costs = construction_cost_info(building_type).resource_costs;
  if (!costs.empty() &&
      !Game::Session::services_for(world).economy->has_at_least(owner_id, costs)) {
    return PlacementRuling::Unaffordable;
  }
  if (Game::Map::MapTransformer::get_factory_registry() == nullptr) {
    return PlacementRuling::NoFactory;
  }
  return PlacementRuling::Ok;
}

auto StructurePlacementService::place(Engine::Core::World& world,
                                      int owner_id,
                                      const std::string& building_type,
                                      const QVector3D& position,
                                      float rotation_y) -> Engine::Core::EntityID {
  if (ruling(world, owner_id, building_type, position) != PlacementRuling::Ok) {
    return Engine::Core::NULL_ENTITY;
  }
  auto registry = Game::Map::MapTransformer::get_factory_registry();
  const auto spawn_type = Game::Units::spawn_typeFromString(building_type);

  auto& nations = *Game::Session::services_for(world).nations;
  const auto* nation = nations.get_nation_for_player(owner_id);

  Game::Units::SpawnParams params;
  params.position = position;
  params.rotation_y = rotation_y;
  params.player_id = owner_id;
  params.ai_controlled = false;
  params.nation_id = nation != nullptr ? nation->id : nations.default_nation_id();
  params.is_initial_spawn = false;
  params.spawn_type = *spawn_type;

  auto unit = registry->create(params.spawn_type, world, params);
  if (!unit) {
    return Engine::Core::NULL_ENTITY;
  }
  Game::Session::services_for(world).economy->spend(
      owner_id, construction_cost_info(building_type).resource_costs);
  if (params.spawn_type == Game::Units::SpawnType::WallSegment) {
    WallNetworkService::refresh_world(world);
  }
  return unit->id();
}

} // namespace Game::Systems
