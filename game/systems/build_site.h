#pragma once

#include <QVector3D>

#include <optional>
#include <span>
#include <string>

#include "../core/entity.h"
#include "ground_verdict.h"
#include "wall_network_service.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

[[nodiscard]] auto
assess_ground(const Engine::Core::World& world,
              const std::string& building_type,
              float x,
              float z,
              Engine::Core::EntityID ignore_entity_id = 0,
              float facing_degrees = 0.0F,
              std::span<const Engine::Core::EntityID> crew = {}) -> GroundVerdict;

[[nodiscard]] auto find_clear_site(const Engine::Core::World& world,
                                   const std::string& building_type,
                                   const QVector3D& wanted,
                                   float search_radius,
                                   float facing_degrees = 0.0F,
                                   std::span<const Engine::Core::EntityID> crew = {})
    -> std::optional<QVector3D>;

// The ground probe the wall network asks its placement questions through.
// WallNetworkService lives in navigation and may not reach up into economy for
// assess_ground, so economy supplies the answer instead; every production
// caller of validate_wall_segment_placement() and find_tower_snap_socket()
// passes this, and nothing else needs to know the shape of the callback.
// The returned probe holds `world` by reference and is meant to be built at
// the call site and handed straight in, not stored.
[[nodiscard]] auto wall_ground_probe(const Engine::Core::World& world) -> GroundProbe;

} // namespace Game::Systems
