#include "app/economy/placement_orders.h"

#include "game/command/command.h"

namespace App::Economy {

auto structure_build_order(const std::vector<std::uint64_t>& builders,
                           const QString& construction_type,
                           const QVector3D& site,
                           float rotation_y) -> App::Core::OrderRequest {
  App::Core::OrderRequest request;
  request.kind = App::Core::OrderKind::Build;
  request.payload = Game::Command::StartConstruction{
      .units = builders,
      .construction_type = construction_type.toStdString(),
      .site = site,
      .rotation_y = rotation_y};
  request.has_destination = true;
  request.destination = site;
  return request;
}

auto wall_plan_order(const std::vector<std::uint64_t>& builders,
                     const Game::Systems::WallPlanRequest& plan,
                     const QVector3D& destination) -> App::Core::OrderRequest {
  App::Core::OrderRequest request;
  request.kind = App::Core::OrderKind::Build;
  request.payload = Game::Command::PlaceWallPlan{.units = builders,
                                                 .gate = plan.gate,
                                                 .anchor_x = plan.anchor.x,
                                                 .anchor_z = plan.anchor.z,
                                                 .target_x = plan.target.x,
                                                 .target_z = plan.target.z,
                                                 .rotation_y = plan.rotation_y,
                                                 .ladder = plan.ladder,
                                                 .pointer_x = plan.pointer.x(),
                                                 .pointer_z = plan.pointer.z()};
  request.has_destination = true;
  request.destination = destination;
  return request;
}

auto direct_placement_order(const std::string& building_type,
                            const QVector3D& site,
                            float rotation_y) -> App::Core::OrderRequest {
  App::Core::OrderRequest request;
  request.kind = App::Core::OrderKind::Build;
  request.payload = Game::Command::PlaceBuilding{
      .building_type = building_type, .position = site, .rotation_y = rotation_y};
  request.has_destination = true;
  request.destination = site;
  return request;
}

} // namespace App::Economy
