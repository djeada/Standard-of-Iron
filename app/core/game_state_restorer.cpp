#include "game_state_restorer.h"

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/environment.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/game_state_serializer.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "game_engine.h"
#include "minimap_manager.h"
#include "render/gl/camera.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/bridge_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/river_renderer.h"
#include "render/ground/riverbank_renderer.h"
#include "render/ground/road_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_renderer.h"
#include "render/scene_renderer.h"
#include <QDebug>

void GameStateRestorer::rebuild_entity_cache(Engine::Core::World *world,
                                             EntityCache &entity_cache,
                                             int local_owner_id) {
  if (!world) {
    entity_cache.reset();
    return;
  }

  entity_cache.reset();

  auto &owners = Game::Systems::OwnerRegistry::instance();
  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id == local_owner_id) {
      if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
        entity_cache.player_barracks_alive = true;
      } else {
        int const production_cost =
            Game::Units::TroopConfig::instance().getProductionCost(
                unit->spawn_type);
        entity_cache.player_troop_count += production_cost;
      }
    } else if (owners.is_ai(unit->owner_id)) {
      if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
        entity_cache.enemy_barracks_count++;
        entity_cache.enemy_barracks_alive = true;
      }
    }
  }
}

void GameStateRestorer::rebuild_registries_after_load(
    Engine::Core::World *world, int &selected_player_id,
    Game::Systems::LevelSnapshot &level, int local_owner_id) {
  if (!world) {
    return;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();

  auto &troops = Game::Systems::TroopCountRegistry::instance();
  troops.rebuild_from_world(*world);

  auto &stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  stats_registry.rebuild_from_world(*world);

  const auto &all_owners = owner_registry.get_all_owners();
  for (const auto &owner : all_owners) {
    if (owner.type == Game::Systems::OwnerType::Player ||
        owner.type == Game::Systems::OwnerType::AI) {
      stats_registry.mark_game_start(owner.owner_id);
    }
  }

  rebuild_building_collisions(world);

  level.player_unit_id = 0;
  auto units = world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto *entity : units) {
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id == local_owner_id) {
      level.player_unit_id = entity->get_id();
      break;
    }
  }

  if (selected_player_id != local_owner_id) {
    selected_player_id = local_owner_id;
  }
}

void GameStateRestorer::rebuild_building_collisions(
    Engine::Core::World *world) {
  auto &registry = Game::Systems::BuildingCollisionRegistry::instance();
  registry.clear();
  if (!world) {
    return;
  }

  auto buildings = world->get_entities_with<Engine::Core::BuildingComponent>();
  for (auto *entity : buildings) {
    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((transform == nullptr) || (unit == nullptr)) {
      continue;
    }

    registry.register_building(
        entity->get_id(), Game::Units::spawn_typeToString(unit->spawn_type),
        transform->position.x, transform->position.z, unit->owner_id);
  }
}

