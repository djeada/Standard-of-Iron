#include "app/economy/harvest_targeting.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <limits>

#include "app/input/input_command_handler.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/session_context.h"
#include "game/systems/harvest_yields.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "scene/camera.h"

namespace App::Economy {
namespace {

constexpr auto k_cut_tree_item_type = "cut_tree";
constexpr auto k_collect_item_type = "collect";
constexpr auto k_collect_stone_item_type = "collect_stone";
constexpr auto k_collect_iron_ore_item_type = "collect_iron_ore";

constexpr int k_harvest_work_search_radius = 16;

auto select_best_builder_for_target(
    Engine::Core::World* world,
    const std::vector<Engine::Core::EntityID>& builder_ids,
    float target_x,
    float target_z) -> Engine::Core::EntityID {
  Engine::Core::EntityID best_id = 0;
  float best_distance_sq = std::numeric_limits<float>::infinity();
  for (auto id : builder_ids) {
    auto* entity = world != nullptr ? world->get_entity(id) : nullptr;
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (transform == nullptr) {
      continue;
    }
    float const dx = transform->position.x - target_x;
    float const dz = transform->position.z - target_z;
    float const distance_sq = dx * dx + dz * dz;
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best_id = id;
    }
  }
  return best_id;
}

auto resolve_harvest_work_position(Engine::Core::World* world,
                                   Engine::Core::EntityID builder_id,
                                   const Game::Map::WorldPropTarget& target)
    -> std::optional<QVector3D> {
  auto* builder = world != nullptr ? world->get_entity(builder_id) : nullptr;
  auto* transform = builder != nullptr
                        ? builder->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  if (transform == nullptr) {
    return std::nullopt;
  }

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    return std::nullopt;
  }

  Game::Systems::Point const tree_grid =
      Game::Systems::NavGrid::world_to_grid(target.x, target.z);
  auto const work_grid = Game::Systems::NavGrid::find_nearest_walkable_grid_facing(
      tree_grid,
      QVector3D(transform->position.x, 0.0F, transform->position.z),
      k_harvest_work_search_radius);
  if (!work_grid.has_value()) {
    return std::nullopt;
  }

  QVector3D work_position = Game::Systems::NavGrid::grid_to_world(*work_grid);
  auto& terrain_service = Game::Session::session_for(*world).terrain();
  work_position.setY(terrain_service.resolve_surface_world_y(
      work_position.x(), work_position.z(), 0.0F, transform->position.y));
  return work_position;
}

auto harvest_target_kind_for_item(const QString& item_type)
    -> std::optional<HarvestTargetKind> {
  if (item_type == QLatin1String(k_cut_tree_item_type)) {
    return HarvestTargetKind::Tree;
  }
  if (item_type == QLatin1String(k_collect_stone_item_type)) {
    return HarvestTargetKind::Boulder;
  }
  if (item_type == QLatin1String(k_collect_iron_ore_item_type)) {
    return HarvestTargetKind::IronOre;
  }
  return std::nullopt;
}

auto harvest_target_kind_for_world_prop_type(Game::Map::WorldProp::Type type)
    -> std::optional<HarvestTargetKind> {
  if (Game::Map::is_tree_world_prop_type(type)) {
    return HarvestTargetKind::Tree;
  }
  if (Game::Map::is_boulder_world_prop_type(type)) {
    return HarvestTargetKind::Boulder;
  }
  if (Game::Map::is_iron_ore_world_prop_type(type)) {
    return HarvestTargetKind::IronOre;
  }
  return std::nullopt;
}

auto missing_harvest_target_reason(HarvestTargetKind kind) -> QString {
  switch (kind) {
  case HarvestTargetKind::Tree:
    return QCoreApplication::translate("ProductionManager", "Select a tree to chop.");
  case HarvestTargetKind::Boulder:
    return QCoreApplication::translate("ProductionManager",
                                       "Select a boulder to collect.");
  case HarvestTargetKind::IronOre:
    return QCoreApplication::translate("ProductionManager",
                                       "Select iron ore to collect.");
  }
  return generic_collect_failure_reason();
}

