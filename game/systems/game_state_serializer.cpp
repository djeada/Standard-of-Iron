#include "game_state_serializer.h"

#include <QDebug>
#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qvectornd.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "game/game_config.h"
#include "game/map/explored_mask_codec.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/util/json_vec_utils.h"
#include "scene/camera.h"

namespace Game::Systems {

namespace {

auto owner_resources_to_json(const std::vector<OwnerResourceState>& rows)
    -> QJsonArray {
  QJsonArray rows_array;
  for (const auto& row : rows) {
    QJsonObject row_obj;
    row_obj["owner_id"] = row.owner_id;
    QJsonObject resources_obj;
    for (ResourceType const type : k_all_resource_types) {
      resources_obj[QLatin1String(resource_type_key(type))] = row.amounts.get(type);
    }
    row_obj["resources"] = resources_obj;
    rows_array.append(row_obj);
  }
  return rows_array;
}

void owner_resources_from_json(const QJsonArray& rows_array,
                               std::vector<OwnerResourceState>& out_rows) {
  out_rows.clear();
  out_rows.reserve(rows_array.size());
  for (const auto& value : rows_array) {
    const auto row_obj = value.toObject();
    OwnerResourceState row;
    row.owner_id = row_obj.value("owner_id").toInt(0);
    const auto resources_obj = row_obj.value("resources").toObject();
    for (auto it = resources_obj.begin(); it != resources_obj.end(); ++it) {
      ResourceType type;
      if (!resource_type_from_key(QStringView(it.key()), type)) {
        continue;
      }
      row.amounts.set(type, it.value().toInt(0));
    }
    out_rows.push_back(row);
  }
}

} // namespace

auto GameStateSerializer::build_metadata(const Engine::Core::World&,
                                         const Render::GL::Camera* camera,
                                         const LevelSnapshot& level,
                                         const RuntimeSnapshot& runtime)
    -> QJsonObject {

  QJsonObject metadata;
  metadata["map_path"] = level.map_path;
  metadata["map_name"] = level.map_name;
  metadata["max_troops_per_player"] = level.max_troops_per_player;
  metadata["local_owner_id"] = runtime.local_owner_id;
  metadata["player_unit_id"] = static_cast<qint64>(level.player_unit_id);

  QJsonObject environment_obj;
  environment_obj["start_time"] = level.environment.start_time;
  environment_obj["time_mode"] =
      QString::fromLatin1(Game::Map::time_mode_name(level.environment.time_mode));
  environment_obj["day_length_seconds"] = level.environment.day_length_seconds;
  environment_obj["lighting_profile"] = level.environment.lighting_profile;
  if (level.environment.fog_density_override >= 0.0F) {
    environment_obj["fog_density"] = level.environment.fog_density_override;
  }
  if (level.environment.exposure_override >= 0.0F) {
    environment_obj["exposure"] = level.environment.exposure_override;
  }
  environment_obj["current_time"] = level.environment_clock.hour;
  environment_obj["transition_active"] = level.environment_clock.transition_active;
  environment_obj["transition_start_time"] =
      level.environment_clock.transition_start_hour;
  environment_obj["transition_target_time"] =
      level.environment_clock.transition_target_hour;
  environment_obj["transition_duration"] = level.environment_clock.transition_duration;
  environment_obj["transition_elapsed"] = level.environment_clock.transition_elapsed;
  metadata["environment"] = environment_obj;

  metadata["game_max_troops_per_player"] =
      Game::GameConfig::instance().get_max_troops_per_player();

  const auto& terrain_service = Game::Map::TerrainService::instance();
  if (const auto* height_map = terrain_service.get_height_map()) {
    metadata["grid_width"] = height_map->get_width();
    metadata["grid_height"] = height_map->get_height();
    metadata["tile_size"] = height_map->get_tile_size();
  }

  if (camera != nullptr) {
    QJsonObject camera_obj;
    camera_obj["position"] =
        Game::JsonUtils::vec3_to_json_array(camera->get_position());
    camera_obj["target"] = Game::JsonUtils::vec3_to_json_array(camera->get_target());
    camera_obj["distance"] = camera->get_distance();
    camera_obj["pitch_deg"] = camera->get_pitch_deg();
    camera_obj["fov"] = camera->get_fov();
    camera_obj["near"] = camera->get_near();
    camera_obj["far"] = camera->get_far();
    metadata["camera"] = camera_obj;
  }

  QJsonObject runtime_obj;
  runtime_obj["paused"] = runtime.paused;
  runtime_obj["time_scale"] = runtime.time_scale;
  runtime_obj["victory_state"] = runtime.victory_state;
  runtime_obj["cursor_mode"] = runtime.cursor_mode;
  runtime_obj["selected_player_id"] = runtime.selected_player_id;
  runtime_obj["follow_selection"] = runtime.follow_selection;
  runtime_obj["resources_by_owner"] =
      owner_resources_to_json(runtime.resources_by_owner);
  runtime_obj["harvested_by_owner"] =
      owner_resources_to_json(runtime.harvested_by_owner);
  runtime_obj["simulation_tick"] = static_cast<qint64>(runtime.simulation_tick);
  runtime_obj["rng_seed"] = static_cast<qint64>(runtime.rng_seed);
  runtime_obj["rng_draw_count"] = static_cast<qint64>(runtime.rng_draw_count);
  metadata["runtime"] = runtime_obj;

  QJsonArray nations_array;
  for (const auto& [player_id, nation_id] :
       NationRegistry::instance().player_nation_assignments()) {
    QJsonObject nation_obj;
    nation_obj["owner_id"] = player_id;
    nation_obj["nation"] = nation_id_to_qstring(nation_id);
    nations_array.append(nation_obj);
  }
  metadata["player_nations"] = nations_array;

  const auto& visibility = Game::Map::VisibilityService::instance();
  if (visibility.is_initialized()) {
    const auto snapshot = visibility.snapshot();
    const auto mask = Game::Map::explored_mask_from_cells(
        snapshot.cells, snapshot.width, snapshot.height);
    const QString encoded = Game::Map::encode_explored_mask(mask);
    if (!encoded.isEmpty()) {
      QJsonObject visibility_obj;
      visibility_obj["width"] = mask.width;
      visibility_obj["height"] = mask.height;
      visibility_obj["explored_rle"] = encoded;
      metadata["visibility"] = visibility_obj;
    }
  }

  return metadata;
}

void GameStateSerializer::restore_visibility_from_metadata(
    const QJsonObject& metadata) {
  if (!metadata.contains("visibility")) {
    return;
  }

  const auto visibility_obj = metadata.value("visibility").toObject();
  const int width = visibility_obj.value("width").toInt(0);
  const int height = visibility_obj.value("height").toInt(0);
  const QString encoded = visibility_obj.value("explored_rle").toString();

  const auto mask = Game::Map::decode_explored_mask(encoded, width, height);
  if (!mask.is_valid()) {
    qWarning() << "GameStateSerializer: saved exploration mask is unreadable; the "
                  "match restores with fog recomputed from unit sight";
    return;
  }

  auto& visibility = Game::Map::VisibilityService::instance();
  if (!visibility.restore_explored(mask.explored, mask.width, mask.height)) {
    qWarning() << "GameStateSerializer: saved exploration mask does not match the "
                  "restored map grid; ignoring it";
  }
}

void GameStateSerializer::restore_camera_from_metadata(const QJsonObject& metadata,
                                                       Render::GL::Camera* camera,
                                                       int viewport_width,
                                                       int viewport_height) {
  if (!metadata.contains("camera") || (camera == nullptr)) {
    return;
  }

  const auto camera_obj = metadata.value("camera").toObject();
  const QVector3D position = Game::JsonUtils::json_array_to_vec3(
      camera_obj.value("position"), camera->get_position());
  const QVector3D target = Game::JsonUtils::json_array_to_vec3(
      camera_obj.value("target"), camera->get_target());
  camera->look_at(position, target, QVector3D(0.0F, 1.0F, 0.0F));

  const float near_plane =
      static_cast<float>(camera_obj.value("near").toDouble(camera->get_near()));
  const float far_plane =
      static_cast<float>(camera_obj.value("far").toDouble(camera->get_far()));
  const float fov =
      static_cast<float>(camera_obj.value("fov").toDouble(camera->get_fov()));

  float aspect = camera->get_aspect();
  if (viewport_height > 0) {
    aspect = float(viewport_width) / float(std::max(1, viewport_height));
  }
  camera->set_perspective(fov, aspect, near_plane, far_plane);
}

void GameStateSerializer::restore_runtime_from_metadata(const QJsonObject& metadata,
                                                        RuntimeSnapshot& runtime) {
  if (!metadata.contains("runtime")) {
    return;
  }

  const auto runtime_obj = metadata.value("runtime").toObject();

  if (runtime_obj.contains("paused")) {
    runtime.paused = runtime_obj.value("paused").toBool(runtime.paused);
  }

  if (runtime_obj.contains("time_scale")) {
    runtime.time_scale = static_cast<float>(
        runtime_obj.value("time_scale").toDouble(runtime.time_scale));
  }

  if (runtime_obj.contains("victory_state")) {
    runtime.victory_state =
        runtime_obj.value("victory_state").toString(runtime.victory_state);
  }

  if (runtime_obj.contains("cursor_mode")) {
    runtime.cursor_mode = runtime_obj.value("cursor_mode").toInt(runtime.cursor_mode);
  }

  if (metadata.contains("local_owner_id")) {
    runtime.local_owner_id =
        metadata.value("local_owner_id").toInt(runtime.local_owner_id);
  }

  if (runtime_obj.contains("selected_player_id")) {
    runtime.selected_player_id =
        runtime_obj.value("selected_player_id").toInt(runtime.selected_player_id);
  }

  if (runtime_obj.contains("follow_selection")) {
    runtime.follow_selection =
        runtime_obj.value("follow_selection").toBool(runtime.follow_selection);
  }

  if (runtime_obj.contains("resources_by_owner")) {
    owner_resources_from_json(runtime_obj.value("resources_by_owner").toArray(),
                              runtime.resources_by_owner);
  }

  if (runtime_obj.contains("harvested_by_owner")) {
    owner_resources_from_json(runtime_obj.value("harvested_by_owner").toArray(),
                              runtime.harvested_by_owner);
  }

  if (runtime_obj.contains("simulation_tick")) {
    runtime.simulation_tick = static_cast<std::uint64_t>(
        runtime_obj.value("simulation_tick").toVariant().toULongLong());
  }

  if (runtime_obj.contains("rng_seed")) {
    runtime.rng_seed = static_cast<std::uint64_t>(
        runtime_obj.value("rng_seed").toVariant().toULongLong());
    runtime.rng_draw_count = static_cast<std::uint64_t>(
        runtime_obj.value("rng_draw_count").toVariant().toULongLong());
  }
}

void GameStateSerializer::restore_player_nations_from_metadata(
    const QJsonObject& metadata) {
  if (!metadata.contains("player_nations")) {
    return;
  }

  std::vector<std::pair<int, NationID>> assignments;
  const auto nations_array = metadata.value("player_nations").toArray();
  assignments.reserve(nations_array.size());
  for (const auto& value : nations_array) {
    const auto nation_obj = value.toObject();
    NationID nation_id{};
    if (!try_parse_nation_id(nation_obj.value("nation").toString(), nation_id)) {
      qWarning() << "Ignoring unknown nation in save:"
                 << nation_obj.value("nation").toString();
      continue;
    }
    assignments.emplace_back(nation_obj.value("owner_id").toInt(0), nation_id);
  }

  NationRegistry::instance().restore_player_nations(assignments);
}

void GameStateSerializer::restore_level_from_metadata(const QJsonObject& metadata,
                                                      LevelSnapshot& level) {
  const QString map_path = metadata.value("map_path").toString();
  if (!map_path.isEmpty()) {
    level.map_path = map_path;
  }

  if (metadata.contains("map_name")) {
    level.map_name = metadata.value("map_name").toString(level.map_name);
  }

  if (metadata.contains("player_unit_id")) {
    level.player_unit_id = static_cast<Engine::Core::EntityID>(
        metadata.value("player_unit_id").toVariant().toULongLong());
  }

  if (metadata.value("environment").isObject()) {
    const QJsonObject environment = metadata.value("environment").toObject();
    level.environment.start_time = Game::Map::normalize_hour(static_cast<float>(
        environment.value("start_time").toDouble(level.environment.start_time)));
    level.environment.time_mode = Game::Map::parse_time_mode(
        environment.value("time_mode")
            .toString(QString::fromLatin1(
                Game::Map::time_mode_name(level.environment.time_mode))));
    level.environment.day_length_seconds = std::max(
        1.0F,
        static_cast<float>(environment.value("day_length_seconds")
                               .toDouble(level.environment.day_length_seconds)));
    level.environment.lighting_profile =
        environment.value("lighting_profile")
            .toString(level.environment.lighting_profile);
    level.environment.fog_density_override =
        environment.contains("fog_density")
            ? std::max(
                  0.0F,
                  static_cast<float>(environment.value("fog_density").toDouble(0.0)))
            : -1.0F;
    level.environment.exposure_override =
        environment.contains("exposure")
            ? std::max(0.01F,
                       static_cast<float>(environment.value("exposure").toDouble(1.0)))
            : -1.0F;
    level.environment_clock.hour = Game::Map::normalize_hour(static_cast<float>(
        environment.value("current_time").toDouble(level.environment.start_time)));
    level.environment_clock.transition_active =
        environment.value("transition_active").toBool(false);
    level.environment_clock.transition_start_hour = Game::Map::normalize_hour(
        static_cast<float>(environment.value("transition_start_time")
                               .toDouble(level.environment_clock.hour)));
    level.environment_clock.transition_target_hour = Game::Map::normalize_hour(
        static_cast<float>(environment.value("transition_target_time")
                               .toDouble(level.environment_clock.hour)));
    level.environment_clock.transition_duration = std::max(
        0.0F,
        static_cast<float>(environment.value("transition_duration").toDouble(0.0)));
    level.environment_clock.transition_elapsed = std::clamp(
        static_cast<float>(environment.value("transition_elapsed").toDouble(0.0)),
        0.0F,
        level.environment_clock.transition_duration);
  }

  int max_troops = metadata.value("max_troops_per_player")
                       .toInt(metadata.value("game_max_troops_per_player")
                                  .toInt(level.max_troops_per_player));
  if (max_troops <= 0) {
    max_troops = Game::GameConfig::instance().get_max_troops_per_player();
  }
  level.max_troops_per_player = max_troops;
  Game::GameConfig::instance().set_max_troops_per_player(max_troops);
}

} // namespace Game::Systems
