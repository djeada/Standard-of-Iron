#include "level_loader.h"
#include "map_loader.h"
#include "map_transformer.h"
#include "environment.h"
#include "../visuals/visual_catalog.h"
#include "../units/factory.h"
#include "../core/world.h"
#include "../core/component.h"
#include "../../render/gl/renderer.h"
#include "../../render/gl/camera.h"
#include <QDebug>

namespace Game { namespace Map {

LevelLoadResult LevelLoader::loadFromAssets(const QString& mapPath,
                                           Engine::Core::World& world,
                                           Render::GL::Renderer& renderer,
                                           Render::GL::Camera& camera) {
    LevelLoadResult res;

    // Load visuals JSON
    Game::Visuals::VisualCatalog visualCatalog;
    QString visualsErr;
    visualCatalog.loadFromJsonFile("assets/visuals/unit_visuals.json", &visualsErr);

    // Install unit factories
    auto unitReg = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::registerBuiltInUnits(*unitReg);
    Game::Map::MapTransformer::setFactoryRegistry(unitReg);

    // Try load map JSON
    Game::Map::MapDefinition def;
    QString err;
    if (Game::Map::MapLoader::loadFromJsonFile(mapPath, def, &err)) {
        res.ok = true;
        res.mapName = def.name;
        // Apply environment
        Game::Map::Environment::apply(def, renderer, camera);
        res.camFov = def.camera.fovY; res.camNear = def.camera.nearPlane; res.camFar = def.camera.farPlane;
        // Populate world
        auto rt = Game::Map::MapTransformer::applyToWorld(def, world, &visualCatalog);
        if (!rt.unitIds.empty()) {
            res.playerUnitId = rt.unitIds.front();
        } else {
            // Fallback: try to spawn archer at origin using registry
            auto reg = Game::Map::MapTransformer::getFactoryRegistry();
            if (reg) {
                Game::Units::SpawnParams sp; sp.position = QVector3D(0.0f, 0.0f, 0.0f); sp.playerId = 0; sp.unitType = "archer";
                if (auto unit = reg->create("archer", world, sp)) {
                    res.playerUnitId = unit->id();
                } else {
                    qWarning() << "LevelLoader: Fallback archer spawn failed";
                }
            }
        }
        // Ensure there's at least one barracks for local player near origin
        bool hasBarracks = false;
        for (auto* e : world.getEntitiesWith<Engine::Core::UnitComponent>()) {
            if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) {
                if (u->unitType == "barracks" && u->ownerId == 1) { hasBarracks = true; break; }
            }
        }
        if (!hasBarracks) {
            auto reg2 = Game::Map::MapTransformer::getFactoryRegistry();
            if (reg2) {
                Game::Units::SpawnParams sp; sp.position = QVector3D(-4.0f, 0.0f, -3.0f); sp.playerId = 1; sp.unitType = "barracks";
                reg2->create("barracks", world, sp);
            }
        }
    } else {
        qWarning() << "LevelLoader: Map load failed:" << err << " - applying default environment";
        Game::Map::Environment::applyDefault(renderer, camera);
        res.ok = false;
        res.camFov = camera.getFOV(); res.camNear = camera.getNear(); res.camFar = camera.getFar();
        // Fallback archer spawn as last resort
        auto reg = Game::Map::MapTransformer::getFactoryRegistry();
        if (reg) {
            Game::Units::SpawnParams sp; sp.position = QVector3D(0.0f, 0.0f, 0.0f); sp.playerId = 0; sp.unitType = "archer";
            if (auto unit = reg->create("archer", world, sp)) { res.playerUnitId = unit->id(); }
        }
    }

    return res;
}

} } // namespace Game::Map
