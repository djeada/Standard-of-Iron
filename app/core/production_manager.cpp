#include "production_manager.h"

#include <QDebug>
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <limits>

#include "app/core/input_command_handler.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/picking_service.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/troop_profile_service.h"
#include "game/systems/wall_network_service.h"
#include "game/units/commander_catalog.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "game/visuals/team_colors.h"
#include "render/gl/camera.h"

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
constexpr auto k_collect_stone_item_type = "collect_stone";
constexpr auto k_collect_iron_ore_item_type = "collect_iron_ore";
constexpr float k_cut_tree_build_time = 6.0F;
constexpr float k_collect_stone_build_time = 6.0F;
constexpr float k_collect_iron_ore_build_time = 6.0F;
constexpr int k_wall_segment_wood_cost = 25;

enum class HarvestTargetKind {
  Tree,
  Boulder,
  IronOre,
};

struct HarvestPlacement {
  std::optional<Game::Map::WorldPropTarget> target;
  std::optional<QVector3D> work_position;
  Engine::Core::EntityID builder_id = 0;
  QString failure_reason;

  [[nodiscard]] auto valid() const -> bool {
    return target.has_value() && work_position.has_value() && builder_id != 0;
  }
};

void clear_builder_task_target(Engine::Core::BuilderProductionComponent* builder_prod) {
  if (builder_prod == nullptr) {
    return;
  }
  builder_prod->has_task_target = false;
  builder_prod->task_target_id = 0;
  builder_prod->task_target_x = 0.0F;
  builder_prod->task_target_z = 0.0F;
  builder_prod->task_target_reserved = false;
}

void clear_builder_assignment(Engine::Core::BuilderProductionComponent* builder_prod) {
  if (builder_prod == nullptr) {
    return;
  }
  builder_prod->in_progress = false;
  builder_prod->time_remaining = 0.0F;
  builder_prod->construction_complete = false;
  builder_prod->has_construction_site = false;
  builder_prod->construction_site_x = 0.0F;
  builder_prod->construction_site_z = 0.0F;
  builder_prod->at_construction_site = false;
  builder_prod->product_type.clear();
  builder_prod->is_placement_preview = false;
  builder_prod->construction_site_entity_id = 0;
  builder_prod->queued_construction_site_ids.clear();
  clear_builder_task_target(builder_prod);
}

auto is_construction_position_valid(float pos_x,
                                    float pos_z,
                                    const std::string& building_type) -> bool {
  auto& collision_registry = Game::Systems::BuildingCollisionRegistry::instance();
  auto size =
      Game::Systems::BuildingCollisionRegistry::get_building_size(building_type);

  if (collision_registry.is_circle_overlapping_building(
          pos_x, pos_z, std::max(size.width, size.depth) * 0.5F, 0)) {
    return false;
  }

  Game::Systems::Pathfinding* pathfinder =
      Game::Systems::CommandService::get_pathfinder();
  if (pathfinder != nullptr) {
    Game::Systems::Point const grid =
        Game::Systems::CommandService::world_to_grid(pos_x, pos_z);
    if (!pathfinder->is_walkable(grid.x, grid.y)) {
      return false;
    }
    if (auto& terrain_service = Game::Map::TerrainService::instance();
        terrain_service.is_initialized() &&
        !terrain_service.is_walkable(grid.x, grid.y)) {
      return false;
    }
  }

  return true;
}

auto is_wall_position_blocked_by_site(Engine::Core::World* world,
                                      int grid_x,
                                      int grid_z) -> bool {
  if (world == nullptr) {
    return false;
  }

  auto sites = world->get_entities_with<Engine::Core::WallConstructionSiteComponent>();
  for (auto* entity : sites) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto* wall = entity->get_component<Engine::Core::WallSegmentComponent>();
    if (wall != nullptr && wall->grid_x == grid_x && wall->grid_z == grid_z) {
      return true;
    }
  }
  return false;
}

