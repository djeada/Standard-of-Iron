#include "map_transformer.h"

#include "../core/component.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/owner_registry.h"
#include "../units/factory.h"
#include "../units/spawn_type.h"
#include "../visuals/team_colors.h"
#include "terrain_service.h"
#include <QDebug>
#include <QVector3D>
#include <set>
#include <unordered_map>

namespace Game::Map {

namespace {
std::shared_ptr<Game::Units::UnitFactoryRegistry> s_registry;
std::unordered_map<int, int> s_playerTeamOverrides;
} // namespace

void MapTransformer::setFactoryRegistry(
    std::shared_ptr<Game::Units::UnitFactoryRegistry> reg) {
  s_registry = std::move(reg);
}
std::shared_ptr<Game::Units::UnitFactoryRegistry>
MapTransformer::getFactoryRegistry() {
  return s_registry;
}

void MapTransformer::setLocalOwnerId(int ownerId) {
  auto &owners = Game::Systems::OwnerRegistry::instance();
  owners.setLocalPlayerId(ownerId);
}

int MapTransformer::localOwnerId() {
  auto &owners = Game::Systems::OwnerRegistry::instance();
  return owners.getLocalPlayerId();
}

void MapTransformer::setPlayerTeamOverrides(
    const std::unordered_map<int, int> &overrides) {
  s_playerTeamOverrides = overrides;
}

void MapTransformer::clearPlayerTeamOverrides() {
  s_playerTeamOverrides.clear();
}

MapRuntime
MapTransformer::applyToWorld(const MapDefinition &def,
                             Engine::Core::World &world,
                             const Game::Visuals::VisualCatalog *visuals) {
  MapRuntime rt;
  rt.unitIds.reserve(def.spawns.size());

  auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();
  std::set<int> uniquePlayerIds;
  std::unordered_map<int, int> playerIdToTeam;

  for (const auto &spawn : def.spawns) {
    if (spawn.playerId == Game::Core::NEUTRAL_OWNER_ID) {
      continue;
    }
    uniquePlayerIds.insert(spawn.playerId);

    if (spawn.teamId > 0) {
      playerIdToTeam[spawn.playerId] = spawn.teamId;
    }
  }

  for (int playerId : uniquePlayerIds) {

    if (ownerRegistry.getOwnerType(playerId) ==
        Game::Systems::OwnerType::Neutral) {

      bool isLocalPlayer = (playerId == ownerRegistry.getLocalPlayerId());
      Game::Systems::OwnerType ownerType =
          isLocalPlayer ? Game::Systems::OwnerType::Player
                        : Game::Systems::OwnerType::AI;

      std::string ownerName = isLocalPlayer
                                  ? "Player " + std::to_string(playerId)
                                  : "AI Player " + std::to_string(playerId);

      ownerRegistry.registerOwnerWithId(playerId, ownerType, ownerName);
    }

    int finalTeamId = 0;
    auto overrideIt = s_playerTeamOverrides.find(playerId);
    if (overrideIt != s_playerTeamOverrides.end()) {

      finalTeamId = overrideIt->second;
    } else {

      auto teamIt = playerIdToTeam.find(playerId);
      if (teamIt != playerIdToTeam.end()) {
        finalTeamId = teamIt->second;
      } else {
      }
    }
    ownerRegistry.setOwnerTeam(playerId, finalTeamId);
  }

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
      sp.spawnType = s.type;
      sp.unitType = Game::Units::spawnTypeToString(s.type);
      sp.aiControlled = !ownerRegistry.isPlayer(s.playerId);
      sp.maxPopulation = s.maxPopulation;
      auto obj = s_registry->create(s.type, world, sp);
      if (obj) {
        e = world.getEntity(obj->id());
        rt.unitIds.push_back(obj->id());
      } else {
        qWarning() << "MapTransformer: no factory for spawn type"
                   << Game::Units::spawnTypeToQString(s.type)
                   << "- skipping spawn at" << worldX << worldZ;
        continue;
      }
    } else {
      qWarning() << "MapTransformer: no factory registry set; skipping spawn";
      continue;
    }

    if (!e)
      continue;

    if (auto *r = e->getComponent<Engine::Core::RenderableComponent>()) {
      if (visuals) {
        Game::Visuals::VisualDef defv;
        if (visuals->lookup(Game::Units::spawnTypeToString(s.type), defv)) {
          Game::Visuals::applyToRenderable(defv, *r);
        }
      }
      if (r->color[0] == 0.0f && r->color[1] == 0.0f && r->color[2] == 0.0f) {
        r->color[0] = r->color[1] = r->color[2] = 1.0f;
      }
    }

    if (auto *t = e->getComponent<Engine::Core::TransformComponent>()) {
      qInfo() << "Spawned" << Game::Units::spawnTypeToQString(s.type)
              << "id=" << e->getId() << "at"
              << QVector3D(t->position.x, t->position.y, t->position.z)
              << "(coordSystem="
              << (def.coordSystem == CoordSystem::Grid ? "Grid" : "World")
              << ")";
    }
  }

  return rt;
}

} // namespace Game::Map
