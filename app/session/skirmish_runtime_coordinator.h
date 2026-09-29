#pragma once

#include <QElapsedTimer>
#include <QString>
#include <QVariantList>

#include <atomic>
#include <functional>

#include "app/core/app_scene_context.h"
#include "app/core/entity_cache.h"
#include "game/map/mission_definition.h"
#include "game/mission/difficulty_profile.h"
#include "game/systems/match_snapshot.h"

class CampaignManager;
class LoadingProgressTracker;
class MinimapManager;
class VisibilityCoordinator;

namespace Engine::Core {
class World;
}

namespace App::Session {
class LoadingOverlay;
}

namespace Game::Session {
class SessionContext;
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
  bool defer_ai_initialization = false;
  LoadingProgressTracker* loading_progress_tracker = nullptr;
  std::function<void()> emit_owner_info_changed;
};

struct PerformSkirmishLoadEffects {
  bool success = false;
  QString error;
  int updated_player_id = 1;
  bool selected_player_changed = false;

  QVariantList resolved_player_configs;
};

struct CenterCameraOnLocalForcesContext {
  Engine::Core::World* world = nullptr;
  Render::GL::Camera* camera = nullptr;
  int local_owner_id = 1;
};

struct InitializePlayerResourcesContext {
  Game::Session::SessionContext& session;
  const Game::Systems::LevelSnapshot& level;
  int local_owner_id = 1;
  const Game::Mission::MissionDefinition* mission_definition = nullptr;

  const Game::Mission::MatchDifficulty* difficulty = nullptr;
};

struct FinalizeSkirmishLoadContext {
  bool& runtime_loading;
  App::Session::LoadingOverlay& loading_overlay;
  bool is_mission_match = false;
};

struct LoadingTipHints {
  QString mission_id;
  bool mission_has_undead = false;
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

  [[nodiscard]] static auto
  loading_tip_hints(const CampaignManager* campaign) -> LoadingTipHints;

  static void apply_difficulty_forces(Engine::Core::World& world,
                                      const Game::Mission::MatchDifficulty& difficulty,
                                      int local_owner_id);

  static void record_startup_counters(Engine::Core::World& world);

  static void prepare_ai_state(Engine::Core::World* world,
                               Game::Session::SessionContext* session);
};

} // namespace App::Core
