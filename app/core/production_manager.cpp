#include "production_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "app/core/input_command_handler.h"
#include "app/core/order_service.h"
#include "game/audio/audio_cues.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/construction_cost_catalog.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "game/systems/picking_service.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/structure_placement_service.h"
#include "game/systems/troop_profile_service.h"
#include "game/systems/wall_network_service.h"
#include "game/units/building_spawn_setup.h"
#include "game/units/commander_catalog.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "game/util/asset_text.h"
#include "game/visuals/team_colors.h"
#include "scene/camera.h"

ProductionManager::ProductionManager(Engine::Core::World* world,
                                     Game::Systems::PickingService* picking_service,
                                     Render::GL::Camera* camera,
                                     QObject* parent)
    : QObject(parent)
    , m_world(world)
    , m_picking_service(picking_service)
    , m_camera(camera) {
}

namespace {

constexpr auto k_cut_tree_item_type = "cut_tree";
constexpr auto k_collect_item_type = "collect";
constexpr auto k_collect_stone_item_type = "collect_stone";
constexpr auto k_collect_iron_ore_item_type = "collect_iron_ore";
constexpr float k_construction_rotation_step_degrees = 5.0F;
constexpr float k_wall_preview_rotation_step_degrees = 90.0F;

enum class HarvestTargetKind {
  Tree,
  Boulder,
  IronOre,
};

struct HarvestPlacement {
  HarvestTargetKind kind = HarvestTargetKind::Tree;
  std::optional<Game::Map::WorldPropTarget> target;
  std::optional<QVector3D> work_position;
  Engine::Core::EntityID builder_id = 0;
  QString failure_reason;

  [[nodiscard]] auto valid() const -> bool {
    return target.has_value() && work_position.has_value() && builder_id != 0;
  }
};

struct ResolvedHarvestTarget {
  HarvestTargetKind kind = HarvestTargetKind::Tree;
  Game::Map::WorldPropTarget target;
};

struct HarvestResourceCell {
  HarvestTargetKind kind = HarvestTargetKind::Tree;
  Game::Systems::Point grid;
};

struct ConstructionPointerHit {
  QVector3D world_position;
  std::uint64_t harvest_target_id = 0;
};

constexpr int k_harvest_work_search_radius = 16;

auto normalize_rotation_degrees(float angle) -> float {
  while (angle < 0.0F) {
    angle += 360.0F;
  }
  while (angle >= 360.0F) {
    angle -= 360.0F;
  }
  return angle;
}

auto wall_preview_is_vertical(float angle) -> bool {
  int const quarter_turns =
      static_cast<int>(std::round(normalize_rotation_degrees(angle) / 90.0F));
  return (quarter_turns % 2) != 0;
}

auto is_previewable_structure_item(const QString& item_type) -> bool {
  return item_type == QStringLiteral("defense_tower") ||
         item_type == QStringLiteral("barracks") ||
         item_type == QStringLiteral("home") ||
         item_type == QStringLiteral("marketplace") ||
         item_type == QStringLiteral("temple");
}

auto item_supports_preview_rotation(const QString& item_type) -> bool {
  return is_previewable_structure_item(item_type);
}

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
  auto const work_grid = Game::Systems::NavGrid::find_nearest_walkable_grid(
      tree_grid, k_harvest_work_search_radius);
  if (!work_grid.has_value()) {
    return std::nullopt;
  }

  QVector3D work_position = Game::Systems::NavGrid::grid_to_world(*work_grid);
  auto& terrain_service = Game::Map::TerrainService::instance();
  work_position.setY(terrain_service.resolve_surface_world_y(
      work_position.x(), work_position.z(), 0.0F, transform->position.y));
  return work_position;
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

auto is_collect_item(const QString& item_type) -> bool {
  return item_type == QLatin1String(k_collect_item_type);
}

auto resource_amounts_to_variant_map(const Game::Systems::ResourceAmounts& amounts)
    -> QVariantMap {
  QVariantMap map;
  for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
    int const amount = amounts.get(type);
    if (amount > 0) {
      map[QLatin1String(Game::Systems::resource_type_key(type))] = amount;
    }
  }
  return map;
}

auto translated_resource_name(Game::Systems::ResourceType type) -> QString {
  switch (type) {
  case Game::Systems::ResourceType::Gold:
    return QCoreApplication::translate("ProductionManager", "gold");
  case Game::Systems::ResourceType::Food:
    return QCoreApplication::translate("ProductionManager", "food");
  case Game::Systems::ResourceType::Wood:
    return QCoreApplication::translate("ProductionManager", "wood");
  case Game::Systems::ResourceType::Stone:
    return QCoreApplication::translate("ProductionManager", "stone");
  case Game::Systems::ResourceType::Iron:
    return QCoreApplication::translate("ProductionManager", "iron");
  case Game::Systems::ResourceType::Count:
    break;
  }
  return QCoreApplication::translate("ProductionManager", "resources");
}

auto first_missing_resource_name(
    int owner_id, const Game::Systems::ResourceAmounts& cost) -> QString {
  Game::Systems::ResourceAmounts const available =
      Game::Systems::PlayerResourceRegistry::instance().get_all(owner_id);
  for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
    if (available.get(type) < cost.get(type)) {
      return translated_resource_name(type);
    }
  }
  return QCoreApplication::translate("ProductionManager", "resources");
}

auto construction_costs_for_item(const QString& item_type)
    -> Game::Systems::ResourceAmounts {
  return Game::Systems::construction_cost_info(item_type.toStdString()).resource_costs;
}

auto wall_segment_failure_text(const Game::Systems::PlannedWallSegment& segment)
    -> QString {
  switch (segment.fault) {
  case Game::Systems::WallSegmentFault::Occupied:
    return QCoreApplication::translate("ProductionManager",
                                       "Blocked by an existing wall.");
  case Game::Systems::WallSegmentFault::NotEnoughWood:
    return QCoreApplication::translate("ProductionManager", "Not enough wood.");
  case Game::Systems::WallSegmentFault::Invalid:
    return QString::fromStdString(segment.failure_reason);
  case Game::Systems::WallSegmentFault::None:
    break;
  }
  return {};
}

auto wood_per_segment_for(const QString& item_type) -> int {
  return Game::Systems::construction_cost_info(item_type.toStdString())
      .resource_costs.get(Game::Systems::ResourceType::Wood);
}

auto insufficient_construction_resources_reason(
    int owner_id, const Game::Systems::ResourceAmounts& cost) -> QString {
  return QCoreApplication::translate("ProductionManager", "Not enough %1.")
      .arg(first_missing_resource_name(owner_id, cost));
}

auto is_harvest_construction_item(const QString& item_type) -> bool {
  return harvest_target_kind_for_item(item_type).has_value() ||
         is_collect_item(item_type);
}