auto unavailable_builder_reason(HarvestTargetKind kind) -> QString {
  switch (kind) {
  case HarvestTargetKind::Tree:
    return QCoreApplication::translate("ProductionManager",
                                       "No available builder can chop that tree.");
  case HarvestTargetKind::Boulder:
    return QCoreApplication::translate(
        "ProductionManager", "No available builder can collect that boulder.");
  case HarvestTargetKind::IronOre:
    return QCoreApplication::translate(
        "ProductionManager", "No available builder can collect that iron ore.");
  }
  return QCoreApplication::translate("ProductionManager",
                                     "No available builder can collect that resource.");
}

auto no_walkable_harvest_reason(HarvestTargetKind kind) -> QString {
  switch (kind) {
  case HarvestTargetKind::Tree:
    return QCoreApplication::translate("ProductionManager",
                                       "No walkable spot near that tree.");
  case HarvestTargetKind::Boulder:
    return QCoreApplication::translate("ProductionManager",
                                       "No walkable spot near that boulder.");
  case HarvestTargetKind::IronOre:
    return QCoreApplication::translate("ProductionManager",
                                       "No walkable spot near that iron ore.");
  }
  return QCoreApplication::translate("ProductionManager",
                                     "No walkable spot near that resource.");
}

auto harvest_target_kind_for_cell_value(
    Game::Systems::Pathfinding::CellValue cell_value)
    -> std::optional<HarvestTargetKind> {
  switch (cell_value) {
  case Game::Systems::Pathfinding::CellValue::Tree:
    return HarvestTargetKind::Tree;
  case Game::Systems::Pathfinding::CellValue::Boulder:
    return HarvestTargetKind::Boulder;
  case Game::Systems::Pathfinding::CellValue::IronOre:
    return HarvestTargetKind::IronOre;
  case Game::Systems::Pathfinding::CellValue::Walkable:
  case Game::Systems::Pathfinding::CellValue::Blocked:
  case Game::Systems::Pathfinding::CellValue::Forest:
    break;
  }
  return std::nullopt;
}

auto find_nearest_resource_cell(Game::Systems::Pathfinding* pathfinder,
                                const Game::Systems::Point& hover_grid,
                                std::optional<HarvestTargetKind> preferred_kind,
                                int max_search_radius)
    -> std::optional<HarvestResourceCell> {
  if (pathfinder == nullptr || max_search_radius < 0) {
    return std::nullopt;
  }

  pathfinder->update_navigation_grid();

  std::optional<HarvestResourceCell> best_cell;
  int best_distance_sq = std::numeric_limits<int>::max();
  for (int dz = -max_search_radius; dz <= max_search_radius; ++dz) {
    for (int dx = -max_search_radius; dx <= max_search_radius; ++dx) {
      Game::Systems::Point const candidate{hover_grid.x + dx, hover_grid.y + dz};
      auto const candidate_kind = harvest_target_kind_for_cell_value(
          pathfinder->cell_value(candidate.x, candidate.y));
      if (!candidate_kind.has_value()) {
        continue;
      }
      if (preferred_kind.has_value() && *candidate_kind != *preferred_kind) {
        continue;
      }

      int const distance_sq = dx * dx + dz * dz;
      if (!best_cell.has_value() || distance_sq < best_distance_sq) {
        best_cell = HarvestResourceCell{.kind = *candidate_kind, .grid = candidate};
        best_distance_sq = distance_sq;
      }
    }
  }

  return best_cell;
}

constexpr int k_collect_resource_cell_search_radius = 3;
constexpr float k_collect_resource_grid_snap_distance = 1.5F;
constexpr float k_collect_screen_fallback_radius_px = 12.0F;

