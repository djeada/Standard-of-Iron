#pragma once

#include <QString>
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "app/orders/order_submission.h"
#include "game/systems/wall_plan_service.h"

namespace App::Economy {

[[nodiscard]] auto structure_build_order(const std::vector<std::uint64_t>& builders,
                                         const QString& construction_type,
                                         const QVector3D& site,
                                         float rotation_y) -> App::Core::OrderRequest;

[[nodiscard]] auto
wall_plan_order(const std::vector<std::uint64_t>& builders,
                const Game::Systems::WallPlanRequest& plan,
                const QVector3D& destination) -> App::Core::OrderRequest;

[[nodiscard]] auto direct_placement_order(const std::string& building_type,
                                          const QVector3D& site,
                                          float rotation_y) -> App::Core::OrderRequest;

} // namespace App::Economy
