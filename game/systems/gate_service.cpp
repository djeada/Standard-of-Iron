#include "gate_service.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "../core/ambient_session.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "building_collision_registry.h"
#include "nav_grid.h"
#include "owner_registry.h"
#include "pathfinding.h"

namespace Game::Systems {

namespace {

using Engine::Core::GateComponent;
using Engine::Core::PendingRemovalComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;

auto blocker_storage() -> std::vector<GateBlocker>& {
  static std::vector<GateBlocker> storage;
  return storage;
}

auto same_blocker(const GateBlocker& lhs, const GateBlocker& rhs) -> bool {
  return lhs.min_x == rhs.min_x && lhs.max_x == rhs.max_x && lhs.min_z == rhs.min_z &&
         lhs.max_z == rhs.max_z && lhs.owner_id == rhs.owner_id &&
         lhs.entity_id == rhs.entity_id;
}

void publish_navigation_blocker_change(bool obstruction_released) {
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    return;
  }
  pathfinder->mark_navigation_grid_dirty();
  if (obstruction_released) {
    pathfinder->mark_obstruction_released();
  }
}

} // namespace

auto GateService::structure_extent(float rotation_y) -> GateExtent {
  const bool spans_x = GateComponent::spans_x_axis(rotation_y);
  return {.half_x = spans_x ? GateComponent::k_structure_half_span
                            : GateComponent::k_cross_half_extent,
          .half_z = spans_x ? GateComponent::k_cross_half_extent
                            : GateComponent::k_structure_half_span};
}

auto GateService::passage_extent(float rotation_y) -> GateExtent {
  const bool spans_x = GateComponent::spans_x_axis(rotation_y);
  return {.half_x = spans_x ? GateComponent::k_passage_half_width
                            : GateComponent::k_cross_half_extent,
          .half_z = spans_x ? GateComponent::k_cross_half_extent
                            : GateComponent::k_passage_half_width};
}

auto GateService::lane_half_width() -> float {
  auto* pathfinder = NavGrid::get_pathfinder();
  constexpr float k_fallback_cell_size = 1.0F;
  const float cell_size =
      pathfinder != nullptr ? pathfinder->grid_cell_size() : k_fallback_cell_size;
  return cell_size * 0.5F;
}

auto GateService::lane_center(float center_x, float center_z) -> QVector3D {
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    return {center_x, 0.0F, center_z};
  }
  return pathfinder->grid_to_world(pathfinder->world_to_grid(center_x, center_z));
}

auto GateService::lane_extent(float rotation_y) -> GateExtent {
  const bool spans_x = GateComponent::spans_x_axis(rotation_y);
  const float lane_half = lane_half_width();

  const float crossing_half = GateComponent::k_cross_half_extent +
                              BuildingCollisionRegistry::get_grid_padding();
  return {.half_x = spans_x ? lane_half : crossing_half,
          .half_z = spans_x ? crossing_half : lane_half};
}

auto GateService::passage_blocker_bounds(float center_x,
                                         float center_z,
                                         float rotation_y) -> WorldRect {
  const QVector3D lane = lane_center(center_x, center_z);
  const float lane_half = lane_half_width();
  const bool spans_x = GateComponent::spans_x_axis(rotation_y);

  const float half_x = spans_x ? lane_half : GateComponent::k_cross_half_extent;
  const float half_z = spans_x ? GateComponent::k_cross_half_extent : lane_half;
  const float axis_x = spans_x ? lane.x() : center_x;
  const float axis_z = spans_x ? center_z : lane.z();

  return {.min_x = axis_x - half_x,
          .max_x = axis_x + half_x,
          .min_z = axis_z - half_z,
          .max_z = axis_z + half_z};
}

void GateService::sync_gate_footprint(Engine::Core::World& world,
                                      Engine::Core::EntityID entity_id,
                                      float rotation_y) {
  const auto extent = structure_extent(rotation_y);
  Game::Session::services_for(world).building_collision->resize_building(
      entity_id,
      BuildingCollisionRegistry::BuildingSize{.width = extent.half_x * 2.0F,
                                              .depth = extent.half_z * 2.0F});
}

auto GateService::is_gate(const Engine::Core::Entity& entity) -> bool {
  return entity.get_component<GateComponent>() != nullptr;
}

auto GateService::serves_owner(const Engine::Core::World& world,
                               int gate_owner_id,
                               int unit_owner_id) -> bool {
  if (gate_owner_id <= 0 || unit_owner_id <= 0) {
    return false;
  }
  return Game::Session::services_for(world).owners->are_allies(gate_owner_id,
                                                               unit_owner_id);
}