auto update_builder_move_target(Engine::Core::Entity* entity,
                                float target_x,
                                float target_z) -> void {
  if (entity == nullptr) {
    return;
  }
  if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
    movement->goal_x = target_x;
    movement->goal_y = target_z;
    movement->target_x = target_x;
    movement->target_y = target_z;
  }
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

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  if (pathfinder == nullptr) {
    return std::nullopt;
  }

  float const unit_radius =
      Game::Systems::CommandService::get_unit_radius(*world, builder_id);
  Game::Systems::Point const tree_grid =
      Game::Systems::CommandService::world_to_grid(target.x, target.z);
  Game::Systems::Point const work_grid =
      Game::Systems::Pathfinding::find_nearest_walkable_point(
          tree_grid, 3, *pathfinder, unit_radius);
  if (!pathfinder->is_walkable_with_radius(work_grid.x, work_grid.y, unit_radius)) {
    return std::nullopt;
  }

  QVector3D work_position = Game::Systems::CommandService::grid_to_world(work_grid);
  auto& terrain_service = Game::Map::TerrainService::instance();
  work_position.setY(terrain_service.resolve_surface_world_y(
      work_position.x(), work_position.z(), 0.0F, transform->position.y));
  return work_position;
}

auto evaluate_harvest_placement(Engine::Core::World* world,
                                const std::vector<Engine::Core::EntityID>& builder_ids,
                                const QVector3D& world_position,
                                HarvestTargetKind kind) -> HarvestPlacement {
  HarvestPlacement placement;
  auto& terrain_service = Game::Map::TerrainService::instance();
  if (kind == HarvestTargetKind::Tree) {
    placement.target =
        terrain_service.find_tree_near_world(world_position.x(), world_position.z());
  } else if (kind == HarvestTargetKind::Boulder) {
    placement.target =
        terrain_service.find_boulder_near_world(world_position.x(), world_position.z());
  } else {
    placement.target = terrain_service.find_iron_ore_near_world(world_position.x(),
                                                                world_position.z());
  }
  if (!placement.target.has_value()) {
    placement.failure_reason = kind == HarvestTargetKind::Tree
                                   ? QStringLiteral("Select a tree to chop.")
                               : kind == HarvestTargetKind::Boulder
                                   ? QStringLiteral("Select a boulder to collect.")
                                   : QStringLiteral("Select iron ore to collect.");
    return placement;
  }

  placement.builder_id = select_best_builder_for_target(
      world, builder_ids, placement.target->x, placement.target->z);
  if (placement.builder_id == 0) {
    placement.failure_reason =
        kind == HarvestTargetKind::Tree
            ? QStringLiteral("No available builder can chop that tree.")
        : kind == HarvestTargetKind::Boulder
            ? QStringLiteral("No available builder can collect that boulder.")
            : QStringLiteral("No available builder can collect that iron ore.");
    return placement;
  }

  placement.work_position =
      resolve_harvest_work_position(world, placement.builder_id, *placement.target);
  if (!placement.work_position.has_value()) {
    placement.failure_reason =
        kind == HarvestTargetKind::Tree
            ? QStringLiteral("No walkable spot near that tree.")
        : kind == HarvestTargetKind::Boulder
            ? QStringLiteral("No walkable spot near that boulder.")
            : QStringLiteral("No walkable spot near that iron ore.");
    return placement;
  }

  return placement;
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

} // namespace

void ProductionManager::start_building_placement(const QString& building_type) {
  if (building_type.isEmpty()) {
    return;
  }
  m_pending_building_type = building_type;
}