auto find_specific_harvest_target_by_id(Game::Map::TerrainService& terrain_service,
                                        HarvestTargetKind kind,
                                        std::uint64_t target_id,
                                        const CrewClaims& claims)
    -> std::optional<Game::Map::WorldPropTarget> {
  if (target_id == 0 || prop_taken(terrain_service, target_id, claims)) {
    return std::nullopt;
  }

  switch (kind) {
  case HarvestTargetKind::Tree:
    return terrain_service.find_tree_by_id(target_id);
  case HarvestTargetKind::Boulder:
    return terrain_service.find_boulder_by_id(target_id);
  case HarvestTargetKind::IronOre:
    return terrain_service.find_iron_ore_by_id(target_id);
  }
  return std::nullopt;
}

auto find_harvest_target_near_grid(Game::Map::TerrainService& terrain_service,
                                   HarvestTargetKind kind,
                                   const Game::Systems::Point& grid,
                                   float max_grid_distance)
    -> std::optional<Game::Map::WorldPropTarget> {
  switch (kind) {
  case HarvestTargetKind::Tree:
    return terrain_service.find_tree_near_grid(
        static_cast<float>(grid.x), static_cast<float>(grid.y), max_grid_distance);
  case HarvestTargetKind::Boulder:
    return terrain_service.find_boulder_near_grid(
        static_cast<float>(grid.x), static_cast<float>(grid.y), max_grid_distance);
  case HarvestTargetKind::IronOre:
    return terrain_service.find_iron_ore_near_grid(
        static_cast<float>(grid.x), static_cast<float>(grid.y), max_grid_distance);
  }
  return std::nullopt;
}

auto resolve_preferred_harvest_target(Game::Map::TerrainService& terrain_service,
                                      const QString& item_type,
                                      std::uint64_t target_id,
                                      const CrewClaims& claims)
    -> std::optional<ResolvedHarvestTarget> {
  if (target_id == 0) {
    return std::nullopt;
  }

  if (auto specific_kind = harvest_target_kind_for_item(item_type);
      specific_kind.has_value()) {
    auto target = find_specific_harvest_target_by_id(
        terrain_service, *specific_kind, target_id, claims);
    if (!target.has_value()) {
      return std::nullopt;
    }
    return ResolvedHarvestTarget{.kind = *specific_kind, .target = *target};
  }

  if (!is_collect_item(item_type) || prop_taken(terrain_service, target_id, claims)) {
    return std::nullopt;
  }

  auto target = terrain_service.find_harvestable_world_prop_by_id(target_id);
  if (!target.has_value()) {
    return std::nullopt;
  }

  auto kind = harvest_target_kind_for_world_prop_type(target->type);
  if (!kind.has_value()) {
    return std::nullopt;
  }

  return ResolvedHarvestTarget{.kind = *kind, .target = *target};
}

auto evaluate_resolved_harvest_placement(
    Engine::Core::World* world,
    const std::vector<Engine::Core::EntityID>& builder_ids,
    const ResolvedHarvestTarget& resolved_target) -> HarvestPlacement {
  HarvestPlacement placement;
  placement.kind = resolved_target.kind;
  placement.target = resolved_target.target;
  placement.builder_id = select_best_builder_for_target(
      world, builder_ids, resolved_target.target.x, resolved_target.target.z);
  if (placement.builder_id == 0) {
    placement.failure_reason = unavailable_builder_reason(resolved_target.kind);
    return placement;
  }

  placement.work_position = resolve_harvest_work_position(
      world, placement.builder_id, resolved_target.target);
  if (!placement.work_position.has_value()) {
    placement.failure_reason = no_walkable_harvest_reason(resolved_target.kind);
    return placement;
  }

  return placement;
}

