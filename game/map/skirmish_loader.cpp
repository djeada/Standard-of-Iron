#include "skirmish_loader.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/level_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/owner_registry.h"
#include "game/systems/selection_system.h"
#include "game/visuals/team_colors.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_renderer.h"
#include "render/scene_renderer.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <algorithm>
#include <set>
#include <unordered_map>

namespace Game {
namespace Map {

SkirmishLoader::SkirmishLoader(Engine::Core::World &world,
                              Render::GL::Renderer &renderer,
                              Render::GL::Camera &camera)
    : m_world(world), m_renderer(renderer), m_camera(camera) {}

SkirmishLoadResult SkirmishLoader::start(const QString &mapPath,
                                        const QVariantList &playerConfigs,
                                        int selectedPlayerId,
                                        int &outSelectedPlayerId) {
  SkirmishLoadResult result;

  if (auto *selectionSystem =
          m_world.getSystem<Game::Systems::SelectionSystem>()) {
    selectionSystem->clearSelection();
  }

  m_renderer.pause();
  m_renderer.lockWorldForModification();
  m_renderer.setSelectedEntities({});
  m_renderer.setHoveredEntityId(0);

  m_world.clear();

  Game::Systems::BuildingCollisionRegistry::instance().clear();

  QSet<int> mapPlayerIds;
  QFile mapFile(mapPath);
  if (mapFile.open(QIODevice::ReadOnly)) {
    QByteArray data = mapFile.readAll();
    mapFile.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.contains("spawns") && obj["spawns"].isArray()) {
        QJsonArray spawns = obj["spawns"].toArray();
        for (const QJsonValue &spawnVal : spawns) {
          if (spawnVal.isObject()) {
            QJsonObject spawn = spawnVal.toObject();
            if (spawn.contains("playerId")) {
              int playerId = spawn["playerId"].toInt();
              if (playerId > 0) {
                mapPlayerIds.insert(playerId);
              }
            }
          }
        }
      }
    }
  } else {
    qWarning() << "Could not open map file for reading player IDs:" << mapPath;
  }

  auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();
  ownerRegistry.clear();

  int playerOwnerId = selectedPlayerId;

  if (!mapPlayerIds.contains(playerOwnerId)) {
    if (!mapPlayerIds.isEmpty()) {
      QList<int> sortedIds = mapPlayerIds.values();
      std::sort(sortedIds.begin(), sortedIds.end());
      playerOwnerId = sortedIds.first();
      qWarning() << "Selected player ID" << selectedPlayerId
                 << "not found in map spawns. Using" << playerOwnerId
                 << "instead.";
      outSelectedPlayerId = playerOwnerId;
    } else {
      qWarning() << "No valid player spawns found in map. Using default "
                    "player ID"
                 << playerOwnerId;
    }
  }

  ownerRegistry.setLocalPlayerId(playerOwnerId);

  std::unordered_map<int, int> teamOverrides;
  QVariantList savedPlayerConfigs;
  std::set<int> processedPlayerIds;

  if (!playerConfigs.isEmpty()) {

    for (const QVariant &configVar : playerConfigs) {
      QVariantMap config = configVar.toMap();
      int playerId = config.value("playerId", -1).toInt();
      int teamId = config.value("teamId", 0).toInt();
      QString colorHex = config.value("colorHex", "#FFFFFF").toString();
      bool isHuman = config.value("isHuman", false).toBool();

      if (isHuman && playerId != playerOwnerId) {
        playerId = playerOwnerId;
      }

      if (processedPlayerIds.count(playerId) > 0) {
        continue;
      }

      if (playerId >= 0) {
        processedPlayerIds.insert(playerId);
        teamOverrides[playerId] = teamId;

        QVariantMap updatedConfig = config;
        updatedConfig["playerId"] = playerId;
        savedPlayerConfigs.append(updatedConfig);
      }
    }
  }

  Game::Map::MapTransformer::setLocalOwnerId(playerOwnerId);
  Game::Map::MapTransformer::setPlayerTeamOverrides(teamOverrides);

  auto lr = Game::Map::LevelLoader::loadFromAssets(mapPath, m_world,
                                                   m_renderer, m_camera);

  if (!lr.ok && !lr.errorMessage.isEmpty()) {
    result.errorMessage = lr.errorMessage;
    m_renderer.unlockWorldForModification();
    m_renderer.resume();
    return result;
  }