void GameStateRestorer::restore_environment_from_metadata(
    const QJsonObject &metadata, Engine::Core::World *world,
    const RendererRefs &renderers, Game::Systems::LevelSnapshot &level,
    int local_owner_id, const ViewportState &viewport) {
  if (!world) {
    return;
  }

  const auto fallback_grid_width = metadata.value("grid_width").toInt(50);
  const auto fallback_grid_height = metadata.value("grid_height").toInt(50);
  const float fallback_tile_size =
      static_cast<float>(metadata.value("tile_size").toDouble(1.0));

  auto &terrain_service = Game::Map::TerrainService::instance();

  bool const terrain_already_restored = terrain_service.is_initialized();

  Game::Map::MapDefinition def;
  QString map_error;
  bool loaded_definition = false;
  const QString &map_path = level.map_path;

  if (!terrain_already_restored && !map_path.isEmpty()) {
    loaded_definition =
        Game::Map::MapLoader::loadFromJsonFile(map_path, def, &map_error);
    if (!loaded_definition) {
      qWarning() << "GameStateRestorer: Failed to load map definition from"
                 << map_path << "during save load:" << map_error;
    }
  }

  if (!terrain_already_restored && loaded_definition) {
    terrain_service.initialize(def);

    if (!def.name.isEmpty()) {
      level.map_name = def.name;
    }

    level.cam_fov = def.camera.fovY;
    level.cam_near = def.camera.near_plane;
    level.cam_far = def.camera.far_plane;
  }

  if (renderers.renderer && renderers.camera) {
    if (loaded_definition) {
      Game::Map::Environment::apply(def, *renderers.renderer,
                                    *renderers.camera);
    } else {
      Game::Map::Environment::applyDefault(*renderers.renderer,
                                           *renderers.camera);
    }
  }

  if (terrain_service.is_initialized()) {
    const auto *height_map = terrain_service.get_height_map();
    const int grid_width =
        (height_map != nullptr) ? height_map->getWidth() : fallback_grid_width;
    const int grid_height = (height_map != nullptr) ? height_map->getHeight()
                                                    : fallback_grid_height;
    const float tile_size = (height_map != nullptr) ? height_map->getTileSize()
                                                    : fallback_tile_size;

    if (renderers.ground) {
      renderers.ground->configure(tile_size, grid_width, grid_height);
      renderers.ground->set_biome(terrain_service.biome_settings());
    }

    if (height_map != nullptr) {
      if (renderers.terrain) {
        renderers.terrain->configure(*height_map,
                                     terrain_service.biome_settings());
      }
      if (renderers.river) {
        renderers.river->configure(height_map->getRiverSegments(),
                                   height_map->getTileSize());
      }
      if (renderers.road) {
        renderers.road->configure(terrain_service.road_segments(),
                                  height_map->getTileSize());
      }
      if (renderers.riverbank) {
        renderers.riverbank->configure(height_map->getRiverSegments(),
                                       *height_map);
      }
      if (renderers.bridge) {
        renderers.bridge->configure(height_map->getBridges(),
                                    height_map->getTileSize());
      }
      if (renderers.biome) {
        renderers.biome->configure(*height_map,
                                   terrain_service.biome_settings());
      }
      if (renderers.stone) {
        renderers.stone->configure(*height_map,
                                   terrain_service.biome_settings());
      }
      if (renderers.plant) {
        renderers.plant->configure(*height_map,
                                   terrain_service.biome_settings());
      }
      if (renderers.pine) {
        renderers.pine->configure(*height_map,
                                  terrain_service.biome_settings());
      }
      if (renderers.olive) {
        renderers.olive->configure(*height_map,
                                   terrain_service.biome_settings());
      }
      if (renderers.firecamp) {
        renderers.firecamp->configure(*height_map,
                                      terrain_service.biome_settings());
      }
    }

    Game::Systems::CommandService::initialize(grid_width, grid_height);

    auto &visibility_service = Game::Map::VisibilityService::instance();
    visibility_service.initialize(grid_width, grid_height, tile_size);
    visibility_service.computeImmediate(*world, local_owner_id);

    if (renderers.fog && visibility_service.is_initialized()) {
      renderers.fog->update_mask(
          visibility_service.getWidth(), visibility_service.getHeight(),
          visibility_service.getTileSize(), visibility_service.snapshotCells());
    }
  } else {
    Game::Map::MapDefinition fallback_def;
    fallback_def.grid.width = fallback_grid_width;
    fallback_def.grid.height = fallback_grid_height;
    fallback_def.grid.tile_size = fallback_tile_size;

    Game::Systems::CommandService::initialize(fallback_grid_width,
                                              fallback_grid_height);

    auto &visibility_service = Game::Map::VisibilityService::instance();
    visibility_service.initialize(fallback_grid_width, fallback_grid_height,
                                  fallback_tile_size);
    visibility_service.computeImmediate(*world, local_owner_id);

    if (renderers.fog && visibility_service.is_initialized()) {
      renderers.fog->update_mask(
          visibility_service.getWidth(), visibility_service.getHeight(),
          visibility_service.getTileSize(), visibility_service.snapshotCells());
    }
  }
}