auto point_to_segment_distance_sq(const QPointF& point,
                                  const QPointF& segment_start,
                                  const QPointF& segment_end) -> float {
  auto const seg_dx = static_cast<float>(segment_end.x() - segment_start.x());
  auto const seg_dy = static_cast<float>(segment_end.y() - segment_start.y());
  auto const px = static_cast<float>(point.x() - segment_start.x());
  auto const py = static_cast<float>(point.y() - segment_start.y());
  float const segment_length_sq = seg_dx * seg_dx + seg_dy * seg_dy;
  if (segment_length_sq <= 1e-4F) {
    return px * px + py * py;
  }

  float const t =
      std::clamp((px * seg_dx + py * seg_dy) / segment_length_sq, 0.0F, 1.0F);
  float const closest_x = static_cast<float>(segment_start.x()) + seg_dx * t;
  float const closest_y = static_cast<float>(segment_start.y()) + seg_dy * t;
  float const dx = static_cast<float>(point.x()) - closest_x;
  float const dy = static_cast<float>(point.y()) - closest_y;
  return dx * dx + dy * dy;
}

} // namespace

auto prop_taken(const Game::Map::TerrainService& terrain_service,
                std::uint64_t prop_id,
                const CrewClaims& claims) -> bool {
  return terrain_service.is_world_prop_reserved(prop_id) && !claims.contains(prop_id);
}

auto harvest_product_type(HarvestTargetKind kind) -> const char* {
  switch (kind) {
  case HarvestTargetKind::Tree:
    return k_cut_tree_item_type;
  case HarvestTargetKind::Boulder:
    return k_collect_stone_item_type;
  case HarvestTargetKind::IronOre:
    return k_collect_iron_ore_item_type;
  }
  return k_collect_item_type;
}

auto is_collect_item(const QString& item_type) -> bool {
  return item_type == QLatin1String(k_collect_item_type);
}

auto interaction_highlights_armed(CursorMode cursor_mode,
                                  bool placing_construction,
                                  const QString& pending_item_type) -> bool {
  switch (cursor_mode) {
  case CursorMode::Collect:
  case CursorMode::Repair:
  case CursorMode::Dismantle:
  case CursorMode::Deliver:
    return true;
  case CursorMode::Normal:
  case CursorMode::Patrol:
  case CursorMode::Attack:
  case CursorMode::Guard:
  case CursorMode::PlaceBuilding:
  case CursorMode::Heal:
  case CursorMode::Build:
  case CursorMode::PlaceCommanderRally:
  case CursorMode::PlaceBarracksRally:
    break;
  }

  return placing_construction && is_harvest_construction_item(pending_item_type);
}

auto is_harvest_construction_item(const QString& item_type) -> bool {
  return harvest_target_kind_for_item(item_type).has_value() ||
         is_collect_item(item_type);
}

auto generic_collect_failure_reason() -> QString {
  return QCoreApplication::translate(
      "ProductionManager", "Select a tree, boulder, ore deposit, ripe farm or sheep.");
}

auto crew_claims(Engine::Core::World* world,
                 const std::vector<Engine::Core::EntityID>& builder_ids) -> CrewClaims {
  CrewClaims claims;
  if (world == nullptr) {
    return claims;
  }
  for (const auto id : builder_ids) {
    const auto* entity = world->get_entity(id);
    const auto* builder =
        entity != nullptr
            ? entity->get_component<Engine::Core::BuilderProductionComponent>()
            : nullptr;
    if (builder != nullptr && builder->task_target_reserved) {
      claims.insert(builder->task_target_id);
    }
  }
  return claims;
}

