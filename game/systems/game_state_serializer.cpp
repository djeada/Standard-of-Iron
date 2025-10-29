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

auto GameStateSerializer::buildMetadata(
    const Engine::Core::World &, const Render::GL::Camera *camera,
    const LevelSnapshot &level, const RuntimeSnapshot &runtime) -> QJsonObject {

  QJsonObject metadata;
  metadata["map_path"] = level.map_path;
  metadata["map_name"] = level.map_name;
  metadata["max_troops_per_player"] = level.max_troops_per_player;
  metadata["localOwnerId"] = runtime.localOwnerId;
  metadata["playerUnitId"] = static_cast<qint64>(level.playerUnitId);

  metadata["gameMaxTroopsPerPlayer"] =
      Game::GameConfig::instance().getMaxTroopsPerPlayer();

  const auto &terrain_service = Game::Map::TerrainService::instance();
  if (const auto *height_map = terrain_service.getHeightMap()) {
    metadata["grid_width"] = height_map->getWidth();
    metadata["grid_height"] = height_map->getHeight();
    metadata["tile_size"] = height_map->getTileSize();
  }

  if (camera != nullptr) {
    QJsonObject camera_obj;
    camera_obj["position"] =
        App::JsonUtils::vec3ToJsonArray(camera->getPosition());
    camera_obj["target"] = App::JsonUtils::vec3ToJsonArray(camera->getTarget());
    camera_obj["distance"] = camera->getDistance();
    camera_obj["pitch_deg"] = camera->getPitchDeg();
    camera_obj["fov"] = camera->getFOV();
    camera_obj["near"] = camera->getNear();
    camera_obj["far"] = camera->getFar();
    metadata["camera"] = camera_obj;
  }

  QJsonObject runtime_obj;
  runtime_obj["paused"] = runtime.paused;
  runtime_obj["timeScale"] = runtime.timeScale;
  runtime_obj["victoryState"] = runtime.victoryState;
  runtime_obj["cursorMode"] = runtime.cursorMode;
  runtime_obj["selectedPlayerId"] = runtime.selectedPlayerId;
  runtime_obj["followSelection"] = runtime.followSelection;
  metadata["runtime"] = runtime_obj;

  return metadata;
}

void GameStateSerializer::restoreCameraFromMetadata(const QJsonObject &metadata,
                                                    Render::GL::Camera *camera,
                                                    int viewportWidth,
                                                    int viewportHeight) {
  if (!metadata.contains("camera") || (camera == nullptr)) {
    return;
  }

  const auto camera_obj = metadata.value("camera").toObject();
  const QVector3D position = App::JsonUtils::jsonArrayToVec3(
      camera_obj.value("position"), camera->getPosition());
  const QVector3D target = App::JsonUtils::jsonArrayToVec3(
      camera_obj.value("target"), camera->getTarget());
  camera->lookAt(position, target, QVector3D(0.0F, 1.0F, 0.0F));

  const float near_plane =
      static_cast<float>(camera_obj.value("near").toDouble(camera->getNear()));
  const float far_plane =
      static_cast<float>(camera_obj.value("far").toDouble(camera->getFar()));
  const float fov =
      static_cast<float>(camera_obj.value("fov").toDouble(camera->getFOV()));

  float aspect = camera->getAspect();
  if (viewportHeight > 0) {
    aspect = float(viewportWidth) / float(std::max(1, viewportHeight));
  }
  camera->setPerspective(fov, aspect, near_plane, far_plane);
}

void GameStateSerializer::restoreRuntimeFromMetadata(
    const QJsonObject &metadata, RuntimeSnapshot &runtime) {
  if (!metadata.contains("runtime")) {
    return;
  }

  const auto runtime_obj = metadata.value("runtime").toObject();

  if (runtime_obj.contains("paused")) {
    runtime.paused = runtime_obj.value("paused").toBool(runtime.paused);
  }

  if (runtime_obj.contains("timeScale")) {
    runtime.timeScale = static_cast<float>(
        runtime_obj.value("timeScale").toDouble(runtime.timeScale));
  }

  runtime.victoryState =
      runtime_obj.value("victoryState").toString(runtime.victoryState);

  if (runtime_obj.contains("cursorMode")) {
    const auto cursor_value = runtime_obj.value("cursorMode");
    if (cursor_value.isDouble()) {
      runtime.cursorMode = cursor_value.toInt(0);
    }
  }

  if (metadata.contains("localOwnerId")) {
    runtime.localOwnerId =
        metadata.value("localOwnerId").toInt(runtime.localOwnerId);
  }

  runtime.selectedPlayerId =
      runtime_obj.value("selectedPlayerId").toInt(runtime.selectedPlayerId);

  runtime.followSelection =
      runtime_obj.value("followSelection").toBool(runtime.followSelection);
}

void GameStateSerializer::restoreLevelFromMetadata(const QJsonObject &metadata,
                                                   LevelSnapshot &level) {
  const QString map_path = metadata.value("map_path").toString();
  if (!map_path.isEmpty()) {
    level.map_path = map_path;
  }

  if (metadata.contains("map_name")) {
    level.map_name = metadata.value("map_name").toString(level.map_name);
  }

  if (metadata.contains("playerUnitId")) {
    level.playerUnitId = static_cast<Engine::Core::EntityID>(
        metadata.value("playerUnitId").toVariant().toULongLong());
  }

  int max_troops = metadata.value("max_troops_per_player")
                       .toInt(level.max_troops_per_player);
  if (max_troops <= 0) {
    max_troops = Game::GameConfig::instance().getMaxTroopsPerPlayer();
  }
  level.max_troops_per_player = max_troops;
  Game::GameConfig::instance().setMaxTroopsPerPlayer(max_troops);
}

} // namespace Game::Systems
