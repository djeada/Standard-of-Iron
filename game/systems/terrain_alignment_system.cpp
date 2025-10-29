#include "terrain_alignment_system.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "../units/troop_config.h"

namespace Game::Systems {

void TerrainAlignmentSystem::update(Engine::Core::World *world, float) {
  auto &terrain_service = Game::Map::TerrainService::instance();

  if (!terrain_service.isInitialized()) {
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
  if (transform == nullptr) {
    return;
  }

  auto &terrain_service = Game::Map::TerrainService::instance();

  float const terrain_height = terrain_service.getTerrainHeight(
      transform->position.x, transform->position.z);

  float entity_base_offset = 0.0F;
  if (auto *unit = entity->getComponent<Engine::Core::UnitComponent>()) {
    entity_base_offset =
        Game::Units::TroopConfig::instance().getSelectionRingGroundOffset(
            unit->spawn_type);
  }

  transform->position.y =
      terrain_height + entity_base_offset * transform->scale.y;
}

} // namespace Game::Systems
