#include "game_state_serializer.h"

#include "app/utils/json_vec_utils.h"
#include "game/game_config.h"
#include "game/map/terrain_service.h"
#include "render/gl/camera.h"

namespace Game {
namespace Systems {

QJsonObject GameStateSerializer::buildMetadata(
    const Engine::Core::World &world, const Render::GL::Camera *camera,
    const LevelSnapshot &level, const RuntimeSnapshot &runtime) {

  QJsonObject metadata;
  metadata["mapPath"] = level.mapPath;
  metadata["mapName"] = level.mapName;
  metadata["maxTroopsPerPlayer"] = level.maxTroopsPerPlayer;
  metadata["localOwnerId"] = runtime.localOwnerId;
  metadata["playerUnitId"] = static_cast<qint64>(level.playerUnitId);

  metadata["gameMaxTroopsPerPlayer"] =
      Game::GameConfig::instance().getMaxTroopsPerPlayer();

  const auto &terrainService = Game::Map::TerrainService::instance();
  if (const auto *heightMap = terrainService.getHeightMap()) {
    metadata["gridWidth"] = heightMap->getWidth();
    metadata["gridHeight"] = heightMap->getHeight();
    metadata["tileSize"] = heightMap->getTileSize();
  }

  if (camera) {
    QJsonObject cameraObj;
    cameraObj["position"] =
        App::JsonUtils::vec3ToJsonArray(camera->getPosition());
    cameraObj["target"] = App::JsonUtils::vec3ToJsonArray(camera->getTarget());
    cameraObj["distance"] = camera->getDistance();
    cameraObj["pitchDeg"] = camera->getPitchDeg();
    cameraObj["fov"] = camera->getFOV();
    cameraObj["near"] = camera->getNear();
    cameraObj["far"] = camera->getFar();
    metadata["camera"] = cameraObj;
  }

  QJsonObject runtimeObj;
  runtimeObj["paused"] = runtime.paused;
  runtimeObj["timeScale"] = runtime.timeScale;
  runtimeObj["victoryState"] = runtime.victoryState;
  runtimeObj["cursorMode"] = runtime.cursorMode;
  runtimeObj["selectedPlayerId"] = runtime.selectedPlayerId;
  runtimeObj["followSelection"] = runtime.followSelection;
  metadata["runtime"] = runtimeObj;

  return metadata;
}

void GameStateSerializer::restoreCameraFromMetadata(
    const QJsonObject &metadata, Render::GL::Camera *camera, int viewportWidth,
    int viewportHeight) {
  if (!metadata.contains("camera") || !camera) {
    return;
  }

  const auto cameraObj = metadata.value("camera").toObject();
  const QVector3D position = App::JsonUtils::jsonArrayToVec3(
      cameraObj.value("position"), camera->getPosition());
  const QVector3D target = App::JsonUtils::jsonArrayToVec3(
      cameraObj.value("target"), camera->getTarget());
  camera->lookAt(position, target, QVector3D(0.0f, 1.0f, 0.0f));

  const float nearPlane = static_cast<float>(
      cameraObj.value("near").toDouble(camera->getNear()));
  const float farPlane =
      static_cast<float>(cameraObj.value("far").toDouble(camera->getFar()));
  const float fov =
      static_cast<float>(cameraObj.value("fov").toDouble(camera->getFOV()));

  float aspect = camera->getAspect();
  if (viewportHeight > 0) {
    aspect = float(viewportWidth) / float(std::max(1, viewportHeight));
  }
  camera->setPerspective(fov, aspect, nearPlane, farPlane);
}

void GameStateSerializer::restoreRuntimeFromMetadata(
    const QJsonObject &metadata, RuntimeSnapshot &runtime) {
  if (!metadata.contains("runtime")) {
    return;
  }

  const auto runtimeObj = metadata.value("runtime").toObject();

  if (runtimeObj.contains("paused")) {
    runtime.paused = runtimeObj.value("paused").toBool(runtime.paused);
  }

  if (runtimeObj.contains("timeScale")) {
    runtime.timeScale = static_cast<float>(
        runtimeObj.value("timeScale").toDouble(runtime.timeScale));
  }

  runtime.victoryState =
      runtimeObj.value("victoryState").toString(runtime.victoryState);

  if (runtimeObj.contains("cursorMode")) {
    const auto cursorValue = runtimeObj.value("cursorMode");
    if (cursorValue.isDouble()) {
      runtime.cursorMode = cursorValue.toInt(0);
    }
  }

  if (metadata.contains("localOwnerId")) {
    runtime.localOwnerId =
        metadata.value("localOwnerId").toInt(runtime.localOwnerId);
  }

  runtime.selectedPlayerId =
      runtimeObj.value("selectedPlayerId").toInt(runtime.selectedPlayerId);

  runtime.followSelection =
      runtimeObj.value("followSelection").toBool(runtime.followSelection);
}

void GameStateSerializer::restoreLevelFromMetadata(const QJsonObject &metadata,
                                                   LevelSnapshot &level) {
  const QString mapPath = metadata.value("mapPath").toString();
  if (!mapPath.isEmpty()) {
    level.mapPath = mapPath;
  }

  if (metadata.contains("mapName")) {
    level.mapName = metadata.value("mapName").toString(level.mapName);
  }

  if (metadata.contains("playerUnitId")) {
    level.playerUnitId = static_cast<Engine::Core::EntityID>(
        metadata.value("playerUnitId").toVariant().toULongLong());
  }

  int maxTroops =
      metadata.value("maxTroopsPerPlayer").toInt(level.maxTroopsPerPlayer);
  if (maxTroops <= 0) {
    maxTroops = Game::GameConfig::instance().getMaxTroopsPerPlayer();
  }
  level.maxTroopsPerPlayer = maxTroops;
  Game::GameConfig::instance().setMaxTroopsPerPlayer(maxTroops);
}

} // namespace Systems
} // namespace Game
