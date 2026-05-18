#include "game_state_restorer.h"

#include <QDebug>

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
#include "minimap_manager.h"
#include "render/gl/camera.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/map_boundary_fog_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/scene_renderer.h"
#include "visibility_coordinator.h"

void GameStateRestorer::rebuild_entity_cache(Engine::Core::World* world,
                                             EntityCache& entity_cache,
                                             int local_owner_id) {
  if (!world) {
    entity_cache.reset();
    return;
  }

  entity_cache.reset();

  auto& owners = Game::Systems::OwnerRegistry::instance();
  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto* e : entities) {
    auto* unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id == local_owner_id) {
      if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
        entity_cache.player_barracks_alive = true;
      } else {
        int const production_cost =
            Game::Units::TroopConfig::instance().get_production_cost(unit->spawn_type);
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
    Engine::Core::World* world,
    int& selected_player_id,
    Game::Systems::LevelSnapshot& level,
    int local_owner_id) {
  if (!world) {
    return;
  }

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();

  auto& troops = Game::Systems::TroopCountRegistry::instance();
  troops.rebuild_from_world(*world);

  auto& stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  stats_registry.rebuild_from_world(*world);

  const auto& all_owners = owner_registry.get_all_owners();
  for (const auto& owner : all_owners) {
    if (owner.type == Game::Systems::OwnerType::Player ||
        owner.type == Game::Systems::OwnerType::AI) {
      stats_registry.mark_game_start(owner.owner_id);
    }
  }

  rebuild_building_collisions(world);

  level.player_unit_id = 0;
  auto units = world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto* entity : units) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
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

void GameStateRestorer::rebuild_building_collisions(Engine::Core::World* world) {
  auto& registry = Game::Systems::BuildingCollisionRegistry::instance();
  registry.clear();
  if (!world) {
    return;
  }

  auto buildings = world->get_entities_with<Engine::Core::BuildingComponent>();
  for (auto* entity : buildings) {
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((transform == nullptr) || (unit == nullptr)) {
      continue;
    }

    registry.register_building(entity->get_id(),
                               Game::Units::spawn_typeToString(unit->spawn_type),
                               transform->position.x,
                               transform->position.z,
                               unit->owner_id);
  }
}

void GameStateRestorer::restore_environment_from_metadata(
    const QJsonObject& metadata,
    const AppSceneContext& scene,
    Game::Systems::LevelSnapshot& level,
    int local_owner_id,
    MinimapManager* minimap_manager,
    VisibilityCoordinator* visibility_coordinator) {
  auto* world = scene.world;
  if (!world) {
    return;
  }

  const auto fallback_grid_width = metadata.value("grid_width").toInt(50);
  const auto fallback_grid_height = metadata.value("grid_height").toInt(50);
  const float fallback_tile_size =
      static_cast<float>(metadata.value("tile_size").toDouble(1.0));

  auto& terrain_service = Game::Map::TerrainService::instance();

  bool const terrain_already_restored = terrain_service.is_initialized();

  Game::Map::MapDefinition def;
  QString map_error;
  bool loaded_definition = false;
  const QString& map_path = level.map_path;

  if (!terrain_already_restored && !map_path.isEmpty()) {
    loaded_definition =
        Game::Map::MapLoader::load_from_json_file(map_path, def, &map_error);
    if (!loaded_definition) {
      qWarning() << "GameStateRestorer: Failed to load map definition from" << map_path
                 << "during save load:" << map_error;
    }
  }

  if (!terrain_already_restored && loaded_definition) {
    terrain_service.initialize(def);

    if (!def.name.isEmpty()) {
      level.map_name = def.name;
    }

    level.cam_fov = def.camera.fov_y;
    level.cam_near = def.camera.near_plane;
    level.cam_far = def.camera.far_plane;
  }

  if (scene.renderer && scene.active_camera) {
    if (loaded_definition) {
      Game::Map::Environment::apply(def, *scene.renderer, *scene.active_camera);
    } else {
      Game::Map::Environment::apply_default(*scene.renderer, *scene.active_camera);
    }
  }

  if (loaded_definition && minimap_manager != nullptr) {
    minimap_manager->generate_for_map(def);
  }

  if (terrain_service.is_initialized()) {
    const auto* height_map = terrain_service.get_height_map();
    const int grid_width =
        (height_map != nullptr) ? height_map->get_width() : fallback_grid_width;
    const int grid_height =
        (height_map != nullptr) ? height_map->get_height() : fallback_grid_height;
    const float tile_size =
        (height_map != nullptr) ? height_map->get_tile_size() : fallback_tile_size;

    if (scene.ground) {
      scene.ground->configure(tile_size, grid_width, grid_height);
      scene.ground->set_biome(terrain_service.biome_settings());
    }

    if (scene.boundary_fog) {
      scene.boundary_fog->configure(grid_width, grid_height, tile_size);
    }

    if (height_map != nullptr) {
      if (scene.terrain) {
        scene.terrain->configure(*height_map, terrain_service.biome_settings());
      }
      if (scene.features) {
        scene.features->configure(*height_map, terrain_service.road_segments());
      }
      if (scene.scatter) {
        scene.scatter->configure(*height_map,
                                 terrain_service.biome_settings(),
                                 terrain_service.world_props());
      }
    }

    Game::Systems::CommandService::initialize(grid_width, grid_height);

    if (visibility_coordinator != nullptr) {
      visibility_coordinator->initialize_for_world(*world,
                                                   local_owner_id,
                                                   grid_width,
                                                   grid_height,
                                                   tile_size,
                                                   level.is_spectator_mode);
    }
  } else {
    Game::Map::MapDefinition fallback_def;
    fallback_def.grid.width = fallback_grid_width;
    fallback_def.grid.height = fallback_grid_height;
    fallback_def.grid.tile_size = fallback_tile_size;

    Game::Systems::CommandService::initialize(fallback_grid_width,
                                              fallback_grid_height);

    if (visibility_coordinator != nullptr) {
      visibility_coordinator->initialize_for_world(*world,
                                                   local_owner_id,
                                                   fallback_grid_width,
                                                   fallback_grid_height,
                                                   fallback_tile_size,
                                                   level.is_spectator_mode);
    }
  }
}