auto generic_collect_failure_reason() -> QString {
  return QCoreApplication::translate("ProductionManager",
                                     "Select a tree, boulder, or iron ore deposit.");
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

using CrewClaims = std::unordered_set<std::uint64_t>;

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

auto prop_taken(const Game::Map::TerrainService& terrain_service,
                std::uint64_t prop_id,
                const CrewClaims& claims) -> bool {
  return terrain_service.is_world_prop_reserved(prop_id) && !claims.contains(prop_id);
}

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

auto resolve_preferred_harvest_target(const QString& item_type,
                                      std::uint64_t target_id,
                                      const CrewClaims& claims)
    -> std::optional<ResolvedHarvestTarget> {
  if (target_id == 0) {
    return std::nullopt;
  }

  auto& terrain_service = Game::Map::TerrainService::instance();
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

auto resolve_harvest_target_at_position(const QString& item_type,
                                        const QVector3D& world_position,
                                        const CrewClaims& claims,
                                        std::uint64_t preferred_target_id = 0)
    -> std::optional<ResolvedHarvestTarget> {
  if (!is_harvest_construction_item(item_type)) {
    return std::nullopt;
  }

  if (auto preferred_target =
          resolve_preferred_harvest_target(item_type, preferred_target_id, claims);
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

  auto& terrain_service = Game::Map::TerrainService::instance();
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

auto evaluate_harvest_placement(Engine::Core::World* world,
                                const std::vector<Engine::Core::EntityID>& builder_ids,
                                const QVector3D& world_position,
                                const QString& item_type,
                                std::uint64_t preferred_target_id = 0)
    -> HarvestPlacement {
  if (auto resolved_target =
          resolve_harvest_target_at_position(item_type,
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

auto resolve_harvest_target_from_screen(const QString& item_type,
                                        const Render::GL::Camera& camera,
                                        const ViewportState& viewport,
                                        const QPointF& screen_point,
                                        const CrewClaims& claims)
    -> std::optional<ResolvedHarvestTarget> {
  if (!is_harvest_construction_item(item_type)) {
    return std::nullopt;
  }

  auto const preferred_kind = harvest_target_kind_for_item(item_type);
  auto& terrain_service = Game::Map::TerrainService::instance();
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
        resolve_preferred_harvest_target(item_type, prop.id, claims);
    if (!resolved_target.has_value()) {
      continue;
    }

    best_distance_sq = distance_sq;
    best_target = resolved_target;
  }

  return best_target;
}

auto maybe_snap_tower_to_wall_socket(Engine::Core::World* world,
                                     int owner_id,
                                     const QVector3D& world_position) -> QVector3D {
  if (world == nullptr || owner_id <= 0) {
    return world_position;
  }

  const auto snapped = Game::Systems::WallNetworkService::find_tower_snap_socket(
      *world, owner_id, world_position.x(), world_position.z());
  if (!snapped.has_value()) {
    return world_position;
  }

  return Game::Systems::NavGrid::grid_to_world(
      Game::Systems::Point{snapped->x, snapped->z});
}

auto resolve_construction_placement_position(Engine::Core::World* world,
                                             const QString& item_type,
                                             int owner_id,
                                             const QVector3D& world_position)
    -> QVector3D {
  if (item_type == QStringLiteral("defense_tower")) {
    return maybe_snap_tower_to_wall_socket(world, owner_id, world_position);
  }
  return world_position;
}

auto maybe_snap_rotated_wall_preview(Engine::Core::World* world,
                                     const QVector3D& world_position,
                                     bool vertical) -> QVector3D {
  if (world == nullptr) {
    return world_position;
  }

  const auto base = Game::Systems::WallNetworkService::snap_world_position(
      world_position.x(), world_position.z());
  std::optional<Game::Systems::WallGridPosition> best_position;
  float best_distance_sq = std::numeric_limits<float>::infinity();

  const auto consider_candidate = [&](Game::Systems::WallGridPosition candidate) {
    const auto validation =
        Game::Systems::WallNetworkService::validate_wall_segment_placement(
            *world, candidate, true);
    if (!validation.valid) {
      return;
    }

    const QVector3D candidate_world = Game::Systems::NavGrid::grid_to_world(
        Game::Systems::Point{candidate.x, candidate.z});
    const float dx = candidate_world.x() - world_position.x();
    const float dz = candidate_world.z() - world_position.z();
    const float distance_sq = dx * dx + dz * dz;
    if (distance_sq >= best_distance_sq) {
      return;
    }

    best_distance_sq = distance_sq;
    best_position = candidate;
  };

  consider_candidate(base);
  if (vertical) {
    consider_candidate(
        {.x = base.x,
         .z = base.z - Game::Systems::WallNetworkService::k_segment_spacing});
    consider_candidate(
        {.x = base.x,
         .z = base.z + Game::Systems::WallNetworkService::k_segment_spacing});
  } else {
    consider_candidate(
        {.x = base.x - Game::Systems::WallNetworkService::k_segment_spacing,
         .z = base.z});
    consider_candidate(
        {.x = base.x + Game::Systems::WallNetworkService::k_segment_spacing,
         .z = base.z});
  }

  if (!best_position.has_value()) {
    return world_position;
  }

  return Game::Systems::NavGrid::grid_to_world(
      Game::Systems::Point{best_position->x, best_position->z});
}

auto resolve_construction_pointer_hit(Engine::Core::World* world,
                                      const QString& item_type,
                                      int owner_id,
                                      const std::vector<Engine::Core::EntityID>& crew,
                                      const Render::GL::Camera& camera,
                                      const ViewportState& viewport,
                                      const QPointF& screen_point)
    -> std::optional<ConstructionPointerHit> {
  QVector3D hit;
  bool const has_hit =
      is_harvest_construction_item(item_type)
          ? Game::Systems::PickingService::screen_to_ground(
                screen_point, camera, viewport.width, viewport.height, hit)
          : Game::Systems::PickingService::screen_to_surface(
                screen_point, camera, viewport.width, viewport.height, hit);
  if (!has_hit) {
    return std::nullopt;
  }

  if (is_harvest_construction_item(item_type)) {
    const CrewClaims claims = crew_claims(world, crew);
    auto resolved_target = resolve_harvest_target_at_position(item_type, hit, claims);
    if (!resolved_target.has_value()) {
      resolved_target = resolve_harvest_target_from_screen(
          item_type, camera, viewport, screen_point, claims);
    }
    if (!resolved_target.has_value()) {
      return std::nullopt;
    }

    auto& terrain_service = Game::Map::TerrainService::instance();
    return ConstructionPointerHit{
        .world_position = terrain_service.resolve_surface_world_position(
            resolved_target->target.x, resolved_target->target.z),
        .harvest_target_id = resolved_target->target.id};
  }

  return ConstructionPointerHit{
      .world_position =
          resolve_construction_placement_position(world, item_type, owner_id, hit),
      .harvest_target_id = 0};
}

} // namespace

void ProductionManager::start_building_placement(const QString& building_type,
                                                 int local_owner_id) {
  if (building_type.isEmpty() || m_world == nullptr) {
    return;
  }

  if (m_is_placing_construction) {
    on_construction_cancel();
  }

  m_pending_building_type = building_type;
  m_pending_construction_type = building_type;
  m_pending_construction_builders.clear();
  m_is_placing_construction = true;
  m_is_direct_building_placement = true;
  m_active_placement_owner_id = local_owner_id;

  auto& nation_registry = Game::Systems::NationRegistry::instance();
  if (const auto* nation = nation_registry.get_nation_for_player(local_owner_id)) {
    m_active_placement_nation_id = nation->id;
  } else {
    m_active_placement_nation_id = nation_registry.default_nation_id();
  }

  m_construction_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_explicit = false;
  m_pending_harvest_target_id = 0;
  clear_preview_entities();
  clear_construction_preview_summary();
  m_wall_preview_segments.clear();
  m_wall_drag_active = false;
  m_wall_drag_anchor_set = false;
  set_construction_preview_active(false);
  set_construction_preview_valid(false);
  Game::Audio::play_cue(Game::Audio::Cue::k_build_placement_begin);
  emit placing_construction_changed();
}

void ProductionManager::place_building_at_screen(qreal sx,
                                                 qreal sy,
                                                 int local_owner_id,
                                                 const ViewportState& viewport) {
  if (m_pending_building_type.isEmpty() || !m_is_direct_building_placement) {
    return;
  }

  m_active_placement_owner_id = local_owner_id;
  on_construction_mouse_move(sx, sy, viewport);
  if (m_construction_preview_active) {
    on_construction_confirm();
  }
}

void ProductionManager::cancel_building_placement() {
  if (m_is_direct_building_placement) {
    on_construction_cancel();
    return;
  }
  m_pending_building_type.clear();
}

void ProductionManager::reset_transient_state() {
  cancel_building_placement();

  if (m_is_placing_construction) {
    on_construction_cancel();
  }

  m_pending_construction_type.clear();
  m_pending_construction_builders.clear();
  m_construction_placement_position = QVector3D();
  m_is_placing_construction = false;
  m_is_direct_building_placement = false;
  m_active_placement_owner_id = 0;
  m_active_placement_nation_id = Game::Systems::NationID::RomanRepublic;
  m_construction_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_explicit = false;
  m_pending_harvest_target_id = 0;
  set_construction_preview_active(false);
  set_construction_preview_valid(false);
  clear_construction_preview_summary();
  clear_preview_entities();
  m_wall_drag_active = false;
  m_wall_drag_anchor_set = false;
}

void ProductionManager::on_construction_mouse_move(qreal sx,
                                                   qreal sy,
                                                   const ViewportState& viewport) {
  if (!m_is_placing_construction || (m_picking_service == nullptr) ||
      (m_camera == nullptr)) {
    return;
  }

  QPointF const screenPt(sx, sy);
  auto const resolved_hit =
      resolve_construction_pointer_hit(m_world,
                                       m_pending_construction_type,
                                       pending_construction_owner_id(),
                                       m_pending_construction_builders,
                                       *m_camera,
                                       viewport,
                                       screenPt);

  if (is_wall_construction_mode()) {
    if (!resolved_hit.has_value()) {
      clear_preview_entities();
      m_wall_preview_segments.clear();
      clear_construction_preview_summary();
      if (!m_wall_drag_active) {
        m_wall_drag_anchor_set = false;
      }
      set_construction_preview_active(false);
      set_construction_preview_valid(false);
      return;
    }
    m_construction_placement_position = resolved_hit->world_position;
    if (!m_wall_drag_active) {
      m_wall_drag_anchor_set = true;
      m_wall_drag_anchor_world = resolved_hit->world_position;
    }
    rebuild_wall_preview_plan(resolved_hit->world_position);
    return;
  }

  if (resolved_hit.has_value()) {
    m_pending_harvest_target_id = resolved_hit->harvest_target_id;
    m_construction_placement_position = resolved_hit->world_position;
    update_non_wall_construction_preview(resolved_hit->world_position);
    return;
  }

  clear_non_wall_construction_preview();
  m_pending_harvest_target_id = 0;
  set_construction_preview_active(false);
  set_construction_preview_valid(false);
}

void ProductionManager::on_construction_pointer_pressed(qreal sx,
                                                        qreal sy,
                                                        const ViewportState& viewport) {
  if (!m_is_placing_construction || !is_wall_construction_mode() ||
      (m_picking_service == nullptr) || (m_camera == nullptr)) {
    return;
  }

  auto const resolved_hit =
      resolve_construction_pointer_hit(m_world,
                                       m_pending_construction_type,
                                       pending_construction_owner_id(),
                                       m_pending_construction_builders,
                                       *m_camera,
                                       viewport,
                                       QPointF(sx, sy));
  if (!resolved_hit.has_value()) {
    return;
  }

  m_wall_drag_active = true;
  m_wall_drag_anchor_set = true;
  m_wall_drag_anchor_world = resolved_hit->world_position;
  m_construction_placement_position = resolved_hit->world_position;
  rebuild_wall_preview_plan(resolved_hit->world_position);
}

void ProductionManager::on_construction_pointer_released(
    qreal sx, qreal sy, const ViewportState& viewport) {
  if (!m_is_placing_construction) {
    return;
  }

  if (!is_wall_construction_mode()) {
    if ((m_picking_service != nullptr) && (m_camera != nullptr)) {
      QPointF const screen_point(sx, sy);
      auto const resolved_hit =
          resolve_construction_pointer_hit(m_world,
                                           m_pending_construction_type,
                                           pending_construction_owner_id(),
                                           m_pending_construction_builders,
                                           *m_camera,
                                           viewport,
                                           screen_point);
      if (resolved_hit.has_value()) {
        m_pending_harvest_target_id = resolved_hit->harvest_target_id;
        m_construction_placement_position = resolved_hit->world_position;
        update_non_wall_construction_preview(m_construction_placement_position);
      }
    }
    on_construction_confirm();
    return;
  }

  if (!m_wall_drag_anchor_set) {
    return;
  }

  QVector3D hit = m_construction_placement_position;
  bool release_hit_valid = false;
  if ((m_picking_service != nullptr) && (m_camera != nullptr)) {
    if (auto const resolved_hit =
            resolve_construction_pointer_hit(m_world,
                                             m_pending_construction_type,
                                             pending_construction_owner_id(),
                                             m_pending_construction_builders,
                                             *m_camera,
                                             viewport,
                                             QPointF(sx, sy));
        resolved_hit.has_value()) {
      hit = resolved_hit->world_position;
      release_hit_valid = true;
    }
  }

  if (!release_hit_valid && !m_construction_preview_active) {
    return;
  }

  m_construction_placement_position = hit;
  rebuild_wall_preview_plan(hit);
  if (!m_construction_preview_active) {
    return;
  }
  confirm_wall_construction_plan();
}

void ProductionManager::on_construction_confirm() {
  if (!m_is_placing_construction) {
    on_construction_cancel();
    return;
  }

  if (!m_is_direct_building_placement && m_pending_construction_builders.empty()) {
    on_construction_cancel();
    return;
  }

  if (is_wall_construction_mode()) {
    if (!m_construction_preview_active) {
      emit construction_placement_rejected(QCoreApplication::translate(
          "ProductionManager", "Drag out a wall line first."));
      return;
    }
    confirm_wall_construction_plan();
    return;
  }

  if (!m_construction_preview_active) {
    emit construction_placement_rejected(
        is_harvest_construction_item(m_pending_construction_type)
            ? generic_collect_failure_reason()
            : QCoreApplication::translate("ProductionManager",
                                          "Choose a build location."));
    return;
  }

  if (m_is_direct_building_placement) {
    confirm_direct_building_placement();
    return;
  }

  if (is_harvest_construction_item(m_pending_construction_type)) {
    HarvestPlacement const placement =
        evaluate_harvest_placement(m_world,
                                   m_pending_construction_builders,
                                   m_construction_placement_position,
                                   m_pending_construction_type,
                                   m_pending_harvest_target_id);
    if (!placement.valid()) {
      set_construction_preview_valid(false);
      emit construction_placement_rejected(placement.failure_reason);
      return;
    }

    if (prop_taken(Game::Map::TerrainService::instance(),
                   placement.target->id,
                   crew_claims(m_world, m_pending_construction_builders))) {
      set_construction_preview_valid(false);
      emit construction_placement_rejected(QCoreApplication::translate(
          "ProductionManager", "That resource is already assigned."));
      return;
    }

    std::vector<Engine::Core::EntityID> crew;
    crew.reserve(m_pending_construction_builders.size());
    crew.push_back(placement.builder_id);
    for (auto id : m_pending_construction_builders) {
      if (id != placement.builder_id) {
        crew.push_back(id);
      }
    }
    {
      App::Core::OrderRequest request;
      request.kind = App::Core::OrderKind::Gather;
      request.payload = Game::Command::StartHarvest{
          .units = std::move(crew),
          .construction_type = harvest_product_type(placement.kind),
          .resource_target = placement.target->id,
          .site = QVector3D(placement.target->x, 0.0F, placement.target->z)};
      request.has_destination = true;
      request.destination = QVector3D(placement.target->x, 0.0F, placement.target->z);
      emit order_feedback(App::Core::submit_player_order(
          *m_world, pending_construction_owner_id(), std::move(request)));
    }

    m_is_placing_construction = false;
    m_is_direct_building_placement = false;
    m_active_placement_owner_id = 0;
    m_active_placement_nation_id = Game::Systems::NationID::RomanRepublic;
    m_pending_construction_type.clear();
    m_pending_building_type.clear();
    m_pending_construction_builders.clear();
    m_construction_preview_rotation_y = 0.0F;
    m_pending_harvest_target_id = 0;
    clear_preview_entities();
    set_construction_preview_active(false);
    set_construction_preview_valid(false);
    emit placing_construction_changed();
    return;
  }

  if (!Game::Systems::StructurePlacementService::footprint_is_clear(
          m_construction_placement_position.x(),
          m_construction_placement_position.z(),
          m_pending_construction_type.toStdString())) {

    emit construction_placement_rejected(
        QCoreApplication::translate("ProductionManager", "Cannot build there."));
    return;
  }

  {
    const int owner_id = pending_construction_owner_id();
    const Game::Systems::ResourceAmounts resource_costs =
        construction_costs_for_item(m_pending_construction_type);
    if (!resource_costs.empty() &&
        !Game::Systems::PlayerResourceRegistry::instance().has_at_least(
            owner_id, resource_costs)) {
      emit construction_placement_rejected(
          insufficient_construction_resources_reason(owner_id, resource_costs));
      return;
    }
    App::Core::OrderRequest request;
    request.kind = App::Core::OrderKind::Build;
    request.payload = Game::Command::StartConstruction{
        .units = m_pending_construction_builders,
        .construction_type = m_pending_construction_type.toStdString(),
        .site = m_construction_placement_position,
        .rotation_y = m_construction_preview_rotation_y};
    request.has_destination = true;
    request.destination = m_construction_placement_position;
    emit order_feedback(
        App::Core::submit_player_order(*m_world, owner_id, std::move(request)));
  }

  m_is_placing_construction = false;
  m_is_direct_building_placement = false;
  m_active_placement_owner_id = 0;
  m_active_placement_nation_id = Game::Systems::NationID::RomanRepublic;
  m_pending_construction_type.clear();
  m_pending_building_type.clear();
  m_pending_construction_builders.clear();
  m_construction_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_explicit = false;
  m_pending_harvest_target_id = 0;
  clear_preview_entities();
  set_construction_preview_active(false);
  set_construction_preview_valid(false);
  emit placing_construction_changed();
}

void ProductionManager::on_construction_cancel() {
  if (!m_is_placing_construction) {
    return;
  }

  clear_preview_entities();
  clear_construction_preview_summary();
  m_wall_preview_segments.clear();
  m_wall_drag_active = false;
  m_wall_drag_anchor_set = false;

  m_is_placing_construction = false;
  m_is_direct_building_placement = false;
  m_active_placement_owner_id = 0;
  m_active_placement_nation_id = Game::Systems::NationID::RomanRepublic;
  m_pending_construction_type.clear();
  m_pending_building_type.clear();
  m_pending_construction_builders.clear();
  m_construction_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_explicit = false;
  m_pending_harvest_target_id = 0;
  set_construction_preview_active(false);
  set_construction_preview_valid(false);
  emit placing_construction_changed();
}

void ProductionManager::start_builder_construction(const QString& item_type) {
  if (m_world == nullptr) {
    return;
  }

  if (m_is_placing_construction) {
    on_construction_cancel();
  }

  m_pending_construction_builders = collect_available_builders();
  if (m_pending_construction_builders.empty()) {
    return;
  }

  m_pending_construction_type = item_type;
  m_pending_building_type.clear();
  m_is_placing_construction = true;
  m_is_direct_building_placement = false;
  m_construction_placement_position =
      calculate_builder_center_position(m_pending_construction_builders);
  clear_preview_entities();
  clear_construction_preview_summary();
  m_wall_preview_segments.clear();
  m_wall_drag_active = false;
  m_wall_drag_anchor_set = false;
  m_construction_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_explicit = false;
  m_pending_harvest_target_id = 0;
  m_active_placement_owner_id = pending_construction_owner_id();
  m_active_placement_nation_id = pending_construction_nation_id();

  set_construction_preview_active(false);
  set_construction_preview_valid(false);
  Game::Audio::play_cue(Game::Audio::Cue::k_build_placement_begin);
  emit placing_construction_changed();
}

void ProductionManager::set_construction_preview_active(bool active) {
  if (m_construction_preview_active == active) {
    return;
  }
  m_construction_preview_active = active;
  emit construction_preview_active_changed();
}

void ProductionManager::set_construction_preview_valid(bool valid) {
  if (m_construction_preview_valid == valid) {
    return;
  }
  m_construction_preview_valid = valid;
  emit construction_preview_valid_changed();
}

void ProductionManager::clear_construction_preview_summary() {
  set_construction_preview_summary(0, 0, 0);
}

void ProductionManager::on_construction_scroll(float delta) {
  if (!m_is_placing_construction || !m_construction_preview_active) {
    return;
  }

  if (is_wall_construction_mode()) {
    m_wall_preview_rotation_y = normalize_rotation_degrees(
        m_wall_preview_rotation_y + delta * k_wall_preview_rotation_step_degrees);
    m_wall_preview_rotation_explicit = true;
    rebuild_wall_preview_plan(m_construction_placement_position);
    return;
  }

  if (!item_supports_preview_rotation(m_pending_construction_type)) {
    return;
  }

  m_construction_preview_rotation_y = normalize_rotation_degrees(
      m_construction_preview_rotation_y + delta * k_construction_rotation_step_degrees);
  rebuild_non_wall_preview_entity(m_construction_placement_position);
}

void ProductionManager::update_non_wall_construction_preview(
    const QVector3D& world_position) {
  if (m_world == nullptr || is_wall_construction_mode()) {
    return;
  }

  set_construction_preview_active(true);
  if (is_harvest_construction_item(m_pending_construction_type)) {
    HarvestPlacement const placement =
        evaluate_harvest_placement(m_world,
                                   m_pending_construction_builders,
                                   world_position,
                                   m_pending_construction_type,
                                   m_pending_harvest_target_id);
    set_construction_preview_valid(placement.valid());
  } else {
    set_construction_preview_valid(
        Game::Systems::StructurePlacementService::footprint_is_clear(
            world_position.x(),
            world_position.z(),
            m_pending_construction_type.toStdString()));
  }
  rebuild_non_wall_preview_entity(world_position);
}

void ProductionManager::clear_non_wall_construction_preview() {
  m_pending_harvest_target_id = 0;
  clear_preview_entities();
  set_construction_preview_active(false);
}

auto ProductionManager::construction_preview_rotatable() const -> bool {
  return m_is_placing_construction && m_construction_preview_active &&
         (is_wall_construction_mode() ||
          item_supports_preview_rotation(m_pending_construction_type));
}

void ProductionManager::set_construction_preview_summary(int segment_count,
                                                         int valid_segment_count,
                                                         int total_cost) {
  if (m_construction_preview_segment_count == segment_count &&
      m_construction_preview_valid_segment_count == valid_segment_count &&
      m_construction_preview_total_cost == total_cost) {
    return;
  }

  m_construction_preview_segment_count = segment_count;
  m_construction_preview_valid_segment_count = valid_segment_count;
  m_construction_preview_total_cost = total_cost;
  emit construction_preview_summary_changed();
}

auto ProductionManager::pending_construction_owner_id() const -> int {
  if (m_active_placement_owner_id > 0) {
    return m_active_placement_owner_id;
  }

  if (m_world == nullptr) {
    return 0;
  }

  for (auto builder_id : m_pending_construction_builders) {
    auto* entity = m_world->get_entity(builder_id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if (unit != nullptr) {
      return unit->owner_id;
    }
  }
  return 0;
}

auto ProductionManager::pending_construction_nation_id() const
    -> Game::Systems::NationID {
  if (m_active_placement_owner_id > 0) {
    return m_active_placement_nation_id;
  }

  if (m_world != nullptr) {
    for (auto builder_id : m_pending_construction_builders) {
      auto* entity = m_world->get_entity(builder_id);
      auto* unit = entity != nullptr
                       ? entity->get_component<Engine::Core::UnitComponent>()
                       : nullptr;
      if (unit != nullptr) {
        return unit->nation_id;
      }
    }
  }

  return Game::Systems::NationRegistry::instance().default_nation_id();
}

auto ProductionManager::is_wall_construction_mode() const -> bool {
  return m_pending_construction_type == QStringLiteral("wall_segment") ||
         is_gate_construction_mode();
}

auto ProductionManager::is_gate_construction_mode() const -> bool {
  return m_pending_construction_type == QStringLiteral("wall_gate");
}

void ProductionManager::clear_preview_entities() {
  if (m_world == nullptr) {
    m_preview_entity_ids.clear();
    return;
  }

  for (auto entity_id : m_preview_entity_ids) {
    if (m_world->get_entity(entity_id) != nullptr) {
      m_world->destroy_entity(entity_id);
    }
  }
  m_preview_entity_ids.clear();
}

void ProductionManager::append_preview_entity(const QString& item_type,
                                              const QVector3D& world_position,
                                              bool valid,
                                              float rotation_y,
                                              const std::string* renderer_override) {
  if (m_world == nullptr) {
    return;
  }

  auto* entity = m_world->create_entity();
  if (entity == nullptr) {
    return;
  }

  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* preview = entity->add_component<Engine::Core::ConstructionPreviewComponent>();
  if (transform == nullptr || preview == nullptr) {
    m_world->destroy_entity(entity->get_id());
    return;
  }

  auto& terrain_service = Game::Map::TerrainService::instance();
  QVector3D resolved_position = world_position;
  if (terrain_service.is_initialized()) {
    resolved_position = terrain_service.resolve_surface_world_position(
        world_position.x(), world_position.z(), 0.0F, world_position.y());
  }

  transform->position = {
      resolved_position.x(), resolved_position.y(), resolved_position.z()};
  transform->rotation = {0.0F, rotation_y, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};

  auto* renderable =
      Game::Units::add_building_renderable(*entity,
                                           pending_construction_owner_id(),
                                           pending_construction_nation_id(),
                                           item_type.toStdString());
  if (renderable == nullptr) {
    m_world->destroy_entity(entity->get_id());
    return;
  }

  renderable->visible = false;
  if (renderer_override != nullptr) {
    renderable->renderer_id = *renderer_override;
  }

  preview->owner_id = pending_construction_owner_id();
  preview->nation_id = pending_construction_nation_id();
  preview->valid = valid;

  m_preview_entity_ids.push_back(entity->get_id());
}

void ProductionManager::rebuild_non_wall_preview_entity(
    const QVector3D& world_position) {
  clear_preview_entities();
  if (!is_previewable_structure_item(m_pending_construction_type)) {
    return;
  }

  append_preview_entity(m_pending_construction_type,
                        world_position,
                        m_construction_preview_valid,
                        item_supports_preview_rotation(m_pending_construction_type)
                            ? m_construction_preview_rotation_y
                            : 0.0F);
}

void ProductionManager::rebuild_wall_preview_entities() {
  const bool gate_mode = is_gate_construction_mode();
  clear_preview_entities();
  if (m_world == nullptr || m_wall_preview_segments.empty()) {
    return;
  }

  auto& terrain_service = Game::Map::TerrainService::instance();
  const int owner_id = pending_construction_owner_id();
  const auto nation_id = pending_construction_nation_id();
  const QVector3D team_color = Game::Visuals::team_colorForOwner(owner_id);

  for (const auto& segment : m_wall_preview_segments) {
    auto* entity = m_world->create_entity();
    if (entity == nullptr) {
      continue;
    }

    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    auto* renderable = entity->add_component<Engine::Core::RenderableComponent>("", "");
    auto* preview = entity->add_component<Engine::Core::ConstructionPreviewComponent>();
    if (transform == nullptr || renderable == nullptr || preview == nullptr) {
      m_world->destroy_entity(entity->get_id());
      continue;
    }

    QVector3D world_position = segment.world_position;
    if (terrain_service.is_initialized()) {
      world_position = terrain_service.resolve_surface_world_position(
          world_position.x(), world_position.z(), 0.0F, world_position.y());
    }

    transform->position = {world_position.x(), world_position.y(), world_position.z()};
    transform->rotation = {0.0F, segment.rotation_y, 0.0F};
    transform->scale = {1.0F, 1.0F, 1.0F};

    renderable->visible = false;
    renderable->mesh = Engine::Core::RenderableComponent::MeshKind::Cube;
    renderable->renderer_id =
        (gate_mode ? Game::Systems::WallNetworkService::resolve_gate_appearance(
                         nation_id, segment.connection_mask, segment.rotation_y)
                   : Game::Systems::WallNetworkService::resolve_appearance(
                         nation_id, segment.connection_mask))
            .renderer_id;
    renderable->color[0] = team_color.x();
    renderable->color[1] = team_color.y();
    renderable->color[2] = team_color.z();

    preview->owner_id = owner_id;
    preview->nation_id = nation_id;
    preview->grid_x = segment.grid_x;
    preview->grid_z = segment.grid_z;
    preview->valid = segment.valid;

    m_preview_entity_ids.push_back(entity->get_id());
  }
}

void ProductionManager::rebuild_wall_preview_plan(
    const QVector3D& current_world_position) {
  m_wall_preview_segments.clear();
  clear_preview_entities();

  if (m_world == nullptr || !m_wall_drag_anchor_set) {
    set_construction_preview_active(false);
    set_construction_preview_valid(false);
    clear_construction_preview_summary();
    return;
  }

  QVector3D preview_world_position = current_world_position;
  if (!m_wall_drag_active && m_wall_preview_rotation_explicit) {
    preview_world_position = maybe_snap_rotated_wall_preview(
        m_world,
        current_world_position,
        wall_preview_is_vertical(m_wall_preview_rotation_y));
  }

  const QVector3D anchor_world =
      m_wall_drag_active ? m_wall_drag_anchor_world : preview_world_position;
  const auto anchor = Game::Systems::WallNetworkService::snap_world_position(
      anchor_world.x(), anchor_world.z());
  auto target = Game::Systems::WallNetworkService::snap_world_position(
      preview_world_position.x(), preview_world_position.z());
  if (m_wall_drag_active && m_wall_preview_rotation_explicit) {
    if (wall_preview_is_vertical(m_wall_preview_rotation_y)) {
      target.x = anchor.x;
    } else {
      target.z = anchor.z;
    }
  }

  m_wall_plan_request =
      Game::Systems::WallPlanRequest{.owner_id = pending_construction_owner_id(),
                                     .gate = is_gate_construction_mode(),
                                     .anchor = anchor,
                                     .target = target,
                                     .rotation_y = m_wall_preview_rotation_y};
  const auto plan = Game::Systems::WallPlanService::plan(*m_world, m_wall_plan_request);
  m_wall_preview_segments = plan.segments;
  const int valid_segment_count = plan.valid_count;
  const int wood_cost = plan.wood_per_segment;

  set_construction_preview_active(!m_wall_preview_segments.empty());
  set_construction_preview_valid(valid_segment_count > 0);
  set_construction_preview_summary(static_cast<int>(m_wall_preview_segments.size()),
                                   valid_segment_count,
                                   valid_segment_count * wood_cost);
  rebuild_wall_preview_entities();
}

void ProductionManager::confirm_wall_construction_plan() {
  const int valid_segment_count = static_cast<int>(
      std::count_if(m_wall_preview_segments.begin(),
                    m_wall_preview_segments.end(),
                    [](const auto& segment) { return segment.valid; }));
  if (valid_segment_count <= 0) {
    QString reason = QCoreApplication::translate(
        "ProductionManager", "No valid wall segments in that drag.");
    for (const auto& segment : m_wall_preview_segments) {
      if (segment.fault != Game::Systems::WallSegmentFault::None) {
        reason = wall_segment_failure_text(segment);
        break;
      }
    }
    emit construction_placement_rejected(reason);
    set_construction_preview_active(!m_wall_preview_segments.empty());
    set_construction_preview_valid(false);
    return;
  }

  const int owner_id = pending_construction_owner_id();
  Game::Systems::ResourceAmounts total_cost;
  total_cost.set(Game::Systems::ResourceType::Wood,
                 valid_segment_count *
                     wood_per_segment_for(m_pending_construction_type));
  if (!Game::Systems::PlayerResourceRegistry::instance().has_at_least(owner_id,
                                                                      total_cost)) {
    emit construction_placement_rejected(
        insufficient_construction_resources_reason(owner_id, total_cost));
    set_construction_preview_active(!m_wall_preview_segments.empty());
    set_construction_preview_valid(false);
    return;
  }

  if (m_pending_construction_builders.empty()) {
    emit construction_placement_rejected(
        QCoreApplication::translate("ProductionManager", "No available builder."));
    return;
  }

  {
    App::Core::OrderRequest request;
    request.kind = App::Core::OrderKind::Build;
    request.payload =
        Game::Command::PlaceWallPlan{.units = m_pending_construction_builders,
                                     .gate = m_wall_plan_request.gate,
                                     .anchor_x = m_wall_plan_request.anchor.x,
                                     .anchor_z = m_wall_plan_request.anchor.z,
                                     .target_x = m_wall_plan_request.target.x,
                                     .target_z = m_wall_plan_request.target.z,
                                     .rotation_y = m_wall_plan_request.rotation_y};
    request.has_destination = true;
    request.destination = m_construction_placement_position;
    emit order_feedback(
        App::Core::submit_player_order(*m_world, owner_id, std::move(request)));
  }

  clear_preview_entities();
  m_wall_preview_segments.clear();
  m_wall_drag_active = false;
  m_wall_drag_anchor_set = false;
  m_is_placing_construction = false;
  m_is_direct_building_placement = false;
  m_active_placement_owner_id = 0;
  m_active_placement_nation_id = Game::Systems::NationID::RomanRepublic;
  m_pending_construction_type.clear();
  m_pending_building_type.clear();
  m_pending_construction_builders.clear();
  m_construction_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_explicit = false;
  set_construction_preview_active(false);
  set_construction_preview_valid(false);
  clear_construction_preview_summary();
  emit placing_construction_changed();
}

void ProductionManager::confirm_direct_building_placement() {
  if (!m_is_direct_building_placement || m_world == nullptr) {
    return;
  }

  const int owner_id = pending_construction_owner_id();
  const std::string building_type = m_pending_construction_type.toStdString();
  switch (Game::Systems::StructurePlacementService::ruling(
      *m_world, owner_id, building_type, m_construction_placement_position)) {
  case Game::Systems::PlacementRuling::Blocked:
    emit construction_placement_rejected(
        QCoreApplication::translate("ProductionManager", "Cannot build there."));
    return;
  case Game::Systems::PlacementRuling::UnknownStructure:
    emit construction_placement_rejected(QCoreApplication::translate(
        "ProductionManager", "That structure cannot be placed."));
    return;
  case Game::Systems::PlacementRuling::Unaffordable:
    emit construction_placement_rejected(insufficient_construction_resources_reason(
        owner_id, construction_costs_for_item(m_pending_construction_type)));
    return;
  case Game::Systems::PlacementRuling::NoFactory:
  case Game::Systems::PlacementRuling::SpawnFailed:
    emit construction_placement_rejected(QCoreApplication::translate(
        "ProductionManager", "Building factory unavailable."));
    return;
  case Game::Systems::PlacementRuling::Ok:
    break;
  }

  {
    App::Core::OrderRequest request;
    request.kind = App::Core::OrderKind::Build;
    request.payload = Game::Command::PlaceBuilding{
        .building_type = building_type,
        .position = m_construction_placement_position,
        .rotation_y = item_supports_preview_rotation(m_pending_construction_type)
                          ? m_construction_preview_rotation_y
                          : 0.0F};
    request.has_destination = true;
    request.destination = m_construction_placement_position;
    emit order_feedback(
        App::Core::submit_player_order(*m_world, owner_id, std::move(request)));
  }

  clear_preview_entities();
  m_wall_preview_segments.clear();
  m_wall_drag_active = false;
  m_wall_drag_anchor_set = false;
  m_is_placing_construction = false;
  m_is_direct_building_placement = false;
  m_active_placement_owner_id = 0;
  m_active_placement_nation_id = Game::Systems::NationID::RomanRepublic;
  m_pending_construction_type.clear();
  m_pending_building_type.clear();
  m_pending_construction_builders.clear();
  m_construction_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_y = 0.0F;
  m_wall_preview_rotation_explicit = false;
  set_construction_preview_active(false);
  set_construction_preview_valid(false);
  clear_construction_preview_summary();
  emit placing_construction_changed();
}

auto ProductionManager::get_selected_production_state(int local_owner_id) const
    -> QVariantMap {
  QVariantMap m;
  m["has_barracks"] = false;
  m["in_progress"] = false;
  m["time_remaining"] = 0.0;
  m["build_time"] = 0.0;
  m["produced_count"] = 0;
  m["max_units"] = 0;
  m["villager_cost"] = 1;
  m["manpower_available"] = 0;

  if (m_world == nullptr) {
    return m;
  }

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }

  Game::Systems::ProductionState st;
  Game::Systems::ProductionService::get_selected_barracks_state(
      *m_world, selection_system->get_selected_units(), local_owner_id, st);

  m["has_barracks"] = st.has_barracks;
  m["in_progress"] = st.in_progress;
  m["product_type"] =
      QString::fromStdString(Game::Units::troop_typeToString(st.product_type));
  m["time_remaining"] = st.time_remaining;
  m["build_time"] = st.build_time;
  m["produced_count"] = st.produced_count;
  m["max_units"] = st.max_units;
  m["villager_cost"] = st.villager_cost;
  m["manpower_available"] = st.manpower_available;
  m["queue_size"] = st.queue_size;
  m["nation_id"] =
      QString::fromStdString(Game::Systems::nation_id_to_string(st.nation_id));

  QVariantList queue_list;
  for (const auto& unit_type : st.production_queue) {
    queue_list.append(
        QString::fromStdString(Game::Units::troop_typeToString(unit_type)));
  }
  m["production_queue"] = queue_list;

  return m;
}

auto ProductionManager::get_selected_builder_production_state() const -> QVariantMap {
  QVariantMap m;
  m["in_progress"] = false;
  m["time_remaining"] = 0.0;
  m["build_time"] = 10.0;
  m["product_type"] = "";

  if (m_world == nullptr) {
    return m;
  }

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }

  const auto& selected = selection_system->get_selected_units();
  for (auto id : selected) {
    auto* e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* builder_prod = e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod != nullptr) {
      m["in_progress"] =
          builder_prod->in_progress || builder_prod->has_construction_site;
      m["time_remaining"] = builder_prod->time_remaining;
      m["build_time"] = builder_prod->build_time;
      m["product_type"] = QString::fromStdString(builder_prod->product_type);
      return m;
    }
  }

  return m;
}

auto ProductionManager::get_selected_marketplace_state(int local_owner_id) const
    -> QVariantMap {
  QVariantMap m;
  m["has_marketplace"] = false;
  m["nation_id"] = "";
  m["trade_quantity"] = 0;
  m["buy_prices"] = QVariantMap{};
  m["sell_prices"] = QVariantMap{};

  if (m_world == nullptr) {
    return m;
  }

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }

  auto const& selected = selection_system->get_selected_units();
  for (auto id : selected) {
    auto* e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* unit = e->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->spawn_type != Game::Units::SpawnType::Marketplace ||
        unit->owner_id != local_owner_id) {
      continue;
    }

    auto const& rates = Game::Systems::MarketplaceSystem::instance().get_rates();
    QVariantMap buy_prices;
    buy_prices["wood"] = rates.buy_price_wood;
    buy_prices["stone"] = rates.buy_price_stone;
    buy_prices["iron"] = rates.buy_price_iron;

    QVariantMap sell_prices;
    sell_prices["wood"] = rates.sell_price_wood;
    sell_prices["stone"] = rates.sell_price_stone;
    sell_prices["iron"] = rates.sell_price_iron;

    m["has_marketplace"] = true;
    m["nation_id"] =
        QString::fromStdString(Game::Systems::nation_id_to_string(unit->nation_id));
    m["trade_quantity"] = rates.trade_quantity;
    m["buy_prices"] = buy_prices;
    m["sell_prices"] = sell_prices;
    return m;
  }

  return m;
}

auto ProductionManager::get_selected_home_production_state(int local_owner_id) const
    -> QVariantMap {
  QVariantMap m;
  m["has_home"] = false;
  m["in_progress"] = false;
  m["time_remaining"] = 0.0;
  m["build_time"] = 0.0;
  m["produced_count"] = 0;
  m["max_units"] = 0;
  m["villager_cost"] = 1;
  m["manpower_available"] = 0;
  m["queue_size"] = 0;
  m["product_type"] = "";
  m["production_queue"] = QVariantList{};
  m["nation_id"] = "";

  if (m_world == nullptr) {
    return m;
  }

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }

  Game::Systems::ProductionState st;
  Game::Systems::ProductionService::get_selected_home_state(
      *m_world, selection_system->get_selected_units(), local_owner_id, st);

  m["has_home"] = st.has_home;
  m["in_progress"] = st.in_progress;
  m["product_type"] =
      QString::fromStdString(Game::Units::troop_typeToString(st.product_type));
  m["time_remaining"] = st.time_remaining;
  m["build_time"] = st.build_time;
  m["produced_count"] = st.produced_count;
  m["max_units"] = st.max_units;
  m["villager_cost"] = st.villager_cost;
  m["manpower_available"] = st.manpower_available;
  m["queue_size"] = st.queue_size;
  m["nation_id"] =
      QString::fromStdString(Game::Systems::nation_id_to_string(st.nation_id));

  QVariantList queue_list;
  for (const auto& unit_type : st.production_queue) {
    queue_list.append(
        QString::fromStdString(Game::Units::troop_typeToString(unit_type)));
  }
  m["production_queue"] = queue_list;

  return m;
}

