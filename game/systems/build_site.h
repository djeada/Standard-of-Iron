#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>
#include <string>

#include "../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

enum class GroundVerdict : std::uint8_t {
  Clear,

  Occupied,

  Impassable,

  Water,

  Uneven,

  OffMap,
};

[[nodiscard]] auto
assess_ground(const Engine::Core::World& world,
              const std::string& building_type,
              float x,
              float z,
              Engine::Core::EntityID ignore_entity_id = 0) -> GroundVerdict;

[[nodiscard]] auto find_clear_site(const Engine::Core::World& world,
                                   const std::string& building_type,
                                   const QVector3D& wanted,
                                   float search_radius) -> std::optional<QVector3D>;

void clear_ground_for(Engine::Core::World& world,
                      const std::string& building_type,
                      const QVector3D& position);

} // namespace Game::Systems
