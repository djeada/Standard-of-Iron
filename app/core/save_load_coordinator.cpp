#include "save_load_coordinator.h"

#include <QDebug>
#include <QJsonObject>

#include <memory>

#include "audio_coordinator.h"
#include "audio_resource_loader.h"
#include "campaign_manager.h"
#include "game/core/world.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/systems/ai_system.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/save_load_service.h"
#include "game/systems/undead_awakening_system.h"
#include "game/systems/victory_service.h"
#include "game/units/factory.h"
#include "game_state_restorer.h"
#include "utils/resource_utils.h"

namespace App::Core {

namespace {

void append_mission_metadata(
    QJsonObject& metadata,
    const std::optional<Game::Mission::MissionContext>& mission_context) {
  if (!mission_context.has_value()) {
    return;
  }

  metadata["mission_mode"] = mission_context->mode;
  metadata["mission_campaign_id"] = mission_context->campaign_id;
  metadata["mission_id"] = mission_context->mission_id;
  metadata["mission_difficulty"] = mission_context->difficulty;
}

void restore_mission_context(const QJsonObject& metadata,
                             CampaignManager* campaign_manager) {
  if (campaign_manager == nullptr || !metadata.contains("mission_mode")) {
    return;
  }

  Game::Mission::MissionContext mission_context;
  mission_context.mode = metadata["mission_mode"].toString();
  mission_context.campaign_id = metadata["mission_campaign_id"].toString();
  mission_context.mission_id = metadata["mission_id"].toString();
  mission_context.difficulty = metadata["mission_difficulty"].toString();
  campaign_manager->set_mission_context(mission_context);
}

} // namespace

auto SaveLoadCoordinator::to_runtime_snapshot(const SaveRuntimeContext& context) const
    -> Game::Systems::RuntimeSnapshot {
  Game::Systems::RuntimeSnapshot snapshot;
  snapshot.paused = context.paused;
  snapshot.time_scale = context.time_scale;
  snapshot.local_owner_id = context.local_owner_id;
  snapshot.victory_state = context.victory_state;
  snapshot.cursor_mode = static_cast<int>(context.cursor_mode);
  snapshot.selected_player_id = context.selected_player_id;
  snapshot.follow_selection = context.follow_selection;
  snapshot.resources_by_owner =
      Game::Systems::PlayerResourceRegistry::instance().snapshot();
  return snapshot;
}

void SaveLoadCoordinator::apply_runtime_snapshot(
    const Game::Systems::RuntimeSnapshot& snapshot, ApplyRuntimeContext context) const {
  context.paused = snapshot.paused;
  context.time_scale = snapshot.time_scale;
  context.local_owner_id = snapshot.local_owner_id;
  context.victory_state = snapshot.victory_state;
  context.selected_player_id = snapshot.selected_player_id;
  context.follow_selection = snapshot.follow_selection;
  Game::Systems::PlayerResourceRegistry::instance().restore(
      snapshot.resources_by_owner);
  context.cursor_mode = static_cast<CursorMode>(snapshot.cursor_mode);
}

auto SaveLoadCoordinator::save_to_slot(const SaveToSlotContext& context) const
    -> SaveToSlotEffects {
  QJsonObject metadata = Game::Systems::GameStateSerializer::build_metadata(
      context.world, context.camera, context.level, context.runtime_snapshot);
  metadata["title"] = context.title;
  if (auto* undead_system =
          context.world.get_system<Game::Systems::UndeadAwakeningSystem>()) {
    metadata["undead_zones"] = undead_system->serialize_state();
  }

  append_mission_metadata(metadata, context.mission_context);

  if (!context.save_load_service.save_game_to_slot(context.world,
                                                   context.slot,
                                                   context.title,
                                                   context.map_name,
                                                   metadata,
                                                   context.screenshot)) {
    return {.error = context.save_load_service.get_last_error()};
  }

  return {.success = true, .emit_save_slots_changed = true};
}

auto SaveLoadCoordinator::load_from_slot(const LoadFromSlotContext& context) const
    -> LoadFromSlotEffects {
  if (!context.save_load_service.load_game_from_slot(context.world, context.slot)) {
    return {.error = context.save_load_service.get_last_error()};
  }

  const QJsonObject metadata = context.save_load_service.get_last_metadata();
  restore_mission_context(metadata, context.campaign_manager);

  Game::Systems::GameStateSerializer::restore_level_from_metadata(metadata,
                                                                  context.level);
  Game::Systems::GameStateSerializer::restore_camera_from_metadata(
      metadata, context.camera, context.viewport_width, context.viewport_height);
  Game::Systems::GameStateSerializer::restore_runtime_from_metadata(
      metadata, context.runtime_snapshot);
  context.apply_runtime_snapshot(context.runtime_snapshot);

  GameStateRestorer::restore_environment_from_metadata(
      metadata,
      context.scene,
      context.level,
      context.runtime_snapshot.local_owner_id,
      context.scene.minimap_manager,
      context.scene.visibility_coordinator);

  auto unit_registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*unit_registry);
  Game::Map::MapTransformer::setFactoryRegistry(unit_registry);
  qInfo() << "Factory registry reinitialized after loading saved game";

  GameStateRestorer::rebuild_registries_after_load(
      &context.world,
      context.selected_player_id,
      context.level,
      context.runtime_snapshot.local_owner_id);
  GameStateRestorer::rebuild_entity_cache(
      &context.world, context.entity_cache, context.runtime_snapshot.local_owner_id);
  if (!context.level.map_path.isEmpty()) {
    Game::Map::MapDefinition map_def;
    QString map_error;
    const QString resolved_map_path =
        Utils::Resources::resolve_resource_path(context.level.map_path);
    if (Game::Map::MapLoader::load_from_json_file(
            resolved_map_path, map_def, &map_error)) {
      if (auto* undead_system =
              context.world.get_system<Game::Systems::UndeadAwakeningSystem>()) {
        undead_system->configure(map_def);
        undead_system->restore_state(metadata["undead_zones"].toArray());
        if (context.victory_service != nullptr) {
          context.victory_service->set_undead_zone_query(undead_system);
        }
      }
    } else {
      qWarning() << "GameEngine: failed to load undead zone map data:" << map_error;
    }
  }

  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
  context.audio_coordinator->configure_audio_manifest_mappings(
      context.runtime_snapshot.local_owner_id);
  context.emit_troop_count_changed();

  if (auto* ai_system = context.world.get_system<Game::Systems::AISystem>()) {
    qInfo() << "Reinitializing AI system after loading saved game";
    ai_system->reinitialize();
  }

  if (context.victory_service != nullptr) {
    if (context.campaign_manager != nullptr &&
        context.campaign_manager->current_mission_context().is_campaign()) {
      context.campaign_manager->configure_mission_victory_conditions(
          context.victory_service, context.runtime_snapshot.local_owner_id);
    } else {
      context.victory_service->configure(Game::Map::VictoryConfig(),
                                         context.runtime_snapshot.local_owner_id);
    }
  }

  return {.success = true,
          .emit_selected_units_changed = true,
          .emit_owner_info_changed = true};
}

} // namespace App::Core
