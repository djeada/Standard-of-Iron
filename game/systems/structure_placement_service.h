#pragma once

#include <QVector3D>

#include <cstdint>
#include <string>

#include "../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

enum class PlacementRuling : std::uint8_t {
  Ok,
  UnknownStructure,
  Blocked,
  Unaffordable,
  NoFactory,
  SpawnFailed,
};

class StructurePlacementService {
public:
  static auto footprint_is_clear(const Engine::Core::World& world,
                                 float x,
                                 float z,
                                 const std::string& building_type) -> bool;

  static auto ruling(Engine::Core::World& world,
                     int owner_id,
                     const std::string& building_type,
                     const QVector3D& position) -> PlacementRuling;

  static auto place(Engine::Core::World& world,
                    int owner_id,
                    const std::string& building_type,
                    const QVector3D& position,
                    float rotation_y) -> Engine::Core::EntityID;
};

} // namespace Game::Systems
