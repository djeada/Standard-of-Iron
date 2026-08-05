#include "attack_targeting.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../core/component.h"
#include "../core/world.h"
#include "../map/render_visibility_rules.h"
#include "../units/troop_config.h"
#include "combat_system/combat_utils.h"
#include "owner_registry.h"

namespace Game::Systems {

namespace {

constexpr float k_building_marker_scale = 0.62F;
constexpr float k_building_marker_min_radius = 0.9F;
constexpr float k_unit_marker_min_radius = 0.55F;

auto marker_radius(Engine::Core::Entity& entity,
                   const Engine::Core::UnitComponent& unit,
                   bool is_building) -> float {
  if (is_building) {
    const auto* transform = entity.get_component<Engine::Core::TransformComponent>();
    float const footprint =
        transform != nullptr
            ? std::max(transform->scale.x, transform->scale.z) * k_building_marker_scale
            : 0.0F;
    return std::max(footprint, k_building_marker_min_radius);
  }

  float const configured =
      Game::Units::TroopConfig::instance().get_selection_ring_size(unit.spawn_type);
  return std::max(configured, k_unit_marker_min_radius);
}

auto target_is_visible(const Game::Map::VisibilityService::Snapshot* visibility,
                       float world_x,
                       float world_z) -> bool {
  if (visibility == nullptr) {
    return true;
  }
  return Game::Map::should_render_non_local_unit(*visibility, world_x, world_z);
}

auto make_marker(Engine::Core::Entity& entity,
                 const Engine::Core::UnitComponent& unit,
                 const Engine::Core::TransformComponent& transform)
    -> AttackTargetMarker {
  bool const is_building = entity.has_component<Engine::Core::BuildingComponent>();

  AttackTargetMarker marker;
  marker.entity_id = entity.get_id();
  marker.world_x = transform.position.x;
  marker.world_y = transform.position.y;
  marker.world_z = transform.position.z;
  marker.radius = marker_radius(entity, unit, is_building);
  marker.is_building = is_building;
  return marker;
}

void append_blocked_hover_marker(const AttackTargetingRequest& request,
                                 AttackTargetingHighlights& highlights) {
  auto* entity = request.world->get_entity(request.hovered_entity_id);
  if (entity == nullptr) {
    return;
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr || transform == nullptr) {
    return;
  }

  AttackTargetMarker marker = make_marker(*entity, *unit, *transform);
  marker.hovered = true;
  marker.attackable = false;
  highlights.markers.push_back(marker);
}

} // namespace

auto classify_attack_target(Engine::Core::World* world,
                            int local_owner_id,
                            bool has_attackers,
                            Engine::Core::EntityID target_id) -> AttackTargetVerdict {
  if (world == nullptr || target_id == 0) {
    return AttackTargetVerdict::NoTarget;
  }

  auto* target = world->get_entity(target_id);
  if (target == nullptr) {
    return AttackTargetVerdict::NoTarget;
  }

  const auto* unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->health <= 0 ||
      target->has_component<Engine::Core::PendingRemovalComponent>()) {
    return AttackTargetVerdict::NoTarget;
  }

  if (unit->owner_id == local_owner_id ||
      OwnerRegistry::instance().are_allies(local_owner_id, unit->owner_id)) {
    return AttackTargetVerdict::Ally;
  }

  if (!Combat::is_valid_enemy_of_owner(local_owner_id, target, true)) {
    return AttackTargetVerdict::Neutral;
  }

  return has_attackers ? AttackTargetVerdict::Valid : AttackTargetVerdict::NoAttackers;
}

auto collect_attack_target_highlights(const AttackTargetingRequest& request)
    -> AttackTargetingHighlights {
  AttackTargetingHighlights highlights;
  highlights.hovered_entity_id = request.hovered_entity_id;
  highlights.hovered_verdict = classify_attack_target(request.world,
                                                      request.local_owner_id,
                                                      request.has_attackers,
                                                      request.hovered_entity_id);

  if (request.world == nullptr) {
    return highlights;
  }

  if (highlights.hovered_verdict != AttackTargetVerdict::Valid &&
      highlights.hovered_verdict != AttackTargetVerdict::NoTarget) {
    append_blocked_hover_marker(request, highlights);
  }

  if (!request.has_attackers || request.max_markers == 0) {
    return highlights;
  }

  float const max_distance_sq = request.max_distance > 0.0F
                                    ? request.max_distance * request.max_distance
                                    : std::numeric_limits<float>::max();

  struct ScoredMarker {
    AttackTargetMarker marker;
    float distance_sq;
  };
  std::vector<ScoredMarker> candidates;

  for (auto* entity : request.world->get_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr) {
      continue;
    }
    if (!Combat::is_valid_enemy_of_owner(request.local_owner_id, entity, true)) {
      continue;
    }

    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }
    if (!target_is_visible(
            request.visibility, transform->position.x, transform->position.z)) {
      continue;
    }

    float const dx = transform->position.x - request.anchor_x;
    float const dz = transform->position.z - request.anchor_z;
    float const distance_sq = dx * dx + dz * dz;
    if (distance_sq > max_distance_sq) {
      continue;
    }

    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    AttackTargetMarker marker = make_marker(*entity, *unit, *transform);
    marker.hovered = marker.entity_id == request.hovered_entity_id;
    candidates.push_back({marker, distance_sq});
  }

  if (candidates.size() > request.max_markers) {
    std::nth_element(candidates.begin(),
                     candidates.begin() +
                         static_cast<std::ptrdiff_t>(request.max_markers),
                     candidates.end(),
                     [](const ScoredMarker& lhs, const ScoredMarker& rhs) {
                       if (lhs.marker.hovered != rhs.marker.hovered) {
                         return lhs.marker.hovered;
                       }
                       return lhs.distance_sq < rhs.distance_sq;
                     });
    candidates.resize(request.max_markers);
  }

  highlights.markers.reserve(highlights.markers.size() + candidates.size());
  for (auto& candidate : candidates) {
    highlights.hovered_marker_included =
        highlights.hovered_marker_included || candidate.marker.hovered;
    highlights.markers.push_back(candidate.marker);
  }
  return highlights;
}

auto attack_target_verdict_key(AttackTargetVerdict verdict) -> std::string_view {
  switch (verdict) {
  case AttackTargetVerdict::Valid:
    return "valid";
  case AttackTargetVerdict::Ally:
    return "ally";
  case AttackTargetVerdict::Neutral:
    return "neutral";
  case AttackTargetVerdict::NoAttackers:
    return "no_attackers";
  case AttackTargetVerdict::NoTarget:
    break;
  }
  return "none";
}

} // namespace Game::Systems