void ProductionManager::place_building_at_screen(qreal sx,
                                                 qreal sy,
                                                 int local_owner_id,
                                                 const ViewportState& viewport) {
  if (m_pending_building_type.isEmpty() || (m_world == nullptr) ||
      (m_picking_service == nullptr) || (m_camera == nullptr)) {
    return;
  }

  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
    return;
  }

  Game::Units::SpawnParams params;
  params.position = hit;
  params.player_id = local_owner_id;
  params.ai_controlled = false;

  auto& nation_registry = Game::Systems::NationRegistry::instance();
  if (const auto* nation = nation_registry.get_nation_for_player(local_owner_id)) {
    params.nation_id = nation->id;
  } else {
    params.nation_id = nation_registry.default_nation_id();
  }

  if (m_pending_building_type == QStringLiteral("defense_tower")) {
    params.spawn_type = Game::Units::SpawnType::DefenseTower;

    auto registry = Game::Map::MapTransformer::get_factory_registry();
    if (registry) {
      auto unit = registry->create(params.spawn_type, *m_world, params);
      if (unit) {
        qInfo() << "Placed defense tower at" << hit.x() << hit.z();
      }
    }
  }

  if (m_pending_building_type == QStringLiteral("wall_segment")) {
    params.spawn_type = Game::Units::SpawnType::WallSegment;

    auto registry = Game::Map::MapTransformer::get_factory_registry();
    if (registry) {
      auto unit = registry->create(params.spawn_type, *m_world, params);
      if (unit) {
        qInfo() << "Placed wall segment at" << hit.x() << hit.z();
      }
    }
  }

  m_pending_building_type.clear();
}

void ProductionManager::cancel_building_placement() {
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
  set_construction_preview_valid(false);
  clear_construction_preview_summary();
  clear_wall_preview_entities();
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
  QVector3D hit;
  if (Game::Systems::PickingService::screen_to_ground(
          screenPt, *m_camera, viewport.width, viewport.height, hit)) {
    m_construction_placement_position = hit;
    if (is_wall_construction_mode()) {
      if (m_wall_drag_active) {
        rebuild_wall_preview_plan(hit);
      }
      return;
    }

    if (auto const harvest_kind =
            harvest_target_kind_for_item(m_pending_construction_type);
        harvest_kind.has_value()) {
      set_construction_preview_valid(
          evaluate_harvest_placement(m_world,
                                     m_pending_construction_builders,
                                     m_construction_placement_position,
                                     *harvest_kind)
              .valid());
    } else {
      set_construction_preview_valid(is_construction_position_valid(
          hit.x(), hit.z(), m_pending_construction_type.toStdString()));
    }

    for (auto id : m_pending_construction_builders) {
      auto* e = m_world->get_entity(id);
      if (e == nullptr) {
        continue;
      }

      auto* builder_prod = e->get_component<Engine::Core::BuilderProductionComponent>();
      if (builder_prod != nullptr) {
        builder_prod->construction_site_x = hit.x();
        builder_prod->construction_site_z = hit.z();
      }
    }
  } else {
    set_construction_preview_valid(false);
  }
}

void ProductionManager::on_construction_pointer_pressed(qreal sx,
                                                        qreal sy,
                                                        const ViewportState& viewport) {
  if (!m_is_placing_construction || !is_wall_construction_mode() ||
      (m_picking_service == nullptr) || (m_camera == nullptr)) {
    return;
  }

  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
    return;
  }

  m_wall_drag_active = true;
  m_wall_drag_anchor_set = true;
  m_wall_drag_anchor_world = hit;
  m_construction_placement_position = hit;
  rebuild_wall_preview_plan(hit);
}

void ProductionManager::on_construction_pointer_released(
    qreal sx, qreal sy, const ViewportState& viewport) {
  if (!m_is_placing_construction) {
    return;
  }

  if (!is_wall_construction_mode()) {
    on_construction_confirm();
    return;
  }

  if (!m_wall_drag_anchor_set) {
    return;
  }

  QVector3D hit = m_construction_placement_position;
  if ((m_picking_service != nullptr) && (m_camera != nullptr)) {
    QVector3D release_hit;
    if (Game::Systems::PickingService::screen_to_ground(
            QPointF(sx, sy), *m_camera, viewport.width, viewport.height, release_hit)) {
      hit = release_hit;
    }
  }

  m_construction_placement_position = hit;
  rebuild_wall_preview_plan(hit);
  confirm_wall_construction_plan();
}

