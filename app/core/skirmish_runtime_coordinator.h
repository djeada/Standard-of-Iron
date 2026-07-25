#pragma once

#include <QElapsedTimer>
#include <QString>
#include <QVariantList>

#include <atomic>
#include <functional>

#include "app_scene_context.h"
#include "entity_cache.h"
#include "game/map/mission_definition.h"
#include "game/systems/game_state_serializer.h"

class LoadingProgressTracker;
class MinimapManager;
class VisibilityCoordinator;

namespace Engine::Core {
class World;
}

namespace Game::Systems {
class VictoryService;
}

namespace Render::GL {
class Camera;
}

namespace App::Core {

struct PerformSkirmishLoadContext {
  Engine::Core::World& world;
  Game::Systems::LevelSnapshot& level;
  EntityCache& entity_cache;
  QString map_path;
  QVariantList player_configs;
  int selected_player_id = 1;
  AppSceneContext scene;
  Game::Systems::VictoryService* victory_service = nullptr;
  MinimapManager* minimap_manager = nullptr;
  VisibilityCoordinator* visibility_coordinator = nullptr;
  bool allow_default_player_barracks = true;
  LoadingProgressTracker* loading_progress_tracker = nullptr;
  std::function<void()> emit_owner_info_changed;
};

struct PerformSkirmishLoadEffects {
  bool success = false;
  QString error;
  int updated_player_id = 1;
  bool selected_player_changed = false;
};

struct CenterCameraOnLocalForcesContext {
  Engine::Core::World* world = nullptr;
  Render::GL::Camera* camera = nullptr;
  int local_owner_id = 1;
};

struct InitializePlayerResourcesContext {
  const Game::Systems::LevelSnapshot& level;
  int local_owner_id = 1;
  const Game::Mission::MissionDefinition* mission_definition = nullptr;
};

struct FinalizeSkirmishLoadContext {
  FinalizeSkirmishLoadContext(bool& runtime_loading,
                              std::atomic_bool& loading_overlay_wait_for_first_frame,
                              int& loading_overlay_frames_remaining,
                              qint64& loading_overlay_min_duration_ms,
                              QElapsedTimer& loading_overlay_timer,
                              bool& finalize_progress_after_overlay,
                              bool& show_objectives_after_loading,
                              bool is_campaign_mission)
      : runtime_loading(runtime_loading)
      , loading_overlay_wait_for_first_frame(loading_overlay_wait_for_first_frame)
      , loading_overlay_frames_remaining(loading_overlay_frames_remaining)
      , loading_overlay_min_duration_ms(loading_overlay_min_duration_ms)
      , loading_overlay_timer(loading_overlay_timer)
      , finalize_progress_after_overlay(finalize_progress_after_overlay)
      , show_objectives_after_loading(show_objectives_after_loading)
      , is_campaign_mission(is_campaign_mission) {}

  bool& runtime_loading;
  std::atomic_bool& loading_overlay_wait_for_first_frame;
  int& loading_overlay_frames_remaining;
  qint64& loading_overlay_min_duration_ms;
  QElapsedTimer& loading_overlay_timer;
  bool& finalize_progress_after_overlay;
  bool& show_objectives_after_loading;
  bool is_campaign_mission = false;
};

struct FinalizeSkirmishLoadEffects {
  bool emit_is_loading_changed = true;
  bool rebuild_entity_cache = true;
  bool emit_troop_count_changed = true;
  bool sync_scatter_world_props = true;
  bool sync_selected_player_state = true;
  bool reset_ambient_state = true;
  bool apply_spectator_mode = true;
  bool emit_owner_info_changed = true;
  bool emit_spectator_mode_changed = true;
};

class SkirmishRuntimeCoordinator {
public:
  [[nodiscard]] auto perform_load(const PerformSkirmishLoadContext& ctx) const
      -> PerformSkirmishLoadEffects;

  void center_camera_on_local_forces(const CenterCameraOnLocalForcesContext& ctx) const;

  void initialize_player_resources(const InitializePlayerResourcesContext& ctx) const;

  [[nodiscard]] auto finalize_load(const FinalizeSkirmishLoadContext& ctx) const
      -> FinalizeSkirmishLoadEffects;
};

} // namespace App::Core
