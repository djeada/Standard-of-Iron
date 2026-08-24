#pragma once

#include <QString>
#include <QVariantMap>

#include "game/systems/resource_types.h"
#include "game/systems/wall_plan_service.h"

namespace Game::Systems {
class PlayerResourceRegistry;
}

namespace App::Economy {

[[nodiscard]] auto
to_variant_map(const Game::Systems::ResourceAmounts& amounts) -> QVariantMap;

[[nodiscard]] auto resource_name(Game::Systems::ResourceType type) -> QString;

[[nodiscard]] auto
construction_costs(const QString& item_type) -> Game::Systems::ResourceAmounts;

[[nodiscard]] auto wood_per_wall_segment(const QString& item_type) -> int;

[[nodiscard]] auto
insufficient_resources_reason(const Game::Systems::PlayerResourceRegistry& economy,
                              int owner_id,
                              const Game::Systems::ResourceAmounts& cost) -> QString;

[[nodiscard]] auto
wall_segment_failure_text(const Game::Systems::PlannedWallSegment& segment) -> QString;

} // namespace App::Economy
