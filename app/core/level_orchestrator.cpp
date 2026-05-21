#include "level_orchestrator.h"

#include <QCoreApplication>
#include <QDebug>

#include <utility>

#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/map_loader.h"
#include "game/map/skirmish_loader.h"
#include "game/systems/ai_system.h"
#include "game/systems/game_state_serializer.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/undead_awakening_system.h"
#include "game/systems/victory_service.h"
#include "loading_progress_tracker.h"
#include "minimap_manager.h"
#include "render/gl/camera.h"
#include "render/scene_renderer.h"
#include "utils/resource_utils.h"
#include "visibility_coordinator.h"

auto LevelOrchestrator::load_skirmish(const QString& map_path,
                                      const QVariantList& player_configs,
                                      int selected_player_id,
                                      Engine::Core::World& world,
                                      const AppSceneContext& scene,
                                      Game::Systems::LevelSnapshot& level,
                                      EntityCache& entity_cache,
                                      Game::Systems::VictoryService* victory_service,
                                      MinimapManager* minimap_manager,
                                      VisibilityCoordinator* visibility_coordinator,
                                      OwnerUpdateCallback owner_update,
                                      bool allow_default_player_barracks,
                                      LoadingProgressTracker* progress_tracker)
    -> LevelLoadResult {

  LevelLoadResult result;
  result.updated_player_id = selected_player_id;

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(LoadingProgressTracker::LoadingStage::LOADING_MAP_DATA);
    QCoreApplication::processEvents();
  }

  entity_cache.reset();

  Game::Map::SkirmishLoader loader(world, *scene.renderer, *scene.active_camera);

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(LoadingProgressTracker::LoadingStage::LOADING_TERRAIN);
    QCoreApplication::processEvents();
  }

  loader.set_ground_renderer(scene.ground);
  loader.set_terrain_renderer(scene.terrain);

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(LoadingProgressTracker::LoadingStage::LOADING_BIOME);
    QCoreApplication::processEvents();
  }

  loader.set_scatter_manager(scene.scatter);

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::LOADING_WATER_FEATURES);
    QCoreApplication::processEvents();
  }

  loader.set_feature_manager(scene.features);

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(LoadingProgressTracker::LoadingStage::LOADING_ROADS);
    QCoreApplication::processEvents();
  }

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::LOADING_ENVIRONMENT);
    QCoreApplication::processEvents();
  }

  loader.set_rain_renderer(scene.rain);

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(LoadingProgressTracker::LoadingStage::LOADING_FOG);
    QCoreApplication::processEvents();
  }

  loader.set_fog_renderer(scene.fog);
  loader.set_boundary_fog_renderer(scene.boundary_fog);

  loader.set_on_owners_updated(std::move(owner_update));
  loader.set_on_visibility_initialized([visibility_coordinator](
                                           Engine::Core::World& loaded_world,
                                           int local_owner_id,
                                           int map_width,
                                           int map_height,
                                           float tile_size,
                                           bool spectator_mode) {
    if (visibility_coordinator == nullptr) {
      return;
    }
    visibility_coordinator->initialize_for_world(
        loaded_world, local_owner_id, map_width, map_height, tile_size, spectator_mode);
  });

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(LoadingProgressTracker::LoadingStage::LOADING_ENTITIES);
    QCoreApplication::processEvents();
  }

  auto load_result = loader.start(map_path,
                                  player_configs,
                                  selected_player_id,
                                  allow_default_player_barracks,
                                  result.updated_player_id);

  if (!load_result.ok) {
    result.success = false;
    result.error_message = load_result.error_message;
    if (progress_tracker != nullptr) {
      progress_tracker->report_error(load_result.error_message);
    }
    return result;
  }

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(LoadingProgressTracker::LoadingStage::LOADING_AUDIO);
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
  level.rain = load_result.rain_settings;
  level.biome_seed = load_result.biome_seed;
  level.lighting = load_result.lighting_settings;

  Game::GameConfig::instance().set_max_troops_per_player(
      load_result.max_troops_per_player);

  if (victory_service != nullptr) {
    victory_service->configure(load_result.victory_config, result.updated_player_id);
  }

  if (load_result.has_focus_position && (scene.active_camera != nullptr)) {
    const auto& cam_config = Game::GameConfig::instance().camera();
    scene.active_camera->set_rts_view(load_result.focus_position,
                                      cam_config.default_distance,
                                      cam_config.default_pitch,
                                      cam_config.default_yaw);
  }

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::GENERATING_MINIMAP);
    QCoreApplication::processEvents();
  }

  Game::Map::MapDefinition map_def;
  QString map_error;
  const QString resolved_map_path = Utils::Resources::resolve_resource_path(map_path);
  if (Game::Map::MapLoader::load_from_json_file(
          resolved_map_path, map_def, &map_error)) {
    if (auto* undead_system =
            world.get_system<Game::Systems::UndeadAwakeningSystem>()) {
      undead_system->configure(map_def);
      if (victory_service != nullptr) {
        victory_service->set_undead_zone_query(undead_system);
      }
    }
    if (minimap_manager != nullptr) {
      minimap_manager->generate_for_map(map_def);
      if (visibility_coordinator != nullptr) {
        visibility_coordinator->publish_current_frame(true);
      }
    }
  } else {
    qWarning() << "LevelOrchestrator: Failed to load map for minimap:" << map_error;
  }

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::INITIALIZING_SYSTEMS);
    QCoreApplication::processEvents();
  }

  if (auto* ai_system = world.get_system<Game::Systems::AISystem>()) {
    ai_system->reinitialize();
  }

  auto& troops = Game::Systems::TroopCountRegistry::instance();
  troops.rebuild_from_world(world);

  auto& stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  stats_registry.rebuild_from_world(world);

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  const auto& all_owners = owner_registry.get_all_owners();
  for (const auto& owner : all_owners) {
    if (owner.type == Game::Systems::OwnerType::Player ||
        owner.type == Game::Systems::OwnerType::AI) {
      stats_registry.mark_game_start(owner.owner_id);
    }
  }

  if (scene.renderer != nullptr) {
    scene.renderer->prewarm_unit_templates(
        &world,
        [progress_tracker](
            const Render::GL::Renderer::TemplatePrewarmProgress& progress) -> bool {
          if (progress_tracker == nullptr) {
            return true;
          }

          QString detail;
          using Phase = Render::GL::Renderer::TemplatePrewarmProgress::Phase;
          switch (progress.phase) {
          case Phase::CollectingProfiles:
            detail = "Prewarming templates: scanning unit profiles...";
            break;
          case Phase::BuildingCoreTemplates:
            detail = QString("Prewarming templates: %1 / %2")
                         .arg(progress.completed)
                         .arg(progress.total);
            break;
          case Phase::QueueingExtendedTemplates:
            detail = QString("Queued deferred template warmup (%1 additional)")
                         .arg(progress.total);
            break;
          case Phase::Completed:
            detail = QString("Template warmup complete (%1 core, %2 total)")
                         .arg(progress.completed)
                         .arg(progress.total);
            break;
          case Phase::Cancelled:
            detail = "Template warmup cancelled";
            break;
          }

          progress_tracker->set_stage(
              LoadingProgressTracker::LoadingStage::INITIALIZING_SYSTEMS, detail);
          QCoreApplication::processEvents();
          return !progress_tracker->has_failed();
        });
  }

  if (progress_tracker != nullptr) {
    progress_tracker->set_stage(LoadingProgressTracker::LoadingStage::FINALIZING);
    QCoreApplication::processEvents();
  }

  result.success = true;
  return result;
}
