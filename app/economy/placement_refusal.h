#pragma once

#include <QString>

#include <vector>

#include "game/systems/structure_placement_service.h"
#include "game/systems/wall_plan_service.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {
class PlayerResourceRegistry;
}

namespace App::Economy {

struct RefusalContext {
  const Game::Systems::PlayerResourceRegistry& economy;
  int owner_id = 0;
  const QString& construction_type;
};

[[nodiscard]] auto placement_refusal_text(Game::Systems::PlacementRuling ruling,
                                          const RefusalContext& context) -> QString;

[[nodiscard]] auto
wall_plan_refusal_text(const std::vector<Game::Systems::PlannedWallSegment>& segments,
                       const RefusalContext& context) -> QString;

[[nodiscard]] auto wall_confirm_refusal_text(
    const std::vector<Game::Systems::PlannedWallSegment>& segments) -> QString;

[[nodiscard]] auto ground_refusal_text(const Engine::Core::World& world,
                                       const QString& building_type,
                                       float world_x,
                                       float world_z,
                                       float rotation_y,
                                       const std::vector<std::uint64_t>& builders,
                                       const RefusalContext& context) -> QString;

[[nodiscard]] auto affordability_refusal_text(const RefusalContext& context) -> QString;

} // namespace App::Economy
