#include "terrain_alignment.h"

#include <QVector3D>

#include "game/core/component_gameplay.h"
#include "game/core/entity.h"
#include "game/map/terrain_service.h"
#include "game/systems/nav_grid.h"
#include "game/units/spawn_type.h"

namespace Arena {

auto entity_keeps_planar_position(const Engine::Core::Entity& entity) -> bool {
  if (entity.has_component<Engine::Core::BuildingComponent>() ||
      entity.has_component<Engine::Core::WallConstructionSiteComponent>()) {
    return true;
  }

  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && Game::Units::is_building_spawn(unit->spawn_type);
}

void align_entity_to_ground(Engine::Core::Entity& entity,
                            const Game::Map::TerrainService& terrain) {
  auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  if (entity_keeps_planar_position(entity)) {
    transform->position.y = terrain.resolve_surface_world_y(
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