void ProductionManager::on_construction_confirm() {
  if (!m_is_placing_construction || m_pending_construction_builders.empty()) {
    on_construction_cancel();
    return;
  }

  if (is_wall_construction_mode()) {
    confirm_wall_construction_plan();
    return;
  }

  if (auto const harvest_kind =
          harvest_target_kind_for_item(m_pending_construction_type);
      harvest_kind.has_value()) {
    HarvestPlacement const placement =
        evaluate_harvest_placement(m_world,
                                   m_pending_construction_builders,
                                   m_construction_placement_position,
                                   *harvest_kind);
    if (!placement.valid()) {
      set_construction_preview_valid(false);
      emit construction_placement_rejected(placement.failure_reason);
      return;
    }

    auto& terrain_service = Game::Map::TerrainService::instance();
    if (!terrain_service.reserve_world_prop(placement.target->id)) {
      set_construction_preview_valid(false);
      emit construction_placement_rejected(
          QStringLiteral("That resource is already assigned."));
      return;
    }

    for (auto id : m_pending_construction_builders) {
      auto* entity = m_world->get_entity(id);
      auto* builder_prod =
          entity != nullptr
              ? entity->get_component<Engine::Core::BuilderProductionComponent>()
              : nullptr;
      if (builder_prod == nullptr) {
        continue;
      }

      if (id != placement.builder_id) {
        clear_builder_assignment(builder_prod);
        continue;
      }

      builder_prod->is_placement_preview = false;
      builder_prod->construction_site_x = placement.work_position->x();
      builder_prod->construction_site_z = placement.work_position->z();
      builder_prod->has_task_target = true;
      builder_prod->task_target_id = placement.target->id;
      builder_prod->task_target_x = placement.target->x;
      builder_prod->task_target_z = placement.target->z;
      builder_prod->task_target_reserved = true;
    }

    if (auto* entity = m_world->get_entity(placement.builder_id)) {
      if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
        movement->goal_x = placement.work_position->x();
        movement->goal_y = placement.work_position->z();
        movement->target_x = placement.work_position->x();
        movement->target_y = placement.work_position->z();
      }
    }

    m_is_placing_construction = false;
    m_pending_construction_type.clear();
    m_pending_construction_builders.clear();
    set_construction_preview_valid(false);
    emit placing_construction_changed();
    return;
  }

  if (!is_construction_position_valid(m_construction_placement_position.x(),
                                      m_construction_placement_position.z(),
                                      m_pending_construction_type.toStdString())) {

    emit construction_placement_rejected(QStringLiteral("Cannot build there."));
    return;
  }

  for (auto id : m_pending_construction_builders) {
    auto* e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* builder_prod = e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod != nullptr) {
      builder_prod->is_placement_preview = false;
      builder_prod->construction_site_x = m_construction_placement_position.x();
      builder_prod->construction_site_z = m_construction_placement_position.z();
    }

    auto* mv = e->get_component<Engine::Core::MovementComponent>();
    if (mv != nullptr) {
      mv->goal_x = m_construction_placement_position.x();
      mv->goal_y = m_construction_placement_position.z();
      mv->target_x = m_construction_placement_position.x();
      mv->target_y = m_construction_placement_position.z();
    }
  }

  m_is_placing_construction = false;
  m_pending_construction_type.clear();
  m_pending_construction_builders.clear();
  set_construction_preview_valid(false);
  emit placing_construction_changed();
}

void ProductionManager::on_construction_cancel() {
  if (!m_is_placing_construction) {
    return;
  }

  clear_wall_preview_entities();
  clear_construction_preview_summary();
  m_wall_preview_segments.clear();
  m_wall_drag_active = false;
  m_wall_drag_anchor_set = false;

  for (auto id : m_pending_construction_builders) {
    auto* e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* builder_prod = e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod != nullptr) {
      if (builder_prod->task_target_reserved) {
        Game::Map::TerrainService::instance().release_world_prop(
            builder_prod->task_target_id);
      }
      clear_builder_assignment(builder_prod);
    }
  }

  m_is_placing_construction = false;
  m_pending_construction_type.clear();
  m_pending_construction_builders.clear();
  set_construction_preview_valid(false);
  emit placing_construction_changed();
}

