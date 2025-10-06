#include "map_transformer.h"

#include "../core/component.h"
#include "../core/world.h"
#include "../units/factory.h"
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

    Engine::Core::Entity *e = nullptr;
    if (s_registry) {
      Game::Units::SpawnParams sp;
      sp.position = QVector3D(worldX, 0.0f, worldZ);
      sp.playerId = s.playerId;
      sp.unitType = s.type.toStdString();
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

      QVector3D tc;
      switch (u->ownerId) {
      case 1:
        tc = QVector3D(0.20f, 0.55f, 1.00f);
        break;
      case 2:
        tc = QVector3D(1.00f, 0.30f, 0.30f);
        break;
      case 3:
        tc = QVector3D(0.20f, 0.80f, 0.40f);
        break;
      case 4:
        tc = QVector3D(1.00f, 0.80f, 0.20f);
        break;
      default:
        tc = QVector3D(0.8f, 0.9f, 1.0f);
        break;
      }
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
      e->addComponent<Engine::Core::MovementComponent>();
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