auto resolve_harvest_target_at_position(Game::Map::TerrainService& terrain_service,
                                        const QString& item_type,
                                        const QVector3D& world_position,
                                        const CrewClaims& claims,
                                        std::uint64_t preferred_target_id)
    -> std::optional<ResolvedHarvestTarget> {
  if (!is_harvest_construction_item(item_type)) {
    return std::nullopt;
  }

  if (auto preferred_target = resolve_preferred_harvest_target(
          terrain_service, item_type, preferred_target_id, claims);
      preferred_target.has_value()) {
    return preferred_target;
  }

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    return std::nullopt;
  }

  auto const hover_grid =
      Game::Systems::NavGrid::world_to_grid(world_position.x(), world_position.z());
  auto const resource_grid =
      find_nearest_resource_cell(pathfinder,
                                 hover_grid,
                                 harvest_target_kind_for_item(item_type),
                                 k_collect_resource_cell_search_radius);
  if (!resource_grid.has_value()) {
    return std::nullopt;
  }

  auto const target =
      find_harvest_target_near_grid(terrain_service,
                                    resource_grid->kind,
                                    resource_grid->grid,
                                    k_collect_resource_grid_snap_distance);
  if (!target.has_value()) {
    return std::nullopt;
  }

  return ResolvedHarvestTarget{.kind = resource_grid->kind, .target = *target};
}

auto evaluate_harvest_placement(Engine::Core::World* world,
                                const std::vector<Engine::Core::EntityID>& builder_ids,
                                const QVector3D& world_position,
                                const QString& item_type,
                                std::uint64_t preferred_target_id) -> HarvestPlacement {
  if (auto resolved_target = resolve_harvest_target_at_position(
          Game::Session::session_for(*world).terrain(),
          item_type,
          world_position,
          crew_claims(world, builder_ids),
          preferred_target_id);
      resolved_target.has_value()) {
    return evaluate_resolved_harvest_placement(world, builder_ids, *resolved_target);
  }

  HarvestPlacement placement;
  if (auto kind = harvest_target_kind_for_item(item_type); kind.has_value()) {
    placement.kind = *kind;
    placement.failure_reason = missing_harvest_target_reason(*kind);
  } else {
    placement.failure_reason = generic_collect_failure_reason();
  }
  return placement;
}

auto resolve_harvest_target_from_screen(Game::Map::TerrainService& terrain_service,
                                        const QString& item_type,
                                        const Render::GL::Camera& camera,
                                        const ViewportState& viewport,
                                        const QPointF& screen_point,
                                        const CrewClaims& claims)
    -> std::optional<ResolvedHarvestTarget> {
  if (!is_harvest_construction_item(item_type)) {
    return std::nullopt;
  }

  auto const preferred_kind = harvest_target_kind_for_item(item_type);
  float best_distance_sq = std::numeric_limits<float>::infinity();
  std::optional<ResolvedHarvestTarget> best_target;

  for (const auto& prop : terrain_service.world_props()) {
    if (!Game::Map::is_harvestable_world_prop_type(prop.type) ||
        prop_taken(terrain_service, prop.id, claims)) {
      continue;
    }

    auto const prop_kind = harvest_target_kind_for_world_prop_type(prop.type);
    if (!prop_kind.has_value()) {
      continue;
    }
    if (preferred_kind.has_value() && *prop_kind != *preferred_kind) {
      continue;
    }

    QPointF base_screen;
    QVector3D const base_world = terrain_service.world_prop_world_position(prop);
    if (!Game::Systems::PickingService::world_to_screen(
            camera, viewport.width, viewport.height, base_world, base_screen)) {
      continue;
    }

    QPointF top_screen = base_screen;
    QVector3D const top_world =
        base_world +
        QVector3D(
            0.0F, std::max(4.0F, Game::Map::world_prop_render_scale(prop.type)), 0.0F);
    Game::Systems::PickingService::world_to_screen(
        camera, viewport.width, viewport.height, top_world, top_screen);

    float const distance_sq =
        point_to_segment_distance_sq(screen_point, base_screen, top_screen);
    float const radius = k_collect_screen_fallback_radius_px;
    if (distance_sq > radius * radius || distance_sq >= best_distance_sq) {
      continue;
    }

    auto const resolved_target =
        resolve_preferred_harvest_target(terrain_service, item_type, prop.id, claims);
    if (!resolved_target.has_value()) {
      continue;
    }

    best_distance_sq = distance_sq;
    best_target = resolved_target;
  }

  return best_target;
}

} // namespace App::Economy