auto ProductionManager::get_unit_production_info(
    const QString& unit_type, const QString& nation_id) const -> QVariantMap {
  QVariantMap info;
  const auto& config = Game::Units::TroopConfig::instance();
  std::string const type_str = unit_type.toStdString();

  info["cost"] = config.get_production_cost(type_str);
  info["population_cost"] = info["cost"];
  info["resource_costs"] = QVariantMap{};
  info["build_time"] = static_cast<double>(config.get_build_time(type_str));
  info["individuals_per_unit"] = config.get_individuals_per_unit(type_str);

  auto troop_type_opt = Game::Units::try_parse_troop_type(type_str);
  if (troop_type_opt.has_value()) {
    auto nation_id_opt = Game::Systems::nation_id_from_string(nation_id.toStdString());
    auto nation_id_enum = nation_id_opt.value_or(
        Game::Systems::NationRegistry::instance().default_nation_id());
    auto profile = Game::Systems::TroopProfileService::instance().get_profile(
        nation_id_enum, *troop_type_opt);
    info["cost"] = profile.production.cost;
    info["population_cost"] = profile.production.cost;
    info["resource_costs"] =
        resource_amounts_to_variant_map(profile.production.resource_costs);
    info["build_time"] = static_cast<double>(profile.production.build_time);
    info["individuals_per_unit"] = profile.individuals_per_unit;
    info["display_name"] =
        Game::Util::tr_asset(Game::Util::k_units_context, profile.display_name);
    if (const auto* commander = Game::Units::commander_definition(*troop_type_opt)) {
      info["is_commander"] = true;
      info["strategic_identity"] = Game::Util::tr_asset(
          Game::Util::k_commanders_context, commander->strategic_identity);
      info["recruitment_effect"] = Game::Util::tr_asset(
          Game::Util::k_commanders_context, commander->recruitment_effect);
      info["battlefield_role"] = Game::Util::tr_asset(Game::Util::k_commanders_context,
                                                      commander->battlefield_role);
      info["strengths"] =
          Game::Util::tr_asset(Game::Util::k_commanders_context, commander->strengths);
      info["weaknesses"] =
          Game::Util::tr_asset(Game::Util::k_commanders_context, commander->weaknesses);
      info["passive_aura"] = Game::Util::tr_asset(Game::Util::k_commanders_context,
                                                  commander->passive_aura);
      info["bonus_type"] = QString::fromStdString(commander->bonus_type);
      info["bonus_summary"] = Game::Util::tr_asset(Game::Util::k_commanders_context,
                                                   commander->bonus_summary);
      info["aura_bonus_value"] = static_cast<double>(commander->aura_bonus_value);
      info["rally_ability"] = Game::Util::tr_asset(Game::Util::k_commanders_context,
                                                   commander->rally_ability);
      info["death_consequence"] = Game::Util::tr_asset(Game::Util::k_commanders_context,
                                                       commander->death_consequence);
      info["visual_requirements"] =
          QString::fromStdString(commander->visual_requirements);
      info["bodyguard_count"] = commander->bodyguard_count;
      info["aura_radius"] = static_cast<double>(commander->aura_radius);
      info["rally_cooldown"] = static_cast<double>(commander->rally_cooldown);
    } else {
      info["is_commander"] = false;
    }
  } else {

    info["display_name"] = unit_type;
    info["is_commander"] = false;
  }

  return info;
}

