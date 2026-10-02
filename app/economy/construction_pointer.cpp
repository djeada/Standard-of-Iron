#include "app/economy/construction_pointer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "app/economy/harvest_targeting.h"
#include "app/economy/placement_session.h"
#include "app/input/viewport_state.h"
#include "game/core/component_core.h"
#include "game/core/component_economy.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/economy/food_targets.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/wall_network_service.h"
#include "game/systems/structure_placement_service.h"
#include "scene/camera.h"

namespace App::Economy {

namespace {

constexpr float k_sheep_pick_radius_px = 30.0F;
constexpr float k_sheep_pick_body_height = 0.35F;

// The builder sent after a sheep usually stands right beside it, so a generic
// unit pick can land on the builder. Sheep are looked for on their own first.
auto pick_sheep_on_screen(Engine::Core::World& world,
                          const Render::GL::Camera& camera,
                          const ViewportState& viewport,
                          const QPointF& screen_point) -> Engine::Core::EntityID {
  Engine::Core::EntityID best_id = 0;
  float best_distance_sq = k_sheep_pick_radius_px * k_sheep_pick_radius_px;
  for (auto [entity, wildlife, transform] :
       world.entity_view<Engine::Core::WildlifeComponent,
                         Engine::Core::TransformComponent>()) {
    (void)wildlife;
    if (!Game::Systems::sheep_is_slaughterable(entity) ||
        Game::Systems::food_target_claimed(world, entity.get_id())) {
      continue;
    }
    QPointF screen;
    QVector3D const body(transform.position.x,
                         transform.position.y +
                             (k_sheep_pick_body_height * transform.scale.y),
                         transform.position.z);
    if (!Game::Systems::PickingService::world_to_screen(
            camera, viewport.width, viewport.height, body, screen)) {
      continue;
    }
    auto const dx = static_cast<float>(screen.x() - screen_point.x());
    auto const dy = static_cast<float>(screen.y() - screen_point.y());
    float const distance_sq = (dx * dx) + (dy * dy);
    if (distance_sq <= best_distance_sq) {
      best_distance_sq = distance_sq;
      best_id = entity.get_id();
    }
  }
  return best_id;
}

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
  Engine::Core::EntityID picked =
      pick_sheep_on_screen(*world, camera, viewport, screen_point);
  if (picked == 0) {
    picked = Game::Systems::PickingService::pick_unit_first(
        static_cast<float>(screen_point.x()),
        static_cast<float>(screen_point.y()),
        *world,
        camera,
        viewport.width,
        viewport.height,
        0);
  }
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

auto maybe_snap_tower_to_wall_socket(Engine::Core::World* world,
                                     int owner_id,
                                     const QVector3D& world_position) -> QVector3D {
  if (world == nullptr || owner_id <= 0) {
    return world_position;
  }

  const auto snapped = Game::Systems::WallNetworkService::find_tower_snap_socket(
      *world,
      owner_id,
      world_position.x(),
      world_position.z(),
      Game::Systems::wall_ground_probe(*world));
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

auto resolve_harvest_hit(Engine::Core::World* world,
                         const QString& item_type,
                         int owner_id,
                         const std::vector<Engine::Core::EntityID>& crew,
                         const Render::GL::Camera& camera,
                         const ViewportState& viewport,
                         const QPointF& screen_point,
                         const QVector3D& ground_hit)
    -> std::optional<ConstructionPointerHit> {
  auto& terrain_service = Game::Session::session_for(*world).terrain();
  if (is_collect_item(item_type)) {
    if (auto food_hit = resolve_food_target_hit(
            world, owner_id, camera, viewport, screen_point, terrain_service);
        food_hit.has_value()) {
      return food_hit;
    }
  }
  const CrewClaims claims = crew_claims(world, crew);
  auto resolved_target = resolve_harvest_target_at_position(
      terrain_service, item_type, ground_hit, claims);
  if (!resolved_target.has_value()) {
    resolved_target = resolve_harvest_target_from_screen(
        terrain_service, item_type, camera, viewport, screen_point, claims);
  }
  if (!resolved_target.has_value()) {
    return std::nullopt;
  }

  return ConstructionPointerHit{
      .world_position = terrain_service.resolve_surface_world_position(
          resolved_target->target.x, resolved_target->target.z),
      .harvest_target_id = resolved_target->target.id};
}

} // namespace

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
            *world, candidate, Game::Systems::wall_ground_probe(*world), true);
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

  constexpr auto spacing = Game::Systems::WallNetworkService::k_segment_spacing;
  consider_candidate(base);
  if (vertical) {
    consider_candidate({.x = base.x, .z = base.z - spacing});
    consider_candidate({.x = base.x, .z = base.z + spacing});
  } else {
    consider_candidate({.x = base.x - spacing, .z = base.z});
    consider_candidate({.x = base.x + spacing, .z = base.z});
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
  const bool harvesting = is_harvest_construction_item(item_type);
  QVector3D hit;
  bool const has_hit =
      harvesting ? Game::Systems::PickingService::screen_to_ground(
                       screen_point, camera, viewport.width, viewport.height, hit)
                 : Game::Systems::PickingService::screen_to_surface(
                       Game::Session::session_for(*world).terrain(),
                       screen_point,
                       camera,
                       viewport.width,
                       viewport.height,
                       hit);
  if (!has_hit) {
    return std::nullopt;
  }

  if (harvesting) {
    return resolve_harvest_hit(
        world, item_type, owner_id, crew, camera, viewport, screen_point, hit);
  }

  return ConstructionPointerHit{
      .world_position =
          resolve_construction_placement_position(world, item_type, owner_id, hit),
      .harvest_target_id = 0};
}

auto nearest_clear_site(Engine::Core::World& world,
                        const QString& item_type,
                        const QVector3D& wanted,
                        float rotation_y,
                        const std::vector<Engine::Core::EntityID>& builders)
    -> QVector3D {
  if (Game::Session::session_for(world)
          .building_collision()
          .is_point_in_blocking_building(wanted.x(), wanted.z())) {
    return wanted;
  }
  const std::string type = item_type.toStdString();
  const auto size = Game::Systems::BuildingCollisionRegistry::get_building_size(type);
  const float search = std::clamp(std::max(size.width, size.depth) * 0.5F, 3.0F, 8.0F);
  const auto site =
      Game::Systems::find_clear_site(world, type, wanted, search, rotation_y, builders);
  if (!site.has_value()) {
    return wanted;
  }
  return QVector3D(site->x(), wanted.y(), site->z());
}

} // namespace App::Economy
