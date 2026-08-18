#include "interaction_targeting.h"

#include <algorithm>
#include <limits>

#include "../core/component.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "../units/spawn_type.h"
#include "food_targets.h"
#include "nav_grid.h"

namespace Game::Systems {

namespace {

constexpr int k_reach_search_radius = 3;
constexpr float k_node_marker_radius = 0.8F;
constexpr float k_building_marker_radius = 1.6F;
constexpr float k_hover_reach_scale = 1.5F;

auto target_is_visible(const Game::Map::VisibilityService::Snapshot* visibility,
                       float world_x,
                       float world_z) -> bool {
  if (visibility == nullptr) {
    return true;
  }
  return visibility->is_explored_world(world_x, world_z);
}

auto has_standing_room(float world_x, float world_z) -> bool {
  const Point grid = NavGrid::world_to_grid(world_x, world_z);
  return NavGrid::find_nearest_walkable_grid(grid, k_reach_search_radius).has_value();
}

struct Scored {
  InteractionTargetMarker marker;
  float distance_sq{0.0F};
};

void collect_resource_nodes(const InteractionTargetingRequest& request,
                            float max_distance_sq,
                            std::vector<Scored>& out) {
  const auto& terrain = Game::Map::TerrainService::instance();
  for (const auto& prop : terrain.world_props()) {
    if (!Game::Map::is_harvestable_world_prop_type(prop.type) ||
        terrain.is_world_prop_reserved(prop.id)) {
      continue;
    }

    const auto position = terrain.world_prop_world_position(prop);
    const float dx = position.x() - request.anchor_x;
    const float dz = position.z() - request.anchor_z;
    const float distance_sq = (dx * dx) + (dz * dz);
    if (distance_sq > max_distance_sq) {
      continue;
    }

    if (!target_is_visible(request.visibility, position.x(), position.z())) {
      continue;
    }

    if (!has_standing_room(position.x(), position.z())) {
      continue;
    }

    InteractionTargetMarker marker;
    marker.world_prop_id = prop.id;
    marker.action = InteractionAction::Gather;
    marker.prop_type = prop.type;
    marker.world_x = position.x();
    marker.world_y = position.y();
    marker.world_z = position.z();
    marker.radius = k_node_marker_radius;
    out.push_back({marker, distance_sq});
  }
}

auto building_action(const InteractionTargetingRequest& request,
                     Engine::Core::Entity& entity,
                     const Engine::Core::UnitComponent& unit) -> InteractionAction {
  if (unit.owner_id != request.local_owner_id || unit.health <= 0) {
    return InteractionAction::None;
  }
  if (entity.has_component<Engine::Core::PendingRemovalComponent>()) {
    return InteractionAction::None;
  }

  if (request.has_civilians && unit.spawn_type == Game::Units::SpawnType::Barracks) {
    const auto* production = entity.get_component<Engine::Core::ProductionComponent>();
    if (production != nullptr &&
        production->manpower_available < production->max_units) {
      return InteractionAction::Deliver;
    }
  }

  if (request.has_builders && unit.health < unit.max_health) {
    return InteractionAction::Repair;
  }

  if (request.has_builders && unit.spawn_type == Game::Units::SpawnType::Farm &&
      farm_is_harvestable(entity, request.local_owner_id) &&
      !food_target_claimed(*request.world, entity.get_id())) {
    return InteractionAction::Harvest;
  }

  return InteractionAction::None;
}

constexpr float k_sheep_marker_radius = 0.7F;

void collect_sheep(const InteractionTargetingRequest& request,
                   float max_distance_sq,
                   std::vector<Scored>& out) {
  for (auto* entity :
       request.world->get_entities_with<Engine::Core::WildlifeComponent>()) {
    if (entity == nullptr || !sheep_is_slaughterable(*entity) ||
        food_target_claimed(*request.world, entity->get_id())) {
      continue;
    }
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }
    const float dx = transform->position.x - request.anchor_x;
    const float dz = transform->position.z - request.anchor_z;
    const float distance_sq = (dx * dx) + (dz * dz);
    if (distance_sq > max_distance_sq) {
      continue;
    }
    if (!target_is_visible(
            request.visibility, transform->position.x, transform->position.z)) {
      continue;
    }

    InteractionTargetMarker marker;
    marker.entity_id = entity->get_id();
    marker.action = InteractionAction::Slaughter;
    marker.world_x = transform->position.x;
    marker.world_y = transform->position.y;
    marker.world_z = transform->position.z;
    marker.radius = k_sheep_marker_radius;
    out.push_back({marker, distance_sq});
  }
}

void collect_buildings(const InteractionTargetingRequest& request,
                       float max_distance_sq,
                       std::vector<Scored>& out) {
  for (auto* entity :
       request.world->get_entities_with<Engine::Core::BuildingComponent>()) {
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr) {
      continue;
    }

    const auto action = building_action(request, *entity, *unit);
    if (action == InteractionAction::None) {
      continue;
    }

    const float dx = transform->position.x - request.anchor_x;
    const float dz = transform->position.z - request.anchor_z;
    const float distance_sq = (dx * dx) + (dz * dz);
    if (distance_sq > max_distance_sq) {
      continue;
    }

    InteractionTargetMarker marker;
    marker.entity_id = entity->get_id();
    marker.action = action;
    marker.world_x = transform->position.x;
    marker.world_y = transform->position.y;
    marker.world_z = transform->position.z;
    marker.radius = k_building_marker_radius;
    out.push_back({marker, distance_sq});
  }
}

} // namespace

