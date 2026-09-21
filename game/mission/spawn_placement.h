#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>

#include "game/core/entity_id.h"

namespace Engine::Core {
class World;
}

namespace Game::Mission {

inline constexpr int k_spawn_search_rings = 48;

enum class BuildingFootprints : std::uint8_t {
  Trust = 0,
  Refuse = 1,
};

[[nodiscard]] auto
find_free_ground_near(Engine::Core::World& world,
                      Engine::Core::EntityID placed,
                      const QVector3D& origin,
                      BuildingFootprints buildings = BuildingFootprints::Trust)
    -> std::optional<QVector3D>;

[[nodiscard]] auto
place_clear_of_units(Engine::Core::World& world,
                     Engine::Core::EntityID placed,
                     const QVector3D& origin,
                     BuildingFootprints buildings = BuildingFootprints::Trust)
    -> std::optional<QVector3D>;

} // namespace Game::Mission