auto GateService::gate_at(Engine::Core::World& world,
                          Engine::Core::EntityID entity_id) -> Engine::Core::Entity* {
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr || entity->get_component<GateComponent>() == nullptr) {
    return nullptr;
  }
  return entity;
}

void GateService::refresh_blockers(Engine::Core::World& world) {
  auto& storage = blocker_storage();
  std::vector<GateBlocker> refreshed;

  for (auto [entity_id, gate, transform, unit] :
       world.view<GateComponent, TransformComponent, UnitComponent>()) {
    if (world.has<PendingRemovalComponent>(entity_id)) {
      continue;
    }

    if (unit.health <= 0 || !gate.blocks_movement()) {
      continue;
    }

    const auto bounds = passage_blocker_bounds(
        transform.position.x, transform.position.z, transform.rotation.y);

    refreshed.push_back(GateBlocker{
        .min_x = bounds.min_x,
        .max_x = bounds.max_x,
        .min_z = bounds.min_z,
        .max_z = bounds.max_z,
        .owner_id = unit.owner_id,
        .entity_id = entity_id,
    });
  }

  bool const changed =
      storage.size() != refreshed.size() ||
      !std::equal(storage.begin(), storage.end(), refreshed.begin(), same_blocker);
  if (!changed) {
    return;
  }
  bool const obstruction_released =
      std::any_of(storage.begin(), storage.end(), [&refreshed](auto const& previous) {
        return std::none_of(
            refreshed.begin(), refreshed.end(), [&previous](auto const& current) {
              return same_blocker(previous, current);
            });
      });
  storage = std::move(refreshed);
  publish_navigation_blocker_change(obstruction_released);
}

void GateService::clear_blockers() {
  auto& storage = blocker_storage();
  if (storage.empty()) {
    return;
  }
  storage.clear();
  publish_navigation_blocker_change(true);
}

auto GateService::blockers() -> const std::vector<GateBlocker>& {
  return blocker_storage();
}

auto GateService::blocks_move(const QVector3D& current,
                              const QVector3D& target) -> bool {
  const auto& storage = blocker_storage();
  if (storage.empty()) {
    return false;
  }

  for (const auto& blocker : storage) {
    if (!blocker.contains(target.x(), target.z())) {
      continue;
    }
    if (blocker.contains(current.x(), current.z())) {
      continue;
    }
    return true;
  }

  return false;
}

auto GateService::blocks_line(const QVector3D& from, const QVector3D& to) -> bool {
  const auto& storage = blocker_storage();
  if (storage.empty()) {
    return false;
  }

  const float delta_x = to.x() - from.x();
  const float delta_z = to.z() - from.z();

  auto slab = [](float start,
                 float delta,
                 float min_bound,
                 float max_bound,
                 float& t_enter,
                 float& t_exit) {
    constexpr float k_epsilon = 1.0e-5F;
    if (std::abs(delta) <= k_epsilon) {
      return start >= min_bound && start <= max_bound;
    }
    float t0 = (min_bound - start) / delta;
    float t1 = (max_bound - start) / delta;
    if (t0 > t1) {
      std::swap(t0, t1);
    }
    t_enter = std::max(t_enter, t0);
    t_exit = std::min(t_exit, t1);
    return t_enter <= t_exit;
  };

  for (const auto& blocker : storage) {
    if (blocker.contains(from.x(), from.z()) || blocker.contains(to.x(), to.z())) {
      continue;
    }
    float t_enter = 0.0F;
    float t_exit = 1.0F;
    if (slab(from.x(), delta_x, blocker.min_x, blocker.max_x, t_enter, t_exit) &&
        slab(from.z(), delta_z, blocker.min_z, blocker.max_z, t_enter, t_exit)) {
      return true;
    }
  }

  return false;
}

auto GateService::set_manual_mode(Engine::Core::Entity& gate, ManualMode mode) -> bool {
  auto* component = gate.get_component<GateComponent>();
  if (component == nullptr || component->manual_mode == mode) {
    return false;
  }
  component->manual_mode = mode;
  return true;
}

auto GateService::cycle_manual_mode(Engine::Core::Entity& gate) -> ManualMode {
  auto* component = gate.get_component<GateComponent>();
  if (component == nullptr) {
    return ManualMode::Automatic;
  }

  switch (component->manual_mode) {
  case ManualMode::Automatic:
    component->manual_mode = ManualMode::ForcedOpen;
    break;
  case ManualMode::ForcedOpen:
    component->manual_mode = ManualMode::ForcedClosed;
    break;
  case ManualMode::ForcedClosed:
    component->manual_mode = ManualMode::Automatic;
    break;
  }
  return component->manual_mode;
}

} // namespace Game::Systems
