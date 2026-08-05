#include "gate_service.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "../core/entity.h"
#include "../core/world.h"
#include "building_collision_registry.h"
#include "owner_registry.h"

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

void GateService::mark_gate_footprint_navigable(Engine::Core::EntityID entity_id) {
  BuildingCollisionRegistry::instance().set_building_navigation_blocking(entity_id,
                                                                         false);
}

void GateService::sync_gate_footprint(Engine::Core::EntityID entity_id,
                                      float rotation_y) {
  const auto extent = structure_extent(rotation_y);
  BuildingCollisionRegistry::instance().resize_building(
      entity_id,
      BuildingCollisionRegistry::BuildingSize{.width = extent.half_x * 2.0F,
                                              .depth = extent.half_z * 2.0F});
}

auto GateService::is_gate(const Engine::Core::Entity& entity) -> bool {
  return entity.get_component<GateComponent>() != nullptr;
}

auto GateService::serves_owner(int gate_owner_id, int unit_owner_id) -> bool {
  if (gate_owner_id <= 0 || unit_owner_id <= 0) {
    return false;
  }
  return OwnerRegistry::instance().are_allies(gate_owner_id, unit_owner_id);
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
  storage.clear();

  for (auto* entity : world.get_entities_with<GateComponent>()) {
    if (entity == nullptr || entity->has_component<PendingRemovalComponent>()) {
      continue;
    }

    const auto* gate = entity->get_component<GateComponent>();
    const auto* transform = entity->get_component<TransformComponent>();
    const auto* unit = entity->get_component<UnitComponent>();
    if (gate == nullptr || transform == nullptr || unit == nullptr) {
      continue;
    }

    if (unit->health <= 0 || !gate->blocks_movement()) {
      continue;
    }

    const auto extent = passage_extent(transform->rotation.y);

    storage.push_back(GateBlocker{
        .min_x = transform->position.x - extent.half_x,
        .max_x = transform->position.x + extent.half_x,
        .min_z = transform->position.z - extent.half_z,
        .max_z = transform->position.z + extent.half_z,
        .owner_id = unit->owner_id,
        .entity_id = entity->get_id(),
    });
  }
}

void GateService::clear_blockers() {
  blocker_storage().clear();
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
