#include "terrain_alignment_system.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "../units/troop_config.h"

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

  float entityBaseOffset = 0.0f;
  if (auto *unit = entity->getComponent<Engine::Core::UnitComponent>()) {
    if (!unit->unitType.empty()) {
      entityBaseOffset = Game::Units::TroopConfig::instance()
                              .getSelectionRingGroundOffset(unit->unitType);
    }
  }

  transform->position.y = terrainHeight + entityBaseOffset * transform->scale.y;
}

} // namespace Game::Systems
