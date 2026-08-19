#include "app/economy/production_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "app/economy/harvest_targeting.h"
#include "app/economy/production_readouts.h"
#include "app/economy/resource_text.h"
#include "app/input/input_command_handler.h"
#include "app/orders/order_submission.h"
#include "game/audio/audio_cues.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/render_bridge/picking_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/construction_cost_catalog.h"
#include "game/systems/food_targets.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
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

using App::Economy::crew_claims;
using App::Economy::CrewClaims;
using App::Economy::evaluate_harvest_placement;
using App::Economy::generic_collect_failure_reason;
using App::Economy::harvest_product_type;
using App::Economy::HarvestPlacement;
using App::Economy::is_harvest_construction_item;
using App::Economy::prop_taken;
using App::Economy::resolve_harvest_target_at_position;
using App::Economy::resolve_harvest_target_from_screen;
using App::Economy::ResolvedHarvestTarget;

constexpr float k_construction_rotation_step_degrees = 5.0F;
constexpr float k_wall_preview_rotation_step_degrees = 90.0F;

struct ConstructionPointerHit {
  QVector3D world_position;
  std::uint64_t harvest_target_id = 0;
  Engine::Core::EntityID food_target_id = 0;
};

auto resolve_food_target_hit(Engine::Core::World* world,
                             int owner_id,
                             const Render::GL::Camera& camera,
                             const ViewportState& viewport,
                             const QPointF& screen_point,
                             const Game::Map::TerrainService& terrain_service)
    -> std::optional<ConstructionPointerHit> {
  if (world == nullptr || viewport.width <= 0 || viewport.height <= 0) {
    return std::nullopt;
  }
  const Engine::Core::EntityID picked = Game::Systems::PickingService::pick_unit_first(
      static_cast<float>(screen_point.x()),
      static_cast<float>(screen_point.y()),
      *world,
      camera,
      viewport.width,
      viewport.height,
      0);
  if (picked == 0) {
    return std::nullopt;
  }
  const auto target = Game::Systems::resolve_food_target(*world, picked, owner_id);
  if (!target.has_value() || Game::Systems::food_target_claimed(*world, picked)) {
    return std::nullopt;
  }
  return ConstructionPointerHit{
      .world_position =
          terrain_service.resolve_surface_world_position(target->x, target->z),
      .harvest_target_id = 0,
      .food_target_id = picked};
}

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
         item_type == QStringLiteral("temple") || item_type == QStringLiteral("farm");
}

