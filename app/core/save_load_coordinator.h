#pragma once

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>

#include "../models/cursor_mode.h"
#include "app_scene_context.h"
#include "entity_cache.h"
#include "game/map/mission_context.h"
#include "game/systems/game_state_serializer.h"

class AudioCoordinator;
class CampaignManager;

namespace Engine::Core {
class World;
}

namespace Game::Systems {
class SaveLoadService;
class VictoryService;
} // namespace Game::Systems

namespace Render::GL {
class Camera;
}

namespace App::Core {

struct SaveRuntimeContext {
  bool paused = false;
  float time_scale = 1.0F;
  int local_owner_id = 1;
  QString victory_state;
  CursorMode cursor_mode{CursorMode::Normal};
  int selected_player_id = 1;
  bool follow_selection = false;
};

struct ApplyRuntimeContext {
  bool& paused;
  float& time_scale;
  int& local_owner_id;
  QString& victory_state;
  CursorMode& cursor_mode;
  int& selected_player_id;
  bool& follow_selection;
};

struct SaveToSlotContext {
  Engine::Core::World& world;
  Game::Systems::SaveLoadService& save_load_service;
  Render::GL::Camera* camera = nullptr;
  const Game::Systems::LevelSnapshot& level;
  const Game::Systems::RuntimeSnapshot& runtime_snapshot;
  QString slot;
  QString title;
  QString map_name;
  const QByteArray& screenshot;
  std::optional<Game::Mission::MissionContext> mission_context;
};

struct SaveToSlotEffects {
  bool success = false;
  bool emit_save_slots_changed = false;
  QString error;
};

struct LoadFromSlotContext {
  Engine::Core::World& world;
  Game::Systems::SaveLoadService& save_load_service;
  QString slot;
  CampaignManager* campaign_manager = nullptr;
  Game::Systems::LevelSnapshot& level;
  Render::GL::Camera* camera = nullptr;
  int viewport_width = 0;
  int viewport_height = 0;
  Game::Systems::RuntimeSnapshot& runtime_snapshot;
  std::function<void(const Game::Systems::RuntimeSnapshot&)> apply_runtime_snapshot;
  int& selected_player_id;
  AppSceneContext scene;
  EntityCache& entity_cache;
  AudioCoordinator* audio_coordinator = nullptr;
  Game::Systems::VictoryService* victory_service = nullptr;
  std::function<void()> emit_troop_count_changed;
};

struct LoadFromSlotEffects {
  bool success = false;
  bool emit_selected_units_changed = false;
  bool emit_owner_info_changed = false;
  QString error;
};

class SaveLoadCoordinator {
public:
  [[nodiscard]] auto to_runtime_snapshot(const SaveRuntimeContext& context) const
      -> Game::Systems::RuntimeSnapshot;
  void apply_runtime_snapshot(const Game::Systems::RuntimeSnapshot& snapshot,
                              ApplyRuntimeContext context) const;

  [[nodiscard]] auto
  save_to_slot(const SaveToSlotContext& context) const -> SaveToSlotEffects;
  [[nodiscard]] auto
  load_from_slot(const LoadFromSlotContext& context) const -> LoadFromSlotEffects;
};

} // namespace App::Core
