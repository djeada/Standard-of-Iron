#include "level_orchestrator.h"

#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/map_loader.h"
#include "game/map/skirmish_loader.h"
#include "game/systems/ai_system.h"
#include "game/systems/game_state_serializer.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/victory_service.h"
#include "game_engine.h"
#include "loading_progress_tracker.h"
#include "minimap_manager.h"
#include "render/gl/camera.h"
#include "render/scene_renderer.h"
#include "utils/resource_utils.h"
#include <QCoreApplication>
#include <QDebug>

auto LevelOrchestrator::load_skirmish(
    const QString &map_path, const QVariantList &player_configs,
    int selected_player_id, Engine::Core::World &world,
    const RendererRefs &renderers, Game::Systems::LevelSnapshot &level,
    EntityCache &entity_cache, Game::Systems::VictoryService *victory_service,
    MinimapManager *minimap_manager, VisibilityReadyCallback visibility_ready,
    OwnerUpdateCallback owner_update, bool allow_default_player_barracks,
    LoadingProgressTracker *progress_tracker) -> LevelLoadResult {

  LevelLoadResult result;
  result.updated_player_id = selected_player_id;

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::LOADING_MAP_DATA);
    QCoreApplication::processEvents();
  }

  entity_cache.reset();

  Game::Map::SkirmishLoader loader(world, *renderers.renderer,
                                   *renderers.camera);

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::LOADING_TERRAIN);
    QCoreApplication::processEvents();
  }

  loader.set_ground_renderer(renderers.ground);
  loader.set_terrain_renderer(renderers.terrain);

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::LOADING_BIOME);
    QCoreApplication::processEvents();
  }

  loader.set_biome_renderer(renderers.biome);

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::LOADING_WATER_FEATURES);
    QCoreApplication::processEvents();
  }

  loader.set_river_renderer(renderers.river);
  loader.set_riverbank_renderer(renderers.riverbank);
  loader.set_bridge_renderer(renderers.bridge);

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::LOADING_ROADS);
    QCoreApplication::processEvents();
  }

  loader.set_road_renderer(renderers.road);

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::LOADING_ENVIRONMENT);
    QCoreApplication::processEvents();
  }

  loader.set_stone_renderer(renderers.stone);
  loader.set_plant_renderer(renderers.plant);
  loader.set_pine_renderer(renderers.pine);
  loader.set_olive_renderer(renderers.olive);
  loader.set_fire_camp_renderer(renderers.firecamp);
  loader.set_rain_renderer(renderers.rain);

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::LOADING_FOG);
    QCoreApplication::processEvents();
  }

  loader.set_fog_renderer(renderers.fog);

  loader.set_on_owners_updated(owner_update);
  loader.set_on_visibility_mask_ready(visibility_ready);

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::LOADING_ENTITIES);
    QCoreApplication::processEvents();
  }

  auto load_result =
      loader.start(map_path, player_configs, selected_player_id,
                   allow_default_player_barracks, result.updated_player_id);

  if (!load_result.ok) {
    result.success = false;
    result.error_message = load_result.errorMessage;
    if (progress_tracker) {
      progress_tracker->report_error(load_result.errorMessage);
    }
    return result;
  }

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::LOADING_AUDIO);
    QCoreApplication::processEvents();
  }

  level.map_name = load_result.map_name;
  level.player_unit_id = load_result.player_unit_id;
  level.cam_fov = load_result.cam_fov;
  level.cam_near = load_result.cam_near;
  level.cam_far = load_result.cam_far;
  level.max_troops_per_player = load_result.max_troops_per_player;
  level.grid_width = load_result.grid_width;
  level.grid_height = load_result.grid_height;
  level.tile_size = load_result.tile_size;
  level.is_spectator_mode = load_result.is_spectator_mode;
  level.rain = load_result.rainSettings;
  level.biome_seed = load_result.biome_seed;

  Game::GameConfig::instance().set_max_troops_per_player(
      load_result.max_troops_per_player);

  if (victory_service) {
    victory_service->configure(load_result.victoryConfig,
                               result.updated_player_id);
  }

  if (load_result.has_focus_position && renderers.camera) {
    const auto &cam_config = Game::GameConfig::instance().camera();
    renderers.camera->set_rts_view(
        load_result.focusPosition, cam_config.default_distance,
        cam_config.default_pitch, cam_config.default_yaw);
  }

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::GENERATING_MINIMAP);
    QCoreApplication::processEvents();
  }

  Game::Map::MapDefinition map_def;
  QString map_error;
  const QString resolved_map_path =
      Utils::Resources::resolveResourcePath(map_path);
  if (Game::Map::MapLoader::loadFromJsonFile(resolved_map_path, map_def,
                                             &map_error)) {
    if (minimap_manager) {
      minimap_manager->generate_for_map(map_def);
    }
  } else {
    qWarning() << "LevelOrchestrator: Failed to load map for minimap:"
               << map_error;
  }

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::INITIALIZING_SYSTEMS);
    QCoreApplication::processEvents();
  }

  if (auto *ai_system = world.get_system<Game::Systems::AISystem>()) {
    ai_system->reinitialize();
  }

  auto &troops = Game::Systems::TroopCountRegistry::instance();
  troops.rebuild_from_world(world);

  auto &stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  stats_registry.rebuild_from_world(world);

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  const auto &all_owners = owner_registry.get_all_owners();
  for (const auto &owner : all_owners) {
    if (owner.type == Game::Systems::OwnerType::Player ||
        owner.type == Game::Systems::OwnerType::AI) {
      stats_registry.mark_game_start(owner.owner_id);
    }
  }

  if (renderers.renderer) {
    renderers.renderer->prewarm_unit_templates();
  }

  if (progress_tracker) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::FINALIZING);
    QCoreApplication::processEvents();
  }

  result.success = true;
  return result;
}