auto item_supports_preview_rotation(const QString& item_type) -> bool {
  return is_previewable_structure_item(item_type);
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
    auto& terrain_service = Game::Map::TerrainService::instance();
    if (App::Economy::is_collect_item(item_type)) {
      if (auto food_hit = resolve_food_target_hit(
              world, owner_id, camera, viewport, screen_point, terrain_service);
          food_hit.has_value()) {
        return food_hit;
      }
    }
    const CrewClaims claims = crew_claims(world, crew);
    auto resolved_target = resolve_harvest_target_at_position(item_type, hit, claims);
    if (!resolved_target.has_value()) {
      resolved_target = resolve_harvest_target_from_screen(
          item_type, camera, viewport, screen_point, claims);
    }
    if (!resolved_target.has_value()) {
      return std::nullopt;
    }

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
  m_pending_food_target_id = 0;
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
  m_pending_food_target_id = 0;
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
    m_pending_food_target_id = resolved_hit->food_target_id;
    m_construction_placement_position = resolved_hit->world_position;
    update_non_wall_construction_preview(resolved_hit->world_position);
    return;
  }

  clear_non_wall_construction_preview();
  m_pending_harvest_target_id = 0;
  m_pending_food_target_id = 0;
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
        m_pending_food_target_id = resolved_hit->food_target_id;
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

  if (m_pending_food_target_id != 0 &&
      is_harvest_construction_item(m_pending_construction_type)) {
    const int owner_id = pending_construction_owner_id();
    const auto target = Game::Systems::resolve_food_target(
        *m_world, m_pending_food_target_id, owner_id);
    if (!target.has_value() ||
        Game::Systems::food_target_claimed(*m_world, m_pending_food_target_id)) {
      set_construction_preview_valid(false);
      emit construction_placement_rejected(QCoreApplication::translate(
          "ProductionManager", "That resource is already assigned."));
      return;
    }

    std::vector<Engine::Core::EntityID> crew = m_pending_construction_builders;
    std::stable_sort(crew.begin(), crew.end(), [&](auto lhs, auto rhs) {
      auto distance_to = [&](Engine::Core::EntityID id) {
        auto* entity = m_world->get_entity(id);
        auto* transform =
            entity != nullptr
                ? entity->get_component<Engine::Core::TransformComponent>()
                : nullptr;
        if (transform == nullptr) {
          return std::numeric_limits<float>::infinity();
        }
        float const dx = transform->position.x - target->x;
        float const dz = transform->position.z - target->z;
        return dx * dx + dz * dz;
      };
      return distance_to(lhs) < distance_to(rhs);
    });
    {
      App::Core::OrderRequest request;
      request.kind = App::Core::OrderKind::Gather;
      request.payload = Game::Command::StartHarvest{
          .units = std::move(crew),
          .construction_type = std::string(target->product_type),
          .resource_target = target->id,
          .site = QVector3D(target->x, 0.0F, target->z)};
      request.has_destination = true;
      request.destination = QVector3D(target->x, 0.0F, target->z);
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
    m_pending_harvest_target_id = 0;
    m_pending_food_target_id = 0;
    clear_preview_entities();
    set_construction_preview_active(false);
    set_construction_preview_valid(false);
    emit placing_construction_changed();
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
    m_pending_food_target_id = 0;
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
        App::Economy::construction_costs(m_pending_construction_type);
    if (!resource_costs.empty() &&
        !Game::Systems::PlayerResourceRegistry::instance().has_at_least(
            owner_id, resource_costs)) {
      emit construction_placement_rejected(
          App::Economy::insufficient_resources_reason(owner_id, resource_costs));
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
  m_pending_food_target_id = 0;
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
  m_pending_food_target_id = 0;
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
  m_pending_food_target_id = 0;
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
  if (m_pending_food_target_id != 0 &&
      is_harvest_construction_item(m_pending_construction_type)) {
    set_construction_preview_valid(
        Game::Systems::resolve_food_target(
            *m_world, m_pending_food_target_id, pending_construction_owner_id())
            .has_value());
  } else if (is_harvest_construction_item(m_pending_construction_type)) {
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
  m_pending_food_target_id = 0;
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

  auto* renderable = Game::Units::add_building_renderable(
      *entity, pending_construction_nation_id(), item_type.toStdString());
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

  for (const auto& segment : m_wall_preview_segments) {
    auto* entity = m_world->create_entity();
    if (entity == nullptr) {
      continue;
    }

    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    auto* renderable = entity->add_component<Engine::Core::RenderableComponent>();
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
    renderable->renderer_id =
        (gate_mode ? Game::Systems::WallNetworkService::resolve_gate_appearance(
                         nation_id, segment.connection_mask, segment.rotation_y)
                   : Game::Systems::WallNetworkService::resolve_appearance(
                         nation_id, segment.connection_mask))
            .renderer_id;

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
        reason = App::Economy::wall_segment_failure_text(segment);
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
                     App::Economy::wood_per_wall_segment(m_pending_construction_type));
  if (!Game::Systems::PlayerResourceRegistry::instance().has_at_least(owner_id,
                                                                      total_cost)) {
    emit construction_placement_rejected(
        App::Economy::insufficient_resources_reason(owner_id, total_cost));
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
    emit construction_placement_rejected(App::Economy::insufficient_resources_reason(
        owner_id, App::Economy::construction_costs(m_pending_construction_type)));
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
