#include "map_transformer.h"

#include "../core/component.h"
#include "../core/world.h"
#include "../systems/owner_registry.h"
#include "../units/factory.h"
#include "../visuals/team_colors.h"
#include "terrain_service.h"
#include <QDebug>
#include <QVector3D>

namespace Game::Map {

namespace {
std::shared_ptr<Game::Units::UnitFactoryRegistry> s_registry;
}

void MapTransformer::setFactoryRegistry(
    std::shared_ptr<Game::Units::UnitFactoryRegistry> reg) {
  s_registry = std::move(reg);
}
std::shared_ptr<Game::Units::UnitFactoryRegistry>
MapTransformer::getFactoryRegistry() {
  return s_registry;
}

void MapTransformer::setLocalOwnerId(int ownerId) {
  Game::Systems::OwnerRegistry::instance().setLocalPlayerId(ownerId);
}

int MapTransformer::localOwnerId() {
  return Game::Systems::OwnerRegistry::instance().getLocalPlayerId();
}

MapRuntime
MapTransformer::applyToWorld(const MapDefinition &def,
                             Engine::Core::World &world,
                             const Game::Visuals::VisualCatalog *visuals) {
  MapRuntime rt;
  rt.unitIds.reserve(def.spawns.size());

  for (const auto &s : def.spawns) {

    float worldX = s.x;
    float worldZ = s.z;
    if (def.coordSystem == CoordSystem::Grid) {
      const float tile = std::max(0.0001f, def.grid.tileSize);
      worldX = (s.x - (def.grid.width * 0.5f - 0.5f)) * tile;
      worldZ = (s.z - (def.grid.height * 0.5f - 0.5f)) * tile;
    }

    auto &terrain = Game::Map::TerrainService::instance();
    if (terrain.isInitialized() && terrain.isForbiddenWorld(worldX, worldZ)) {
      const float tile = std::max(0.0001f, def.grid.tileSize);
      bool found = false;
      const int maxRadius = 12;
      for (int r = 1; r <= maxRadius && !found; ++r) {
        for (int ox = -r; ox <= r && !found; ++ox) {
          for (int oz = -r; oz <= r && !found; ++oz) {

            if (std::abs(ox) != r && std::abs(oz) != r)
              continue;
            float candX = worldX + float(ox) * tile;
            float candZ = worldZ + float(oz) * tile;
            if (!terrain.isForbiddenWorld(candX, candZ)) {
              worldX = candX;
              worldZ = candZ;
              found = true;
            }
          }
        }
      }
      if (!found) {
        qWarning()
            << "MapTransformer: spawn at" << s.x << s.z
            << "is forbidden and no nearby free tile found; spawning anyway";
      }
    }

    Engine::Core::Entity *e = nullptr;
    if (s_registry) {
      Game::Units::SpawnParams sp;
      sp.position = QVector3D(worldX, 0.0f, worldZ);
      sp.playerId = s.playerId;
      sp.unitType = s.type.toStdString();
      sp.aiControlled =
          !Game::Systems::OwnerRegistry::instance().isPlayer(s.playerId);
      auto obj = s_registry->create(s.type.toStdString(), world, sp);
      if (obj) {
        e = world.getEntity(obj->id());
        rt.unitIds.push_back(obj->id());
      }
    }
    if (!e) {

      e = world.createEntity();
      if (!e)
        continue;
      auto *t = e->addComponent<Engine::Core::TransformComponent>();
      t->position = {worldX, 0.0f, worldZ};
      t->scale = {0.5f, 0.5f, 0.5f};
      auto *r = e->addComponent<Engine::Core::RenderableComponent>("", "");
      r->visible = true;
      auto *u = e->addComponent<Engine::Core::UnitComponent>();
      u->unitType = s.type.toStdString();
      u->ownerId = s.playerId;
      u->visionRange = 14.0f;

      if (!Game::Systems::OwnerRegistry::instance().isPlayer(s.playerId)) {
        e->addComponent<Engine::Core::AIControlledComponent>();
      }

      if (auto *existingMv =
              e->getComponent<Engine::Core::MovementComponent>()) {
        existingMv->goalX = worldX;
        existingMv->goalY = worldZ;
        existingMv->targetX = worldX;
        existingMv->targetY = worldZ;
      } else if (auto *mv =
                     e->addComponent<Engine::Core::MovementComponent>()) {
        mv->goalX = worldX;
        mv->goalY = worldZ;
        mv->targetX = worldX;
        mv->targetY = worldZ;
      }

      QVector3D tc = Game::Visuals::teamColorForOwner(u->ownerId);
      r->color[0] = tc.x();
      r->color[1] = tc.y();
      r->color[2] = tc.z();
      if (s.type == "archer") {
        u->health = 80;
        u->maxHealth = 80;
        u->speed = 3.0f;
        u->visionRange = 16.0f;
        auto *atk = e->addComponent<Engine::Core::AttackComponent>();
        atk->range = 6.0f;
        atk->damage = 12;
        atk->cooldown = 1.2f;
      }
      if (!e->getComponent<Engine::Core::MovementComponent>()) {
        auto *mv = e->addComponent<Engine::Core::MovementComponent>();
        if (mv) {
          mv->goalX = worldX;
          mv->goalY = worldZ;
          mv->targetX = worldX;
          mv->targetY = worldZ;
        }
      }
      rt.unitIds.push_back(e->getId());
    }

    if (auto *r = e->getComponent<Engine::Core::RenderableComponent>()) {
      if (visuals) {
        Game::Visuals::VisualDef defv;
        if (visuals->lookup(s.type.toStdString(), defv)) {
          Game::Visuals::applyToRenderable(defv, *r);
        }
      }
      if (r->color[0] == 0.0f && r->color[1] == 0.0f && r->color[2] == 0.0f) {
        r->color[0] = r->color[1] = r->color[2] = 1.0f;
      }
    }

    if (auto *t = e->getComponent<Engine::Core::TransformComponent>()) {
      qInfo() << "Spawned" << s.type << "id=" << e->getId() << "at"
              << QVector3D(t->position.x, t->position.y, t->position.z)
              << "(coordSystem="
              << (def.coordSystem == CoordSystem::Grid ? "Grid" : "World")
              << ")";
    }
  }

  return rt;
}

} // namespace Game::Map
