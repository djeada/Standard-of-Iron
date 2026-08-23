#include "terrain_alignment.h"

#include <QVector3D>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/nav_grid.h"
#include "game/units/spawn_type.h"

namespace Arena {

auto entity_keeps_planar_position(Engine::Core::World& world,
                                  Engine::Core::EntityID entity_id) -> bool {
  if (world.has<Engine::Core::BuildingComponent>(entity_id) ||
      world.has<Engine::Core::WallConstructionSiteComponent>(entity_id)) {
    return true;
  }

  const auto* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
  return unit != nullptr && Game::Units::is_building_spawn(unit->spawn_type);
}

void align_entity_to_ground(Engine::Core::World& world,
                            Engine::Core::EntityID entity_id) {
  auto* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
  if (transform == nullptr) {
    return;
  }

  if (entity_keeps_planar_position(world, entity_id)) {
    transform->position.y =
        Game::Map::TerrainService::instance().resolve_surface_world_y(
            transform->position.x, transform->position.z, 0.0F, transform->position.y);
    return;
  }

  const QVector3D snapped = Game::Systems::NavGrid::snap_to_walkable_ground(
      {transform->position.x, transform->position.y, transform->position.z});
  transform->position.x = snapped.x();
  transform->position.y = snapped.y();
  transform->position.z = snapped.z();
}

} // namespace Arena
