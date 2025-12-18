#include "game_state_serializer.h"

#include "app/utils/json_vec_utils.h"
#include "game/game_config.h"
#include "game/map/terrain_service.h"
#include "render/gl/camera.h"
#include <algorithm>
#include <qglobal.h>
#include <qjsonobject.h>
#include <qvectornd.h>

namespace Game::Systems {

auto GameStateSerializer::build_metadata(
    const Engine::Core::World &, const Render::GL::Camera *camera,
    const LevelSnapshot &level, const RuntimeSnapshot &runtime) -> QJsonObject {

  QJsonObject metadata;
  metadata["map_path"] = level.map_path;
  metadata["map_name"] = level.map_name;
  metadata["max_troops_per_player"] = level.max_troops_per_player;
  metadata["local_owner_id"] = runtime.local_owner_id;
  metadata["player_unit_id"] = static_cast<qint64>(level.player_unit_id);

  metadata["gameMaxTroopsPerPlayer"] =
      Game::GameConfig::instance().get_max_troops_per_player();

  const auto &terrain_service = Game::Map::TerrainService::instance();
  if (const auto *height_map = terrain_service.get_height_map()) {
    metadata["grid_width"] = height_map->getWidth();
    metadata["grid_height"] = height_map->getHeight();
    metadata["tile_size"] = height_map->getTileSize();
  }

  if (camera != nullptr) {
    QJsonObject camera_obj;
    camera_obj["position"] =
        App::JsonUtils::vec3ToJsonArray(camera->get_position());
    camera_obj["target"] =
        App::JsonUtils::vec3ToJsonArray(camera->get_target());
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
  runtime_obj["victoryState"] = runtime.victory_state;
  runtime_obj["cursorMode"] = runtime.cursor_mode;
  runtime_obj["selectedPlayerId"] = runtime.selected_player_id;
  runtime_obj["followSelection"] = runtime.follow_selection;
  metadata["runtime"] = runtime_obj;

  return metadata;
}

void GameStateSerializer::restore_camera_from_metadata(const QJsonObject &metadata,
                                                    Render::GL::Camera *camera,
                                                    int viewport_width,
                                                    int viewport_height) {
  if (!metadata.contains("camera") || (camera == nullptr)) {
    return;
  }

  const auto camera_obj = metadata.value("camera").toObject();
  const QVector3D position = App::JsonUtils::jsonArrayToVec3(
      camera_obj.value("position"), camera->get_position());
  const QVector3D target = App::JsonUtils::jsonArrayToVec3(
      camera_obj.value("target"), camera->get_target());
  camera->lookAt(position, target, QVector3D(0.0F, 1.0F, 0.0F));

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

void GameStateSerializer::restore_runtime_from_metadata(
    const QJsonObject &metadata, RuntimeSnapshot &runtime) {
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

  runtime.victory_state =
      runtime_obj.value("victoryState").toString(runtime.victory_state);

  if (runtime_obj.contains("cursorMode")) {
    const auto cursor_value = runtime_obj.value("cursorMode");
    if (cursor_value.isDouble()) {
      runtime.cursor_mode = cursor_value.toInt(0);
    }
  }

  if (metadata.contains("local_owner_id")) {
    runtime.local_owner_id =
        metadata.value("local_owner_id").toInt(runtime.local_owner_id);
  }

  runtime.selected_player_id =
      runtime_obj.value("selectedPlayerId").toInt(runtime.selected_player_id);

  runtime.follow_selection =
      runtime_obj.value("followSelection").toBool(runtime.follow_selection);
}

void GameStateSerializer::restore_level_from_metadata(const QJsonObject &metadata,
                                                   LevelSnapshot &level) {
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

  int max_troops = metadata.value("max_troops_per_player")
                       .toInt(level.max_troops_per_player);
  if (max_troops <= 0) {
    max_troops = Game::GameConfig::instance().get_max_troops_per_player();
  }
  level.max_troops_per_player = max_troops;
  Game::GameConfig::instance().set_max_troops_per_player(max_troops);
}

} // namespace Game::Systems
