#include "app/economy/placement_refusal.h"

#include <QCoreApplication>

#include "app/economy/resource_text.h"
#include "game/systems/player_resource_registry.h"

namespace App::Economy {

auto placement_refusal_text(Game::Systems::PlacementRuling ruling,
                            const RefusalContext& context) -> QString {
  using Game::Systems::PlacementRuling;
  switch (ruling) {
  case PlacementRuling::BlockedByStructure:
    return QCoreApplication::translate("ProductionManager",
                                       "Something is already standing here.");
  case PlacementRuling::BlockedByTroops:
    return QCoreApplication::translate("ProductionManager",
                                       "Troops are standing here. Move them first.");
  case PlacementRuling::BlockedByObstacle:
    return QCoreApplication::translate("ProductionManager",
                                       "This ground cannot be cleared to build on.");
  case PlacementRuling::BlockedByWater:
    return QCoreApplication::translate("ProductionManager",
                                       "Nothing can be built on the water.");
  case PlacementRuling::BlockedByGround:
    return QCoreApplication::translate("ProductionManager",
                                       "The ground here is too steep to build on.");
  case PlacementRuling::OutsideBattlefield:
    return QCoreApplication::translate("ProductionManager",
                                       "That is beyond the edge of the battlefield.");
  case PlacementRuling::UnknownStructure:
    return QCoreApplication::translate("ProductionManager",
                                       "That structure cannot be placed.");
  case PlacementRuling::Unaffordable:
    return insufficient_resources_reason(context.economy,
                                         context.owner_id,
                                         construction_costs(context.construction_type));
  case PlacementRuling::NoFactory:
  case PlacementRuling::SpawnFailed:
    return QCoreApplication::translate("ProductionManager",
                                       "Building factory unavailable.");
  case PlacementRuling::Ok:
    break;
  }
  return {};
}

auto wall_plan_refusal_text(
    const std::vector<Game::Systems::PlannedWallSegment>& segments,
    const RefusalContext& context) -> QString {
  using Game::Systems::WallSegmentFault;
  for (const auto& segment : segments) {
    switch (segment.fault) {
    case WallSegmentFault::None:
      continue;
    case WallSegmentFault::Occupied:
      return QCoreApplication::translate("ProductionManager",
                                         "A wall already stands here.");
    case WallSegmentFault::Invalid:
      return placement_refusal_text(
          Game::Systems::StructurePlacementService::ruling_for(segment.verdict),
          context);
    case WallSegmentFault::NotEnoughWood:
      return placement_refusal_text(Game::Systems::PlacementRuling::Unaffordable,
                                    context);
    }
  }
  return QCoreApplication::translate("ProductionManager",
                                     "No part of this wall can stand there.");
}

auto wall_confirm_refusal_text(
    const std::vector<Game::Systems::PlannedWallSegment>& segments) -> QString {
  for (const auto& segment : segments) {
    if (segment.fault != Game::Systems::WallSegmentFault::None) {
      return wall_segment_failure_text(segment);
    }
  }
  return QCoreApplication::translate("ProductionManager",
                                     "No valid wall segments in that drag.");
}

auto ground_refusal_text(const Engine::Core::World& world,
                         const QString& building_type,
                         float world_x,
                         float world_z,
                         float rotation_y,
                         const std::vector<std::uint64_t>& builders,
                         const RefusalContext& context) -> QString {
  return placement_refusal_text(
      Game::Systems::StructurePlacementService::ground_ruling(
          world, building_type.toStdString(), world_x, world_z, rotation_y, builders),
      context);
}

auto affordability_refusal_text(const RefusalContext& context) -> QString {
  const Game::Systems::ResourceAmounts costs =
      construction_costs(context.construction_type);
  if (costs.empty() || context.economy.has_at_least(context.owner_id, costs)) {
    return {};
  }
  return insufficient_resources_reason(context.economy, context.owner_id, costs);
}

} // namespace App::Economy
