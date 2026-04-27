#include "terrain_alignment_system.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../map/terrain_service.h"

#include <QVector3D>

namespace Game::Systems {

void TerrainAlignmentSystem::update(Engine::Core::World *world, float) {
  auto &terrain_service = Game::Map::TerrainService::instance();

  if (!terrain_service.is_initialized()) {
    return;
  }

  auto entities = world->get_entities_with<Engine::Core::TransformComponent>();
  for (auto *entity : entities) {
    align_entity_to_terrain(entity);
  }
}

void TerrainAlignmentSystem::align_entity_to_terrain(
    Engine::Core::Entity *entity) {
  auto *transform = entity->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  auto &terrain_service = Game::Map::TerrainService::instance();

  QVector3D const aligned = terrain_service.resolve_surface_world_position(
      transform->position.x, transform->position.z, 0.0F,
      transform->position.y);
  transform->position.x = aligned.x();
  transform->position.y = aligned.y();
  transform->position.z = aligned.z();
}

} // namespace Game::Systems