auto ProductionManager::get_construction_info(const QString& item_type) const
    -> QVariantMap {
  QVariantMap info;
  std::string const item_str = item_type.toStdString();
  info["build_time"] = static_cast<double>(get_construction_build_time(item_str));
  info["resource_costs"] =
      resource_amounts_to_variant_map(construction_costs_for_item(item_type));
  info["display_name"] = item_type;
  return info;
}

auto ProductionManager::set_rally_at_screen(qreal sx,
                                            qreal sy,
                                            int local_owner_id,
                                            const ViewportState& viewport) -> bool {
  if ((m_world == nullptr) || (m_picking_service == nullptr) || (m_camera == nullptr)) {
    return false;
  }

  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_surface(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
    return false;
  }

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return false;
  }

  bool updated_any = false;
  const auto& selected = selection_system->get_selected_units();
  for (auto id : selected) {
    auto* e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != local_owner_id ||
        unit->spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto* prod = e->get_component<Engine::Core::ProductionComponent>();
    if (prod == nullptr) {
      prod = e->add_component<Engine::Core::ProductionComponent>();
    }
    if (prod == nullptr) {
      continue;
    }
    prod->rally_x = hit.x();
    prod->rally_z = hit.z();
    prod->rally_set = true;
    updated_any = true;
  }
  return updated_any;
}