void ProductionManager::start_builder_construction(const QString& item_type) {
  if (m_world == nullptr) {
    return;
  }

  m_pending_construction_builders = collect_available_builders();
  if (m_pending_construction_builders.empty()) {
    return;
  }

  m_pending_construction_type = item_type;
  m_is_placing_construction = true;
  m_construction_placement_position =
      calculate_builder_center_position(m_pending_construction_builders);
  clear_wall_preview_entities();
  clear_construction_preview_summary();
  m_wall_preview_segments.clear();
  m_wall_drag_active = false;
  m_wall_drag_anchor_set = false;

  std::string const item_str = item_type.toStdString();
  float const build_time = get_construction_build_time(item_str);

  if (item_type == QStringLiteral("wall_segment")) {
    set_construction_preview_valid(false);
    emit placing_construction_changed();
    return;
  }

  for (auto id : m_pending_construction_builders) {
    auto* e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* builder_prod = e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod == nullptr) {
      continue;
    }

    builder_prod->product_type = item_str;
    builder_prod->build_time = build_time;
    builder_prod->time_remaining = build_time;
    builder_prod->has_construction_site = true;
    builder_prod->construction_site_x = m_construction_placement_position.x();
    builder_prod->construction_site_z = m_construction_placement_position.z();
    builder_prod->at_construction_site = false;
    builder_prod->in_progress = false;
    builder_prod->is_placement_preview = true;
    clear_builder_task_target(builder_prod);
  }

  if (auto const harvest_kind =
          harvest_target_kind_for_item(m_pending_construction_type);
      harvest_kind.has_value()) {
    set_construction_preview_valid(
        evaluate_harvest_placement(m_world,
                                   m_pending_construction_builders,
                                   m_construction_placement_position,
                                   *harvest_kind)
            .valid());
  } else {
    set_construction_preview_valid(
        is_construction_position_valid(m_construction_placement_position.x(),
                                       m_construction_placement_position.z(),
                                       item_str));
  }

  emit placing_construction_changed();
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
  return m_pending_construction_type == QStringLiteral("wall_segment");
}

void ProductionManager::clear_wall_preview_entities() {
  if (m_world == nullptr) {
    m_wall_preview_entity_ids.clear();
    return;
  }

  for (auto entity_id : m_wall_preview_entity_ids) {
    if (m_world->get_entity(entity_id) != nullptr) {
      m_world->destroy_entity(entity_id);
    }
  }
  m_wall_preview_entity_ids.clear();
}

void ProductionManager::rebuild_wall_preview_entities() {
  clear_wall_preview_entities();
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
    renderable->renderer_id = Game::Systems::WallNetworkService::resolve_appearance(
                                  nation_id, segment.connection_mask)
                                  .renderer_id;
    renderable->color[0] = team_color.x();
    renderable->color[1] = team_color.y();
    renderable->color[2] = team_color.z();

    preview->owner_id = owner_id;
    preview->nation_id = nation_id;
    preview->grid_x = segment.grid_x;
    preview->grid_z = segment.grid_z;
    preview->valid = segment.valid;

    m_wall_preview_entity_ids.push_back(entity->get_id());
  }
}

