#include "app/economy/resource_text.h"

#include <QCoreApplication>

#include "game/systems/construction_cost_catalog.h"
#include "game/systems/player_resource_registry.h"

namespace App::Economy {
namespace {

constexpr const char* k_context = "ProductionManager";

[[nodiscard]] auto
first_missing_resource_name(const Game::Systems::PlayerResourceRegistry& economy,
                            int owner_id,
                            const Game::Systems::ResourceAmounts& cost) -> QString {
  const Game::Systems::ResourceAmounts available = economy.get_all(owner_id);
  for (const Game::Systems::ResourceType type : Game::Systems::k_all_resource_types) {
    if (available.get(type) < cost.get(type)) {
      return resource_name(type);
    }
  }
  return QCoreApplication::translate(k_context, "resources");
}

} // namespace

auto to_variant_map(const Game::Systems::ResourceAmounts& amounts) -> QVariantMap {
  QVariantMap map;
  for (const Game::Systems::ResourceType type : Game::Systems::k_all_resource_types) {
    if (const int amount = amounts.get(type); amount > 0) {
      map[QLatin1String(Game::Systems::resource_type_key(type))] = amount;
    }
  }
  return map;
}

auto resource_name(Game::Systems::ResourceType type) -> QString {
  switch (type) {
  case Game::Systems::ResourceType::Gold:
    return QCoreApplication::translate(k_context, "gold");
  case Game::Systems::ResourceType::Food:
    return QCoreApplication::translate(k_context, "food");
  case Game::Systems::ResourceType::Wood:
    return QCoreApplication::translate(k_context, "wood");
  case Game::Systems::ResourceType::Stone:
    return QCoreApplication::translate(k_context, "stone");
  case Game::Systems::ResourceType::Iron:
    return QCoreApplication::translate(k_context, "iron");
  case Game::Systems::ResourceType::Count:
    break;
  }
  return QCoreApplication::translate(k_context, "resources");
}

auto construction_costs(const QString& item_type) -> Game::Systems::ResourceAmounts {
  return Game::Systems::construction_cost_info(item_type.toStdString()).resource_costs;
}

auto wood_per_wall_segment(const QString& item_type) -> int {
  return Game::Systems::construction_cost_info(item_type.toStdString())
      .resource_costs.get(Game::Systems::ResourceType::Wood);
}

auto insufficient_resources_reason(const Game::Systems::PlayerResourceRegistry& economy,
                                   int owner_id,
                                   const Game::Systems::ResourceAmounts& cost)
    -> QString {
  return QCoreApplication::translate(k_context, "Not enough %1.")
      .arg(first_missing_resource_name(economy, owner_id, cost));
}

auto wall_segment_failure_text(const Game::Systems::PlannedWallSegment& segment)
    -> QString {
  switch (segment.fault) {
  case Game::Systems::WallSegmentFault::Occupied:
    return QCoreApplication::translate(k_context, "Blocked by an existing wall.");
  case Game::Systems::WallSegmentFault::NotEnoughWood:
    return QCoreApplication::translate(k_context, "Not enough wood.");
  case Game::Systems::WallSegmentFault::Invalid:
    return QString::fromStdString(segment.failure_reason);
  case Game::Systems::WallSegmentFault::None:
    break;
  }
  return {};
}

} // namespace App::Economy
