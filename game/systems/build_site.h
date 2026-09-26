#pragma once

#include <QVector3D>

#include <optional>
#include <span>
#include <string>

#include "../core/entity.h"
#include "ground_verdict.h"
#include "site_keep_out.h"
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

[[nodiscard]] auto
find_clear_site(const Engine::Core::World& world,
                const std::string& building_type,
                const QVector3D& wanted,
                float search_radius,
                float facing_degrees = 0.0F,
                std::span<const Engine::Core::EntityID> crew = {},
                std::span<const SiteKeepOut> keep_out = {}) -> std::optional<QVector3D>;

// A building raised on standing troops seals them into its footprint, so an
// order is refused while any unit other than the crew stands where the nav grid
// will block: the padded footprint, widened by the unit's formation. Neutral
// wildlife and wall runs are exempt. Order placement asks this; the ground
// itself is assess_ground's question.
[[nodiscard]] auto
troops_stand_on(const Engine::Core::World& world,
                const std::string& building_type,
                float x,
                float z,
                float facing_degrees,
                std::span<const Engine::Core::EntityID> crew) -> bool;

[[nodiscard]] auto wall_ground_probe(const Engine::Core::World& world) -> GroundProbe;

} // namespace Game::Systems
