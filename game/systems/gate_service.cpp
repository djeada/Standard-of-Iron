#include "gate_service.h"

#include <algorithm>

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

constexpr float k_gate_half_extent = 1.0F;

auto blocker_storage() -> std::vector<GateBlocker>& {
  static std::vector<GateBlocker> storage;
  return storage;
}

} // namespace

void GateService::mark_gate_footprint_navigable(Engine::Core::EntityID entity_id) {
  BuildingCollisionRegistry::instance().set_building_navigation_blocking(entity_id,
                                                                         false);
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

    storage.push_back(GateBlocker{
        .min_x = transform->position.x - k_gate_half_extent,
        .max_x = transform->position.x + k_gate_half_extent,
        .min_z = transform->position.z - k_gate_half_extent,
        .max_z = transform->position.z + k_gate_half_extent,
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
