#include "target_focus.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "../core/component.h"
#include "../core/world.h"
#include "../map/render_visibility_rules.h"
#include "attack_targeting.h"
#include "owner_registry.h"

namespace Game::Systems {

namespace {

auto hostile_to(const OwnerRegistry* owners, int local_owner_id, int owner_id) -> bool {
  if (owner_id == local_owner_id) {
    return false;
  }
  return owners == nullptr || owners->are_enemies(local_owner_id, owner_id);
}

auto visible(const Game::Map::VisibilityService::Snapshot* visibility,
             float x,
             float z) -> bool {
  return visibility == nullptr ||
         Game::Map::should_render_non_local_unit(*visibility, x, z);
}

auto make_marker(Engine::Core::Entity& entity,
                 TargetFocusRole role,
                 bool hostile) -> TargetFocusMarker {
  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  const auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  TargetFocusMarker marker;
  marker.entity_id = entity.get_id();
  marker.role = role;
  marker.hostile = hostile;
  marker.is_building = entity.has_component<Engine::Core::BuildingComponent>();
  if (transform != nullptr) {
    marker.world_x = transform->position.x;
    marker.world_y = transform->position.y;
    marker.world_z = transform->position.z;
  }
  marker.radius =
      unit != nullptr ? attack_marker_radius(entity, *unit, marker.is_building) : 0.5F;
  return marker;
}

} // namespace

auto collect_target_focus_markers(const TargetFocusRequest& request)
    -> std::vector<TargetFocusMarker> {
  std::vector<TargetFocusMarker> markers;
  auto* world = request.world;
  if (world == nullptr) {
    return markers;
  }
  const OwnerRegistry* owners = request.owners;

  auto alive_unit = [&](Engine::Core::EntityID id) -> Engine::Core::Entity* {
    auto* entity = world->get_entity(id);
    const auto* unit = entity != nullptr
                           ? entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    if (unit == nullptr || unit->health <= 0) {
      return nullptr;
    }
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      return nullptr;
    }
    if (unit->owner_id != request.local_owner_id &&
        !visible(request.visibility, transform->position.x, transform->position.z)) {
      return nullptr;
    }
    return entity;
  };

  if (request.inspected != Engine::Core::NULL_ENTITY) {
    if (auto* entity = alive_unit(request.inspected)) {
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      const bool hostile = hostile_to(owners, request.local_owner_id, unit->owner_id);
      markers.push_back(make_marker(*entity, TargetFocusRole::Inspected, hostile));
    }
  }

  if (request.selection == nullptr || request.selection->empty()) {
    return markers;
  }

  std::unordered_map<Engine::Core::EntityID, int> locked;
  std::unordered_set<Engine::Core::EntityID> selected;
  for (const auto id : *request.selection) {
    auto* entity = world->get_entity(id);
    const auto* unit = entity != nullptr
                           ? entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    if (unit == nullptr || unit->owner_id != request.local_owner_id) {
      continue;
    }
    selected.insert(id);
    const auto* attack = entity->get_component<Engine::Core::AttackTargetComponent>();
    if (attack == nullptr || attack->target_id == Engine::Core::NULL_ENTITY) {
      continue;
    }
    ++locked[attack->target_id];
  }
  if (selected.empty()) {
    return markers;
  }

  std::vector<std::pair<Engine::Core::EntityID, int>> ordered(locked.begin(),
                                                              locked.end());
  std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
    return a.second != b.second ? a.second > b.second : a.first < b.first;
  });
  std::size_t locked_count = 0;
  for (const auto& [target_id, weight] : ordered) {
    if (locked_count >= request.max_locked_targets) {
      break;
    }
    if (target_id == request.inspected) {
      continue;
    }
    auto* entity = alive_unit(target_id);
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto marker =
        make_marker(*entity,
                    TargetFocusRole::LockedTarget,
                    hostile_to(owners, request.local_owner_id, unit->owner_id));
    marker.weight = weight;
    markers.push_back(marker);
    ++locked_count;
  }

  std::size_t incoming_count = 0;
  for (auto* entity : world->get_entities_with<Engine::Core::AttackTargetComponent>()) {
    if (incoming_count >= request.max_incoming_attackers) {
      break;
    }
    const auto* attack = entity->get_component<Engine::Core::AttackTargetComponent>();
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (attack == nullptr || unit == nullptr || unit->health <= 0) {
      continue;
    }
    if (!hostile_to(owners, request.local_owner_id, unit->owner_id)) {
      continue;
    }
    if (selected.count(attack->target_id) == 0) {
      continue;
    }
    if (entity->get_id() == request.inspected || locked.count(entity->get_id()) != 0) {
      continue;
    }
    if (alive_unit(entity->get_id()) == nullptr) {
      continue;
    }
    markers.push_back(make_marker(*entity, TargetFocusRole::IncomingAttacker, true));
    ++incoming_count;
  }

  return markers;
}

} // namespace Game::Systems
