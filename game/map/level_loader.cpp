#include "level_loader.h"
#include "../../render/gl/camera.h"
#include "../../render/scene_renderer.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../systems/owner_registry.h"
#include "../units/factory.h"
#include "../visuals/visual_catalog.h"
#include "environment.h"
#include "map_definition.h"
#include "map_loader.h"
#include "map_transformer.h"
#include "terrain_service.h"
#include <QDebug>

namespace Game {
namespace Map {

LevelLoadResult LevelLoader::loadFromAssets(const QString &mapPath,
                                            Engine::Core::World &world,
                                            Render::GL::Renderer &renderer,
                                            Render::GL::Camera &camera) {
  LevelLoadResult res;

  Game::Visuals::VisualCatalog visualCatalog;
  QString visualsErr;
  if (!visualCatalog.loadFromJsonFile("assets/visuals/unit_visuals.json",
                                      &visualsErr)) {
    res.ok = false;
    res.errorMessage =
        QString("Failed to load visual catalog: %1").arg(visualsErr);
    qWarning() << res.errorMessage;
    return res;
  }

  auto unitReg = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::registerBuiltInUnits(*unitReg);
  Game::Map::MapTransformer::setFactoryRegistry(unitReg);

  Game::Map::MapDefinition def;
  QString err;
  if (Game::Map::MapLoader::loadFromJsonFile(mapPath, def, &err)) {
    res.ok = true;
    res.mapName = def.name;

    Game::Map::TerrainService::instance().initialize(def);

    Game::Map::Environment::apply(def, renderer, camera);
    res.camFov = def.camera.fovY;
    res.camNear = def.camera.nearPlane;
    res.camFar = def.camera.farPlane;
    res.gridWidth = def.grid.width;
    res.gridHeight = def.grid.height;
    res.tileSize = def.grid.tileSize;
    res.maxTroopsPerPlayer = def.maxTroopsPerPlayer;
    res.victoryConfig = def.victory;

    auto rt =
        Game::Map::MapTransformer::applyToWorld(def, world, &visualCatalog);
    if (!rt.unitIds.empty()) {
      res.playerUnitId = rt.unitIds.front();
    } else {

      auto reg = Game::Map::MapTransformer::getFactoryRegistry();
      if (reg) {
        Game::Units::SpawnParams sp;
        sp.position = QVector3D(0.0f, 0.0f, 0.0f);
        sp.playerId = 0;
        sp.unitType = "archer";
        sp.aiControlled =
            !Game::Systems::OwnerRegistry::instance().isPlayer(sp.playerId);
        if (auto unit = reg->create("archer", world, sp)) {
          res.playerUnitId = unit->id();
        } else {
          qWarning() << "LevelLoader: Fallback archer spawn failed";
        }
      }
    }

    bool hasBarracks = false;
    for (auto *e : world.getEntitiesWith<Engine::Core::UnitComponent>()) {
      if (auto *u = e->getComponent<Engine::Core::UnitComponent>()) {
        if (u->unitType == "barracks" &&
            Game::Systems::OwnerRegistry::instance().isPlayer(u->ownerId)) {
          hasBarracks = true;
          break;
        }
      }
    }
    if (!hasBarracks) {
      auto reg2 = Game::Map::MapTransformer::getFactoryRegistry();
      if (reg2) {
        Game::Units::SpawnParams sp;
        sp.position = QVector3D(-4.0f, 0.0f, -3.0f);
        sp.playerId =
            Game::Systems::OwnerRegistry::instance().getLocalPlayerId();
        sp.unitType = "barracks";
        sp.aiControlled =
            !Game::Systems::OwnerRegistry::instance().isPlayer(sp.playerId);
        reg2->create("barracks", world, sp);
      }
    }
  } else {
    res.ok = false;
    res.errorMessage = QString("Map load failed: %1").arg(err);
    qWarning() << "LevelLoader: Map load failed:" << err
               << " - applying default environment";
    Game::Map::Environment::applyDefault(renderer, camera);
    res.ok = false;
    res.camFov = camera.getFOV();
    res.camNear = camera.getNear();
    res.camFar = camera.getFar();
    res.gridWidth = 50;
    res.gridHeight = 50;
    res.tileSize = 1.0f;

    auto reg = Game::Map::MapTransformer::getFactoryRegistry();
    if (reg) {
      Game::Units::SpawnParams sp;
      sp.position = QVector3D(0.0f, 0.0f, 0.0f);
      sp.playerId = 0;
      sp.unitType = "archer";
      sp.aiControlled =
          !Game::Systems::OwnerRegistry::instance().isPlayer(sp.playerId);
      if (auto unit = reg->create("archer", world, sp)) {
        res.playerUnitId = unit->id();
      }
    }
  }

  return res;
}

} 
} 