auto ProductionManager::collect_available_builders()
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> builders;

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return builders;
  }

  const auto& selected = selection_system->get_selected_units();
  for (auto id : selected) {
    auto* e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* builder_prod = e->get_component<Engine::Core::BuilderProductionComponent>();
    auto* unit = e->get_component<Engine::Core::UnitComponent>();
    if (builder_prod != nullptr && unit != nullptr &&
        unit->spawn_type == Game::Units::SpawnType::Builder &&
        !builder_prod->in_progress && !builder_prod->has_construction_site) {
      builders.push_back(id);
    }
  }

  return builders;
}

auto ProductionManager::calculate_builder_center_position(
    const std::vector<Engine::Core::EntityID>& builder_ids) -> QVector3D {
  float sum_x = 0.0F;
  float sum_y = 0.0F;
  float sum_z = 0.0F;
  int valid_count = 0;

  for (auto id : builder_ids) {
    auto* e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* transform = e->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {
      sum_x += transform->position.x;
      sum_y += transform->position.y;
      sum_z += transform->position.z;
      valid_count++;
    }
  }

  if (valid_count > 0) {
    return {sum_x / static_cast<float>(valid_count),
            sum_y / static_cast<float>(valid_count),
            sum_z / static_cast<float>(valid_count)};
  }

  return {0.0F, 0.0F, 0.0F};
}

auto ProductionManager::get_construction_build_time(const std::string& item_type)
    -> float {
  return Game::Systems::construction_build_time(item_type);
}
