#include "app/persistence/save_load_coordinator.h"

#include <QDebug>
#include <QJsonObject>

#include <memory>

#include "app/audio/audio_coordinator.h"
#include "app/audio/audio_resource_loader.h"
#include "app/persistence/game_state_restorer.h"
#include "app/world/visibility_coordinator.h"
#include "game/core/world.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/mission/campaign_manager.h"
#include "game/render_bridge/game_state_serializer.h"
#include "game/save/serialization.h"
#include "game/session/deterministic_rng.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/save_load_service.h"
#include "game/systems/undead_awakening_system.h"
#include "game/systems/victory_service.h"
#include "game/systems/world_restore.h"
#include "game/units/factory.h"
#include "game/wildlife/wildlife_system.h"
#include "render/scene_renderer.h"
#include "utils/resource_utils.h"

namespace App::Core {

namespace {

void restore_mission_context(const Game::Systems::Save::Record& record,
                             CampaignManager* campaign_manager) {
  if (campaign_manager == nullptr || record.mode.isEmpty()) {
    return;
  }

  Game::Mission::MissionContext mission_context;
  mission_context.mode = record.mode;
  mission_context.campaign_id = record.campaign_id;
  mission_context.mission_id = record.mission_id;
  mission_context.difficulty = record.difficulty;
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
  snapshot.harvested_by_owner =
      Game::Systems::PlayerResourceRegistry::instance().harvested_snapshot();

  auto& session = Game::Session::SessionContext::active();
  snapshot.simulation_tick = session.clock().tick();
  snapshot.rng_seed = session.rng().seed();
  snapshot.rng_draw_count = session.rng().draw_count();
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
  Game::Systems::PlayerResourceRegistry::instance().restore_harvested(
      snapshot.harvested_by_owner);

  auto& session = Game::Session::SessionContext::active();
  session.clock().restore(snapshot.simulation_tick);
  session.rng().restore(snapshot.rng_seed, snapshot.rng_draw_count);

  context.cursor_mode = static_cast<CursorMode>(snapshot.cursor_mode);
}

auto SaveLoadCoordinator::begin_save_to_slot(const SaveToSlotContext& context) const
    -> SaveToSlotEffects {
  QJsonObject metadata = Game::Systems::GameStateSerializer::build_metadata(
      context.world, context.camera, context.level, context.runtime_snapshot);
  metadata["title"] = context.title;
  if (auto* undead_system =
          context.world.get_system<Game::Systems::UndeadAwakeningSystem>()) {
    metadata["undead_zones"] = undead_system->serialize_state();
  }
  if (auto* wildlife_system =
          context.world.get_system<Game::Wildlife::WildlifeSystem>()) {
    metadata["wildlife"] = wildlife_system->serialize_state();
  }
  if (!context.mission_wave_state.isEmpty()) {
    metadata["mission_waves"] = context.mission_wave_state;
  }
  if (!context.mission_stage_state.isEmpty()) {
    metadata["mission_stages"] = context.mission_stage_state;
  }
  if (!context.commander_message_state.isEmpty()) {
    metadata["commander_messages"] = context.commander_message_state;
  }

  Game::Systems::SaveRequest request;
  request.slot_name = context.slot;
  request.title = context.title;

  request.map_name = context.map_name;
  request.map_path = context.level.map_path;
  if (context.mission_context.has_value()) {
    request.mode = context.mission_context->mode;
    request.campaign_id = context.mission_context->campaign_id;
    request.mission_id = context.mission_context->mission_id;
    request.difficulty = context.mission_context->difficulty;
  }
  if (request.mode.isEmpty()) {
    request.mode = QStringLiteral("skirmish");
  }
  request.kind = context.kind;
  request.play_time_seconds = context.play_time_seconds;
  request.metadata = metadata;
  request.autosave_retention = context.autosave_retention;

  request.world = Engine::Core::Serialization::serialize_world(&context.world);

  const quint64 job_id = context.save_load_service.begin_save(request);
  if (job_id == 0) {
    return {.error = context.save_load_service.get_last_error()};
  }

  return {.queued = true, .job_id = job_id};
}

auto SaveLoadCoordinator::load_from_slot(const LoadFromSlotContext& context) const
    -> LoadFromSlotEffects {

  if (context.scene.renderer != nullptr) {
    context.scene.renderer->clear_entity_render_caches();
  }

  if (!context.save_load_service.load_game_from_slot(context.world, context.slot)) {
    return {.error = context.save_load_service.get_last_error()};
  }

  const Game::Systems::Save::Record& record =
      context.save_load_service.get_last_record();
  const QJsonObject metadata = record.metadata;
  restore_mission_context(record, context.campaign_manager);

  Game::Systems::GameStateSerializer::restore_player_nations_from_metadata(metadata);
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

  Game::Systems::GameStateSerializer::restore_visibility_from_metadata(metadata);
  if (context.scene.visibility_coordinator != nullptr) {
    context.scene.visibility_coordinator->publish_current_frame(true);
  }

  auto unit_registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*unit_registry);
  Game::Map::MapTransformer::setFactoryRegistry(unit_registry);
  qInfo() << "Factory registry reinitialized after loading saved game";

  const auto restored = Game::Persistence::rebuild_registries_after_load(
      &context.world, context.runtime_snapshot.local_owner_id);
  context.level.player_unit_id = restored.player_unit_id;
  context.selected_player_id = context.runtime_snapshot.local_owner_id;
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
      if (auto* wildlife_system =
              context.world.get_system<Game::Wildlife::WildlifeSystem>()) {
        wildlife_system->configure(map_def);
        wildlife_system->restore_state(metadata["wildlife"].toObject());
      }
    } else {
      qWarning() << "GameEngine: failed to load undead zone map data:" << map_error;
    }
  }
  if (context.scene.session != nullptr) {
    context.scene.session->terrain().seal();
  } else {
    qWarning() << "SaveLoadCoordinator: no session in scene context; terrain left "
                  "unsealed";
  }

  if (context.restore_mission_waves) {
    context.restore_mission_waves(metadata.value("mission_waves").toObject());
  }
  if (context.restore_mission_stages) {
    context.restore_mission_stages(metadata.value("mission_stages").toObject());
  }
  if (context.restore_commander_messages) {
    context.restore_commander_messages(metadata.value("commander_messages").toObject());
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
