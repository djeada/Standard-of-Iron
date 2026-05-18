#include "game_state_serializer.h"

#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qvectornd.h>

#include <algorithm>

#include "app/utils/json_vec_utils.h"
#include "game/game_config.h"
#include "game/map/terrain_service.h"
#include "render/gl/camera.h"

namespace Game::Systems {

namespace {

auto value_for_key(const QJsonObject& object,
                   const char* key,
                   const char* legacy_key = nullptr) -> QJsonValue {
  if (object.contains(key)) {
    return object.value(key);
  }
  if ((legacy_key != nullptr) && object.contains(legacy_key)) {
    return object.value(legacy_key);
  }
  return {};
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
    camera_obj["position"] = App::JsonUtils::vec3_to_json_array(camera->get_position());
    camera_obj["target"] = App::JsonUtils::vec3_to_json_array(camera->get_target());
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
  QJsonArray resources_array;
  for (const auto& row : runtime.resources_by_owner) {
    QJsonObject row_obj;
    row_obj["owner_id"] = row.owner_id;
    QJsonObject resources_obj;
    for (ResourceType const type : k_all_resource_types) {
      resources_obj[QLatin1String(resource_type_key(type))] = row.amounts.get(type);
    }
    row_obj["resources"] = resources_obj;
    resources_array.append(row_obj);
  }
  runtime_obj["resources_by_owner"] = resources_array;
  metadata["runtime"] = runtime_obj;

  return metadata;
}

void GameStateSerializer::restore_camera_from_metadata(const QJsonObject& metadata,
                                                       Render::GL::Camera* camera,
                                                       int viewport_width,
                                                       int viewport_height) {
  if (!metadata.contains("camera") || (camera == nullptr)) {
    return;
  }

  const auto camera_obj = metadata.value("camera").toObject();
  const QVector3D position = App::JsonUtils::json_array_to_vec3(
      camera_obj.value("position"), camera->get_position());
  const QVector3D target = App::JsonUtils::json_array_to_vec3(
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

  if (const auto victory_state =
          value_for_key(runtime_obj, "victory_state", "victoryState");
      !victory_state.isUndefined()) {
    runtime.victory_state = victory_state.toString(runtime.victory_state);
  }

  if (const auto cursor_value = value_for_key(runtime_obj, "cursor_mode", "cursorMode");
      !cursor_value.isUndefined()) {
    if (cursor_value.isDouble()) {
      runtime.cursor_mode = cursor_value.toInt(0);
    }
  }

  if (metadata.contains("local_owner_id")) {
    runtime.local_owner_id =
        metadata.value("local_owner_id").toInt(runtime.local_owner_id);
  }

  if (const auto selected_player_id =
          value_for_key(runtime_obj, "selected_player_id", "selectedPlayerId");
      !selected_player_id.isUndefined()) {
    runtime.selected_player_id = selected_player_id.toInt(runtime.selected_player_id);
  }

  if (const auto follow_selection =
          value_for_key(runtime_obj, "follow_selection", "followSelection");
      !follow_selection.isUndefined()) {
    runtime.follow_selection = follow_selection.toBool(runtime.follow_selection);
  }

  if (runtime_obj.contains("resources_by_owner")) {
    runtime.resources_by_owner.clear();
    const auto resources_array = runtime_obj.value("resources_by_owner").toArray();
    runtime.resources_by_owner.reserve(resources_array.size());
    for (const auto& value : resources_array) {
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
      runtime.resources_by_owner.push_back(row);
    }
  } else if (runtime_obj.contains("wood_by_owner")) {
    runtime.resources_by_owner.clear();
    const auto wood_array = runtime_obj.value("wood_by_owner").toArray();
    runtime.resources_by_owner.reserve(wood_array.size());
    for (const auto& value : wood_array) {
      const auto wood_obj = value.toObject();
      OwnerResourceState row;
      row.owner_id = wood_obj.value("owner_id").toInt(0);
      row.amounts.set(ResourceType::Wood, wood_obj.value("wood").toInt(0));
      runtime.resources_by_owner.push_back(row);
    }
  }
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

  auto max_troops_value = value_for_key(metadata, "max_troops_per_player");
  if (max_troops_value.isUndefined()) {
    max_troops_value =
        value_for_key(metadata, "game_max_troops_per_player", "gameMaxTroopsPerPlayer");
  }
  int max_troops = max_troops_value.isUndefined()
                       ? level.max_troops_per_player
                       : max_troops_value.toInt(level.max_troops_per_player);
  if (max_troops <= 0) {
    max_troops = Game::GameConfig::instance().get_max_troops_per_player();
  }
  level.max_troops_per_player = max_troops;
  Game::GameConfig::instance().set_max_troops_per_player(max_troops);
}

} // namespace Game::Systems
