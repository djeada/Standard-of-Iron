#pragma once

#include <QVector3D>

#include <vector>

#include "undead_zone_runtime.h"

namespace Engine::Core {
class World;
}

namespace Game::Map {
class TerrainService;
}

namespace Game::Systems {

class UndeadGuardians {
public:
  explicit UndeadGuardians(const Game::Map::TerrainService& terrain);

  void enforce_leashes(Engine::Core::World& world,
                       std::vector<UndeadRuntimeZone>& zones,
                       float delta_time);

  void post_new_wave(Engine::Core::World& world, const UndeadRuntimeZone& zone) const;

  [[nodiscard]] auto spawn_position_for_index(const UndeadRuntimeZone& zone,
                                              int spawn_index,
                                              int spawn_count) const -> QVector3D;

private:
  void enforce_leash(Engine::Core::World& world, const UndeadRuntimeZone& zone) const;
  void station_guardian(Engine::Core::World& world,
                        const UndeadRuntimeZone& zone,
                        Engine::Core::EntityID guardian_id,
                        int post_index,
                        int post_count,
                        bool recall) const;
  [[nodiscard]] auto guard_post_for_index(const UndeadRuntimeZone& zone,
                                          int post_index,
                                          int post_count) const -> QVector3D;

  const Game::Map::TerrainService& m_terrain;
  float m_leash_poll = 0.0F;
};

} // namespace Game::Systems
