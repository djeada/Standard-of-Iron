#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "../core/entity.h"
#include "systems/economy/build_site.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

enum class PlacementRuling : std::uint8_t {
  Ok,
  UnknownStructure,

  BlockedByStructure,

  // The ground is clear but the player's own companies stand on it.
  BlockedByTroops,

  BlockedByObstacle,

  BlockedByWater,

  BlockedByGround,

  OutsideBattlefield,
  Unaffordable,
  NoFactory,
  SpawnFailed,
};

class StructurePlacementService {
public:
  static auto ruling_for(GroundVerdict verdict) -> PlacementRuling;

  static auto
  ground_ruling(const Engine::Core::World& world,
                const std::string& building_type,
                float x,
                float z,
                float rotation_y = 0.0F,
                std::span<const Engine::Core::EntityID> crew = {}) -> PlacementRuling;

  static auto ruling(Engine::Core::World& world,
                     int owner_id,
                     const std::string& building_type,
                     const QVector3D& position,
                     float rotation_y = 0.0F) -> PlacementRuling;

  static auto place(Engine::Core::World& world,
                    int owner_id,
                    const std::string& building_type,
                    const QVector3D& position,
                    float rotation_y = 0.0F) -> Engine::Core::EntityID;
};

} // namespace Game::Systems
