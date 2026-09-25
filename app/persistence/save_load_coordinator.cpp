#include "app/persistence/save_load_coordinator.h"

#include <QDebug>
#include <QJsonObject>
#include <QObject>

#include <cstdint>
#include <memory>

#include "app/audio/audio_coordinator.h"
#include "app/audio/audio_resource_loader.h"
#include "app/persistence/game_state_restorer.h"
#include "app/session/level_orchestrator.h"
#include "app/world/visibility_coordinator.h"
#include "game/core/world.h"
#include "game/map/map_context.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/mission/campaign_manager.h"
#include "game/render_bridge/game_state_serializer.h"
#include "game/save/serialization.h"
#include "game/session/deterministic_rng.h"
#include "game/session/map_session.h"
#include "game/session/session_context.h"
#include "game/session/session_snapshot.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/save_load_service.h"
#include "game/systems/victory_service.h"
#include "game/systems/world_restore.h"
#include "game/units/factory.h"
#include "render/scene_renderer.h"
#include "utils/resource_utils.h"

namespace App::Core {

namespace {

constexpr const char* k_match_launch_key = "match_launch";

auto restore_mission_context(const Game::Systems::Save::Record& record,
                             CampaignManager* campaign_manager)
    -> Game::Mission::MatchDifficulty {
  const QJsonObject launch =
      record.metadata.value(QLatin1String(k_match_launch_key)).toObject();

  Game::Mission::MatchDifficulty difficulty{record.difficulty};
  const QJsonObject by_owner =
      launch.value(QLatin1String("difficulty_by_owner")).toObject();
  for (auto it = by_owner.constBegin(); it != by_owner.constEnd(); ++it) {
    bool owner_ok = false;
    const int owner_id = it.key().toInt(&owner_ok);
    if (owner_ok) {
      difficulty.set_owner(owner_id, it.value().toString());
    }
  }

  if (campaign_manager == nullptr) {
    return difficulty;
  }

  Game::Mission::MissionContext mission_context;
  mission_context.mode =
      record.mode.isEmpty() ? QStringLiteral("skirmish") : record.mode;
  mission_context.campaign_id = record.campaign_id;
  mission_context.mission_id = record.mission_id;
  mission_context.difficulty = record.difficulty;
  mission_context.mission_file = launch.value(QLatin1String("mission_file")).toString();
  campaign_manager->restore_mission_context(mission_context);
  return difficulty;
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
  auto& session = context.session;
  snapshot.resources_by_owner = session.economy().snapshot();
  snapshot.harvested_by_owner = session.economy().harvested_snapshot();

  snapshot.simulation_tick = session.clock().tick();
  snapshot.rng_seed = session.rng().seed();
  snapshot.rng_draw_count = session.rng().draw_count();
  return snapshot;
}

void SaveLoadCoordinator::apply_runtime_snapshot(
    const Game::Systems::RuntimeSnapshot& snapshot, ApplyRuntimeContext context) const {

  context.paused = false;
  context.time_scale = snapshot.time_scale;
  context.local_owner_id = snapshot.local_owner_id;
  context.victory_state = snapshot.victory_state;
  context.selected_player_id = snapshot.selected_player_id;
  context.follow_selection = snapshot.follow_selection;
  auto& session = context.session;
  session.economy().restore(snapshot.resources_by_owner);
  session.economy().restore_harvested(snapshot.harvested_by_owner);

  session.clock().restore(snapshot.simulation_tick);
  session.rng().restore(snapshot.rng_seed, snapshot.rng_draw_count);

  context.cursor_mode = static_cast<CursorMode>(snapshot.cursor_mode);
}

auto SaveLoadCoordinator::begin_save_to_slot(const SaveToSlotContext& context) const
    -> SaveToSlotEffects {
  QJsonObject metadata = Game::Systems::GameStateSerializer::build_metadata(
      context.world, context.camera, context.level, context.runtime_snapshot);
  metadata["title"] = context.title;
  if (!context.mission_title.isEmpty()) {
    metadata["mission_title"] = context.mission_title;
  }
  metadata["session_snapshot"] = Game::Session::SessionSnapshot::capture(
      Game::Session::SnapshotScope{.world = &context.world, .map = nullptr});
  if (!context.mission_wave_state.isEmpty()) {
    metadata["mission_waves"] = context.mission_wave_state;
  }
  if (!context.mission_stage_state.isEmpty()) {
    metadata["mission_stages"] = context.mission_stage_state;
  }
  if (!context.commander_message_state.isEmpty()) {
    metadata["commander_messages"] = context.commander_message_state;
  }
  if (!context.tutorial_state.isEmpty()) {
    metadata["tutorial"] = context.tutorial_state;
  }
  if (!context.battle_stats.isEmpty()) {
    metadata["battle_stats"] = context.battle_stats;
  }

  Game::Systems::SaveRequest request;
  request.slot_name = context.slot;
  request.title = context.title;

  request.map_name = context.map_name;
  request.map_path = context.level.map_path;
  QJsonObject launch;
  if (context.mission_context.has_value()) {
    request.mode = context.mission_context->mode;
    request.campaign_id = context.mission_context->campaign_id;
    request.mission_id = context.mission_context->mission_id;
    request.difficulty = context.mission_context->difficulty;
    launch["mission_file"] = context.mission_context->mission_file;
  }
  if (context.difficulty != nullptr) {
    launch["difficulty"] = context.difficulty->baseline_id();
    QJsonObject by_owner;
    for (const int owner_id : context.difficulty->owner_ids()) {
      by_owner[QString::number(owner_id)] = context.difficulty->id_for(owner_id);
    }
    if (!by_owner.isEmpty()) {
      launch["difficulty_by_owner"] = by_owner;
    }
  }
  if (!launch.isEmpty()) {
    metadata[QLatin1String(k_match_launch_key)] = launch;
  }
  if (request.mode.isEmpty()) {
    request.mode = QStringLiteral("skirmish");
  }
  request.kind = context.kind;
  request.play_time_seconds = context.play_time_seconds;
  request.metadata = metadata;
  request.autosave_retention = context.autosave_retention;

  request.world = Engine::Core::Serialization::serialize_world(&context.world);

  const Engine::Core::CaptureStamp stamp =
      Engine::Core::Serialization::read_capture_stamp(request.world);
  if (!stamp.matches(context.runtime_snapshot.simulation_tick,
                     context.runtime_snapshot.rng_draw_count)) {
    qWarning() << "SaveLoadCoordinator: torn capture - session metadata is at tick"
               << context.runtime_snapshot.simulation_tick << "draw"
               << context.runtime_snapshot.rng_draw_count << "but the world is at"
               << stamp.tick << "draw" << stamp.rng_draws;
    return {.error = QObject::tr("Save: the match advanced while it was being "
                                 "captured. Please try again.")};
  }

  const quint64 job_id = context.save_load_service.begin_save(request);
  if (job_id == 0) {
    return {.error = context.save_load_service.get_last_error()};
  }

  return {.queued = true, .job_id = job_id};
}

auto SaveLoadCoordinator::load_from_slot(const LoadFromSlotContext& context) const
    -> LoadFromSlotEffects {
  QString partial_restore_warning;

  if (context.scene.renderer != nullptr) {
    context.scene.renderer->clear_entity_render_caches();
  }

  if (auto* ai_system = context.world.get_system<Game::Systems::AISystem>()) {
    ai_system->wait_for_decisions();
  }

  bool world_discarded = false;
  if (!context.save_load_service.load_game_from_slot(
          context.world, context.slot, &world_discarded)) {
    return {.error = context.save_load_service.get_last_error(),
            .world_discarded = world_discarded};
  }

  const Game::Systems::Save::Record& record =
      context.save_load_service.get_last_record();
  const QJsonObject metadata = record.metadata;
  LoadFromSlotEffects effects;
  effects.match_difficulty = restore_mission_context(record, context.campaign_manager);

  Game::Systems::GameStateSerializer::restore_player_nations_from_metadata(
      Game::Session::session_for(context.world).nations(), metadata);
  Game::Systems::GameStateSerializer::restore_level_from_metadata(metadata,
                                                                  context.level);
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
  Game::Systems::GameStateSerializer::restore_camera_from_metadata(
      metadata, context.camera, context.viewport_width, context.viewport_height);

  Game::Systems::GameStateSerializer::restore_visibility_from_metadata(
      Game::Session::session_for(context.world).visibility(), metadata);
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
  Game::Map::MapContext map_context;
  if (!context.level.map_path.isEmpty()) {
    QString map_error;
    map_context =
        Game::Map::MapContextStore::acquire(context.level.map_path, &map_error);
    if (!map_context.valid()) {

      const QString map_warning =
          QObject::tr("Loaded, but '%1' could not be read, so wildlife, undead "
                      "zones and cursed veins were not restored.")
              .arg(context.level.map_path);
      partial_restore_warning =
          partial_restore_warning.isEmpty()
              ? map_warning
              : partial_restore_warning + QStringLiteral(" ") + map_warning;
      qWarning() << "SaveLoadCoordinator: failed to load map data for the save:"
                 << map_error;
    }
  }

  if (auto* ai_system = context.world.get_system<Game::Systems::AISystem>()) {
    qInfo() << "Reinitializing AI system before restoring its saved state";
    ai_system->reinitialize();
  }

  const auto report = Game::Session::restore_map_session(
      {.world = &context.world,
       .map = map_context.definition(),
       .victory_service = context.victory_service,
       .configure_victory_rules = context.configure_victory,
       .snapshot = metadata.value("session_snapshot").toObject()});
  for (const auto& key : report.missing_from_save) {
    qWarning() << "SaveLoadCoordinator: the save carries no state for"
               << QString::fromStdString(key);
  }
  for (const auto& key : report.unclaimed_in_save) {
    qWarning() << "SaveLoadCoordinator: nothing in this build claims the saved"
               << QString::fromStdString(key) << "state";
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
  if (context.restore_tutorial) {
    context.restore_tutorial(metadata.value("tutorial").toObject());
  }
  if (context.restore_battle_stats) {
    context.restore_battle_stats(metadata.value("battle_stats").toObject());
  }

  // Loading dropped every baked creature body with the render caches above.
  // Bake them again for the restored roster; without this the barrier left by
  // the previous match's prewarm keeps anything not yet baked off screen.
  prewarm_match_render_templates(context.world, context.scene);

  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
  context.audio_coordinator->configure_audio_manifest_mappings(
      context.runtime_snapshot.local_owner_id);
  context.emit_troop_count_changed();

  effects.success = true;
  effects.emit_selected_units_changed = true;
  effects.emit_owner_info_changed = true;
  effects.warning = partial_restore_warning;
  return effects;
}

} // namespace App::Core