auto collect_interaction_target_highlights(const InteractionTargetingRequest& request)
    -> InteractionTargetingHighlights {
  InteractionTargetingHighlights highlights;
  highlights.hovered_entity_id = request.hovered_entity_id;

  if (request.world == nullptr || request.max_markers == 0) {
    return highlights;
  }
  if (!request.has_builders && !request.has_civilians) {
    return highlights;
  }

  const float max_distance_sq = request.max_distance > 0.0F
                                    ? request.max_distance * request.max_distance
                                    : std::numeric_limits<float>::max();

  std::vector<Scored> candidates;
  if (request.has_builders) {
    collect_resource_nodes(request, max_distance_sq, candidates);
    collect_sheep(request, max_distance_sq, candidates);
  }
  collect_buildings(request, max_distance_sq, candidates);

  for (auto& candidate : candidates) {
    candidate.marker.hovered = candidate.marker.entity_id != 0 &&
                               candidate.marker.entity_id == request.hovered_entity_id;
    if (candidate.marker.hovered) {
      highlights.hovered_action = candidate.marker.action;
    }
  }

  if (highlights.hovered_action == InteractionAction::None &&
      request.has_hovered_ground) {
    Scored* closest = nullptr;
    float closest_distance_sq = std::numeric_limits<float>::max();
    for (auto& candidate : candidates) {
      const float dx = candidate.marker.world_x - request.hovered_ground_x;
      const float dz = candidate.marker.world_z - request.hovered_ground_z;
      const float distance_sq = (dx * dx) + (dz * dz);
      const float reach = candidate.marker.radius * k_hover_reach_scale;
      if (distance_sq > reach * reach || distance_sq >= closest_distance_sq) {
        continue;
      }
      closest = &candidate;
      closest_distance_sq = distance_sq;
    }
    if (closest != nullptr) {
      closest->marker.hovered = true;
      highlights.hovered_action = closest->marker.action;
    }
  }

  if (candidates.size() > request.max_markers) {
    std::nth_element(candidates.begin(),
                     candidates.begin() +
                         static_cast<std::ptrdiff_t>(request.max_markers),
                     candidates.end(),
                     [](const Scored& lhs, const Scored& rhs) {
                       if (lhs.marker.hovered != rhs.marker.hovered) {
                         return lhs.marker.hovered;
                       }
                       return lhs.distance_sq < rhs.distance_sq;
                     });
    candidates.resize(request.max_markers);
  }

  highlights.markers.reserve(candidates.size());
  for (auto& candidate : candidates) {
    highlights.markers.push_back(candidate.marker);
  }
  return highlights;
}

auto interaction_action_key(InteractionAction action) -> std::string_view {
  switch (action) {
  case InteractionAction::Gather:
    return "gather";
  case InteractionAction::Deliver:
    return "deliver";
  case InteractionAction::Repair:
    return "repair";
  case InteractionAction::Harvest:
    return "harvest";
  case InteractionAction::Slaughter:
    return "slaughter";
  case InteractionAction::None:
    break;
  }
  return "none";
}

} // namespace Game::Systems
