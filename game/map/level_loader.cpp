#include "level_loader.h"
#include "../../render/gl/camera.h"
#include "../../render/scene_renderer.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../systems/nation_registry.h"
#include "../systems/owner_registry.h"
#include "../units/factory.h"
#include "../visuals/visual_catalog.h"
#include "environment.h"
#include "map_definition.h"
#include "map_loader.h"
#include "map_transformer.h"
#include "terrain_service.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"
#include "units/unit.h"
#include "utils/resource_utils.h"
#include <QDebug>
#include <QFile>
#include <memory>
#include <qglobal.h>
#include <qstringliteral.h>

namespace Game::Map {

auto LevelLoader::loadFromAssets(
    const QString &map_path, Engine::Core::World &world,
    Render::GL::Renderer &renderer,
    Render::GL::Camera &camera) -> LevelLoadResult {
  LevelLoadResult res;

  auto &owners = Game::Systems::OwnerRegistry::instance();

  Game::Visuals::VisualCatalog visual_catalog;
  const QString visuals_path = Utils::Resources::resolveResourcePath(
      QStringLiteral(":/assets/visuals/unit_visuals.json"));
  bool visuals_loaded = false;
  if (QFile::exists(visuals_path)) {
    QString visuals_err;
    visuals_loaded =
        visual_catalog.loadFromJsonFile(visuals_path, &visuals_err);
    if (!visuals_loaded && !visuals_err.isEmpty()) {
      qWarning() << "LevelLoader: Visual catalog parse failed:" << visuals_err;
    }
  } else {
    qInfo() << "LevelLoader: unit visuals catalog not found at" << visuals_path
            << "- continuing without overrides.";
  }

  auto unit_reg = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::registerBuiltInUnits(*unit_reg);
  Game::Map::MapTransformer::setFactoryRegistry(unit_reg);

  const QString resolved_map_path =
      Utils::Resources::resolveResourcePath(map_path);

  Game::Map::MapDefinition def;
  QString err;
  if (Game::Map::MapLoader::loadFromJsonFile(resolved_map_path, def, &err)) {
    res.ok = true;
    res.map_name = def.name;

    Game::Map::TerrainService::instance().initialize(def);

    Game::Map::Environment::apply(def, renderer, camera);
    res.cam_fov = def.camera.fovY;
    res.cam_near = def.camera.near_plane;
    res.cam_far = def.camera.far_plane;
    res.grid_width = def.grid.width;
    res.grid_height = def.grid.height;
    res.tile_size = def.grid.tile_size;
    res.max_troops_per_player = def.max_troops_per_player;
    res.victoryConfig = def.victory;

    const Game::Visuals::VisualCatalog *catalog_ptr =
        visuals_loaded ? &visual_catalog : nullptr;
    auto rt =
        Game::Map::MapTransformer::apply_to_world(def, world, catalog_ptr);
    if (!rt.unit_ids.empty()) {
      res.player_unit_id = rt.unit_ids.front();
    } else {

      auto &nationRegistry = Game::Systems::NationRegistry::instance();
      auto reg = Game::Map::MapTransformer::getFactoryRegistry();
      if (reg) {
        Game::Units::SpawnParams sp;
        sp.position = QVector3D(0.0F, 0.0F, 0.0F);
        sp.player_id = 0;
        sp.spawn_type = Game::Units::SpawnType::Archer;
        sp.ai_controlled = !owners.is_player(sp.player_id);
        if (const auto *nation =
                nationRegistry.get_nation_for_player(sp.player_id)) {
          sp.nation_id = nation->id;
        } else {
          sp.nation_id = nationRegistry.default_nation_id();
        }
        if (auto unit =
                reg->create(Game::Units::SpawnType::Archer, world, sp)) {
          res.player_unit_id = unit->id();
        } else {
          qWarning() << "LevelLoader: Fallback archer spawn failed";
        }
      }
    }

    bool has_barracks = false;
    for (auto *e : world.get_entities_with<Engine::Core::UnitComponent>()) {
      if (auto *u = e->get_component<Engine::Core::UnitComponent>()) {
        if (u->spawn_type == Game::Units::SpawnType::Barracks &&
            owners.is_player(u->owner_id)) {
          has_barracks = true;
          break;
        }
      }
    }
    if (!has_barracks) {
      auto &nationRegistry = Game::Systems::NationRegistry::instance();
      auto reg2 = Game::Map::MapTransformer::getFactoryRegistry();
      if (reg2) {
        Game::Units::SpawnParams sp;
        sp.position = QVector3D(-4.0F, 0.0F, -3.0F);
        sp.player_id = owners.get_local_player_id();
        sp.spawn_type = Game::Units::SpawnType::Barracks;
        sp.ai_controlled = !owners.is_player(sp.player_id);
        if (const auto *nation =
                nationRegistry.get_nation_for_player(sp.player_id)) {
          sp.nation_id = nation->id;
        } else {
          sp.nation_id = nationRegistry.default_nation_id();
        }
        reg2->create(Game::Units::SpawnType::Barracks, world, sp);
      }
    }
  } else {
    res.ok = false;
    res.errorMessage = QString("Map load failed: %1").arg(err);
    qWarning() << "LevelLoader: Map load failed:" << err
               << "(path:" << resolved_map_path << ')'
               << "- applying default environment";
    Game::Map::Environment::applyDefault(renderer, camera);
    res.ok = false;
    res.cam_fov = camera.getFOV();
    res.cam_near = camera.getNear();
    res.cam_far = camera.getFar();
    res.grid_width = 50;
    res.grid_height = 50;
    res.tile_size = 1.0F;

    auto &nationRegistry = Game::Systems::NationRegistry::instance();
    auto reg = Game::Map::MapTransformer::getFactoryRegistry();
    if (reg) {
      Game::Units::SpawnParams sp;
      sp.position = QVector3D(0.0F, 0.0F, 0.0F);
      sp.player_id = 0;
      sp.spawn_type = Game::Units::SpawnType::Archer;
      sp.ai_controlled = !owners.is_player(sp.player_id);
      if (const auto *nation =
              nationRegistry.get_nation_for_player(sp.player_id)) {
        sp.nation_id = nation->id;
      } else {
        sp.nation_id = nationRegistry.default_nation_id();
      }
      if (auto unit = reg->create(Game::Units::SpawnType::Archer, world, sp)) {
        res.player_unit_id = unit->id();
      }
    }
  }

  return res;
}

} // namespace Game::Map
