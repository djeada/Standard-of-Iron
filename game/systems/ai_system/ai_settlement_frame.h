#pragma once

#include <QVector2D>
#include <QVector3D>

#include <optional>

#include "ai_doctrine_catalog.h"
#include "ai_types.h"

namespace Game::Systems::AI {

[[nodiscard]] auto settlement_facing(const AIContext& context,
                                     const AISnapshot& snapshot) -> QVector2D;

[[nodiscard]] auto locked_settlement_facing(const AIContext& context,
                                            const AISnapshot& snapshot) -> QVector2D;

[[nodiscard]] auto plan_offset_to_world(const QVector2D& facing,
                                        float local_x,
                                        float local_z) -> QVector3D;

[[nodiscard]] auto plan_rotation_to_world(const QVector2D& facing,
                                          float local_rotation) -> float;

[[nodiscard]] auto settlement_muster_world(const AIContext& context,
                                           const AISnapshot& snapshot,
                                           MusterSide side) -> std::optional<QVector3D>;

[[nodiscard]] auto doctrine_muster_side(const AIStrategyConfig& strategy) -> MusterSide;

void apply_settlement_stations(const AISnapshot& snapshot, AIContext& context);

void update_station_report(const AISnapshot& snapshot, AIContext& context);

[[nodiscard]] auto station_radius(const AIContext& context) -> float;

} // namespace Game::Systems::AI