void ProductionManager::rebuild_wall_preview_plan(
    const QVector3D& current_world_position) {
  m_wall_preview_segments.clear();
  clear_wall_preview_entities();

  if (m_world == nullptr || !m_wall_drag_anchor_set) {
    set_construction_preview_valid(false);
    clear_construction_preview_summary();
    return;
  }

  const auto anchor = Game::Systems::WallNetworkService::snap_world_position(
      m_wall_drag_anchor_world.x(), m_wall_drag_anchor_world.z());
  const auto target = Game::Systems::WallNetworkService::snap_world_position(
      current_world_position.x(), current_world_position.z());
  const auto chain =
      Game::Systems::WallNetworkService::build_axis_aligned_chain(anchor, target);

  auto occupancy = Game::Systems::WallNetworkService::OccupancySet{};
  Game::Systems::WallNetworkService::add_world_occupancy(*m_world, occupancy, true);

  int available_wood = Game::Systems::PlayerResourceRegistry::instance().get(
      pending_construction_owner_id(), Game::Systems::ResourceType::Wood);
  int valid_segment_count = 0;

  for (const auto& grid_pos : chain) {
    const auto key =
        Game::Systems::WallNetworkService::encode_key(grid_pos.x, grid_pos.z);
    const QVector3D world_position = Game::Systems::CommandService::grid_to_world(
        Game::Systems::Point{grid_pos.x, grid_pos.z});

    WallPlacementSegment segment;
    segment.grid_x = grid_pos.x;
    segment.grid_z = grid_pos.z;
    segment.world_position = world_position;

    if (occupancy.find(key) != occupancy.end() ||
        is_wall_position_blocked_by_site(m_world, grid_pos.x, grid_pos.z)) {
      segment.failure_reason = "Blocked by an existing wall.";
    } else if (!is_construction_position_valid(
                   world_position.x(), world_position.z(), "wall_segment")) {
      segment.failure_reason = "Cannot build there.";
    } else if (available_wood < k_wall_segment_wood_cost) {
      segment.failure_reason = "Not enough wood.";
    } else {
      segment.valid = true;
      ++valid_segment_count;
      available_wood -= k_wall_segment_wood_cost;
      occupancy.insert(key);
    }

    m_wall_preview_segments.push_back(segment);
  }

  const int owner_id = pending_construction_owner_id();
  const auto nation_id = pending_construction_nation_id();
  auto preview_occupancy = Game::Systems::WallNetworkService::OccupancySet{};
  Game::Systems::WallNetworkService::add_world_occupancy(
      *m_world, preview_occupancy, true);
  for (const auto& segment : m_wall_preview_segments) {
    if (segment.valid) {
      preview_occupancy.insert(Game::Systems::WallNetworkService::encode_key(
          segment.grid_x, segment.grid_z));
    }
  }

  auto& terrain_service = Game::Map::TerrainService::instance();
  for (auto& segment : m_wall_preview_segments) {
    segment.connection_mask =
        Game::Systems::WallNetworkService::compute_connection_mask(
            preview_occupancy, segment.grid_x, segment.grid_z);
    const auto appearance = Game::Systems::WallNetworkService::resolve_appearance(
        nation_id, segment.connection_mask);
    segment.rotation_y = appearance.rotation_y;
    if (terrain_service.is_initialized()) {
      segment.world_position = terrain_service.resolve_surface_world_position(
          segment.world_position.x(), segment.world_position.z(), 0.0F, 0.0F);
    }
  }

  set_construction_preview_valid(valid_segment_count > 0);
  set_construction_preview_summary(static_cast<int>(m_wall_preview_segments.size()),
                                   valid_segment_count,
                                   valid_segment_count * k_wall_segment_wood_cost);
  rebuild_wall_preview_entities();
}

