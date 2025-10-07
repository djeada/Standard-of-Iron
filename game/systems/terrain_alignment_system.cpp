#include "terrain_alignment_system.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../map/terrain_service.h"

namespace Game::Systems {

void TerrainAlignmentSystem::update(Engine::Core::World *world,
                                    float deltaTime) {
  auto &terrainService = Game::Map::TerrainService::instance();

  if (!terrainService.isInitialized()) {
    return;
  }

  auto entities = world->getEntitiesWith<Engine::Core::TransformComponent>();
  for (auto *entity : entities) {
    alignEntityToTerrain(entity);
  }
}

void TerrainAlignmentSystem::alignEntityToTerrain(
    Engine::Core::Entity *entity) {
  auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
  if (!transform)
    return;

  auto &terrainService = Game::Map::TerrainService::instance();

  float terrainHeight = terrainService.getTerrainHeight(transform->position.x,
                                                        transform->position.z);

  const float entityBaseOffset = 0.0f;
  transform->position.y = terrainHeight + entityBaseOffset;
}

} // namespace Game::Systems
