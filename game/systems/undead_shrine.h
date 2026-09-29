#pragma once

#include "game/map/undead_shrine_placement.h"
#include "undead_zone_runtime.h"

namespace Engine::Core {
class World;
}

namespace Game::Map {
class TerrainService;
struct MapDefinition;
} // namespace Game::Map

namespace Game::Units {
class UnitFactoryRegistry;
}

namespace Game::Systems {

class GlobalStatsRegistry;
class NationRegistry;
class OwnerRegistry;

class UndeadShrine {
public:
  UndeadShrine(Game::Map::TerrainService& terrain,
               OwnerRegistry& owners,
               NationRegistry& nations,
               GlobalStatsRegistry& stats);

  void register_zone_owner(const UndeadRuntimeZone& zone) const;
  void place(const Game::Map::MapDefinition& map_definition,
             UndeadRuntimeZone& zone,
             Game::Map::UndeadShrineExclusions& exclusions) const;

  void ensure_anchor_structure(Engine::Core::World& world,
                               UndeadRuntimeZone& zone,
                               Game::Units::UnitFactoryRegistry* factories) const;
  void refresh_anchor_structure(Engine::Core::World& world,
                                UndeadRuntimeZone& zone) const;
  void refresh_capture_lock(Engine::Core::World& world,
                            const UndeadRuntimeZone& zone) const;

  void break_garrison(Engine::Core::World& world,
                      UndeadRuntimeZone& zone,
                      bool captured) const;

private:
  void pay_clear_reward(Engine::Core::World& world,
                        const UndeadRuntimeZone& zone,
                        bool captured) const;

  Game::Map::TerrainService& m_terrain;
  OwnerRegistry& m_owners;
  NationRegistry& m_nations;
  GlobalStatsRegistry& m_stats;
};

} // namespace Game::Systems