  if (!savedPlayerConfigs.isEmpty()) {
    for (const QVariant &configVar : savedPlayerConfigs) {
      QVariantMap config = configVar.toMap();
      int playerId = config.value("playerId", -1).toInt();
      QString colorHex = config.value("colorHex", "#FFFFFF").toString();

      if (playerId >= 0 && colorHex.startsWith("#") &&
          colorHex.length() == 7) {
        bool ok;
        int r = colorHex.mid(1, 2).toInt(&ok, 16);
        int g = colorHex.mid(3, 2).toInt(&ok, 16);
        int b = colorHex.mid(5, 2).toInt(&ok, 16);
        ownerRegistry.setOwnerColor(playerId, r / 255.0f, g / 255.0f,
                                    b / 255.0f);
      }
    }

    auto entities = m_world.getEntitiesWith<Engine::Core::UnitComponent>();
    std::unordered_map<int, int> ownerEntityCount;
    for (auto *entity : entities) {
      auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
      auto *renderable =
          entity->getComponent<Engine::Core::RenderableComponent>();
      if (unit && renderable) {
        QVector3D tc = Game::Visuals::teamColorForOwner(unit->ownerId);
        renderable->color[0] = tc.x();
        renderable->color[1] = tc.y();
        renderable->color[2] = tc.z();
        ownerEntityCount[unit->ownerId]++;
      }
    }
  }
  
  if (m_onOwnersUpdated) {
    m_onOwnersUpdated();
  }

  auto &terrainService = Game::Map::TerrainService::instance();

  if (m_ground) {
    if (lr.ok)
      m_ground->configure(lr.tileSize, lr.gridWidth, lr.gridHeight);
    else
      m_ground->configureExtent(50.0f);
    if (terrainService.isInitialized())
      m_ground->setBiome(terrainService.biomeSettings());
  }

  if (m_terrain) {
    if (terrainService.isInitialized() && terrainService.getHeightMap()) {
      m_terrain->configure(*terrainService.getHeightMap(),
                           terrainService.biomeSettings());
    }
  }

  if (m_biome) {
    if (terrainService.isInitialized() && terrainService.getHeightMap()) {
      m_biome->configure(*terrainService.getHeightMap(),
                         terrainService.biomeSettings());
    }
  }

  if (m_stone) {
    if (terrainService.isInitialized() && terrainService.getHeightMap()) {
      m_stone->configure(*terrainService.getHeightMap(),
                         terrainService.biomeSettings());
    }
  }

  int mapWidth = lr.ok ? lr.gridWidth : 100;
  int mapHeight = lr.ok ? lr.gridHeight : 100;
  Game::Systems::CommandService::initialize(mapWidth, mapHeight);

  auto &visibilityService = Game::Map::VisibilityService::instance();
  visibilityService.initialize(mapWidth, mapHeight, lr.tileSize);
  visibilityService.computeImmediate(m_world, playerOwnerId);
  
  if (m_fog && visibilityService.isInitialized()) {
    m_fog->updateMask(
        visibilityService.getWidth(), visibilityService.getHeight(),
        visibilityService.getTileSize(), visibilityService.snapshotCells());
    
    if (m_onVisibilityMaskReady) {
      m_onVisibilityMaskReady();
    }
  }

  if (m_biome) {
    m_biome->refreshGrass();
  }

  m_renderer.unlockWorldForModification();
  m_renderer.resume();

  Engine::Core::Entity *focusEntity = nullptr;

  auto candidates = m_world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : candidates) {
    if (!e)
      continue;
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u)
      continue;
    if (u->unitType == "barracks" && u->ownerId == playerOwnerId &&
        u->health > 0) {
      focusEntity = e;
      break;
    }
  }

  if (!focusEntity && lr.playerUnitId != 0) {
    focusEntity = m_world.getEntity(lr.playerUnitId);
  }

  if (focusEntity) {
    if (auto *t =
            focusEntity->getComponent<Engine::Core::TransformComponent>()) {
      result.focusPosition = QVector3D(t->position.x, t->position.y, t->position.z);
      result.hasFocusPosition = true;
    }
  }

  result.ok = true;
  result.mapName = lr.mapName;
  result.playerUnitId = lr.playerUnitId;
  result.camFov = lr.camFov;
  result.camNear = lr.camNear;
  result.camFar = lr.camFar;
  result.gridWidth = lr.gridWidth;
  result.gridHeight = lr.gridHeight;
  result.tileSize = lr.tileSize;
  result.maxTroopsPerPlayer = lr.maxTroopsPerPlayer;
  result.victoryConfig = lr.victoryConfig;

  return result;
}

}
}