void ProductionManager::confirm_wall_construction_plan() {
  const int valid_segment_count = static_cast<int>(
      std::count_if(m_wall_preview_segments.begin(),
                    m_wall_preview_segments.end(),
                    [](const auto& segment) { return segment.valid; }));
  if (valid_segment_count <= 0) {
    QString reason = QStringLiteral("No valid wall segments in that drag.");
    for (const auto& segment : m_wall_preview_segments) {
      if (!segment.failure_reason.empty()) {
        reason = QString::fromStdString(segment.failure_reason);
        break;
      }
    }
    emit construction_placement_rejected(reason);
    set_construction_preview_valid(false);
    return;
  }

  const int owner_id = pending_construction_owner_id();
  const int total_cost = valid_segment_count * k_wall_segment_wood_cost;
  if (Game::Systems::PlayerResourceRegistry::instance().get(
          owner_id, Game::Systems::ResourceType::Wood) < total_cost) {
    emit construction_placement_rejected(QStringLiteral("Not enough wood."));
    set_construction_preview_valid(false);
    return;
  }

  Game::Systems::PlayerResourceRegistry::instance().add(
      owner_id, Game::Systems::ResourceType::Wood, -total_cost);

  const auto nation_id = pending_construction_nation_id();
  const QVector3D team_color = Game::Visuals::team_colorForOwner(owner_id);
  const float build_time = get_construction_build_time("wall_segment");

  std::vector<Engine::Core::EntityID> site_ids;
  site_ids.reserve(valid_segment_count);
  auto& terrain_service = Game::Map::TerrainService::instance();
  for (const auto& segment : m_wall_preview_segments) {
    if (!segment.valid) {
      continue;
    }

    auto* entity = m_world->create_entity();
    if (entity == nullptr) {
      continue;
    }

    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    auto* renderable = entity->add_component<Engine::Core::RenderableComponent>("", "");
    auto* wall = entity->add_component<Engine::Core::WallSegmentComponent>();
    auto* site = entity->add_component<Engine::Core::WallConstructionSiteComponent>();
    if (transform == nullptr || renderable == nullptr || wall == nullptr ||
        site == nullptr) {
      m_world->destroy_entity(entity->get_id());
      continue;
    }

    QVector3D position = segment.world_position;
    if (terrain_service.is_initialized()) {
      position = terrain_service.resolve_surface_world_position(
          position.x(), position.z(), 0.0F, position.y());
    }

    transform->position = {position.x(), position.y(), position.z()};
    transform->rotation = {0.0F, segment.rotation_y, 0.0F};
    transform->scale = {1.0F, 1.0F, 1.0F};

    renderable->visible = false;
    renderable->mesh = Engine::Core::RenderableComponent::MeshKind::Cube;
    renderable->renderer_id = Game::Systems::WallNetworkService::resolve_appearance(
                                  nation_id, segment.connection_mask)
                                  .renderer_id;
    renderable->color[0] = team_color.x();
    renderable->color[1] = team_color.y();
    renderable->color[2] = team_color.z();

    wall->grid_x = segment.grid_x;
    wall->grid_z = segment.grid_z;
    wall->connection_mask = segment.connection_mask;

    site->owner_id = owner_id;
    site->nation_id = nation_id;
    site->build_time = build_time;
    site->progress = 0.0F;

    site_ids.push_back(entity->get_id());
  }

  Game::Systems::WallNetworkService::refresh_world(*m_world);

  if (m_pending_construction_builders.empty()) {
    emit construction_placement_rejected(QStringLiteral("No available builder."));
    return;
  }

  std::vector<std::vector<Engine::Core::EntityID>> assignments(
      m_pending_construction_builders.size());
  for (std::size_t i = 0; i < site_ids.size(); ++i) {
    assignments[i % assignments.size()].push_back(site_ids[i]);
  }

  for (std::size_t builder_index = 0;
       builder_index < m_pending_construction_builders.size();
       ++builder_index) {
    const auto builder_id = m_pending_construction_builders[builder_index];
    auto* entity = m_world->get_entity(builder_id);
    auto* builder_prod =
        entity != nullptr
            ? entity->get_component<Engine::Core::BuilderProductionComponent>()
            : nullptr;
    if (builder_prod == nullptr) {
      continue;
    }

    clear_builder_assignment(builder_prod);
    if (assignments[builder_index].empty()) {
      continue;
    }

    builder_prod->product_type = "wall_segment";
    builder_prod->build_time = build_time;
    builder_prod->time_remaining = build_time;
    builder_prod->construction_complete = false;
    builder_prod->queued_construction_site_ids = assignments[builder_index];
    builder_prod->construction_site_entity_id =
        builder_prod->queued_construction_site_ids.front();
    builder_prod->queued_construction_site_ids.erase(
        builder_prod->queued_construction_site_ids.begin());

    auto* site_entity = m_world->get_entity(builder_prod->construction_site_entity_id);
    auto* site_transform =
        site_entity != nullptr
            ? site_entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    if (site_transform == nullptr) {
      clear_builder_assignment(builder_prod);
      continue;
    }

    builder_prod->has_construction_site = true;
    builder_prod->construction_site_x = site_transform->position.x;
    builder_prod->construction_site_z = site_transform->position.z;
    update_builder_move_target(
        entity, builder_prod->construction_site_x, builder_prod->construction_site_z);
  }

  clear_wall_preview_entities();
  m_wall_preview_segments.clear();
  m_wall_drag_active = false;
  m_wall_drag_anchor_set = false;
  m_is_placing_construction = false;
  m_pending_construction_type.clear();
  m_pending_construction_builders.clear();
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
      m["in_progress"] = builder_prod->in_progress ||
                         builder_prod->is_placement_preview ||
                         builder_prod->has_construction_site;
      m["time_remaining"] = builder_prod->time_remaining;
      m["build_time"] = builder_prod->build_time;
      m["product_type"] = QString::fromStdString(builder_prod->product_type);
      return m;
    }
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
    info["build_time"] = static_cast<double>(profile.production.build_time);
    info["individuals_per_unit"] = profile.individuals_per_unit;
    info["display_name"] = QString::fromStdString(profile.display_name);
    if (const auto* commander = Game::Units::commander_definition(*troop_type_opt)) {
      info["is_commander"] = true;
      info["strategic_identity"] =
          QString::fromStdString(commander->strategic_identity);
      info["recruitment_effect"] =
          QString::fromStdString(commander->recruitment_effect);
      info["battlefield_role"] = QString::fromStdString(commander->battlefield_role);
      info["strengths"] = QString::fromStdString(commander->strengths);
      info["weaknesses"] = QString::fromStdString(commander->weaknesses);
      info["passive_aura"] = QString::fromStdString(commander->passive_aura);
      info["bonus_type"] = QString::fromStdString(commander->bonus_type);
      info["bonus_summary"] = QString::fromStdString(commander->bonus_summary);
      info["aura_bonus_value"] = static_cast<double>(commander->aura_bonus_value);
      info["rally_ability"] = QString::fromStdString(commander->rally_ability);
      info["death_consequence"] = QString::fromStdString(commander->death_consequence);
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

void ProductionManager::set_rally_at_screen(qreal sx,
                                            qreal sy,
                                            int local_owner_id,
                                            const ViewportState& viewport) {
  if ((m_world == nullptr) || (m_picking_service == nullptr) || (m_camera == nullptr)) {
    return;
  }

  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
    return;
  }

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }

  const auto& selected = selection_system->get_selected_units();
  for (auto id : selected) {
    auto* e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != local_owner_id) {
      continue;
    }

    auto* prod = e->get_component<Engine::Core::ProductionComponent>();
    if (prod != nullptr) {
      prod->rally_x = hit.x();
      prod->rally_z = hit.z();
      prod->rally_set = true;
    }
  }
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
    if (builder_prod != nullptr && !builder_prod->in_progress &&
        !builder_prod->has_construction_site && !builder_prod->is_placement_preview) {
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
  constexpr float DEFAULT_BUILD_TIME = 10.0F;
  constexpr float CATAPULT_BUILD_TIME = 15.0F;
  constexpr float BALLISTA_BUILD_TIME = 12.0F;
  constexpr float DEFENSE_TOWER_BUILD_TIME = 20.0F;
  constexpr float WALL_SEGMENT_BUILD_TIME = 8.0F;

  if (item_type == "catapult") {
    return CATAPULT_BUILD_TIME;
  }
  if (item_type == "ballista") {
    return BALLISTA_BUILD_TIME;
  }
  if (item_type == "defense_tower") {
    return DEFENSE_TOWER_BUILD_TIME;
  }
  if (item_type == "wall_segment") {
    return WALL_SEGMENT_BUILD_TIME;
  }
  if (item_type == k_cut_tree_item_type) {
    return k_cut_tree_build_time;
  }
  if (item_type == k_collect_stone_item_type) {
    return k_collect_stone_build_time;
  }
  if (item_type == k_collect_iron_ore_item_type) {
    return k_collect_iron_ore_build_time;
  }
  return DEFAULT_BUILD_TIME;
}
