#include "map_transformer.h"

#include "../core/component.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/owner_registry.h"
#include "../units/factory.h"
#include "../units/spawn_type.h"
#include "core/entity.h"
#include "map/map_definition.h"
#include "terrain_service.h"
#include "units/unit.h"
#include "visuals/visual_catalog.h"
#include <QDebug>
#include <QVector3D>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <qglobal.h>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

namespace Game::Map {

namespace {
std::shared_ptr<Game::Units::UnitFactoryRegistry> s_registry;
std::unordered_map<int, int> s_player_team_overrides;
} // namespace

void MapTransformer::setFactoryRegistry(
    std::shared_ptr<Game::Units::UnitFactoryRegistry> reg) {
  s_registry = std::move(reg);
}
auto MapTransformer::getFactoryRegistry()
    -> std::shared_ptr<Game::Units::UnitFactoryRegistry> {
  return s_registry;
}

void MapTransformer::setLocalOwnerId(int owner_id) {
  auto &owners = Game::Systems::OwnerRegistry::instance();
  owners.setLocalPlayerId(owner_id);
}

auto MapTransformer::localOwnerId() -> int {
  auto &owners = Game::Systems::OwnerRegistry::instance();
  return owners.getLocalPlayerId();
}

void MapTransformer::setPlayerTeamOverrides(
    const std::unordered_map<int, int> &overrides) {
  s_player_team_overrides = overrides;
}

void MapTransformer::clearPlayerTeamOverrides() {
  s_player_team_overrides.clear();
}

auto MapTransformer::applyToWorld(
    const MapDefinition &def, Engine::Core::World &world,
    const Game::Visuals::VisualCatalog *visuals) -> MapRuntime {
  MapRuntime rt;
  rt.unit_ids.reserve(def.spawns.size());

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  std::set<int> unique_player_ids;
  std::unordered_map<int, int> player_idToTeam;

  for (const auto &spawn : def.spawns) {
    if (spawn.player_id == Game::Core::NEUTRAL_OWNER_ID) {
      continue;
    }
    unique_player_ids.insert(spawn.player_id);

    if (spawn.team_id > 0) {
      player_idToTeam[spawn.player_id] = spawn.team_id;
    }
  }

  for (int const player_id : unique_player_ids) {
    bool const has_team_override =
        (s_player_team_overrides.find(player_id) !=
         s_player_team_overrides.end());

    // Skip players not in the configuration (only when overrides are provided)
    // This ensures only selected players spawn, while maintaining backward
    // compatibility when no overrides are set.
    if (!s_player_team_overrides.empty() && !has_team_override) {
      continue;
    }

    if (owner_registry.getOwnerType(player_id) ==
        Game::Systems::OwnerType::Neutral) {

      bool const is_local_player =
          (player_id == owner_registry.getLocalPlayerId());
      Game::Systems::OwnerType const owner_type =
          is_local_player ? Game::Systems::OwnerType::Player
                          : Game::Systems::OwnerType::AI;

      std::string const owner_name =
          is_local_player ? "Player " + std::to_string(player_id)
                          : "AI Player " + std::to_string(player_id);

      owner_registry.registerOwnerWithId(player_id, owner_type, owner_name);
    }

    int final_team_id = 0;
    auto override_it = s_player_team_overrides.find(player_id);
    if (override_it != s_player_team_overrides.end()) {

      final_team_id = override_it->second;
    } else {

      auto team_it = player_idToTeam.find(player_id);
      if (team_it != player_idToTeam.end()) {
        final_team_id = team_it->second;
      } else {
      }
    }
    owner_registry.setOwnerTeam(player_id, final_team_id);
  }

  for (const auto &s : def.spawns) {

    float world_x = s.x;
    float world_z = s.z;
    if (def.coordSystem == CoordSystem::Grid) {
      const float tile = std::max(0.0001F, def.grid.tile_size);
      world_x = (s.x - (def.grid.width * 0.5F - 0.5F)) * tile;
      world_z = (s.z - (def.grid.height * 0.5F - 0.5F)) * tile;
    }

    auto &terrain = Game::Map::TerrainService::instance();
    if (terrain.isInitialized() && terrain.isForbiddenWorld(world_x, world_z)) {
      const float tile = std::max(0.0001F, def.grid.tile_size);
      bool found = false;
      const int max_radius = 12;
      for (int r = 1; r <= max_radius && !found; ++r) {
        for (int ox = -r; ox <= r && !found; ++ox) {
          for (int oz = -r; oz <= r && !found; ++oz) {

            if (std::abs(ox) != r && std::abs(oz) != r) {
              continue;
            }
            float const cand_x = world_x + float(ox) * tile;
            float const cand_z = world_z + float(oz) * tile;
            if (!terrain.isForbiddenWorld(cand_x, cand_z)) {
              world_x = cand_x;
              world_z = cand_z;
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
      sp.position = QVector3D(world_x, 0.0F, world_z);
      sp.player_id = s.player_id;
      sp.spawn_type = s.type;
      sp.aiControlled = !owner_registry.isPlayer(s.player_id);
      sp.maxPopulation = s.maxPopulation;
      auto obj = s_registry->create(s.type, world, sp);
      if (obj) {
        e = world.getEntity(obj->id());
        rt.unit_ids.push_back(obj->id());
      } else {
        qWarning() << "MapTransformer: no factory for spawn type"
                   << Game::Units::spawn_typeToQString(s.type)
                   << "- skipping spawn at" << world_x << world_z;
        continue;
      }
    } else {
      qWarning() << "MapTransformer: no factory registry set; skipping spawn";
      continue;
    }

    if (e == nullptr) {
      continue;
    }

    if (auto *r = e->getComponent<Engine::Core::RenderableComponent>()) {
      if (visuals != nullptr) {
        Game::Visuals::VisualDef defv;
        if (visuals->lookup(Game::Units::spawn_typeToString(s.type), defv)) {
          Game::Visuals::applyToRenderable(defv, *r);
        }
      }
      if (r->color[0] == 0.0F && r->color[1] == 0.0F && r->color[2] == 0.0F) {
        r->color[0] = r->color[1] = r->color[2] = 1.0F;
      }
    }

    if (auto *t = e->getComponent<Engine::Core::TransformComponent>()) {
      qInfo() << "Spawned" << Game::Units::spawn_typeToQString(s.type)
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
